#include <juce_core/juce_core.h>
#include "TestUtils.h"
#include "separation/DrumSlicer.h"

namespace afq
{
    struct DrumSlicerTests : juce::UnitTest
    {
        DrumSlicerTests() : juce::UnitTest ("DrumSlicer", "AkwardFreQ") {}

        // A kick-like pattern: one hit every `spacing` seconds starting at `first`.
        static juce::AudioBuffer<float> hitTrain (int channels, double seconds, double first, double spacing, std::vector<double>& times,
                                                  bool noiseFloor = true)
        {
            auto b = test::silence (channels, test::samplesFor (seconds));
            for (double t = first; t < seconds - 0.25; t += spacing)
            {
                test::addDecayingHit (b, test::samplesFor (t), test::samplesFor (0.25), 90.0, 0.8f, 0.06);
                times.push_back (t);
            }
            return noiseFloor ? test::withNoiseFloor (b, -50.0f) : b;
        }

        void runTest() override
        {
            using namespace test;

            beginTest ("one slice per hit, starting at the hit");
            for (bool noiseFloor : { true, false }) // stems carry a noise floor; a gated stem is exact digital silence between hits
            {
                std::vector<double> times;
                const auto b = hitTrain (2, 4.5, 0.25, 0.5, times, noiseFloor);
                const auto slices = DrumSlicer::slice (b, kSampleRate);
                const auto what = juce::String (noiseFloor ? "with a -50 dBFS noise floor" : "between digital silence");
                expectEquals ((int) slices.size(), (int) times.size(), "one slice per hit " + what);
                for (size_t i = 0; i < std::min (slices.size(), times.size()); ++i)
                    expectWithinAbsoluteError ((double) slices[i].startSample, times[i] * kSampleRate, 0.025 * kSampleRate,
                                               "slice " + juce::String ((int) i + 1) + " starts at its hit " + what);
            }

            beginTest ("slices are ordered, contiguous and numbered from 1");
            {
                std::vector<double> times;
                const auto b = hitTrain (1, 4.5, 0.25, 0.5, times);
                const auto slices = DrumSlicer::slice (b, kSampleRate, 0, -1, 10000); // no cap, so the tail is not dropped
                expect (! slices.empty(), "found slices");
                for (size_t i = 0; i < slices.size(); ++i)
                {
                    expectEquals (slices[i].index, (int) i + 1);
                    expectGreaterThan ((int) (slices[i].endSample - slices[i].startSample), 0, "no empty slice");
                    if (i > 0) expectEquals ((int) slices[i].startSample, (int) slices[i - 1].endSample, "no gap or overlap");
                }
                expectEquals ((int) slices.back().endSample, b.getNumSamples(), "the last slice runs to the end of the buffer");
            }

            beginTest ("stereo and mono give the same slices");
            {
                std::vector<double> times;
                const auto stereo = hitTrain (2, 3.5, 0.25, 0.5, times);
                std::vector<double> times2;
                const auto mono = hitTrain (1, 3.5, 0.25, 0.5, times2);
                const auto a = DrumSlicer::slice (stereo, kSampleRate);
                const auto c = DrumSlicer::slice (mono, kSampleRate);
                expectEquals ((int) a.size(), (int) c.size());
                for (size_t i = 0; i < std::min (a.size(), c.size()); ++i) expectEquals ((int) a[i].startSample, (int) c[i].startSample);
            }

            beginTest ("only slices inside the requested range");
            {
                std::vector<double> times;
                const auto b = hitTrain (1, 6.5, 0.25, 0.5, times);
                const int64_t from = samplesFor (2.0), to = samplesFor (4.0);
                const auto slices = DrumSlicer::slice (b, kSampleRate, from, to);
                expect (! slices.empty(), "found slices in the range");
                for (const auto& s : slices)
                    expect (s.startSample >= from && s.endSample <= to, "slice " + juce::String (s.index) + " stays inside the range");
            }

            beginTest ("maxSlices caps the count");
            {
                std::vector<double> times;
                const auto b = hitTrain (1, 10.5, 0.25, 0.25, times);
                expectLessOrEqual ((int) DrumSlicer::slice (b, kSampleRate, 0, -1, 5).size(), 5);
                expectGreaterThan ((int) DrumSlicer::slice (b, kSampleRate, 0, -1, 64).size(), 5, "the cap is what limits it, not the input");
            }

            beginTest ("input with no hits becomes a single slice");
            {
                const auto quiet = silence (1, samplesFor (2.0));
                const auto s = DrumSlicer::slice (quiet, kSampleRate);
                expectEquals ((int) s.size(), 1);
                if (! s.empty()) { expectEquals ((int) s[0].startSample, 0); expectEquals ((int) s[0].endSample, quiet.getNumSamples()); }
            }

            beginTest ("degenerate ranges do not crash");
            {
                expect (DrumSlicer::slice (silence (1, 4), kSampleRate).empty(), "fewer than 8 samples gives nothing");
                expect (DrumSlicer::slice (silence (1, 0), kSampleRate).empty(), "empty buffer gives nothing");
                const auto b = silence (1, samplesFor (1.0));
                (void) DrumSlicer::slice (b, kSampleRate, -100, 999999999);
                (void) DrumSlicer::slice (b, kSampleRate, 5000, 100);
                expect (true, "out-of-range arguments were handled");
            }
        }
    };

    static DrumSlicerTests drumSlicerTests;
}
