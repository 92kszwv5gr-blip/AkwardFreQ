#pragma once

#include <juce_dsp/juce_dsp.h>
#include "RegionTypes.h"

namespace afq
{
    // Fixed-size FFT frame analyzer shared by onset detection and the
    // Tier B/C sub-splitters. Not real-time safe by itself when allocating —
    // construct once, reuse across calls (all buffers are pre-allocated).
    class FrameFeatureExtractor
    {
    public:
        explicit FrameFeatureExtractor (int fftOrderIn = 10); // 1024-point FFT default

        int fftSize() const { return fftSize_; }

        // Computes the magnitude spectrum (size fftSize/2+1) of one windowed frame.
        // `frame` must contain fftSize samples (mono). Writes into `outMagnitudes`
        // which must be at least fftSize/2+1 long.
        void computeMagnitudeSpectrum (const float* frame, float* outMagnitudes);

        // Builds the full 12-element FeatureVector for a single onset-bounded segment
        // (used by the Tier B/C sub-splitters, not by detectOnsets which inlines its
        // own frame-to-frame flux). SpectralFlux here is self-contained: it compares
        // an "attack" spectrum (first fftSize samples) against a "tail" spectrum
        // (a window around the segment midpoint) to measure how much the spectral
        // content moves within this one hit — a good proxy for sweep-like content
        // (zaps) vs. static decay (percussive hits) vs. rapid change (glitches).
        // `attackTimeMs`/`decayTimeMs` are computed from the raw segment envelope,
        // not the spectrum, so `segmentSamples`/`numSegmentSamples` (which may be
        // longer than fftSize) are passed separately.
        FeatureVector analyzeSegment (const float* segmentSamples, int numSegmentSamples,
                                       double sampleRate,
                                       float* outMagnitudes /* scratch+output, fftSize/2+1, attack-frame spectrum */);

    private:
        int fftOrder_;
        int fftSize_;
        int numBins_;
        juce::dsp::FFT fft_;
        juce::dsp::WindowingFunction<float> window_;
        std::vector<float> fftScratch_; // 2*fftSize for in-place real FFT
        std::vector<float> tailFrameScratch_;
        std::vector<float> tailMagScratch_;

        float bandEnergyFraction (const float* magnitudes, double sampleRate, float loHz, float hiHz) const;
        float centroid (const float* magnitudes, double sampleRate) const;
        float flatness (const float* magnitudes) const;
        float flux (const float* magnitudes, const float* prevMagnitudes) const;
    };

    // Peak-picks onsets from a mono buffer using spectral-flux novelty with an
    // adaptive (moving-median) threshold. Returns onset positions in samples,
    // ascending, with a minimum inter-onset gap to avoid double-triggers.
    std::vector<int64_t> detectOnsets (const juce::AudioBuffer<float>& monoBuffer,
                                        double sampleRate,
                                        int fftOrder = 10,
                                        int hopSize = 256,
                                        float sensitivity = 1.5f,   // threshold = median + sensitivity * MAD
                                        int minGapSamples = 512);

    // Simple time-domain helpers, used both standalone and inside FrameFeatureExtractor.
    float computeRms (const float* samples, int numSamples);
    float computeZeroCrossingRate (const float* samples, int numSamples);

    // Finds the sample index of the peak absolute value within [0, numSamples).
    int findPeakIndex (const float* samples, int numSamples);

    // Attack time: from segment start to `peakIndex` reaching 90% of peak level.
    float estimateAttackTimeMs (const float* samples, int numSamples, int peakIndex, double sampleRate);

    // Decay time: from `peakIndex` to level falling to -20dB below peak (or end of segment).
    float estimateDecayTimeMs (const float* samples, int numSamples, int peakIndex, double sampleRate);

    // Autocorrelation-based pitch/harmonicity search over the given Hz range.
    // Returns {harmonicity 0-1, pitchConfidence 0-1, estimatedHz}.
    struct PitchEstimate { float harmonicity = 0.0f; float confidence = 0.0f; float hz = 0.0f; };
    PitchEstimate estimatePitch (const float* samples, int numSamples, double sampleRate,
                                  float minHz = 50.0f, float maxHz = 2000.0f);

    // Autocorrelation of an onset-time sequence to estimate tempo (BPM) from a
    // full-length onset envelope. Used by SamplePackExporter for pack metadata.
    double estimateBpmFromOnsets (const std::vector<int64_t>& onsetSamples, double sampleRate,
                                   double minBpm = 110.0, double maxBpm = 150.0);

    // Krumhansl-Schmuckler key estimation from a chroma vector built from the
    // mix's magnitude spectra. Returns e.g. "A minor".
    juce::String estimateKey (const juce::AudioBuffer<float>& monoBuffer, double sampleRate);
}
