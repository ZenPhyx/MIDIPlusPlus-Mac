#include "MacInputInjector.hpp"
#include <ApplicationServices/ApplicationServices.h>
#include <unordered_map>

static const std::unordered_map<char, CGKeyCode> BASE_KEY_MAP = {
    {'a', 0},  {'s', 1},  {'d', 2},  {'f', 3},  {'h', 4},  {'g', 5},
    {'z', 6},  {'x', 7},  {'c', 8},  {'v', 9},  {'b', 11},
    {'q', 12}, {'w', 13}, {'e', 14}, {'r', 15}, {'y', 16}, {'t', 17},
    {'u', 32}, {'i', 34}, {'o', 31}, {'p', 35},
    {'j', 38}, {'k', 40}, {'l', 37},
    {'n', 45}, {'m', 46},
    {'1', 18}, {'2', 19}, {'3', 20}, {'4', 21}, {'5', 23},
    {'6', 22}, {'7', 26}, {'8', 28}, {'9', 25}, {'0', 29},
};

static const std::unordered_map<char, CGKeyCode> SHIFT_KEY_MAP = {
    {'!', 18}, {'@', 19}, {'$', 21}, {'%', 23},
    {'^', 22}, {'*', 28}, {'(', 25},
};

static constexpr CGKeyCode SHIFT_KEY = 56;

static bool resolveKey(char key, CGKeyCode& outCode, bool& outShift) {
    if (key >= 'A' && key <= 'Z') {
        auto it = BASE_KEY_MAP.find(static_cast<char>(key - 'A' + 'a'));
        if (it == BASE_KEY_MAP.end()) return false;
        outCode = it->second; outShift = true; return true;
    }
    auto sit = SHIFT_KEY_MAP.find(key);
    if (sit != SHIFT_KEY_MAP.end()) {
        outCode = sit->second; outShift = true; return true;
    }
    auto bit = BASE_KEY_MAP.find(key);
    if (bit == BASE_KEY_MAP.end()) return false;
    outCode = bit->second; outShift = false; return true;
}

static void post(CGKeyCode code, bool down, CGEventFlags flags = 0) {
    CGEventRef e = CGEventCreateKeyboardEvent(nullptr, code, down);
    CGEventSetFlags(e, flags);
    CGEventPost(kCGSessionEventTap, e);
    CFRelease(e);
}

void pressKey(char key) {
    CGKeyCode code; bool shift;
    if (!resolveKey(key, code, shift)) return;
    if (shift) {
        // Hold shift only long enough to register the key-down, then release it.
        // The key itself stays held — Roblox sustains the note until key-up arrives.
        post(SHIFT_KEY, true,  kCGEventFlagMaskShift);
        post(code,      true,  kCGEventFlagMaskShift);
        post(SHIFT_KEY, false, 0);
    } else {
        post(code, true, 0);
    }
}

void releaseKey(char key) {
    CGKeyCode code; bool shift;
    if (!resolveKey(key, code, shift)) return;
    post(code, false, 0); // shift is already released, just send key-up
}

void tapKey(char key) {
    pressKey(key);
    releaseKey(key);
}

void resetModifiers() {
    // Shift is never held between press and release, so nothing to reset.
    post(SHIFT_KEY, false, 0); // safety: force shift up in case anything got stuck
}
