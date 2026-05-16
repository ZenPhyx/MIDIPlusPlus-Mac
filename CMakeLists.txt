cmake_minimum_required(VERSION 3.20)

project(RobloxPiano)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(RobloxPiano
    src/main.cpp
    src/MacInputInjector.mm
    src/MIDIParser.cpp
    src/RobloxKeyMapper.cpp
    src/RtMidi.cpp
)

target_include_directories(RobloxPiano PRIVATE include)

target_compile_definitions(RobloxPiano PRIVATE __MACOSX_CORE__)

target_link_libraries(RobloxPiano
    "-framework CoreMIDI"
    "-framework CoreAudio"
    "-framework CoreFoundation"
    "-framework ApplicationServices"
)
