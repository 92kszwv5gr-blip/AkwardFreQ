#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include "PluginWindow.h"

namespace afq
{
    // Wraps one hosted third-party VST3 plugin instance: load, prepare,
    // process (real-time and offline share the same call — the difference is
    // in who's driving the loop), bypass, editor window, state save/restore.
    //
    // Thread rules: load()/prepare()/state get-set/editor management must all
    // happen off the audio thread (message thread or a background loader
    // thread). process() is the only method safe to call from the audio
    // thread, and only after load()+prepare() have both succeeded.
    //
    // There is no crash isolation here — a misbehaving third-party plugin
    // can take the whole host down. That's inherent to in-process plugin
    // hosting (the same tradeoff every JUCE-based "plugin chainer" makes;
    // true sandboxing needs an out-of-process architecture, out of scope here).
    class HostedPluginSlot
    {
    public:
        HostedPluginSlot();
        ~HostedPluginSlot();

        bool load (juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description,
                   juce::String& errorMessage);
        bool isLoaded() const noexcept { return instance_ != nullptr; }

        const juce::PluginDescription& getDescription() const noexcept { return description_; }
        juce::String getName() const { return description_.name; }

        void prepare (double sampleRate, int blockSize, int numChannels);

        // Real-time safe once loaded+prepared and not bypassed; a silent
        // pass-through otherwise.
        void process (juce::AudioBuffer<float>& buffer);

        void setBypassed (bool shouldBypass) noexcept { bypassed_.store (shouldBypass); }
        bool isBypassed() const noexcept { return bypassed_.load(); }

        double getTailLengthSeconds() const;

        // Editor window — message thread only.
        bool hasEditor() const;
        void showEditorWindow (const juce::String& titlePrefix);
        void closeEditorWindow();
        bool isEditorWindowOpen() const noexcept { return editorWindow_ != nullptr; }

        // State persistence — message thread only.
        juce::MemoryBlock getState() const;
        void setState (const juce::MemoryBlock& state);

    private:
        juce::PluginDescription description_;
        std::unique_ptr<juce::AudioPluginInstance> instance_; // declared before editorWindow_: see ~HostedPluginSlot
        std::unique_ptr<PluginWindow> editorWindow_;           // destroyed first (reverse declaration order)

        std::atomic<bool> bypassed_ { false };
        bool prepared_ = false;

        JUCE_DECLARE_NON_COPYABLE (HostedPluginSlot)
    };
}
