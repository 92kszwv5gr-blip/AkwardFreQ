#pragma once

#include <juce_dsp/juce_dsp.h>

namespace afq
{
    // Finds a clean-sounding loop point near a rough, user-picked time range.
    class LoopSnapper
    {
    public:
        struct Result
        {
            int64_t startSample = 0;
            int64_t endSample = 0;
            bool snapped = false; // false = bpm was <=0, so this is just the clamped rough range
        };

        // Snaps [roughStart, roughEnd) to exactly `bars` bars at the given bpm,
        // bar-grid-aligned near roughStart, then fine-tuned within a small
        // window (+/- 1/8 bar) to minimize the waveform discontinuity at the
        // loop seam: it compares the audio leading into the candidate start
        // against the audio leading into the candidate end, since those are
        // the two segments that sit back-to-back once the loop wraps — a
        // close match there is what makes a loop sound seamless rather than
        // clicking at the repeat point.
        //
        // Falls back to the clamped rough range (Result::snapped = false) if
        // bpm <= 0 (no tempo to build a bar grid from) or the buffer is too
        // short for the requested bar count.
        static Result snapToLoop (const juce::AudioBuffer<float>& buffer, double sampleRate, double bpm,
                                   int64_t roughStart, int64_t roughEnd, int bars);
    };
}
