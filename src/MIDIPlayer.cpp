#include "MIDIPlayer.hpp"
#include "midi_parser.h"
#include "RobloxKeyMapper.hpp"
#include "InputInjector.hpp"

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

static std::vector<TimedNote> buildTimeline(const MidiFile& midi, int transpose, char sustainKey) {
    struct RawEvent { uint32_t tick; int note; bool press; };
    std::vector<RawEvent> raw;
    int lo = 127, hi = 0;

    for (const auto& track : midi.tracks) {
        for (const auto& ev : track.events) {
            uint8_t type = ev.status & 0xF0;
            bool isOn  = (type == 0x90 && ev.data2 > 0);
            bool isOff = (type == 0x80) || (type == 0x90 && ev.data2 == 0);
            if (isOn)  { raw.push_back({ev.absoluteTick, ev.data1, true});
                         lo = std::min(lo,(int)ev.data1); hi = std::max(hi,(int)ev.data1); }
            else if (isOff) raw.push_back({ev.absoluteTick, ev.data1, false});
        }
    }
    if (raw.empty()) return {};
    std::sort(raw.begin(), raw.end(), [](auto& a, auto& b){ return a.tick < b.tick; });

    std::vector<TimedNote> tl;
    for (auto& r : raw) {
        auto key = RobloxKeyMapper::map(r.note + transpose);
        if (key) tl.push_back({tickToUs(r.tick, midi.division, midi.tempoChanges), *key, r.press});
    }

    // CC64 = sustain pedal — map to the user-configured key (default: Space)
    if (sustainKey) {
        for (const auto& track : midi.tracks) {
            for (const auto& ev : track.events) {
                if ((ev.status & 0xF0) == 0xB0 && ev.data1 == 64) {
                    bool on = ev.data2 >= 64;
                    tl.push_back({tickToUs(ev.absoluteTick, midi.division, midi.tempoChanges),
                                  sustainKey, on});
                }
            }
        }
        std::sort(tl.begin(), tl.end(), [](const TimedNote& a, const TimedNote& b){
            return a.us < b.us;
        });
    }
    return tl;
}

static MidiFile parseMidi(const std::string& path) {
    std::string p = path;
    if (!p.empty() && p.front() == '~') {
        const char* home = std::getenv("HOME");
        if (home) p = std::string(home) + p.substr(1);
    }
    MidiParser parser;
    return parser.parse(p);
}

// Map a piano key character to its base scan-code identity (case-insensitive,
// symbols mapped back to their number key). Two keys conflict when they share
// the same physical key on the keyboard — e.g. 'c' and 'C', '1' and '!'.
static char keyBase(char k) {
    if (k >= 'A' && k <= 'Z') return (char)(k - 'A' + 'a');
    if (k == '!') return '1'; if (k == '@') return '2';
    if (k == '$') return '4'; if (k == '%') return '5';
    if (k == '^') return '6'; if (k == '*') return '8';
    if (k == '(') return '9';
    return k;
}

#ifdef _WIN32
// On Windows, keys that share a scan code (e.g. 'c' and 'C') cannot be held
// simultaneously. When two notes at the same timestamp conflict, stagger the
// second one by 15 ms — inaudible to humans, but both notes play correctly.
static void staggerConflicts(std::vector<TimedNote>& tl) {
    for (size_t i = 0; i + 1 < tl.size(); ++i) {
        if (tl[i].us == tl[i+1].us && keyBase(tl[i].key) == keyBase(tl[i+1].key))
            tl[i+1].us += 15000;
    }
    std::sort(tl.begin(), tl.end(), [](const TimedNote& a, const TimedNote& b){
        return a.us < b.us;
    });
}
#endif

static bool countdown(std::atomic<bool>& running, MIDIPlayer::StatusCallback cb) {
    for (int i = 3; i > 0; --i) {
        if (!running) return false;
        cb("Starting in " + std::to_string(i) + "...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return running.load();
}

// ─── Public API ──────────────────────────────────────────────────────────────

double MIDIPlayer::fileDuration(const std::string& path) {
    try {
        auto midi = parseMidi(path);
        if (midi.tempoChanges.empty()) midi.tempoChanges.push_back({0, 500000});
        std::sort(midi.tempoChanges.begin(), midi.tempoChanges.end(),
                  [](auto& a, auto& b){ return a.tick < b.tick; });
        uint32_t lastTick = 0;
        for (const auto& track : midi.tracks)
            for (const auto& ev : track.events)
                lastTick = std::max(lastTick, ev.absoluteTick);
        return tickToUs(lastTick, midi.division, midi.tempoChanges) / 1e6;
    } catch (...) { return 0.0; }
}

void MIDIPlayer::stop() {
    m_running = false;
    m_paused  = false;
    m_pauseCv.notify_all();
    if (m_thread.joinable()) m_thread.join();
    m_position = 0.0;
}

void MIDIPlayer::pause() {
    m_paused = true;
}

void MIDIPlayer::resume() {
    m_paused = false;
    m_pauseCv.notify_all();
}

void MIDIPlayer::seek(double seconds) {
    m_seekTarget.store(seconds < 0.0 ? 0.0 : seconds);
    if (m_paused.load()) { m_paused = false; m_pauseCv.notify_all(); }
}

void MIDIPlayer::setSpeed(double speed) {
    m_speed.store(std::max(0.25, std::min(2.0, speed)));
}

void MIDIPlayer::setSustainKey(char key) {
    m_sustainKey.store(key);
}

void MIDIPlayer::playFile(const std::string& path,
                          KeyCallback    onKey,
                          StatusCallback onStatus,
                          DoneCallback   onDone) {
    stop();
    m_running    = true;
    m_paused     = false;
    m_position   = 0.0;
    m_seekTarget = -1.0;

    m_thread = std::thread([this, path, onKey, onStatus, onDone]() {
        onStatus("Loading...");
        MidiFile midi;
        try { midi = parseMidi(path); }
        catch (const std::exception& ex) {
            onStatus(std::string("Error: ") + ex.what());
            onDone(); return;
        }

        if (midi.tempoChanges.empty()) midi.tempoChanges.push_back({0, 500000});
        std::sort(midi.tempoChanges.begin(), midi.tempoChanges.end(),
                  [](auto& a, auto& b){ return a.tick < b.tick; });

        int lo = 127, hi = 0;
        for (const auto& track : midi.tracks)
            for (const auto& ev : track.events)
                if ((ev.status & 0xF0) == 0x90 && ev.data2 > 0) {
                    lo = std::min(lo,(int)ev.data1); hi = std::max(hi,(int)ev.data1);
                }

        int shift     = RobloxKeyMapper::autoTranspose(lo, hi);
        auto timeline = buildTimeline(midi, shift, m_sustainKey.load());

        if (timeline.empty()) {
            onStatus("Error: No playable notes found.");
            onDone(); return;
        }

        double dur = timeline.back().us / 1e6;
        m_duration.store(dur);

#ifdef _WIN32
        staggerConflicts(timeline);
#endif

        std::string info = std::to_string(timeline.size()) + " notes";
        if (shift) info += "  (transposed " + (shift > 0 ? std::string("+") : "") + std::to_string(shift) + ")";
        onStatus(info);

        if (!countdown(m_running, onStatus)) { onDone(); return; }
        onStatus("Playing \xe2\x99\xaa");

        using Clock = std::chrono::steady_clock;
        auto start  = Clock::now();
        size_t i    = 0;

        while (i < timeline.size() && m_running.load()) {
            // Seek
            double seekVal = m_seekTarget.exchange(-1.0);
            if (seekVal >= 0.0) {
                double seekUs = seekVal * 1e6;
                i = 0;
                while (i < timeline.size() && timeline[i].us < (int64_t)seekUs) ++i;
                start = Clock::now() - std::chrono::microseconds((int64_t)(seekUs / m_speed.load()));
                m_position.store(seekVal);
                continue;
            }

            // Pause
            if (m_paused.load()) {
                auto pauseStart = Clock::now();
                {
                    std::unique_lock<std::mutex> lk(m_pauseMu);
                    m_pauseCv.wait(lk, [this]{ return !m_paused.load() || !m_running.load(); });
                }
                start += Clock::now() - pauseStart;
                if (!m_running.load()) break;
                continue;
            }

            // Sleep until note fires
            double sp  = m_speed.load();
            auto target = start + std::chrono::microseconds((int64_t)(timeline[i].us / sp));

            while (m_running.load() && !m_paused.load() && m_seekTarget.load() < 0.0) {
                auto now = Clock::now();
                if (now >= target) break;
                auto rem = std::chrono::duration_cast<std::chrono::microseconds>(target - now);
                std::this_thread::sleep_for(std::min(rem, std::chrono::microseconds(8'000)));
                auto el = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start);
                m_position.store(std::min(el.count() * sp / 1e6, dur));
            }

            if (!m_running.load()) break;
            if (m_paused.load() || m_seekTarget.load() >= 0.0) continue;

            onKey(timeline[i].key, timeline[i].press);
            auto el = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start);
            m_position.store(std::min(el.count() * sp / 1e6, dur));
            ++i;
        }

        m_position.store(0.0);
        if (m_running) onStatus("Done!");
        onDone();
    });
}
