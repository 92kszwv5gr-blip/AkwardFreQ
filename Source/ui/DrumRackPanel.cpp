#include "DrumRackPanel.h"

namespace afq
{
    namespace
    {
        constexpr int kRawBusItemId = 1;
        // Only the layers that plausibly contain individual percussive hits.
        const LayerType kSelectableLayers[] = { LayerType::Kick, LayerType::HiHat, LayerType::Percussion,
                                                 LayerType::Breakbeat, LayerType::Bass };
    }

    DrumRackPanel::DrumRackPanel()
    {
        addAndMakeVisible (layerCombo_);
        layerCombo_.addItem ("Whole Drums Bus (unsplit)", kRawBusItemId);
        for (auto t : kSelectableLayers)
            layerCombo_.addItem (layerName (t), (int) t + 2);
        layerCombo_.setSelectedId (kRawBusItemId, juce::dontSendNotification);

        addAndMakeVisible (rangeLabel_);
        rangeLabel_.setFont (12.0f);
        rangeLabel_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

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

            const int selectedId = layerCombo_.getSelectedId();
            const bool useRawBus = (selectedId == kRawBusItemId);
            const LayerType layer = useRawBus ? LayerType::Unclassified : (LayerType) (selectedId - 2);

            DrumRackExporter::Settings settings;
            settings.destinationFolder = destinationFolder_;
            settings.kitName = kitNameEditor_.getText().isNotEmpty() ? kitNameEditor_.getText() : "AkwardFreQ Kit";
            settings.prefix = prefixEditor_.getText();

            if (onExportRequested) onExportRequested (layer, useRawBus, rangeStart_, rangeEnd_, settings);
            statusLabel_.setText ("Chopping and exporting...", juce::dontSendNotification);
        };

        addAndMakeVisible (statusLabel_);
        statusLabel_.setFont (12.0f);
    }

    void DrumRackPanel::setSelectedRange (int64_t startSample, int64_t endSample, bool hasRange)
    {
        rangeStart_ = hasRange ? startSample : 0;
        rangeEnd_ = hasRange ? endSample : -1;
        rangeLabel_.setText (hasRange
            ? juce::String::formatted ("Range: %.2fs selection", (endSample - startSample) / 44100.0)
            : juce::String ("Range: whole track"), juce::dontSendNotification);
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
        auto nameRow = area.removeFromTop (26);
        kitNameEditor_.setBounds (nameRow.removeFromLeft (nameRow.getWidth() * 2 / 3));
        nameRow.removeFromLeft (8);
        prefixEditor_.setBounds (nameRow);

        area.removeFromTop (8);
        auto folderRow = area.removeFromTop (26);
        chooseFolderButton_.setBounds (folderRow.removeFromLeft (180));
        folderRow.removeFromLeft (8);
        destinationLabel_.setBounds (folderRow);

        area.removeFromTop (12);
        exportButton_.setBounds (area.removeFromTop (32));

        area.removeFromTop (10);
        statusLabel_.setBounds (area.removeFromTop (20));
    }
}
