cmake_minimum_required(VERSION 3.22)

project(AkwardFreQ VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

#-------------------------------------------------------------------------------
# ONNX Runtime — not vendored in this repo. Download a prebuilt release
# (https://github.com/microsoft/onnxruntime/releases, the "onnxruntime-win-x64-<ver>.zip"
# asset for Windows), extract it anywhere, and point CMake at it:
#   cmake -B build -DONNXRUNTIME_ROOT_DIR="C:/path/to/onnxruntime-win-x64-1.17.0"
# See README.md for the full Windows build walkthrough.
#-------------------------------------------------------------------------------
set(ONNXRUNTIME_ROOT_DIR "" CACHE PATH "Path to an extracted onnxruntime release (must contain include/ and lib/)")
if (ONNXRUNTIME_ROOT_DIR STREQUAL "")
    message(FATAL_ERROR "ONNXRUNTIME_ROOT_DIR is not set. Download a prebuilt onnxruntime release and pass "
                         "-DONNXRUNTIME_ROOT_DIR=<path>. See README.md.")
endif()

add_library(onnxruntime SHARED IMPORTED GLOBAL)
if (WIN32)
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_IMPLIB "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime.lib"
        IMPORTED_LOCATION "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime.dll"
    )
elseif (APPLE)
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ONNXRUNTIME_ROOT_DIR}/lib/libonnxruntime.dylib"
    )
else()
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ONNXRUNTIME_ROOT_DIR}/lib/libonnxruntime.so"
    )
endif()
target_include_directories(onnxruntime INTERFACE "${ONNXRUNTIME_ROOT_DIR}/include")

#-------------------------------------------------------------------------------
# Plugin target
#-------------------------------------------------------------------------------
juce_add_plugin(AkwardFreQ
    COMPANY_NAME                "AkwardFreQ"
    PLUGIN_MANUFACTURER_CODE    Akfq
    PLUGIN_CODE                 Akf1
    FORMATS                     VST3
    PRODUCT_NAME                "AkwardFreQ"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx
    COPY_PLUGIN_AFTER_BUILD     TRUE
)

target_sources(AkwardFreQ PRIVATE
    Source/Params.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/separation/AnalysisUtils.cpp
    Source/separation/LayerRenderer.cpp
    Source/separation/GenreBias.cpp
    Source/separation/LayerClassifier.cpp
    Source/separation/DrumSubSplitter.cpp
    Source/separation/OtherSubSplitter.cpp
    Source/separation/DemucsEngine.cpp
    Source/separation/SeparationEngine.cpp
    Source/separation/TrainingDataExporter.cpp
    Source/separation/DrumSlicer.cpp

    Source/mastering/LoudnessMeter.cpp
    Source/mastering/MasteringChain.cpp

    Source/export/SamplePackExporter.cpp
    Source/export/WavFileWriter.cpp
    Source/export/SfzExporter.cpp
    Source/export/AbletonPresetWriter.cpp
    Source/export/DrumRackExporter.cpp

    Source/midi/AudioToMidiConverter.cpp
    Source/midi/LoopSnapper.cpp

    Source/ui/WaveformRegionView.cpp
    Source/ui/RegionListPanel.cpp
    Source/ui/MasteringPanel.cpp
    Source/ui/ExportPanel.cpp
    Source/ui/InstrumentExportPanel.cpp
    Source/ui/DrumRackPanel.cpp
    Source/ui/MidiPanel.cpp
)

target_include_directories(AkwardFreQ PRIVATE Source)

target_compile_definitions(AkwardFreQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(AkwardFreQ
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
        onnxruntime
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# onnxruntime is a shared library — the DAW needs to find it at load time.
# Copy it next to the built VST3 bundle on Windows.
if (WIN32)
    add_custom_command(TARGET AkwardFreQ_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime.dll"
            "$<TARGET_FILE_DIR:AkwardFreQ_VST3>"
        COMMENT "Copying onnxruntime.dll next to AkwardFreQ.vst3"
    )
endif()
