#include <juce_core/juce_core.h>
#include "TestUtils.h"
#include "mastering/LoudnessMeter.h"

namespace afq
{
    struct LoudnessMeterTests : juce::UnitTest
    {
        LoudnessMeterTests() : juce::UnitTest ("LoudnessMeter", "AkwardFreQ") {}

        // A sine at `dbfs` (peak level) in every channel.
        static juce::AudioBuffer<float> tone (int channels, double seconds, double hz, float dbfs, double sampleRate = test::kSampleRate)
        {
            auto b = test::silence (channels, test::samplesFor (seconds, sampleRate));
            test::addSine (b, 0, b.getNumSamples(), hz, juce::Decibels::decibelsToGain (dbfs), sampleRate);
            return b;
        }

        static float integrated (const juce::AudioBuffer<float>& b, double sampleRate = test::kSampleRate)
        {
            LoudnessMeter meter;
            meter.prepare (sampleRate, b.getNumChannels());
            return meter.measureIntegratedLoudness (b);
        }

        // Two buffers with the same channel count, one after the other.
        static juce::AudioBuffer<float> concat (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
        {
            juce::AudioBuffer<float> out (a.getNumChannels(), a.getNumSamples() + b.getNumSamples());
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
            {
                out.copyFrom (ch, 0, a, ch, 0, a.getNumSamples());
                out.copyFrom (ch, a.getNumSamples(), b, ch, 0, b.getNumSamples());
            }
            return out;
        }

        void runTest() override
        {
            using namespace test;

            beginTest ("K-weighting filters equal the coefficients published in BS.1770-4 at 48 kHz");
            {
                const auto c = LoudnessMeter::kWeightingCoefficients (48000.0);
                constexpr double tol = 1.0e-9;
                expectWithinAbsoluteError (c[0].b0,  1.53512485958697, tol, "shelf b0");
                expectWithinAbsoluteError (c[0].b1, -2.69169618940638, tol, "shelf b1");
                expectWithinAbsoluteError (c[0].b2,  1.19839281085285, tol, "shelf b2");
                expectWithinAbsoluteError (c[0].a1, -1.69065929318241, tol, "shelf a1");
                expectWithinAbsoluteError (c[0].a2,  0.73248077421585, tol, "shelf a2");
                expectWithinAbsoluteError (c[1].b0,  1.0, tol, "high-pass b0");
                expectWithinAbsoluteError (c[1].b1, -2.0, tol, "high-pass b1");
                expectWithinAbsoluteError (c[1].b2,  1.0, tol, "high-pass b2");
                expectWithinAbsoluteError (c[1].a1, -1.99004745483398, tol, "high-pass a1");
                expectWithinAbsoluteError (c[1].a2,  0.99007225036621, tol, "high-pass a2");
            }

            beginTest ("EBU Tech 3341 reference: stereo 1 kHz sine at -23 dBFS reads -23.0 LUFS");
            {
                expectWithinAbsoluteError (integrated (tone (2, 20.0, 1000.0, -23.0f)), -23.0f, 0.1f, "stereo, -23 dBFS");
                expectWithinAbsoluteError (integrated (tone (2, 20.0, 1000.0, -20.0f)), -20.0f, 0.1f, "stereo, -20 dBFS");
            }

            beginTest ("channels are summed in power, not averaged");
            {
                const float mono = integrated (tone (1, 10.0, 1000.0, -20.0f));
                const float stereo = integrated (tone (2, 10.0, 1000.0, -20.0f));
                expectWithinAbsoluteError (stereo - mono, 3.01f, 0.05f, "the same signal in two channels is 3.01 LU louder than in one");
                expectWithinAbsoluteError (mono, -23.0f, 0.1f, "a mono sine at -20 dBFS reads -23.0 LUFS");

                auto oneSided = tone (2, 10.0, 1000.0, -20.0f);
                oneSided.clear (1, 0, oneSided.getNumSamples());
                expectWithinAbsoluteError (integrated (oneSided), mono, 0.01f, "a silent second channel adds nothing");
            }

            beginTest ("level and sample-rate behaviour");
            {
                expectWithinAbsoluteError (integrated (tone (2, 10.0, 1000.0, -14.0f)) - integrated (tone (2, 10.0, 1000.0, -20.0f)), 6.0f, 0.05f, "6 dB more is 6 LU more");
                for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
                    expectWithinAbsoluteError (integrated (tone (2, 10.0, 1000.0, -23.0f, sr), sr), -23.0f, 0.1f, "1 kHz at -23 dBFS reads -23 LUFS at " + juce::String (sr) + " Hz");
            }

            beginTest ("K-weighting frequency response");
            {
                const float ref = integrated (tone (1, 10.0, 1000.0, -20.0f));
                const float high = integrated (tone (1, 10.0, 10000.0, -20.0f));
                const float low100 = integrated (tone (1, 10.0, 100.0, -20.0f));
                const float low50 = integrated (tone (1, 10.0, 50.0, -20.0f));
                // Expected values are the response of the published filters relative to 1 kHz, worked out independently
                // in Python from the 48 kHz coefficients (44.1 kHz differs by under 0.01 dB).
                expectWithinAbsoluteError (high - ref, 3.34f, 0.05f, "10 kHz is 3.34 dB above 1 kHz");
                expectWithinAbsoluteError (low100 - ref, -1.83f, 0.05f, "100 Hz is 1.83 dB below 1 kHz");
                expectWithinAbsoluteError (low50 - ref, -4.63f, 0.05f, "50 Hz is 4.63 dB below 1 kHz");
            }

            beginTest ("gating");
            {
                // Two levels of tone. The quiet part is more than 10 LU below the loud one, so the relative gate drops it.
                const float loudAlone = integrated (tone (2, 20.0, 1000.0, -23.0f));
                const auto gated = concat (tone (2, 20.0, 1000.0, -36.0f), tone (2, 20.0, 1000.0, -23.0f));
                expectWithinAbsoluteError (integrated (gated), loudAlone, 0.1f, "a part 13 LU quieter is gated out (EBU Tech 3341 case 3)");

                // Within 10 LU nothing is gated: the result is the power mean of the two.
                const auto close = concat (tone (2, 20.0, 1000.0, -29.0f), tone (2, 20.0, 1000.0, -23.0f));
                const double expectedPower = 0.5 * (std::pow (10.0, -29.0 / 10.0) + std::pow (10.0, -23.0 / 10.0));
                expectWithinAbsoluteError (integrated (close), (float) (10.0 * std::log10 (expectedPower)), 0.1f, "levels within 10 LU are power-averaged");

                expectLessOrEqual (integrated (tone (2, 20.0, 1000.0, -85.0f)), -69.9f, "a signal below the -70 LUFS absolute gate reads -70");
            }

            beginTest ("edge cases");
            {
                expectEquals (integrated (silence (2, samplesFor (2.0))), -70.0f, "silence reads -70");
                expectEquals (integrated (silence (2, 100)), -70.0f, "input shorter than one block reads -70");
                expectEquals (integrated (silence (2, 0)), -70.0f, "empty input reads -70");
                expect (std::isfinite (integrated (tone (2, 0.39, 1000.0, -10.0f))), "just under one block stays finite");
                expect (std::isfinite (integrated (tone (2, 0.4, 1000.0, 6.0f))), "an over-full-scale signal stays finite");
            }

            beginTest ("short-term measurement");
            {
                LoudnessMeter meter;
                meter.prepare (kSampleRate, 2);
                const auto steady = tone (2, 4.0, 1000.0, -23.0f);
                expectWithinAbsoluteError (meter.measureShortTermLoudness (steady), -23.0f, 0.05f, "a steady tone reads the same as integrated");
                expectEquals (meter.measureShortTermLoudness (silence (2, 0)), -70.0f, "an empty block reads -70");
                expectLessThan (meter.measureShortTermLoudness (silence (2, 512)), -60.0f, "a silent block reads very low");

                // Blocks as short as a host buffer: the filters start from rest, so allow a little more.
                auto block = tone (2, 0.1, 1000.0, -23.0f);
                expectWithinAbsoluteError (meter.measureShortTermLoudness (block), -23.0f, 0.3f, "a 100 ms block");
            }
        }
    };

    static LoudnessMeterTests loudnessMeterTests;
}
