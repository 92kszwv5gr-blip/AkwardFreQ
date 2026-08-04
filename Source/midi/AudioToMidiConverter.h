#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace afq
{
    // Monophonic pitch-tracking transcription: segments audio at onsets, pitch-
    // tracks each segment (reusing AnalysisUtils::estimatePitch), and emits one
    // MIDI note per segment. This is genuinely useful for monophonic content —
    // a synth lead or bass line — but it is NOT a polyphonic/chord transcriber:
    // fed a chordal stab or an atmosphere pad, it will output a single
    // wandering note per onset (usually the strongest partial), not the actual
    // chord. Real polyphonic transcription needs a dedicated trained model,
    // out of scope here.
    class AudioToMidiConverter
    {
    public:
        struct Settings
        {
            double sampleRate = 44100.0;
            float minConfidence = 0.3f;      // autocorrelation confidence below this -> treated as a rest
            float minNoteDurationMs = 30.0f; // segments shorter than this are dropped
            float silenceRms = 0.01f;        // segments quieter than this are treated as a rest
        };

        // Transcribes [startSample, endSample) of `buffer`. Returned sequence's
        // timestamps are in this converter's internal SMPTE-tick units (see
        // .cpp) — pass straight through to writeMidiFile, don't reinterpret them.
        static juce::MidiMessageSequence transcribe (const juce::AudioBuffer<float>& buffer,
                                                       int64_t startSample, int64_t endSample,
                                                       const Settings& settings);

        // Writes a Standard MIDI File using an absolute (SMPTE) time format —
        // deliberately not tempo/ticks-per-quarter-note, so there's no risk of
        // a BPM-mismatch stretching the transcription relative to the source
        // audio. `bpm` is accepted for a future tempo-track but unused today.
        static bool writeMidiFile (const juce::MidiMessageSequence& sequence, double sampleRate, double bpm,
                                    const juce::File& outFile, juce::String& errorMessage);
    };
}
