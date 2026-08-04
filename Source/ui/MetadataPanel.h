#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../export/MetadataTags.h"

namespace afq
{
    // Compact tag-entry UI embedded in each export tab (Export / Instrument /
    // Drum Chop) — title/artist/genre/comment fields plus the plugin's own
    // BPM/key estimate, applied to every WAV that export operation writes.
    // Each embedding owns its own fields (a one-shot might reasonably get a
    // different title than the whole pack), not a single shared global state.
    class MetadataPanel : public juce::Component
    {
    public:
        MetadataPanel();

        // Pre-fills the read-only BPM/key line — still included in the tags
        // returned by getMetadata() even though the fields themselves aren't editable here.
        void setAnalysisInfo (double bpm, const juce::String& key);

        TrackMetadata getMetadata() const;

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        juce::Label titleLabel_ { {}, "Title" };
        juce::Label artistLabel_ { {}, "Artist" };
        juce::Label genreLabel_ { {}, "Genre" };
        juce::Label commentLabel_ { {}, "Comment" };
        juce::TextEditor titleEditor_, artistEditor_, genreEditor_, commentEditor_;
        juce::Label infoLabel_ { {}, "BPM/Key: not analyzed yet" };

        double bpm_ = 0.0;
        juce::String key_;
    };
}
