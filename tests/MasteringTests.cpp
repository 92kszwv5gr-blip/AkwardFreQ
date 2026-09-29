#include <juce_core/juce_core.h>
#include "TestUtils.h"
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

            beginTest ("bypass leaves the audio untouched");
            {
                const auto in = testMix();
                MasteringChain::Settings s;
                s.bypass = true;
                const auto out = render (s, in);
                bool identical = true;
                for (int ch = 0; ch < 2 && identical; ++ch)
                    for (int i = 0; i < in.getNumSamples(); ++i)
                        if (out.getSample (ch, i) != in.getSample (ch, i)) { identical = false; break; }
                expect (identical, "output is bit-identical to the input");
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
                // juce::dsp::Limiter compresses above its threshold and then hard-clips at 0 dBFS, not at the
                // threshold. So "Limiter Ceiling -6 dB" is a threshold: peaks can still reach 0 dBFS.
                const auto in = whiteNoise (2, samplesFor (2.0), 1.0f, 5);
                for (float ceilingDb : { -1.0f, -6.0f })
                {
                    const float peak = peakOf (render (active (ceilingDb, 0.0f), in), samplesFor (0.5));
                    knownIssue (*this, peak <= juce::Decibels::decibelsToGain (ceilingDb) * 1.05f,
                                "peak " + juce::String (toDb (peak), 1) + " dBFS with the ceiling set to " + juce::String (ceilingDb) + " dB");
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
                        if (a.getSample (ch, i) != b.getSample (ch, i)) { identical = false; break; }
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

            beginTest ("loudness meter");
            {
                LoudnessMeter meter;
                meter.prepare (kSampleRate, 2);

                auto tone = [] (float amp) { auto b = silence (2, samplesFor (4.0)); addSine (b, 0, b.getNumSamples(), 1000.0, amp); return b; };
                const float quiet = meter.measureIntegratedLoudness (tone (0.1f));
                const float loud = meter.measureIntegratedLoudness (tone (0.2f));
                expectWithinAbsoluteError (loud - quiet, 6.02f, 0.2f, "doubling the amplitude adds 6 dB");
                // BS.1770 sums the channel powers, so a stereo 1 kHz sine at -20 dBFS should read about -20.7 LUFS.
                // The meter averages the channels instead, so it reads about 3 dB low for stereo (mono is unaffected).
                knownIssue (*this, std::abs (quiet - (-20.7f)) < 1.5f,
                            "stereo 1 kHz sine at -20 dBFS reads " + juce::String (quiet, 2) + " LUFS, BS.1770 says about -20.7");
                expectLessThan (meter.measureIntegratedLoudness (silence (2, samplesFor (2.0))), -60.0f, "silence reads far below normal levels");
                expect (std::isfinite (meter.measureIntegratedLoudness (silence (2, 100))), "very short input stays finite");
            }
        }
    };

    static MasteringTests masteringTests;
}
