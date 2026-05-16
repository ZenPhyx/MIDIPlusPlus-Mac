cmake_minimum_required(VERSION 3.20)
project(RobloxPiano)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.mm
    src/AppDelegate.mm
    src/MainViewController.mm
    src/MIDIPlayer.cpp
    src/MacInputInjector.mm
    src/MIDIParser.cpp
    src/RobloxKeyMapper.cpp
    src/RtMidi.cpp
)

add_executable(RobloxPiano MACOSX_BUNDLE ${SOURCES})

set_target_properties(RobloxPiano PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist"
)

target_include_directories(RobloxPiano PRIVATE include)

target_compile_definitions(RobloxPiano PRIVATE __MACOSX_CORE__)

target_link_libraries(RobloxPiano
    "-framework Cocoa"
    "-framework CoreMIDI"
    "-framework CoreAudio"
    "-framework CoreFoundation"
    "-framework ApplicationServices"
)
