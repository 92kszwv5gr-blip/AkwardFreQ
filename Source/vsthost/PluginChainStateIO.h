#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <memory>
#include "PluginChain.h"

namespace afq
{
    class AkwardFreQProcessor;

    // Shared XML (de)serialization for a PluginChain's loaded slots (plugin
    // identity + bypass state + internal plugin state), used both for the
    // per-project state (PluginProcessor::getStateInformation, where it's
    // nested under a wrapper element) and for standalone VST-chain presets
    // (PluginChainPanel, where the returned element IS the whole preset file).
    // Kept in one place because deserializePluginChain's ordered, one-slot-
    // at-a-time restore (see its comment) is easy to get subtly wrong if
    // duplicated.

    // Builds a new element named `tagName` with one <Slot> child per loaded
    // slot in `chain`. Caller owns the result.
    std::unique_ptr<juce::XmlElement> serializePluginChain (PluginChain& chain, const juce::String& tagName);

    // Restores slots from `chainEl` (as produced by serializePluginChain) into
    // `chain`, one at a time — each fully loaded (instantiate -> setState ->
    // setBypassed) before the next starts, so chain order survives even
    // though plugin instantiation is asynchronous and load times vary. Safe
    // to call with chainEl == nullptr (onFinished still fires, immediately).
    // `onFinished`, if given, is called once after the last slot (delivered
    // via juce::MessageManager::callAsync the same way loadVstIntoChain's own
    // completion is) — lets UI that shows the chain live (e.g. a preset
    // dropdown replacing the whole chain) know when it's safe to refresh.
    void deserializePluginChain (AkwardFreQProcessor& processor, const juce::XmlElement* chainEl, PluginChain& chain,
                                  std::function<void()> onFinished = nullptr);
}
