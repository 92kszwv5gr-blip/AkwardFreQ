#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../PluginProcessor.h"

namespace afq
{
    // Chops any stem (or the whole unsplit `drums` bus) into slices and
    // exports them as named .wav files. Two slicing modes: onset-detected
    // (follows transients — the original "drum chop" behavior) or equal
    // slices (mechanically divides the range into exactly N pieces, N set by
    // a single knob — for material with weak transients, or when you just
    // want even N-way chops regardless of what's actually in the audio).
    // Operates over whatever range is currently selected via the Split tab's
    // range-selection mode (shared with the MIDI panel) — or the whole track
    // if no range is set.
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

        juce::TextButton modeOnsetButton_ { "Onset-Detected" };
        juce::TextButton modeEqualButton_ { "Equal Slices" };
        juce::Label sliceCountLabel_ { {}, "Slices" };
        juce::Slider sliceCountKnob_;

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
        DrumRackExporter::SliceMode mode_ = DrumRackExporter::SliceMode::OnsetDetected;

        void setMode (DrumRackExporter::SliceMode mode);
    };
}
