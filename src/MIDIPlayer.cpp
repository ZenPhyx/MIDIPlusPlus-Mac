#include "MIDIPlayer.hpp"
#include "midi_parser.h"
#include "RobloxKeyMapper.hpp"
#include "MacInputInjector.hpp"
#include "RtMidi.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <string>
#include <vector>

// ─── Helpers ─────────────────────────────────────────────────────────────────

static int64_t tickToUs(uint32_t tick, uint16_t division,
                        const std::vector<TempoChange>& tempos) {
    int64_t  us       = 0;
    uint32_t prevTick = 0;
    uint32_t tempo    = 500000;
    for (const auto& tc : tempos) {
        if (tc.tick >= tick) break;
        us       += static_cast<int64_t>(tc.tick - prevTick) * tempo / division;
        prevTick  = tc.tick;
        tempo     = tc.microsecondsPerQuarter;
    }
    us += static_cast<int64_t>(tick - prevTick) * tempo / division;
    return us;
}

struct TimedNote { int64_t us; char key; bool press; };

static std::vector<TimedNote> buildTimeline(const MidiFile& midi, int transposeAmount) {
    // Collect note-on and note-off events
    struct RawEvent { uint32_t tick; int note; bool press; };
    std::vector<RawEvent> raw;
    int lo = 127, hi = 0;

    for (const auto& track : midi.tracks) {
        for (const auto& ev : track.events) {
            uint8_t type = ev.status & 0xF0;
            bool isOn  = (type == 0x90 && ev.data2 > 0);
            bool isOff = (type == 0x80) || (type == 0x90 && ev.data2 == 0);
            if (isOn) {
                raw.push_back({ev.absoluteTick, ev.data1, true});
                lo = std::min(lo, (int)ev.data1);
                hi = std::max(hi, (int)ev.data1);
            } else if (isOff) {
                raw.push_back({ev.absoluteTick, ev.data1, false});
            }
        }
    }
    if (raw.empty()) return {};

    std::sort(raw.begin(), raw.end(), [](auto& a, auto& b){ return a.tick < b.tick; });

    std::vector<TimedNote> timeline;
    for (auto& r : raw) {
        auto key = RobloxKeyMapper::map(r.note + transposeAmount);
        if (key) timeline.push_back({tickToUs(r.tick, midi.division, midi.tempoChanges), *key, r.press});
    }
    return timeline;
}

static bool countdown(std::atomic<bool>& running, MIDIPlayer::StatusCallback onStatus) {
    for (int i = 3; i > 0; --i) {
        if (!running) return false;
        onStatus("Starting in " + std::to_string(i) + "...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return running.load();
}

// ─── MIDIPlayer ──────────────────────────────────────────────────────────────

void MIDIPlayer::stop() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
}

void MIDIPlayer::playFile(const std::string& path,
                          KeyCallback    onKey,
                          StatusCallback onStatus,
                          DoneCallback   onDone) {
    stop();
    m_running = true;

    m_thread = std::thread([this, path, onKey, onStatus, onDone]() {
        // Parse
        onStatus("Loading...");
        MidiFile midi;
        try {
            // Expand leading ~
            std::string p = path;
            if (!p.empty() && p.front() == '~') {
                const char* home = std::getenv("HOME");
                if (home) p = std::string(home) + p.substr(1);
            }
            MidiParser parser;
            midi = parser.parse(p);
        } catch (const std::exception& ex) {
            onStatus(std::string("Error: ") + ex.what());
            onDone();
            return;
        }

        if (midi.tempoChanges.empty())
            midi.tempoChanges.push_back({0, 500000});
        std::sort(midi.tempoChanges.begin(), midi.tempoChanges.end(),
                  [](auto& a, auto& b){ return a.tick < b.tick; });

        // Collect notes for range detection
        int lo = 127, hi = 0;
        for (const auto& track : midi.tracks)
            for (const auto& ev : track.events)
                if ((ev.status & 0xF0) == 0x90 && ev.data2 > 0) {
                    lo = std::min(lo, (int)ev.data1);
                    hi = std::max(hi, (int)ev.data1);
                }

        int shift = RobloxKeyMapper::autoTranspose(lo, hi);
        auto timeline = buildTimeline(midi, shift);

        if (timeline.empty()) {
            onStatus("Error: No playable notes found.");
            onDone();
            return;
        }

        std::string info = std::to_string(timeline.size()) + " notes";
        if (shift != 0) info += "  (transposed " + (shift > 0 ? std::string("+") : "") + std::to_string(shift) + ")";
        onStatus(info);

        if (!countdown(m_running, onStatus)) { onDone(); return; }

        onStatus("Playing \xe2\x99\xaa"); // ♪

        using Clock = std::chrono::steady_clock;
        auto start  = Clock::now();

        for (const auto& note : timeline) {
            if (!m_running) break;
            auto target = start + std::chrono::microseconds(note.us);
            std::this_thread::sleep_until(target);
            if (!m_running) break;
            onKey(note.key, note.press);
        }

        if (m_running) onStatus("Done!");
        onDone();
    });
}

