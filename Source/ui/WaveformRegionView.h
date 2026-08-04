#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../separation/RegionTypes.h"

namespace afq
{
    // Waveform display with colored region overlays for the correction workflow.
    // Click a region to select it (mirrors RegionListPanel's selection); drag a
    // region's left/right edge (within a small handle margin) to adjust its
    // boundary directly. There is no automatic re-classification on resize —
    // the region keeps its label, userCorrected is set to true, and the audio
    // slice used at export time changes accordingly.
    class WaveformRegionView : public juce::Component
    {
    public:
        WaveformRegionView();

        // `displayBuffer` is a mono/stereo mixdown used only for the waveform
        // outline — must outlive this component's use of it (owned by the editor).
        void setAudioSource (const juce::AudioBuffer<float>* displayBuffer, double sampleRate);
        void setRegions (std::vector<Region>* regions);
        void refresh();

        void setViewRange (int64_t startSample, int64_t lengthSamples);
        int64_t getTotalLengthSamples() const noexcept { return totalLengthSamples_; }

        void setSelectedRegionIndex (int index);
        int getSelectedRegionIndex() const noexcept { return selectedIndex_; }

        std::function<void (int)> onRegionSelected;
        std::function<void()> onRegionsChanged;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

    private:
        const juce::AudioBuffer<float>* buffer_ = nullptr;
        double sampleRate_ = 44100.0;
        std::vector<Region>* regions_ = nullptr;

        int64_t viewStart_ = 0;
        int64_t viewLength_ = 0;
        int64_t totalLengthSamples_ = 0;

        int selectedIndex_ = -1;

        enum class DragMode { none, moveStart, moveEnd };
        DragMode dragMode_ = DragMode::none;
        int dragRegionIndex_ = -1;

        float sampleToX (int64_t sample) const;
        int64_t xToSample (float x) const;
        int findRegionAt (int64_t sample) const;
        int findEdgeHandleAt (float x, float pixelTolerance) const; // returns region index if near an edge
        DragMode edgeKindAt (int regionIndex, float x, float pixelTolerance) const;
    };
}
