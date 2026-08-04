#include "PresetBar.h"

namespace afq
{
    PresetBar::PresetBar (const juce::String& categoryFolderName, const juce::String& labelText)
        : manager_ (categoryFolderName)
    {
        label_.setText (labelText, juce::dontSendNotification);
        label_.setFont (11.0f);
        label_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (label_);

        addAndMakeVisible (combo_);
        combo_.onChange = [this]
        {
            const auto name = selectedName();
            if (name.isEmpty() || ! onApplyState) return;
            if (auto xml = manager_.loadPreset (name))
                onApplyState (*xml);
        };

        addAndMakeVisible (saveButton_);
        saveButton_.onClick = [this] { promptAndSave(); };

        addAndMakeVisible (defaultButton_);
        defaultButton_.onClick = [this]
        {
            const auto name = selectedName();
            if (name.isEmpty()) return;
            manager_.setDefaultPreset (name);
            refresh();
        };

        addAndMakeVisible (deleteButton_);
        deleteButton_.onClick = [this]
        {
            const auto name = selectedName();
            if (name.isEmpty()) return;
            manager_.deletePreset (name);
            refresh();
        };

        refresh();
    }

    void PresetBar::refresh()
    {
        currentNames_ = manager_.getPresetNames();
        const auto defaultName = manager_.getDefaultPresetName();
        const auto previouslySelected = selectedName();

        combo_.clear (juce::dontSendNotification);
        for (int i = 0; i < currentNames_.size(); ++i)
        {
            const auto& n = currentNames_.getReference (i);
            combo_.addItem (n == defaultName ? (n + "  (default)") : n, i + 1);
        }
        combo_.setTextWhenNothingSelected (currentNames_.isEmpty() ? "No presets saved yet" : "Choose a preset...");

        if (previouslySelected.isNotEmpty())
            selectByName (previouslySelected);
    }

    juce::String PresetBar::selectedName() const
    {
        const int idx = combo_.getSelectedItemIndex();
        if (idx < 0 || idx >= currentNames_.size()) return {};
        return currentNames_[idx];
    }

    void PresetBar::selectByName (const juce::String& name)
    {
        const int idx = currentNames_.indexOf (name);
        if (idx >= 0) combo_.setSelectedItemIndex (idx, juce::dontSendNotification);
    }

    void PresetBar::loadDefaultIfPresent()
    {
        const auto defaultName = manager_.getDefaultPresetName();
        if (defaultName.isEmpty()) return;

        if (auto xml = manager_.loadPreset (defaultName))
        {
            selectByName (defaultName);
            if (onApplyState) onApplyState (*xml);
        }
    }

    void PresetBar::promptAndSave()
    {
        auto* aw = new juce::AlertWindow ("Save Preset", "Preset name:", juce::AlertWindow::NoIcon);
        aw->addTextEditor ("name", selectedName(), {});
        aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result)
        {
            if (result != 1 || ! onCaptureState) return;

            const auto name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            if (auto xml = onCaptureState())
            {
                manager_.savePreset (name, *xml);
                refresh();
                selectByName (name);
            }
        }), true);
    }

    void PresetBar::paint (juce::Graphics&) {}

    void PresetBar::resized()
    {
        auto area = getLocalBounds();
        label_.setBounds (area.removeFromLeft (90));
        area.removeFromLeft (4);
        deleteButton_.setBounds (area.removeFromRight (60));
        area.removeFromRight (4);
        defaultButton_.setBounds (area.removeFromRight (86));
        area.removeFromRight (4);
        saveButton_.setBounds (area.removeFromRight (76));
        area.removeFromRight (4);
        combo_.setBounds (area);
    }
}
