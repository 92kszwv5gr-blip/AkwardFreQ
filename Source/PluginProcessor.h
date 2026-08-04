#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>
#include "Params.h"
#include "separation/SeparationEngine.h"
#include "mastering/MasteringChain.h"
#include "export/SamplePackExporter.h"

namespace afq
{
    class AkwardFreQProcessor : public juce::AudioProcessor
    {
    public:
        AkwardFreQProcessor();
        ~AkwardFreQProcessor() override;

        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;
        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override { return "AkwardFreQ"; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock& destData) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

        juce::AudioProcessorValueTreeState apvts;

        //==============================================================================
        // Capture / import
        void startCapture();
        void stopCaptureAndAnalyze();
        void importTrackAndAnalyze (const juce::File& audioFile);
        bool isCapturing() const noexcept { return isCapturing_.load(); }

        // Preview transport (auditions the analyzed layers, replacing live input
        // during playback — see PluginProcessor.cpp for why preview isn't synced
        // to the host transport in v1).
        void startPreview();
        void stopPreview();
        bool isPreviewActive() const noexcept { return previewActive_.load(); }

        // Reference / EQ match
        void loadReferenceTrackForMastering (const juce::File& referenceFile);

        // Export
        void exportSamplePack (SamplePackExporter::ExportSettings settings);

        // Training data
        void saveCorrectionsForRetraining();

        // Returns the same shared instance the audio thread uses for preview
        // playback. Safe for the UI to mutate `.regions` in place (correction
        // workflow) concurrently with audio-thread reads of `.layerBuffers` —
        // those are disjoint subobjects, not a data race. Do NOT resize
        // `.regions` or replace `.layerBuffers` from the UI thread.
        std::shared_ptr<SeparationResult> getLatestResultForUI() const;

        using SeparationProgressCallback = std::function<void (float, juce::String)>;
        using SeparationCompleteCallback = std::function<void()>;
        using ExportProgressCallback = std::function<void (float, juce::String)>;
        using ExportCompleteCallback = std::function<void (bool, juce::String)>;

        // All delivered via juce::MessageManager::callAsync from background
        // threads — safe to touch UI from. onMasteringLoudnessMeasured does NOT
        // exist as a callback: the audio thread must never invoke arbitrary UI
        // std::functions. Poll getMeasuredLoudnessLufs() from a juce::Timer
        // instead (see MasteringPanel usage in PluginEditor).
        SeparationProgressCallback onSeparationProgress;
        SeparationCompleteCallback onSeparationComplete;
        ExportProgressCallback onExportProgress;
        ExportCompleteCallback onExportComplete;

        float getMeasuredLoudnessLufs() const { return masteringChain_.getLastMeasuredLoudnessLufs(); }

        const juce::AudioBuffer<float>* getCurrentTrackBufferForDisplay() const { return &currentTrackBuffer_; }
        double getCurrentTrackSampleRate() const { return currentTrackSampleRate_; }

    private:
        SeparationEngine separationEngine_;
        MasteringChain masteringChain_;

        juce::AudioFormatManager formatManager_;

        // Capture state (audio thread writes, message thread reads after stop).
        std::atomic<bool> isCapturing_ { false };
        juce::AudioBuffer<float> captureBuffer_;
        std::atomic<int64_t> captureWritePos_ { 0 };
        static constexpr double kMaxCaptureMinutes = 12.0;

        // The track currently loaded for analysis (either captured or imported),
        // and where it came from (for training-data export provenance).
        juce::AudioBuffer<float> currentTrackBuffer_;
        double currentTrackSampleRate_ = 44100.0;
        juce::File currentTrackFile_;

        // Thread-safe handoff of separation results from the background worker to
        // the audio thread's preview mixer, guarded with a try-lock (audio thread
        // never blocks — a missed grab just skips preview mixing for one block).
        juce::SpinLock resultLock_;
        std::shared_ptr<SeparationResult> latestResult_;
        void setLatestResult (SeparationResult&& result);

        // Preview transport.
        std::atomic<bool> previewActive_ { false };
        std::atomic<int64_t> previewPlayheadSample_ { 0 };
        void renderPreviewMix (juce::AudioBuffer<float>& buffer);

        double hostSampleRate_ = 44100.0;

        juce::File getModelsDirectory() const;
        void loadModelsIfNeeded();
        bool modelsLoadAttempted_ = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AkwardFreQProcessor)
    };
}
