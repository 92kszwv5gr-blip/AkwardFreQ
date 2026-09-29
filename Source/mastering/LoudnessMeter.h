#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

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

        // Filter memory for one channel's pair of K-weighting sections.
        struct KWeightingState
        {
            std::array<double, 2> z1 { 0.0, 0.0 }, z2 { 0.0, 0.0 };
        };

        // One sample through both sections.
        static double kWeight (const std::array<Biquad, 2>& coefficients, KWeightingState& state, double x) noexcept;

        void prepare (double sampleRate, int numChannels);

        // Mean-square power (summed over channels) to LUFS: -0.691 + 10 log10 (power), or -70 for silence.
        static double powerToLufs (double power);

        // Integrated loudness of a whole buffer with two-stage gating, in LUFS. Allocates: use it offline. Returns
        // -70 for input shorter than one 400 ms block or entirely below the absolute gate.
        float measureIntegratedLoudness (const juce::AudioBuffer<float>& buffer) const;

        // Loudness of one block with no gating, in LUFS. Does not allocate, so it is safe on the audio thread. The
        // filters start from rest on every call, so a block much shorter than 400 ms reads slightly off at low frequencies.
        float measureShortTermLoudness (const juce::AudioBuffer<float>& block) const;

    private:
        double sampleRate_ = 44100.0;
        std::array<Biquad, 2> kWeighting_ = kWeightingCoefficients (44100.0);

    };

    // A loudness meter that runs on a live stream. The K-weighting filters keep their state from block to block, the
    // squared signal is summed into 100 ms hops, and the loudness is read over the last `windowSeconds` (3 s by
    // default, the BS.1770 "short-term" window). Hops quieter than a gate are left out of the average, so pauses do not
    // drag the reading down. Does not allocate after prepare(); safe on the audio thread.
    class StreamingLoudnessMeter
    {
    public:
        static constexpr double kHopSeconds = 0.1;

        struct Reading
        {
            float lufs = -70.0f; // mean power of the counted hops, in LUFS; -70 when none were counted
            int hops = 0;        // how many hops were above the gate
            float latestHopLufs = -70.0f; // the most recent completed hop on its own, gate or no gate
        };

        void prepare (double sampleRate, int numChannels, double windowSeconds = 3.0);
        void reset() noexcept;

        // Feeds a block. Returns true if at least one hop was completed inside it.
        bool process (const juce::AudioBuffer<float>& block) noexcept;

        Reading read (float gateLufs) const noexcept;

    private:
        std::array<LoudnessMeter::Biquad, 2> coefficients_ = LoudnessMeter::kWeightingCoefficients (44100.0);
        std::vector<LoudnessMeter::KWeightingState> filters_; // one per channel
        std::vector<double> hopPower_;                         // ring, most recent last
        int hopSamples_ = 4410;
        int hopFill_ = 0;
        double hopSum_ = 0.0;
        int ringPos_ = 0;
        int ringCount_ = 0;
    };
}
