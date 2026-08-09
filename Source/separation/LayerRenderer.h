#pragma once

#include <juce_dsp/juce_dsp.h>
#include "RegionTypes.h"

namespace afq
{
    // Builds one isolated audio buffer per LayerType by gating a source bus buffer
    // to each classified region's time range (with short crossfades to avoid
    // clicks at boundaries) and applying a light band-pass matched to that layer's
    // typical spectral range.
    //
    // This is explicitly NOT full source separation — the underlying audio for
    // overlapping events is still whatever Tier A (Demucs) put in that bus. It's a
    // gate + filter reconstruction on top of the coarse stems, which is why v1's
    // stab/zap/glitch/fx layers in particular should be treated as "best guess",
    // not clean isolated stems.
    class LayerRenderer
    {
    public:
        static std::array<juce::AudioBuffer<float>, (size_t) LayerType::Count>
            render (const juce::AudioBuffer<float>& sourceBuffer, double sampleRate,
                    const std::vector<Region>& regions);

    private:
        static void applyTypeFilter (juce::AudioBuffer<float>& buffer, double sampleRate, LayerType type);
    };
}
