#include "MasteringChain.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

namespace afq
{
    namespace
    {
        constexpr float kCrossoverFreqs[3] = { 150.0f, 1500.0f, 6000.0f };
        constexpr double kEqBandLowHz = 40.0;
        constexpr double kEqBandHighHz = 16000.0;
        constexpr int kMeasurementIntervalSamplesAt44k = 4410; // ~100ms
    }

    void MasteringChain::prepare (double sampleRate, int numChannels, int maxBlockSize)
    {
        sampleRate_ = sampleRate;
        numChannels_ = numChannels;

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) numChannels };

        using LRFilter = juce::dsp::LinkwitzRileyFilter<float>;
        for (auto& f : lowpassSplits_)  { f.prepare (spec); f.setType (LRFilter::Type::lowpass); }
        for (auto& f : highpassSplits_) { f.prepare (spec); f.setType (LRFilter::Type::highpass); }
        updateCrossoverFrequencies();

        for (auto& c : bandCompressors_) c.prepare (spec);
        updateBandCompressors();

        for (auto& b : bandBuffers_) b.setSize (numChannels, maxBlockSize);
        stageScratchA_.setSize (numChannels, maxBlockSize);
        stageScratchB_.setSize (numChannels, maxBlockSize);

        for (int i = 0; i < kNumEqBands; ++i)
            eqBandCenterHz_[(size_t) i] = (float) (kEqBandLowHz * std::pow (kEqBandHighHz / kEqBandLowHz,
                                                                              (double) i / (double) (kNumEqBands - 1)));

        juce::dsp::ProcessSpec monoSpec { sampleRate, (juce::uint32) maxBlockSize, 1 };
        for (auto& f : eqFiltersLeft_)  f.prepare (monoSpec);
        for (auto& f : eqFiltersRight_) f.prepare (monoSpec);
        eqMatchGainDb_.fill (0.0f);
        updateEqFilters();

        loudnessMeter_.prepare (sampleRate, numChannels);
        samplesSinceLastMeasurement_ = 0;
        smoothedGainLinear_.reset (sampleRate, 0.3);
        smoothedGainLinear_.setCurrentAndTargetValue (1.0f);

        limiter_.prepare (spec);

        reset();
    }

    void MasteringChain::reset()
    {
        for (auto& f : lowpassSplits_) f.reset();
        for (auto& f : highpassSplits_) f.reset();
        for (auto& c : bandCompressors_) c.reset();
        for (auto& f : eqFiltersLeft_) f.reset();
        for (auto& f : eqFiltersRight_) f.reset();
        limiter_.reset();
    }

    void MasteringChain::updateCrossoverFrequencies()
    {
        for (int i = 0; i < 3; ++i)
        {
            lowpassSplits_[(size_t) i].setCutoffFrequency (kCrossoverFreqs[i]);
            highpassSplits_[(size_t) i].setCutoffFrequency (kCrossoverFreqs[i]);
        }
    }

    void MasteringChain::updateBandCompressors()
    {
        const float thresholdDb = juce::jmap (settings_.compAmount, 0.0f, 1.0f, -2.0f, -24.0f);
        const float ratio = juce::jmap (settings_.compAmount, 0.0f, 1.0f, 1.2f, 6.0f);
        for (auto& c : bandCompressors_)
        {
            c.setThreshold (thresholdDb);
            c.setRatio (ratio);
            c.setAttack (10.0f);
            c.setRelease (120.0f);
        }
    }

    void MasteringChain::updateEqFilters()
    {
        for (int i = 0; i < kNumEqBands; ++i)
        {
            const float gainDb = eqMatchGainDb_[(size_t) i] * settings_.eqMatchAmount;
            const float gainLinear = juce::Decibels::decibelsToGain (juce::jlimit (-12.0f, 12.0f, gainDb));
            // Q chosen so adjacent log-spaced bands overlap smoothly rather than combing.
            auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                sampleRate_, (double) eqBandCenterHz_[(size_t) i], 1.4, gainLinear);
            eqFiltersLeft_[(size_t) i].coefficients = coeffs;
            eqFiltersRight_[(size_t) i].coefficients = coeffs;
        }
    }

    std::array<float, MasteringChain::kNumEqBands> MasteringChain::analyzeBandEnergyDb (
        const juce::AudioBuffer<float>& buffer, double sampleRate) const
    {
        std::array<float, kNumEqBands> result {};
        result.fill (-60.0f);
        if (buffer.getNumSamples() == 0) return result;

        constexpr int fftOrder = 12; // 4096
        constexpr int fftSize = 1 << fftOrder;
        juce::dsp::FFT fft (fftOrder);
        juce::dsp::WindowingFunction<float> window (fftSize, juce::dsp::WindowingFunction<float>::hann);

        std::array<double, kNumEqBands> bandEnergy {};
        std::array<double, kNumEqBands> bandBinCount {};
        bandEnergy.fill (0.0);
        bandBinCount.fill (0.0);

        std::vector<float> fftData ((size_t) (2 * fftSize), 0.0f);
        std::vector<float> monoFrame ((size_t) fftSize, 0.0f);

        const int numSamples = buffer.getNumSamples();
        const int hop = fftSize;
        const int maxFrames = 300; // cap analysis cost
        const int numChannels = buffer.getNumChannels();
        int frameCount = 0;

        for (int pos = 0; pos + fftSize <= numSamples && frameCount < maxFrames; pos += hop, ++frameCount)
        {
            std::fill (monoFrame.begin(), monoFrame.end(), 0.0f);
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* d = buffer.getReadPointer (ch) + pos;
                for (int i = 0; i < fftSize; ++i) monoFrame[(size_t) i] += d[i] / (float) numChannels;
            }

            std::fill (fftData.begin(), fftData.end(), 0.0f);
            std::copy (monoFrame.begin(), monoFrame.end(), fftData.begin());
            window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
            fft.performRealOnlyForwardTransform (fftData.data());

            const int numBins = fftSize / 2 + 1;
            for (int bin = 1; bin < numBins; ++bin)
            {
                const double freq = bin * sampleRate / fftSize;
                if (freq < kEqBandLowHz * 0.5 || freq > kEqBandHighHz * 1.5) continue;

                const float re = fftData[(size_t) (2 * bin)];
                const float im = fftData[(size_t) (2 * bin + 1)];
                const double mag2 = (double) re * re + (double) im * im;

                // Assign to nearest log-spaced band center.
                double bestDist = 1.0e30;
                int bestBand = 0;
                for (int b = 0; b < kNumEqBands; ++b)
                {
                    const double dist = std::abs (std::log (freq) - std::log ((double) eqBandCenterHz_[(size_t) b]));
                    if (dist < bestDist) { bestDist = dist; bestBand = b; }
                }
                bandEnergy[(size_t) bestBand] += mag2;
                bandBinCount[(size_t) bestBand] += 1.0;
            }
        }

        for (int b = 0; b < kNumEqBands; ++b)
        {
            if (bandBinCount[(size_t) b] > 0.0)
            {
                const double meanEnergy = bandEnergy[(size_t) b] / bandBinCount[(size_t) b];
                result[(size_t) b] = (float) (10.0 * std::log10 (meanEnergy + 1.0e-12));
            }
        }
        return result;
    }

    void MasteringChain::setCurrentTrackAnalysis (const juce::AudioBuffer<float>& currentTrack, double sr)
    {
        currentBandEnergyDb_ = analyzeBandEnergyDb (currentTrack, sr);
        hasCurrentAnalysis_ = true;
        if (hasReference_)
            for (int i = 0; i < kNumEqBands; ++i)
                eqMatchGainDb_[(size_t) i] = juce::jlimit (-12.0f, 12.0f,
                    referenceBandEnergyDb_[(size_t) i] - currentBandEnergyDb_[(size_t) i]);
    }

    void MasteringChain::setReferenceTrack (const juce::AudioBuffer<float>& referenceTrack, double sr)
    {
        referenceBandEnergyDb_ = analyzeBandEnergyDb (referenceTrack, sr);
        hasReference_ = true;
        if (hasCurrentAnalysis_)
            for (int i = 0; i < kNumEqBands; ++i)
                eqMatchGainDb_[(size_t) i] = juce::jlimit (-12.0f, 12.0f,
                    referenceBandEnergyDb_[(size_t) i] - currentBandEnergyDb_[(size_t) i]);
    }

    void MasteringChain::processBlock (juce::AudioBuffer<float>& buffer)
    {
        if (settings_.bypass) return;

        updateBandCompressors();
        updateEqFilters();

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        //---- Multiband split ----
        for (int ch = 0; ch < numChannels; ++ch)
        {
            bandBuffers_[0].copyFrom (ch, 0, buffer, ch, 0, numSamples);
            stageScratchA_.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        }
        {
            juce::dsp::AudioBlock<float> blk (bandBuffers_[0]);
            juce::dsp::ProcessContextReplacing<float> ctx (blk.getSubBlock (0, (size_t) numSamples));
            lowpassSplits_[0].process (ctx);

            juce::dsp::AudioBlock<float> blkHp (stageScratchA_);
            juce::dsp::ProcessContextReplacing<float> ctxHp (blkHp.getSubBlock (0, (size_t) numSamples));
            highpassSplits_[0].process (ctxHp);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            bandBuffers_[1].copyFrom (ch, 0, stageScratchA_, ch, 0, numSamples);
            stageScratchB_.copyFrom (ch, 0, stageScratchA_, ch, 0, numSamples);
        }
        {
            juce::dsp::AudioBlock<float> blk (bandBuffers_[1]);
            juce::dsp::ProcessContextReplacing<float> ctx (blk.getSubBlock (0, (size_t) numSamples));
            lowpassSplits_[1].process (ctx);

            juce::dsp::AudioBlock<float> blkHp (stageScratchB_);
            juce::dsp::ProcessContextReplacing<float> ctxHp (blkHp.getSubBlock (0, (size_t) numSamples));
            highpassSplits_[1].process (ctxHp);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            bandBuffers_[2].copyFrom (ch, 0, stageScratchB_, ch, 0, numSamples);
            bandBuffers_[3].copyFrom (ch, 0, stageScratchB_, ch, 0, numSamples);
        }
        {
            juce::dsp::AudioBlock<float> blk2 (bandBuffers_[2]);
            juce::dsp::ProcessContextReplacing<float> ctx2 (blk2.getSubBlock (0, (size_t) numSamples));
            lowpassSplits_[2].process (ctx2);

            juce::dsp::AudioBlock<float> blk3 (bandBuffers_[3]);
            juce::dsp::ProcessContextReplacing<float> ctx3 (blk3.getSubBlock (0, (size_t) numSamples));
            highpassSplits_[2].process (ctx3);
        }

        //---- Per-band compression ----
        for (int b = 0; b < kNumBands; ++b)
        {
            juce::dsp::AudioBlock<float> blk (bandBuffers_[(size_t) b]);
            juce::dsp::ProcessContextReplacing<float> ctx (blk.getSubBlock (0, (size_t) numSamples));
            bandCompressors_[(size_t) b].process (ctx);
        }

        //---- Sum bands back ----
        buffer.clear();
        for (int b = 0; b < kNumBands; ++b)
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.addFrom (ch, 0, bandBuffers_[(size_t) b], ch, 0, numSamples);

        //---- EQ match (stereo only) ----
        if (hasReference() && settings_.eqMatchAmount > 0.0f)
        {
            for (int ch = 0; ch < juce::jmin (numChannels, 2); ++ch)
            {
                auto& filters = (ch == 0) ? eqFiltersLeft_ : eqFiltersRight_;
                float* data = buffer.getWritePointer (ch);
                juce::dsp::AudioBlock<float> block (&data, 1, (size_t) numSamples);
                juce::dsp::ProcessContextReplacing<float> ctx (block);
                for (auto& f : filters) f.process (ctx);
            }
        }

        //---- Loudness trim ----
        samplesSinceLastMeasurement_ += numSamples;
        const int measurementInterval = (int) (kMeasurementIntervalSamplesAt44k * (sampleRate_ / 44100.0));
        if (samplesSinceLastMeasurement_ >= measurementInterval)
        {
            samplesSinceLastMeasurement_ = 0;
            const float measured = loudnessMeter_.measureShortTermLoudness (buffer);
            lastMeasuredLoudness_.store (measured, std::memory_order_relaxed);
            const float diffDb = juce::jlimit (-24.0f, 24.0f, settings_.targetLoudnessLufs - measured);
            smoothedGainLinear_.setTargetValue (juce::Decibels::decibelsToGain (diffDb));
        }
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = smoothedGainLinear_.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * g);
        }

        //---- Limiter ----
        limiter_.setThreshold (settings_.limiterCeilingDb);
        {
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block.getSubBlock (0, (size_t) numSamples));
            limiter_.process (ctx);
        }
    }
}
