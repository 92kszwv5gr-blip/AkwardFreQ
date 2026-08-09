#pragma once

#include <juce_core/juce_core.h>

namespace afq
{
    // Writes an Ableton Simpler instrument preset (.adv) by PATCHING a real,
    // Ableton-exported template rather than generating the XML tree from
    // scratch. Ableton's .adv format is undocumented, gzip-compressed XML with
    // dozens of envelope/filter/warp parameters — hand-building a correct tree
    // from public reverse-engineering notes alone is high-risk (Ableton's
    // loader is strict, and a wrong field can make the whole preset refuse to
    // load). Patching a known-valid file and only touching the fields we're
    // confident about (the sample's file path, primarily) is far more likely
    // to keep working.
    //
    // Requires a template: export any simple Simpler preset from Ableton once
    // (drag a sample onto a track, right-click the Simpler device -> Save
    // Preset) and point Models/Templates/simpler_template.adv at it — see
    // Models/Templates/README.md. Without a template this returns false with
    // a clear error rather than guessing at a from-scratch schema.
    class AbletonPresetWriter
    {
    public:
        struct SimplerPatch
        {
            juce::File templateAdvFile;
            juce::File sampleWavFile;   // absolute path Ableton should reference
            int rootKey = 60;           // best-effort — only applied if a recognizable key-center node is found
            int lowKey = 0;
            int highKey = 127;
        };

        // On success, writes the patched preset to `outAdvFile`. The sample
        // path is always patched (that's the well-understood part of the
        // schema); root key / key range patching is attempted opportunistically
        // and silently skipped if the expected nodes aren't found in this
        // particular template — check the plugin's log for what was and
        // wasn't patched.
        static bool writeSimplerPreset (const SimplerPatch& patch, const juce::File& outAdvFile,
                                         juce::String& errorMessage);

    private:
        static bool loadGzipXml (const juce::File& file, std::unique_ptr<juce::XmlElement>& outRoot,
                                  juce::String& errorMessage);
        static bool saveGzipXml (const juce::XmlElement& root, const juce::File& outFile, juce::String& errorMessage);

        static juce::XmlElement* findFirstByTag (juce::XmlElement* root, const juce::String& tag);
        static void patchValueAttribute (juce::XmlElement* element, const juce::String& newValue);
    };
}
