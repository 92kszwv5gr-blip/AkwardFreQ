#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace afq
{
    // Reusable "insert chain" UI: scan for installed VST3 plugins, add them
    // to a PluginChain, and manage the resulting list (bypass/edit/reorder/
    // remove). Used both for the Mastering tab's real-time insert chain and
    // the Export tab's offline batch-render chain — same UI, different
    // PluginChain instance and title passed in by the caller.
    class PluginChainPanel : public juce::Component
    {
    public:
        PluginChainPanel (AkwardFreQProcessor& processor, PluginChain& chain, const juce::String& title);

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

        juce::Component rowsContainer_; // slot rows live here, not directly on `this` —
                                         // lets refreshSlotRows() clear just the rows.
        std::vector<std::unique_ptr<juce::Component>> slotRows_;

        void refreshAvailablePlugins();
        void refreshSlotRows();
    };
}
