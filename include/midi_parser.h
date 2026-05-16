#pragma once
#include "midi_structures.h"
#include <fstream>
#include <string>
#include <cstdint>

class MidiParser {
public:
    void reset();
    [[nodiscard]] MidiFile parse(const std::string& filename);
private:
    mutable std::ifstream file;
    static constexpr uint32_t swapUint32(uint32_t value) noexcept;
    static constexpr uint16_t swapUint16(uint16_t value) noexcept;
    [[nodiscard]] bool readInt32(uint32_t& value);
    [[nodiscard]] bool readInt16(uint16_t& value);
    [[nodiscard]] bool readChunk(char* buffer, size_t size);
    void parseMetaEvent(MidiEvent& event, MidiFile& midiFile, uint32_t absoluteTick,
        const char* trackEnd, const char*& ptr);
};
