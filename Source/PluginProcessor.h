#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>
#include "Params.h"
#include "separation/SeparationEngine.h"
#include "mastering/MasteringChain.h"
#include "export/SamplePackExporter.h"
#include "export/SfzExporter.h"
#include "export/AbletonPresetWriter.h"
#include "export/DrumRackExporter.h"
#include "midi/AudioToMidiConverter.h"
#include "midi/LoopSnapper.h"

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

        // Loop preview: plays [startSample, endSample) of the currently
        // loaded/captured track (the raw mix, not a separated layer) on
        // repeat, replacing live input — for auditioning a MIDI/loop range's
        // smoothness before committing to it. Independent of startPreview()/
        // stopPreview() above (that one plays the separated layers once).
        void startLoopPreview (int64_t startSample, int64_t endSample);
        void stopLoopPreview();
        bool isLoopPreviewActive() const noexcept { return loopPreviewActive_.load(); }

        // Snaps a rough range to exactly `bars` bars using the current
        // track's estimated BPM (falls back to a clamped, un-snapped range if
        // no separation result / BPM estimate is available yet).
        LoopSnapper::Result snapLoopRange (int64_t roughStart, int64_t roughEnd, int bars) const;

        // Reference / EQ match
        void loadReferenceTrackForMastering (const juce::File& referenceFile);

        // Export
        void exportSamplePack (SamplePackExporter::ExportSettings settings);

        // One-shot instrument export: slices [startSample,endSample) out of
        // the given layer's isolated buffer and writes it as SFZ, and/or (if
        // a Simpler template is present, see Models/Templates/README.md) as
        // an Ableton Simpler preset. Runs on a background thread; results
        // arrive via onInstrumentExportComplete.
        struct OneShotExportRequest
        {
            LayerType sourceLayer = LayerType::SynthLead;
            int64_t startSample = 0;
            int64_t endSample = 0;
            bool writeSfz = true;
            bool writeAbletonSimpler = false;
            SfzExporter::Settings sfzSettings;
            juce::File abletonOutputFile; // only used if writeAbletonSimpler
            int rootKeyOverride = -1;
            int lowKey = 0;
            int highKey = 127;
        };
        void exportOneShotInstrument (OneShotExportRequest request);

        // Drum chop/slice export: slices [rangeStart,rangeEnd) into hits and
        // writes them as named, prefixed .wav files. Set `useRawDrumsBus` to
        // chop the whole Tier A `drums` stem (before Tier B sub-splitting —
        // gets you every hit regardless of kick/hat/perc classification);
        // otherwise `sourceLayer` picks one isolated sub-layer (e.g. just the
        // kicks). Results arrive via onDrumRackExportComplete.
        void exportDrumRackFolder (LayerType sourceLayer, bool useRawDrumsBus, int64_t rangeStart, int64_t rangeEnd,
                                    DrumRackExporter::Settings settings);

        // Audio-to-MIDI: transcribes [startSample,endSample) of the given
        // layer's isolated buffer (pick a monophonic one — SynthLead or Bass —
        // for anything usable; see AudioToMidiConverter's docs on why chordal
        // layers won't transcribe well) and writes a Standard MIDI File.
        // Results arrive via onMidiExportComplete.
        void generateMidiFromRange (LayerType sourceLayer, int64_t startSample, int64_t endSample,
                                     const juce::File& outMidiFile);

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
        using SimpleCompleteCallback = std::function<void (bool, juce::String)>;

        SeparationProgressCallback onSeparationProgress;
        SeparationCompleteCallback onSeparationComplete;
        ExportProgressCallback onExportProgress;
        ExportCompleteCallback onExportComplete;
        SimpleCompleteCallback onInstrumentExportComplete;
        SimpleCompleteCallback onDrumRackExportComplete;
        SimpleCompleteCallback onMidiExportComplete;

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

        // Loop preview transport (independent of the above — loops a raw
        // range of currentTrackBuffer_ rather than mixing separated layers).
        std::atomic<bool> loopPreviewActive_ { false };
        std::atomic<int64_t> loopStartSample_ { 0 };
        std::atomic<int64_t> loopEndSample_ { 0 };
        std::atomic<int64_t> loopPlayheadSample_ { 0 };
        void renderLoopPreview (juce::AudioBuffer<float>& buffer);

        double hostSampleRate_ = 44100.0;

        juce::File getModelsDirectory() const;
        void loadModelsIfNeeded();
        bool modelsLoadAttempted_ = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AkwardFreQProcessor)
    };
}
