#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace afq
{
    // Exports a single one-shot slice as a .wav + .sfz instrument — SFZ is an
    // open, text-based sampler format (unlike Kontakt's proprietary/encrypted
    // .nki, which cannot legitimately be written) that Kontakt, Decent Sampler,
    // sforzando, and most other samplers can load.
    class SfzExporter
    {
    public:
        struct Settings
        {
            juce::File destinationFolder;
            juce::String instrumentName = "AkwardFreQ One-Shot";
            juce::String prefix;
            int lowKey = 0;             // MIDI note 0-127, key range the sample is stretched/mapped across
            int highKey = 127;
            int rootKeyOverride = -1;   // -1 = auto-detect via pitch tracking; otherwise a fixed MIDI note
        };

        // Writes <destinationFolder>/<prefix><instrumentName>/ containing the
        // sliced .wav and a matching .sfz. Returns true on success; on
        // failure `errorMessage` explains why.
        static bool exportOneShot (const juce::AudioBuffer<float>& sourceBuffer, int64_t startSample, int64_t endSample,
                                    double sampleRate, const Settings& settings, juce::String& errorMessage);
    };
}
