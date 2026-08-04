#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "export/SamplePackExporter.h"
#include "separation/TrainingDataExporter.h"
#include <thread>

namespace afq
{
    AkwardFreQProcessor::AkwardFreQProcessor()
        : juce::AudioProcessor (BusesProperties()
                                     .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PARAMS", params::createParameterLayout())
    {
        formatManager_.registerBasicFormats(); // WAV/AIFF/FLAC/OGG — no MP3 decoder ships with JUCE, see README
    }

    AkwardFreQProcessor::~AkwardFreQProcessor() = default;

    void AkwardFreQProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        hostSampleRate_ = sampleRate;
        masteringChain_.prepare (sampleRate, juce::jmax (1, getTotalNumOutputChannels()), samplesPerBlock);

        const int64_t captureCapacity = (int64_t) (sampleRate * 60.0 * kMaxCaptureMinutes);
        captureBuffer_.setSize (juce::jmax (1, getTotalNumInputChannels()), (int) captureCapacity, false, true, true);
        captureBuffer_.clear();
        captureWritePos_.store (0);

        loadModelsIfNeeded();
    }

    void AkwardFreQProcessor::releaseResources() {}

    bool AkwardFreQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto mono = juce::AudioChannelSet::mono();
        const auto stereo = juce::AudioChannelSet::stereo();
        const auto out = layouts.getMainOutputChannelSet();
        if (out != mono && out != stereo) return false;
        return layouts.getMainInputChannelSet() == out;
    }

    juce::File AkwardFreQProcessor::getModelsDirectory() const
    {
        return juce::File::getSpecialLocation (juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile ("Models");
    }

    void AkwardFreQProcessor::loadModelsIfNeeded()
    {
        if (modelsLoadAttempted_) return;
        modelsLoadAttempted_ = true;

        const auto modelsDir = getModelsDirectory();
        const auto demucsPath = modelsDir.getChildFile ("htdemucs.onnx");
        const auto userModelPath = modelsDir.getChildFile ("UserTrained").getChildFile ("layer_classifier.onnx");

        const bool ok = separationEngine_.loadModels (demucsPath, userModelPath);
        if (! ok)
            juce::Logger::writeToLog ("AkwardFreQ: could not load " + demucsPath.getFullPathName()
                                       + " — run tools/export_demucs_onnx.py and place the .onnx there. "
                                         "Stem splitting will be unavailable until then.");
    }

    void AkwardFreQProcessor::setLatestResult (SeparationResult&& result)
    {
        auto shared = std::make_shared<SeparationResult> (std::move (result));
        const juce::SpinLock::ScopedLockType lock (resultLock_);
        latestResult_ = shared;
    }

    std::shared_ptr<SeparationResult> AkwardFreQProcessor::getLatestResultForUI() const
    {
        const juce::SpinLock::ScopedLockType lock (resultLock_);
        return latestResult_;
    }

    //==============================================================================
    void AkwardFreQProcessor::startCapture()
    {
        captureBuffer_.clear();
        captureWritePos_.store (0);
        isCapturing_.store (true);
    }

    void AkwardFreQProcessor::stopCaptureAndAnalyze()
    {
        isCapturing_.store (false);
        const int64_t len = captureWritePos_.load();
        if (len <= 0)
        {
            if (onSeparationProgress) onSeparationProgress (1.0f, "No audio captured");
            return;
        }

        currentTrackBuffer_.setSize (captureBuffer_.getNumChannels(), (int) len, false, true, true);
        for (int ch = 0; ch < captureBuffer_.getNumChannels(); ++ch)
            currentTrackBuffer_.copyFrom (ch, 0, captureBuffer_, ch, 0, (int) len);
        currentTrackSampleRate_ = hostSampleRate_;
        currentTrackFile_ = juce::File(); // captured, not from a file

        loadModelsIfNeeded();
        if (! separationEngine_.isDemucsModelLoaded())
        {
            if (onSeparationProgress) onSeparationProgress (1.0f, "Demucs model not found — see README");
            return;
        }

        const auto genre = (params::GenrePreset) (int) apvts.getRawParameterValue (params::genrePresetId)->load();
        separationEngine_.separateAsync (currentTrackBuffer_, currentTrackSampleRate_, genre,
            [this] (SeparationResult result)
            {
                masteringChain_.setCurrentTrackAnalysis (currentTrackBuffer_, currentTrackSampleRate_);
                setLatestResult (std::move (result));
                if (onSeparationComplete) onSeparationComplete();
            },
            [this] (float p, juce::String msg) { if (onSeparationProgress) onSeparationProgress (p, msg); });
    }

    void AkwardFreQProcessor::importTrackAndAnalyze (const juce::File& audioFile)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formatManager_.createReaderFor (audioFile));
        if (reader == nullptr)
        {
            if (onSeparationProgress)
                onSeparationProgress (1.0f, "Unsupported or unreadable file (note: MP3 decode isn't bundled — see README)");
            return;
        }

        currentTrackBuffer_.setSize ((int) reader->numChannels, (int) reader->lengthInSamples, false, true, true);
        reader->read (&currentTrackBuffer_, 0, (int) reader->lengthInSamples, 0, true, true);
        currentTrackSampleRate_ = reader->sampleRate;
        currentTrackFile_ = audioFile;

        loadModelsIfNeeded();
        if (! separationEngine_.isDemucsModelLoaded())
        {
            if (onSeparationProgress) onSeparationProgress (1.0f, "Demucs model not found — see README");
            return;
        }

        const auto genre = (params::GenrePreset) (int) apvts.getRawParameterValue (params::genrePresetId)->load();
        separationEngine_.separateAsync (currentTrackBuffer_, currentTrackSampleRate_, genre,
            [this] (SeparationResult result)
            {
                masteringChain_.setCurrentTrackAnalysis (currentTrackBuffer_, currentTrackSampleRate_);
                setLatestResult (std::move (result));
                if (onSeparationComplete) onSeparationComplete();
            },
            [this] (float p, juce::String msg) { if (onSeparationProgress) onSeparationProgress (p, msg); });
    }

    void AkwardFreQProcessor::startPreview()
    {
        previewPlayheadSample_.store (0);
        previewActive_.store (true);
    }

    void AkwardFreQProcessor::stopPreview() { previewActive_.store (false); }

    void AkwardFreQProcessor::loadReferenceTrackForMastering (const juce::File& referenceFile)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formatManager_.createReaderFor (referenceFile));
        if (reader == nullptr) return;

        juce::AudioBuffer<float> refBuffer ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&refBuffer, 0, (int) reader->lengthInSamples, 0, true, true);

        // Analysis (FFT over the whole file) — fine on the message thread for a
        // one-off reference load, but not on the audio thread.
        masteringChain_.setReferenceTrack (refBuffer, reader->sampleRate);
    }

    void AkwardFreQProcessor::exportSamplePack (SamplePackExporter::ExportSettings settings)
    {
        auto result = getLatestResultForUI();
        if (! result)
        {
            if (onExportComplete) onExportComplete (false, "No separation result yet — split a track first.");
            return;
        }

        std::thread worker ([this, result, settings]() mutable
        {
            juce::String error;
            const bool ok = SamplePackExporter::exportPack (*result, settings, error,
                [this] (float p, juce::String msg)
                {
                    juce::MessageManager::callAsync ([this, p, msg]() { if (onExportProgress) onExportProgress (p, msg); });
                });

            juce::MessageManager::callAsync ([this, ok, error]()
            {
                if (onExportComplete) onExportComplete (ok, ok ? "Export complete." : error);
            });
        });
        worker.detach();
    }

    void AkwardFreQProcessor::saveCorrectionsForRetraining()
    {
        auto result = getLatestResultForUI();
        if (! result) return;

        const auto trainingDir = getModelsDirectory().getParentDirectory().getChildFile ("TrainingData");
        juce::String error;
        TrainingDataExporter::save (*result, currentTrackFile_, trainingDir, error);
    }

    //==============================================================================
    void AkwardFreQProcessor::renderPreviewMix (juce::AudioBuffer<float>& buffer)
    {
        auto result = getLatestResultForUI();
        if (! result) { previewActive_.store (false); return; }

        const int numSamples = buffer.getNumSamples();
        buffer.clear();
        const int64_t pos = previewPlayheadSample_.load();

        int64_t totalLen = 0;
        for (const auto& lb : result->layerBuffers) totalLen = juce::jmax (totalLen, (int64_t) lb.getNumSamples());
        if (totalLen == 0) { previewActive_.store (false); return; }

        bool anySolo = false;
        for (int i = 0; i < (int) LayerType::Count; ++i)
            if (apvts.getRawParameterValue (params::layerSoloId (i))->load() > 0.5f) { anySolo = true; break; }

        for (int layer = 0; layer < (int) LayerType::Count; ++layer)
        {
            const auto& src = result->layerBuffers[(size_t) layer];
            if (src.getNumSamples() == 0) continue;

            const bool muted = apvts.getRawParameterValue (params::layerMuteId (layer))->load() > 0.5f;
            const bool solo = apvts.getRawParameterValue (params::layerSoloId (layer))->load() > 0.5f;
            if (muted || (anySolo && ! solo)) continue;

            const float gain = apvts.getRawParameterValue (params::layerGainId (layer))->load();

            for (int ch = 0; ch < juce::jmin (buffer.getNumChannels(), src.getNumChannels()); ++ch)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    const int64_t s = pos + i;
                    if (s >= 0 && s < src.getNumSamples())
                        buffer.addSample (ch, i, src.getSample (ch, (int) s) * gain);
                }
            }
        }

        int64_t newPos = pos + numSamples;
        if (newPos >= totalLen) { newPos = 0; previewActive_.store (false); }
        previewPlayheadSample_.store (newPos);
    }

    void AkwardFreQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
            buffer.clear (ch, 0, buffer.getNumSamples());

        if (isCapturing_.load())
        {
            const int numSamples = buffer.getNumSamples();
            const int64_t writePos = captureWritePos_.load();
            const int64_t capacity = captureBuffer_.getNumSamples();
            const int64_t remaining = capacity - writePos;
            if (remaining > 0)
            {
                const int toCopy = (int) juce::jmin<int64_t> (numSamples, remaining);
                for (int ch = 0; ch < juce::jmin (buffer.getNumChannels(), captureBuffer_.getNumChannels()); ++ch)
                    captureBuffer_.copyFrom (ch, (int) writePos, buffer, ch, 0, toCopy);
                captureWritePos_.store (writePos + toCopy);
            }
        }

        if (previewActive_.load())
            renderPreviewMix (buffer);

        masteringChain_.processBlock (buffer);
    }

    //==============================================================================
    juce::AudioProcessorEditor* AkwardFreQProcessor::createEditor() { return new AkwardFreQEditor (*this); }

    void AkwardFreQProcessor::getStateInformation (juce::MemoryBlock& destData)
    {
        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }

    void AkwardFreQProcessor::setStateInformation (const void* data, int sizeInBytes)
    {
        std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
        if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new afq::AkwardFreQProcessor();
}
