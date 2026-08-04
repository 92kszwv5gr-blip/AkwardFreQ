#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "PresetBar.h"

namespace afq
{
    // Reusable "insert chain" UI: scan for installed VST3 plugins, add them
    // to a PluginChain, and manage the resulting list (bypass/edit/reorder/
    // remove). Used both for the Mastering tab's real-time insert chain and
    // the Export tab's offline batch-render chain — same UI, different
    // PluginChain instance and title passed in by the caller.
    //
    // Both usages share one "VstChain" preset category (see presetBar_), so a
    // chain built for one purpose — e.g. a bass-focused processing chain vs.
    // a full-mix mastering chain — can be saved once and recalled into either
    // the Mastering insert slot or the Export batch-render slot.
    class PluginChainPanel : public juce::Component
    {
    public:
        PluginChainPanel (AkwardFreQProcessor& processor, PluginChain& chain, const juce::String& title);

        // Captures/restores the chain's contents (plugin identities + bypass +
        // internal state), independent of presetBar_'s own "VstChain" library —
        // used by the top-level Global preset, which needs to tell "the
        // Mastering chain" and "the Export chain" apart when both are bundled
        // as siblings, hence the caller-supplied tag name (presetBar_ itself
        // doesn't care what tag a standalone preset file uses).
        std::unique_ptr<juce::XmlElement> captureChainXml (const juce::String& tagName = "VstChain") const;
        void applyChainXml (const juce::XmlElement& xml);

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        AkwardFreQProcessor& processor_;
        PluginChain& chain_;

        juce::Label titleLabel_;
        juce::TextButton rescanButton_ { "Rescan Plugins" };
        juce::ComboBox availablePluginsCombo_;
        juce::TextButton addButton_ { "Add" };
        juce::Label statusLabel_;
        PresetBar presetBar_ { "VstChain", "Chain Preset" };

        juce::Component rowsContainer_; // slot rows live here, not directly on `this` —
                                         // lets refreshSlotRows() clear just the rows.
        std::vector<std::unique_ptr<juce::Component>> slotRows_;

        void refreshAvailablePlugins();
        void refreshSlotRows();
    };
}
