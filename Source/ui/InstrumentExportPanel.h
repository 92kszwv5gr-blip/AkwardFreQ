#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../PluginProcessor.h"

namespace afq
{
    // Exports the currently-selected region (from the Split tab) as a
    // standalone one-shot instrument — SFZ and/or a best-effort Ableton
    // Simpler preset. Selection comes from PluginEditor forwarding whichever
    // region is selected in SplitPanel; this panel doesn't have its own
    // waveform/region picker.
    class InstrumentExportPanel : public juce::Component
    {
    public:
        InstrumentExportPanel();

        void setSelectedRegion (LayerType type, int64_t startSample, int64_t endSample, bool hasSelection);
        void setComplete (bool ok, const juce::String& message);

        std::function<void (AkwardFreQProcessor::OneShotExportRequest)> onExportRequested;

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        juce::Label selectionLabel_ { {}, "No region selected — pick one in the Split tab" };
        juce::TextEditor nameEditor_;
        juce::TextEditor prefixEditor_;
        juce::ToggleButton writeSfzToggle_ { "Export as SFZ" };
        juce::ToggleButton writeAbletonToggle_ { "Export as Ableton Simpler (.adv, best-effort)" };
        juce::TextButton chooseFolderButton_ { "Choose Destination..." };
        juce::Label destinationLabel_ { {}, "No destination chosen" };
        juce::ToggleButton autoRootToggle_ { "Auto-detect root note" };
        juce::Slider rootKeySlider_, lowKeySlider_, highKeySlider_;
        juce::Label rootKeyLabel_ { {}, "Root Key" }, lowKeyLabel_ { {}, "Low Key" }, highKeyLabel_ { {}, "High Key" };
        juce::TextButton exportButton_ { "Export One-Shot" };
        juce::Label statusLabel_;

        LayerType selectedType_ = LayerType::SynthLead;
        int64_t selectedStart_ = 0, selectedEnd_ = 0;
        bool hasSelection_ = false;
        juce::File destinationFolder_;
        std::unique_ptr<juce::FileChooser> fileChooser_;
    };
}
