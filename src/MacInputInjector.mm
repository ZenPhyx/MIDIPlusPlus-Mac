#include "MacInputInjector.hpp"
#include <ApplicationServices/ApplicationServices.h>
#include <unordered_map>

// Hardware keycodes for unshifted keys (letters and digits).
static const std::unordered_map<char, CGKeyCode> BASE_KEY_MAP = {
    {'a', 0},  {'s', 1},  {'d', 2},  {'f', 3},  {'h', 4},  {'g', 5},
    {'z', 6},  {'x', 7},  {'c', 8},  {'v', 9},  {'b', 11},
    {'q', 12}, {'w', 13}, {'e', 14}, {'r', 15}, {'y', 16}, {'t', 17},
    {'u', 32}, {'i', 34}, {'o', 31}, {'p', 35},
    {'j', 38}, {'k', 40}, {'l', 37},
    {'n', 45}, {'m', 46},
    {'1', 18}, {'2', 19}, {'3', 20}, {'4', 21}, {'5', 23},
    {'6', 22}, {'7', 26}, {'8', 28}, {'9', 25}, {'0', 29},
    {' ', 49},  // Space — sustain pedal
};

// Each shifted piano key gets its own phantom keycode (200+, not on any real Mac
// keyboard).  The key character is injected via CGEventKeyboardSetUnicodeString so
// Roblox sees the correct character while the kernel treats it as a completely
// separate key — allowing e.g. 'd' (keycode 2) and 'D' (keycode 209) to be held
// simultaneously without any conflict.
static const std::unordered_map<char, CGKeyCode> PHANTOM_KEY_MAP = {
    {'Q', 200}, {'W', 201}, {'E', 202}, {'T', 203}, {'Y', 204},
    {'I', 205}, {'O', 206}, {'P', 207},
    {'S', 208}, {'D', 209}, {'G', 210}, {'H', 211}, {'J', 212}, {'L', 213},
    {'Z', 214}, {'C', 215}, {'V', 216}, {'B', 217},
    {'!', 218}, {'@', 219}, {'$', 220}, {'%', 221}, {'^', 222}, {'*', 223}, {'(', 224},
};

static void postBase(CGKeyCode code, bool down) {
    CGEventRef e = CGEventCreateKeyboardEvent(nullptr, code, down);
    CGEventPost(kCGSessionEventTap, e);
    CFRelease(e);
}

// Posts a phantom key event whose keycode doesn't exist on real hardware.
// The unicode string tells the receiving app what character it represents.
static void postPhantom(char c, CGKeyCode code, bool down) {
    UniChar uc = static_cast<UniChar>(static_cast<unsigned char>(c));
    CGEventRef e = CGEventCreateKeyboardEvent(nullptr, code, down);
    CGEventKeyboardSetUnicodeString(e, 1, &uc);
    CGEventPost(kCGSessionEventTap, e);
    CFRelease(e);
}

void pressKey(char key) {
    auto pit = PHANTOM_KEY_MAP.find(key);
    if (pit != PHANTOM_KEY_MAP.end()) { postPhantom(key, pit->second, true); return; }
    auto bit = BASE_KEY_MAP.find(key);
    if (bit != BASE_KEY_MAP.end()) postBase(bit->second, true);
}

void releaseKey(char key) {
    auto pit = PHANTOM_KEY_MAP.find(key);
    if (pit != PHANTOM_KEY_MAP.end()) { postPhantom(key, pit->second, false); return; }
    auto bit = BASE_KEY_MAP.find(key);
    if (bit != BASE_KEY_MAP.end()) postBase(bit->second, false);
}

void tapKey(char key) { pressKey(key); releaseKey(key); }
void resetModifiers() {}  // no real modifiers held — no-op

// Phantom keycodes make scan-code conflicts impossible on Mac,
// so live mode ownership is unnecessary — just delegate directly.
void livePress(char key)   { pressKey(key); }
void liveRelease(char key) { releaseKey(key); }
