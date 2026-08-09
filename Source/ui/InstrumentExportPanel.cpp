#include "InstrumentExportPanel.h"

namespace afq
{
    namespace
    {
        void setupKeySlider (juce::Slider& s, juce::Label& l, juce::Component& parent, int defaultValue)
        {
            s.setRange (0.0, 127.0, 1.0);
            s.setValue (defaultValue, juce::dontSendNotification);
            s.setSliderStyle (juce::Slider::IncDecButtons);
            s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 20);
            parent.addAndMakeVisible (s);
            l.setFont (12.0f);
            parent.addAndMakeVisible (l);
        }
    }

    InstrumentExportPanel::InstrumentExportPanel()
    {
        addAndMakeVisible (selectionLabel_);
        selectionLabel_.setFont (13.0f);

        addAndMakeVisible (nameEditor_);
        nameEditor_.setText ("AkwardFreQ One-Shot", juce::dontSendNotification);
        nameEditor_.setTextToShowWhenEmpty ("Instrument name", juce::Colours::grey);

        addAndMakeVisible (prefixEditor_);
        prefixEditor_.setTextToShowWhenEmpty ("Prefix (optional)", juce::Colours::grey);

        addAndMakeVisible (writeSfzToggle_);
        writeSfzToggle_.setToggleState (true, juce::dontSendNotification);

        addAndMakeVisible (writeAbletonToggle_);

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

        addAndMakeVisible (autoRootToggle_);
        autoRootToggle_.setToggleState (true, juce::dontSendNotification);
        autoRootToggle_.onClick = [this] { rootKeySlider_.setEnabled (! autoRootToggle_.getToggleState()); };

        setupKeySlider (rootKeySlider_, rootKeyLabel_, *this, 60);
        setupKeySlider (lowKeySlider_, lowKeyLabel_, *this, 0);
        setupKeySlider (highKeySlider_, highKeyLabel_, *this, 127);
        rootKeySlider_.setEnabled (false); // auto-detect is on by default

        addAndMakeVisible (exportButton_);
        exportButton_.onClick = [this]
        {
            if (! hasSelection_)
            {
                statusLabel_.setText ("Select a region in the Split tab first.", juce::dontSendNotification);
                return;
            }
            if (destinationFolder_ == juce::File())
            {
                statusLabel_.setText ("Choose a destination folder first.", juce::dontSendNotification);
                return;
            }
            if (! writeSfzToggle_.getToggleState() && ! writeAbletonToggle_.getToggleState())
            {
                statusLabel_.setText ("Pick at least one export format.", juce::dontSendNotification);
                return;
            }

            AkwardFreQProcessor::OneShotExportRequest request;
            request.sourceLayer = selectedType_;
            request.useRawDrumsBus = useRawDrumsBus_;
            request.startSample = selectedStart_;
            request.endSample = selectedEnd_;
            request.writeSfz = writeSfzToggle_.getToggleState();
            request.writeAbletonSimpler = writeAbletonToggle_.getToggleState();
            request.rootKeyOverride = autoRootToggle_.getToggleState() ? -1 : (int) rootKeySlider_.getValue();
            request.lowKey = (int) lowKeySlider_.getValue();
            request.highKey = (int) highKeySlider_.getValue();

            request.sfzSettings.destinationFolder = destinationFolder_;
            request.sfzSettings.instrumentName = nameEditor_.getText().isNotEmpty() ? nameEditor_.getText() : "AkwardFreQ One-Shot";
            request.sfzSettings.prefix = prefixEditor_.getText();
            request.metadata = metadataPanel_.getMetadata().toRiffTags();

            if (request.writeAbletonSimpler)
            {
                const juce::String safeName = (request.sfzSettings.prefix + request.sfzSettings.instrumentName)
                                                   .retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_- ")
                                                   .replace (" ", "_");
                request.abletonOutputFile = destinationFolder_.getChildFile (safeName + ".adv");
            }

            if (onExportRequested) onExportRequested (request);
            statusLabel_.setText ("Exporting...", juce::dontSendNotification);
        };

        addAndMakeVisible (metadataPanel_);

        addAndMakeVisible (statusLabel_);
        statusLabel_.setFont (12.0f);

        addAndMakeVisible (presetBar_);
        presetBar_.onCaptureState = [this] { return captureXml(); };
        presetBar_.onApplyState = [this] (const juce::XmlElement& xml) { applyXml (xml); };
        presetBar_.loadDefaultIfPresent();
    }

    std::unique_ptr<juce::XmlElement> InstrumentExportPanel::captureXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("InstrumentExportPreset");
        xml->setAttribute ("name", nameEditor_.getText());
        xml->setAttribute ("prefix", prefixEditor_.getText());
        xml->setAttribute ("writeSfz", writeSfzToggle_.getToggleState());
        xml->setAttribute ("writeAbleton", writeAbletonToggle_.getToggleState());
        xml->setAttribute ("autoRoot", autoRootToggle_.getToggleState());
        xml->setAttribute ("rootKey", rootKeySlider_.getValue());
        xml->setAttribute ("lowKey", lowKeySlider_.getValue());
        xml->setAttribute ("highKey", highKeySlider_.getValue());
        return xml;
    }

    void InstrumentExportPanel::applyXml (const juce::XmlElement& xml)
    {
        nameEditor_.setText (xml.getStringAttribute ("name"), juce::dontSendNotification);
        prefixEditor_.setText (xml.getStringAttribute ("prefix"), juce::dontSendNotification);
        writeSfzToggle_.setToggleState (xml.getBoolAttribute ("writeSfz", true), juce::dontSendNotification);
        writeAbletonToggle_.setToggleState (xml.getBoolAttribute ("writeAbleton", false), juce::dontSendNotification);

        const bool autoRoot = xml.getBoolAttribute ("autoRoot", true);
        autoRootToggle_.setToggleState (autoRoot, juce::dontSendNotification);
        rootKeySlider_.setEnabled (! autoRoot);

        rootKeySlider_.setValue (xml.getDoubleAttribute ("rootKey", 60.0), juce::dontSendNotification);
        lowKeySlider_.setValue (xml.getDoubleAttribute ("lowKey", 0.0), juce::dontSendNotification);
        highKeySlider_.setValue (xml.getDoubleAttribute ("highKey", 127.0), juce::dontSendNotification);
    }

    void InstrumentExportPanel::setAnalysisInfo (double bpm, const juce::String& key)
    {
        metadataPanel_.setAnalysisInfo (bpm, key);
    }

    void InstrumentExportPanel::setSelectedRegion (LayerType type, int64_t startSample, int64_t endSample, bool hasSelection,
                                                     bool useRawDrumsBus)
    {
        selectedType_ = type;
        selectedStart_ = startSample;
        selectedEnd_ = endSample;
        hasSelection_ = hasSelection;
        useRawDrumsBus_ = useRawDrumsBus;

        const juce::String sourceLabel = useRawDrumsBus ? juce::String ("Drums Bus slice") : layerName (type);
        selectionLabel_.setText (hasSelection
            ? ("Selected: " + sourceLabel + " (" + juce::String ((endSample - startSample) / 44.1 / 1000.0, 2) + "s)")
            : juce::String ("No region selected — pick one in the Split tab"), juce::dontSendNotification);
    }

    void InstrumentExportPanel::setComplete (bool ok, const juce::String& message)
    {
        statusLabel_.setText (message, juce::dontSendNotification);
        statusLabel_.setColour (juce::Label::textColourId, ok ? juce::Colours::limegreen : juce::Colours::orangered);
    }

    void InstrumentExportPanel::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff1e1e1e)); }

    void InstrumentExportPanel::resized()
    {
        auto area = getLocalBounds().reduced (12);

        presetBar_.setBounds (area.removeFromTop (22));
        area.removeFromTop (6);

        selectionLabel_.setBounds (area.removeFromTop (22));
        area.removeFromTop (8);

        auto nameRow = area.removeFromTop (26);
        nameEditor_.setBounds (nameRow.removeFromLeft (nameRow.getWidth() * 2 / 3));
        nameRow.removeFromLeft (8);
        prefixEditor_.setBounds (nameRow);

        area.removeFromTop (8);
        writeSfzToggle_.setBounds (area.removeFromTop (22));
        writeAbletonToggle_.setBounds (area.removeFromTop (22));

        area.removeFromTop (8);
        auto folderRow = area.removeFromTop (26);
        chooseFolderButton_.setBounds (folderRow.removeFromLeft (180));
        folderRow.removeFromLeft (8);
        destinationLabel_.setBounds (folderRow);

        area.removeFromTop (10);
        autoRootToggle_.setBounds (area.removeFromTop (22));

        auto keyRow = area.removeFromTop (26);
        const int keyColW = keyRow.getWidth() / 3;
        auto layoutKey = [&] (juce::Label& l, juce::Slider& s)
        {
            auto col = keyRow.removeFromLeft (keyColW);
            l.setBounds (col.removeFromLeft (60));
            s.setBounds (col);
        };
        layoutKey (rootKeyLabel_, rootKeySlider_);
        layoutKey (lowKeyLabel_, lowKeySlider_);
        layoutKey (highKeyLabel_, highKeySlider_);

        area.removeFromTop (10);
        metadataPanel_.setBounds (area.removeFromTop (126));

        area.removeFromTop (12);
        exportButton_.setBounds (area.removeFromTop (32));

        area.removeFromTop (10);
        statusLabel_.setBounds (area.removeFromTop (20));
    }
}
