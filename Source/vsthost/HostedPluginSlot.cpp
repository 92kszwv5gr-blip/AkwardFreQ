#include "HostedPluginSlot.h"

namespace afq
{
    HostedPluginSlot::HostedPluginSlot() = default;

    HostedPluginSlot::~HostedPluginSlot()
    {
        editorWindow_.reset();
        if (instance_ != nullptr && prepared_)
            instance_->releaseResources();
    }

    bool HostedPluginSlot::load (juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description,
                                  juce::String& errorMessage)
    {
        instance_ = formatManager.createPluginInstance (description, 44100.0, 512, errorMessage);
        if (instance_ == nullptr) return false;

        description_ = description;
        return true;
    }

    void HostedPluginSlot::prepare (double sampleRate, int blockSize, int numChannels)
    {
        if (instance_ == nullptr) return;

        instance_->setPlayConfigDetails (numChannels, numChannels, sampleRate, blockSize);
        instance_->prepareToPlay (sampleRate, blockSize);
        prepared_ = true;
    }

    void HostedPluginSlot::process (juce::AudioBuffer<float>& buffer)
    {
        if (instance_ == nullptr || ! prepared_ || bypassed_.load (std::memory_order_relaxed)) return;

        juce::MidiBuffer noMidi; // inserts don't receive MIDI in this host
        instance_->processBlock (buffer, noMidi);
    }

    double HostedPluginSlot::getTailLengthSeconds() const
    {
        return instance_ != nullptr ? instance_->getTailLengthSeconds() : 0.0;
    }

    bool HostedPluginSlot::hasEditor() const
    {
        return instance_ != nullptr && instance_->hasEditor();
    }

    void HostedPluginSlot::showEditorWindow (const juce::String& titlePrefix)
    {
        if (! hasEditor()) return;

        if (editorWindow_ != nullptr)
        {
            editorWindow_->toFront (true);
            return;
        }

        auto* editor = instance_->createEditorIfNeeded();
        if (editor == nullptr) return;

        editorWindow_ = std::make_unique<PluginWindow> (titlePrefix + description_.name,
                                                          [this] { closeEditorWindow(); });
        editorWindow_->setUsingNativeTitleBar (true);
        editorWindow_->setContentOwned (editor, true);
        editorWindow_->setResizable (editor->isResizable(), false);
        editorWindow_->centreWithSize (editor->getWidth(), editor->getHeight());
        editorWindow_->setVisible (true);
        editorWindow_->toFront (true);
    }

    void HostedPluginSlot::closeEditorWindow()
    {
        editorWindow_.reset(); // DocumentWindow::setContentOwned means this also deletes the editor
    }

    juce::MemoryBlock HostedPluginSlot::getState() const
    {
        juce::MemoryBlock block;
        if (instance_ != nullptr) instance_->getStateInformation (block);
        return block;
    }

    void HostedPluginSlot::setState (const juce::MemoryBlock& state)
    {
        if (instance_ != nullptr && state.getSize() > 0)
            instance_->setStateInformation (state.getData(), (int) state.getSize());
    }
}
