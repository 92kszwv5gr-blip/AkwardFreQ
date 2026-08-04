#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace afq
{
    class PluginChain;

    // Chops a drum-type buffer (or a sub-range of one) into hits via
    // DrumSlicer and writes each as a named, prefixed .wav — the reliable
    // "drum rack" export path. See Models/Templates/README.md for why there
    // isn't a one-click Ableton Drum Rack (.adg) generator yet.
    class DrumRackExporter
    {
    public:
        enum class SliceMode
        {
            OnsetDetected, // DrumSlicer — follows transients, variable slice count/length
            Equal          // EqualSlicer — exactly N equal-length slices, N from `sliceCount`
        };

        struct Settings
        {
            juce::File destinationFolder;
            juce::String kitName = "AkwardFreQ Kit";
            juce::String prefix;
            SliceMode mode = SliceMode::OnsetDetected;
            int sliceCount = 16; // only used when mode == Equal
        };

        // `rangeEndSample <= rangeStartSample` slices the whole buffer.
        // Writes <destinationFolder>/<prefix><kitName>/<prefix><kitName>_NN.wav
        // per slice, in time order. `vstChain`, if non-null, runs each slice
        // through it (see BatchVstRenderer) before writing.
        static bool exportSlicedDrums (const juce::AudioBuffer<float>& sourceBuffer, double sampleRate,
                                        int64_t rangeStartSample, int64_t rangeEndSample,
                                        const Settings& settings, juce::String& errorMessage,
                                        int* outSliceCount = nullptr, PluginChain* vstChain = nullptr);
    };
}
