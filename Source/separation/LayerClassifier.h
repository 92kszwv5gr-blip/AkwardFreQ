#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include "RegionTypes.h"

namespace afq
{
    // Tier D: assigns a LayerType to a FeatureVector.
    //
    // By default this runs the rule-based (fuzzy trapezoidal-membership) heuristics
    // described in docs/FEATURE_SPEC.md's sibling design notes — this is what ships
    // in v1 and requires no training data.
    //
    // If the user has retrained a classifier via tools/retrain_layer_classifier.py
    // and dropped the resulting .onnx into Models/UserTrained/, loadUserModel()
    // swaps classification over to that model transparently; segmentation
    // (onset detection, frame extraction) in DrumSubSplitter/OtherSubSplitter is
    // unaffected either way.
    class LayerClassifier
    {
    public:
        LayerClassifier();
        ~LayerClassifier();

        // Returns true if a valid ONNX classifier was loaded. On failure the
        // classifier silently keeps using rule-based heuristics.
        bool loadUserModel (const juce::File& onnxPath);
        bool hasUserModel() const noexcept;

        struct Classification
        {
            LayerType type = LayerType::Unclassified;
            float confidence = 0.0f;
        };

        // `candidates` restricts the decision to a plausible subset (e.g. only
        // {Kick, HiHat, Percussion, Breakbeat} when called on the drums bus).
        // Kept even when a user ONNX model is active, so the trained model still
        // only ever picks among physically-plausible labels for that bus.
        // `bias`, if non-null, multiplies each candidate's score/probability
        // before the argmax (see GenreBias.h) — used to nudge close calls toward
        // layer types more common in the selected genre preset. It cannot push a
        // type over minClassifierConfidence on its own if the raw score is ~0.
        using BiasMap = std::unordered_map<LayerType, float>;
        Classification classify (const FeatureVector& fv, const std::vector<LayerType>& candidates,
                                  const BiasMap* bias = nullptr) const;

        static constexpr float minClassifierConfidence = 0.35f;

    private:
        Classification classifyRuleBased (const FeatureVector& fv, const std::vector<LayerType>& candidates,
                                           const BiasMap* bias) const;
        Classification classifyWithOnnxModel (const FeatureVector& fv, const std::vector<LayerType>& candidates,
                                               const BiasMap* bias) const;
        float scoreFor (LayerType type, const FeatureVector& fv) const;

        struct OnnxModel;
        std::unique_ptr<OnnxModel> onnxModel_;
    };
}
