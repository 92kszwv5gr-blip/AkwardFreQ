#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../presets/PresetManager.h"

namespace afq
{
    // Reusable "preset strip": a dropdown of saved presets for one category,
    // plus Save As.../Set Default/Delete. The owning panel supplies how to
    // capture its current settings as XML (onCaptureState) and how to apply a
    // loaded preset back onto itself (onApplyState) — this component only
    // handles the browsing/storage UI, not what's actually inside a preset.
    //
    // "Set Default" is the answer to "stop making me set this up every time":
    // whichever preset is marked default gets applied automatically the next
    // time the owning panel is constructed — call loadDefaultIfPresent() once
    // your panel has finished wiring up onApplyState (typically last in the
    // panel's constructor).
    class PresetBar : public juce::Component
    {
    public:
        PresetBar (const juce::String& categoryFolderName, const juce::String& labelText);

        std::function<std::unique_ptr<juce::XmlElement>()> onCaptureState;
        std::function<void (const juce::XmlElement&)> onApplyState;

        void loadDefaultIfPresent();
        void refresh(); // repopulates the dropdown from disk

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        PresetManager manager_;
        juce::Label label_;
        juce::ComboBox combo_;
        juce::TextButton saveButton_ { "Save As..." };
        juce::TextButton defaultButton_ { "Set Default" };
        juce::TextButton deleteButton_ { "Delete" };

        juce::StringArray currentNames_; // parallel to combo_'s items (1-based ids)

        void promptAndSave();
        void selectByName (const juce::String& name);
        juce::String selectedName() const;
    };
}
