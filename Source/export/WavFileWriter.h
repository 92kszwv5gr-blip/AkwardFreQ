#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace afq
{
    // Shared "write this sample range as a 24-bit .wav" helper — used
    // wherever a slice of an AudioBuffer needs to become its own file.
    //
    // `metadata`, if non-empty, is embedded as a RIFF INFO chunk (JUCE's WAV
    // writer supports the standard FourCC keys — INAM/IART/IGNR/ICMT/ISFT/
    // ICRD etc.). This is the practical equivalent of "ID3 tags" for WAV
    // output: true ID3v2 is an MP3-native spec, RIFF INFO is WAV's own
    // standard tagging mechanism and is what most players/DAWs read from a
    // tagged WAV file. See MetadataPanel for how these key/value pairs get built.
    bool writeWavSlice (const juce::AudioBuffer<float>& source, int64_t startSample, int64_t endSample,
                         double sampleRate, const juce::File& outFile, juce::String& errorMessage,
                         const juce::StringPairArray& metadata = {});
}
