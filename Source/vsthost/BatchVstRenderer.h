#pragma once

#include <juce_dsp/juce_dsp.h>
#include "PluginChain.h"

namespace afq
{
    // Offline block-by-block render of a buffer through an ordered
    // PluginChain — the "batch render with your own VSTs before export"
    // feature. Runs on a background thread (export already does its file I/O
    // there); never call from the audio thread.
    class BatchVstRenderer
    {
    public:
        struct Settings
        {
            // How much extra tail to render past the source's own length, to
            // catch reverb/delay decay — capped by whatever the plugins
            // themselves report via getTailLengthSeconds() (the smaller of
            // the two wins, so a plugin reporting no tail doesn't force 2s of
            // extra silent rendering).
            double maxExtraTailSeconds = 2.0;
            int blockSize = 1024;
        };

        // Returns a copy of `source` unchanged if the chain has no loaded,
        // non-bypassed slots. Re-prepares each active slot for this render's
        // sample rate/block size/channel count before processing — safe to
        // call repeatedly on the same chain across multiple export jobs.
        static juce::AudioBuffer<float> render (const juce::AudioBuffer<float>& source, double sampleRate,
                                                  PluginChain& chain, const Settings& settings);

        // A default argument here (`= {}`) would try to use Settings' default
        // member initializers before they're "complete" (they only become
        // usable once BatchVstRenderer itself finishes being defined) — GCC
        // and Clang both reject that. An overload sidesteps it with identical
        // call-site ergonomics.
        static juce::AudioBuffer<float> render (const juce::AudioBuffer<float>& source, double sampleRate,
                                                  PluginChain& chain)
        {
            return render (source, sampleRate, chain, Settings{});
        }
    };
}
