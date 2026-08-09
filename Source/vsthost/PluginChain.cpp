#include "PluginChain.h"

namespace afq
{
    PluginChain::PluginChain() = default;
    PluginChain::~PluginChain() = default;

    std::shared_ptr<PluginChain::SlotVector> PluginChain::getSnapshotPtr() const
    {
        const juce::SpinLock::ScopedLockType lock (lock_);
        return slots_;
    }

    void PluginChain::setSnapshot (SlotVector newSlots)
    {
        auto shared = std::make_shared<SlotVector> (std::move (newSlots));
        const juce::SpinLock::ScopedLockType lock (lock_);
        slots_ = shared;
    }

    std::vector<std::shared_ptr<HostedPluginSlot>> PluginChain::getSlotsCopy() const
    {
        return *getSnapshotPtr();
    }

    void PluginChain::addSlot (std::shared_ptr<HostedPluginSlot> slot)
    {
        if (slot) slot->prepare (lastSampleRate_, lastBlockSize_, lastNumChannels_);

        auto current = getSlotsCopy();
        current.push_back (std::move (slot));
        setSnapshot (std::move (current));
    }

    void PluginChain::removeSlot (int index)
    {
        auto current = getSlotsCopy();
        if (index < 0 || index >= (int) current.size()) return;
        current.erase (current.begin() + index);
        setSnapshot (std::move (current));
    }

    void PluginChain::moveSlot (int fromIndex, int toIndex)
    {
        auto current = getSlotsCopy();
        if (fromIndex < 0 || fromIndex >= (int) current.size()) return;
        if (toIndex < 0 || toIndex >= (int) current.size()) return;
        if (fromIndex == toIndex) return;

        auto item = current[(size_t) fromIndex];
        current.erase (current.begin() + fromIndex);
        current.insert (current.begin() + toIndex, item);
        setSnapshot (std::move (current));
    }

    void PluginChain::clear() { setSnapshot ({}); }

    void PluginChain::setBypassed (int index, bool bypassed)
    {
        auto current = getSlotsCopy();
        if (index < 0 || index >= (int) current.size()) return;
        current[(size_t) index]->setBypassed (bypassed); // HostedPluginSlot's own field is atomic; safe in place
    }

    int PluginChain::getNumSlots() const { return (int) getSlotsCopy().size(); }

    std::shared_ptr<HostedPluginSlot> PluginChain::getSlot (int index) const
    {
        auto current = getSlotsCopy();
        if (index < 0 || index >= (int) current.size()) return nullptr;
        return current[(size_t) index];
    }

    void PluginChain::prepareAll (double sampleRate, int blockSize, int numChannels)
    {
        lastSampleRate_ = sampleRate;
        lastBlockSize_ = blockSize;
        lastNumChannels_ = numChannels;

        auto current = getSlotsCopy();
        for (auto& s : current)
            if (s) s->prepare (sampleRate, blockSize, numChannels);
    }

    void PluginChain::processAudioThread (juce::AudioBuffer<float>& buffer)
    {
        auto snapshot = getSnapshotPtr();
        if (! snapshot) return;
        for (auto& s : *snapshot)
            if (s) s->process (buffer);
    }
}
