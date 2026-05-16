#include "MacInputInjector.hpp"
#include <ApplicationServices/ApplicationServices.h>
#include <unordered_map>

// macOS key codes (kVK_ANSI_*) for every character used by Roblox Virtual Piano.
static const std::unordered_map<char, CGKeyCode> BASE_KEY_MAP = {
    // Letters
    {'a', 0},  {'s', 1},  {'d', 2},  {'f', 3},  {'h', 4},  {'g', 5},
    {'z', 6},  {'x', 7},  {'c', 8},  {'v', 9},  {'b', 11},
    {'q', 12}, {'w', 13}, {'e', 14}, {'r', 15}, {'y', 16}, {'t', 17},
    {'u', 32}, {'i', 34}, {'o', 31}, {'p', 35},
    {'j', 38}, {'k', 40}, {'l', 37},
    {'n', 45}, {'m', 46},
    // Digits
    {'1', 18}, {'2', 19}, {'3', 20}, {'4', 21}, {'5', 23},
    {'6', 22}, {'7', 26}, {'8', 28}, {'9', 25}, {'0', 29},
};

// Shift-symbol characters used by Roblox black keys -> base key code.
// All of these require Shift held down.
static const std::unordered_map<char, CGKeyCode> SHIFT_KEY_MAP = {
    {'!', 18}, // Shift+1
    {'@', 19}, // Shift+2
    {'$', 21}, // Shift+4
    {'%', 23}, // Shift+5
    {'^', 22}, // Shift+6
    {'*', 28}, // Shift+8
    {'(', 25}, // Shift+9
};

static constexpr CGKeyCode SHIFT_KEY = 56;

void tapKey(char key) {
    CGKeyCode code;
    bool needsShift = false;

    if (key >= 'A' && key <= 'Z') {
        char lower = static_cast<char>(key - 'A' + 'a');
        auto it = BASE_KEY_MAP.find(lower);
        if (it == BASE_KEY_MAP.end()) return;
        code = it->second;
        needsShift = true;
    } else {
        auto shiftIt = SHIFT_KEY_MAP.find(key);
        if (shiftIt != SHIFT_KEY_MAP.end()) {
            code = shiftIt->second;
            needsShift = true;
        } else {
            auto baseIt = BASE_KEY_MAP.find(key);
            if (baseIt == BASE_KEY_MAP.end()) return;
            code = baseIt->second;
        }
    }

    CGEventFlags flags = needsShift ? kCGEventFlagMaskShift : 0;

    if (needsShift) {
        CGEventRef shiftDown = CGEventCreateKeyboardEvent(nullptr, SHIFT_KEY, true);
        CGEventPost(kCGHIDEventTap, shiftDown);
        CFRelease(shiftDown);
    }

    CGEventRef keyDown = CGEventCreateKeyboardEvent(nullptr, code, true);
    CGEventRef keyUp   = CGEventCreateKeyboardEvent(nullptr, code, false);
    CGEventSetFlags(keyDown, flags);
    CGEventSetFlags(keyUp,   flags);
    CGEventPost(kCGHIDEventTap, keyDown);
    CGEventPost(kCGHIDEventTap, keyUp);
    CFRelease(keyDown);
    CFRelease(keyUp);

    if (needsShift) {
        CGEventRef shiftUp = CGEventCreateKeyboardEvent(nullptr, SHIFT_KEY, false);
        CGEventPost(kCGHIDEventTap, shiftUp);
        CFRelease(shiftUp);
    }
}
