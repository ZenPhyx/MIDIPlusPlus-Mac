#include "InputInjector.hpp"
#include "NtUserInput.h"
#include <array>
#include <unordered_map>

// White keys — hardware scan codes, no modifier.
static const std::unordered_map<char, WORD> BASE_SCAN = {
    {'a',0x1E},{'s',0x1F},{'d',0x20},{'f',0x21},{'g',0x22},{'h',0x23},
    {'j',0x24},{'k',0x25},{'l',0x26},{'z',0x2C},{'x',0x2D},{'c',0x2E},
    {'v',0x2F},{'b',0x30},{'n',0x31},{'m',0x32},
    {'q',0x10},{'w',0x11},{'e',0x12},{'r',0x13},{'t',0x14},{'y',0x15},
    {'u',0x16},{'i',0x17},{'o',0x18},{'p',0x19},
    {'1',0x02},{'2',0x03},{'3',0x04},{'4',0x05},{'5',0x06},
    {'6',0x07},{'7',0x08},{'8',0x09},{'9',0x0A},{'0',0x0B},
    {' ',0x39},  // Space — sustain pedal
};

// Black keys — base scan code (same physical key as matching white key).
// Shift is injected transiently per-event, not held globally.
static const std::unordered_map<char, WORD> SHIFT_SCAN = {
    {'Q',0x10},{'W',0x11},{'E',0x12},{'T',0x14},{'Y',0x15},
    {'I',0x17},{'O',0x18},{'P',0x19},
    {'S',0x1F},{'D',0x20},{'G',0x22},{'H',0x23},{'J',0x24},{'L',0x26},
    {'Z',0x2C},{'C',0x2E},{'V',0x2F},{'B',0x30},
    {'!',0x02},{'@',0x03},{'$',0x05},{'%',0x06},
    {'^',0x07},{'*',0x09},{'(',0x0A},
};

static const WORD LSHIFT  = 0x2A;
static const WORD LCTRL   = 0x1D;
static const DWORD SC_DN  = KEYEVENTF_SCANCODE;
static const DWORD SC_UP  = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

static INPUT kbd(WORD scan, DWORD flags) {
    INPUT in{};
    in.type       = INPUT_KEYBOARD;
    in.ki.wScan   = scan;
    in.ki.dwFlags = flags;
    return in;
}

void pressKey(char key) {
    // Extended 88-key range: Ctrl+key (high bit set)
    if (isCtrlKey(key)) {
        char base = ctrlBase(key);
        auto bit = BASE_SCAN.find(base);
        if (bit != BASE_SCAN.end()) {
            INPUT inp[3] = { kbd(LCTRL, SC_DN), kbd(bit->second, SC_DN), kbd(LCTRL, SC_UP) };
            NtUserSendInputCall(3, inp, sizeof(INPUT));
        }
        return;
    }
    auto sit = SHIFT_SCAN.find(key);
    if (sit != SHIFT_SCAN.end()) {
        // MIDI++ approach: shift-down, key-down, shift-up — all in one atomic batch.
        INPUT inp[3] = { kbd(LSHIFT, SC_DN), kbd(sit->second, SC_DN), kbd(LSHIFT, SC_UP) };
        NtUserSendInputCall(3, inp, sizeof(INPUT));
        return;
    }
    auto bit = BASE_SCAN.find(key);
    if (bit != BASE_SCAN.end()) {
        INPUT in = kbd(bit->second, SC_DN);
        NtUserSendInputCall(1, &in, sizeof(INPUT));
    }
}

void releaseKey(char key) {
    if (isCtrlKey(key)) {
        char base = ctrlBase(key);
        auto bit = BASE_SCAN.find(base);
        if (bit != BASE_SCAN.end()) {
            INPUT inp[3] = { kbd(LCTRL, SC_DN), kbd(bit->second, SC_UP), kbd(LCTRL, SC_UP) };
            NtUserSendInputCall(3, inp, sizeof(INPUT));
        }
        return;
    }
    auto sit = SHIFT_SCAN.find(key);
    if (sit != SHIFT_SCAN.end()) {
        INPUT inp[3] = { kbd(LSHIFT, SC_DN), kbd(sit->second, SC_UP), kbd(LSHIFT, SC_UP) };
        NtUserSendInputCall(3, inp, sizeof(INPUT));
        return;
    }
    auto bit = BASE_SCAN.find(key);
    if (bit != BASE_SCAN.end()) {
        INPUT in = kbd(bit->second, SC_UP);
        NtUserSendInputCall(1, &in, sizeof(INPUT));
    }
}

void tapKey(char key) { pressKey(key); releaseKey(key); }

void resetModifiers() {
    INPUT in = kbd(LSHIFT, SC_UP);
    NtUserSendInputCall(1, &in, sizeof(INPUT));
}

// ── Live-mode ownership ───────────────────────────────────────────────────────
// Tracks which Roblox key character currently "owns" each scan code.
// '\0' means the scan code is free.  When a new note needs a scan code that is
// already owned by a different character, the owner is evicted first — exactly
// matching MIDI++'s g_scancodeOwner / g_scancodeCount model.
static std::array<char, 256> g_scanOwner{};

static WORD scanOf(char key) {
    if (isCtrlKey(key)) key = ctrlBase(key);
    auto sit = SHIFT_SCAN.find(key);
    if (sit != SHIFT_SCAN.end()) return sit->second;
    auto bit = BASE_SCAN.find(key);
    if (bit != BASE_SCAN.end()) return bit->second;
    return 0;
}

void livePress(char key) {
    WORD scan = scanOf(key);
    if (!scan) return;

    char prev = g_scanOwner[scan];
    if (prev && prev != key) {
        // Evict the current owner so the new note can have the scan code
        releaseKey(prev);
    }
    g_scanOwner[scan] = key;
    pressKey(key);
}

void liveRelease(char key) {
    WORD scan = scanOf(key);
    if (!scan) return;

    // Only release if this key is still the owner (it may have been evicted)
    if (g_scanOwner[scan] == key) {
        g_scanOwner[scan] = '\0';
        releaseKey(key);
    }
}
