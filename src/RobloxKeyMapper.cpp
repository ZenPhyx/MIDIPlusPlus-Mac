#include "RobloxKeyMapper.hpp"
#include <array>
#include <climits>
#include <cstdlib>

// Full 61-key Roblox Virtual Piano mapping.
// Index 0 = MIDI note 36 (C2), index 60 = MIDI note 96 (C7).
// '\0' marks gaps (should not occur with a valid 61-key layout).
static constexpr std::array<char, 61> NOTE_TO_KEY = {
    '1', '!',       // C2, C#2
    '2', '@',       // D2, D#2
    '3',            // E2
    '4', '$',       // F2, F#2
    '5', '%',       // G2, G#2
    '6', '^',       // A2, A#2
    '7',            // B2
    '8', '*',       // C3, C#3
    '9', '(',       // D3, D#3
    '0',            // E3
    'q', 'Q',       // F3, F#3
    'w', 'W',       // G3, G#3
    'e', 'E',       // A3, A#3
    'r',            // B3
    't', 'T',       // C4, C#4
    'y', 'Y',       // D4, D#4
    'u',            // E4
    'i', 'I',       // F4, F#4
    'o', 'O',       // G4, G#4
    'p', 'P',       // A4, A#4
    'a',            // B4
    's', 'S',       // C5, C#5
    'd', 'D',       // D5, D#5
    'f',            // E5
    'g', 'G',       // F5, F#5
    'h', 'H',       // G5, G#5
    'j', 'J',       // A5, A#5
    'k',            // B5
    'l', 'L',       // C6, C#6
    'z', 'Z',       // D6, D#6
    'x',            // E6
    'c', 'C',       // F6, F#6
    'v', 'V',       // G6, G#6
    'b', 'B',       // A6, A#6
    'n',            // B6
    'm',            // C7
};

static constexpr int MIN_NOTE = 36;
static constexpr int MAX_NOTE = 96;

std::optional<char> RobloxKeyMapper::map(int midiNote) {
    if (midiNote < MIN_NOTE || midiNote > MAX_NOTE)
        return std::nullopt;
    char k = NOTE_TO_KEY[midiNote - MIN_NOTE];
    if (k == '\0')
        return std::nullopt;
    return k;
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
