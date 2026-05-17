#pragma once

// Ctrl+key encoding: set the high bit of the ASCII value.
// All standard Roblox piano keys are printable ASCII (< 128), so the high bit
// is free to use as a "needs Control modifier" flag.
inline char        ctrlKey(char c) { return static_cast<char>(static_cast<unsigned char>(c) | 0x80u); }
inline bool        isCtrlKey(char c) { return static_cast<unsigned char>(c) >= 0x80u; }
inline char        ctrlBase(char c) { return static_cast<char>(static_cast<unsigned char>(c) & 0x7Fu); }

// Platform-agnostic key injection interface.
// Implemented by MacInputInjector.mm (macOS) and WinInputInjector.cpp (Windows).
void pressKey(char key);
void releaseKey(char key);
void tapKey(char key);
void resetModifiers();

// Live-mode press/release with scan-code ownership (Windows).
// When two notes share a scan code, the new note evicts the current owner first.
// On Mac, phantom keycodes make conflicts impossible — these just call press/release.
void livePress(char key);
void liveRelease(char key);
