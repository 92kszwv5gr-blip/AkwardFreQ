#include <juce_core/juce_core.h>
#include "TestUtils.h"
#include "export/OneShotCleaner.h"

namespace afq
{
    struct OneShotCleanerTests : juce::UnitTest
    {
        OneShotCleanerTests() : juce::UnitTest ("OneShotCleaner", "AkwardFreQ") {}

        // 0.5 s of silence, a decaying 200 Hz hit, then silence, in a 2 s buffer.
        static juce::AudioBuffer<float> paddedHit (float amp, int channels = 1)
        {
            auto b = test::silence (channels, test::samplesFor (2.0));
            test::addDecayingHit (b, test::samplesFor (0.5), test::samplesFor (0.6), 200.0, amp, 0.05);
            return b;
        }

        void runTest() override
        {
            using namespace test;

            beginTest ("trims the silence around a hit");
            {
                const auto src = paddedHit (0.25f);
                const auto out = OneShotCleaner::clean (src, 0, src.getNumSamples(), kSampleRate);
                const double seconds = (double) out.getNumSamples() / kSampleRate;
                // The hit stays above the -50 dB threshold for about 0.29 s; plus 2 ms pre-roll and 15 ms post-roll.
                expectGreaterThan (seconds, 0.29, "keeps the whole audible hit");
                expectLessThan (seconds, 0.33, "drops the silence either side");
            }

            beginTest ("normalizes the peak to the target level");
            {
                const auto src = paddedHit (0.25f);
                const auto out = OneShotCleaner::clean (src, 0, src.getNumSamples(), kSampleRate);
                expectWithinAbsoluteError (peakOf (out), juce::Decibels::decibelsToGain (-1.0f), 0.005f, "peak is -1 dBFS");
                expectLessOrEqual (peakOf (out), 1.0f, "never clips");
            }

            beginTest ("fades in and out so the edges are click-free");
            {
                const auto src = paddedHit (0.25f);
                const auto out = OneShotCleaner::clean (src, 0, src.getNumSamples(), kSampleRate);
                expectLessThan (std::abs (out.getSample (0, 0)), 0.001f, "first sample is (near) zero");
                expectLessThan (std::abs (out.getSample (0, out.getNumSamples() - 1)), 0.001f, "last sample is (near) zero");
            }

            beginTest ("caps the boost applied to very quiet input");
            {
                const auto src = paddedHit (0.001f);
                const auto out = OneShotCleaner::clean (src, 0, src.getNumSamples(), kSampleRate);
                const float maxExpected = 0.001f * juce::Decibels::decibelsToGain (24.0f);
                expectLessOrEqual (peakOf (out), maxExpected * 1.1f, "gain is limited to +24 dB");
                expectGreaterThan (peakOf (out), maxExpected * 0.8f, "but the full +24 dB is applied");
            }

            beginTest ("keeps the channel balance");
            {
                auto src = silence (2, samplesFor (1.0));
                addDecayingHit (src, samplesFor (0.2), samplesFor (0.5), 200.0, 0.5f, 0.05);
                for (int i = 0; i < src.getNumSamples(); ++i) src.setSample (1, i, src.getSample (1, i) * 0.5f);
                const auto out = OneShotCleaner::clean (src, 0, src.getNumSamples(), kSampleRate);
                expectEquals (out.getNumChannels(), 2);
                float left = 0.0f, right = 0.0f;
                for (int i = 0; i < out.getNumSamples(); ++i) { left = std::max (left, std::abs (out.getSample (0, i))); right = std::max (right, std::abs (out.getSample (1, i))); }
                expectWithinAbsoluteError (left / right, 2.0f, 0.05f, "left stays twice as loud as right");
            }

            beginTest ("silent input is returned at full length without damage");
            {
                const auto src = silence (1, 1000);
                const auto out = OneShotCleaner::clean (src, 0, 1000, kSampleRate);
                expectEquals (out.getNumSamples(), 1000);
                expect (allFinite (out), "no NaN or infinity");
                expectEquals (peakOf (out), 0.0f);
            }

            beginTest ("empty and out-of-range selections");
            {
                const auto src = paddedHit (0.25f);
                expectEquals (OneShotCleaner::clean (src, 5000, 5000, kSampleRate).getNumSamples(), 0, "empty range");
                expectEquals (OneShotCleaner::clean (src, 9000, 100, kSampleRate).getNumSamples(), 0, "backwards range");
                const auto clamped = OneShotCleaner::clean (src, -1000, src.getNumSamples() + 1000, kSampleRate);
                expectGreaterThan (clamped.getNumSamples(), 0, "range is clamped to the buffer");
                expectEquals (clamped.getNumChannels(), 1);
            }
        }
    };

    static OneShotCleanerTests oneShotCleanerTests;
}
