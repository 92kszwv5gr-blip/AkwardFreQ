#include "MetadataPanel.h"

namespace afq
{
    namespace
    {
        void setupField (juce::Label& label, juce::TextEditor& editor, juce::Component& parent, const juce::String& placeholder)
        {
            label.setFont (11.0f);
            label.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
            parent.addAndMakeVisible (label);

            editor.setTextToShowWhenEmpty (placeholder, juce::Colours::grey);
            parent.addAndMakeVisible (editor);
        }
    }

    MetadataPanel::MetadataPanel()
    {
        setupField (titleLabel_, titleEditor_, *this, "e.g. Forest Stomper");
        setupField (artistLabel_, artistEditor_, *this, "e.g. your artist/producer name");
        setupField (genreLabel_, genreEditor_, *this, "e.g. Forest Psy");
        setupField (commentLabel_, commentEditor_, *this, "optional");

        addAndMakeVisible (infoLabel_);
        infoLabel_.setFont (11.0f);
        infoLabel_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    }

    void MetadataPanel::setAnalysisInfo (double bpm, const juce::String& key)
    {
        bpm_ = bpm;
        key_ = key;
        infoLabel_.setText (juce::String::formatted ("Auto-tagged: %.0f BPM, ", bpm) + key
                             + " (included automatically — not editable here)", juce::dontSendNotification);
    }

    TrackMetadata MetadataPanel::getMetadata() const
    {
        TrackMetadata m;
        m.title = titleEditor_.getText();
        m.artist = artistEditor_.getText();
        m.genre = genreEditor_.getText();
        m.comment = commentEditor_.getText();
        m.bpm = bpm_;
        m.key = key_;
        return m;
    }

    void MetadataPanel::paint (juce::Graphics& g)
    {
        g.setColour (juce::Colour (0xff232323));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
        g.setColour (juce::Colour (0xff32373c));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, 1.0f);
    }

    void MetadataPanel::resized()
    {
        auto area = getLocalBounds().reduced (8);

        auto row1 = area.removeFromTop (40);
        auto titleCol = row1.removeFromLeft (row1.getWidth() / 2);
        titleLabel_.setBounds (titleCol.removeFromTop (14));
        titleEditor_.setBounds (titleCol.removeFromTop (24));
        row1.removeFromLeft (8);
        auto artistCol = row1;
        artistLabel_.setBounds (artistCol.removeFromTop (14));
        artistEditor_.setBounds (artistCol.removeFromTop (24));

        area.removeFromTop (6);
        auto row2 = area.removeFromTop (40);
        auto genreCol = row2.removeFromLeft (row2.getWidth() / 2);
        genreLabel_.setBounds (genreCol.removeFromTop (14));
        genreEditor_.setBounds (genreCol.removeFromTop (24));
        row2.removeFromLeft (8);
        auto commentCol = row2;
        commentLabel_.setBounds (commentCol.removeFromTop (14));
        commentEditor_.setBounds (commentCol.removeFromTop (24));

        area.removeFromTop (6);
        infoLabel_.setBounds (area.removeFromTop (16));
    }
}
