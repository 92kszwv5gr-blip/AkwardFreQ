#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../PluginProcessor.h"
#include "MetadataPanel.h"
#include "PresetBar.h"

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

        void setSelectedRegion (LayerType type, int64_t startSample, int64_t endSample, bool hasSelection,
                                 bool useRawDrumsBus = false);
        void setComplete (bool ok, const juce::String& message);
        void setAnalysisInfo (double bpm, const juce::String& key);

        // Captures/restores the export-format toggles, key-range defaults, and
        // name/prefix defaults (not the current selection, which comes from
        // the Split tab). Used by presetBar_ and by the top-level Global preset.
        std::unique_ptr<juce::XmlElement> captureXml() const;
        void applyXml (const juce::XmlElement& xml);

        std::function<void (AkwardFreQProcessor::OneShotExportRequest)> onExportRequested;

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        PresetBar presetBar_ { "InstrumentExport", "Instrument Preset" };
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
        MetadataPanel metadataPanel_;
        juce::TextButton exportButton_ { "Export One-Shot" };
        juce::Label statusLabel_;

        LayerType selectedType_ = LayerType::SynthLead;
        int64_t selectedStart_ = 0, selectedEnd_ = 0;
        bool hasSelection_ = false;
        bool useRawDrumsBus_ = false;
        juce::File destinationFolder_;
        std::unique_ptr<juce::FileChooser> fileChooser_;
    };
}
