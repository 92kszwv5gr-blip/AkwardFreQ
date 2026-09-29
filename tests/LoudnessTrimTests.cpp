#include <juce_core/juce_core.h>
#include "TestUtils.h"
#include "mastering/LoudnessTrim.h"
#include "mastering/LookaheadLimiter.h"

namespace afq
{
    struct LoudnessTrimTests : juce::UnitTest
    {
        LoudnessTrimTests() : juce::UnitTest ("LoudnessTrim", "AkwardFreQ") {}

        struct Run
        {
            juce::AudioBuffer<float> output;
            std::vector<float> gainDbPerBlock;
            int blockSize = 512;
        };

        // trim -> limiter -> observe, as MasteringChain runs them.
        static Run run (const juce::AudioBuffer<float>& in, float targetLufs, float ceilingDb = 0.0f, int blockSize = 512)
        {
            LoudnessTrim trim;
            trim.prepare (test::kSampleRate, in.getNumChannels());
            trim.setTargetLufs (targetLufs);
            LookaheadLimiter limiter;
            limiter.prepare (test::kSampleRate, in.getNumChannels());
            limiter.setCeilingDb (ceilingDb);

            Run r;
            r.blockSize = blockSize;
            r.output.setSize (in.getNumChannels(), in.getNumSamples());
            for (int pos = 0; pos < in.getNumSamples(); pos += blockSize)
            {
                const int n = std::min (blockSize, in.getNumSamples() - pos);
                juce::AudioBuffer<float> block (in.getNumChannels(), n);
                for (int ch = 0; ch < in.getNumChannels(); ++ch) block.copyFrom (ch, 0, in, ch, pos, n);
                trim.apply (block);
                limiter.process (block);
                trim.observeOutput (block);
                for (int ch = 0; ch < in.getNumChannels(); ++ch) r.output.copyFrom (ch, pos, block, ch, 0, n);
                r.gainDbPerBlock.push_back (trim.getGainDb());
            }
            return r;
        }

        static float gainAt (const Run& r, double seconds) { return r.gainDbPerBlock[(size_t) std::min ((int) r.gainDbPerBlock.size() - 1, test::samplesFor (seconds) / r.blockSize)]; }

        static float loudnessBetween (const juce::AudioBuffer<float>& b, double from, double to)
        {
            const int start = test::samplesFor (from), end = std::min (b.getNumSamples(), test::samplesFor (to));
            juce::AudioBuffer<float> part (b.getNumChannels(), end - start);
            for (int ch = 0; ch < b.getNumChannels(); ++ch) part.copyFrom (ch, 0, b, ch, start, end - start);
            LoudnessMeter m;
            m.prepare (test::kSampleRate, b.getNumChannels());
            return m.measureIntegratedLoudness (part);
        }

        // Stereo 1 kHz tone with a level per section: {seconds, dBFS peak}.
        static juce::AudioBuffer<float> sections (int channels, const std::vector<std::pair<double, float>>& parts)
        {
            double total = 0.0;
            for (auto& p : parts) total += p.first;
            auto b = test::silence (channels, test::samplesFor (total));
            double at = 0.0;
            for (auto& p : parts)
            {
                test::addSine (b, test::samplesFor (at), test::samplesFor (p.first), 1000.0, juce::Decibels::decibelsToGain (p.second));
                at += p.first;
            }
            return b;
        }

        // Kick-like hits every 0.4 s over a little noise: a signal with real peaks, so the limiter has work to do.
        static juce::AudioBuffer<float> kickLoop (double seconds)
        {
            auto b = test::whiteNoise (2, test::samplesFor (seconds), 0.03f, 17u);
            for (double t = 0.0; t < seconds - 0.4; t += 0.4) test::addDecayingHit (b, test::samplesFor (t), test::samplesFor (0.35), 60.0, 0.9f, 0.09);
            return b;
        }

        void runTest() override
        {
            using namespace test;

            beginTest ("a steady signal lands on the target");
            for (float levelDb : { -34.0f, -30.0f, -20.0f })
                for (float target : { -23.0f, -14.0f, -10.0f })
                {
                    const auto r = run (sections (2, { { 30.0, levelDb } }), target);
                    expectWithinAbsoluteError (loudnessBetween (r.output, 5.0, 30.0), target, 0.3f,
                                               "a " + juce::String (levelDb) + " dBFS tone trimmed to " + juce::String (target) + " LUFS");
                }

            beginTest ("mono works the same way");
            {
                const auto r = run (sections (1, { { 20.0, -30.0f } }), -14.0f);
                expectWithinAbsoluteError (loudnessBetween (r.output, 4.0, 20.0), -14.0f, 0.3f, "mono tone trimmed to -14 LUFS");
            }

            beginTest ("the loudness the limiter takes away is made up for");
            {
                // Feed-forward alone would leave this well short: the kicks' peaks are limited. The feedback on the output
                // brings the finished loudness to the target.
                const auto in = kickLoop (90.0);
                const auto r = run (in, -14.0f, -1.0f);
                const float late = loudnessBetween (r.output, 60.0, 90.0);
                expectWithinAbsoluteError (late, -14.0f, 0.5f, "output loudness after 60 s (got " + juce::String (late, 2) + ")");
                expectLessOrEqual (peakOf (r.output), juce::Decibels::decibelsToGain (-1.0f), "and the ceiling holds");
            }

            beginTest ("moves slowly: no pumping on beats or sections");
            {
                // 20 s at -20 dBFS then 20 s at -30 dBFS, target -14 LUFS.
                const auto r = run (sections (2, { { 20.0, -20.0f }, { 20.0, -30.0f } }), -14.0f);
                const float before = gainAt (r, 19.9);
                expectLessThan (std::abs (gainAt (r, 21.0) - before), 1.0f, "one second into the quiet section the gain has moved under 1 dB");
                // The output level of the second section converges toward the target over tens of seconds.
                expectGreaterThan (gainAt (r, 40.0), gainAt (r, 21.0), "the gain keeps rising to make up the quiet section");
                float largestStep = 0.0f;
                for (size_t i = 1; i < r.gainDbPerBlock.size(); ++i) largestStep = std::max (largestStep, std::abs (r.gainDbPerBlock[i] - r.gainDbPerBlock[i - 1]));
                expectLessThan (largestStep, LoudnessTrim::kMaxSlewDbPerSecond * 512.0f / (float) kSampleRate + 0.01f, "the applied gain never moves faster than the slew limit, even on the 10 LU change of level");

                // A 100 ms burst 20 dB louder than the tone, in an otherwise steady signal, barely moves the gain.
                auto steady = sections (2, { { 30.0, -30.0f } });
                addSine (steady, samplesFor (15.0), samplesFor (0.1), 1000.0, juce::Decibels::decibelsToGain (-10.0f));
                const auto quiet = run (sections (2, { { 30.0, -30.0f } }), -14.0f);
                const auto burst = run (steady, -14.0f);
                float worst = 0.0f;
                for (size_t i = 0; i < burst.gainDbPerBlock.size(); ++i) worst = std::max (worst, std::abs (burst.gainDbPerBlock[i] - quiet.gainDbPerBlock[i]));
                // A burst is power in the 3 s window, so it does count a little: 20 dB for 100 ms is +6 dB of window power.
                expectLessThan (worst, 1.2f, "a 100 ms burst 20 dB louder moves the gain by only about a dB (moved " + juce::String (worst, 2) + ")");
            }

            beginTest ("silence and noise floor are never boosted");
            {
                const auto r = run (sections (2, { { 15.0, -25.0f }, { 20.0, -300.0f }, { 20.0, -75.0f }, { 10.0, -25.0f } }), -14.0f);
                const float settled = gainAt (r, 16.0); // the music ends at 15 s; the next hop or two may still count
                expectEquals (gainAt (r, 34.0), settled, "20 s of digital silence leave the gain where it was");
                expectEquals (gainAt (r, 54.0), settled, "so does 20 s of -75 dBFS tone");
                expectWithinAbsoluteError (loudnessBetween (r.output, 58.0, 65.0), -14.0f, 0.5f, "and the music that follows comes out on target");
                expect (allFinite (r.output), "output stays finite");
            }

            beginTest ("gain limits");
            {
                // -45 LUFS to -8 LUFS would need +37 dB. The gain stops at the limit and the output stays clean.
                const auto quietTone = run (sections (2, { { 20.0, -45.0f } }), -8.0f);
                expectEquals (gainAt (quietTone, 19.0), LoudnessTrim::kMaxGainDb, "gain stops at the +24 dB limit");
                expect (allFinite (quietTone.output), "finite");

                // Below the gate the signal is ignored altogether.
                const auto belowGate = run (sections (2, { { 20.0, -60.0f } }), -8.0f);
                expectEquals (gainAt (belowGate, 19.0), 0.0f, "a signal under the -50 LUFS gate is not touched");

                // A target the limiter cannot reach: high-crest audio, low ceiling, loud target. The gain must stay bounded.
                const auto unreachable = run (kickLoop (60.0), -6.0f, -6.0f);
                float highest = -100.0f;
                for (float g : unreachable.gainDbPerBlock) highest = std::max (highest, g);
                expectLessOrEqual (highest, LoudnessTrim::kMaxGainDb, "an unreachable target does not run the gain away");
                expectLessOrEqual (peakOf (unreachable.output), juce::Decibels::decibelsToGain (-6.0f), "the ceiling still holds");
                expect (allFinite (unreachable.output), "output finite");
            }

            beginTest ("a large change of target re-seeds quickly");
            {
                // Target -14 for 10 s, then a target of -8 needs +6 LU... use a bigger jump: -30 to -10 (20 LU).
                LoudnessTrim trim;
                trim.prepare (kSampleRate, 2);
                LookaheadLimiter limiter;
                limiter.prepare (kSampleRate, 2);
                const auto in = sections (2, { { 30.0, -30.0f } });
                juce::AudioBuffer<float> out (2, in.getNumSamples());
                for (int pos = 0; pos < in.getNumSamples(); pos += 512)
                {
                    trim.setTargetLufs (pos < samplesFor (10.0) ? -30.0f : -10.0f);
                    const int n = std::min (512, in.getNumSamples() - pos);
                    juce::AudioBuffer<float> block (2, n);
                    for (int ch = 0; ch < 2; ++ch) block.copyFrom (ch, 0, in, ch, pos, n);
                    trim.apply (block); limiter.process (block); trim.observeOutput (block);
                    for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, block, ch, 0, n);
                }
                expectWithinAbsoluteError (loudnessBetween (out, 16.0, 30.0), -10.0f, 0.5f, "within about 6 s of a 20 LU change of target the output is on it");
            }

            beginTest ("block size makes no real difference");
            {
                const auto in = kickLoop (40.0);
                const auto a = run (in, -14.0f, -1.0f, 64);
                const auto b = run (in, -14.0f, -1.0f, 4096);
                expectWithinAbsoluteError (loudnessBetween (a.output, 20.0, 40.0), loudnessBetween (b.output, 20.0, 40.0), 0.2f, "loudness with 64- and 4096-sample blocks");
                expectWithinAbsoluteError (a.gainDbPerBlock.back(), b.gainDbPerBlock.back(), 0.3f, "and the gain they settle on");
            }

            beginTest ("reset returns to unity");
            {
                LoudnessTrim trim;
                trim.prepare (kSampleRate, 2);
                trim.setTargetLufs (-10.0f);
                auto b = sections (2, { { 5.0, -30.0f } });
                for (int pos = 0; pos + 512 <= b.getNumSamples(); pos += 512)
                {
                    juce::AudioBuffer<float> block (b.getArrayOfWritePointers(), 2, pos, 512);
                    trim.apply (block); trim.observeOutput (block);
                }
                expectGreaterThan (trim.getGainDb(), 5.0f, "the trim found a gain");
                trim.reset();
                expectEquals (trim.getGainDb(), 0.0f, "reset puts the gain back to 0 dB");
                expectEquals (trim.getOutputLoudnessLufs(), -70.0f, "and clears the reading");
            }
        }
    };

    static LoudnessTrimTests loudnessTrimTests;
}
