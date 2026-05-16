#include "InputInjector.hpp"
#include "NtUserInput.h"
#include <atomic>
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

// Black keys — same scan code as the matching white key, but need Left Shift.
static const std::unordered_map<char, WORD> SHIFT_SCAN = {
    {'Q',0x10},{'W',0x11},{'E',0x12},{'T',0x14},{'Y',0x15},
    {'I',0x17},{'O',0x18},{'P',0x19},
    {'S',0x1F},{'D',0x20},{'G',0x22},{'H',0x23},{'J',0x24},{'L',0x26},
    {'Z',0x2C},{'C',0x2E},{'V',0x2F},{'B',0x30},
    {'!',0x02},{'@',0x03},{'$',0x05},{'%',0x06},
    {'^',0x07},{'*',0x09},{'(',0x0A},
};

static const WORD LSHIFT = 0x2A;

// Reference count: how many currently-held notes require Left Shift.
// Shift goes down on 0→1 transition, up on 1→0 transition.
// This prevents shift from being released prematurely when multiple
// black keys overlap (e.g. holding A♭ while pressing B♭).
static std::atomic<int> g_shiftUsers{0};

static INPUT makeScan(WORD scan, bool down) {
    INPUT in{};
    in.type       = INPUT_KEYBOARD;
    in.ki.wScan   = scan;
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
    return in;
}

static void sendOne(WORD scan, bool down) {
    INPUT in = makeScan(scan, down);
    NtUserSendInputCall(1, &in, sizeof(INPUT));
}

static void sendTwo(WORD scan0, bool down0, WORD scan1, bool down1) {
    INPUT inp[2] = { makeScan(scan0, down0), makeScan(scan1, down1) };
    NtUserSendInputCall(2, inp, sizeof(INPUT));
}

void pressKey(char key) {
    auto sit = SHIFT_SCAN.find(key);
    if (sit != SHIFT_SCAN.end()) {
        int prev = g_shiftUsers.fetch_add(1);
        if (prev == 0) {
            // First black key down — send shift+key together
            sendTwo(LSHIFT, true, sit->second, true);
        } else {
            // Shift is already held — just send the key
            sendOne(sit->second, true);
        }
        return;
    }
    auto bit = BASE_SCAN.find(key);
    if (bit != BASE_SCAN.end()) sendOne(bit->second, true);
}

void releaseKey(char key) {
    auto sit = SHIFT_SCAN.find(key);
    if (sit != SHIFT_SCAN.end()) {
        int prev = g_shiftUsers.fetch_sub(1);
        if (prev <= 1) {
            // Last black key up — release key+shift together
            sendTwo(sit->second, false, LSHIFT, false);
            g_shiftUsers.store(0);  // clamp negative on double-release
        } else {
            // Other black keys still held — just release this key
            sendOne(sit->second, false);
        }
        return;
    }
    auto bit = BASE_SCAN.find(key);
    if (bit != BASE_SCAN.end()) sendOne(bit->second, false);
}

void tapKey(char key) { pressKey(key); releaseKey(key); }

void resetModifiers() {
    g_shiftUsers.store(0);
    sendOne(LSHIFT, false);
}
