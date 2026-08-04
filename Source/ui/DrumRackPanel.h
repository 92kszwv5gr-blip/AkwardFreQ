#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../PluginProcessor.h"
#include "SliceMarkerView.h"
#include "MetadataPanel.h"

namespace afq
{
    // Chops any stem (or the whole unsplit `drums` bus) into slices and
    // exports them as named .wav files — the ReCycle/SliceX/Renoise-style
    // slicer. Two slicing modes: onset-detected (follows transients — the
    // original "drum chop" behavior) or equal slices (mechanically divides
    // the range into exactly N pieces, N set by a single knob). A live
    // waveform preview (SliceMarkerView) redraws its slice markers as the
    // mode/knob/layer/range change, before you ever click Export — click a
    // marker to select it, then either export the whole batch or send just
    // that one slice to the Instrument tab as a one-shot.
    // Operates over whatever range is currently selected via the Split tab's
    // range-selection mode (shared with the MIDI panel) — or the whole track
    // if no range is set.
    class DrumRackPanel : public juce::Component
    {
    public:
        explicit DrumRackPanel (AkwardFreQProcessor& processor);

        void setSelectedRange (int64_t startSample, int64_t endSample, bool hasRange);
        void setComplete (bool ok, const juce::String& message);
        void setAnalysisInfo (double bpm, const juce::String& key);

        // Called when a new separation result is ready — refreshes which
        // buffer the preview slices from.
        void onSeparationResultReady();

        std::function<void (LayerType, bool /*useRawDrumsBus*/, int64_t, int64_t, DrumRackExporter::Settings)> onExportRequested;

        // Fired when "Send Slice to Instrument" is clicked: (layer,
        // useRawDrumsBus, sliceStart, sliceEnd).
        std::function<void (LayerType, bool, int64_t, int64_t)> onSendSliceToInstrument;

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        AkwardFreQProcessor& processor_;

        juce::ComboBox layerCombo_;
        juce::Label rangeLabel_ { {}, "Range: whole track" };

        juce::TextButton modeOnsetButton_ { "Onset-Detected" };
        juce::TextButton modeEqualButton_ { "Equal Slices" };
        juce::Label sliceCountLabel_ { {}, "Slices" };
        juce::Slider sliceCountKnob_;

        SliceMarkerView sliceView_;
        juce::Label sliceCountReadout_ { {}, "0 slices" };
        juce::TextButton sendToInstrumentButton_ { "Send Selected Slice to Instrument" };

        MetadataPanel metadataPanel_;

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
        std::vector<DrumSlice> previewSlices_;

        void setMode (DrumRackExporter::SliceMode mode);
        void refreshPreview(); // recomputes previewSlices_ + updates sliceView_, live (no export)
        const juce::AudioBuffer<float>* currentSourceBuffer() const;
        bool currentUsesRawBus() const;
        LayerType currentLayer() const;
    };
}
