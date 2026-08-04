#pragma once

#include <juce_dsp/juce_dsp.h>
#include <functional>
#include <memory>

namespace afq
{
    // Tier A: pretrained HT-Demucs inference via ONNX Runtime, run in chunks with
    // overlap-add crossfading (Demucs' own recommended inference pattern for long
    // tracks — the model is trained/exported for a fixed ~8s segment length).
    //
    // Inference only. No training happens here or anywhere in the plugin — see
    // tools/export_demucs_onnx.py for how Models/htdemucs.onnx is produced offline.
    //
    // Must be called from a background thread (SeparationEngine's worker thread),
    // never from the audio thread: one full-track separation can take seconds.
    class DemucsEngine
    {
    public:
        DemucsEngine();
        ~DemucsEngine();

        bool loadModel (const juce::File& onnxPath);
        bool isModelLoaded() const noexcept;

        // Output source order matches tools/export_demucs_onnx.py's export order:
        // [drums, bass, other, vocals]. `vocals` is produced but discarded by
        // SeparationEngine for this genre focus (psytrance is near-always
        // instrumental) — kept here so the engine stays genre-agnostic.
        struct Output
        {
            juce::AudioBuffer<float> drums, bass, other, vocals;
        };

        Output separate (const juce::AudioBuffer<float>& mix, double sampleRate,
                          const std::function<void (float)>& progressCallback = nullptr);

    private:
        struct OnnxModel;
        std::unique_ptr<OnnxModel> model_;

        static constexpr int kModelSampleRate = 44100;
        static constexpr int kSegmentSamples = 343980;      // ~7.8s at 44100Hz, htdemucs default segment
        static constexpr int kOverlapSamples = 44100 / 4;   // 250ms crossfade between chunks

        juce::AudioBuffer<float> resampleTo (const juce::AudioBuffer<float>& src, double srcRate, double dstRate) const;
    };
}
