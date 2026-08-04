#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../separation/RegionTypes.h"

namespace afq
{
    // Audio-to-MIDI workflow: pick a range on the Split tab's waveform (this
    // panel just drives that view's range-selection mode, it doesn't own a
    // waveform itself), optionally snap it to a clean N-bar loop, preview the
    // loop to check it sounds smooth, then transcribe it to a MIDI file.
    class MidiPanel : public juce::Component
    {
    public:
        MidiPanel();

        void setSelectedRange (int64_t startSample, int64_t endSample, bool hasRange);
        void setLoopPreviewActive (bool active);
        void setComplete (bool ok, const juce::String& message);

        std::function<void (bool)> onRangeSelectionModeToggled;
        std::function<void (int)> onSnapToLoopRequested; // bars: 2/4/8/16
        std::function<void (bool)> onLoopPreviewToggled;
        std::function<void (LayerType, juce::File)> onGenerateMidiRequested;

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        juce::ToggleButton pickRangeToggle_ { "Pick Range on Waveform" };
        juce::Label rangeLabel_ { {}, "No range selected" };

        juce::TextButton snap2_ { "2 bars" }, snap4_ { "4 bars" }, snap8_ { "8 bars" }, snap16_ { "16 bars" };
        juce::Label snapLabel_ { {}, "Snap to Loop:" };

        juce::TextButton loopPreviewButton_ { "Loop Preview" };

        juce::ComboBox layerCombo_;
        juce::TextButton chooseFileButton_ { "Choose Output .mid..." };
        juce::Label destinationLabel_ { {}, "No output file chosen" };
        juce::TextButton generateButton_ { "Generate MIDI" };
        juce::Label statusLabel_;

        int64_t rangeStart_ = 0, rangeEnd_ = 0;
        bool hasRange_ = false;
        juce::File outputFile_;
        std::unique_ptr<juce::FileChooser> fileChooser_;

        void emitSnap (int bars);
    };
}
