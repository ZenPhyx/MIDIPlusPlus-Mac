#pragma once
#include <optional>
#include <cstdint>

// Maps MIDI note numbers to Roblox Virtual Piano key characters.
//
// Layout (C2=36 to C7=96, 61 keys):
//   White keys : 1 2 3 4 5 6 7 8 9 0 q w e r t y u i o p a s d f g h j k l z x c v b n m
//   Black keys : ! @ $ % ^ * ( Q W E T Y I O P S D G H J L Z C V B
//
// Uppercase letters and special symbols require Shift on the physical keyboard.
class RobloxKeyMapper {
public:
    // Returns the Roblox key char for midiNote, or nullopt if out of range.
    static std::optional<char> map(int midiNote);

    // Returns the semitone shift (positive or negative, multiple of 12) that
    // moves the range [lowestNote, highestNote] into [36, 96].
    // Returns 0 if it already fits; returns the nearest valid shift otherwise.
    static int autoTranspose(int lowestNote, int highestNote);
};
