#include <juce_core/juce_core.h>
#include "TestUtils.h"
#include "mastering/LookaheadLimiter.h"

namespace afq
{
    struct LookaheadLimiterTests : juce::UnitTest
    {
        LookaheadLimiterTests() : juce::UnitTest ("LookaheadLimiter", "AkwardFreQ") {}

        static LookaheadLimiter make (double sampleRate, int channels, float ceilingDb)
        {
            LookaheadLimiter l;
            l.prepare (sampleRate, channels);
            l.setCeilingDb (ceilingDb);
            return l;
        }

        static void processInChunks (LookaheadLimiter& l, juce::AudioBuffer<float>& b, int chunk)
        {
            for (int pos = 0; pos < b.getNumSamples(); pos += chunk)
            {
                juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), b.getNumChannels(), pos, std::min (chunk, b.getNumSamples() - pos));
                l.process (view);
            }
        }

        // A loud, dense, hostile signal: a chord and noise bursts, well above 0 dBFS.
        static juce::AudioBuffer<float> hotSignal (int channels, double seconds, double sampleRate = test::kSampleRate)
        {
            auto b = test::silence (channels, test::samplesFor (seconds, sampleRate));
            for (double hz : { 110.0, 165.0, 440.0, 1234.0 }) test::addSine (b, 0, b.getNumSamples(), hz, 0.9f, sampleRate);
            const auto noise = test::whiteNoise (channels, b.getNumSamples(), 1.5f, 8u);
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < b.getNumSamples(); ++i)
                    if ((i / test::samplesFor (0.05, sampleRate)) % 4 == 0) b.setSample (ch, i, b.getSample (ch, i) + noise.getSample (ch, i));
            return b;
        }

        void runTest() override
        {
            using namespace test;

            beginTest ("the output never exceeds the ceiling");
            for (double sr : { 44100.0, 48000.0, 96000.0 })
                for (float ceilingDb : { -6.0f, -1.0f, -0.3f, 0.0f })
                {
                    auto l = make (sr, 2, ceilingDb);
                    auto b = hotSignal (2, 3.0, sr);
                    const float inputPeak = peakOf (b);
                    l.process (b);
                    const float ceiling = juce::Decibels::decibelsToGain (ceilingDb);
                    expect (inputPeak > 2.0f * ceiling, "the test signal is far above the ceiling");
                    expectLessOrEqual (peakOf (b), ceiling, "peak stays at or below the ceiling: " + juce::String (ceilingDb) + " dB at " + juce::String (sr) + " Hz");
                    expectEquals ((int) l.getClampedSampleCount(), 0, "the lookahead alone holds the ceiling; the safety clamp never had to act");
                    expect (allFinite (b), "output is finite");
                }

            beginTest ("signals under the ceiling pass through unchanged, delayed by the latency");
            {
                auto l = make (kSampleRate, 2, -1.0f);
                auto in = whiteNoise (2, samplesFor (1.0), 0.3f, 5u);
                auto out = in;
                l.process (out);
                const int latency = l.getLatencySamples();
                bool identical = true;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < in.getNumSamples(); ++i)
                    {
                        const float expected = i < latency ? 0.0f : in.getSample (ch, i - latency);
                        if (! sameSample (out.getSample (ch, i), expected)) identical = false;
                    }
                expect (identical, "bit-exact apart from the delay when nothing exceeds the ceiling");
            }

            beginTest ("latency");
            {
                for (double sr : { 44100.0, 48000.0, 96000.0 })
                {
                    auto l = make (sr, 1, 0.0f);
                    expectEquals (l.getLatencySamples(), (int) std::lround (LookaheadLimiter::kLookaheadSeconds * sr), "latency is the lookahead at " + juce::String (sr) + " Hz");
                    auto b = silence (1, samplesFor (0.1, sr));
                    b.setSample (0, 1000, 0.5f);
                    l.process (b);
                    expectEquals (b.getSample (0, 1000 + l.getLatencySamples()), 0.5f, "an impulse comes out exactly `latency` samples later");
                }
            }

            beginTest ("gain reduction is smooth, not a clip");
            {
                // A 1 kHz sine at 1.0 against a -6 dB ceiling. A hard clipper would flatten the peaks and add harmonics;
                // the limiter scales the whole waveform, so once it has settled output / delayed input is constant.
                auto l = make (kSampleRate, 1, -6.0f);
                auto in = silence (1, samplesFor (1.0));
                addSine (in, 0, in.getNumSamples(), 1000.0, 1.0f);
                auto out = in;
                l.process (out);
                const int latency = l.getLatencySamples();
                float lo = 10.0f, hi = -10.0f;
                for (int i = samplesFor (0.5); i < samplesFor (0.9); ++i)
                {
                    const float x = in.getSample (0, i - latency);
                    if (std::abs (x) < 0.5f) continue; // the ratio of two tiny numbers is noise
                    const float ratio = out.getSample (0, i) / x;
                    lo = std::min (lo, ratio); hi = std::max (hi, ratio);
                }
                expectEquals ((int) l.getClampedSampleCount(), 0, "no clipping: the safety clamp never had to act");
                expectWithinAbsoluteError (lo, 0.5f, 0.01f, "settled gain is ceiling / peak");
                expect (hi - lo < 0.01f, "the gain is constant across the waveform (spread " + juce::String (hi - lo, 4) + ")");
            }

            beginTest ("gain changes gradually when a loud passage starts");
            {
                // 200 Hz at 0.45 (under the -6 dB ceiling of 0.501), then without a break at full scale. The reduction has to
                // be in place by the time the loud samples come out, so it ramps down over the lookahead before them.
                auto l = make (kSampleRate, 1, -6.0f);
                const int loudStart = samplesFor (0.2);
                auto in = silence (1, samplesFor (0.5));
                for (int i = 0; i < in.getNumSamples(); ++i)
                    in.setSample (0, i, (i < loudStart ? 0.45f : 1.0f) * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 200.0 * i / kSampleRate));
                auto out = in;
                l.process (out);
                const int latency = l.getLatencySamples();

                // The gain applied to input sample i is out[i + latency] / in[i]; read it wherever in[i] is big enough to divide by.
                float largestStep = 0.0f, gainBefore = -1.0f, gainAfter = -1.0f, previous = 0.0f;
                int lastIndex = -10;
                for (int i = loudStart - 3 * latency; i < loudStart + 3 * latency; ++i)
                {
                    const float x = in.getSample (0, i);
                    if (std::abs (x) < 0.3f) continue;
                    const float g = out.getSample (0, i + latency) / x;
                    if (i - lastIndex == 1) largestStep = std::max (largestStep, std::abs (g - previous));
                    if (i < loudStart - 2 * latency) gainBefore = g;
                    if (i >= loudStart + latency) gainAfter = g;
                    previous = g; lastIndex = i;
                }
                expectEquals ((int) l.getClampedSampleCount(), 0, "the ramp is early enough that the safety clamp never had to act");
                expectWithinAbsoluteError (gainBefore, 1.0f, 1.0e-3f, "no reduction before the ramp starts");
                expectWithinAbsoluteError (gainAfter, 0.501f, 0.01f, "the loud part is brought down to the ceiling");
                expectLessThan (largestStep, 0.01f, "the gain moves in small steps (largest neighbouring step " + juce::String (largestStep, 4) + ")");
                expectLessOrEqual (peakOf (out), juce::Decibels::decibelsToGain (-6.0f), "and the first loud peak already respects the ceiling");
            }

            beginTest ("the gain recovers after a loud passage");
            {
                auto l = make (kSampleRate, 1, -6.0f);
                auto in = silence (1, samplesFor (3.0));
                addSine (in, 0, samplesFor (0.2), 300.0, 1.5f);
                addSine (in, samplesFor (0.2), samplesFor (2.8), 300.0, 0.1f); // -20 dBFS, under the ceiling
                auto out = in;
                l.process (out);
                const int latency = l.getLatencySamples();
                const int check = samplesFor (2.9);
                float worst = 0.0f;
                for (int i = check; i < samplesFor (3.0) - 1; ++i)
                    worst = std::max (worst, std::abs (out.getSample (0, i) - in.getSample (0, i - latency)));
                expectEquals ((int) l.getClampedSampleCount(), 0, "the safety clamp never had to act");
                expectLessThan (worst, 1.0e-6f, "after 2.7 s the release is complete and the signal is back to unity gain");
                // After the loud part the level rises again within a few release times, not instantly and not never.
                expectLessThan (rmsOf (out, samplesFor (0.25), samplesFor (0.30)), rmsOf (out, samplesFor (2.5), samplesFor (2.6)), "gain is still recovering shortly after the loud part");
            }

            beginTest ("both channels get the same gain");
            {
                auto l = make (kSampleRate, 2, -3.0f);
                auto in = silence (2, samplesFor (1.0));
                addSine (in, 0, in.getNumSamples(), 500.0, 1.0f);  // left is hot
                for (int i = 0; i < in.getNumSamples(); ++i) in.setSample (1, i, 0.1f * in.getSample (0, i)); // right is 20 dB down
                auto out = in;
                l.process (out);
                float worst = 0.0f;
                for (int i = samplesFor (0.6); i < samplesFor (0.9); ++i)
                    worst = std::max (worst, std::abs (out.getSample (1, i) - 0.1f * out.getSample (0, i)));
                expectEquals ((int) l.getClampedSampleCount(), 0, "the safety clamp never had to act");
                expectLessThan (worst, 1.0e-5f, "the quiet channel is turned down by the same gain (the image does not shift)");
                expectGreaterThan (rmsOf (out, samplesFor (0.6), samplesFor (0.9)), 0.0, "and it is not muted");
            }

            beginTest ("output does not depend on how the audio is split into blocks");
            {
                const auto in = hotSignal (2, 1.5);
                auto whole = in;
                auto a = make (kSampleRate, 2, -1.0f);
                a.process (whole);
                for (int chunk : { 1, 7, 64, 480, 512, 4096 })
                {
                    auto pieces = in;
                    auto b = make (kSampleRate, 2, -1.0f);
                    processInChunks (b, pieces, chunk);
                    bool identical = true;
                    for (int ch = 0; ch < 2 && identical; ++ch)
                        for (int i = 0; i < in.getNumSamples(); ++i)
                            if (! sameSample (pieces.getSample (ch, i), whole.getSample (ch, i))) { identical = false; break; }
                    expect (identical, "bit-identical with blocks of " + juce::String (chunk));
                }
            }

            beginTest ("reset clears all state");
            {
                auto l = make (kSampleRate, 2, -1.0f);
                auto first = hotSignal (2, 0.5);
                juce::AudioBuffer<float> firstHalf (first.getArrayOfWritePointers(), 2, 0, first.getNumSamples() / 2);
                juce::AudioBuffer<float> secondHalf (first.getArrayOfWritePointers(), 2, first.getNumSamples() / 2, first.getNumSamples() / 2);
                l.process (firstHalf);
                l.setCeilingDb (-20.0f); // lowered mid-stream: gains already computed for -1 dB are too high, so the clamp has to act
                l.process (secondHalf);
                expectGreaterThan ((int) l.getClampedSampleCount(), 0, "the clamp counter counts (a ceiling lowered mid-stream needs it)");
                l.reset();
                l.setCeilingDb (-1.0f);
                expectEquals ((int) l.getClampedSampleCount(), 0, "reset clears the clamp counter");
                auto in = hotSignal (2, 0.5);
                auto second = in;
                l.process (second);
                auto fresh = make (kSampleRate, 2, -1.0f);
                auto reference = in;
                fresh.process (reference);
                bool identical = true;
                for (int ch = 0; ch < 2 && identical; ++ch)
                    for (int i = 0; i < in.getNumSamples(); ++i)
                        if (! sameSample (second.getSample (ch, i), reference.getSample (ch, i))) { identical = false; break; }
                expect (identical, "a reset limiter behaves like a new one");
            }

            beginTest ("delay() matches process() timing and leaves the gain at unity");
            {
                auto l = make (kSampleRate, 2, -1.0f);
                auto in = whiteNoise (2, samplesFor (0.5), 0.3f, 6u);
                auto out = in;
                l.delay (out);
                const int latency = l.getLatencySamples();
                bool identical = true;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < in.getNumSamples(); ++i)
                        if (! sameSample (out.getSample (ch, i), i < latency ? 0.0f : in.getSample (ch, i - latency))) identical = false;
                expect (identical, "delay() is the input delayed by the latency");

                // Switching from processing to delaying and back keeps the timing: an impulse fed after the switch
                // still comes out `latency` samples later.
                auto m = make (kSampleRate, 1, -1.0f);
                auto hot = hotSignal (1, 0.2);
                m.process (hot);
                auto quiet = silence (1, samplesFor (0.05));
                m.delay (quiet);
                auto probe = silence (1, samplesFor (0.05));
                probe.setSample (0, 100, 0.25f);
                m.process (probe);
                expectEquals (probe.getSample (0, 100 + latency), 0.25f, "an impulse after switching back is delayed by exactly the latency");
            }

            beginTest ("hostile input");
            {
                auto l = make (kSampleRate, 2, -1.0f);
                auto b = silence (2, 4096);
                b.setSample (0, 100, std::numeric_limits<float>::quiet_NaN());
                b.setSample (1, 200, std::numeric_limits<float>::infinity());
                b.setSample (0, 300, -std::numeric_limits<float>::infinity());
                addSine (b, 400, 3000, 440.0, 3.0f);
                l.process (b);
                expect (allFinite (b), "NaN and infinity are replaced by silence, not passed through");
                expectLessOrEqual (peakOf (b), juce::Decibels::decibelsToGain (-1.0f), "and the ceiling still holds afterwards");

                auto empty = silence (2, 0);
                l.process (empty);
                l.delay (empty);
                expectEquals (empty.getNumSamples(), 0, "an empty block is fine");

                auto mono = make (kSampleRate, 2, -1.0f);
                auto one = hotSignal (1, 0.2);
                mono.process (one);
                expectLessOrEqual (peakOf (one), juce::Decibels::decibelsToGain (-1.0f), "a mono buffer on a stereo limiter is limited");
            }

            beginTest ("a ceiling lowered mid-stream still holds");
            {
                auto l = make (kSampleRate, 1, 0.0f);
                auto b = hotSignal (1, 1.0);
                auto first = juce::AudioBuffer<float> (b.getArrayOfWritePointers(), 1, 0, samplesFor (0.5));
                l.process (first);
                l.setCeilingDb (-6.0f);
                auto second = juce::AudioBuffer<float> (b.getArrayOfWritePointers(), 1, samplesFor (0.5), samplesFor (0.5));
                l.process (second);
                expectLessOrEqual (peakOf (b, samplesFor (0.5), b.getNumSamples()), juce::Decibels::decibelsToGain (-6.0f), "after the change nothing exceeds the new ceiling");
            }
        }
    };

    static LookaheadLimiterTests lookaheadLimiterTests;
}
