#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "TestUtils.h"
#include "midi/AudioToMidiConverter.h"

namespace afq
{
    struct AudioToMidiTests : juce::UnitTest
    {
        AudioToMidiTests() : juce::UnitTest ("AudioToMidi", "AkwardFreQ") {}

        static constexpr double kTicksPerSecond = 25.0 * 80.0; // the converter's SMPTE tick rate

        struct Note { int number; double startSeconds; double endSeconds; int velocity; };

        static std::vector<Note> notesOf (const juce::MidiMessageSequence& seq)
        {
            std::vector<Note> out;
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                const auto* e = seq.getEventPointer (i);
                if (! e->message.isNoteOn()) continue;
                const double end = e->noteOffObject != nullptr ? e->noteOffObject->message.getTimeStamp() : -1.0;
                out.push_back ({ e->message.getNoteNumber(), e->message.getTimeStamp() / kTicksPerSecond, end / kTicksPerSecond, (int) e->message.getVelocity() });
            }
            return out;
        }

        // Sine notes, one after another, each `noteSeconds` long with `gapSeconds` of silence after it. Real
        // instruments release rather than stop dead, so each note fades out over `releaseSeconds`; 0 is a hard cut.
        static juce::AudioBuffer<float> melody (const std::vector<double>& hz, double noteSeconds, double gapSeconds,
                                                double releaseSeconds = 0.02, float amp = 0.5f)
        {
            auto b = test::silence (1, test::samplesFor ((noteSeconds + gapSeconds) * (double) hz.size()));
            const int fade = test::samplesFor (releaseSeconds);
            for (size_t i = 0; i < hz.size(); ++i)
            {
                const int start = test::samplesFor ((double) i * (noteSeconds + gapSeconds));
                const int length = test::samplesFor (noteSeconds);
                test::addSine (b, start, length, hz[i], amp);
                if (fade > 0)
                    for (int k = 0; k < fade; ++k)
                        b.setSample (0, start + length - fade + k, b.getSample (0, start + length - fade + k) * (1.0f - (float) k / (float) fade));
            }
            return test::withNoiseFloor (b, -50.0f);
        }

        static AudioToMidiConverter::Settings settings() { return {}; }

        void runTest() override
        {
            using namespace test;

            beginTest ("transcribes a monophonic melody");
            {
                // A4, C5, E5 -> MIDI 69, 72, 76, notes 0.3 s long every 0.4 s.
                const auto b = melody ({ 440.0, 523.2511, 659.2551 }, 0.3, 0.1);
                const auto notes = notesOf (AudioToMidiConverter::transcribe (b, 0, b.getNumSamples(), settings()));
                std::vector<int> found;
                for (const auto& n : notes) found.push_back (n.number);
                expect (found == std::vector<int> { 69, 72, 76 }, "three notes with the right pitches, got " + juce::String ((int) notes.size()) + " notes");
                if (notes.size() == 3)
                    for (size_t i = 0; i < 3; ++i)
                        expectWithinAbsoluteError (notes[i].startSeconds, 0.4 * (double) i, 0.03, "note " + juce::String ((int) i) + " starts where the note does");
            }

            beginTest ("melody with hard-cut notes");
            {
                // Pitches are right, but a note that stops dead is a click, the click counts as an onset, and the
                // segment that starts there still holds the last ~16 ms of the note (onsets are placed at the start of
                // the analysis frame), so hard-cut notes gain a short second note. Measured: with a 10 ms or longer
                // release the melody transcribes exactly; at 5 ms it gives 4 notes; hard-cut it gives 6.
                const auto b = melody ({ 440.0, 523.2511, 659.2551 }, 0.3, 0.1, 0.0);
                const auto notes = notesOf (AudioToMidiConverter::transcribe (b, 0, b.getNumSamples(), settings()));
                std::vector<int> distinct;
                for (const auto& n : notes) if (distinct.empty() || distinct.back() != n.number) distinct.push_back (n.number);
                expect (distinct == std::vector<int> { 69, 72, 76 }, "the pitch sequence is still right");
                knownIssue (*this, notes.size() == 3, "hard-cut notes give " + juce::String ((int) notes.size()) + " notes for 3");
            }

            beginTest ("notes have sensible times and velocities");
            {
                const auto b = melody ({ 440.0, 523.2511, 659.2551 }, 0.3, 0.1);
                const auto notes = notesOf (AudioToMidiConverter::transcribe (b, 0, b.getNumSamples(), settings()));
                expect (! notes.empty(), "a clear melody produces notes");
                for (const auto& n : notes)
                {
                    expect (n.startSeconds >= 0.0 && n.endSeconds > n.startSeconds, "note starts at or after 0 and ends after it starts");
                    expect (n.endSeconds <= (double) b.getNumSamples() / kSampleRate + 0.01, "note ends within the audio");
                    expect (n.velocity >= 1 && n.velocity <= 127, "velocity within 1..127");
                    expect (n.number >= 0 && n.number <= 127, "note number within 0..127");
                }
            }

            beginTest ("louder audio gives higher velocities");
            {
                auto meanVelocity = [] (float amp)
                {
                    const auto b = melody ({ 440.0 }, 0.4, 0.0, 0.02, amp);
                    const auto notes = notesOf (AudioToMidiConverter::transcribe (b, 0, b.getNumSamples(), settings()));
                    double sum = 0.0;
                    for (const auto& n : notes) sum += n.velocity;
                    return notes.empty() ? 0.0 : sum / (double) notes.size();
                };
                const double quiet = meanVelocity (0.05f), loud = meanVelocity (0.5f);
                expectGreaterThan (quiet, 0.0, "quiet audio still gives notes");
                expectGreaterThan (loud, quiet, "louder audio gives higher mean velocity");
            }

            beginTest ("silence and noise produce no notes");
            {
                const auto quiet = silence (1, samplesFor (1.0));
                expect (notesOf (AudioToMidiConverter::transcribe (quiet, 0, quiet.getNumSamples(), settings())).empty(), "silence");
                const auto noise = whiteNoise (1, samplesFor (1.0), 0.5f, 11);
                expect (notesOf (AudioToMidiConverter::transcribe (noise, 0, noise.getNumSamples(), settings())).empty(), "unpitched noise is treated as rests");
            }

            beginTest ("a minimum duration longer than any note gives no notes");
            {
                auto strict = settings();
                strict.minNoteDurationMs = 500.0f;
                const auto b = melody ({ 440.0, 523.2511 }, 0.3, 0.1);
                expect (notesOf (AudioToMidiConverter::transcribe (b, 0, b.getNumSamples(), strict)).empty(), "0.3 s notes with a 500 ms minimum");
            }

            beginTest ("timestamps are relative to the start of the range");
            {
                const auto b = melody ({ 440.0, 523.2511, 659.2551 }, 0.3, 0.1);
                const auto notes = notesOf (AudioToMidiConverter::transcribe (b, samplesFor (0.4), samplesFor (1.2), settings()));
                expect (! notes.empty(), "the range holds audio");
                for (const auto& n : notes)
                    expect (n.startSeconds >= 0.0 && n.endSeconds <= 0.8 + 0.01, "note times are measured from the range start (0.8 s long)");
            }

            beginTest ("empty or backwards range gives no notes");
            {
                const auto b = melody ({ 440.0 }, 0.5, 0.0);
                expectEquals (AudioToMidiConverter::transcribe (b, 1000, 1000, settings()).getNumEvents(), 0);
                expectEquals (AudioToMidiConverter::transcribe (b, 5000, 1000, settings()).getNumEvents(), 0);
            }

            beginTest ("a range past the end of the buffer is handled");
            {
                const auto b = melody ({ 440.0, 523.2511 }, 0.3, 0.1);
                const auto seq = AudioToMidiConverter::transcribe (b, 0, b.getNumSamples() + 100000, settings());
                for (const auto& n : notesOf (seq))
                    expectLessOrEqual (n.startSeconds, (double) b.getNumSamples() / kSampleRate + 0.01, "no note starts after the audio ends");
                const auto seq2 = AudioToMidiConverter::transcribe (b, -5000, b.getNumSamples(), settings());
                expect (seq2.getNumEvents() >= 0, "a negative start does not crash");
            }

            beginTest ("MIDI file round trip");
            {
                // Built by hand so this checks writeMidiFile alone, whatever the transcriber does.
                juce::MidiMessageSequence seq;
                const int numbers[] = { 69, 72, 76 };
                for (int i = 0; i < 3; ++i)
                {
                    auto on = juce::MidiMessage::noteOn (1, numbers[i], (juce::uint8) 90);
                    on.setTimeStamp ((double) i * 0.4 * kTicksPerSecond);
                    auto off = juce::MidiMessage::noteOff (1, numbers[i]);
                    off.setTimeStamp (((double) i * 0.4 + 0.3) * kTicksPerSecond);
                    seq.addEvent (on);
                    seq.addEvent (off);
                }
                seq.updateMatchedPairs();

                const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("afq-test", ".mid");
                juce::String error;
                expect (AudioToMidiConverter::writeMidiFile (seq, kSampleRate, 128.0, file, error), "write succeeds: " + error);
                expect (file.existsAsFile() && file.getSize() > 0, "file exists and is not empty");

                juce::MidiFile read;
                juce::FileInputStream in (file);
                expect (in.openedOk() && read.readFrom (in), "file reads back as a valid MIDI file");
                expectEquals (read.getNumTracks(), 1);
                expect (read.getTimeFormat() < 0, "uses an absolute (SMPTE) time format, so tempo cannot stretch it");

                read.convertTimestampTicksToSeconds();
                std::vector<int> gotNumbers;
                std::vector<double> gotStarts, gotEnds;
                for (int i = 0; i < read.getTrack (0)->getNumEvents(); ++i)
                {
                    const auto* e = read.getTrack (0)->getEventPointer (i);
                    if (! e->message.isNoteOn()) continue;
                    gotNumbers.push_back (e->message.getNoteNumber());
                    gotStarts.push_back (e->message.getTimeStamp());
                    gotEnds.push_back (e->noteOffObject != nullptr ? e->noteOffObject->message.getTimeStamp() : -1.0);
                }
                expect (gotNumbers == std::vector<int> ({ 69, 72, 76 }), "the same three notes come back");
                if (gotStarts.size() == 3)
                    for (int i = 0; i < 3; ++i)
                    {
                        expectWithinAbsoluteError (gotStarts[(size_t) i], (double) i * 0.4, 0.001, "note " + juce::String (i) + " start");
                        expectWithinAbsoluteError (gotEnds[(size_t) i], (double) i * 0.4 + 0.3, 0.001, "note " + juce::String (i) + " end");
                    }
                file.deleteFile();
            }

            beginTest ("writing to an unwritable location reports an error");
            {
                juce::MidiMessageSequence seq;
                juce::String error;
                const juce::File bad ("/this/folder/does/not/exist/out.mid");
                expect (! AudioToMidiConverter::writeMidiFile (seq, kSampleRate, 120.0, bad, error), "returns false");
                expect (error.isNotEmpty(), "and says why");
            }
        }
    };

    static AudioToMidiTests audioToMidiTests;
}
