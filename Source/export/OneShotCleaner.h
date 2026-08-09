#pragma once

#include <juce_dsp/juce_dsp.h>

namespace afq
{
    // "Cleans up" an isolated one-shot before it becomes an instrument: trims
    // dead air from the edges (without shaving into the actual attack —
    // pre-roll keeps a little headroom before the detected onset), normalizes
    // peak level, and applies short fades to guarantee click-free boundaries
    // regardless of how precisely the source region's edges were placed.
    //
    // This is deliberately NOT synthesis — nothing about the sound is
    // regenerated or reshaped, it's the same waveform, just trimmed/leveled/
    // faded the way you'd tidy up a sample by hand before dropping it in a
    // sampler.
    class OneShotCleaner
    {
    public:
        struct Settings
        {
            float silenceThresholdDb = -50.0f; // relative to the slice's own peak
            float preRollMs = 2.0f;            // kept before the detected onset
            float postRollMs = 15.0f;          // kept after the last above-threshold sample
            float fadeInMs = 2.0f;
            float fadeOutMs = 8.0f;
            float targetPeakDb = -1.0f;        // normalization target
            float maxGainDb = 24.0f;           // cap on how hard near-silent content gets boosted
        };

        static juce::AudioBuffer<float> clean (const juce::AudioBuffer<float>& source,
                                                 int64_t startSample, int64_t endSample,
                                                 double sampleRate, const Settings& settings);

        // A default argument here (`= {}`) would try to use Settings' default
        // member initializers before they're "complete" (they only become
        // usable once OneShotCleaner itself finishes being defined) — GCC and
        // Clang both reject that. An overload sidesteps it with identical
        // call-site ergonomics.
        static juce::AudioBuffer<float> clean (const juce::AudioBuffer<float>& source,
                                                 int64_t startSample, int64_t endSample, double sampleRate)
        {
            return clean (source, startSample, endSample, sampleRate, Settings{});
        }
    };
}
