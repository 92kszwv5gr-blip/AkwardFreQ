#include "LoudnessMeter.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace afq
{
    namespace
    {
        // Runs one channel through both K-weighting sections.
        class KWeightingFilter
        {
        public:
            explicit KWeightingFilter (const std::array<LoudnessMeter::Biquad, 2>& coefficients) : c_ (coefficients) {}
            double process (double x) noexcept { return LoudnessMeter::kWeight (c_, state_, x); }

        private:
            const std::array<LoudnessMeter::Biquad, 2>& c_;
            LoudnessMeter::KWeightingState state_;
        };

        constexpr double kAbsoluteGateLufs = -70.0;
        constexpr double kRelativeGateLu = -10.0;
        constexpr double kHopSeconds = 0.1; // 75% overlap
    }

    std::array<LoudnessMeter::Biquad, 2> LoudnessMeter::kWeightingCoefficients (double sampleRate)
    {
        constexpr double pi = 3.14159265358979323846;

        // Stage 1: the head-related high shelf.
        constexpr double shelfHz = 1681.974450955533, shelfGainDb = 3.999843853973347, shelfQ = 0.7071752369554196;
        const double k1 = std::tan (pi * shelfHz / sampleRate);
        const double vh = std::pow (10.0, shelfGainDb / 20.0);
        const double vb = std::pow (vh, 0.4996667741545416);
        const double a0 = 1.0 + k1 / shelfQ + k1 * k1;
        Biquad shelf;
        shelf.b0 = (vh + vb * k1 / shelfQ + k1 * k1) / a0;
        shelf.b1 = 2.0 * (k1 * k1 - vh) / a0;
        shelf.b2 = (vh - vb * k1 / shelfQ + k1 * k1) / a0;
        shelf.a1 = 2.0 * (k1 * k1 - 1.0) / a0;
        shelf.a2 = (1.0 - k1 / shelfQ + k1 * k1) / a0;

        // Stage 2: the RLB high-pass. The numerator is [1, -2, 1] and is not divided by a0, as in the standard.
        constexpr double rlbHz = 38.13547087602444, rlbQ = 0.5003270373238773;
        const double k2 = std::tan (pi * rlbHz / sampleRate);
        const double a0b = 1.0 + k2 / rlbQ + k2 * k2;
        Biquad rlb;
        rlb.b0 = 1.0;
        rlb.b1 = -2.0;
        rlb.b2 = 1.0;
        rlb.a1 = 2.0 * (k2 * k2 - 1.0) / a0b;
        rlb.a2 = (1.0 - k2 / rlbQ + k2 * k2) / a0b;

        return { shelf, rlb };
    }

    double LoudnessMeter::kWeight (const std::array<Biquad, 2>& coefficients, KWeightingState& state, double x) noexcept
    {
        for (size_t stage = 0; stage < 2; ++stage)
        {
            const auto& c = coefficients[stage];
            const double y = c.b0 * x + state.z1[stage];
            state.z1[stage] = c.b1 * x - c.a1 * y + state.z2[stage];
            state.z2[stage] = c.b2 * x - c.a2 * y;
            x = y;
        }
        return x;
    }

    void LoudnessMeter::prepare (double sampleRate, int numChannels)
    {
        juce::ignoreUnused (numChannels); // every channel is weighted 1.0, so the channel count needs no state
        sampleRate_ = sampleRate;
        kWeighting_ = kWeightingCoefficients (sampleRate);
    }

    double LoudnessMeter::powerToLufs (double power)
    {
        return power > 1.0e-12 ? -0.691 + 10.0 * std::log10 (power) : kAbsoluteGateLufs;
    }

    float LoudnessMeter::measureShortTermLoudness (const juce::AudioBuffer<float>& block) const
    {
        const int numSamples = block.getNumSamples();
        if (numSamples == 0) return (float) kAbsoluteGateLufs;

        // BS.1770 sums the channels' mean-square powers.
        double power = 0.0;
        for (int ch = 0; ch < block.getNumChannels(); ++ch)
        {
            KWeightingFilter filter (kWeighting_);
            const float* data = block.getReadPointer (ch);
            double sumSquares = 0.0;
            for (int i = 0; i < numSamples; ++i)
            {
                const double y = filter.process ((double) data[i]);
                sumSquares += y * y;
            }
            power += sumSquares / numSamples;
        }
        return (float) powerToLufs (power);
    }

    float LoudnessMeter::measureIntegratedLoudness (const juce::AudioBuffer<float>& buffer) const
    {
        // A block is four consecutive hops (400 ms with 75% overlap), so summing the K-weighted squares once per hop
        // gives every block's power without keeping the whole filtered signal in memory.
        const int numSamples = buffer.getNumSamples();
        const int hopSize = (int) (sampleRate_ * kHopSeconds);
        constexpr int kHopsPerBlock = 4;
        const int blockSize = hopSize * kHopsPerBlock;
        if (hopSize <= 0 || numSamples < blockSize) return (float) kAbsoluteGateLufs;

        const int numHops = numSamples / hopSize;
        const int numBlocks = numHops - kHopsPerBlock + 1;
        std::vector<double> hopSquares ((size_t) numHops);
        std::vector<double> blockPower ((size_t) numBlocks, 0.0);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            KWeightingFilter filter (kWeighting_);
            const float* data = buffer.getReadPointer (ch);
            for (int h = 0; h < numHops; ++h)
            {
                double sum = 0.0;
                for (int i = h * hopSize; i < (h + 1) * hopSize; ++i)
                {
                    const double y = filter.process ((double) data[i]);
                    sum += y * y;
                }
                hopSquares[(size_t) h] = sum;
            }
            for (int b = 0; b < numBlocks; ++b)
            {
                double sum = 0.0;
                for (int k = 0; k < kHopsPerBlock; ++k) sum += hopSquares[(size_t) (b + k)];
                blockPower[(size_t) b] += sum / blockSize;
            }
        }

        auto gatedMean = [&blockPower] (double thresholdLufs, double& mean)
        {
            double sum = 0.0;
            size_t count = 0;
            for (double p : blockPower)
                if (powerToLufs (p) > thresholdLufs) { sum += p; ++count; }
            if (count > 0) mean = sum / (double) count;
            return count > 0;
        };

        double absoluteGatedMean = 0.0;
        if (! gatedMean (kAbsoluteGateLufs, absoluteGatedMean)) return (float) kAbsoluteGateLufs;

        double relativeGatedMean = absoluteGatedMean;
        gatedMean (powerToLufs (absoluteGatedMean) + kRelativeGateLu, relativeGatedMean);
        return (float) powerToLufs (relativeGatedMean);
    }

    //==============================================================================
    void StreamingLoudnessMeter::prepare (double sampleRate, int numChannels, double windowSeconds)
    {
        coefficients_ = LoudnessMeter::kWeightingCoefficients (sampleRate);
        filters_.assign ((size_t) juce::jmax (1, numChannels), LoudnessMeter::KWeightingState {});
        hopSamples_ = juce::jmax (1, (int) std::lround (sampleRate * kHopSeconds));
        hopPower_.assign ((size_t) juce::jmax (1, (int) std::lround (windowSeconds / kHopSeconds)), 0.0);
        reset();
    }

    void StreamingLoudnessMeter::reset() noexcept
    {
        for (auto& f : filters_) f = {};
        std::fill (hopPower_.begin(), hopPower_.end(), 0.0);
        hopFill_ = 0;
        hopSum_ = 0.0;
        ringPos_ = 0;
        ringCount_ = 0;
    }

    bool StreamingLoudnessMeter::process (const juce::AudioBuffer<float>& block) noexcept
    {
        const int numChannels = juce::jmin (block.getNumChannels(), (int) filters_.size());
        const int numSamples = block.getNumSamples();
        bool completedHop = false;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const double y = LoudnessMeter::kWeight (coefficients_, filters_[(size_t) ch], (double) block.getSample (ch, i));
                hopSum_ += y * y;
            }
            if (++hopFill_ == hopSamples_)
            {
                hopPower_[(size_t) ringPos_] = hopSum_ / hopSamples_;
                ringPos_ = ringPos_ + 1 == (int) hopPower_.size() ? 0 : ringPos_ + 1;
                ringCount_ = juce::jmin (ringCount_ + 1, (int) hopPower_.size());
                hopFill_ = 0;
                hopSum_ = 0.0;
                completedHop = true;
            }
        }
        return completedHop;
    }

    StreamingLoudnessMeter::Reading StreamingLoudnessMeter::read (float gateLufs) const noexcept
    {
        // Only hops already completed count. The ring holds the last ringCount_ of them.
        double sum = 0.0;
        int counted = 0;
        for (int k = 0; k < ringCount_; ++k)
        {
            const double p = hopPower_[(size_t) k];
            if (LoudnessMeter::powerToLufs (p) > (double) gateLufs) { sum += p; ++counted; }
        }
        Reading r;
        r.hops = counted;
        if (ringCount_ > 0)
            r.latestHopLufs = (float) LoudnessMeter::powerToLufs (hopPower_[(size_t) (ringPos_ == 0 ? (int) hopPower_.size() - 1 : ringPos_ - 1)]);
        if (counted > 0) r.lufs = (float) LoudnessMeter::powerToLufs (sum / counted);
        return r;
    }
}
