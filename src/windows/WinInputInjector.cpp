#include "InputInjector.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unordered_map>

// ── Normal keys: hardware scan codes (no shift needed) ───────────────────────
static const std::unordered_map<char, WORD> BASE_SCAN = {
    {'a',0x1E},{'s',0x1F},{'d',0x20},{'f',0x21},{'g',0x22},{'h',0x23},
    {'j',0x24},{'k',0x25},{'l',0x26},{'z',0x2C},{'x',0x2D},{'c',0x2E},
    {'v',0x2F},{'b',0x30},{'n',0x31},{'m',0x32},
    {'q',0x10},{'w',0x11},{'e',0x12},{'r',0x13},{'t',0x14},{'y',0x15},
    {'u',0x16},{'i',0x17},{'o',0x18},{'p',0x19},
    {'1',0x02},{'2',0x03},{'3',0x04},{'4',0x05},{'5',0x06},
    {'6',0x07},{'7',0x08},{'8',0x09},{'9',0x0A},{'0',0x0B},
};

// ── Shifted piano keys: inject as Unicode characters so each gets its own
//    independent slot in the OS input stream — 'd' (scan 0x20) and 'D'
//    (unicode 0x44) can be held simultaneously without conflict.
static const std::unordered_map<char, WORD> UNICODE_KEYS = {
    {'Q',0x51},{'W',0x57},{'E',0x45},{'T',0x54},{'Y',0x59},
    {'I',0x49},{'O',0x4F},{'P',0x50},
    {'S',0x53},{'D',0x44},{'G',0x47},{'H',0x48},{'J',0x4A},{'L',0x4C},
    {'Z',0x5A},{'C',0x43},{'V',0x56},{'B',0x42},
    {'!',0x21},{'@',0x40},{'$',0x24},{'%',0x25},
    {'^',0x5E},{'*',0x2A},{'(',0x28},
};

static void sendScan(WORD scan, bool down) {
    INPUT in = {};
    in.type       = INPUT_KEYBOARD;
    in.ki.wScan   = scan;
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
    SendInput(1, &in, sizeof(INPUT));
}

static void sendUnicode(WORD ch, bool down) {
    INPUT in = {};
    in.type       = INPUT_KEYBOARD;
    in.ki.wScan   = ch;
    in.ki.dwFlags = KEYEVENTF_UNICODE | (down ? 0 : KEYEVENTF_KEYUP);
    SendInput(1, &in, sizeof(INPUT));
}

void pressKey(char key) {
    auto uit = UNICODE_KEYS.find(key);
    if (uit != UNICODE_KEYS.end()) { sendUnicode(uit->second, true); return; }
    auto bit = BASE_SCAN.find(key);
    if (bit != BASE_SCAN.end()) sendScan(bit->second, true);
}

void releaseKey(char key) {
    auto uit = UNICODE_KEYS.find(key);
    if (uit != UNICODE_KEYS.end()) { sendUnicode(uit->second, false); return; }
    auto bit = BASE_SCAN.find(key);
    if (bit != BASE_SCAN.end()) sendScan(bit->second, false);
}

void tapKey(char key) { pressKey(key); releaseKey(key); }
void resetModifiers()  {} // no physical modifiers held
