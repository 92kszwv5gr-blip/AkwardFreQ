#include "ExportPanel.h"
#include "../Params.h"

namespace afq
{
    ExportPanel::ExportPanel()
    {
        addAndMakeVisible (packNameEditor_);
        packNameEditor_.setText ("AkwardFreQ Pack", juce::dontSendNotification);
        packNameEditor_.setTextToShowWhenEmpty ("Pack name", juce::Colours::grey);

        addAndMakeVisible (genreCombo_);
        const auto& names = params::genrePresetNames();
        for (int i = 0; i < names.size(); ++i) genreCombo_.addItem (names[i], i + 1);
        genreCombo_.setSelectedId ((int) params::GenrePreset::PsyTrance + 1, juce::dontSendNotification);

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
            if (! onExportRequested) return;
            if (destinationFolder_ == juce::File())
            {
                statusLabel_.setText ("Choose a destination folder first.", juce::dontSendNotification);
                return;
            }

            SamplePackExporter::ExportSettings settings;
            settings.destinationFolder = destinationFolder_;
            settings.packName = packNameEditor_.getText().isNotEmpty() ? packNameEditor_.getText() : "AkwardFreQ Pack";
            settings.genreTag = genreCombo_.getText();
            onExportRequested (settings);
        };

        addAndMakeVisible (progressBar_);
        addAndMakeVisible (statusLabel_);
        statusLabel_.setFont (12.0f);

        addAndMakeVisible (infoLabel_);
        infoLabel_.setFont (12.0f);
        infoLabel_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    }

    void ExportPanel::setTrackInfo (double bpm, const juce::String& key)
    {
        infoLabel_.setText (juce::String::formatted ("Detected: %.0f BPM, ", bpm) + key, juce::dontSendNotification);
    }

    void ExportPanel::setProgress (float progress0to1, const juce::String& message)
    {
        progressValue_ = (double) progress0to1;
        statusLabel_.setText (message, juce::dontSendNotification);
    }

    void ExportPanel::setComplete (bool success, const juce::String& message)
    {
        progressValue_ = success ? 1.0 : 0.0;
        statusLabel_.setText (message, juce::dontSendNotification);
        statusLabel_.setColour (juce::Label::textColourId, success ? juce::Colours::limegreen : juce::Colours::orangered);
    }

    void ExportPanel::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff1e1e1e)); }

    void ExportPanel::resized()
    {
        auto area = getLocalBounds().reduced (12);

        infoLabel_.setBounds (area.removeFromTop (20));
        area.removeFromTop (8);

        auto nameRow = area.removeFromTop (28);
        packNameEditor_.setBounds (nameRow.removeFromLeft (nameRow.getWidth() * 2 / 3));
        nameRow.removeFromLeft (8);
        genreCombo_.setBounds (nameRow);

        area.removeFromTop (8);
        auto folderRow = area.removeFromTop (28);
        chooseFolderButton_.setBounds (folderRow.removeFromLeft (180));
        folderRow.removeFromLeft (8);
        destinationLabel_.setBounds (folderRow);

        area.removeFromTop (12);
        exportButton_.setBounds (area.removeFromTop (32));

        area.removeFromTop (12);
        progressBar_.setBounds (area.removeFromTop (18));
        area.removeFromTop (4);
        statusLabel_.setBounds (area.removeFromTop (20));
    }
}
