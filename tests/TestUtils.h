#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <random>
#include <vector>

// Helpers for building deterministic test signals. Everything is seeded, so a failure reproduces exactly.
namespace afq::test
{
    constexpr double kSampleRate = 44100.0;

    inline int samplesFor (double seconds, double sampleRate = kSampleRate) { return (int) std::llround (seconds * sampleRate); }

    inline juce::AudioBuffer<float> silence (int channels, int numSamples)
    {
        juce::AudioBuffer<float> b (channels, numSamples);
        b.clear();
        return b;
    }

    // Adds a sine starting at `startSample` (phase 0, so the onset is abrupt) to every channel.
    inline void addSine (juce::AudioBuffer<float>& b, int startSample, int numSamples, double hz, float amp,
                         double sampleRate = kSampleRate)
    {
        const int end = std::min (b.getNumSamples(), startSample + numSamples);
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            float* d = b.getWritePointer (ch);
            for (int i = startSample; i < end; ++i)
                d[i] += amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * (double) (i - startSample) / sampleRate);
        }
    }

    // A percussive hit: sine with an exponential decay and an instant attack.
    inline void addDecayingHit (juce::AudioBuffer<float>& b, int startSample, int lengthSamples, double hz, float amp,
                                double decaySeconds, double sampleRate = kSampleRate)
    {
        const int end = std::min (b.getNumSamples(), startSample + lengthSamples);
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            float* d = b.getWritePointer (ch);
            for (int i = startSample; i < end; ++i)
            {
                const double t = (double) (i - startSample) / sampleRate;
                d[i] += amp * (float) (std::exp (-t / decaySeconds) * std::sin (2.0 * juce::MathConstants<double>::pi * hz * t));
            }
        }
    }

    inline juce::AudioBuffer<float> whiteNoise (int channels, int numSamples, float amp, unsigned seed)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
        juce::AudioBuffer<float> b (channels, numSamples);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                b.setSample (ch, i, amp * dist (rng));
        return b;
    }

    // Real audio never contains exact digital silence between events; a low noise floor makes test signals
    // behave like real stems. Default -70 dBFS.
    inline juce::AudioBuffer<float> withNoiseFloor (juce::AudioBuffer<float> b, float dbfs = -70.0f, unsigned seed = 99)
    {
        const auto noise = whiteNoise (1, b.getNumSamples(), juce::Decibels::decibelsToGain (dbfs), seed); // same noise on every channel
        for (int ch = 0; ch < b.getNumChannels(); ++ch) b.addFrom (ch, 0, noise, 0, 0, b.getNumSamples());
        return b;
    }

    // Marks behaviour that is known to be wrong today, so the suite stays green without hiding the problem.
    // While the behaviour is still wrong it logs a note and counts nothing. The moment it starts holding, the
    // check FAILS and asks for the marker to be replaced with a normal expect, so a fix cannot go unnoticed.
    inline void knownIssue (juce::UnitTest& t, bool desiredBehaviourHolds, const juce::String& description)
    {
        if (desiredBehaviourHolds) t.expect (false, "KNOWN ISSUE NOW FIXED - turn this into a normal check: " + description);
        else t.logMessage ("  KNOWN ISSUE (not counted): " + description);
    }

    inline float peakOf (const juce::AudioBuffer<float>& b, int start = 0, int end = -1)
    {
        if (end < 0) end = b.getNumSamples();
        float p = 0.0f;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = start; i < end; ++i)
                p = std::max (p, std::abs (b.getSample (ch, i)));
        return p;
    }

    inline double rmsOf (const juce::AudioBuffer<float>& b, int start = 0, int end = -1)
    {
        if (end < 0) end = b.getNumSamples();
        double sum = 0.0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = start; i < end; ++i)
                sum += (double) b.getSample (ch, i) * b.getSample (ch, i);
        const double n = (double) (end - start) * b.getNumChannels();
        return n > 0 ? std::sqrt (sum / n) : 0.0;
    }

    inline bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i))) return false;
        return true;
    }

    inline double toDb (double linear) { return 20.0 * std::log10 (std::max (linear, 1.0e-12)); }

    // Regression reference: a short list of numbers stored in tests/reference/<name>.txt.
    // It records what the code produced when the reference was made, so it catches unintended change;
    // it does not prove the numbers are right. Regenerate on purpose with AFQ_UPDATE_REFERENCE=1.
    struct Reference
    {
        static juce::File fileFor (const juce::String& name) { return juce::File (AFQ_REFERENCE_DIR).getChildFile (name + ".txt"); }

        static bool updating() { return juce::SystemStats::getEnvironmentVariable ("AFQ_UPDATE_REFERENCE", "0") == "1"; }

        // Returns an empty string when the values match, otherwise a description of the first problem.
        static juce::String compare (const juce::String& name, const std::vector<double>& values, double tolerance)
        {
            const auto file = fileFor (name);

            if (updating())
            {
                juce::String text;
                for (double v : values) text << juce::String (v, 6) << "\n";
                file.getParentDirectory().createDirectory();
                return file.replaceWithText (text) ? juce::String() : "could not write " + file.getFullPathName();
            }

            if (! file.existsAsFile())
                return "reference file missing: " + file.getFullPathName() + " (create it with AFQ_UPDATE_REFERENCE=1)";

            juce::StringArray lines;
            lines.addLines (file.loadFileAsString());
            lines.removeEmptyStrings();
            if ((size_t) lines.size() != values.size())
                return "reference has " + juce::String (lines.size()) + " values, test produced " + juce::String ((int) values.size());

            for (size_t i = 0; i < values.size(); ++i)
                if (std::abs (values[i] - lines[(int) i].getDoubleValue()) > tolerance)
                    return "value " + juce::String ((int) i) + " is " + juce::String (values[i], 4) + ", reference says "
                         + juce::String (lines[(int) i].getDoubleValue(), 4) + " (tolerance " + juce::String (tolerance) + ")";

            return {};
        }
    };
}
