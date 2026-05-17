#include "RobloxKeyMapper.hpp"
#include "InputInjector.hpp"
#include <array>
#include <climits>
#include <cstdlib>

// ── 88-key Roblox Virtual Piano mapping ───────────────────────────────────────
// Index 0 = MIDI 21 (A0), index 87 = MIDI 108 (C8).
//
// Standard 61 keys (MIDI 36–96): plain ASCII characters, no modifier.
// Extended low  (MIDI 21–35,  A0–B1):  Ctrl + the same digit/letter key.
// Extended high (MIDI 97–108, C#7–C8): Ctrl + the same letter key.
//
// ctrlKey(c) sets the high bit of c as a "needs Ctrl" flag understood by
// both WinInputInjector and MacInputInjector.

static constexpr char CK(char c) {
    return static_cast<char>(static_cast<unsigned char>(c) | 0x80u);
}

static constexpr std::array<char, 88> NOTE_TO_KEY = {
    // ── Low extension: Ctrl+key  (MIDI 21–35, A0–B1) ────────────────────────
    CK('1'), CK('2'), CK('3'),              // A0, A#0, B0
    CK('4'), CK('5'), CK('6'), CK('7'),     // C1, C#1, D1, D#1
    CK('8'), CK('9'), CK('0'),              // E1, F1, F#1
    CK('q'), CK('w'), CK('e'),              // G1, G#1, A1
    CK('r'), CK('t'),                       // A#1, B1

    // ── Standard 61 keys (MIDI 36–96, C2–C7) ─────────────────────────────────
    '1', '!',                               // C2, C#2
    '2', '@',                               // D2, D#2
    '3',                                    // E2
    '4', '$',                               // F2, F#2
    '5', '%',                               // G2, G#2
    '6', '^',                               // A2, A#2
    '7',                                    // B2
    '8', '*',                               // C3, C#3
    '9', '(',                               // D3, D#3
    '0',                                    // E3
    'q', 'Q',                               // F3, F#3
    'w', 'W',                               // G3, G#3
    'e', 'E',                               // A3, A#3
    'r',                                    // B3
    't', 'T',                               // C4, C#4
    'y', 'Y',                               // D4, D#4
    'u',                                    // E4
    'i', 'I',                               // F4, F#4
    'o', 'O',                               // G4, G#4
    'p', 'P',                               // A4, A#4
    'a',                                    // B4
    's', 'S',                               // C5, C#5
    'd', 'D',                               // D5, D#5
    'f',                                    // E5
    'g', 'G',                               // F5, F#5
    'h', 'H',                               // G5, G#5
    'j', 'J',                               // A5, A#5
    'k',                                    // B5
    'l', 'L',                               // C6, C#6
    'z', 'Z',                               // D6, D#6
    'x',                                    // E6
    'c', 'C',                               // F6, F#6
    'v', 'V',                               // G6, G#6
    'b', 'B',                               // A6, A#6
    'n',                                    // B6
    'm',                                    // C7

    // ── High extension: Ctrl+key (MIDI 97–108, C#7–C8) ───────────────────────
    CK('y'), CK('u'), CK('i'),              // C#7, D7, D#7
    CK('o'), CK('p'),                       // E7, F7
    CK('a'), CK('s'), CK('d'),              // F#7, G7, G#7
    CK('f'), CK('g'), CK('h'),              // A7, A#7, B7
    CK('j'),                                // C8
};

static constexpr int MIN_NOTE = 21;
static constexpr int MAX_NOTE = 108;

std::optional<char> RobloxKeyMapper::map(int midiNote) {
    if (midiNote < MIN_NOTE || midiNote > MAX_NOTE)
        return std::nullopt;
    return NOTE_TO_KEY[midiNote - MIN_NOTE];
}

int RobloxKeyMapper::autoTranspose(int lowestNote, int highestNote) {
    if (lowestNote >= MIN_NOTE && highestNote <= MAX_NOTE)
        return 0;

    int bestShift = 0;
    int bestPenalty = INT_MAX;

    for (int shift = -48; shift <= 48; shift += 12) {
        int lo = lowestNote + shift;
        int hi = highestNote + shift;
        if (lo < MIN_NOTE || hi > MAX_NOTE) continue;
        int penalty = std::abs(shift);
        if (penalty < bestPenalty) {
            bestPenalty = penalty;
            bestShift = shift;
        }
    }
    return bestShift;
}
