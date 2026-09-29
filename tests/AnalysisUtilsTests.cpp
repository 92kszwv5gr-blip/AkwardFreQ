#include <juce_core/juce_core.h>
#include "TestUtils.h"
#include "separation/AnalysisUtils.h"

namespace afq
{
    struct AnalysisUtilsTests : juce::UnitTest
    {
        AnalysisUtilsTests() : juce::UnitTest ("AnalysisUtils", "AkwardFreQ") {}

        void runTest() override
        {
            using namespace test;

            beginTest ("computeRms");
            {
                std::vector<float> constant (1000, 0.5f);
                expectWithinAbsoluteError (computeRms (constant.data(), 1000), 0.5f, 1.0e-6f, "constant signal RMS equals its level");
                const float zero = computeRms (constant.data(), 0);
                expect (std::isfinite (zero) && zero == 0.0f, "zero samples must give 0, not NaN");
            }

            beginTest ("mixToMono averages the channels");
            {
                juce::AudioBuffer<float> stereo (2, 4);
                for (int i = 0; i < 4; ++i) { stereo.setSample (0, i, 0.4f); stereo.setSample (1, i, 0.2f); }
                auto mono = mixToMono (stereo);
                expectEquals (mono.getNumChannels(), 1);
                expectEquals (mono.getNumSamples(), 4);
                expectWithinAbsoluteError (mono.getSample (0, 2), 0.3f, 1.0e-6f, "(0.4 + 0.2) / 2");

                for (int i = 0; i < 4; ++i) stereo.setSample (1, i, -0.4f);
                expectWithinAbsoluteError (mixToMono (stereo).getSample (0, 1), 0.0f, 1.0e-6f, "opposite-phase channels cancel");
            }

            beginTest ("findPeakIndex");
            {
                std::vector<float> d (100, 0.1f);
                d[37] = -0.9f;
                expectEquals (findPeakIndex (d.data(), 100), 37, "finds the largest absolute value, including negative peaks");
            }

            beginTest ("estimatePitch finds the frequency of a sine");
            for (double hz : { 110.0, 220.0, 440.0, 880.0 })
            {
                auto b = silence (1, 4096);
                addSine (b, 0, 4096, hz, 0.5f);
                const auto p = estimatePitch (b.getReadPointer (0), 4096, kSampleRate);
                expectWithinAbsoluteError (p.hz, (float) hz, (float) (hz * 0.02), "estimated Hz for a " + juce::String (hz) + " Hz sine");
                expectGreaterThan (p.confidence, 0.5f, "a clean sine is confidently pitched");
            }

            beginTest ("estimatePitch is not confident about noise or silence");
            {
                auto noise = whiteNoise (1, 4096, 0.5f, 7);
                expectLessThan (estimatePitch (noise.getReadPointer (0), 4096, kSampleRate).confidence, 0.3f, "white noise has no pitch");
                auto quiet = silence (1, 4096);
                const auto p = estimatePitch (quiet.getReadPointer (0), 4096, kSampleRate);
                expect (p.hz == 0.0f || p.confidence < 0.3f, "silence is not a pitched signal");
            }

            beginTest ("detectOnsets finds every hit and nothing else");
            {
                // Checks that each hit has an onset within 25 ms (onsets sit at the start of the analysis frame, up to one
                // frame before the transient) and that there are no other onsets.
                auto expectOnlyHits = [this] (const juce::AudioBuffer<float>& b, const std::vector<double>& hitTimes, const juce::String& what)
                {
                    const auto onsets = detectOnsets (b, kSampleRate);
                    int matched = 0;
                    for (double t : hitTimes)
                    {
                        bool found = false;
                        for (auto o : onsets) if (std::abs ((double) o - (double) samplesFor (t)) <= 0.025 * kSampleRate) found = true;
                        matched += found ? 1 : 0;
                    }
                    expectEquals (matched, (int) hitTimes.size(), what + ": every hit has an onset");
                    expectEquals ((int) onsets.size(), matched, what + ": no onsets other than the hits");
                    expect (std::is_sorted (onsets.begin(), onsets.end()), what + ": onsets are ascending");
                };

                auto hitsAt = [] (double seconds, const std::vector<double>& times, float amp, float floorDb)
                {
                    auto b = silence (1, samplesFor (seconds));
                    for (double t : times) addDecayingHit (b, samplesFor (t), samplesFor (0.2), 100.0, amp, 0.05);
                    return floorDb > -200.0f ? withNoiseFloor (b, floorDb) : b;
                };

                const std::vector<double> four { 0.25, 0.75, 1.25, 1.75 };
                expectOnlyHits (hitsAt (2.5, four, 0.8f, -50.0f), four, "4 hits, -50 dBFS noise floor");
                expectOnlyHits (hitsAt (2.5, four, 0.8f, -70.0f), four, "4 hits, -70 dBFS noise floor");
                expectOnlyHits (hitsAt (2.5, four, 0.8f, -300.0f), four, "4 hits between digital silence");
                expectOnlyHits (hitsAt (2.5, four, 0.01f, -70.0f), four, "quiet hits (-40 dBFS) on a -70 dBFS floor");
                expectOnlyHits (hitsAt (2.5, four, 0.001f, -80.0f), four, "very quiet hits (-60 dBFS) on a -80 dBFS floor");
                expectOnlyHits (hitsAt (8.5, { 0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5 }, 0.7f, -60.0f), { 0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5 }, "8 hits over 8 s");
                expectOnlyHits (hitsAt (8.0, { 0.3, 6.5 }, 0.8f, -50.0f), { 0.3, 6.5 }, "two hits 6 s apart, noise floor in between");
                expectOnlyHits (hitsAt (7.0, { 3.0 }, 0.5f, -60.0f), { 3.0 }, "one hit with 3 s of noise floor either side");

                {   // a fade-out: each hit 3 dB quieter than the last, the last 21 dB below the first
                    auto b = silence (1, samplesFor (5.0));
                    std::vector<double> times;
                    for (int i = 0; i < 8; ++i)
                    {
                        addDecayingHit (b, samplesFor (0.3 + 0.5 * i), samplesFor (0.2), 100.0, 0.8f * std::pow (0.7f, (float) i), 0.05);
                        times.push_back (0.3 + 0.5 * i);
                    }
                    expectOnlyHits (withNoiseFloor (b, -70.0f), times, "fade-out train");
                }
                {   // noise-burst hi-hats
                    auto b = silence (1, samplesFor (4.0));
                    std::vector<double> times;
                    for (int i = 0; i < 8; ++i)
                    {
                        const auto burst = whiteNoise (1, samplesFor (0.05), 0.4f, 5u + (unsigned) i);
                        for (int k = 0; k < burst.getNumSamples(); ++k)
                            b.setSample (0, samplesFor (0.25 + 0.5 * i) + k, burst.getSample (0, k) * std::exp (-(float) k / (0.01f * (float) kSampleRate)));
                        times.push_back (0.25 + 0.5 * i);
                    }
                    expectOnlyHits (withNoiseFloor (b, -60.0f), times, "noise-burst hats");
                }
                {   // kicks over a sustained chord
                    auto b = silence (1, samplesFor (4.0));
                    std::vector<double> times;
                    for (double f : { 220.0, 277.18, 329.63 }) addSine (b, 0, samplesFor (4.0), f, 0.12f);
                    for (int i = 0; i < 7; ++i) { addDecayingHit (b, samplesFor (0.3 + 0.5 * i), samplesFor (0.2), 90.0, 0.6f, 0.06); times.push_back (0.3 + 0.5 * i); }
                    expectOnlyHits (withNoiseFloor (b, -60.0f), times, "kicks over a sustained chord");
                }

                expect (detectOnsets (silence (1, samplesFor (1.0)), kSampleRate).empty(), "digital silence has no onsets");
                expect (detectOnsets (whiteNoise (1, samplesFor (5.0), 0.1f, 3u), kSampleRate).empty(), "stationary noise at -20 dBFS has no onsets");
                expect (detectOnsets (whiteNoise (1, samplesFor (5.0), 0.003f, 4u), kSampleRate).empty(), "stationary noise at -50 dBFS has no onsets");
                expect (detectOnsets (silence (1, 100), kSampleRate).empty(), "a buffer shorter than one analysis frame has no onsets");
            }

            beginTest ("detectOnsets settings");
            {
                // Noise-floor ripple is what the relative floor exists to reject: turning it off must let much of it through.
                auto b = silence (1, samplesFor (2.5));
                for (double t : { 0.25, 0.75, 1.25, 1.75 }) addDecayingHit (b, samplesFor (t), samplesFor (0.2), 100.0, 0.8f, 0.05);
                b = withNoiseFloor (b, -50.0f);
                const auto withFloor = detectOnsets (b, kSampleRate).size();
                const auto withoutFloor = detectOnsets (b, kSampleRate, 10, 256, 6.0f, 512, 0.0f).size();
                expectGreaterThan ((int) withoutFloor, (int) withFloor, "the relative floor removes onsets");

                // The minimum gap is honoured.
                const auto tight = detectOnsets (b, kSampleRate, 10, 256, 1.5f, 8192);
                for (size_t i = 1; i < tight.size(); ++i)
                    expectGreaterOrEqual ((int) (tight[i] - tight[i - 1]), 8192, "onsets are at least the minimum gap apart");

                // Reported positions lie inside the buffer.
                for (auto o : detectOnsets (b, kSampleRate)) expect (o >= 0 && o < b.getNumSamples(), "onset lies inside the buffer");
            }

            beginTest ("estimateBpmFromOnsets");
            for (double bpm : { 120.0, 128.0, 140.0 })
            {
                std::vector<int64_t> onsets;
                for (int beat = 0; beat < 32; ++beat) onsets.push_back ((int64_t) std::llround (beat * 60.0 / bpm * kSampleRate));
                expectWithinAbsoluteError (estimateBpmFromOnsets (onsets, kSampleRate), bpm, 1.0, "tempo of an evenly spaced onset train at " + juce::String (bpm));
            }
        }
    };

    static AnalysisUtilsTests analysisUtilsTests;
}
