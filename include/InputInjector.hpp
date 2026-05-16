#pragma once

// Platform-agnostic key injection interface.
// Implemented by MacInputInjector.mm (macOS) and WinInputInjector.cpp (Windows).
void pressKey(char key);
void releaseKey(char key);
void tapKey(char key);
void resetModifiers();
