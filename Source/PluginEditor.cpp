#include "PluginEditor.h"

namespace afq
{
    //==============================================================================
    SplitPanel::SplitPanel (AkwardFreQProcessor& processor) : processor_ (processor)
    {
        addAndMakeVisible (genreCombo_);
        const auto& names = params::genrePresetNames();
        for (int i = 0; i < names.size(); ++i) genreCombo_.addItem (names[i], i + 1);
        genreAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            processor_.apvts, params::genrePresetId, genreCombo_);

        addAndMakeVisible (importButton_);
        importButton_.onClick = [this]
        {
            fileChooser_ = std::make_unique<juce::FileChooser> (
                "Import a track", juce::File(), "*.wav;*.aiff;*.flac;*.ogg;*.mp3");
            fileChooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    const auto file = fc.getResult();
                    if (file.existsAsFile())
                    {
                        statusLabel_.setText ("Importing & separating...", juce::dontSendNotification);
                        processor_.importTrackAndAnalyze (file);
                    }
                });
        };

        addAndMakeVisible (captureButton_);
        captureButton_.onClick = [this]
        {
            if (! capturing_)
            {
                processor_.startCapture();
                capturing_ = true;
                captureButton_.setButtonText ("Stop Capture && Analyze");
                statusLabel_.setText ("Capturing — play the track through this track in Ableton now.", juce::dontSendNotification);
            }
            else
            {
                processor_.stopCaptureAndAnalyze();
                capturing_ = false;
                captureButton_.setButtonText ("Start Capture");
                statusLabel_.setText ("Separating stems...", juce::dontSendNotification);
            }
        };

        addAndMakeVisible (previewButton_);
        previewButton_.onClick = [this]
        {
            if (! previewing_)
            {
                processor_.startPreview();
                previewing_ = true;
                previewButton_.setButtonText ("Stop Preview");
            }
            else
            {
                processor_.stopPreview();
                previewing_ = false;
                previewButton_.setButtonText ("Preview Mix");
            }
        };

        addAndMakeVisible (statusLabel_);
        statusLabel_.setFont (12.0f);
        statusLabel_.setText ("Import a track, or capture it from Ableton's playback, to begin.", juce::dontSendNotification);

        addAndMakeVisible (progressBar_);

        addAndMakeVisible (waveformView_);
        addAndMakeVisible (regionList_);

        waveformView_.onRegionSelected = [this] (int idx) { regionList_.setSelectedRow (idx); };
        waveformView_.onRegionsChanged = [this] { regionList_.refresh(); };

        regionList_.onRegionSelected = [this] (int idx) { waveformView_.setSelectedRegionIndex (idx); };
        regionList_.onRegionsChanged = [this] { waveformView_.refresh(); };
        regionList_.onPlayRequested = [this] (int idx) { playSingleRegion (idx); };
        regionList_.onSaveCorrectionsRequested = [this]
        {
            processor_.saveCorrectionsForRetraining();
            statusLabel_.setText ("Corrections saved to TrainingData/ for retraining (see tools/retrain_layer_classifier.py).",
                                   juce::dontSendNotification);
        };
    }

    void SplitPanel::setProgressAndStatus (float progress0to1, const juce::String& message)
    {
        progressValue_ = (double) progress0to1;
        statusLabel_.setText (message, juce::dontSendNotification);
        if (progress0to1 >= 1.0f)
        {
            capturing_ = false;
            captureButton_.setButtonText ("Start Capture");
        }
    }

    void SplitPanel::onSeparationResultReady()
    {
        currentResult_ = processor_.getLatestResultForUI();
        refreshFromCurrentResult();
    }

    void SplitPanel::refreshFromCurrentResult()
    {
        if (! currentResult_) return;

        waveformView_.setAudioSource (processor_.getCurrentTrackBufferForDisplay(), processor_.getCurrentTrackSampleRate());
        waveformView_.setRegions (&currentResult_->regions);
        waveformView_.setViewRange (0, waveformView_.getTotalLengthSamples());

        regionList_.setRegions (&currentResult_->regions);
        regionList_.refresh();
        waveformView_.refresh();
    }

    void SplitPanel::playSingleRegion (int regionIndex)
    {
        // v1 doesn't own a standalone audio output device for single-hit preview
        // (only Ableton's transport drives audio out of this plugin) — selecting
        // the region in the waveform is the practical stand-in for now.
        waveformView_.setSelectedRegionIndex (regionIndex);
        regionList_.setSelectedRow (regionIndex);
    }

    void SplitPanel::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff1e1e1e)); }

    void SplitPanel::resized()
    {
        auto area = getLocalBounds().reduced (10);

        auto controlRow = area.removeFromTop (28);
        genreCombo_.setBounds (controlRow.removeFromLeft (200));
        controlRow.removeFromLeft (8);
        importButton_.setBounds (controlRow.removeFromLeft (130));
        controlRow.removeFromLeft (8);
        captureButton_.setBounds (controlRow.removeFromLeft (170));
        controlRow.removeFromLeft (8);
        previewButton_.setBounds (controlRow.removeFromLeft (120));

        area.removeFromTop (8);
        auto statusRow = area.removeFromTop (24);
        progressBar_.setBounds (statusRow.removeFromLeft (160));
        statusRow.removeFromLeft (8);
        statusLabel_.setBounds (statusRow);

        area.removeFromTop (8);
        waveformView_.setBounds (area.removeFromTop (area.getHeight() * 55 / 100));
        area.removeFromTop (8);
        regionList_.setBounds (area);
    }

    //==============================================================================
    AkwardFreQEditor::AkwardFreQEditor (AkwardFreQProcessor& p)
        : juce::AudioProcessorEditor (&p), processor_ (p), splitPanel_ (p), masteringPanel_ (p.apvts)
    {
        setResizable (true, true);
        setResizeLimits (760, 560, 1600, 1200);
        setSize (1040, 720);

        addAndMakeVisible (tabs_);
        tabs_.addTab ("Split", juce::Colour (0xff1e1e1e), &splitPanel_, false);
        tabs_.addTab ("Master", juce::Colour (0xff1e1e1e), &masteringPanel_, false);
        tabs_.addTab ("Export", juce::Colour (0xff1e1e1e), &exportPanel_, false);

        processor_.onSeparationProgress = [this] (float p, juce::String msg)
        {
            splitPanel_.setProgressAndStatus (p, msg);
        };

        processor_.onSeparationComplete = [this]
        {
            splitPanel_.onSeparationResultReady();
            if (auto result = processor_.getLatestResultForUI())
                exportPanel_.setTrackInfo (result->estimatedBpm, result->estimatedKey);
        };

        processor_.onExportProgress = [this] (float p, juce::String msg) { exportPanel_.setProgress (p, msg); };
        processor_.onExportComplete = [this] (bool ok, juce::String msg) { exportPanel_.setComplete (ok, msg); };

        masteringPanel_.onLoadReferenceTrack = [this] (juce::File f)
        {
            processor_.loadReferenceTrackForMastering (f);
            masteringPanel_.setReferenceLabel (f.getFileName());
        };

        exportPanel_.onExportRequested = [this] (SamplePackExporter::ExportSettings settings)
        {
            processor_.exportSamplePack (settings);
        };

        startTimerHz (4);
    }

    AkwardFreQEditor::~AkwardFreQEditor()
    {
        stopTimer();
        processor_.onSeparationProgress = nullptr;
        processor_.onSeparationComplete = nullptr;
        processor_.onExportProgress = nullptr;
        processor_.onExportComplete = nullptr;
    }

    void AkwardFreQEditor::timerCallback()
    {
        masteringPanel_.setMeasuredLoudnessLufs (processor_.getMeasuredLoudnessLufs());
    }

    void AkwardFreQEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff141414)); }

    void AkwardFreQEditor::resized() { tabs_.setBounds (getLocalBounds()); }
}
