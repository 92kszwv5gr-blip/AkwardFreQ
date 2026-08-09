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

        addAndMakeVisible (presetBar_);
        presetBar_.onCaptureState = [this] { return captureXml(); };
        presetBar_.onApplyState = [this] (const juce::XmlElement& xml) { applyXml (xml); };
        presetBar_.loadDefaultIfPresent();

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

        waveformView_.onRegionSelected = [this] (int idx)
        {
            regionList_.setSelectedRow (idx);
            if (! onRegionSelectionChanged) return;
            if (currentResult_ && idx >= 0 && idx < (int) currentResult_->regions.size())
            {
                const auto& r = currentResult_->regions[(size_t) idx];
                onRegionSelectionChanged (r.type, r.startSample, r.endSample, true);
            }
            else
            {
                onRegionSelectionChanged (LayerType::Unclassified, 0, 0, false);
            }
        };
        waveformView_.onRegionsChanged = [this] { regionList_.refresh(); };
        waveformView_.onRangeSelected = [this] (int64_t start, int64_t end)
        {
            if (onRangeSelectionChanged) onRangeSelectionChanged (start, end);
        };

        regionList_.onRegionSelected = [this] (int idx)
        {
            waveformView_.setSelectedRegionIndex (idx);
            if (! onRegionSelectionChanged) return;
            if (currentResult_ && idx >= 0 && idx < (int) currentResult_->regions.size())
            {
                const auto& r = currentResult_->regions[(size_t) idx];
                onRegionSelectionChanged (r.type, r.startSample, r.endSample, true);
            }
        };
        regionList_.onRegionsChanged = [this] { waveformView_.refresh(); };
        regionList_.onPlayRequested = [this] (int idx) { playSingleRegion (idx); };
        regionList_.onSaveCorrectionsRequested = [this]
        {
            processor_.saveCorrectionsForRetraining();
            statusLabel_.setText ("Corrections saved to TrainingData/ for retraining (see tools/retrain_layer_classifier.py).",
                                   juce::dontSendNotification);
        };
    }

    std::unique_ptr<juce::XmlElement> SplitPanel::captureXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("StemSplitterPreset");
        xml->setAttribute ("genrePresetId", genreCombo_.getSelectedId());
        return xml;
    }

    void SplitPanel::applyXml (const juce::XmlElement& xml)
    {
        const int id = xml.getIntAttribute ("genrePresetId", genreCombo_.getSelectedId());
        if (id > 0) genreCombo_.setSelectedId (id, juce::sendNotificationSync);
    }

    void SplitPanel::setRangeSelectionMode (bool enabled) { waveformView_.setRangeSelectionMode (enabled); }
    void SplitPanel::setSelectedRange (int64_t startSample, int64_t endSample)
    {
        waveformView_.setSelectedRange (startSample, endSample);
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

        presetBar_.setBounds (area.removeFromTop (22));
        area.removeFromTop (6);

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
        : juce::AudioProcessorEditor (&p), processor_ (p), splitPanel_ (p), masteringPanel_ (p), exportPanel_ (p),
          drumRackPanel_ (p)
    {
        setResizable (true, true);
        setResizeLimits (760, 560, 1600, 1200);
        setSize (1040, 720);

        addAndMakeVisible (globalPresetBar_);
        globalPresetBar_.onCaptureState = [this] { return captureGlobalXml(); };
        globalPresetBar_.onApplyState = [this] (const juce::XmlElement& xml) { applyGlobalXml (xml); };

        addAndMakeVisible (tabs_);
        tabs_.addTab ("Split", juce::Colour (0xff1e1e1e), &splitPanel_, false);
        tabs_.addTab ("Master", juce::Colour (0xff1e1e1e), &masteringPanel_, false);
        tabs_.addTab ("Export", juce::Colour (0xff1e1e1e), &exportPanel_, false);
        tabs_.addTab ("Instrument", juce::Colour (0xff1e1e1e), &instrumentPanel_, false);
        tabs_.addTab ("Drum Chop", juce::Colour (0xff1e1e1e), &drumRackPanel_, false);
        tabs_.addTab ("MIDI", juce::Colour (0xff1e1e1e), &midiPanel_, false);

        processor_.onSeparationProgress = [this] (float p, juce::String msg)
        {
            splitPanel_.setProgressAndStatus (p, msg);
        };

        processor_.onSeparationComplete = [this]
        {
            splitPanel_.onSeparationResultReady();
            drumRackPanel_.onSeparationResultReady();
            if (auto result = processor_.getLatestResultForUI())
            {
                exportPanel_.setTrackInfo (result->estimatedBpm, result->estimatedKey);
                instrumentPanel_.setAnalysisInfo (result->estimatedBpm, result->estimatedKey);
                drumRackPanel_.setAnalysisInfo (result->estimatedBpm, result->estimatedKey);
            }
        };

        processor_.onExportProgress = [this] (float p, juce::String msg) { exportPanel_.setProgress (p, msg); };
        processor_.onExportComplete = [this] (bool ok, juce::String msg) { exportPanel_.setComplete (ok, msg); };
        processor_.onInstrumentExportComplete = [this] (bool ok, juce::String msg) { instrumentPanel_.setComplete (ok, msg); };
        processor_.onDrumRackExportComplete = [this] (bool ok, juce::String msg) { drumRackPanel_.setComplete (ok, msg); };
        processor_.onMidiExportComplete = [this] (bool ok, juce::String msg) { midiPanel_.setComplete (ok, msg); };

        masteringPanel_.onLoadReferenceTrack = [this] (juce::File f)
        {
            processor_.loadReferenceTrackForMastering (f);
            masteringPanel_.setReferenceLabel (f.getFileName());
        };

        exportPanel_.onExportRequested = [this] (SamplePackExporter::ExportSettings settings)
        {
            processor_.exportSamplePack (settings);
        };

        // Split tab's region selection feeds the Instrument tab (one-shot
        // export always operates on "whatever region is currently selected").
        splitPanel_.onRegionSelectionChanged = [this] (LayerType type, int64_t start, int64_t end, bool hasSelection)
        {
            instrumentPanel_.setSelectedRegion (type, start, end, hasSelection);
        };

        // Split tab's range selection (a separate mode from region correction)
        // feeds both the Drum Chop and MIDI tabs, and is the single source of
        // truth this editor uses for snap-to-loop / loop-preview / MIDI
        // generation, all of which only receive a bar-count or bool from
        // their panel and need to know the "current range" themselves.
        splitPanel_.onRangeSelectionChanged = [this] (int64_t start, int64_t end)
        {
            lastRangeStart_ = start;
            lastRangeEnd_ = end;
            hasRange_ = true;
            drumRackPanel_.setSelectedRange (start, end, true);
            midiPanel_.setSelectedRange (start, end, true);
        };

        instrumentPanel_.onExportRequested = [this] (AkwardFreQProcessor::OneShotExportRequest request)
        {
            processor_.exportOneShotInstrument (request);
        };

        drumRackPanel_.onExportRequested = [this] (LayerType layer, bool useRawBus, int64_t start, int64_t end,
                                                     DrumRackExporter::Settings settings)
        {
            processor_.exportDrumRackFolder (layer, useRawBus, start, end, settings);
        };

        // A slice selected in the Drum Chop preview can be sent straight to
        // the Instrument tab as a one-shot, same as selecting a region on the
        // Split tab — closes the slicer -> one-shot -> instrument loop.
        drumRackPanel_.onSendSliceToInstrument = [this] (LayerType layer, bool useRawBus, int64_t start, int64_t end)
        {
            instrumentPanel_.setSelectedRegion (layer, start, end, true, useRawBus);
            tabs_.setCurrentTabIndex (3); // Instrument tab — see addTab order above
        };

        midiPanel_.onRangeSelectionModeToggled = [this] (bool enabled) { splitPanel_.setRangeSelectionMode (enabled); };

        midiPanel_.onSnapToLoopRequested = [this] (int bars)
        {
            if (! hasRange_) return; // MidiPanel already guards this, belt-and-suspenders here
            const auto snapped = processor_.snapLoopRange (lastRangeStart_, lastRangeEnd_, bars);

            lastRangeStart_ = snapped.startSample;
            lastRangeEnd_ = snapped.endSample;

            splitPanel_.setSelectedRange (snapped.startSample, snapped.endSample);
            drumRackPanel_.setSelectedRange (snapped.startSample, snapped.endSample, true);
            midiPanel_.setSelectedRange (snapped.startSample, snapped.endSample, true);
            midiPanel_.setComplete (snapped.snapped, snapped.snapped
                ? ("Snapped to " + juce::String (bars) + " bars.")
                : "Couldn't snap (no BPM estimate yet, or range too long) — using the range as-is.");
        };

        midiPanel_.onLoopPreviewToggled = [this] (bool enabled)
        {
            if (enabled && hasRange_) processor_.startLoopPreview (lastRangeStart_, lastRangeEnd_);
            else processor_.stopLoopPreview();
            midiPanel_.setLoopPreviewActive (processor_.isLoopPreviewActive());
        };

        midiPanel_.onGenerateMidiRequested = [this] (LayerType layer, juce::File outFile)
        {
            processor_.generateMidiFromRange (layer, lastRangeStart_, lastRangeEnd_, outFile);
        };

        // Every panel has already applied its own category default by this
        // point (each does so in its own constructor, above). A saved Global
        // default is a deliberate "override everything with this whole
        // session's settings" choice, so it's applied last, after all of them.
        globalPresetBar_.loadDefaultIfPresent();

        startTimerHz (4);
    }

    AkwardFreQEditor::~AkwardFreQEditor()
    {
        stopTimer();
        processor_.onSeparationProgress = nullptr;
        processor_.onSeparationComplete = nullptr;
        processor_.onExportProgress = nullptr;
        processor_.onExportComplete = nullptr;
        processor_.onInstrumentExportComplete = nullptr;
        processor_.onDrumRackExportComplete = nullptr;
        processor_.onMidiExportComplete = nullptr;
    }

    void AkwardFreQEditor::timerCallback()
    {
        masteringPanel_.setMeasuredLoudnessLufs (processor_.getMeasuredLoudnessLufs());
    }

    std::unique_ptr<juce::XmlElement> AkwardFreQEditor::captureGlobalXml() const
    {
        auto root = std::make_unique<juce::XmlElement> ("GlobalPreset");
        root->addChildElement (splitPanel_.captureXml().release());
        root->addChildElement (masteringPanel_.captureXml().release());
        root->addChildElement (masteringPanel_.captureVstChainXml().release());
        root->addChildElement (exportPanel_.captureXml().release());
        root->addChildElement (exportPanel_.captureVstChainXml().release());
        root->addChildElement (instrumentPanel_.captureXml().release());
        root->addChildElement (drumRackPanel_.captureXml().release());
        return root;
    }

    void AkwardFreQEditor::applyGlobalXml (const juce::XmlElement& xml)
    {
        if (auto* el = xml.getChildByName ("StemSplitterPreset"))     splitPanel_.applyXml (*el);
        if (auto* el = xml.getChildByName ("MasteringPreset"))        masteringPanel_.applyXml (*el);
        if (auto* el = xml.getChildByName ("MasteringVstChain"))      masteringPanel_.applyVstChainXml (*el);
        if (auto* el = xml.getChildByName ("ExportPreset"))           exportPanel_.applyXml (*el);
        if (auto* el = xml.getChildByName ("ExportVstChain"))         exportPanel_.applyVstChainXml (*el);
        if (auto* el = xml.getChildByName ("InstrumentExportPreset")) instrumentPanel_.applyXml (*el);
        if (auto* el = xml.getChildByName ("DrumChopPreset"))         drumRackPanel_.applyXml (*el);
    }

    void AkwardFreQEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff141414)); }

    void AkwardFreQEditor::resized()
    {
        auto area = getLocalBounds();
        globalPresetBar_.setBounds (area.removeFromTop (26).reduced (8, 2));
        tabs_.setBounds (area);
    }
}
