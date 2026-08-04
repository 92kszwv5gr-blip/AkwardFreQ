#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <functional>
#include "RegionTypes.h"
#include "DemucsEngine.h"
#include "LayerClassifier.h"
#include "DrumSubSplitter.h"
#include "OtherSubSplitter.h"
#include "../Params.h"

namespace afq
{
    // Orchestrates the full Tier A -> B/C -> D pipeline for one imported/captured
    // track, entirely on a background thread. Never touches the audio thread.
    class SeparationEngine
    {
    public:
        SeparationEngine();
        ~SeparationEngine();

        // `userClassifierOnnxPath` may point to a nonexistent file — that's the
        // normal v1 case before any correction data has been retrained; the
        // classifier just falls back to rule-based heuristics.
        bool loadModels (const juce::File& demucsOnnxPath, const juce::File& userClassifierOnnxPath);
        bool isDemucsModelLoaded() const;

        using ResultCallback = std::function<void (SeparationResult)>;
        using ProgressCallback = std::function<void (float progress0to1, juce::String stageLabel)>;

        // Runs on a detached background thread; both callbacks are delivered via
        // juce::MessageManager::callAsync, so they're safe to touch UI from.
        // No-op (with an immediate log warning) if a job is already running —
        // v1 supports one in-flight separation at a time.
        void separateAsync (juce::AudioBuffer<float> mix, double sampleRate, params::GenrePreset genre,
                             ResultCallback onComplete, ProgressCallback onProgress = nullptr);

        bool isRunning() const noexcept { return running_.load(); }

    private:
        DemucsEngine demucs_;
        LayerClassifier classifier_;
        DrumSubSplitter drumSplitter_;
        OtherSubSplitter otherSplitter_;
        std::atomic<bool> running_ { false };

        std::vector<Region> extractBassRegions (const juce::AudioBuffer<float>& bassBuffer, double sampleRate) const;

        void runJob (juce::AudioBuffer<float> mix, double sampleRate, params::GenrePreset genre,
                     ResultCallback onComplete, ProgressCallback onProgress);
    };
}
