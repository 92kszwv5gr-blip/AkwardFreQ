#include "LoudnessTrim.h"
#include <cmath>

namespace afq
{
    namespace
    {
        constexpr int kMinHopsToTrust = 4; // 400 ms of signal above the gate
        constexpr double kWindowSeconds = 3.0; // the meters' averaging window
    }

    void LoudnessTrim::prepare (double sampleRate, int numChannels)
    {
        sampleRate_ = sampleRate;
        inputMeter_.prepare (sampleRate, numChannels);
        outputMeter_.prepare (sampleRate, numChannels);
        slewPerSample_ = (float) (kMaxSlewDbPerSecond / sampleRate);
        reset();
    }

    void LoudnessTrim::reset() noexcept
    {
        inputMeter_.reset();
        outputMeter_.reset();
        controlGainDb_ = 0.0f;
        currentGainDb_ = 0.0f;
        seeded_ = false;
        blankingHops_ = 0;
        outputLufs_.store (-70.0f, std::memory_order_relaxed);
        appliedGainDb_.store (0.0f, std::memory_order_relaxed);
    }

    void LoudnessTrim::apply (juce::AudioBuffer<float>& buffer) noexcept
    {
        inputMeter_.process (buffer);

        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        float* const* data = buffer.getArrayOfWritePointers();
        for (int i = 0; i < numSamples; ++i)
        {
            // Slew the applied gain toward the controller's, in dB, so it can never step.
            currentGainDb_ += juce::jlimit (-slewPerSample_, slewPerSample_, controlGainDb_ - currentGainDb_);
            const float g = std::exp (currentGainDb_ * 0.11512925464970229f); // ln(10) / 20
            for (int ch = 0; ch < numChannels; ++ch) data[ch][i] *= g;
        }
        appliedGainDb_.store (currentGainDb_, std::memory_order_relaxed);
    }

    void LoudnessTrim::observeOutput (const juce::AudioBuffer<float>& buffer) noexcept
    {
        if (outputMeter_.process (buffer))
            controlStep();
    }

    void LoudnessTrim::controlStep() noexcept
    {
        const auto out = outputMeter_.read (kGateLufs);
        outputLufs_.store (out.lufs, std::memory_order_relaxed);
        if (out.hops < kMinHopsToTrust || out.latestHopLufs <= kGateLufs) return; // not enough music yet, or a pause: hold the gain

        const auto in = inputMeter_.read (kGateLufs);
        const float error = targetLufs_ - out.lufs;

        if (! seeded_ || std::abs (error) > kReseedThresholdLu)
        {
            if (in.hops >= kMinHopsToTrust)
            {
                controlGainDb_ = juce::jlimit (-kMaxGainDb, kMaxGainDb, targetLufs_ - in.lufs);
                seeded_ = true;
                blankingHops_ = (int) std::lround (kWindowSeconds / StreamingLoudnessMeter::kHopSeconds);
            }
            return;
        }

        if (blankingHops_ > 0) { --blankingHops_; return; }

        // Integral action: the error, spread over the time constant, limited to the maximum rate.
        const float hop = (float) StreamingLoudnessMeter::kHopSeconds;
        const float step = juce::jlimit (-kMaxRateDbPerSecond * hop, kMaxRateDbPerSecond * hop, error * hop / (float) kIntegrationSeconds);
        controlGainDb_ = juce::jlimit (-kMaxGainDb, kMaxGainDb, controlGainDb_ + step);
    }
}
