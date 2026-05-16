#include "MacInputInjector.hpp"

#include <ApplicationServices/ApplicationServices.h>
#include <unordered_map>
#include <iostream>
#include <cctype>

static std::unordered_map<char, CGKeyCode> keyMap = {
    {'a', 0}, {'s', 1}, {'d', 2}, {'f', 3}, {'h', 4}, {'g', 5},
    {'z', 6}, {'x', 7}, {'c', 8}, {'v', 9}, {'b', 11},
    {'q', 12}, {'w', 13}, {'e', 14}, {'r', 15}, {'y', 16},
    {'t', 17}, {'1', 18}, {'2', 19}, {'3', 20}, {'4', 21},
    {'6', 22}, {'5', 23}, {'9', 25}, {'7', 26}, {'8', 28},
    {'0', 29}, {'o', 31}, {'u', 32}, {'i', 34}, {'p', 35},
    {'l', 37}, {'j', 38}, {'k', 40}, {'n', 45}, {'m', 46}
};

void tapKey(char key) {
    key = std::tolower(key);

    if (!keyMap.count(key)) {
        std::cout << "Unsupported key: " << key << std::endl;
        return;
    }

    CGKeyCode code = keyMap[key];

    CGEventRef down = CGEventCreateKeyboardEvent(nullptr, code, true);
    CGEventRef up = CGEventCreateKeyboardEvent(nullptr, code, false);

    CGEventPost(kCGHIDEventTap, down);
    CGEventPost(kCGHIDEventTap, up);

    CFRelease(down);
    CFRelease(up);
}