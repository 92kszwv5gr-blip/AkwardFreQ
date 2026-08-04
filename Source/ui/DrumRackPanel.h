#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../PluginProcessor.h"

namespace afq
{
    // Chops a drum layer (or the whole unsplit `drums` bus) into hits and
    // exports them as named .wav files. Operates over whatever range is
    // currently selected via the Split tab's range-selection mode (shared
    // with the MIDI panel) — or the whole track if no range is set.
    class DrumRackPanel : public juce::Component
    {
    public:
        DrumRackPanel();

        void setSelectedRange (int64_t startSample, int64_t endSample, bool hasRange);
        void setComplete (bool ok, const juce::String& message);

        std::function<void (LayerType, bool /*useRawDrumsBus*/, int64_t, int64_t, DrumRackExporter::Settings)> onExportRequested;

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        juce::ComboBox layerCombo_;
        juce::Label rangeLabel_ { {}, "Range: whole track" };
        juce::TextEditor kitNameEditor_;
        juce::TextEditor prefixEditor_;
        juce::TextButton chooseFolderButton_ { "Choose Destination..." };
        juce::Label destinationLabel_ { {}, "No destination chosen" };
        juce::TextButton exportButton_ { "Chop & Export" };
        juce::Label statusLabel_;

        int64_t rangeStart_ = 0;
        int64_t rangeEnd_ = -1;
        juce::File destinationFolder_;
        std::unique_ptr<juce::FileChooser> fileChooser_;
    };
}
