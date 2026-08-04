#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace afq
{
    // Shared "write this sample range as a 24-bit .wav" helper — used
    // wherever a slice of an AudioBuffer needs to become its own file.
    bool writeWavSlice (const juce::AudioBuffer<float>& source, int64_t startSample, int64_t endSample,
                         double sampleRate, const juce::File& outFile, juce::String& errorMessage);
}
