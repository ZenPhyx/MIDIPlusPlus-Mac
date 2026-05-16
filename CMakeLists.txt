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

if(APP_ICON AND EXISTS "${CMAKE_SOURCE_DIR}/assets/AppIcon.icns")
    set(ICON_FILE "${CMAKE_SOURCE_DIR}/assets/AppIcon.icns")
    list(APPEND SOURCES ${ICON_FILE})
    set_source_files_properties(${ICON_FILE} PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
endif()

add_executable(RobloxPiano MACOSX_BUNDLE ${SOURCES})

set_target_properties(RobloxPiano PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist"
    MACOSX_BUNDLE_ICON_FILE  "AppIcon"
)

target_include_directories(RobloxPiano PRIVATE include)

# Enable ARC for Objective-C++ UI files
set_source_files_properties(
    src/main.mm
    src/AppDelegate.mm
    src/MainViewController.mm
    PROPERTIES COMPILE_FLAGS "-fobjc-arc"
)

target_compile_definitions(RobloxPiano PRIVATE __MACOSX_CORE__)

target_link_libraries(RobloxPiano
    "-framework Cocoa"
    "-framework CoreMIDI"
    "-framework CoreAudio"
    "-framework CoreFoundation"
    "-framework ApplicationServices"
)
