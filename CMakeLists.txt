cmake_minimum_required(VERSION 3.20)
project(Snuffiano)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Shared C++ core (cross-platform) ─────────────────────────────────────────
set(CORE_SOURCES
    src/MIDIPlayer.cpp
    src/MIDIParser.cpp
    src/RobloxKeyMapper.cpp
    src/RtMidi.cpp
)

# ─────────────────────────────────────────────────────────────────────────────
if(WIN32)
# ── Windows build ─────────────────────────────────────────────────────────────

    # Download WebView2 SDK (NuGet package = zip file)
    include(FetchContent)
    FetchContent_Declare(wv2
        URL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.3240.44"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(wv2)

    add_executable(Snuffiano WIN32
        ${CORE_SOURCES}
        src/windows/main.cpp
        src/windows/WinInputInjector.cpp
        src/windows/NtUserInput.cpp
        src/windows/resources.rc
    )

    target_include_directories(Snuffiano PRIVATE
        include
        src/windows
        "${wv2_SOURCE_DIR}/build/native/include"
    )
    target_compile_definitions(Snuffiano PRIVATE __WINDOWS_MM__ UNICODE _UNICODE)
    target_compile_options(Snuffiano PRIVATE /utf-8)

    # Static runtime — no VC++ redistributable needed
    set_property(TARGET Snuffiano PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    target_link_libraries(Snuffiano
        "${wv2_SOURCE_DIR}/build/native/x64/WebView2LoaderStatic.lib"
        winmm ole32 oleaut32 version gdi32 comctl32 comdlg32 shell32
    )

# ─────────────────────────────────────────────────────────────────────────────
elseif(APPLE)
# ── macOS build ───────────────────────────────────────────────────────────────

    set(MAC_SOURCES
        src/main.mm
        src/AppDelegate.mm
        src/MainViewController.mm
        src/MacInputInjector.mm
    )

    if(APP_ICON AND EXISTS "${CMAKE_SOURCE_DIR}/assets/AppIcon.icns")
        set(ICON_FILE "${CMAKE_SOURCE_DIR}/assets/AppIcon.icns")
        list(APPEND MAC_SOURCES ${ICON_FILE})
        set_source_files_properties(${ICON_FILE} PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources)
    endif()

    add_executable(Snuffiano MACOSX_BUNDLE ${CORE_SOURCES} ${MAC_SOURCES})

    set_target_properties(Snuffiano PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist"
        MACOSX_BUNDLE_ICON_FILE  "AppIcon"
    )

    target_include_directories(Snuffiano PRIVATE include)

    set_source_files_properties(
        src/main.mm src/AppDelegate.mm src/MainViewController.mm src/MacInputInjector.mm
        PROPERTIES COMPILE_FLAGS "-fobjc-arc"
    )

    target_compile_definitions(Snuffiano PRIVATE __MACOSX_CORE__)

    target_link_libraries(Snuffiano
        "-framework Cocoa"
        "-framework CoreMIDI"
        "-framework CoreAudio"
        "-framework CoreFoundation"
        "-framework ApplicationServices"
    )

endif()
