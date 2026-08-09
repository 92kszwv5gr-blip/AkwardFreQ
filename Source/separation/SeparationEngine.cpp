#include "SeparationEngine.h"
#include "AnalysisUtils.h"
#include "LayerRenderer.h"
#include <thread>
#include <algorithm>
#include <cmath>

namespace afq
{
    namespace
    {
        constexpr double kSustainHopSeconds = 0.05; // 50ms envelope hop for bass gating
        constexpr float kBassSilenceFloorDb = -40.0f;
    }

    SeparationEngine::SeparationEngine()
        : drumSplitter_ (classifier_), otherSplitter_ (classifier_)
    {
    }

    SeparationEngine::~SeparationEngine() = default;

    bool SeparationEngine::loadModels (const juce::File& demucsOnnxPath, const juce::File& userClassifierOnnxPath)
    {
        const bool demucsOk = demucs_.loadModel (demucsOnnxPath);
        if (userClassifierOnnxPath.existsAsFile())
            classifier_.loadUserModel (userClassifierOnnxPath); // failure here is non-fatal, falls back to heuristics
        return demucsOk;
    }

    bool SeparationEngine::isDemucsModelLoaded() const { return demucs_.isModelLoaded(); }

    std::vector<Region> SeparationEngine::extractBassRegions (const juce::AudioBuffer<float>& bassBuffer,
                                                                double sampleRate) const
    {
        std::vector<Region> regions;
        const int numSamples = bassBuffer.getNumSamples();
        if (numSamples == 0) return regions;

        const auto mono = mixToMono (bassBuffer);
        const float* data = mono.getReadPointer (0);

        float peak = 1.0e-6f;
        for (int i = 0; i < numSamples; ++i) peak = std::max (peak, std::abs (data[i]));
        const float floorLevel = peak * juce::Decibels::decibelsToGain (kBassSilenceFloorDb);

        const int hop = std::max (1, (int) (sampleRate * kSustainHopSeconds));
        bool inRegion = false;
        int64_t regionStart = 0;

        FrameFeatureExtractor extractor;
        std::vector<float> magScratch ((size_t) (extractor.fftSize() / 2 + 1), 0.0f);

        auto closeRegion = [&] (int64_t end)
        {
            const int len = (int) (end - regionStart);
            if (len <= 0) return;
            Region region;
            region.type = LayerType::Bass;
            region.startSample = regionStart;
            region.endSample = end;
            region.confidence = 1.0f; // direct from Tier A, not a Tier B/C guess
            region.features = extractor.analyzeSegment (data + regionStart, len, sampleRate, magScratch.data());
            regions.push_back (region);
        };

        for (int pos = 0; pos < numSamples; pos += hop)
        {
            const int len = std::min (hop, numSamples - pos);
            const float rms = computeRms (data + pos, len);
            const bool active = rms > floorLevel;

            if (active && ! inRegion) { inRegion = true; regionStart = pos; }
            else if (! active && inRegion) { inRegion = false; closeRegion (pos); }
        }
        if (inRegion) closeRegion (numSamples);

        return regions;
    }

    void SeparationEngine::separateAsync (juce::AudioBuffer<float> mix, double sampleRate, params::GenrePreset genre,
                                           ResultCallback onComplete, ProgressCallback onProgress)
    {
        if (running_.exchange (true))
        {
            juce::Logger::writeToLog ("SeparationEngine::separateAsync called while a job is already running — ignored.");
            return;
        }

        std::thread worker ([this, mix = std::move (mix), sampleRate, genre,
                              onComplete = std::move (onComplete), onProgress = std::move (onProgress)]() mutable
        {
            runJob (std::move (mix), sampleRate, genre, std::move (onComplete), std::move (onProgress));
            running_.store (false);
        });
        worker.detach();
    }

    void SeparationEngine::runJob (juce::AudioBuffer<float> mix, double sampleRate, params::GenrePreset genre,
                                    ResultCallback onComplete, ProgressCallback onProgress)
    {
        auto notifyProgress = [&onProgress] (float p, const juce::String& label)
        {
            if (! onProgress) return;
            juce::MessageManager::callAsync ([onProgress, p, label]() { onProgress (p, label); });
        };

        if (! demucs_.isModelLoaded())
        {
            notifyProgress (1.0f, "Error: Demucs model not loaded");
            if (onComplete)
                juce::MessageManager::callAsync ([onComplete]() { onComplete (SeparationResult {}); });
            return;
        }

        notifyProgress (0.0f, "Separating stems (HT-Demucs)");
        auto demucsOut = demucs_.separate (mix, sampleRate, [&] (float p)
        {
            notifyProgress (p * 0.6f, "Separating stems (HT-Demucs)");
        });

        notifyProgress (0.65f, "Classifying drum layers");
        auto drumRegions = drumSplitter_.splitRegions (demucsOut.drums, sampleRate, genre);

        notifyProgress (0.75f, "Classifying melodic / FX layers");
        auto otherRegions = otherSplitter_.splitRegions (demucsOut.other, sampleRate, genre);

        notifyProgress (0.85f, "Gating bass");
        auto bassRegions = extractBassRegions (demucsOut.bass, sampleRate);

        notifyProgress (0.9f, "Rendering isolated layers");
        auto drumLayers = LayerRenderer::render (demucsOut.drums, sampleRate, drumRegions);
        auto otherLayers = LayerRenderer::render (demucsOut.other, sampleRate, otherRegions);
        auto bassLayers = LayerRenderer::render (demucsOut.bass, sampleRate, bassRegions);

        SeparationResult result;
        result.sampleRate = sampleRate;
        result.numChannels = mix.getNumChannels();
        result.bassBuffer = std::move (demucsOut.bass);
        result.drumsBuffer = std::move (demucsOut.drums);
        result.otherBuffer = std::move (demucsOut.other);

        for (size_t i = 0; i < (size_t) LayerType::Count; ++i)
        {
            if (drumLayers[i].getNumSamples() > 0)       result.layerBuffers[i] = std::move (drumLayers[i]);
            else if (otherLayers[i].getNumSamples() > 0) result.layerBuffers[i] = std::move (otherLayers[i]);
            else if (bassLayers[i].getNumSamples() > 0)  result.layerBuffers[i] = std::move (bassLayers[i]);
        }

        result.regions.reserve (drumRegions.size() + otherRegions.size() + bassRegions.size());
        result.regions.insert (result.regions.end(), drumRegions.begin(), drumRegions.end());
        result.regions.insert (result.regions.end(), otherRegions.begin(), otherRegions.end());
        result.regions.insert (result.regions.end(), bassRegions.begin(), bassRegions.end());
        std::sort (result.regions.begin(), result.regions.end(),
                   [] (const Region& a, const Region& b) { return a.startSample < b.startSample; });

        notifyProgress (0.95f, "Estimating BPM / key");
        const auto monoMix = mixToMono (mix);
        const auto mixOnsets = detectOnsets (monoMix, sampleRate);
        result.estimatedBpm = estimateBpmFromOnsets (mixOnsets, sampleRate);
        result.estimatedKey = estimateKey (monoMix, sampleRate);

        notifyProgress (1.0f, "Done");
        if (onComplete)
            juce::MessageManager::callAsync ([onComplete, result = std::move (result)]() mutable { onComplete (std::move (result)); });
    }
}
