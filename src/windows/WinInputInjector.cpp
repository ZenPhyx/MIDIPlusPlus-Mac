#include "InputInjector.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unordered_map>

// White keys — hardware scan codes, no modifier needed.
static const std::unordered_map<char, WORD> BASE_SCAN = {
    {'a',0x1E},{'s',0x1F},{'d',0x20},{'f',0x21},{'g',0x22},{'h',0x23},
    {'j',0x24},{'k',0x25},{'l',0x26},{'z',0x2C},{'x',0x2D},{'c',0x2E},
    {'v',0x2F},{'b',0x30},{'n',0x31},{'m',0x32},
    {'q',0x10},{'w',0x11},{'e',0x12},{'r',0x13},{'t',0x14},{'y',0x15},
    {'u',0x16},{'i',0x17},{'o',0x18},{'p',0x19},
    {'1',0x02},{'2',0x03},{'3',0x04},{'4',0x05},{'5',0x06},
    {'6',0x07},{'7',0x08},{'8',0x09},{'9',0x0A},{'0',0x0B},
};

// Black keys — same scan code as the matching white key, but sent with Left Shift.
// Roblox uses Enum.KeyCode (hardware scan codes), so KEYEVENTF_UNICODE won't work.
// This is the same approach as the original Windows MIDI++ project.
static const std::unordered_map<char, WORD> SHIFT_SCAN = {
    {'Q',0x10},{'W',0x11},{'E',0x12},{'T',0x14},{'Y',0x15},
    {'I',0x17},{'O',0x18},{'P',0x19},
    {'S',0x1F},{'D',0x20},{'G',0x22},{'H',0x23},{'J',0x24},{'L',0x26},
    {'Z',0x2C},{'C',0x2E},{'V',0x2F},{'B',0x30},
    {'!',0x02},{'@',0x03},{'$',0x05},{'%',0x06},
    {'^',0x07},{'*',0x09},{'(',0x0A},
};

static const WORD LSHIFT = 0x2A;  // Left Shift scan code

static INPUT makeScan(WORD scan, bool down) {
    INPUT in = {};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = scan;
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
    return in;
}

void pressKey(char key) {
    auto sit = SHIFT_SCAN.find(key);
    if (sit != SHIFT_SCAN.end()) {
        // Shift down then key down — sent as one atomic batch
        INPUT inp[2] = { makeScan(LSHIFT, true), makeScan(sit->second, true) };
        SendInput(2, inp, sizeof(INPUT));
        return;
    }
    auto bit = BASE_SCAN.find(key);
    if (bit != BASE_SCAN.end()) {
        INPUT in = makeScan(bit->second, true);
        SendInput(1, &in, sizeof(INPUT));
    }
}

void releaseKey(char key) {
    auto sit = SHIFT_SCAN.find(key);
    if (sit != SHIFT_SCAN.end()) {
        // Key up then shift up
        INPUT inp[2] = { makeScan(sit->second, false), makeScan(LSHIFT, false) };
        SendInput(2, inp, sizeof(INPUT));
        return;
    }
    auto bit = BASE_SCAN.find(key);
    if (bit != BASE_SCAN.end()) {
        INPUT in = makeScan(bit->second, false);
        SendInput(1, &in, sizeof(INPUT));
    }
}

void tapKey(char key) { pressKey(key); releaseKey(key); }

void resetModifiers() {
    // Force shift up in case it got stuck
    INPUT in = makeScan(LSHIFT, false);
    SendInput(1, &in, sizeof(INPUT));
}
