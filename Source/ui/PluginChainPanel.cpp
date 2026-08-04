#include "PluginChainPanel.h"

namespace afq
{
    namespace
    {
        // One row: plugin name, bypass toggle, edit/remove/reorder buttons.
        // Holds a shared_ptr to the slot it represents so it keeps working
        // correctly even if the chain's underlying vector is rebuilt around
        // it (PluginChain's mutators build a new vector but reuse existing
        // shared_ptr slots when not the one being touched).
        class PluginSlotRow : public juce::Component
        {
        public:
            PluginSlotRow (std::shared_ptr<HostedPluginSlot> slot, PluginChain& chain, int index,
                            std::function<void()> onChanged)
                : slot_ (std::move (slot)), chain_ (chain), index_ (index), onChanged_ (std::move (onChanged))
            {
                nameLabel_.setText (slot_ ? slot_->getName() : "(failed to load)", juce::dontSendNotification);
                nameLabel_.setFont (12.0f);
                addAndMakeVisible (nameLabel_);

                addAndMakeVisible (bypassToggle_);
                bypassToggle_.setToggleState (slot_ && slot_->isBypassed(), juce::dontSendNotification);
                bypassToggle_.onClick = [this] { if (slot_) slot_->setBypassed (bypassToggle_.getToggleState()); };

                addAndMakeVisible (editButton_);
                editButton_.setEnabled (slot_ && slot_->hasEditor());
                editButton_.onClick = [this] { if (slot_) slot_->showEditorWindow ("AkwardFreQ Insert — "); };

                addAndMakeVisible (upButton_);
                upButton_.onClick = [this] { chain_.moveSlot (index_, index_ - 1); if (onChanged_) onChanged_(); };

                addAndMakeVisible (downButton_);
                downButton_.onClick = [this] { chain_.moveSlot (index_, index_ + 1); if (onChanged_) onChanged_(); };

                addAndMakeVisible (removeButton_);
                removeButton_.onClick = [this] { chain_.removeSlot (index_); if (onChanged_) onChanged_(); };
            }

            void resized() override
            {
                auto area = getLocalBounds().reduced (2);
                upButton_.setBounds (area.removeFromLeft (20));
                downButton_.setBounds (area.removeFromLeft (20));
                area.removeFromLeft (4);
                removeButton_.setBounds (area.removeFromRight (56));
                area.removeFromRight (4);
                editButton_.setBounds (area.removeFromRight (48));
                area.removeFromRight (4);
                bypassToggle_.setBounds (area.removeFromRight (64));
                area.removeFromRight (4);
                nameLabel_.setBounds (area);
            }

        private:
            std::shared_ptr<HostedPluginSlot> slot_;
            PluginChain& chain_;
            int index_;
            std::function<void()> onChanged_;

            juce::Label nameLabel_;
            juce::ToggleButton bypassToggle_ { "Bypass" };
            juce::TextButton editButton_ { "Edit" };
            juce::TextButton upButton_ { juce::CharPointer_UTF8 ("\xe2\x96\xb2") };   // ▲
            juce::TextButton downButton_ { juce::CharPointer_UTF8 ("\xe2\x96\xbc") }; // ▼
            juce::TextButton removeButton_ { "Remove" };
        };
    }

    PluginChainPanel::PluginChainPanel (AkwardFreQProcessor& processor, PluginChain& chain, const juce::String& title)
        : processor_ (processor), chain_ (chain)
    {
        titleLabel_.setText (title, juce::dontSendNotification);
        titleLabel_.setFont (juce::Font (13.0f, juce::Font::bold));
        addAndMakeVisible (titleLabel_);

        addAndMakeVisible (rescanButton_);
        rescanButton_.onClick = [this]
        {
            statusLabel_.setText ("Scanning for VST3 plugins (this can take a while the first time)...",
                                   juce::dontSendNotification);
            processor_.rescanVstPlugins ([this]
            {
                refreshAvailablePlugins();
                statusLabel_.setText ("Scan complete.", juce::dontSendNotification);
            });
        };

        addAndMakeVisible (availablePluginsCombo_);

        addAndMakeVisible (addButton_);
        addButton_.onClick = [this]
        {
            const int selectedIndex = availablePluginsCombo_.getSelectedItemIndex();
            const auto& types = processor_.getPluginScanner().getKnownPlugins().getTypes();
            if (selectedIndex < 0 || selectedIndex >= types.size()) return;

            const auto description = types.getReference (selectedIndex);
            statusLabel_.setText ("Loading " + description.name + "...", juce::dontSendNotification);
            statusLabel_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

            processor_.loadVstIntoChain (chain_, description, [this] (bool ok, juce::String error)
            {
                statusLabel_.setText (ok ? "Loaded." : error, juce::dontSendNotification);
                statusLabel_.setColour (juce::Label::textColourId, ok ? juce::Colours::limegreen : juce::Colours::orangered);
                refreshSlotRows();
            });
        };

        addAndMakeVisible (statusLabel_);
        statusLabel_.setFont (11.0f);
        statusLabel_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        statusLabel_.setText ("No scan yet — click Rescan Plugins.", juce::dontSendNotification);

        addAndMakeVisible (rowsContainer_);

        refreshAvailablePlugins();
        refreshSlotRows();
    }

    void PluginChainPanel::refreshAvailablePlugins()
    {
        availablePluginsCombo_.clear (juce::dontSendNotification);
        const auto& types = processor_.getPluginScanner().getKnownPlugins().getTypes();
        for (int i = 0; i < types.size(); ++i)
            availablePluginsCombo_.addItem (types.getReference (i).name, i + 1);
        if (types.size() > 0)
            availablePluginsCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
    }

    void PluginChainPanel::refreshSlotRows()
    {
        slotRows_.clear();
        rowsContainer_.removeAllChildren();

        const auto slots = chain_.getSlotsCopy();
        for (int i = 0; i < (int) slots.size(); ++i)
        {
            auto row = std::make_unique<PluginSlotRow> (slots[(size_t) i], chain_, i, [this] { refreshSlotRows(); });
            rowsContainer_.addAndMakeVisible (*row);
            slotRows_.push_back (std::move (row));
        }

        resized();
    }

    void PluginChainPanel::paint (juce::Graphics& g)
    {
        g.setColour (juce::Colour (0xff232323));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
        g.setColour (juce::Colour (0xff32373c));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, 1.0f);
    }

    void PluginChainPanel::resized()
    {
        auto area = getLocalBounds().reduced (8);

        titleLabel_.setBounds (area.removeFromTop (18));
        area.removeFromTop (4);

        auto controlRow = area.removeFromTop (24);
        rescanButton_.setBounds (controlRow.removeFromLeft (120));
        controlRow.removeFromLeft (6);
        addButton_.setBounds (controlRow.removeFromRight (50));
        controlRow.removeFromRight (6);
        availablePluginsCombo_.setBounds (controlRow);

        area.removeFromTop (4);
        statusLabel_.setBounds (area.removeFromTop (16));

        area.removeFromTop (4);
        rowsContainer_.setBounds (area);

        auto rowArea = rowsContainer_.getLocalBounds();
        for (auto& row : slotRows_)
        {
            row->setBounds (rowArea.removeFromTop (26));
            rowArea.removeFromTop (2);
        }
    }
}
