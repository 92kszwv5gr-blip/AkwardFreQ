#pragma once

#include <juce_dsp/juce_dsp.h>

namespace afq
{
    // Approximate ITU-R BS.1770 loudness meter: K-weighting (high-shelf + highpass
    // approximating the standard's shelf/RLB stages) followed by 400ms gating
    // blocks with the standard's absolute (-70 LUFS) and relative (-10dB below
    // ungated mean) gates.
    //
    // This is NOT a certified/validated LUFS meter — the K-weighting filter
    // coefficients here are a reasonable approximation, not the literal fixed
    // BS.1770 biquad coefficients (which are defined per-sample-rate). Good
    // enough for consistent relative loudness matching within this plugin; do
    // not present its numbers as broadcast-certified LUFS.
    class LoudnessMeter
    {
    public:
        void prepare (double sampleRate, int numChannels);

        // Full-buffer integrated loudness measurement (BS.1770-style two-stage
        // gating), in approximate LUFS. Used offline on a whole track/reference.
        float measureIntegratedLoudness (const juce::AudioBuffer<float>& buffer);

        // Single-block loudness (no gating), useful for a live meter over a
        // short (~400ms) rolling window. Also approximate LUFS.
        float measureShortTermLoudness (const juce::AudioBuffer<float>& block);

    private:
        double sampleRate_ = 44100.0;
        int numChannels_ = 2;

        void applyKWeighting (const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>& out) const;
        static double meanSquareToLkfs (double meanSquare);
    };
}
