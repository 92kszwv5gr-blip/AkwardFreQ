#pragma once

#include <juce_dsp/juce_dsp.h>
#include "RegionTypes.h"
#include "LayerClassifier.h"
#include "../Params.h"

namespace afq
{
    // Tier C: splits Demucs' coarse "other" bus into SynthLead / Stabs /
    // Atmosphere / FX / Zap / Glitch regions.
    //
    // Two complementary passes:
    //  1. Onset-based: short segments starting at each detected onset, classified
    //     among the transient-ish types (Stabs, Zap, Glitch, FX, SynthLead attacks).
    //  2. Sustain-based: the long gaps between onsets (>500ms, energy above the
    //     silence floor) are analyzed as their own segment and classified among
    //     the sustained types (SynthLead, Atmosphere) — this is what actually
    //     catches pad/atmosphere layers, which mostly don't have sharp onsets.
    class OtherSubSplitter
    {
    public:
        explicit OtherSubSplitter (LayerClassifier& classifier);

        std::vector<Region> splitRegions (const juce::AudioBuffer<float>& otherBuffer, double sampleRate,
                                           params::GenrePreset genre = params::GenrePreset::Generic);

    private:
        LayerClassifier& classifier_;
    };
}
