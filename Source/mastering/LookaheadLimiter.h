#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdint>
#include <vector>

namespace afq
{
    // A brickwall peak limiter with lookahead: no sample leaves it above the ceiling, and the gain reduction ramps
    // in over the lookahead so the peaks are turned down smoothly instead of being clipped.
    //
    // For each sample the gain needed to bring the peak (the loudest channel, so the stereo image is preserved)
    // under the ceiling is computed. The minimum of that over the lookahead window is taken, so the reduction is
    // already in force when the peak arrives; it is released exponentially; and a moving average over the same
    // window turns the steps into a linear ramp. Because the average covers only samples whose windowed minimum is
    // at or below the gain the delayed sample needs, the output cannot exceed the ceiling.
    //
    // This limits sample peaks. Inter-sample (true) peaks can exceed the ceiling by a fraction of a dB after
    // D/A conversion, so leave some headroom below 0 dBFS if that matters.
    //
    // The audio is delayed by getLatencySamples(); report it to the host. Everything except prepare() is real-time
    // safe: no allocation, no locks.
    class LookaheadLimiter
    {
    public:
        static constexpr double kLookaheadSeconds = 0.003;
        static constexpr float kDefaultReleaseMs = 100.0f;

        // Allocates. `numChannels` is the most channels process() will be given.
        void prepare (double sampleRate, int numChannels);

        // Clears the delayed audio and the gain state.
        void reset() noexcept;

        // Takes effect immediately. Lowering the ceiling mid-stream can leave up to getLatencySamples() already
        // computed gains too low a ceiling for; the output stage clamps those, so the ceiling still holds.
        void setCeilingDb (float ceilingDb) noexcept;
        void setReleaseMs (float releaseMs) noexcept;

        int getLatencySamples() const noexcept { return lookahead_; }

        // How many output samples the final safety clamp had to cut since reset(). The lookahead is meant to make
        // the clamp unnecessary, so this stays 0 while the ceiling is steady; a non-zero count means the gain was
        // late. Read it from the thread that calls process().
        uint64_t getClampedSampleCount() const noexcept { return clampedSamples_; }

        // Limits `buffer` in place. The output is the input delayed by getLatencySamples().
        void process (juce::AudioBuffer<float>& buffer) noexcept;

        // Passes `buffer` through unchanged apart from the same delay, and leaves the gain state at unity, so
        // switching between process() and delay() keeps the timing steady.
        void delay (juce::AudioBuffer<float>& buffer) noexcept;

    private:
        double sampleRate_ = 44100.0;
        int numChannels_ = 0;
        int lookahead_ = 1; // samples; also the latency
        float ceilingLinear_ = 1.0f;
        double releaseDecay_ = 0.0; // per-sample factor of the exponential release

        // Delayed audio, one ring of `lookahead_` samples per channel.
        juce::AudioBuffer<float> delayLine_;
        int delayPos_ = 0;

        // Monotonic wedge holding the minimum required gain over the last lookahead_ + 1 samples (capacity
        // lookahead_ + 2, since a new entry is pushed before the oldest is dropped).
        std::vector<double> wedgeGain_;
        std::vector<int64_t> wedgeIndex_;
        int wedgeHead_ = 0, wedgeCount_ = 0;
        int64_t sampleCounter_ = 0;

        double envelope_ = 1.0; // windowed minimum with the release applied

        // Moving average of envelope_ over lookahead_ + 1 samples.
        std::vector<double> averageRing_;
        int averagePos_ = 0;
        double averageSum_ = 1.0;

        bool gainIsUnity_ = true;
        uint64_t clampedSamples_ = 0;

        void resetGainState() noexcept;
    };
}
