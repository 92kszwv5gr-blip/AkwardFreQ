#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../export/SamplePackExporter.h"
#include "PluginChainPanel.h"
#include "MetadataPanel.h"
#include "PresetBar.h"

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

        // Captures/restores pack name/genre-tag defaults and whether the VST
        // batch-render chain is enabled (not the chain's own contents — that's
        // vstChainPanel_'s separate "VstChain" preset library). Used by
        // presetBar_ and by the top-level Global preset.
        std::unique_ptr<juce::XmlElement> captureXml() const;
        void applyXml (const juce::XmlElement& xml);

        // Pass-through to the embedded batch-render VST chain's own capture/
        // apply — kept separate from captureXml()/applyXml() above so the
        // Global preset can bundle them as distinct, independently-meaningful
        // pieces (same reasoning as MasteringPanel::captureVstChainXml()).
        std::unique_ptr<juce::XmlElement> captureVstChainXml() const { return vstChainPanel_.captureChainXml ("ExportVstChain"); }
        void applyVstChainXml (const juce::XmlElement& xml) { vstChainPanel_.applyChainXml (xml); }

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        AkwardFreQProcessor& processor_;

        PresetBar presetBar_ { "Export", "Export Preset" };

        juce::TextEditor packNameEditor_;
        juce::ComboBox genreCombo_;
        juce::TextButton chooseFolderButton_ { "Choose Destination..." };
        juce::Label destinationLabel_ { {}, "No destination chosen" };
        juce::TextButton exportButton_ { "Export Sample Pack" };
        double progressValue_ = 0.0;
        juce::ProgressBar progressBar_ { progressValue_ };
        juce::Label statusLabel_;
        juce::Label infoLabel_ { {}, "No track analyzed yet" };

        MetadataPanel metadataPanel_;

        juce::ToggleButton useVstChainToggle_ { "Batch-render through VST chain before exporting" };
        PluginChainPanel vstChainPanel_;

        juce::File destinationFolder_;
        std::unique_ptr<juce::FileChooser> fileChooser_;
    };
}
