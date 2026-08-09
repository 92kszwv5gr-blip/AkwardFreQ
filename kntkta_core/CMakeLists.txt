cmake_minimum_required(VERSION 3.22)
project(KNTKTA VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------ #
# JUCE — fetch if not found locally
# Set JUCE_PATH to your local JUCE installation or let FetchContent get it
# ------------------------------------------------------------------ #
option(KNTKTA_FETCH_JUCE "Download JUCE via FetchContent" ON)

if(KNTKTA_FETCH_JUCE)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        7.0.9
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
else()
    find_package(JUCE REQUIRED PATHS ${JUCE_PATH})
endif()

# ------------------------------------------------------------------ #
# SQLite (bundled amalgamation)
# ------------------------------------------------------------------ #
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/database/sqlite3.c")
    add_library(sqlite3 STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/database/sqlite3.c
    )
else()
    add_library(sqlite3 INTERFACE)
endif()
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/database)

# ------------------------------------------------------------------ #
# KNTKTA Plugin / App
# ------------------------------------------------------------------ #
juce_add_plugin(KNTKTA
    COMPANY_NAME            "KNTKTA"
    PLUGIN_MANUFACTURER_CODE Kntk
    PLUGIN_CODE             Kntk
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "KNTKTA"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES         Fx Tools
    AU_MAIN_TYPE            kAudioUnitType_MIDIProcessor
)

# ------------------------------------------------------------------ #
# Source files
# ------------------------------------------------------------------ #
target_sources(KNTKTA PRIVATE
    Source/Core/KntktaProcessor.cpp
)

target_include_directories(KNTKTA PRIVATE
    Source
    Source/Core
    Source/GUI
)

# ------------------------------------------------------------------ #
# JUCE module dependencies
# ------------------------------------------------------------------ #
target_compile_definitions(KNTKTA PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(KNTKTA PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    sqlite3
)

# ------------------------------------------------------------------ #
# CLAP support (optional)
# ------------------------------------------------------------------ #
option(KNTKTA_BUILD_CLAP "Build CLAP plugin target" OFF)
if(KNTKTA_BUILD_CLAP)
    include(FetchContent)
    FetchContent_Declare(
        clap-juce-extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(clap-juce-extensions)
    clap_juce_extensions_plugin(TARGET KNTKTA CLAP_ID "io.kntkta.kntkta")
endif()
