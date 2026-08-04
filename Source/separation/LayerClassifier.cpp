#include "LayerClassifier.h"
#include "OnnxMingwShim.h"
#include <onnxruntime_cxx_api.h>
#include <algorithm>
#include <cmath>

namespace afq
{
    namespace
    {
        // Trapezoidal fuzzy-membership: 0 outside [loStart,hiEnd], 1 on the plateau
        // [loPlateau,hiPlateau], linear ramp in between. This is the building block
        // for every rule-based layer score below.
        float trapezoid (float x, float loStart, float loPlateau, float hiPlateau, float hiEnd)
        {
            if (x <= loStart || x >= hiEnd) return 0.0f;
            if (x >= loPlateau && x <= hiPlateau) return 1.0f;
            if (x < loPlateau) return (x - loStart) / std::max (1.0e-6f, loPlateau - loStart);
            return (hiEnd - x) / std::max (1.0e-6f, hiEnd - hiPlateau);
        }

        float average (std::initializer_list<float> vals, std::initializer_list<float> weights)
        {
            float num = 0.0f, den = 0.0f;
            auto vi = vals.begin();
            auto wi = weights.begin();
            for (; vi != vals.end(); ++vi, ++wi) { num += (*vi) * (*wi); den += *wi; }
            return den > 0.0f ? num / den : 0.0f;
        }
    }

    //==============================================================================
    // ONNX Runtime session wrapper for a user-retrained classifier.
    struct LayerClassifier::OnnxModel
    {
        Ort::Env env { ORT_LOGGING_LEVEL_WARNING, "AkwardFreQLayerClassifier" };
        std::unique_ptr<Ort::Session> session;
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault);
        std::string inputName, outputName;

        bool load (const juce::File& onnxPath)
        {
            try
            {
               #if JUCE_WINDOWS
                const auto wpath = onnxPath.getFullPathName().toWideCharPointer();
                session = std::make_unique<Ort::Session> (env, wpath, Ort::SessionOptions {});
               #else
                const auto path = onnxPath.getFullPathName().toStdString();
                session = std::make_unique<Ort::Session> (env, path.c_str(), Ort::SessionOptions {});
               #endif

                Ort::AllocatorWithDefaultOptions allocator;
                auto inName = session->GetInputNameAllocated (0, allocator);
                auto outName = session->GetOutputNameAllocated (0, allocator);
                inputName = inName.get();
                outputName = outName.get();
                return true;
            }
            catch (const Ort::Exception&)
            {
                session.reset();
                return false;
            }
        }

        bool classify (const FeatureVector& fv, std::array<float, kNumClassifiedLayers>& outLogits)
        {
            if (session == nullptr) return false;

            std::array<int64_t, 2> inputShape { 1, kFeatureVectorSize };
            auto inputTensor = Ort::Value::CreateTensor<float> (
                memoryInfo, const_cast<float*> (fv.values.data()), kFeatureVectorSize,
                inputShape.data(), inputShape.size());

            const char* inputNames[]  = { inputName.c_str() };
            const char* outputNames[] = { outputName.c_str() };

            try
            {
                auto outputTensors = session->Run (Ort::RunOptions { nullptr },
                                                     inputNames, &inputTensor, 1,
                                                     outputNames, 1);
                const float* logits = outputTensors.front().GetTensorData<float>();
                std::copy (logits, logits + kNumClassifiedLayers, outLogits.begin());
                return true;
            }
            catch (const Ort::Exception&)
            {
                return false;
            }
        }
    };

    LayerClassifier::LayerClassifier() = default;
    LayerClassifier::~LayerClassifier() = default;

    bool LayerClassifier::loadUserModel (const juce::File& onnxPath)
    {
        if (! onnxPath.existsAsFile()) return false;
        auto model = std::make_unique<OnnxModel>();
        if (! model->load (onnxPath)) return false;
        onnxModel_ = std::move (model);
        return true;
    }

    bool LayerClassifier::hasUserModel() const noexcept { return onnxModel_ != nullptr; }

    namespace
    {
        float biasFor (const LayerClassifier::BiasMap* bias, LayerType type)
        {
            if (bias == nullptr) return 1.0f;
            auto it = bias->find (type);
            return it != bias->end() ? it->second : 1.0f;
        }
    }

    LayerClassifier::Classification LayerClassifier::classify (const FeatureVector& fv,
                                                                 const std::vector<LayerType>& candidates,
                                                                 const BiasMap* bias) const
    {
        if (onnxModel_ != nullptr)
        {
            auto result = classifyWithOnnxModel (fv, candidates, bias);
            if (result.type != LayerType::Unclassified || result.confidence > 0.0f)
                return result;
        }
        return classifyRuleBased (fv, candidates, bias);
    }

    LayerClassifier::Classification LayerClassifier::classifyWithOnnxModel (const FeatureVector& fv,
                                                                              const std::vector<LayerType>& candidates,
                                                                              const BiasMap* bias) const
    {
        std::array<float, kNumClassifiedLayers> logits {};
        if (! onnxModel_->classify (fv, logits))
            return classifyRuleBased (fv, candidates, bias);

        // Softmax
        const float maxLogit = *std::max_element (logits.begin(), logits.end());
        std::array<float, kNumClassifiedLayers> probs {};
        float sumExp = 0.0f;
        for (int i = 0; i < kNumClassifiedLayers; ++i)
        {
            probs[(size_t) i] = std::exp (logits[(size_t) i] - maxLogit);
            sumExp += probs[(size_t) i];
        }
        for (auto& p : probs) p /= sumExp;

        LayerType bestType = LayerType::Unclassified;
        float bestProb = 0.0f;
        for (auto candidate : candidates)
        {
            const int idx = (int) candidate;
            if (idx < 0 || idx >= kNumClassifiedLayers) continue;
            const float biased = probs[(size_t) idx] * biasFor (bias, candidate);
            if (biased > bestProb) { bestProb = biased; bestType = candidate; }
        }

        if (bestProb < minClassifierConfidence)
            return { LayerType::Unclassified, bestProb };

        return { bestType, bestProb };
    }

    float LayerClassifier::scoreFor (LayerType type, const FeatureVector& fv) const
    {
        using I = FeatureVector::Index;

        switch (type)
        {
            case LayerType::Kick:
                return average ({
                    trapezoid (fv[I::BandEnergyLow], 0.30f, 0.55f, 1.0f, 1.0f),
                    trapezoid (fv[I::AttackTimeMs], 0.0f, 0.0f, 5.0f, 25.0f),
                    trapezoid (fv[I::DecayTimeMs], 30.0f, 80.0f, 300.0f, 600.0f),
                    trapezoid (fv[I::SpectralCentroid], 0.0f, 0.0f, 0.08f, 0.20f)
                }, { 0.35f, 0.30f, 0.20f, 0.15f });

            case LayerType::HiHat:
                return average ({
                    trapezoid (fv[I::BandEnergyHigh], 0.35f, 0.6f, 1.0f, 1.0f),
                    trapezoid (fv[I::AttackTimeMs], 0.0f, 0.0f, 3.0f, 15.0f),
                    trapezoid (fv[I::DecayTimeMs], 0.0f, 0.0f, 150.0f, 400.0f),
                    trapezoid (fv[I::SpectralFlatness], 0.25f, 0.45f, 1.0f, 1.0f)
                }, { 0.35f, 0.2f, 0.2f, 0.25f });

            case LayerType::Percussion:
                return average ({
                    trapezoid (fv[I::BandEnergyMid], 0.35f, 0.55f, 1.0f, 1.0f),
                    trapezoid (fv[I::AttackTimeMs], 0.0f, 0.0f, 10.0f, 40.0f),
                    trapezoid (fv[I::DecayTimeMs], 20.0f, 60.0f, 250.0f, 500.0f),
                    trapezoid (fv[I::SpectralFlatness], 0.15f, 0.3f, 0.7f, 0.9f)
                }, { 0.3f, 0.25f, 0.25f, 0.2f });

            case LayerType::Breakbeat:
                // Per-onset features rarely justify this on their own — DrumSubSplitter
                // overrides whole windows via loop/pattern-repetition detection.
                // Keep a modest baseline so it can still win by default among candidates
                // if nothing else scores meaningfully.
                return average ({
                    trapezoid (fv[I::BandEnergyMid], 0.25f, 0.4f, 1.0f, 1.0f),
                    trapezoid (fv[I::ZeroCrossingRate], 0.1f, 0.2f, 0.6f, 0.9f)
                }, { 0.6f, 0.4f }) * 0.5f;

            case LayerType::Bass:
                return average ({
                    trapezoid (fv[I::BandEnergyLow], 0.5f, 0.7f, 1.0f, 1.0f),
                    trapezoid (fv[I::DecayTimeMs], 200.0f, 400.0f, 1000.0f, 1000.0f),
                    trapezoid (fv[I::Harmonicity], 0.4f, 0.6f, 1.0f, 1.0f)
                }, { 0.4f, 0.3f, 0.3f });

            case LayerType::SynthLead:
                return average ({
                    trapezoid (fv[I::DecayTimeMs], 300.0f, 600.0f, 1000.0f, 1000.0f),
                    trapezoid (fv[I::SpectralFlux], 0.0f, 0.0f, 0.15f, 0.35f),
                    trapezoid (fv[I::Harmonicity], 0.5f, 0.7f, 1.0f, 1.0f),
                    trapezoid (fv[I::PitchConfidence], 0.5f, 0.7f, 1.0f, 1.0f),
                    trapezoid (fv[I::BandEnergyMid], 0.3f, 0.5f, 1.0f, 1.0f)
                }, { 0.2f, 0.15f, 0.25f, 0.25f, 0.15f });

            case LayerType::Stabs:
                return average ({
                    trapezoid (fv[I::AttackTimeMs], 0.0f, 0.0f, 10.0f, 30.0f),
                    trapezoid (fv[I::DecayTimeMs], 50.0f, 100.0f, 350.0f, 600.0f),
                    trapezoid (fv[I::Harmonicity], 0.35f, 0.55f, 0.9f, 1.0f),
                    trapezoid (fv[I::SpectralFlux], 0.1f, 0.25f, 0.6f, 0.9f)
                }, { 0.3f, 0.25f, 0.25f, 0.2f });

            case LayerType::Atmosphere:
                return average ({
                    trapezoid (fv[I::DecayTimeMs], 500.0f, 800.0f, 1000.0f, 1000.0f),
                    trapezoid (fv[I::SpectralFlux], 0.0f, 0.0f, 0.1f, 0.25f),
                    trapezoid (fv[I::SpectralFlatness], 0.2f, 0.35f, 0.7f, 0.9f),
                    trapezoid (fv[I::AttackTimeMs], 50.0f, 120.0f, 200.0f, 200.0f)
                }, { 0.3f, 0.3f, 0.2f, 0.2f });

            case LayerType::Zap:
                return average ({
                    trapezoid (fv[I::SpectralFlux], 0.3f, 0.5f, 1.0f, 1.0f),
                    trapezoid (fv[I::AttackTimeMs], 0.0f, 0.0f, 15.0f, 50.0f),
                    trapezoid (fv[I::DecayTimeMs], 50.0f, 100.0f, 400.0f, 700.0f),
                    trapezoid (fv[I::SpectralCentroid], 0.15f, 0.3f, 0.7f, 1.0f),
                    trapezoid (fv[I::Harmonicity], 0.3f, 0.5f, 0.9f, 1.0f)
                }, { 0.3f, 0.2f, 0.15f, 0.2f, 0.15f });

            case LayerType::Glitch:
                return average ({
                    trapezoid (fv[I::SpectralFlatness], 0.4f, 0.6f, 1.0f, 1.0f),
                    trapezoid (fv[I::DecayTimeMs], 0.0f, 0.0f, 120.0f, 300.0f),
                    trapezoid (fv[I::SpectralFlux], 0.25f, 0.45f, 1.0f, 1.0f),
                    trapezoid (fv[I::ZeroCrossingRate], 0.15f, 0.3f, 1.0f, 1.0f)
                }, { 0.3f, 0.25f, 0.25f, 0.2f });

            case LayerType::FX:
            default:
                // Catch-all: transient-but-otherwise-unclassifiable content. Modest
                // constant baseline so it only wins when nothing else fits well.
                return 0.3f + 0.3f * trapezoid (fv[I::SpectralFlux], 0.2f, 0.4f, 1.0f, 1.0f);
        }
    }

    LayerClassifier::Classification LayerClassifier::classifyRuleBased (const FeatureVector& fv,
                                                                          const std::vector<LayerType>& candidates,
                                                                          const BiasMap* bias) const
    {
        LayerType bestType = LayerType::Unclassified;
        float bestScore = 0.0f;
        for (auto candidate : candidates)
        {
            const float score = scoreFor (candidate, fv) * biasFor (bias, candidate);
            if (score > bestScore) { bestScore = score; bestType = candidate; }
        }

        if (bestScore < minClassifierConfidence)
            return { LayerType::Unclassified, bestScore };

        return { bestType, bestScore };
    }
}
