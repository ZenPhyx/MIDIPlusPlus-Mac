#include "RtMidi.h"
#include "midi_parser.h"
#include "MacInputInjector.hpp"
#include "RobloxKeyMapper.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <cstdio>

// ─── Signal handling ────────────────────────────────────────────────────────

static std::atomic<bool> g_running{true};

static void onSigInt(int) { g_running = false; }

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void countdown(int seconds, const std::string& message) {
    for (int i = seconds; i > 0; --i) {
        std::cout << "\r" << message << " " << i << "...  " << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "\r" << message << " GO!           \n";
}

// ─── Live MIDI input mode ─────────────────────────────────────────────────────

static void runLiveMode() {
    RtMidiIn midi;

    unsigned int portCount = midi.getPortCount();
    if (portCount == 0) {
        std::cerr << "No MIDI input devices found. Plug in your Yamaha and try again.\n";
        return;
    }

    std::cout << "\nAvailable MIDI input devices:\n";
    for (unsigned int i = 0; i < portCount; ++i)
        std::cout << "  [" << i << "] " << midi.getPortName(i) << "\n";

    unsigned int choice = 0;
    if (portCount > 1) {
        std::cout << "Select device (0-" << portCount - 1 << "): ";
        std::cin >> choice;
        std::cin.ignore();
        if (choice >= portCount) { std::cerr << "Invalid choice.\n"; return; }
    } else {
        std::cout << "Using: " << midi.getPortName(0) << "\n";
    }

    midi.openPort(choice);
    midi.ignoreTypes(true, true, true); // ignore sysex, timing, active sensing

    std::cout << "\nLive mode active. Play your keyboard!\n";
    std::cout << "Press Ctrl+C to stop.\n\n";

    countdown(3, "Switching to Roblox in");

    std::vector<unsigned char> message;
    while (g_running) {
        midi.getMessage(&message);
        if (message.size() >= 3) {
            uint8_t status   = message[0] & 0xF0;
            uint8_t midiNote = message[1];
            uint8_t velocity = message[2];

            bool isNoteOn = (status == 0x90 && velocity > 0);
            if (isNoteOn) {
                auto key = RobloxKeyMapper::map(static_cast<int>(midiNote));
                if (key.has_value()) {
                    tapKey(*key);
                } else {
                    // Note out of Roblox range — silently ignore
                }
            }
        }
        // Tiny sleep to avoid burning CPU while polling
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    midi.closePort();
    std::cout << "\nLive mode stopped.\n";
}

// ─── MIDI file playback mode ──────────────────────────────────────────────────

struct TimedNote {
    int64_t timestampUs; // microseconds from song start
    char    key;
};

// Convert an absolute tick to microseconds from the start of the song.
static int64_t tickToUs(uint32_t tick, uint16_t division,
                        const std::vector<TempoChange>& tempoChanges) {
    int64_t  us       = 0;
    uint32_t prevTick = 0;
    uint32_t tempo    = 500000; // default 120 BPM

    for (const auto& tc : tempoChanges) {
        if (tc.tick >= tick) break;
        us       += static_cast<int64_t>(tc.tick - prevTick) * tempo / division;
        prevTick  = tc.tick;
        tempo     = tc.microsecondsPerQuarter;
    }
    us += static_cast<int64_t>(tick - prevTick) * tempo / division;
    return us;
}

static void runFileMode() {
    std::cout << "\nEnter path to MIDI file: ";
    std::string path;
    std::getline(std::cin, path);
    // Trim quotes the user might have pasted
    if (!path.empty() && path.front() == '"') path = path.substr(1);
    if (!path.empty() && path.back()  == '"') path.pop_back();
    // Expand leading ~ to home directory
    if (!path.empty() && path.front() == '~') {
        const char* home = std::getenv("HOME");
        if (home) path = std::string(home) + path.substr(1);
    }

    MidiParser parser;
    MidiFile midiFile;
    try {
        midiFile = parser.parse(path);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to load MIDI file: " << ex.what() << "\n";
        return;
    }

    // Ensure tempo list is sorted and non-empty
    std::sort(midiFile.tempoChanges.begin(), midiFile.tempoChanges.end(),
              [](const TempoChange& a, const TempoChange& b){ return a.tick < b.tick; });
    if (midiFile.tempoChanges.empty())
        midiFile.tempoChanges.push_back({0, 500000});

    // Collect all note-on events from every track
    int lowestNote  = 127;
    int highestNote = 0;
    std::vector<std::pair<uint32_t, int>> rawNotes; // {tick, midiNote}

    for (const auto& track : midiFile.tracks) {
        for (const auto& ev : track.events) {
            uint8_t type = ev.status & 0xF0;
            if (type == 0x90 && ev.data2 > 0) {
                rawNotes.push_back({ev.absoluteTick, ev.data1});
                lowestNote  = std::min(lowestNote,  static_cast<int>(ev.data1));
                highestNote = std::max(highestNote, static_cast<int>(ev.data1));
            }
        }
    }

    if (rawNotes.empty()) {
        std::cerr << "No note events found in this MIDI file.\n";
        return;
    }

    // Auto-transpose if needed
    int transposeAmount = RobloxKeyMapper::autoTranspose(lowestNote, highestNote);
    if (transposeAmount != 0) {
        std::cout << "Auto-transposing " << (transposeAmount > 0 ? "+" : "")
                  << transposeAmount << " semitones to fit Roblox range.\n";
    }

    // Build sorted timed-note list
    std::sort(rawNotes.begin(), rawNotes.end(),
              [](const auto& a, const auto& b){ return a.first < b.first; });

    std::vector<TimedNote> timeline;
    timeline.reserve(rawNotes.size());

    int64_t songDurationUs = 0;
    for (const auto& [tick, midiNote] : rawNotes) {
        int transposed = midiNote + transposeAmount;
        auto key = RobloxKeyMapper::map(transposed);
        if (!key.has_value()) continue;
        int64_t ts = tickToUs(tick, midiFile.division, midiFile.tempoChanges);
        timeline.push_back({ts, *key});
        songDurationUs = std::max(songDurationUs, ts);
    }

    if (timeline.empty()) {
        std::cerr << "No playable notes after transposition.\n";
        return;
    }

    double durationSec = static_cast<double>(songDurationUs) / 1e6;
    std::cout << "Loaded " << timeline.size() << " notes | "
              << "Duration: ~" << static_cast<int>(durationSec) << "s\n";
    std::cout << "Press Ctrl+C at any time to stop playback.\n\n";

    countdown(3, "Switching to Roblox in");

    using Clock = std::chrono::steady_clock;
    auto startTime = Clock::now();

    for (const auto& note : timeline) {
        if (!g_running) break;
        auto targetTime = startTime + std::chrono::microseconds(note.timestampUs);
        std::this_thread::sleep_until(targetTime);
        if (!g_running) break;
        tapKey(note.key);
    }

    std::cout << "\nPlayback finished.\n";
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main() {
    std::signal(SIGINT, onSigInt);

    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║     Roblox Virtual Piano Player      ║\n";
    std::cout << "╚══════════════════════════════════════╝\n\n";
    std::cout << "NOTE: This app needs Accessibility access to inject keys.\n";
    std::cout << "      Grant it in System Settings → Privacy → Accessibility.\n\n";

    while (g_running) {
        std::cout << "┌─────────────────────────────┐\n";
        std::cout << "│  1. Play a MIDI file        │\n";
        std::cout << "│  2. Live mode (MIDI keyboard│\n";
        std::cout << "│  Q. Quit                    │\n";
        std::cout << "└─────────────────────────────┘\n";
        std::cout << "Choice: ";

        std::string input;
        if (!std::getline(std::cin, input) || input == "q" || input == "Q") {
            break;
        }

        if (input == "1") {
            runFileMode();
        } else if (input == "2") {
            runLiveMode();
        } else {
            std::cout << "Invalid choice.\n\n";
        }

        // Reset running flag after live mode ends so the menu reappears
        g_running = true;
        std::cout << "\n";
    }

    std::cout << "Goodbye!\n";
    return 0;
}
