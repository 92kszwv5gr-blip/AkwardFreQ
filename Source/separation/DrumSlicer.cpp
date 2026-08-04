#include "DrumSlicer.h"
#include "AnalysisUtils.h"
#include <algorithm>

namespace afq
{
    namespace
    {
        juce::AudioBuffer<float> mixRangeToMono (const juce::AudioBuffer<float>& buffer, int64_t start, int64_t end)
        {
            const int len = (int) (end - start);
            juce::AudioBuffer<float> mono (1, len);
            mono.clear();
            const float scale = 1.0f / (float) juce::jmax (1, buffer.getNumChannels());
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                mono.addFrom (0, 0, buffer, ch, (int) start, len, scale);
            return mono;
        }
    }

    std::vector<DrumSlice> DrumSlicer::slice (const juce::AudioBuffer<float>& sourceBuffer, double sampleRate,
                                               int64_t rangeStartSample, int64_t rangeEndSample, int maxSlices)
    {
        std::vector<DrumSlice> slices;
        const int64_t totalLen = sourceBuffer.getNumSamples();

        int64_t start = juce::jlimit ((int64_t) 0, totalLen, rangeStartSample);
        int64_t end = (rangeEndSample > start) ? juce::jmin (rangeEndSample, totalLen) : totalLen;
        if (end - start < 8) return slices;

        const auto mono = mixRangeToMono (sourceBuffer, start, end);
        auto onsets = detectOnsets (mono, sampleRate);
        if (onsets.empty())
        {
            // No detectable transients (e.g. a sustained/atonal input) — treat
            // the whole range as a single slice rather than returning nothing.
            slices.push_back ({ start, end, 1 });
            return slices;
        }

        const int count = juce::jmin ((int) onsets.size(), maxSlices);
        for (int i = 0; i < count; ++i)
        {
            const int64_t sliceStart = start + onsets[(size_t) i];
            const int64_t sliceEnd = (i + 1 < (int) onsets.size()) ? (start + onsets[(size_t) (i + 1)]) : end;
            if (sliceEnd - sliceStart < 8) continue;
            slices.push_back ({ sliceStart, sliceEnd, (int) slices.size() + 1 });
        }

        return slices;
    }
}
