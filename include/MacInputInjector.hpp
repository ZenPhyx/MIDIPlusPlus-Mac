#pragma once

void pressKey(char key);    // key down (shift released immediately for black keys)
void releaseKey(char key);  // key up
void tapKey(char key);      // instant press + release
void resetModifiers();      // force-release any stuck modifiers
