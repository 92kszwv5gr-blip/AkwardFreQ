#include "PluginWindow.h"

namespace afq
{
    PluginWindow::PluginWindow (const juce::String& title, std::function<void()> onClosed)
        : juce::DocumentWindow (title, juce::Colour (0xff1e1e1e), juce::DocumentWindow::closeButton),
          onClosed_ (std::move (onClosed))
    {
    }

    void PluginWindow::closeButtonPressed()
    {
        if (onClosed_) onClosed_();
    }
}
