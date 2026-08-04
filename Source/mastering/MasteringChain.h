#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "LoudnessMeter.h"

namespace afq
{
    // Real DSP mastering chain: 4-band compressor (Linkwitz-Riley crossover split),
    // optional reference-track EQ matching, automatic loudness trim toward a
    // target LUFS-ish level, and a final brickwall limiter.
    //
    // EQ matching needs an offline analysis pass (there's no single-instant
    // "target spectrum" to chase in real time) — call setCurrentTrackAnalysis()
    // and setReferenceTrack() from the same background thread that runs stem
    // separation, using the same captured/imported audio. processBlock() itself
    // is real-time safe: no allocation, no analysis, just applying the
    // already-computed correction curve.
    class MasteringChain
    {
    public:
        void prepare (double sampleRate, int numChannels, int maxBlockSize);
        void reset();

        struct Settings
        {
            float targetLoudnessLufs = -8.0f;
            float compAmount = 0.35f;     // 0-1
            float limiterCeilingDb = -0.3f;
            float eqMatchAmount = 0.0f;   // 0-1, scales the correction curve; 0 = no EQ matching
            bool bypass = true;
        };
        void setSettings (const Settings& s) noexcept { settings_ = s; }

        // Both must be called off the audio thread (they run FFT analysis over
        // the whole buffer). `hasReference()` stays false, and the EQ stage is a
        // no-op, until both a current-track analysis and a reference have been set.
        void setCurrentTrackAnalysis (const juce::AudioBuffer<float>& currentTrack, double sampleRate);
        void setReferenceTrack (const juce::AudioBuffer<float>& referenceTrack, double sampleRate);
        bool hasReference() const noexcept { return hasReference_ && hasCurrentAnalysis_; }

        // Real-time safe.
        void processBlock (juce::AudioBuffer<float>& buffer);

        // Safe to poll from the UI thread via a Timer — updated roughly every
        // 100ms from the audio thread.
        float getLastMeasuredLoudnessLufs() const noexcept { return lastMeasuredLoudness_.load (std::memory_order_relaxed); }

    private:
        static constexpr int kNumBands = 4;
        static constexpr int kNumEqBands = 12;

        Settings settings_;
        double sampleRate_ = 44100.0;
        int numChannels_ = 2;

        // Crossover network: 3 split points -> 4 bands, cascaded LR filters.
        std::array<juce::dsp::LinkwitzRileyFilter<float>, 3> lowpassSplits_, highpassSplits_;
        std::array<juce::dsp::Compressor<float>, kNumBands> bandCompressors_;
        std::array<juce::AudioBuffer<float>, kNumBands> bandBuffers_;
        juce::AudioBuffer<float> stageScratchA_, stageScratchB_;

        // EQ match: bank of peak filters at log-spaced band centers.
        std::array<float, kNumEqBands> eqBandCenterHz_ {};
        std::array<float, kNumEqBands> eqMatchGainDb_ {};
        std::array<juce::dsp::IIR::Filter<float>, kNumEqBands> eqFiltersLeft_, eqFiltersRight_;
        bool hasReference_ = false, hasCurrentAnalysis_ = false;
        std::array<float, kNumEqBands> referenceBandEnergyDb_ {};
        std::array<float, kNumEqBands> currentBandEnergyDb_ {};

        // Loudness trim: periodically measure the (short, noisy) block loudness
        // and retarget a heavily-smoothed gain toward targetLoudnessLufs. The
        // 300ms smoothing ramp is what turns per-block noise into a slow, stable
        // makeup-gain trim rather than a real BS.1770 integrated measurement.
        LoudnessMeter loudnessMeter_;
        int samplesSinceLastMeasurement_ = 0;
        juce::LinearSmoothedValue<float> smoothedGainLinear_;
        std::atomic<float> lastMeasuredLoudness_ { -23.0f };

        // Limiter.
        juce::dsp::Limiter<float> limiter_;

        std::array<float, kNumEqBands> analyzeBandEnergyDb (const juce::AudioBuffer<float>& buffer, double sampleRate) const;
        void updateEqFilters();
        void updateBandCompressors();
        void updateCrossoverFrequencies();
    };
}
