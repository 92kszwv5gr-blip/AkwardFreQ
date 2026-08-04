#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>
#include "HostedPluginSlot.h"

namespace afq
{
    // Ordered list of hosted plugins with lock-free-for-readers real-time
    // access. Mutations (add/remove/reorder) build a whole new vector and
    // atomically swap in a shared_ptr to it — the audio thread only ever
    // copies that outer shared_ptr (an atomic refcount bump under a brief
    // SpinLock, no heap allocation), never the vector's contents. This is the
    // same producer/consumer pattern SeparationResult uses elsewhere in this
    // plugin (see PluginProcessor::setLatestResult).
    class PluginChain
    {
    public:
        PluginChain();
        ~PluginChain();

        // Message thread only.
        void addSlot (std::shared_ptr<HostedPluginSlot> slot);
        void removeSlot (int index);
        void moveSlot (int fromIndex, int toIndex);
        void setBypassed (int index, bool bypassed);
        void clear(); // drops every slot — e.g. before loading a different saved chain preset over this one

        int getNumSlots() const;
        std::shared_ptr<HostedPluginSlot> getSlot (int index) const;

        // Re-prepares every currently-loaded slot (call after sample rate /
        // block size changes, and once after loading a new slot).
        void prepareAll (double sampleRate, int blockSize, int numChannels);

        // Real-time safe: no allocation, no blocking (SpinLock held only long
        // enough to copy one shared_ptr).
        void processAudioThread (juce::AudioBuffer<float>& buffer);

        // Message/background thread only — copies the slot list by value.
        std::vector<std::shared_ptr<HostedPluginSlot>> getSlotsCopy() const;

    private:
        using SlotVector = std::vector<std::shared_ptr<HostedPluginSlot>>;

        juce::SpinLock lock_;
        std::shared_ptr<SlotVector> slots_ { std::make_shared<SlotVector>() };

        double lastSampleRate_ = 44100.0;
        int lastBlockSize_ = 512;
        int lastNumChannels_ = 2;

        std::shared_ptr<SlotVector> getSnapshotPtr() const;
        void setSnapshot (SlotVector newSlots);
    };
}
