#pragma once

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
