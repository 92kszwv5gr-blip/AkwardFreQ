#include "RegionListPanel.h"

namespace afq
{
    namespace
    {
        juce::String formatTime (int64_t sample, double sampleRate)
        {
            const double seconds = sampleRate > 0.0 ? (double) sample / sampleRate : 0.0;
            const int mins = (int) (seconds / 60.0);
            const double secs = seconds - mins * 60.0;
            return juce::String::formatted ("%d:%05.2f", mins, secs);
        }

        // A "reasonable default" sample rate for time display when the panel
        // doesn't otherwise know it — regions store sample positions, not time,
        // so exact display accuracy only matters for the label, not any logic.
        constexpr double kAssumedDisplaySampleRate = 44100.0;
    }

    class RegionRowComponent : public juce::Component
    {
    public:
        RegionRowComponent()
        {
            addAndMakeVisible (combo_);
            addAndMakeVisible (playButton_);
            addAndMakeVisible (timeLabel_);

            for (int i = 0; i < (int) LayerType::Count - 1; ++i) // exclude Unclassified from picker choices except as current display
                combo_.addItem (layerName ((LayerType) i), i + 1);
            combo_.addItem (layerName (LayerType::Unclassified), (int) LayerType::Count);

            combo_.onChange = [this]
            {
                if (region_ == nullptr) return;
                const int selectedId = combo_.getSelectedId();
                if (selectedId <= 0) return;
                region_->type = (LayerType) (selectedId - 1);
                region_->userCorrected = true;
                if (onChanged) onChanged();
                repaint();
            };

            playButton_.onClick = [this] { if (onPlay) onPlay(); };
            timeLabel_.setFont (11.0f);
            timeLabel_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        }

        void configure (Region* region, int rowIndex)
        {
            region_ = region;
            rowIndex_ = rowIndex;
            if (region_ != nullptr)
            {
                combo_.setSelectedId ((int) region_->type + 1, juce::dontSendNotification);
                const juce::String range = formatTime (region_->startSample, kAssumedDisplaySampleRate)
                                            + " - " + formatTime (region_->endSample, kAssumedDisplaySampleRate);
                juce::String suffix = region_->userCorrected ? "  [corrected]"
                                     : juce::String::formatted ("  (%.0f%%)", region_->confidence * 100.0f);
                timeLabel_.setText (range + suffix, juce::dontSendNotification);
            }
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (2);
            playButton_.setBounds (area.removeFromLeft (28));
            area.removeFromLeft (4);
            combo_.setBounds (area.removeFromLeft (140));
            area.removeFromLeft (6);
            timeLabel_.setBounds (area);
        }

        std::function<void()> onChanged;
        std::function<void()> onPlay;

    private:
        Region* region_ = nullptr;
        int rowIndex_ = -1;
        juce::ComboBox combo_;
        juce::TextButton playButton_ { juce::CharPointer_UTF8 ("\xe2\x96\xb6") }; // ▶
        juce::Label timeLabel_;
    };

    RegionListPanel::RegionListPanel()
    {
        addAndMakeVisible (headerLabel_);
        headerLabel_.setFont (juce::Font (14.0f, juce::Font::bold));

        addAndMakeVisible (listBox_);
        listBox_.setRowHeight (26);

        addAndMakeVisible (saveCorrectionsButton_);
        saveCorrectionsButton_.onClick = [this] { if (onSaveCorrectionsRequested) onSaveCorrectionsRequested(); };
    }

    void RegionListPanel::setRegions (std::vector<Region>* regions) { regions_ = regions; refresh(); }
    void RegionListPanel::refresh() { listBox_.updateContent(); listBox_.repaint(); }
    void RegionListPanel::setSelectedRow (int index) { listBox_.selectRow (index); }

    void RegionListPanel::resized()
    {
        auto area = getLocalBounds();
        headerLabel_.setBounds (area.removeFromTop (22));
        saveCorrectionsButton_.setBounds (area.removeFromBottom (28).reduced (2));
        listBox_.setBounds (area);
    }

    int RegionListPanel::getNumRows() { return regions_ != nullptr ? (int) regions_->size() : 0; }

    void RegionListPanel::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool selected)
    {
        // Rendering is done via refreshComponentForRow's child component instead;
        // this just draws the row background.
        juce::ignoreUnused (rowNumber, width);
        g.fillAll (selected ? juce::Colour (0xff2c3e50) : juce::Colour (0xff232323));
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.drawHorizontalLine (height - 1, 0.0f, (float) width);
    }

    void RegionListPanel::listBoxItemClicked (int row, const juce::MouseEvent&)
    {
        if (onRegionSelected) onRegionSelected (row);
    }

    juce::Component* RegionListPanel::refreshComponentForRow (int rowNumber, bool /*selected*/, juce::Component* existing)
    {
        if (regions_ == nullptr || rowNumber < 0 || rowNumber >= (int) regions_->size())
        {
            delete existing;
            return nullptr;
        }

        auto* row = dynamic_cast<RegionRowComponent*> (existing);
        if (row == nullptr)
        {
            delete existing;
            row = new RegionRowComponent();
        }

        // Color swatch handled by row background paint (kept simple — the combo
        // box itself makes the assigned layer obvious).
        row->configure (&(*regions_)[(size_t) rowNumber], rowNumber);
        row->onChanged = [this] { if (onRegionsChanged) onRegionsChanged(); refresh(); };
        row->onPlay = [this, rowNumber] { if (onPlayRequested) onPlayRequested (rowNumber); };

        return row;
    }
}
