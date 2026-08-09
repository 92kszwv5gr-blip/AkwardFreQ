#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace afq
{
    // One chopped hit from DrumSlicer::slice — plain sample-range data, not a
    // classified Region (slicing is purely rhythmic/transient-based, no layer
    // labeling involved).
    struct DrumSlice
    {
        int64_t startSample = 0;
        int64_t endSample = 0;
        int index = 0; // 1-based position in the slice sequence, for naming
    };

    // Chops a drum-type buffer (or a sub-range of one, e.g. a detected
    // Breakbeat region) into individual hits via onset detection. This is
    // rhythmic slicing, not layer classification — feed it any drum-ish
    // audio (the whole `drums` bus, a single Breakbeat region, a raw
    // imported break loop) and it returns hit boundaries.
    class DrumSlicer
    {
    public:
        // `rangeEndSample <= rangeStartSample` slices the whole buffer.
        // `maxSlices` caps runaway slice counts on long/noisy input (extra
        // onsets beyond the cap are dropped, not merged).
        static std::vector<DrumSlice> slice (const juce::AudioBuffer<float>& sourceBuffer, double sampleRate,
                                              int64_t rangeStartSample = 0, int64_t rangeEndSample = -1,
                                              int maxSlices = 64);
    };
}
