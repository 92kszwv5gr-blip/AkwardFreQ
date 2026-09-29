#include <juce_core/juce_core.h>
#include "TestUtils.h"
#include "mastering/LookaheadLimiter.h"
#include "mastering/LoudnessMeter.h"
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

            beginTest ("the chain brings a steady signal to the target loudness");
            {
                auto tone = silence (2, samplesFor (20.0));
                addSine (tone, 0, tone.getNumSamples(), 1000.0, juce::Decibels::decibelsToGain (-30.0f));
                MasteringChain chain;
                chain.prepare (kSampleRate, 2, 512);
                auto s = active (-1.0f, 0.0f, -14.0f);
                chain.setSettings (s);
                juce::AudioBuffer<float> out (2, tone.getNumSamples());
                for (int pos = 0; pos + 512 <= tone.getNumSamples(); pos += 512)
                {
                    juce::AudioBuffer<float> block (2, 512);
                    for (int ch = 0; ch < 2; ++ch) block.copyFrom (ch, 0, tone, ch, pos, 512);
                    chain.processBlock (block);
                    for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, block, ch, 0, 512);
                }
                juce::AudioBuffer<float> tail (2, samplesFor (10.0));
                for (int ch = 0; ch < 2; ++ch) tail.copyFrom (ch, 0, out, ch, samplesFor (8.0), tail.getNumSamples());
                LoudnessMeter meter;
                meter.prepare (kSampleRate, 2);
                expectWithinAbsoluteError (meter.measureIntegratedLoudness (tail), -14.0f, 0.6f, "output loudness with the target at -14 LUFS");
                expectWithinAbsoluteError (chain.getLastMeasuredLoudnessLufs(), -14.0f, 0.6f, "and the loudness the chain reports agrees");
                expectGreaterThan (chain.getTrimGainDb(), 10.0f, "by a gain of about 16 dB");
            }

            beginTest ("a block larger than announced is handled");
            {
                const auto in = whiteNoise (2, 1000, 0.3f, 8u);
                auto render1000 = [&] (bool oneBlock)
                {
                    MasteringChain chain;
                    chain.prepare (kSampleRate, 2, 256);
                    chain.setSettings (active());
                    auto b = in;
                    if (oneBlock) chain.processBlock (b);
                    else
                        for (int pos = 0; pos < 1000; pos += 256)
                        {
                            juce::AudioBuffer<float> piece (b.getArrayOfWritePointers(), 2, pos, std::min (256, 1000 - pos));
                            chain.processBlock (piece);
                        }
                    return b;
                };
                const auto big = render1000 (true), chunked = render1000 (false);
                bool identical = true;
                for (int ch = 0; ch < 2 && identical; ++ch)
                    for (int i = 0; i < 1000; ++i)
                        if (! sameSample (big.getSample (ch, i), chunked.getSample (ch, i))) { identical = false; break; }
                expect (identical, "a 1000-sample block into a chain prepared for 256 equals four smaller blocks, and does not overrun");
                expect (allFinite (big), "finite");
            }

            beginTest ("EQ match raises the band where the reference is stronger");
            {
                // Reference: white noise plus a strong 9.3 kHz tone (the centre of the tenth EQ band). Current: plain noise.
                auto reference = whiteNoise (2, samplesFor (3.0), 0.05f, 31u);
                addSine (reference, 0, reference.getNumSamples(), 9300.0, 0.4f);
                const auto current = whiteNoise (2, samplesFor (3.0), 0.05f, 32u);
                const auto in = whiteNoise (2, samplesFor (4.0), 0.05f, 33u);

                auto bandEnergy = [] (const juce::AudioBuffer<float>& b, double fromHz, double toHz)
                {
                    constexpr int order = 13, n = 1 << order;
                    juce::dsp::FFT fft (order);
                    juce::dsp::WindowingFunction<float> window (n, juce::dsp::WindowingFunction<float>::hann);
                    std::vector<float> data ((size_t) (2 * n));
                    double energy = 0.0;
                    for (int start = samplesFor (1.0); start + n <= b.getNumSamples(); start += n)
                    {
                        std::fill (data.begin(), data.end(), 0.0f);
                        std::copy (b.getReadPointer (0) + start, b.getReadPointer (0) + start + n, data.begin());
                        window.multiplyWithWindowingTable (data.data(), (size_t) n);
                        fft.performRealOnlyForwardTransform (data.data());
                        for (int bin = 0; bin < n / 2; ++bin)
                        {
                            const double hz = bin * kSampleRate / n;
                            if (hz >= fromHz && hz <= toHz) energy += (double) data[(size_t) 2 * bin] * data[(size_t) 2 * bin] + (double) data[(size_t) 2 * bin + 1] * data[(size_t) 2 * bin + 1];
                        }
                    }
                    return 10.0 * std::log10 (energy + 1.0e-12);
                };

                auto renderWith = [&] (float eqAmount)
                {
                    MasteringChain chain;
                    chain.prepare (kSampleRate, 2, 512);
                    chain.setReferenceTrack (reference, kSampleRate);
                    chain.setCurrentTrackAnalysis (current, kSampleRate);
                    auto s = active (-0.3f, 0.0f, -23.0f);
                    s.eqMatchAmount = eqAmount;
                    chain.setSettings (s);
                    auto b = in;
                    for (int pos = 0; pos + 512 <= b.getNumSamples(); pos += 512)
                    {
                        juce::AudioBuffer<float> piece (b.getArrayOfWritePointers(), 2, pos, 512);
                        chain.processBlock (piece);
                    }
                    return b;
                };

                const auto flat = renderWith (0.0f), matched = renderWith (1.0f);
                const double lift = bandEnergy (matched, 9000.0, 9600.0) - bandEnergy (flat, 9000.0, 9600.0);
                const double away = bandEnergy (matched, 1500.0, 2500.0) - bandEnergy (flat, 1500.0, 2500.0);
                // The loudness trim holds the overall level, so both bands move together; what the EQ does is the difference.
                expectWithinAbsoluteError (lift - away, 12.0, 2.0, "the 9.3 kHz band ends up about the 12 dB limit above a band well away (got " + juce::String (lift - away, 1) + " dB)");
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
