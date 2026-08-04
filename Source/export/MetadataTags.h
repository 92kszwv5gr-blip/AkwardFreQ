#pragma once

#include <juce_core/juce_core.h>

namespace afq
{
    // User-entered (or auto-filled) tag fields, applied to every WAV file a
    // given export operation writes. True ID3v2 is an MP3-native spec; for
    // our WAV output the practical equivalent is the RIFF INFO chunk, which
    // is WAV's own standard tagging mechanism and what most players/DAWs
    // (including Ableton's sample browser) read title/artist/genre from.
    struct TrackMetadata
    {
        juce::String title;
        juce::String artist;
        juce::String genre;
        juce::String comment;
        double bpm = 0.0;    // 0 = unknown, omitted from tags
        juce::String key;    // empty = unknown, omitted from tags

        juce::StringPairArray toRiffTags() const
        {
            juce::StringPairArray tags;
            if (title.isNotEmpty())   tags.set ("INAM", title);
            if (artist.isNotEmpty())  tags.set ("IART", artist);
            if (genre.isNotEmpty())   tags.set ("IGNR", genre);
            if (key.isNotEmpty())     tags.set ("IKEY", key); // non-standard FourCC; folded into ICMT below too, in case a reader ignores it

            juce::String comments = comment;
            if (bpm > 0.0)
                comments = (comments.isNotEmpty() ? comments + " | " : juce::String()) + juce::String (bpm, 1) + " BPM";
            if (key.isNotEmpty())
                comments = (comments.isNotEmpty() ? comments + " | " : juce::String()) + "Key: " + key;
            if (comments.isNotEmpty()) tags.set ("ICMT", comments);

            tags.set ("ISFT", "AkwardFreQ"); // originating software — always set
            return tags;
        }
    };
}
