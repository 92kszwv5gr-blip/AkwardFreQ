#include "DrumSubSplitter.h"
#include "AnalysisUtils.h"
#include "GenreBias.h"
#include <bitset>
#include <algorithm>

namespace afq
{
    namespace
    {
        juce::AudioBuffer<float> mixToMono (const juce::AudioBuffer<float>& buffer)
        {
            juce::AudioBuffer<float> mono (1, buffer.getNumSamples());
            mono.clear();
            const float scale = 1.0f / (float) juce::jmax (1, buffer.getNumChannels());
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                mono.addFrom (0, 0, buffer, ch, 0, buffer.getNumSamples(), scale);
            return mono;
        }

        constexpr int kMaxSegmentSamplesAt44k = 44100; // 1s cap on any single onset segment
    }

    DrumSubSplitter::DrumSubSplitter (LayerClassifier& classifier) : classifier_ (classifier) {}

    std::vector<std::pair<int64_t, int64_t>> DrumSubSplitter::detectBreakbeatWindows (
        const std::vector<int64_t>& onsets, int64_t totalLengthSamples, double sampleRate)
    {
        std::vector<std::pair<int64_t, int64_t>> flagged;
        if (onsets.size() < 8) return flagged;

        double bpm = estimateBpmFromOnsets (onsets, sampleRate);
        if (bpm < 1.0) bpm = 145.0; // typical psytrance fallback

        const double barLenSamples = sampleRate * 60.0 / bpm * 4.0;
        const double windowLenD = barLenSamples * 2.0; // 2-bar window
        const int64_t windowLen = (int64_t) windowLenD;
        if (windowLen < 1000) return flagged;

        constexpr int kGridDivisions = 32; // 1/16 notes across a 2-bar window
        const int numWindows = (int) (totalLengthSamples / windowLen);
        if (numWindows < 2) return flagged;

        std::vector<std::bitset<kGridDivisions>> grids ((size_t) numWindows);
        for (auto onset : onsets)
        {
            const int w = (int) (onset / windowLen);
            if (w < 0 || w >= numWindows) continue;
            const int64_t offsetInWindow = onset - (int64_t) w * windowLen;
            int bin = (int) ((double) offsetInWindow / (double) windowLen * kGridDivisions);
            bin = juce::jlimit (0, kGridDivisions - 1, bin);
            grids[(size_t) w].set ((size_t) bin);
        }

        std::vector<bool> windowFlagged ((size_t) numWindows, false);
        for (int w = 0; w + 1 < numWindows; ++w)
        {
            const auto& a = grids[(size_t) w];
            const auto& b = grids[(size_t) (w + 1)];
            const auto intersection = (a & b).count();
            const auto unionCount = (a | b).count();
            if (unionCount == 0) continue;

            const double jaccard = (double) intersection / (double) unionCount;
            const bool busy = a.count() >= (size_t) (kGridDivisions * 0.4);
            if (jaccard > 0.6 && busy)
            {
                windowFlagged[(size_t) w] = true;
                windowFlagged[(size_t) (w + 1)] = true;
            }
        }

        // Merge adjacent flagged windows into contiguous ranges.
        int w = 0;
        while (w < numWindows)
        {
            if (! windowFlagged[(size_t) w]) { ++w; continue; }
            int start = w;
            while (w < numWindows && windowFlagged[(size_t) w]) ++w;
            flagged.emplace_back ((int64_t) start * windowLen, (int64_t) w * windowLen);
        }

        return flagged;
    }

    std::vector<Region> DrumSubSplitter::splitRegions (const juce::AudioBuffer<float>& drumsBuffer, double sampleRate,
                                                        params::GenrePreset genre)
    {
        std::vector<Region> regions;
        if (drumsBuffer.getNumSamples() == 0) return regions;

        const auto bias = genreBias (genre);

        const auto mono = mixToMono (drumsBuffer);
        const auto onsets = detectOnsets (mono, sampleRate);
        if (onsets.empty()) return regions;

        const auto breakbeatWindows = detectBreakbeatWindows (onsets, mono.getNumSamples(), sampleRate);
        const int maxSegmentSamples = (int) (kMaxSegmentSamplesAt44k * (sampleRate / 44100.0));

        FrameFeatureExtractor extractor;
        std::vector<float> magScratch ((size_t) (extractor.fftSize() / 2 + 1), 0.0f);
        const float* monoData = mono.getReadPointer (0);
        const int numSamples = mono.getNumSamples();

        const std::vector<LayerType> candidates { LayerType::Kick, LayerType::HiHat, LayerType::Percussion };

        for (size_t i = 0; i < onsets.size(); ++i)
        {
            const int64_t start = onsets[i];
            int64_t end = (i + 1 < onsets.size()) ? onsets[i + 1] : (int64_t) numSamples;
            end = std::min (end, start + maxSegmentSamples);
            end = std::min (end, (int64_t) numSamples);
            const int len = (int) (end - start);
            if (len <= 8) continue;

            Region region;
            region.startSample = start;
            region.endSample = end;
            region.features = extractor.analyzeSegment (monoData + start, len, sampleRate, magScratch.data());

            const bool inBreakbeatWindow = std::any_of (breakbeatWindows.begin(), breakbeatWindows.end(),
                [start] (const auto& range) { return start >= range.first && start < range.second; });

            if (inBreakbeatWindow)
            {
                region.type = LayerType::Breakbeat;
                region.confidence = 0.6f;
            }
            else
            {
                const auto result = classifier_.classify (region.features, candidates, &bias);
                region.type = result.type;
                region.confidence = result.confidence;
            }

            regions.push_back (region);
        }

        return regions;
    }
}
