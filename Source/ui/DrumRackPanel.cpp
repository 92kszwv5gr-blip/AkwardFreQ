#include "DrumRackPanel.h"
#include "../separation/DrumSlicer.h"
#include "../separation/EqualSlicer.h"

namespace afq
{
    namespace
    {
        constexpr int kRawBusItemId = 1;
        // Any stem is a plausible slicing target now, not just drum layers.
        const LayerType kSelectableLayers[] = {
            LayerType::Kick, LayerType::HiHat, LayerType::Percussion, LayerType::Breakbeat,
            LayerType::Bass, LayerType::SynthLead, LayerType::Stabs, LayerType::Atmosphere,
            LayerType::FX, LayerType::Zap, LayerType::Glitch
        };
    }

    DrumRackPanel::DrumRackPanel (AkwardFreQProcessor& processor) : processor_ (processor)
    {
        addAndMakeVisible (layerCombo_);
        layerCombo_.addItem ("Whole Drums Bus (unsplit)", kRawBusItemId);
        for (auto t : kSelectableLayers)
            layerCombo_.addItem (layerName (t), (int) t + 2);
        layerCombo_.setSelectedId (kRawBusItemId, juce::dontSendNotification);
        layerCombo_.onChange = [this] { refreshPreview(); };

        addAndMakeVisible (rangeLabel_);
        rangeLabel_.setFont (12.0f);
        rangeLabel_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

        addAndMakeVisible (modeOnsetButton_);
        addAndMakeVisible (modeEqualButton_);
        modeOnsetButton_.setClickingTogglesState (false);
        modeEqualButton_.setClickingTogglesState (false);
        modeOnsetButton_.onClick = [this] { setMode (DrumRackExporter::SliceMode::OnsetDetected); };
        modeEqualButton_.onClick = [this] { setMode (DrumRackExporter::SliceMode::Equal); };

        addAndMakeVisible (sliceCountLabel_);
        sliceCountLabel_.setFont (11.0f);
        sliceCountLabel_.setJustificationType (juce::Justification::centred);

        addAndMakeVisible (sliceCountKnob_);
        sliceCountKnob_.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        sliceCountKnob_.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 18);
        sliceCountKnob_.setRange (1.0, 64.0, 1.0);
        sliceCountKnob_.setValue (16.0, juce::dontSendNotification);
        sliceCountKnob_.setDoubleClickReturnValue (true, 16.0);
        sliceCountKnob_.onValueChange = [this] { refreshPreview(); };

        setMode (DrumRackExporter::SliceMode::OnsetDetected); // sets initial button/knob visibility

        addAndMakeVisible (sliceView_);
        sliceView_.onSliceSelected = [this] (int index)
        {
            sendToInstrumentButton_.setEnabled (index >= 0);
        };

        addAndMakeVisible (sliceCountReadout_);
        sliceCountReadout_.setFont (11.0f);
        sliceCountReadout_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

        addAndMakeVisible (sendToInstrumentButton_);
        sendToInstrumentButton_.setEnabled (false);
        sendToInstrumentButton_.onClick = [this]
        {
            const int idx = sliceView_.getSelectedSliceIndex();
            if (idx < 0 || idx >= (int) previewSlices_.size()) return;
            const auto& s = previewSlices_[(size_t) idx];
            if (onSendSliceToInstrument) onSendSliceToInstrument (currentLayer(), currentUsesRawBus(), s.startSample, s.endSample);
        };

        addAndMakeVisible (kitNameEditor_);
        kitNameEditor_.setText ("AkwardFreQ Kit", juce::dontSendNotification);
        kitNameEditor_.setTextToShowWhenEmpty ("Kit name", juce::Colours::grey);

        addAndMakeVisible (prefixEditor_);
        prefixEditor_.setTextToShowWhenEmpty ("Prefix (optional)", juce::Colours::grey);

        addAndMakeVisible (chooseFolderButton_);
        chooseFolderButton_.onClick = [this]
        {
            fileChooser_ = std::make_unique<juce::FileChooser> ("Choose a destination folder", juce::File());
            fileChooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                [this] (const juce::FileChooser& fc)
                {
                    const auto folder = fc.getResult();
                    if (folder != juce::File())
                    {
                        destinationFolder_ = folder;
                        destinationLabel_.setText (folder.getFullPathName(), juce::dontSendNotification);
                    }
                });
        };
        addAndMakeVisible (destinationLabel_);
        destinationLabel_.setFont (12.0f);
        destinationLabel_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

        addAndMakeVisible (exportButton_);
        exportButton_.onClick = [this]
        {
            if (destinationFolder_ == juce::File())
            {
                statusLabel_.setText ("Choose a destination folder first.", juce::dontSendNotification);
                return;
            }

            DrumRackExporter::Settings settings;
            settings.destinationFolder = destinationFolder_;
            settings.kitName = kitNameEditor_.getText().isNotEmpty() ? kitNameEditor_.getText() : "AkwardFreQ Kit";
            settings.prefix = prefixEditor_.getText();
            settings.mode = mode_;
            settings.sliceCount = (int) sliceCountKnob_.getValue();
            settings.metadata = metadataPanel_.getMetadata().toRiffTags();

            if (onExportRequested) onExportRequested (currentLayer(), currentUsesRawBus(), rangeStart_, rangeEnd_, settings);
            statusLabel_.setText ("Chopping and exporting...", juce::dontSendNotification);
        };

        addAndMakeVisible (metadataPanel_);

        addAndMakeVisible (statusLabel_);
        statusLabel_.setFont (12.0f);
    }

    void DrumRackPanel::setAnalysisInfo (double bpm, const juce::String& key)
    {
        metadataPanel_.setAnalysisInfo (bpm, key);
    }

    bool DrumRackPanel::currentUsesRawBus() const { return layerCombo_.getSelectedId() == kRawBusItemId; }

    LayerType DrumRackPanel::currentLayer() const
    {
        return currentUsesRawBus() ? LayerType::Unclassified : (LayerType) (layerCombo_.getSelectedId() - 2);
    }

    const juce::AudioBuffer<float>* DrumRackPanel::currentSourceBuffer() const
    {
        auto result = processor_.getLatestResultForUI();
        if (! result) return nullptr;
        return currentUsesRawBus() ? &result->drumsBuffer : &result->layerBuffers[(size_t) currentLayer()];
    }

    void DrumRackPanel::setMode (DrumRackExporter::SliceMode mode)
    {
        mode_ = mode;
        const bool equal = (mode == DrumRackExporter::SliceMode::Equal);

        modeOnsetButton_.setToggleState (! equal, juce::dontSendNotification);
        modeEqualButton_.setToggleState (equal, juce::dontSendNotification);
        modeOnsetButton_.setColour (juce::TextButton::buttonColourId, ! equal ? juce::Colour (0xff3f7ba8) : juce::Colour (0xff2b2b2b));
        modeEqualButton_.setColour (juce::TextButton::buttonColourId, equal ? juce::Colour (0xff3f7ba8) : juce::Colour (0xff2b2b2b));

        sliceCountLabel_.setVisible (equal);
        sliceCountKnob_.setVisible (equal);

        refreshPreview();
    }

    void DrumRackPanel::refreshPreview()
    {
        auto result = processor_.getLatestResultForUI();
        const auto* source = currentSourceBuffer();

        if (! result || source == nullptr || source->getNumSamples() == 0)
        {
            previewSlices_.clear();
            sliceView_.setAudioSource (nullptr, 44100.0);
            sliceView_.setSlices ({});
            sliceCountReadout_.setText ("No audio to preview yet — split a track first.", juce::dontSendNotification);
            sendToInstrumentButton_.setEnabled (false);
            return;
        }

        previewSlices_ = (mode_ == DrumRackExporter::SliceMode::Equal)
            ? EqualSlicer::slice (source->getNumSamples(), (int) sliceCountKnob_.getValue(), rangeStart_, rangeEnd_)
            : DrumSlicer::slice (*source, result->sampleRate, rangeStart_, rangeEnd_);

        sliceView_.setAudioSource (source, result->sampleRate);
        sliceView_.setSlices (previewSlices_);
        sliceCountReadout_.setText (juce::String (previewSlices_.size()) + " slice"
                                     + (previewSlices_.size() == 1 ? juce::String() : juce::String ("s")),
                                     juce::dontSendNotification);
        sendToInstrumentButton_.setEnabled (false);
    }

    void DrumRackPanel::onSeparationResultReady() { refreshPreview(); }

    void DrumRackPanel::setSelectedRange (int64_t startSample, int64_t endSample, bool hasRange)
    {
        rangeStart_ = hasRange ? startSample : 0;
        rangeEnd_ = hasRange ? endSample : -1;
        rangeLabel_.setText (hasRange
            ? juce::String::formatted ("Range: %.2fs selection", (endSample - startSample) / 44100.0)
            : juce::String ("Range: whole track"), juce::dontSendNotification);
        refreshPreview();
    }

    void DrumRackPanel::setComplete (bool ok, const juce::String& message)
    {
        statusLabel_.setText (message, juce::dontSendNotification);
        statusLabel_.setColour (juce::Label::textColourId, ok ? juce::Colours::limegreen : juce::Colours::orangered);
    }

    void DrumRackPanel::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff1e1e1e)); }

    void DrumRackPanel::resized()
    {
        auto area = getLocalBounds().reduced (12);

        layerCombo_.setBounds (area.removeFromTop (26));
        area.removeFromTop (6);
        rangeLabel_.setBounds (area.removeFromTop (20));

        area.removeFromTop (8);
        auto modeRow = area.removeFromTop (26);
        modeOnsetButton_.setBounds (modeRow.removeFromLeft (130));
        modeRow.removeFromLeft (6);
        modeEqualButton_.setBounds (modeRow.removeFromLeft (110));
        modeRow.removeFromLeft (10);
        sliceCountLabel_.setBounds (modeRow.removeFromLeft (44).withHeight (14));
        sliceCountKnob_.setBounds (modeRow.removeFromLeft (70).withHeight (60).withY (modeRow.getY() - 16));

        area.removeFromTop (sliceCountKnob_.isVisible() ? 46 : 8);

        sliceView_.setBounds (area.removeFromTop (120));
        area.removeFromTop (4);
        auto previewRow = area.removeFromTop (24);
        sliceCountReadout_.setBounds (previewRow.removeFromLeft (100));
        previewRow.removeFromLeft (8);
        sendToInstrumentButton_.setBounds (previewRow.removeFromLeft (240));

        area.removeFromTop (10);
        auto nameRow = area.removeFromTop (26);
        kitNameEditor_.setBounds (nameRow.removeFromLeft (nameRow.getWidth() * 2 / 3));
        nameRow.removeFromLeft (8);
        prefixEditor_.setBounds (nameRow);

        area.removeFromTop (8);
        auto folderRow = area.removeFromTop (26);
        chooseFolderButton_.setBounds (folderRow.removeFromLeft (180));
        folderRow.removeFromLeft (8);
        destinationLabel_.setBounds (folderRow);

        area.removeFromTop (10);
        metadataPanel_.setBounds (area.removeFromTop (100));

        area.removeFromTop (12);
        exportButton_.setBounds (area.removeFromTop (32));

        area.removeFromTop (10);
        statusLabel_.setBounds (area.removeFromTop (20));
    }
}
