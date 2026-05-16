#include "InputInjector.hpp"
#include "NtUserInput.h"
#include <array>
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

// Black keys — same scan code as matching white key, injected with Left Shift.
static const std::unordered_map<char, WORD> SHIFT_SCAN = {
    {'Q',0x10},{'W',0x11},{'E',0x12},{'T',0x14},{'Y',0x15},
    {'I',0x17},{'O',0x18},{'P',0x19},
    {'S',0x1F},{'D',0x20},{'G',0x22},{'H',0x23},{'J',0x24},{'L',0x26},
    {'Z',0x2C},{'C',0x2E},{'V',0x2F},{'B',0x30},
    {'!',0x02},{'@',0x03},{'$',0x05},{'%',0x06},
    {'^',0x07},{'*',0x09},{'(',0x0A},
};

static const WORD LSHIFT_SCAN = 0x2A;
static const DWORD SC_FLAG = KEYEVENTF_SCANCODE;
static const DWORD KU_FLAG = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

// Per-scan-code reference count of how many currently-held notes need shift
// on that scan code. Shift goes down on 0→1, up on 1→0 — exactly matching
// the g_scancodeCount ownership model from MIDI++.
static std::array<std::atomic<short>, 256> g_shiftCount{};

static INPUT makeKbd(WORD scan, DWORD flags) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = scan;
    in.ki.dwFlags = flags;
    return in;
}

static void send(WORD scan, DWORD flags) {
    INPUT in = makeKbd(scan, flags);
    NtUserSendInputCall(1, &in, sizeof(INPUT));
}

static void send2(WORD s0, DWORD f0, WORD s1, DWORD f1) {
    INPUT inp[2] = { makeKbd(s0, f0), makeKbd(s1, f1) };
    NtUserSendInputCall(2, inp, sizeof(INPUT));
}

// Total number of held notes that need shift across all scan codes
static int shiftUsersTotal() {
    int total = 0;
    for (auto& c : g_shiftCount) total += c.load(std::memory_order_relaxed);
    return total;
}

void pressKey(char key) {
    auto sit = SHIFT_SCAN.find(key);
    if (sit != SHIFT_SCAN.end()) {
        WORD scan = sit->second;
        int prevTotal = shiftUsersTotal();
        g_shiftCount[scan].fetch_add(1, std::memory_order_relaxed);
        if (prevTotal == 0) {
            // First shifted note anywhere — send shift+key together
            send2(LSHIFT_SCAN, SC_FLAG, scan, SC_FLAG);
        } else {
            // Shift is already held — just press the key
            send(scan, SC_FLAG);
        }
        return;
    }
    auto bit = BASE_SCAN.find(key);
    if (bit != BASE_SCAN.end()) send(bit->second, SC_FLAG);
}

void releaseKey(char key) {
    auto sit = SHIFT_SCAN.find(key);
    if (sit != SHIFT_SCAN.end()) {
        WORD scan = sit->second;
        short prev = g_shiftCount[scan].fetch_sub(1, std::memory_order_relaxed);
        if (prev <= 0) {
            // Clamp — double release guard
            g_shiftCount[scan].store(0, std::memory_order_relaxed);
        }
        int remaining = shiftUsersTotal();
        if (remaining <= 0) {
            // Last shifted note anywhere — release key+shift together
            send2(scan, KU_FLAG, LSHIFT_SCAN, KU_FLAG);
        } else {
            // Other shifted notes still held — just release this key
            send(scan, KU_FLAG);
        }
        return;
    }
    auto bit = BASE_SCAN.find(key);
    if (bit != BASE_SCAN.end()) send(bit->second, KU_FLAG);
}

void tapKey(char key) { pressKey(key); releaseKey(key); }

void resetModifiers() {
    for (auto& c : g_shiftCount) c.store(0, std::memory_order_relaxed);
    send(LSHIFT_SCAN, KU_FLAG);
}
