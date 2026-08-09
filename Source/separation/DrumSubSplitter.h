#pragma once

#include <juce_dsp/juce_dsp.h>
#include "RegionTypes.h"
#include "LayerClassifier.h"
#include "../Params.h"

namespace afq
{
    // Tier B: splits Demucs' coarse "drums" bus into Kick / HiHat / Percussion /
    // Breakbeat regions via onset detection + per-hit feature classification,
    // plus a loop-repetition pass that flags candidate breakbeat windows.
    class DrumSubSplitter
    {
    public:
        explicit DrumSubSplitter (LayerClassifier& classifier);

        // `drumsBuffer` is Demucs' "drums" stem for the whole track. Returns
        // classified regions covering the buffer's non-silent onsets.
        std::vector<Region> splitRegions (const juce::AudioBuffer<float>& drumsBuffer, double sampleRate,
                                           params::GenrePreset genre = params::GenrePreset::Generic);

    private:
        LayerClassifier& classifier_;

        // Returns [start,end) sample ranges flagged as repeating rhythmic loops
        // (candidate breakbeats) via bar-to-bar onset-pattern similarity.
        std::vector<std::pair<int64_t, int64_t>> detectBreakbeatWindows (
            const std::vector<int64_t>& onsets, int64_t totalLengthSamples, double sampleRate);
    };
}
