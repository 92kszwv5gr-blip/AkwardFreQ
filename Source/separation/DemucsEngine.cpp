#include "DemucsEngine.h"
#include <onnxruntime_cxx_api.h>
#include <algorithm>
#include <cmath>

namespace afq
{
    struct DemucsEngine::OnnxModel
    {
        Ort::Env env { ORT_LOGGING_LEVEL_WARNING, "AkwardFreQDemucs" };
        std::unique_ptr<Ort::Session> session;
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault);
        std::string inputName, outputName;

        bool load (const juce::File& onnxPath)
        {
            try
            {
                Ort::SessionOptions options;
                options.SetIntraOpNumThreads (juce::SystemStats::getNumCpus());
                options.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);

               #if JUCE_WINDOWS
                session = std::make_unique<Ort::Session> (env, onnxPath.getFullPathName().toWideCharPointer(), options);
               #else
                const auto path = onnxPath.getFullPathName().toStdString();
                session = std::make_unique<Ort::Session> (env, path.c_str(), options);
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
    };

    DemucsEngine::DemucsEngine() = default;
    DemucsEngine::~DemucsEngine() = default;

    bool DemucsEngine::loadModel (const juce::File& onnxPath)
    {
        if (! onnxPath.existsAsFile()) return false;
        auto model = std::make_unique<OnnxModel>();
        if (! model->load (onnxPath)) return false;
        model_ = std::move (model);
        return true;
    }

    bool DemucsEngine::isModelLoaded() const noexcept { return model_ != nullptr; }

    juce::AudioBuffer<float> DemucsEngine::resampleTo (const juce::AudioBuffer<float>& src, double srcRate, double dstRate) const
    {
        if (std::abs (srcRate - dstRate) < 0.5) return src;

        const double ratio = srcRate / dstRate;
        const int dstLength = (int) std::ceil (src.getNumSamples() / ratio);
        juce::AudioBuffer<float> dst (src.getNumChannels(), dstLength);

        for (int ch = 0; ch < src.getNumChannels(); ++ch)
        {
            juce::LagrangeInterpolator interpolator;
            interpolator.reset();
            interpolator.process (ratio, src.getReadPointer (ch), dst.getWritePointer (ch), dstLength);
        }
        return dst;
    }

    DemucsEngine::Output DemucsEngine::separate (const juce::AudioBuffer<float>& mix, double sampleRate,
                                                  const std::function<void (float)>& progressCallback)
    {
        Output out;
        if (model_ == nullptr || ! model_->session)
        {
            jassertfalse; // caller must check isModelLoaded() first
            return out;
        }

        // Force stereo input, resample to the model's native rate.
        juce::AudioBuffer<float> stereoMix (2, mix.getNumSamples());
        if (mix.getNumChannels() >= 2)
        {
            stereoMix.copyFrom (0, 0, mix, 0, 0, mix.getNumSamples());
            stereoMix.copyFrom (1, 0, mix, 1, 0, mix.getNumSamples());
        }
        else
        {
            stereoMix.copyFrom (0, 0, mix, 0, 0, mix.getNumSamples());
            stereoMix.copyFrom (1, 0, mix, 0, 0, mix.getNumSamples());
        }

        const auto modelRateMix = resampleTo (stereoMix, sampleRate, (double) kModelSampleRate);
        const int mixLength = modelRateMix.getNumSamples();

        constexpr int kNumSources = 4; // drums, bass, other, vocals — see header doc
        std::array<juce::AudioBuffer<float>, kNumSources> accum;
        for (auto& b : accum) { b.setSize (2, mixLength); b.clear(); }

        const int hop = kSegmentSamples - kOverlapSamples;
        const int numChunks = std::max (1, (int) std::ceil ((double) std::max (1, mixLength - kOverlapSamples) / hop));

        std::vector<float> chunkWindow ((size_t) kSegmentSamples, 1.0f);
        std::vector<float> inputData ((size_t) (2 * kSegmentSamples), 0.0f);

        const std::array<int64_t, 3> inputShape { 1, 2, kSegmentSamples };

        for (int chunkIdx = 0; chunkIdx < numChunks; ++chunkIdx)
        {
            const int chunkStart = chunkIdx * hop;
            const bool isFirst = (chunkIdx == 0);
            const bool isLast = (chunkIdx == numChunks - 1);

            // Build per-chunk trapezoidal crossfade window: ramp up at the start
            // (unless first chunk), ramp down at the end (unless last chunk).
            for (int i = 0; i < kSegmentSamples; ++i)
            {
                float w = 1.0f;
                if (! isFirst && i < kOverlapSamples) w = (float) i / (float) kOverlapSamples;
                if (! isLast && i >= kSegmentSamples - kOverlapSamples)
                    w = std::min (w, (float) (kSegmentSamples - 1 - i) / (float) kOverlapSamples);
                chunkWindow[(size_t) i] = w;
            }

            std::fill (inputData.begin(), inputData.end(), 0.0f);
            const int validLen = std::min (kSegmentSamples, std::max (0, mixLength - chunkStart));
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* src = modelRateMix.getReadPointer (ch) + chunkStart;
                std::copy (src, src + validLen, inputData.begin() + (size_t) ch * kSegmentSamples);
            }

            auto inputTensor = Ort::Value::CreateTensor<float> (
                model_->memoryInfo, inputData.data(), inputData.size(),
                inputShape.data(), inputShape.size());

            const char* inputNames[]  = { model_->inputName.c_str() };
            const char* outputNames[] = { model_->outputName.c_str() };

            try
            {
                auto outputTensors = model_->session->Run (Ort::RunOptions { nullptr },
                                                             inputNames, &inputTensor, 1,
                                                             outputNames, 1);
                // Expected shape: [1, kNumSources, 2, kSegmentSamples]
                const float* data = outputTensors.front().GetTensorData<float>();
                const size_t sourceStride = 2 * (size_t) kSegmentSamples;

                for (int s = 0; s < kNumSources; ++s)
                {
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const float* srcChan = data + (size_t) s * sourceStride + (size_t) ch * kSegmentSamples;
                        float* dst = accum[(size_t) s].getWritePointer (ch);
                        for (int i = 0; i < validLen; ++i)
                        {
                            const int pos = chunkStart + i;
                            if (pos >= mixLength) break;
                            dst[pos] += srcChan[i] * chunkWindow[(size_t) i];
                        }
                    }
                }
            }
            catch (const Ort::Exception&)
            {
                // Leave this chunk's contribution as silence and continue —
                // better a short glitch in one region than aborting the whole track.
            }

            if (progressCallback)
                progressCallback ((float) (chunkIdx + 1) / (float) numChunks);
        }

        out.drums  = resampleTo (accum[0], (double) kModelSampleRate, sampleRate);
        out.bass   = resampleTo (accum[1], (double) kModelSampleRate, sampleRate);
        out.other  = resampleTo (accum[2], (double) kModelSampleRate, sampleRate);
        out.vocals = resampleTo (accum[3], (double) kModelSampleRate, sampleRate);
        return out;
    }
}
