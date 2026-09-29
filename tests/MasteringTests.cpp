#include <juce_core/juce_core.h>
#include "TestUtils.h"
#include "mastering/LookaheadLimiter.h"
#include "mastering/MasteringChain.h"

namespace afq
{
    struct MasteringTests : juce::UnitTest
    {
        MasteringTests() : juce::UnitTest ("Mastering", "AkwardFreQ") {}

        static juce::AudioBuffer<float> render (const MasteringChain::Settings& settings, const juce::AudioBuffer<float>& in, int blockSize = 512)
        {
            MasteringChain chain;
            chain.prepare (test::kSampleRate, in.getNumChannels(), blockSize);
            chain.setSettings (settings);

            juce::AudioBuffer<float> out (in.getNumChannels(), in.getNumSamples());
            for (int pos = 0; pos < in.getNumSamples(); pos += blockSize)
            {
                const int n = std::min (blockSize, in.getNumSamples() - pos);
                juce::AudioBuffer<float> block (in.getNumChannels(), n);
                for (int ch = 0; ch < in.getNumChannels(); ++ch) block.copyFrom (ch, 0, in, ch, pos, n);
                chain.processBlock (block);
                for (int ch = 0; ch < in.getNumChannels(); ++ch) out.copyFrom (ch, pos, block, ch, 0, n);
            }
            return out;
        }

        static int latencyOf()
        {
            MasteringChain chain;
            chain.prepare (test::kSampleRate, 2, 512);
            return chain.getLatencySamples();
        }

        static MasteringChain::Settings active (float ceilingDb = -0.3f, float comp = 0.35f, float target = -8.0f)
        {
            MasteringChain::Settings s;
            s.bypass = false;
            s.compAmount = comp;
            s.limiterCeilingDb = ceilingDb;
            s.targetLoudnessLufs = target;
            s.eqMatchAmount = 0.0f;
            return s;
        }

        // A small "mix": kick hits at 145 BPM over a little seeded noise.
        static juce::AudioBuffer<float> testMix()
        {
            auto b = test::whiteNoise (2, test::samplesFor (2.0), 0.05f, 21);
            for (double t = 0.0; t < 1.9; t += 60.0 / 145.0)
                test::addDecayingHit (b, test::samplesFor (t), test::samplesFor (0.3), 55.0, 0.7f, 0.08);
            return b;
        }

        void runTest() override
        {
            using namespace test;

            beginTest ("bypass leaves the audio untouched, delayed by the reported latency");
            {
                const auto in = testMix();
                MasteringChain::Settings s;
                s.bypass = true;
                const auto out = render (s, in);
                const int latency = latencyOf();
                expectEquals (latency, (int) std::lround (LookaheadLimiter::kLookaheadSeconds * kSampleRate), "the chain reports the limiter's lookahead");
                bool identical = true;
                for (int ch = 0; ch < 2 && identical; ++ch)
                    for (int i = 0; i < in.getNumSamples(); ++i)
                        if (! sameSample (out.getSample (ch, i), i < latency ? 0.0f : in.getSample (ch, i - latency))) { identical = false; break; }
                expect (identical, "output is bit-identical to the input delayed by the latency");
            }

            beginTest ("toggling bypass does not shift the timing");
            {
                const auto in = testMix();
                constexpr int blockSize = 256;
                MasteringChain chain;
                chain.prepare (kSampleRate, 2, blockSize);
                const int latency = chain.getLatencySamples();
                MasteringChain::Settings on = active(), off;
                off.bypass = true;

                bool bypassedBlocksAligned = true;
                for (int block = 0; (block + 1) * blockSize <= in.getNumSamples(); ++block)
                {
                    const bool bypassed = block < 10 || block >= 20; // blocks 10..19 are processed
                    chain.setSettings (bypassed ? off : on);
                    juce::AudioBuffer<float> b (2, blockSize);
                    for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, in, ch, block * blockSize, blockSize);
                    chain.processBlock (b);
                    if (! bypassed) continue;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < blockSize; ++i)
                        {
                            // The delay line holds what the chain output before the switch, so the first `latency` samples after
                            // it are that (processed) audio; from then on the output is the raw input.
                            const int src = block * blockSize + i - latency;
                            if (block >= 20 && src < 20 * blockSize) continue;
                            if (! test::sameSample (b.getSample (ch, i), src < 0 ? 0.0f : in.getSample (ch, src))) bypassedBlocksAligned = false;
                        }
                }
                expect (bypassedBlocksAligned, "bypassed audio is the input delayed by exactly the latency, before and after a processed stretch");
            }

            beginTest ("processing changes the audio and stays finite");
            {
                const auto in = testMix();
                const auto out = render (active(), in);
                expect (allFinite (out), "no NaN or infinity");
                expectGreaterThan (std::abs (rmsOf (out) - rmsOf (in)), 1.0e-4, "the level moved");
            }

            beginTest ("the limiter never exceeds full scale");
            {
                const auto out = render (active (-1.0f, 0.0f), whiteNoise (2, samplesFor (2.0), 1.0f, 5));
                expectLessOrEqual (peakOf (out), 1.0f, "the output stays at or below 0 dBFS");
            }

            beginTest ("the limiter ceiling is a real ceiling");
            {
                // Full-scale noise through the whole chain, including the loudness trim's makeup gain.
                const auto in = whiteNoise (2, samplesFor (2.0), 1.0f, 5);
                for (float ceilingDb : { -6.0f, -3.0f, -1.0f, -0.3f, 0.0f })
                    for (float comp : { 0.0f, 0.35f, 1.0f })
                    {
                        const float peak = peakOf (render (active (ceilingDb, comp), in));
                        expectLessOrEqual (peak, juce::Decibels::decibelsToGain (ceilingDb),
                                           "peak " + juce::String (toDb (peak), 2) + " dBFS with ceiling " + juce::String (ceilingDb) + " dB, comp " + juce::String (comp));
                    }
            }

            beginTest ("the same input always gives the same output");
            {
                const auto in = testMix();
                const auto a = render (active(), in);
                const auto b = render (active(), in);
                bool identical = true;
                for (int ch = 0; ch < 2 && identical; ++ch)
                    for (int i = 0; i < in.getNumSamples(); ++i)
                        if (! sameSample (a.getSample (ch, i), b.getSample (ch, i))) { identical = false; break; }
                expect (identical, "two fresh chains agree exactly");
            }

            beginTest ("regression reference for the default chain");
            {
                // Peak, overall RMS, then RMS of each 200 ms block, all in dB. This pins current behaviour; it
                // catches unintended change and does not prove the numbers are correct. Regenerate on purpose:
                //   AFQ_UPDATE_REFERENCE=1 ./AkwardFreQTests Mastering
                const auto out = render (active(), testMix());
                std::vector<double> values { toDb (peakOf (out)), toDb (rmsOf (out)) };
                for (int block = 0; block < 10; ++block)
                    values.push_back (toDb (rmsOf (out, samplesFor (0.2) * block, samplesFor (0.2) * (block + 1))));
                const auto problem = Reference::compare ("mastering_default", values, 0.1);
                expect (problem.isEmpty(), problem);
            }
        }
    };

    static MasteringTests masteringTests;
}
