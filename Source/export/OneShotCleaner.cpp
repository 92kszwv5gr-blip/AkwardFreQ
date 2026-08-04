#include "OneShotCleaner.h"
#include <algorithm>
#include <cmath>

namespace afq
{
    juce::AudioBuffer<float> OneShotCleaner::clean (const juce::AudioBuffer<float>& source,
                                                      int64_t startSample, int64_t endSample,
                                                      double sampleRate, const Settings& settings)
    {
        const int64_t total = source.getNumSamples();
        const int64_t start = juce::jlimit ((int64_t) 0, total, startSample);
        const int64_t end = juce::jlimit (start, total, endSample);
        const int len = (int) (end - start);
        const int numChannels = source.getNumChannels();

        if (len <= 0)
            return juce::AudioBuffer<float> (numChannels, 0);

        // Per-sample envelope (max abs across channels) over the raw slice,
        // used both to find the trim points and the peak for normalization.
        std::vector<float> envelope ((size_t) len, 0.0f);
        float slicePeak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* d = source.getReadPointer (ch) + start;
            for (int i = 0; i < len; ++i)
            {
                const float v = std::abs (d[i]);
                envelope[(size_t) i] = std::max (envelope[(size_t) i], v);
                slicePeak = std::max (slicePeak, v);
            }
        }

        int trimStart = 0;
        int trimEnd = len;

        if (slicePeak > 1.0e-6f)
        {
            const float threshold = slicePeak * juce::Decibels::decibelsToGain (settings.silenceThresholdDb);

            int firstAbove = -1;
            for (int i = 0; i < len; ++i) if (envelope[(size_t) i] > threshold) { firstAbove = i; break; }

            int lastAbove = -1;
            for (int i = len - 1; i >= 0; --i) if (envelope[(size_t) i] > threshold) { lastAbove = i; break; }

            if (firstAbove >= 0 && lastAbove >= firstAbove)
            {
                const int preRoll = (int) (settings.preRollMs * 0.001 * sampleRate);
                const int postRoll = (int) (settings.postRollMs * 0.001 * sampleRate);
                trimStart = juce::jmax (0, firstAbove - preRoll);
                trimEnd = juce::jmin (len, lastAbove + postRoll + 1);
            }
            // else: entirely below threshold (near-silent slice) — leave
            // trimStart/trimEnd as the full original range rather than
            // collapsing to nothing.
        }

        const int cleanLen = juce::jmax (1, trimEnd - trimStart);
        juce::AudioBuffer<float> result (numChannels, cleanLen);

        // Normalize using the (already-computed) slice peak, capped so
        // near-silent content doesn't get amplified into audible noise.
        float gain = 1.0f;
        if (slicePeak > 1.0e-6f)
        {
            const float targetPeak = juce::Decibels::decibelsToGain (settings.targetPeakDb);
            const float rawGainDb = juce::Decibels::gainToDecibels (targetPeak / slicePeak);
            gain = juce::Decibels::decibelsToGain (juce::jmin (rawGainDb, settings.maxGainDb));
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* src = source.getReadPointer (ch) + start + trimStart;
            float* dst = result.getWritePointer (ch);
            for (int i = 0; i < cleanLen; ++i) dst[i] = src[i] * gain;
        }

        const int fadeInSamples = juce::jmin (cleanLen / 2, (int) (settings.fadeInMs * 0.001 * sampleRate));
        const int fadeOutSamples = juce::jmin (cleanLen / 2, (int) (settings.fadeOutMs * 0.001 * sampleRate));

        if (fadeInSamples > 0)
            for (int ch = 0; ch < numChannels; ++ch)
                result.applyGainRamp (ch, 0, fadeInSamples, 0.0f, 1.0f);

        if (fadeOutSamples > 0)
            for (int ch = 0; ch < numChannels; ++ch)
                result.applyGainRamp (ch, cleanLen - fadeOutSamples, fadeOutSamples, 1.0f, 0.0f);

        return result;
    }
}
