#include "LoudnessMeter.h"
#include <cmath>

namespace afq
{
    void LoudnessMeter::prepare (double sampleRate, int numChannels)
    {
        sampleRate_ = sampleRate;
        numChannels_ = numChannels;
    }

    void LoudnessMeter::applyKWeighting (const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>& out) const
    {
        out.setSize (in.getNumChannels(), in.getNumSamples(), false, false, true);

        auto shelfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate_, 1500.0, 0.7071, juce::Decibels::decibelsToGain (4.0f));
        auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate_, 60.0, 0.5);

        for (int ch = 0; ch < in.getNumChannels(); ++ch)
        {
            juce::dsp::IIR::Filter<float> shelf, highpass;
            shelf.coefficients = shelfCoeffs;
            highpass.coefficients = hpCoeffs;

            out.copyFrom (ch, 0, in, ch, 0, in.getNumSamples());
            auto* data = out.getWritePointer (ch);
            juce::dsp::AudioBlock<float> block (&data, 1, (size_t) out.getNumSamples());
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            shelf.process (ctx);
            highpass.process (ctx);
        }
    }

    double LoudnessMeter::meanSquareToLkfs (double meanSquare)
    {
        return meanSquare > 1.0e-12 ? -0.691 + 10.0 * std::log10 (meanSquare) : -70.0;
    }

    float LoudnessMeter::measureShortTermLoudness (const juce::AudioBuffer<float>& block)
    {
        juce::AudioBuffer<float> filtered;
        applyKWeighting (block, filtered);

        double sumSq = 0.0;
        const int numCh = filtered.getNumChannels();
        const int numSamples = filtered.getNumSamples();
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* d = filtered.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i) sumSq += (double) d[i] * d[i];
        }
        const double meanSq = (numCh > 0 && numSamples > 0) ? sumSq / (numCh * numSamples) : 0.0;
        return (float) meanSquareToLkfs (meanSq);
    }

    float LoudnessMeter::measureIntegratedLoudness (const juce::AudioBuffer<float>& buffer)
    {
        juce::AudioBuffer<float> filtered;
        applyKWeighting (buffer, filtered);

        const int numCh = filtered.getNumChannels();
        const int numSamples = filtered.getNumSamples();
        const int blockSize = (int) (sampleRate_ * 0.4);
        const int hopSize = (int) (sampleRate_ * 0.1);
        if (blockSize <= 0 || numSamples < blockSize) return -70.0f;

        std::vector<double> blockMeanSq;
        blockMeanSq.reserve ((size_t) (numSamples / hopSize));

        for (int pos = 0; pos + blockSize <= numSamples; pos += hopSize)
        {
            double sumSq = 0.0;
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float* d = filtered.getReadPointer (ch) + pos;
                for (int i = 0; i < blockSize; ++i) sumSq += (double) d[i] * d[i];
            }
            blockMeanSq.push_back (numCh > 0 ? sumSq / (numCh * blockSize) : 0.0);
        }
        if (blockMeanSq.empty()) return -70.0f;

        // Absolute gate: discard blocks below -70 LUFS.
        std::vector<double> absGated;
        for (auto m : blockMeanSq)
            if (meanSquareToLkfs (m) > -70.0) absGated.push_back (m);
        if (absGated.empty()) return -70.0f;

        double sumAbs = 0.0;
        for (auto m : absGated) sumAbs += m;
        const double ungatedMeanPower = sumAbs / (double) absGated.size();
        const double relativeThreshold = meanSquareToLkfs (ungatedMeanPower) - 10.0;

        // Relative gate: discard blocks more than 10dB below the ungated mean.
        std::vector<double> relGated;
        for (auto m : absGated)
            if (meanSquareToLkfs (m) > relativeThreshold) relGated.push_back (m);
        if (relGated.empty()) relGated = absGated;

        double sumRel = 0.0;
        for (auto m : relGated) sumRel += m;
        const double finalMeanPower = sumRel / (double) relGated.size();

        return (float) meanSquareToLkfs (finalMeanPower);
    }
}
