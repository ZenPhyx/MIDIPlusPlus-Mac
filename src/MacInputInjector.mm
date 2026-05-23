#include "MacInputInjector.hpp"
#include "InputInjector.hpp"
#include <ApplicationServices/ApplicationServices.h>
#include <unordered_map>

// Hardware keycodes for unshifted keys (letters, digits, space, tab).
static const std::unordered_map<char, CGKeyCode> BASE_KEY_MAP = {
    {'a', 0},  {'s', 1},  {'d', 2},  {'f', 3},  {'h', 4},  {'g', 5},
    {'z', 6},  {'x', 7},  {'c', 8},  {'v', 9},  {'b', 11},
    {'q', 12}, {'w', 13}, {'e', 14}, {'r', 15}, {'y', 16}, {'t', 17},
    {'u', 32}, {'i', 34}, {'o', 31}, {'p', 35},
    {'j', 38}, {'k', 40}, {'l', 37},
    {'n', 45}, {'m', 46},
    {'1', 18}, {'2', 19}, {'3', 20}, {'4', 21}, {'5', 23},
    {'6', 22}, {'7', 26}, {'8', 28}, {'9', 25}, {'0', 29},
    {' ', 49}, // Space — default sustain pedal
    {'\t', 48}, // Tab — alternate sustain key
};

// Black keys — maps each shifted character back to its unshifted base.
// Shift is injected transiently (down → key → up), never held globally.
// This mirrors the Windows transient-shift model exactly.
static const std::unordered_map<char, char> SHIFT_BASE = {
    {'Q','q'}, {'W','w'}, {'E','e'}, {'T','t'}, {'Y','y'},
    {'I','i'}, {'O','o'}, {'P','p'},
    {'S','s'}, {'D','d'}, {'G','g'}, {'H','h'}, {'J','j'}, {'L','l'},
    {'Z','z'}, {'C','c'}, {'V','v'}, {'B','b'},
    {'!','1'}, {'@','2'}, {'$','4'}, {'%','5'},
    {'^','6'}, {'*','8'}, {'(','9'},
};

static const CGKeyCode LSHIFT_CODE = 56;

static void postKey(CGKeyCode code, bool down, CGEventFlags flags = 0) {
    CGEventRef e = CGEventCreateKeyboardEvent(nullptr, code, down);
    if (flags) CGEventSetFlags(e, flags);
    CGEventPost(kCGSessionEventTap, e);
    CFRelease(e);
}

void pressKey(char key) {
    // Extended 88-key range: Ctrl+key (high bit set)
    if (isCtrlKey(key)) {
        char base = ctrlBase(key);
        auto bit = BASE_KEY_MAP.find(base);
        if (bit != BASE_KEY_MAP.end())
            postKey(bit->second, true, kCGEventFlagMaskControl);
        return;
    }
    // Black keys — transient shift: shift-down, key-down, shift-up
    auto sit = SHIFT_BASE.find(key);
    if (sit != SHIFT_BASE.end()) {
        auto bit = BASE_KEY_MAP.find(sit->second);
        if (bit != BASE_KEY_MAP.end()) {
            postKey(LSHIFT_CODE, true);
            postKey(bit->second, true, kCGEventFlagMaskShift);
            postKey(LSHIFT_CODE, false);
        }
        return;
    }
    // White keys — direct key-down
    auto bit = BASE_KEY_MAP.find(key);
    if (bit != BASE_KEY_MAP.end()) postKey(bit->second, true);
}

void releaseKey(char key) {
    if (isCtrlKey(key)) {
        char base = ctrlBase(key);
        auto bit = BASE_KEY_MAP.find(base);
        if (bit != BASE_KEY_MAP.end())
            postKey(bit->second, false, 0);
        return;
    }
    // Black keys — transient shift: shift-down, key-up, shift-up
    auto sit = SHIFT_BASE.find(key);
    if (sit != SHIFT_BASE.end()) {
        auto bit = BASE_KEY_MAP.find(sit->second);
        if (bit != BASE_KEY_MAP.end()) {
            postKey(LSHIFT_CODE, true);
            postKey(bit->second, false, kCGEventFlagMaskShift);
            postKey(LSHIFT_CODE, false);
        }
        return;
    }
    // White keys — direct key-up
    auto bit = BASE_KEY_MAP.find(key);
    if (bit != BASE_KEY_MAP.end()) postKey(bit->second, false);
}

void tapKey(char key) { pressKey(key); releaseKey(key); }

void resetModifiers() {
    postKey(LSHIFT_CODE, false); // force-release shift if it got stuck
}

// On Mac with transient shift, live press/release is the same as press/release.
// Shift is never held between events so there is no persistent scan-code state.
void livePress(char key)   { pressKey(key); }
void liveRelease(char key) { releaseKey(key); }
