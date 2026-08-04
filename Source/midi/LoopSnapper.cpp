#include "LoopSnapper.h"
#include "../separation/AnalysisUtils.h"
#include <limits>
#include <cmath>

namespace afq
{
    LoopSnapper::Result LoopSnapper::snapToLoop (const juce::AudioBuffer<float>& buffer, double sampleRate, double bpm,
                                                   int64_t roughStart, int64_t roughEnd, int bars)
    {
        Result result;
        const int64_t totalLen = buffer.getNumSamples();

        if (bpm <= 0.0)
        {
            result.startSample = juce::jlimit ((int64_t) 0, totalLen, roughStart);
            result.endSample = juce::jlimit (result.startSample, totalLen, roughEnd);
            result.snapped = false;
            return result;
        }

        const double barLenSamplesD = sampleRate * 60.0 / bpm * 4.0;
        const int64_t barLenSamples = (int64_t) std::llround (barLenSamplesD);
        const int64_t targetLen = barLenSamples * (int64_t) bars;

        if (barLenSamples < 8 || targetLen >= totalLen)
        {
            result.startSample = juce::jlimit ((int64_t) 0, totalLen, roughStart);
            result.endSample = juce::jlimit (result.startSample, totalLen, roughEnd);
            result.snapped = false;
            return result;
        }

        int64_t gridStart = (int64_t) std::llround ((double) roughStart / barLenSamplesD) * barLenSamples;
        gridStart = juce::jlimit ((int64_t) 0, totalLen - targetLen, gridStart);

        const int64_t searchRadius = juce::jmax ((int64_t) 1, barLenSamples / 8); // an eighth-bar of wiggle room
        const int windowLen = (int) juce::jmax ((int64_t) 64, (int64_t) (sampleRate * 0.010)); // ~10ms

        const auto mono = mixToMono (buffer);
        const float* data = mono.getReadPointer (0);

        int64_t bestDelta = 0;
        double bestCost = std::numeric_limits<double>::max();
        bool foundValid = false;

        for (int64_t delta = -searchRadius; delta <= searchRadius; ++delta)
        {
            const int64_t s = gridStart + delta;
            const int64_t e = s + targetLen;
            if (s - windowLen < 0 || e > totalLen) continue;

            double cost = 0.0;
            const float* leadIntoStart = data + (s - windowLen);
            const float* leadIntoEnd = data + (e - windowLen);
            for (int i = 0; i < windowLen; ++i)
            {
                const double d = (double) leadIntoStart[i] - (double) leadIntoEnd[i];
                cost += d * d;
            }

            if (cost < bestCost) { bestCost = cost; bestDelta = delta; foundValid = true; }
        }

        result.startSample = foundValid ? (gridStart + bestDelta) : gridStart;
        result.endSample = result.startSample + targetLen;
        result.snapped = true;
        return result;
    }
}
