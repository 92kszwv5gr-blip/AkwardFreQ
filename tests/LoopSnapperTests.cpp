#include <juce_core/juce_core.h>
#include "TestUtils.h"
#include "midi/LoopSnapper.h"

namespace afq
{
    struct LoopSnapperTests : juce::UnitTest
    {
        LoopSnapperTests() : juce::UnitTest ("LoopSnapper", "AkwardFreQ") {}

        static int64_t barLength (double bpm) { return (int64_t) std::llround (test::kSampleRate * 60.0 / bpm * 4.0); }

        // The seam mismatch LoopSnapper minimises, computed independently here: the 10 ms leading into the
        // start of the loop against the 10 ms leading into its end (those two are adjacent once the loop repeats).
        static double seamCost (const juce::AudioBuffer<float>& b, int64_t start, int64_t end)
        {
            const int w = (int) std::max<int64_t> (64, (int64_t) (test::kSampleRate * 0.010));
            double cost = 0.0;
            for (int i = 0; i < w; ++i)
            {
                double a = 0.0, c = 0.0;
                for (int ch = 0; ch < b.getNumChannels(); ++ch)
                {
                    a += b.getSample (ch, (int) (start - w + i));
                    c += b.getSample (ch, (int) (end - w + i));
                }
                const double d = (a - c) / b.getNumChannels();
                cost += d * d;
            }
            return cost;
        }

        void runTest() override
        {
            using namespace test;
            const double bpm = 120.0;
            const int64_t bar = barLength (bpm);

            beginTest ("without a tempo it returns the clamped rough range");
            {
                const auto b = whiteNoise (1, samplesFor (20.0), 0.3f, 1);
                const auto r = LoopSnapper::snapToLoop (b, kSampleRate, 0.0, 1000, 5000, 4);
                expect (! r.snapped);
                expectEquals ((int) r.startSample, 1000);
                expectEquals ((int) r.endSample, 5000);
                const auto r2 = LoopSnapper::snapToLoop (b, kSampleRate, -5.0, -20, b.getNumSamples() + 999, 4);
                expect (r2.startSample == 0 && r2.endSample == b.getNumSamples(), "out-of-range rough range is clamped to the buffer");
            }

            beginTest ("the loop is exactly the requested number of bars");
            for (int bars : { 1, 2, 4, 8 })
            {
                const auto b = whiteNoise (2, samplesFor (40.0), 0.3f, 2);
                const auto r = LoopSnapper::snapToLoop (b, kSampleRate, bpm, samplesFor (3.3), samplesFor (10.0), bars);
                expect (r.snapped, juce::String (bars) + " bars: snapped");
                expectEquals ((int) (r.endSample - r.startSample), (int) (bar * bars), juce::String (bars) + " bars: length");
                expect (r.startSample >= 0 && r.endSample <= b.getNumSamples(), juce::String (bars) + " bars: inside the buffer");
            }

            beginTest ("the start stays within an eighth of a bar of the bar grid");
            {
                const auto b = whiteNoise (1, samplesFor (40.0), 0.3f, 3);
                for (double roughSeconds : { 0.0, 0.7, 2.9, 5.1, 9.6 })
                {
                    const int64_t rough = samplesFor (roughSeconds);
                    const auto r = LoopSnapper::snapToLoop (b, kSampleRate, bpm, rough, rough + bar * 4, 4);
                    const int64_t grid = (int64_t) std::llround ((double) rough / (double) bar) * bar;
                    expectLessOrEqual ((int) std::abs (r.startSample - grid), (int) (bar / 8), "rough start " + juce::String (roughSeconds) + " s");
                }
            }

            beginTest ("the seam is never worse than the plain bar-grid position");
            {
                // Noise with an exactly repeating stretch, so a better position exists near the grid.
                auto b = whiteNoise (1, samplesFor (40.0), 0.3f, 4);
                const int64_t period = bar * 2;
                const int64_t copyFrom = bar * 3 - 3000;
                for (int64_t i = 0; i < period + 6000; ++i)
                    b.setSample (0, (int) (copyFrom + period + i), b.getSample (0, (int) (copyFrom + i)));

                const auto r = LoopSnapper::snapToLoop (b, kSampleRate, bpm, bar * 3, bar * 5, 2);
                const double atGrid = seamCost (b, bar * 3, bar * 3 + period);
                const double chosen = seamCost (b, r.startSample, r.endSample);
                expectLessOrEqual (chosen, atGrid + 1.0e-9, "chosen seam cost <= grid seam cost");
            }

            beginTest ("when the grid position is already the best, it does not move");
            {
                // Perfectly periodic audio with a period that divides one bar: every position ties at zero cost,
                // so the snapper should stay on the grid instead of drifting to the edge of its search window.
                auto b = silence (1, samplesFor (40.0));
                for (int i = 0; i < b.getNumSamples(); ++i)
                    b.setSample (0, i, 0.4f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 2.0 * (double) (i % (int) bar) / (double) bar));
                const auto r = LoopSnapper::snapToLoop (b, kSampleRate, bpm, bar * 4, bar * 6, 2);
                expectEquals ((int) r.startSample, (int) (bar * 4), "start stays on the bar grid");
            }

            beginTest ("silence also stays on the grid");
            {
                const auto b = silence (1, samplesFor (40.0));
                const auto r = LoopSnapper::snapToLoop (b, kSampleRate, bpm, bar * 3, bar * 5, 2);
                expectEquals ((int) r.startSample, (int) (bar * 3), "start stays on the bar grid");
            }

            beginTest ("a buffer too short for the loop falls back to the rough range");
            {
                const auto b = whiteNoise (1, samplesFor (5.0), 0.3f, 5);
                const auto r = LoopSnapper::snapToLoop (b, kSampleRate, bpm, 2000, 9000, 8);
                expect (! r.snapped);
                expectEquals ((int) r.startSample, 2000);
                expectEquals ((int) r.endSample, 9000);
            }

            beginTest ("a rough start near the end is pulled back so the loop fits");
            {
                const auto b = whiteNoise (1, samplesFor (40.0), 0.3f, 6);
                const auto r = LoopSnapper::snapToLoop (b, kSampleRate, bpm, b.getNumSamples() - 1000, b.getNumSamples(), 4);
                expect (r.snapped);
                expectLessOrEqual ((int) r.endSample, b.getNumSamples());
                expectEquals ((int) (r.endSample - r.startSample), (int) (bar * 4));
            }
        }
    };

    static LoopSnapperTests loopSnapperTests;
}
