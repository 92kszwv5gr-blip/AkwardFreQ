#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "../export/MetadataTags.h"
#include "PresetBar.h"

namespace afq
{
    // Compact tag-entry UI embedded in each export tab (Export / Instrument /
    // Drum Chop) — title/artist/genre/comment fields plus the plugin's own
    // BPM/key estimate, applied to every WAV that export operation writes.
    // Each embedding owns its own field *values* (a one-shot might reasonably
    // get a different title than the whole pack), but all three share one
    // "TagProfile" preset library (presetBar_ — see PresetBar) so something
    // like your artist name only has to be typed and saved as default once,
    // not re-entered on every tab.
    class MetadataPanel : public juce::Component
    {
    public:
        MetadataPanel();

        // Pre-fills the read-only BPM/key line — still included in the tags
        // returned by getMetadata() even though the fields themselves aren't editable here.
        void setAnalysisInfo (double bpm, const juce::String& key);

        TrackMetadata getMetadata() const;

        // Captures/restores just the editable fields (title/artist/genre/
        // comment) — used both by presetBar_ and by the top-level Global
        // preset, which bundles this panel's state alongside everything else.
        std::unique_ptr<juce::XmlElement> captureXml() const;
        void applyXml (const juce::XmlElement& xml);

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        juce::Label titleLabel_ { {}, "Title" };
        juce::Label artistLabel_ { {}, "Artist" };
        juce::Label genreLabel_ { {}, "Genre" };
        juce::Label commentLabel_ { {}, "Comment" };
        juce::TextEditor titleEditor_, artistEditor_, genreEditor_, commentEditor_;
        juce::Label infoLabel_ { {}, "BPM/Key: not analyzed yet" };
        PresetBar presetBar_ { "TagProfile", "Tag Profile" };

        double bpm_ = 0.0;
        juce::String key_;
    };
}
