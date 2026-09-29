#include "LookaheadLimiter.h"
#include <algorithm>
#include <cmath>

namespace afq
{
    namespace
    {
        // Non-finite input would poison the gain state, so it is replaced by silence.
        inline float sanitised (float x) noexcept { return std::isfinite (x) ? x : 0.0f; }

        // The gain is aimed a hair under the ceiling (about 0.000009 dB) so that rounding in the float multiply
        // cannot land a sample one ulp above it.
        constexpr double kRoundingMargin = 1.0 - 1.0e-6;
    }

    void LookaheadLimiter::prepare (double sampleRate, int numChannels)
    {
        jassert (sampleRate > 0.0 && numChannels > 0);
        sampleRate_ = sampleRate;
        numChannels_ = juce::jmax (1, numChannels);
        lookahead_ = juce::jmax (1, (int) std::lround (kLookaheadSeconds * sampleRate));

        delayLine_.setSize (numChannels_, lookahead_);
        wedgeGain_.assign ((size_t) lookahead_ + 2, 1.0);  // one spare: a new entry is pushed before the oldest is dropped
        wedgeIndex_.assign ((size_t) lookahead_ + 2, 0);
        averageRing_.assign ((size_t) lookahead_ + 1, 1.0);

        setReleaseMs (kDefaultReleaseMs);
        setCeilingDb (0.0f);
        reset();
    }

    void LookaheadLimiter::reset() noexcept
    {
        delayLine_.clear();
        delayPos_ = 0;
        clampedSamples_ = 0;
        resetGainState();
    }

    void LookaheadLimiter::resetGainState() noexcept
    {
        wedgeHead_ = 0;
        wedgeCount_ = 0;
        sampleCounter_ = 0;
        envelope_ = 1.0;
        std::fill (averageRing_.begin(), averageRing_.end(), 1.0);
        averagePos_ = 0;
        averageSum_ = (double) averageRing_.size();
        gainIsUnity_ = true;
    }

    void LookaheadLimiter::setCeilingDb (float ceilingDb) noexcept
    {
        ceilingLinear_ = juce::Decibels::decibelsToGain (ceilingDb);
    }

    void LookaheadLimiter::setReleaseMs (float releaseMs) noexcept
    {
        const double samples = juce::jmax (1.0, (double) releaseMs * 0.001 * sampleRate_);
        releaseDecay_ = std::exp (-1.0 / samples);
    }

    void LookaheadLimiter::process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numChannels = juce::jmin (buffer.getNumChannels(), numChannels_);
        const int numSamples = buffer.getNumSamples();
        float* const* data = buffer.getArrayOfWritePointers();
        const int windowSize = lookahead_ + 1;
        const int wedgeCapacity = lookahead_ + 2;
        gainIsUnity_ = false;

        for (int i = 0; i < numSamples; ++i)
        {
            // Gain that would bring this sample's loudest channel to the ceiling.
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch) peak = juce::jmax (peak, std::abs (sanitised (data[ch][i])));
            const double required = peak > ceilingLinear_ ? (double) ceilingLinear_ * kRoundingMargin / (double) peak : 1.0;

            // Minimum over the window [n - lookahead, n]: pop weaker entries, push, drop the ones that fell out.
            while (wedgeCount_ > 0 && wedgeGain_[(size_t) ((wedgeHead_ + wedgeCount_ - 1) % wedgeCapacity)] >= required) --wedgeCount_;
            const int tail = (wedgeHead_ + wedgeCount_) % wedgeCapacity;
            wedgeGain_[(size_t) tail] = required;
            wedgeIndex_[(size_t) tail] = sampleCounter_;
            ++wedgeCount_;
            while (wedgeIndex_[(size_t) wedgeHead_] < sampleCounter_ - lookahead_)
            {
                wedgeHead_ = (wedgeHead_ + 1) % wedgeCapacity;
                --wedgeCount_;
            }
            const double windowedMinimum = wedgeGain_[(size_t) wedgeHead_];
            ++sampleCounter_;

            // Instant attack, exponential release, then the moving average that makes the attack a ramp.
            envelope_ = std::min (windowedMinimum, 1.0 - (1.0 - envelope_) * releaseDecay_);
            averageSum_ += envelope_ - averageRing_[(size_t) averagePos_];
            averageRing_[(size_t) averagePos_] = envelope_;
            averagePos_ = averagePos_ + 1 == windowSize ? 0 : averagePos_ + 1;
            const float gain = (float) (averageSum_ / (double) windowSize);

            // Swap the new sample into the delay line and apply the gain to the one that comes out. The clamp is a
            // guard against rounding, and against a ceiling that was lowered a moment ago; it does not otherwise act.
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* ring = delayLine_.getWritePointer (ch);
                const float delayed = ring[delayPos_];
                ring[delayPos_] = sanitised (data[ch][i]);
                const float output = delayed * gain;
                if (std::abs (output) > ceilingLinear_) ++clampedSamples_;
                data[ch][i] = juce::jlimit (-ceilingLinear_, ceilingLinear_, output);
            }
            delayPos_ = delayPos_ + 1 == lookahead_ ? 0 : delayPos_ + 1;
        }
    }

    void LookaheadLimiter::delay (juce::AudioBuffer<float>& buffer) noexcept
    {
        if (! gainIsUnity_) resetGainState();

        const int numChannels = juce::jmin (buffer.getNumChannels(), numChannels_);
        const int numSamples = buffer.getNumSamples();
        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* ring = delayLine_.getWritePointer (ch);
                const float delayed = ring[delayPos_];
                ring[delayPos_] = buffer.getSample (ch, i);
                buffer.setSample (ch, i, delayed);
            }
            delayPos_ = delayPos_ + 1 == lookahead_ ? 0 : delayPos_ + 1;
        }
    }
}
