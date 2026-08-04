#include "BatchVstRenderer.h"

namespace afq
{
    juce::AudioBuffer<float> BatchVstRenderer::render (const juce::AudioBuffer<float>& source, double sampleRate,
                                                         PluginChain& chain, const Settings& settings)
    {
        auto allSlots = chain.getSlotsCopy();
        std::vector<std::shared_ptr<HostedPluginSlot>> active;
        for (auto& s : allSlots)
            if (s && s->isLoaded() && ! s->isBypassed())
                active.push_back (s);

        if (active.empty()) return source;

        const int numChannels = source.getNumChannels();
        for (auto& s : active) s->prepare (sampleRate, settings.blockSize, numChannels);

        double maxTailSeconds = 0.0;
        for (auto& s : active) maxTailSeconds = juce::jmax (maxTailSeconds, s->getTailLengthSeconds());
        maxTailSeconds = juce::jmin (maxTailSeconds, settings.maxExtraTailSeconds);
        const int tailSamples = (int) (maxTailSeconds * sampleRate);

        const int totalLen = source.getNumSamples() + tailSamples;

        juce::AudioBuffer<float> working (numChannels, totalLen);
        working.clear();
        for (int ch = 0; ch < numChannels; ++ch)
            working.copyFrom (ch, 0, source, ch, 0, source.getNumSamples());

        for (int pos = 0; pos < totalLen; pos += settings.blockSize)
        {
            const int len = juce::jmin (settings.blockSize, totalLen - pos);
            // Non-owning view over `working`'s existing memory — no per-block copy.
            juce::AudioBuffer<float> blockView (working.getArrayOfWritePointers(), numChannels, pos, len);
            for (auto& s : active) s->process (blockView);
        }

        return working;
    }
}
