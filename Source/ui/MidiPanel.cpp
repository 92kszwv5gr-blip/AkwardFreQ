#include "MidiPanel.h"

namespace afq
{
    namespace
    {
        // Monophonic layers only — see AudioToMidiConverter's docs on why
        // chordal content (stabs, atmospheres) won't transcribe usefully.
        const LayerType kTranscribableLayers[] = { LayerType::SynthLead, LayerType::Bass, LayerType::Kick };
    }

    MidiPanel::MidiPanel()
    {
        addAndMakeVisible (pickRangeToggle_);
        pickRangeToggle_.onClick = [this]
        {
            if (onRangeSelectionModeToggled) onRangeSelectionModeToggled (pickRangeToggle_.getToggleState());
        };

        addAndMakeVisible (rangeLabel_);
        rangeLabel_.setFont (12.0f);
        rangeLabel_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

        addAndMakeVisible (snapLabel_);
        snapLabel_.setFont (12.0f);

        for (auto* b : { &snap2_, &snap4_, &snap8_, &snap16_ })
            addAndMakeVisible (*b);
        snap2_.onClick = [this] { emitSnap (2); };
        snap4_.onClick = [this] { emitSnap (4); };
        snap8_.onClick = [this] { emitSnap (8); };
        snap16_.onClick = [this] { emitSnap (16); };

        addAndMakeVisible (loopPreviewButton_);
        loopPreviewButton_.setClickingTogglesState (true);
        loopPreviewButton_.onClick = [this]
        {
            if (onLoopPreviewToggled) onLoopPreviewToggled (loopPreviewButton_.getToggleState());
        };

        addAndMakeVisible (layerCombo_);
        for (auto t : kTranscribableLayers) layerCombo_.addItem (layerName (t), (int) t + 1);
        layerCombo_.setSelectedId ((int) LayerType::SynthLead + 1, juce::dontSendNotification);

        addAndMakeVisible (chooseFileButton_);
        chooseFileButton_.onClick = [this]
        {
            fileChooser_ = std::make_unique<juce::FileChooser> ("Choose output MIDI file", juce::File(), "*.mid");
            fileChooser_->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                [this] (const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (file != juce::File())
                    {
                        if (! file.hasFileExtension (".mid")) file = file.withFileExtension (".mid");
                        outputFile_ = file;
                        destinationLabel_.setText (file.getFullPathName(), juce::dontSendNotification);
                    }
                });
        };
        addAndMakeVisible (destinationLabel_);
        destinationLabel_.setFont (12.0f);
        destinationLabel_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

        addAndMakeVisible (generateButton_);
        generateButton_.onClick = [this]
        {
            if (! hasRange_)
            {
                statusLabel_.setText ("Pick a range on the waveform first.", juce::dontSendNotification);
                return;
            }
            if (outputFile_ == juce::File())
            {
                statusLabel_.setText ("Choose an output .mid file first.", juce::dontSendNotification);
                return;
            }

            const auto layer = (LayerType) (layerCombo_.getSelectedId() - 1);
            if (onGenerateMidiRequested) onGenerateMidiRequested (layer, outputFile_);
            statusLabel_.setText ("Transcribing...", juce::dontSendNotification);
        };

        addAndMakeVisible (statusLabel_);
        statusLabel_.setFont (12.0f);
    }

    void MidiPanel::emitSnap (int bars)
    {
        if (! hasRange_)
        {
            statusLabel_.setText ("Pick a rough range on the waveform first, then snap it.", juce::dontSendNotification);
            return;
        }
        if (onSnapToLoopRequested) onSnapToLoopRequested (bars);
    }

    void MidiPanel::setSelectedRange (int64_t startSample, int64_t endSample, bool hasRange)
    {
        rangeStart_ = startSample;
        rangeEnd_ = endSample;
        hasRange_ = hasRange;
        rangeLabel_.setText (hasRange
            ? juce::String::formatted ("Range: %.2fs", (endSample - startSample) / 44100.0)
            : juce::String ("No range selected"), juce::dontSendNotification);
    }

    void MidiPanel::setLoopPreviewActive (bool active)
    {
        loopPreviewButton_.setToggleState (active, juce::dontSendNotification);
    }

    void MidiPanel::setComplete (bool ok, const juce::String& message)
    {
        statusLabel_.setText (message, juce::dontSendNotification);
        statusLabel_.setColour (juce::Label::textColourId, ok ? juce::Colours::limegreen : juce::Colours::orangered);
    }

    void MidiPanel::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff1e1e1e)); }

    void MidiPanel::resized()
    {
        auto area = getLocalBounds().reduced (12);

        pickRangeToggle_.setBounds (area.removeFromTop (24));
        area.removeFromTop (4);
        rangeLabel_.setBounds (area.removeFromTop (20));

        area.removeFromTop (10);
        auto snapRow = area.removeFromTop (26);
        snapLabel_.setBounds (snapRow.removeFromLeft (100));
        const int btnW = snapRow.getWidth() / 4;
        snap2_.setBounds (snapRow.removeFromLeft (btnW).reduced (2));
        snap4_.setBounds (snapRow.removeFromLeft (btnW).reduced (2));
        snap8_.setBounds (snapRow.removeFromLeft (btnW).reduced (2));
        snap16_.setBounds (snapRow.reduced (2));

        area.removeFromTop (10);
        loopPreviewButton_.setBounds (area.removeFromTop (28));

        area.removeFromTop (14);
        layerCombo_.setBounds (area.removeFromTop (26));

        area.removeFromTop (8);
        auto fileRow = area.removeFromTop (26);
        chooseFileButton_.setBounds (fileRow.removeFromLeft (180));
        fileRow.removeFromLeft (8);
        destinationLabel_.setBounds (fileRow);

        area.removeFromTop (12);
        generateButton_.setBounds (area.removeFromTop (32));

        area.removeFromTop (10);
        statusLabel_.setBounds (area.removeFromTop (20));
    }
}
