#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace afq
{
    // ITU-R BS.1770-4 loudness meter for mono and stereo material: the standard's two K-weighting stages (a high
    // shelf, then the RLB high-pass), the sum of the channel powers, 400 ms blocks with 75% overlap, and the
    // absolute (-70 LUFS) and relative (-10 LU) gates. Verified against the standard's published 48 kHz filter
    // coefficients and the EBU Tech 3341 sine reference (see tests/LoudnessMeterTests.cpp).
    //
    // Not covered: surround channel weights (every channel is weighted 1.0, so for more than two channels the
    // result is not the standard's), loudness range and true-peak. Not a certified meter.
    class LoudnessMeter
    {
    public:
        // One biquad section, transposed direct form II, in double precision: the RLB high-pass has poles very
        // close to the unit circle, which single precision resolves poorly.
        struct Biquad
        {
            double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        };

        // The standard's pre-filter (high shelf) and RLB high-pass for any sample rate, from its analogue
        // prototype parameters via the bilinear transform. At 48 kHz they equal the coefficients in BS.1770-4.
        static std::array<Biquad, 2> kWeightingCoefficients (double sampleRate);

        void prepare (double sampleRate, int numChannels);

        // Integrated loudness of a whole buffer with two-stage gating, in LUFS. Allocates: use it offline. Returns
        // -70 for input shorter than one 400 ms block or entirely below the absolute gate.
        float measureIntegratedLoudness (const juce::AudioBuffer<float>& buffer) const;

        // Loudness of one block with no gating, in LUFS. Does not allocate, so it is safe on the audio thread. The
        // filters start from rest on every call, so a block much shorter than 400 ms reads slightly off at low frequencies.
        float measureShortTermLoudness (const juce::AudioBuffer<float>& block) const;

    private:
        double sampleRate_ = 44100.0;
        std::array<Biquad, 2> kWeighting_ = kWeightingCoefficients (44100.0);

        static double powerToLufs (double power);
    };
}
