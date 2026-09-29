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

            beginTest ("detectOnsets finds every hit");
            {
                auto clean = silence (1, samplesFor (2.5));
                const std::vector<double> hitTimes { 0.25, 0.75, 1.25, 1.75 };
                for (double t : hitTimes) addDecayingHit (clean, samplesFor (t), samplesFor (0.2), 100.0, 0.8f, 0.05);
                const auto b = withNoiseFloor (clean, -50.0f); // stems always carry some noise

                // Recall is good: every hit has an onset within 20 ms of it, even though extra onsets are reported too.
                const auto onsets = detectOnsets (b, kSampleRate);
                for (double t : hitTimes)
                {
                    bool found = false;
                    for (auto o : onsets) if (std::abs ((double) o - (double) samplesFor (t)) <= 0.02 * kSampleRate) found = true;
                    expect (found, "an onset within 20 ms of the hit at " + juce::String (t) + " s");
                }
                expect (std::is_sorted (onsets.begin(), onsets.end()), "onsets are ascending");
                expect (detectOnsets (silence (1, samplesFor (1.0)), kSampleRate).empty(), "silence has no onsets");

                // Precision is poor. The threshold is median + s x MAD of the novelty curve with no absolute floor, so
                // ripples in decay tails and in the noise floor count as onsets. Measured here: 4 hits give 38 onsets at
                // the default sensitivity (1.5) and still 10 at sensitivity 8; hits separated by digital silence give 45.
                // DrumSlicer and AudioToMidiConverter use the defaults, so both over-segment. Proposed fix: a higher
                // default plus an absolute or relative floor, validated on real audio.
                const auto atDefaults = onsets.size();
                knownIssue (*this, atDefaults == hitTimes.size(),
                            "default settings find " + juce::String ((int) atDefaults) + " onsets for " + juce::String ((int) hitTimes.size()) + " hits at a -50 dBFS noise floor");
                const auto raised = detectOnsets (b, kSampleRate, 10, 256, 8.0f).size();
                knownIssue (*this, raised == hitTimes.size(),
                            "sensitivity 8 still finds " + juce::String ((int) raised) + " onsets for " + juce::String ((int) hitTimes.size()) + " hits");
                const auto digitalSilence = detectOnsets (clean, kSampleRate).size();
                knownIssue (*this, digitalSilence == hitTimes.size(),
                            "hits separated by digital silence give " + juce::String ((int) digitalSilence) + " onsets, not " + juce::String ((int) hitTimes.size()));
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
