#include "OtherSubSplitter.h"
#include "AnalysisUtils.h"
#include "GenreBias.h"
#include <algorithm>

namespace afq
{
    namespace
    {
        constexpr int kMaxOnsetSegmentSamplesAt44k = 26460;  // 600ms
        constexpr double kMinSustainGapSeconds = 0.5;
        constexpr float kSilenceRmsFloor = 0.003f;
    }

    OtherSubSplitter::OtherSubSplitter (LayerClassifier& classifier) : classifier_ (classifier) {}

    std::vector<Region> OtherSubSplitter::splitRegions (const juce::AudioBuffer<float>& otherBuffer, double sampleRate,
                                                          params::GenrePreset genre)
    {
        std::vector<Region> regions;
        if (otherBuffer.getNumSamples() == 0) return regions;

        const auto bias = genreBias (genre);
        const auto mono = mixToMono (otherBuffer);
        const float* monoData = mono.getReadPointer (0);
        const int numSamples = mono.getNumSamples();

        const auto onsets = detectOnsets (mono, sampleRate);

        FrameFeatureExtractor extractor;
        std::vector<float> magScratch ((size_t) (extractor.fftSize() / 2 + 1), 0.0f);

        const int maxOnsetSegment = (int) (kMaxOnsetSegmentSamplesAt44k * (sampleRate / 44100.0));
        const int64_t minSustainGap = (int64_t) (kMinSustainGapSeconds * sampleRate);

        const std::vector<LayerType> onsetCandidates {
            LayerType::Stabs, LayerType::Zap, LayerType::Glitch, LayerType::FX, LayerType::SynthLead
        };
        const std::vector<LayerType> sustainCandidates { LayerType::SynthLead, LayerType::Atmosphere };

        // Pass 1: onset-triggered transient segments.
        for (size_t i = 0; i < onsets.size(); ++i)
        {
            const int64_t start = onsets[i];
            int64_t end = (i + 1 < onsets.size()) ? std::min (onsets[i + 1], start + maxOnsetSegment)
                                                    : std::min ((int64_t) numSamples, start + maxOnsetSegment);
            end = std::min (end, (int64_t) numSamples);
            const int len = (int) (end - start);
            if (len <= 8) continue;

            Region region;
            region.startSample = start;
            region.endSample = end;
            region.features = extractor.analyzeSegment (monoData + start, len, sampleRate, magScratch.data());

            const auto result = classifier_.classify (region.features, onsetCandidates, &bias);
            region.type = result.type;
            region.confidence = result.confidence;
            regions.push_back (region);
        }

        // Pass 2: sustain segments in the gaps between onsets (and before the
        // first / after the last onset), which is where pads/atmospheres and
        // long lead notes actually live.
        std::vector<int64_t> boundaries;
        boundaries.push_back (0);
        for (auto o : onsets) boundaries.push_back (o);
        boundaries.push_back (numSamples);

        for (size_t i = 0; i + 1 < boundaries.size(); ++i)
        {
            const int64_t start = boundaries[i];
            const int64_t end = boundaries[i + 1];
            if (end - start < minSustainGap) continue;

            const int len = (int) (end - start);
            if (computeRms (monoData + start, len) < kSilenceRmsFloor) continue;

            Region region;
            region.startSample = start;
            region.endSample = end;
            // Sustain segments can be much longer than a single FFT frame — analyze
            // a representative window from the middle so attack/decay timing reflect
            // the sustain character rather than getting clipped by the segment cap.
            const int analysisLen = std::min (len, maxOnsetSegment * 4);
            const int64_t analysisStart = start + (len - analysisLen) / 2;
            region.features = extractor.analyzeSegment (monoData + analysisStart, analysisLen, sampleRate, magScratch.data());

            const auto result = classifier_.classify (region.features, sustainCandidates, &bias);
            region.type = result.type;
            region.confidence = result.confidence;
            regions.push_back (region);
        }

        std::sort (regions.begin(), regions.end(),
                   [] (const Region& a, const Region& b) { return a.startSample < b.startSample; });

        return regions;
    }
}
