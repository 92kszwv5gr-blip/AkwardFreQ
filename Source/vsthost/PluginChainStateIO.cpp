#include "PluginChainStateIO.h"
#include "../PluginProcessor.h"

namespace afq
{
    std::unique_ptr<juce::XmlElement> serializePluginChain (PluginChain& chain, const juce::String& tagName)
    {
        auto chainEl = std::make_unique<juce::XmlElement> (tagName);
        for (auto& slot : chain.getSlotsCopy())
        {
            if (! slot || ! slot->isLoaded()) continue;

            auto* slotEl = chainEl->createNewChildElement ("Slot");
            slotEl->setAttribute ("bypassed", slot->isBypassed());
            slotEl->setAttribute ("state", slot->getState().toBase64Encoding());

            std::unique_ptr<juce::XmlElement> descXml (slot->getDescription().createXml());
            if (descXml != nullptr) slotEl->addChildElement (descXml.release());
        }
        return chainEl;
    }

    namespace
    {
        struct PendingSlotRestore
        {
            juce::PluginDescription description;
            juce::MemoryBlock state;
            bool bypassed = false;
        };

        void restoreNextVstSlot (AkwardFreQProcessor& processor, PluginChain& chain,
                                  std::shared_ptr<std::vector<PendingSlotRestore>> pending, size_t index,
                                  std::shared_ptr<std::function<void()>> onFinished)
        {
            if (index >= pending->size())
            {
                if (*onFinished) (*onFinished)();
                return;
            }

            processor.loadVstIntoChain (chain, (*pending)[index].description,
                [&processor, &chain, pending, index, onFinished] (bool ok, juce::String)
                {
                    if (ok)
                    {
                        auto slots = chain.getSlotsCopy();
                        if (! slots.empty())
                        {
                            slots.back()->setState ((*pending)[index].state);
                            slots.back()->setBypassed ((*pending)[index].bypassed);
                        }
                    }
                    restoreNextVstSlot (processor, chain, pending, index + 1, onFinished);
                });
        }
    }

    void deserializePluginChain (AkwardFreQProcessor& processor, const juce::XmlElement* chainEl, PluginChain& chain,
                                  std::function<void()> onFinished)
    {
        auto onFinishedPtr = std::make_shared<std::function<void()>> (std::move (onFinished));

        if (chainEl == nullptr)
        {
            if (*onFinishedPtr) (*onFinishedPtr)();
            return;
        }

        auto pending = std::make_shared<std::vector<PendingSlotRestore>>();
        for (auto* slotEl : chainEl->getChildIterator())
        {
            if (! slotEl->hasTagName ("Slot")) continue;

            auto* descXml = slotEl->getFirstChildElement();
            if (descXml == nullptr) continue;

            PendingSlotRestore item;
            if (! item.description.loadFromXml (*descXml)) continue;

            item.bypassed = slotEl->getBoolAttribute ("bypassed", false);
            item.state.fromBase64Encoding (slotEl->getStringAttribute ("state"));
            pending->push_back (std::move (item));
        }

        restoreNextVstSlot (processor, chain, pending, 0, onFinishedPtr);
    }
}
