#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../separation/RegionTypes.h"

namespace afq
{
    // Region list with a per-row layer-reassignment dropdown — the main
    // correction mechanism (faster than dragging waveform edges when you just
    // need to fix a mislabeled hit, not its timing).
    class RegionListPanel : public juce::Component, private juce::ListBoxModel
    {
    public:
        RegionListPanel();

        void setRegions (std::vector<Region>* regions);
        void refresh();
        void setSelectedRow (int index);

        std::function<void (int)> onRegionSelected;
        std::function<void()> onRegionsChanged;
        std::function<void (int)> onPlayRequested;
        std::function<void()> onSaveCorrectionsRequested;

        void resized() override;

        // juce::ListBoxModel
        int getNumRows() override;
        void paintListBoxItem (int rowNumber, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        juce::Component* refreshComponentForRow (int rowNumber, bool selected, juce::Component* existing) override;

    private:
        juce::ListBox listBox_ { "regions", this };
        juce::TextButton saveCorrectionsButton_ { "Save Corrections for Retraining" };
        juce::Label headerLabel_ { {}, "Detected Regions" };
        std::vector<Region>* regions_ = nullptr;
    };
}
