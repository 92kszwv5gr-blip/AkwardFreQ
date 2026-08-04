#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

namespace afq
{
    // Thin DocumentWindow wrapper so a hosted plugin's editor can be shown in
    // its own top-level window, with a callback for when the user closes it —
    // avoids HostedPluginSlot needing to subclass DocumentWindow itself.
    class PluginWindow : public juce::DocumentWindow
    {
    public:
        PluginWindow (const juce::String& title, std::function<void()> onClosed);
        void closeButtonPressed() override;

    private:
        std::function<void()> onClosed_;
    };
}
