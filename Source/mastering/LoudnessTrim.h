#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include "LoudnessMeter.h"

namespace afq
{
    // Automatic gain that steers the loudness of the finished output toward a target, slowly enough to leave the
    // music's own dynamics alone.
    //
    // Two streaming meters read the signal entering the trim (after the compressor and EQ) and the signal leaving the
    // limiter. The controller runs once per 100 ms hop:
    //  - It starts from feed-forward: the first time enough signal has been heard, the gain is set to
    //    (target - input loudness), so the level is close to right within half a second.
    //  - After that it integrates the error (target - output loudness) with a time constant of 8 s and a rate limit of
    //    3 dB/s, so it follows the section-to-section level of a track, not the level of each beat. Measuring the
    //    output, after the limiter, means the loudness the limiter takes away is made up for automatically, up to the
    //    gain limits.
    //  - Nothing moves it while the latest hop is quieter than -50 LUFS, so silence and noise floor are never boosted
    //    and a gap in the music leaves the gain exactly where it was.
    //  - A large miss (over 10 LU, for example a new track or a big change of target) re-seeds from feed-forward. After
    //    a seed the integrator waits for the 3 s window to turn over, because until then the output reading still
    //    describes the old gain and integrating on it would overshoot.
    // The gain is limited to +-24 dB and the applied gain slews at no more than 12 dB/s, so it never steps, even on a
    // re-seed.
    //
    // Call apply() before the limiter and observeOutput() on the limiter's output, once per block each. Everything
    // but prepare() is real-time safe.
    class LoudnessTrim
    {
    public:
        static constexpr float kMaxGainDb = 24.0f;
        static constexpr float kGateLufs = -50.0f;
        static constexpr double kIntegrationSeconds = 8.0;
        static constexpr float kMaxRateDbPerSecond = 3.0f;
        static constexpr float kReseedThresholdLu = 10.0f;
        static constexpr float kMaxSlewDbPerSecond = 12.0f;

        void prepare (double sampleRate, int numChannels);
        void reset() noexcept;

        void setTargetLufs (float lufs) noexcept { targetLufs_ = lufs; }

        // Measures and applies the current gain to `buffer`.
        void apply (juce::AudioBuffer<float>& buffer) noexcept;

        // Measures the finished output and, once per hop, moves the gain.
        void observeOutput (const juce::AudioBuffer<float>& buffer) noexcept;

        // For the UI; updated every hop. Short-term (3 s) loudness of the output, or -70 if nothing above the gate yet.
        float getOutputLoudnessLufs() const noexcept { return outputLufs_.load (std::memory_order_relaxed); }
        // The gain being applied right now, in dB (after slewing).
        float getGainDb() const noexcept { return appliedGainDb_.load (std::memory_order_relaxed); }

    private:
        double sampleRate_ = 44100.0;
        float targetLufs_ = -14.0f;

        StreamingLoudnessMeter inputMeter_, outputMeter_;
        float controlGainDb_ = 0.0f; // where the controller wants the gain
        float currentGainDb_ = 0.0f;
        float slewPerSample_ = 0.0f;
        bool seeded_ = false;
        int blankingHops_ = 0; // control steps left before the integrator may act again

        std::atomic<float> outputLufs_ { -70.0f };
        std::atomic<float> appliedGainDb_ { 0.0f };

        void controlStep() noexcept;
    };
}
