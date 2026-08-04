#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace afq
{
    // Chops a drum-type buffer (or a sub-range of one) into hits via
    // DrumSlicer and writes each as a named, prefixed .wav — the reliable
    // "drum rack" export path. See Models/Templates/README.md for why there
    // isn't a one-click Ableton Drum Rack (.adg) generator yet.
    class DrumRackExporter
    {
    public:
        struct Settings
        {
            juce::File destinationFolder;
            juce::String kitName = "AkwardFreQ Kit";
            juce::String prefix;
        };

        // `rangeEndSample <= rangeStartSample` slices the whole buffer.
        // Writes <destinationFolder>/<prefix><kitName>/<prefix><kitName>_NN.wav
        // per slice, in time order.
        static bool exportSlicedDrums (const juce::AudioBuffer<float>& sourceBuffer, double sampleRate,
                                        int64_t rangeStartSample, int64_t rangeEndSample,
                                        const Settings& settings, juce::String& errorMessage,
                                        int* outSliceCount = nullptr);
    };
}
