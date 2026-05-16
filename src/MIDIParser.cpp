#include "midi_parser.h"
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstring>
#include <climits>
#include <fstream>
#include <cerrno>
#include <cstring>

constexpr uint32_t MAX_EVENT_LENGTH = 0x100000;

constexpr uint32_t MidiParser::swapUint32(uint32_t value) noexcept {
    return ((value >> 24) & 0xFF) | ((value >> 8) & 0xFF00) |
           ((value << 8) & 0xFF0000) | ((value << 24) & 0xFF000000);
}

constexpr uint16_t MidiParser::swapUint16(uint16_t value) noexcept {
    return static_cast<uint16_t>((value >> 8) | (value << 8));
}

void MidiParser::reset() {
    if (file.is_open()) file.close();
    file.clear();
}

bool MidiParser::readInt32(uint32_t& value) {
    if (!file.read(reinterpret_cast<char*>(&value), 4)) return false;
    value = swapUint32(value);
    return true;
}

bool MidiParser::readInt16(uint16_t& value) {
    if (!file.read(reinterpret_cast<char*>(&value), 2)) return false;
    value = swapUint16(value);
    return true;
}

bool MidiParser::readChunk(char* buffer, size_t size) {
    return file.read(buffer, static_cast<std::streamsize>(size)).good();
}

namespace {
    inline uint8_t readByte(const char*& ptr, const char* end) {
        if (ptr >= end)
            throw std::runtime_error("Unexpected end of track data");
        return static_cast<uint8_t>(*ptr++);
    }

    inline void readVarLen(const char*& ptr, const char* end, uint32_t& value) {
        value = 0;
        uint8_t byte;
        int count = 0;
        do {
            if (count++ >= 4)
                throw std::runtime_error("Variable-length quantity too long");
            byte = readByte(ptr, end);
            if (value > (UINT32_MAX >> 7))
                throw std::runtime_error("Variable-length quantity overflow");
            value = (value << 7) | (byte & 0x7F);
        } while (byte & 0x80);
    }
}

void MidiParser::parseMetaEvent(MidiEvent& event, MidiFile& midiFile,
                                uint32_t absoluteTick,
                                const char* trackEnd, const char*& ptr) {
    uint8_t metaType = readByte(ptr, trackEnd);
    uint32_t length = 0;
    readVarLen(ptr, trackEnd, length);
    if (length > MAX_EVENT_LENGTH)
        throw std::runtime_error("Meta event too large");
    if (static_cast<size_t>(trackEnd - ptr) < length)
        throw std::runtime_error("Meta event length exceeds track data");

    event.metaData.resize(length);
    std::memcpy(event.metaData.data(), ptr, length);
    ptr += length;

    event.status = 0xFF;
    event.data1  = metaType;

    if (metaType == 0x51 && length == 3) {
        uint32_t us = (static_cast<uint8_t>(event.metaData[0]) << 16) |
                      (static_cast<uint8_t>(event.metaData[1]) <<  8) |
                       static_cast<uint8_t>(event.metaData[2]);
        midiFile.tempoChanges.push_back({absoluteTick, us});
    } else if (metaType == 0x58 && length == 4) {
        if (event.metaData[1] >= 8) throw std::runtime_error("Invalid time signature denominator");
        midiFile.timeSignatures.push_back({
            absoluteTick,
            static_cast<uint8_t>(event.metaData[0]),
            static_cast<uint8_t>(1 << event.metaData[1]),
            static_cast<uint8_t>(event.metaData[2]),
            static_cast<uint8_t>(event.metaData[3])
        });
    } else if (metaType == 0x59 && length == 2) {
        midiFile.keySignatures.push_back({
            absoluteTick,
            static_cast<int8_t>(event.metaData[0]),
            static_cast<uint8_t>(event.metaData[1])
        });
    }
}

MidiFile MidiParser::parse(const std::string& filename) {
    reset();

    // Basic path safety: reject traversal sequences
    if (filename.find("..") != std::string::npos)
        throw std::runtime_error("Invalid filename: path traversal not allowed");

    file.open(filename, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + filename + " (" + std::strerror(errno) + ")");

    MidiFile midiFile;
    char headerChunk[4];
    if (!readChunk(headerChunk, 4) || std::string(headerChunk, 4) != "MThd")
        throw std::runtime_error("Not a MIDI file (missing MThd)");

    uint32_t headerLength;
    if (!readInt32(headerLength) || headerLength != 6)
        throw std::runtime_error("Invalid MIDI header length");

    if (!readInt16(midiFile.format) || !readInt16(midiFile.numTracks) || !readInt16(midiFile.division))
        throw std::runtime_error("Error reading MIDI header");

    if (midiFile.division == 0) throw std::runtime_error("Invalid time division");
    if (midiFile.format > 2)   throw std::runtime_error("Invalid MIDI format");
    if (midiFile.numTracks == 0) throw std::runtime_error("No tracks in file");

    for (int i = 0; i < midiFile.numTracks; ++i) {
        char trackChunk[4];
        if (!readChunk(trackChunk, 4) || std::string(trackChunk, 4) != "MTrk")
            throw std::runtime_error("Missing MTrk header for track " + std::to_string(i));

        uint32_t trackLength;
        if (!readInt32(trackLength))
            throw std::runtime_error("Error reading track length");

        std::vector<char> trackData(trackLength);
        if (!file.read(trackData.data(), trackLength))
            throw std::runtime_error("Error reading track data");

        const char* ptr      = trackData.data();
        const char* trackEnd = ptr + trackLength;
        MidiTrack track;
        uint32_t absoluteTick = 0;
        uint8_t  lastStatus   = 0;
        track.events.reserve(1000);

        while (ptr < trackEnd) {
            uint32_t deltaTime = 0;
            readVarLen(ptr, trackEnd, deltaTime);
            if (UINT32_MAX - absoluteTick < deltaTime)
                throw std::runtime_error("Tick counter overflow");
            absoluteTick += deltaTime;

            uint8_t status = readByte(ptr, trackEnd);
            if (status < 0x80) {
                if (lastStatus == 0) throw std::runtime_error("Running status with no prior status");
                status = lastStatus;
                --ptr;
            } else {
                lastStatus = status;
            }

            MidiEvent event;
            event.absoluteTick = absoluteTick;
            event.status       = status;

            uint8_t type = status & 0xF0;

            if (type == 0x80 || type == 0x90 || type == 0xA0 || type == 0xB0 || type == 0xE0) {
                if (static_cast<size_t>(trackEnd - ptr) < 2)
                    throw std::runtime_error("Unexpected end of track (2-byte event)");
                event.data1 = std::min(readByte(ptr, trackEnd), static_cast<uint8_t>(127));
                event.data2 = std::min(readByte(ptr, trackEnd), static_cast<uint8_t>(127));
                track.events.push_back(std::move(event));
            } else if (type == 0xC0 || type == 0xD0) {
                if (static_cast<size_t>(trackEnd - ptr) < 1)
                    throw std::runtime_error("Unexpected end of track (1-byte event)");
                event.data1 = std::min(readByte(ptr, trackEnd), static_cast<uint8_t>(127));
                track.events.push_back(std::move(event));
            } else if (status == 0xF0 || status == 0xF7) {
                uint32_t len = 0;
                readVarLen(ptr, trackEnd, len);
                if (len > MAX_EVENT_LENGTH || static_cast<size_t>(trackEnd - ptr) < len)
                    throw std::runtime_error("SysEx event overflows track data");
                event.metaData.assign(ptr, ptr + len);
                ptr += len;
                track.events.push_back(std::move(event));
            } else if (status == 0xFF) {
                parseMetaEvent(event, midiFile, absoluteTick, trackEnd, ptr);
                track.events.push_back(std::move(event));
            } else {
                // Skip unknown system messages
                uint8_t dataCount = (status == 0xF2) ? 2 : (status == 0xF1 || status == 0xF3) ? 1 : 0;
                for (uint8_t j = 0; j < dataCount && ptr < trackEnd; ++j)
                    readByte(ptr, trackEnd);
            }
        }
        midiFile.tracks.push_back(std::move(track));
    }
    file.close();
    return midiFile;
}
