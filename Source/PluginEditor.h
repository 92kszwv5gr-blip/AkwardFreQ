#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/WaveformRegionView.h"
#include "ui/RegionListPanel.h"
#include "ui/MasteringPanel.h"
#include "ui/ExportPanel.h"
#include "ui/InstrumentExportPanel.h"
#include "ui/DrumRackPanel.h"
#include "ui/MidiPanel.h"
#include "ui/PresetBar.h"

namespace afq
{
    // "Split" tab: capture/import a track, run separation, and correct the
    // resulting region labels via waveform + list views.
    class SplitPanel : public juce::Component
    {
    public:
        explicit SplitPanel (AkwardFreQProcessor& processor);

        void resized() override;
        void paint (juce::Graphics&) override;

        // Called by the editor (which owns the processor callback wiring) when
        // the processor reports separation progress or a completed job.
        void setProgressAndStatus (float progress0to1, const juce::String& message);
        void onSeparationResultReady();

        // Range-selection mode (shared waveform, used by the MIDI/loop and
        // drum-chop panels — see WaveformRegionView::setRangeSelectionMode).
        void setRangeSelectionMode (bool enabled);
        void setSelectedRange (int64_t startSample, int64_t endSample);

        // Fired whenever the region selection or the range selection changes,
        // so the editor can forward the new selection to the other tabs.
        std::function<void (LayerType, int64_t, int64_t, bool)> onRegionSelectionChanged;
        std::function<void (int64_t, int64_t)> onRangeSelectionChanged;

        // Captures/restores just the genre preset choice — used by presetBar_
        // and by the top-level Global preset.
        std::unique_ptr<juce::XmlElement> captureXml() const;
        void applyXml (const juce::XmlElement& xml);

    private:
        AkwardFreQProcessor& processor_;

        juce::ComboBox genreCombo_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> genreAttachment_;
        PresetBar presetBar_ { "StemSplitter", "Splitter Preset" };

        juce::TextButton importButton_ { "Import Track..." };
        juce::TextButton captureButton_ { "Start Capture" };
        juce::TextButton previewButton_ { "Preview Mix" };

        juce::Label statusLabel_;
        double progressValue_ = 0.0;
        juce::ProgressBar progressBar_ { progressValue_ };

        WaveformRegionView waveformView_;
        RegionListPanel regionList_;

        std::unique_ptr<juce::FileChooser> fileChooser_;
        std::shared_ptr<SeparationResult> currentResult_; // keeps the buffers alive for display
        bool capturing_ = false;
        bool previewing_ = false;

        void refreshFromCurrentResult();
        void playSingleRegion (int regionIndex);

        // Owns short-lived preview-of-a-single-region playback via the OS's
        // default audio output would need a separate juce::AudioDeviceManager —
        // out of scope for v1. The list's "play" button instead just selects
        // and scrolls to the region in the waveform; auditioning individual
        // hits happens via the mute/solo/gain layer controls during full
        // "Preview Mix" playback in Ableton.
    };

    class AkwardFreQEditor : public juce::AudioProcessorEditor, private juce::Timer
    {
    public:
        explicit AkwardFreQEditor (AkwardFreQProcessor&);
        ~AkwardFreQEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;

        // Bundles every panel's own captureXml()/applyXml() (plus both VST
        // chains) into one snapshot — "load this whole session's worth of
        // settings at once." Each panel's own preset library (genre, mastering
        // knobs, tag profile, VST chains, etc.) keeps working independently;
        // this is the "save/recall everything together" option layered on top.
        std::unique_ptr<juce::XmlElement> captureGlobalXml() const;
        void applyGlobalXml (const juce::XmlElement& xml);

        AkwardFreQProcessor& processor_;
        PresetBar globalPresetBar_ { "Global", "Global Preset" };
        juce::TabbedComponent tabs_ { juce::TabbedButtonBar::TabsAtTop };

        // Tracks the Split tab's current range selection (shared by the Drum
        // Chop and MIDI tabs) so callbacks that only receive a bar count
        // (snap-to-loop) or a bool (loop preview toggle) still know what
        // range to act on.
        int64_t lastRangeStart_ = 0;
        int64_t lastRangeEnd_ = 0;
        bool hasRange_ = false;

        SplitPanel splitPanel_;
        MasteringPanel masteringPanel_;
        ExportPanel exportPanel_;
        InstrumentExportPanel instrumentPanel_;
        DrumRackPanel drumRackPanel_;
        MidiPanel midiPanel_;
    };
}
