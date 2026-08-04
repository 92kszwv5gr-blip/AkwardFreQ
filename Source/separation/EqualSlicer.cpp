#include "EqualSlicer.h"

namespace afq
{
    std::vector<DrumSlice> EqualSlicer::slice (int64_t bufferLengthSamples, int sliceCount,
                                                int64_t rangeStartSample, int64_t rangeEndSample)
    {
        std::vector<DrumSlice> slices;

        const int64_t start = juce::jlimit ((int64_t) 0, bufferLengthSamples, rangeStartSample);
        const int64_t end = (rangeEndSample > start) ? juce::jmin (rangeEndSample, bufferLengthSamples) : bufferLengthSamples;
        const int64_t total = end - start;
        if (total <= 0) return slices;

        const int count = juce::jlimit (1, 256, sliceCount);
        slices.reserve ((size_t) count);

        for (int i = 0; i < count; ++i)
        {
            const int64_t sliceStart = start + (total * i) / count;
            const int64_t sliceEnd = start + (total * (i + 1)) / count;
            if (sliceEnd <= sliceStart) continue;
            slices.push_back ({ sliceStart, sliceEnd, (int) slices.size() + 1 });
        }

        return slices;
    }
}
