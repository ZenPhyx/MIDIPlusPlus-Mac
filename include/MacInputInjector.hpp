#pragma once

void pressKey(char key);    // key down
void releaseKey(char key);  // key up
void tapKey(char key);      // instant press + release
void resetModifiers();      // release any stuck modifier keys
