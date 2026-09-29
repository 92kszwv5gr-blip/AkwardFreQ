// Scores the onset detector against real drum recordings that come with MIDI ground truth, such as the Groove MIDI
// Dataset or E-GMD from Google Magenta. Not part of the automated suite (it needs a dataset on disk):
//
//   AkwardFreQBenchmark <dataset folder> [--limit N] [--tolerance-ms 50]
//
// Every .wav with a .mid / .midi of the same name beside it is a case. Ground truth is the MIDI note-on times, with
// hits closer together than 30 ms merged (a kick and a hat struck together are one onset). A detected onset is
// correct when it lies within the tolerance of an unmatched ground-truth onset (the MIREX rule: precision, recall,
// F-measure). Onsets are placed at the start of the analysis frame, up to one frame (23 ms) before the transient, so
// the timing offset is reported too.
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "separation/AnalysisUtils.h"

namespace
{
    struct Config
    {
        const char* name;
        float sensitivity;
        float relativeFloor;
        int fftOrder = 10;
        int hop = 256;
        int minGap = 512;
    };

    struct Tally
    {
        long detected = 0, truth = 0, matched = 0;
        std::vector<double> offsetsMs; // detected minus truth, for matched onsets
        std::vector<double> fPerFile;
    };

    std::vector<double> groundTruth (const juce::File& midiFile)
    {
        juce::FileInputStream in (midiFile);
        juce::MidiFile midi;
        if (! in.openedOk() || ! midi.readFrom (in)) return {};
        midi.convertTimestampTicksToSeconds();

        std::vector<double> times;
        for (int t = 0; t < midi.getNumTracks(); ++t)
        {
            auto* track = midi.getTrack (t);
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                const auto& m = track->getEventPointer (i)->message;
                if (m.isNoteOn()) times.push_back (m.getTimeStamp());
            }
        }
        std::sort (times.begin(), times.end());

        std::vector<double> merged;
        for (double t : times)
            if (merged.empty() || t - merged.back() >= 0.030) merged.push_back (t);
        return merged;
    }

    // Greedy one-to-one matching of sorted lists within +-tolerance.
    void score (const std::vector<double>& truth, const std::vector<double>& detected, double tol, Tally& tally, double& fOut)
    {
        std::vector<bool> used (truth.size(), false);
        long matched = 0;
        for (double d : detected)
        {
            int best = -1;
            double bestDist = tol;
            for (size_t i = 0; i < truth.size(); ++i)
            {
                if (used[i]) continue;
                const double dist = std::abs (d - truth[i]);
                if (dist <= bestDist) { bestDist = dist; best = (int) i; }
            }
            if (best >= 0)
            {
                used[(size_t) best] = true;
                ++matched;
                tally.offsetsMs.push_back ((d - truth[(size_t) best]) * 1000.0);
            }
        }
        tally.detected += (long) detected.size();
        tally.truth += (long) truth.size();
        tally.matched += matched;
        const double p = detected.empty() ? 0.0 : (double) matched / (double) detected.size();
        const double r = truth.empty() ? 0.0 : (double) matched / (double) truth.size();
        fOut = (p + r) > 0.0 ? 2.0 * p * r / (p + r) : 0.0;
    }

    double percentile (std::vector<double> v, double q)
    {
        if (v.empty()) return 0.0;
        std::sort (v.begin(), v.end());
        return v[(size_t) juce::jlimit (0, (int) v.size() - 1, (int) std::lround (q * (double) (v.size() - 1)))];
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit; // AudioFormatManager wants a message thread context on some platforms
    juce::File root;
    int limit = 0;
    double toleranceMs = 50.0;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--limit" && i + 1 < argc) limit = juce::String (argv[++i]).getIntValue();
        else if (a == "--tolerance-ms" && i + 1 < argc) toleranceMs = juce::String (argv[++i]).getDoubleValue();
        else root = juce::File (a);
    }
    if (! root.isDirectory())
    {
        std::cerr << "usage: AkwardFreQBenchmark <dataset folder> [--limit N] [--tolerance-ms 50]\n";
        return 2;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    auto wavs = root.findChildFiles (juce::File::findFiles, true, "*.wav");
    std::sort (wavs.begin(), wavs.end(), [] (const juce::File& a, const juce::File& b) { return a.getFullPathName() < b.getFullPathName(); });
    if (limit > 0 && (int) wavs.size() > limit)
    {
        // An even spread over the sorted list, so a small run still covers every drummer and style.
        juce::Array<juce::File> picked;
        for (int i = 0; i < limit; ++i) picked.add (wavs[(int) ((double) i * (double) wavs.size() / (double) limit)]);
        wavs = picked;
    }

    // "previous" is the old default (no relative floor, sensitivity 1.5), on the same log-compressed flux: it shows what
    // the floor and the sensitivity are worth, not the exact pre-fix code.
    const std::vector<Config> configs {
        { "current defaults (sens 6, floor 0.15)", 6.0f, 0.15f },
        { "no relative floor (sens 6, floor 0)", 6.0f, 0.0f },
        { "old defaults (sens 1.5, floor 0)", 1.5f, 0.0f },
        { "sens 3, floor 0.15", 3.0f, 0.15f },
        { "sens 10, floor 0.15", 10.0f, 0.15f },
        { "sens 6, floor 0.05", 6.0f, 0.05f },
        { "sens 6, floor 0.30", 6.0f, 0.30f },
        { "window 512, hop 128", 6.0f, 0.15f, 9, 128, 512 },
        { "window 512, hop 128, gap 256", 6.0f, 0.15f, 9, 128, 256 },
        { "window 2048, hop 256", 6.0f, 0.15f, 11, 256, 512 },
        { "window 1024, hop 128", 6.0f, 0.15f, 10, 128, 512 },
        { "window 512, hop 128, sens 3", 3.0f, 0.15f, 9, 128, 512 },
        { "window 512, hop 128, floor 0.05", 6.0f, 0.05f, 9, 128, 512 },
    };
    std::vector<Tally> tallies (configs.size());

    int used = 0, skipped = 0;
    double hours = 0.0;
    for (const auto& wav : wavs)
    {
        juce::File midiFile = wav.withFileExtension ("mid");
        if (! midiFile.existsAsFile()) midiFile = wav.withFileExtension ("midi");
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (wav));
        const auto truth = midiFile.existsAsFile() ? groundTruth (midiFile) : std::vector<double>();
        if (reader == nullptr || truth.empty()) { ++skipped; continue; }

        juce::AudioBuffer<float> audio ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&audio, 0, (int) reader->lengthInSamples, 0, true, true);
        const auto mono = afq::mixToMono (audio);
        hours += (double) reader->lengthInSamples / reader->sampleRate / 3600.0;

        for (size_t c = 0; c < configs.size(); ++c)
        {
            const auto onsets = afq::detectOnsets (mono, reader->sampleRate, configs[c].fftOrder, configs[c].hop, configs[c].sensitivity, configs[c].minGap, configs[c].relativeFloor);
            std::vector<double> detected;
            for (auto o : onsets) detected.push_back ((double) o / reader->sampleRate);
            double f = 0.0;
            score (truth, detected, toleranceMs / 1000.0, tallies[c], f);
            tallies[c].fPerFile.push_back (f);
        }
        ++used;
    }

    std::cout << "files scored: " << used << " (" << skipped << " skipped), " << juce::String (hours, 2) << " hours of audio, tolerance +-"
              << toleranceMs << " ms\n\n";
    std::cout << "configuration                              precision  recall  F1     median F1/file  detected/truth  timing offset ms (median, 5th..95th)\n";
    for (size_t c = 0; c < configs.size(); ++c)
    {
        const auto& t = tallies[c];
        const double p = t.detected ? (double) t.matched / (double) t.detected : 0.0;
        const double r = t.truth ? (double) t.matched / (double) t.truth : 0.0;
        const double f = (p + r) > 0 ? 2 * p * r / (p + r) : 0.0;
        printf ("%-42s %.3f      %.3f   %.3f  %.3f           %.2f            %+.1f (%+.1f..%+.1f)\n", configs[c].name, p, r, f,
                percentile (t.fPerFile, 0.5), t.truth ? (double) t.detected / (double) t.truth : 0.0,
                percentile (t.offsetsMs, 0.5), percentile (t.offsetsMs, 0.05), percentile (t.offsetsMs, 0.95));
    }
    return used > 0 ? 0 : 1;
}
