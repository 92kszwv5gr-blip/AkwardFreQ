#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include "../separation/DrumSlicer.h"

namespace afq
{
    // Waveform + slice-marker view for the Drum Chop / Slicer tab — the
    // ReCycle/SliceX/Renoise-style visual feedback: see the slice points on
    // the waveform, click one to select/preview it. Slicing logic itself
    // (onset-detected or equal-N) lives in DrumSlicer/EqualSlicer; this
    // component only draws whatever slice list its owner hands it and
    // reports clicks — recompute and call setSlices() again whenever the
    // mode, slice-count knob, or range changes, so the view always reflects
    // a live preview rather than only updating after "Chop & Export".
    class SliceMarkerView : public juce::Component
    {
    public:
        SliceMarkerView();

        // `displayBuffer` must outlive this component's use of it (owned by
        // the editor, same pattern as WaveformRegionView).
        void setAudioSource (const juce::AudioBuffer<float>* displayBuffer, double sampleRate);
        void setSlices (std::vector<DrumSlice> slices);

        void setSelectedSliceIndex (int index);
        int getSelectedSliceIndex() const noexcept { return selectedIndex_; }

        std::function<void (int)> onSliceSelected;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        const juce::AudioBuffer<float>* buffer_ = nullptr;
        double sampleRate_ = 44100.0;
        std::vector<DrumSlice> slices_;
        int selectedIndex_ = -1;

        float sampleToX (int64_t sample) const;
        int64_t xToSample (float x) const;
        int findSliceAt (int64_t sample) const;
    };
}
