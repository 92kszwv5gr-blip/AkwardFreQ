#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
#include "../separation/RegionTypes.h"

namespace afq
{
    // Slices the tagged regions in a SeparationResult into individual one-shot
    // .wav files, organized into per-layer folders, with a manifest.json summary.
    //
    // This is NOT generative sample synthesis — every exported file is a direct
    // slice of audio that was actually in the source track (via the Tier A/B/C/D
    // isolated layer buffers). See the "sample pack" scoping discussion: true
    // generative genre-specific sample creation is out of scope for this plugin.
    class SamplePackExporter
    {
    public:
        struct ExportSettings
        {
            juce::File destinationFolder;
            juce::String packName = "AkwardFreQ Pack";
            juce::String genreTag;
            int minRegionLengthMs = 30;   // skip slivers shorter than this
            bool writeManifest = true;
        };

        // Runs synchronously — does file I/O for potentially hundreds of small
        // files, so call this from a background thread, never the audio thread.
        // Returns true on success; on failure `errorMessage` explains why and
        // any files already written are left in place (not rolled back).
        static bool exportPack (const SeparationResult& result, const ExportSettings& settings,
                                 juce::String& errorMessage,
                                 const std::function<void (float progress0to1, juce::String message)>& onProgress = nullptr);
    };
}
