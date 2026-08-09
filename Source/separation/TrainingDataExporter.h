#pragma once

#include <juce_core/juce_core.h>
#include "RegionTypes.h"

namespace afq
{
    // Writes a track's classified/corrected regions (feature vector + label,
    // per docs/FEATURE_SPEC.md) as a JSON file for tools/retrain_layer_classifier.py
    // to ingest later. Because the feature vectors are already computed by the
    // Tier B/C splitters (stored on each Region), this export needs no audio
    // re-analysis — it's a direct dump, safe to call from the message thread.
    //
    // Every region is included, not just user-corrected ones, so the retrained
    // classifier also learns from the (majority) cases where the heuristic
    // already got it right — `userCorrected` on each entry tells the training
    // script which label to trust unconditionally vs. which to weight as a
    // heuristic-agreed example.
    class TrainingDataExporter
    {
    public:
        static bool save (const SeparationResult& result, const juce::File& sourceAudioFile,
                           const juce::File& trainingDataFolder, juce::String& errorMessage);
    };
}
