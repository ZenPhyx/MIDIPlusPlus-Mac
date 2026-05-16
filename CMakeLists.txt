cmake_minimum_required(VERSION 3.20)

project(MIDIPlusPlusMac)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MIDIPlusPlusMac
    src/main.cpp
    src/MacInputInjector.mm
    src/MIDIParser.cpp
    src/TranspositionCore.cpp
    src/ConfigHandler.cpp
    src/RtMidi.cpp
)

target_include_directories(MIDIPlusPlusMac PRIVATE include)

target_compile_definitions(MIDIPlusPlusMac PRIVATE __MACOSX_CORE__)

target_link_libraries(MIDIPlusPlusMac
    "-framework CoreMIDI"
    "-framework CoreAudio"
    "-framework CoreFoundation"
    "-framework ApplicationServices"
)