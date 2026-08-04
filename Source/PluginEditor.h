#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/WaveformRegionView.h"
#include "ui/RegionListPanel.h"
#include "ui/MasteringPanel.h"
#include "ui/ExportPanel.h"

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

    private:
        AkwardFreQProcessor& processor_;

        juce::ComboBox genreCombo_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> genreAttachment_;

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

        AkwardFreQProcessor& processor_;
        juce::TabbedComponent tabs_ { juce::TabbedButtonBar::TabsAtTop };

        SplitPanel splitPanel_;
        MasteringPanel masteringPanel_;
        ExportPanel exportPanel_;
    };
}
