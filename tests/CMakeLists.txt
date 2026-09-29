# Automated tests. Off by default so normal and Windows cross-builds are unaffected:
#   cmake -B build -DAFQ_BUILD_TESTS=ON ...   then   cmake --build build --target AkwardFreQTests
#   ./build/tests/AkwardFreQTests_artefacts/*/AkwardFreQTests        (or: ctest --test-dir build --output-on-failure)
juce_add_console_app(AkwardFreQTests PRODUCT_NAME "AkwardFreQTests")

target_sources(AkwardFreQTests PRIVATE
    main.cpp
    TestUtils.h
    AnalysisUtilsTests.cpp
    EqualSlicerTests.cpp
    DrumSlicerTests.cpp
    LoopSnapperTests.cpp
    AudioToMidiTests.cpp
    OneShotCleanerTests.cpp
    MasteringTests.cpp
    LoudnessMeterTests.cpp
    LookaheadLimiterTests.cpp
    LoudnessTrimTests.cpp
    RealtimeSafetyTests.cpp
    AllocationCounter.cpp
    AllocationCounter.h

    # The code under test, compiled straight into the runner (no plugin, no ONNX Runtime needed).
    ../Source/separation/AnalysisUtils.cpp
    ../Source/separation/DrumSlicer.cpp
    ../Source/separation/EqualSlicer.cpp
    ../Source/midi/LoopSnapper.cpp
    ../Source/midi/AudioToMidiConverter.cpp
    ../Source/export/OneShotCleaner.cpp
    ../Source/mastering/LoudnessMeter.cpp
    ../Source/mastering/LookaheadLimiter.cpp
    ../Source/mastering/LoudnessTrim.cpp
    ../Source/mastering/MasteringChain.cpp
)

target_include_directories(AkwardFreQTests PRIVATE ../Source)

target_compile_definitions(AkwardFreQTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    AFQ_REFERENCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/reference"
)

target_link_libraries(AkwardFreQTests
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_graphics   # RegionTypes.h (pulled in by AnalysisUtils.h) uses juce::Colour
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Count malloc as well as operator new where GNU ld can wrap it (see AllocationCounter.h).
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(AkwardFreQTests PRIVATE AFQ_WRAP_MALLOC=1)
    target_link_options(AkwardFreQTests PRIVATE -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc)
endif()

add_test(NAME AkwardFreQTests COMMAND AkwardFreQTests)
