#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../export/SamplePackExporter.h"
#include "PluginChainPanel.h"

namespace afq
{
    class ExportPanel : public juce::Component
    {
    public:
        explicit ExportPanel (AkwardFreQProcessor& processor);

        // Fired on the message thread when the user clicks Export; the editor
        // wires this to kick off SamplePackExporter::exportPack on a background
        // thread (file I/O for potentially hundreds of files, not audio-thread
        // safe and not fast enough for the message thread either).
        std::function<void (SamplePackExporter::ExportSettings)> onExportRequested;

        void setTrackInfo (double bpm, const juce::String& key);
        void setProgress (float progress0to1, const juce::String& message);
        void setComplete (bool success, const juce::String& message);

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        AkwardFreQProcessor& processor_;

        juce::TextEditor packNameEditor_;
        juce::ComboBox genreCombo_;
        juce::TextButton chooseFolderButton_ { "Choose Destination..." };
        juce::Label destinationLabel_ { {}, "No destination chosen" };
        juce::TextButton exportButton_ { "Export Sample Pack" };
        double progressValue_ = 0.0;
        juce::ProgressBar progressBar_ { progressValue_ };
        juce::Label statusLabel_;
        juce::Label infoLabel_ { {}, "No track analyzed yet" };

        juce::ToggleButton useVstChainToggle_ { "Batch-render through VST chain before exporting" };
        PluginChainPanel vstChainPanel_;

        juce::File destinationFolder_;
        std::unique_ptr<juce::FileChooser> fileChooser_;
    };
}
