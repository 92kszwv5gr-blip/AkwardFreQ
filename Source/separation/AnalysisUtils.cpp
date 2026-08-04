#include "AnalysisUtils.h"
#include <algorithm>
#include <cmath>

namespace afq
{
    //==============================================================================
    FrameFeatureExtractor::FrameFeatureExtractor (int fftOrderIn)
        : fftOrder_ (fftOrderIn),
          fftSize_ (1 << fftOrderIn),
          numBins_ (fftSize_ / 2 + 1),
          fft_ (fftOrderIn),
          window_ (fftSize_, juce::dsp::WindowingFunction<float>::hann)
    {
        fftScratch_.assign (2 * (size_t) fftSize_, 0.0f);
    }

    void FrameFeatureExtractor::computeMagnitudeSpectrum (const float* frame, float* outMagnitudes)
    {
        std::fill (fftScratch_.begin(), fftScratch_.end(), 0.0f);
        std::copy (frame, frame + fftSize_, fftScratch_.begin());
        window_.multiplyWithWindowingTable (fftScratch_.data(), (size_t) fftSize_);
        fft_.performRealOnlyForwardTransform (fftScratch_.data());

        for (int i = 0; i < numBins_; ++i)
        {
            const float re = fftScratch_[(size_t) (2 * i)];
            const float im = fftScratch_[(size_t) (2 * i + 1)];
            outMagnitudes[i] = std::sqrt (re * re + im * im);
        }
    }

    float FrameFeatureExtractor::centroid (const float* magnitudes, double sampleRate) const
    {
        double weightedSum = 0.0, magSum = 0.0;
        for (int i = 0; i < numBins_; ++i)
        {
            const double freq = i * sampleRate / fftSize_;
            weightedSum += freq * magnitudes[i];
            magSum += magnitudes[i];
        }
        if (magSum < 1.0e-9) return 0.0f;
        const double nyquist = sampleRate * 0.5;
        return (float) juce::jlimit (0.0, 1.0, (weightedSum / magSum) / nyquist);
    }

    float FrameFeatureExtractor::flatness (const float* magnitudes) const
    {
        // Skip DC bin. Use log-domain geometric mean for numerical stability.
        double logSum = 0.0, arithSum = 0.0;
        int count = 0;
        for (int i = 1; i < numBins_; ++i)
        {
            const double m = (double) magnitudes[i] + 1.0e-9;
            logSum += std::log (m);
            arithSum += m;
            ++count;
        }
        if (count == 0 || arithSum < 1.0e-9) return 0.0f;
        const double geoMean = std::exp (logSum / count);
        const double arithMean = arithSum / count;
        return (float) juce::jlimit (0.0, 1.0, geoMean / arithMean);
    }

    float FrameFeatureExtractor::flux (const float* magnitudes, const float* prevMagnitudes) const
    {
        if (prevMagnitudes == nullptr) return 0.0f;
        double sumPos = 0.0, sumMag = 1.0e-9;
        for (int i = 0; i < numBins_; ++i)
        {
            const double d = (double) magnitudes[i] - (double) prevMagnitudes[i];
            if (d > 0.0) sumPos += d;
            sumMag += magnitudes[i];
        }
        return (float) juce::jlimit (0.0, 1.0, sumPos / sumMag);
    }

    float FrameFeatureExtractor::bandEnergyFraction (const float* magnitudes, double sampleRate,
                                                       float loHz, float hiHz) const
    {
        double bandEnergy = 0.0, totalEnergy = 1.0e-9;
        for (int i = 0; i < numBins_; ++i)
        {
            const double freq = i * sampleRate / fftSize_;
            const double e = (double) magnitudes[i] * (double) magnitudes[i];
            totalEnergy += e;
            if (freq >= loHz && freq < hiHz) bandEnergy += e;
        }
        return (float) juce::jlimit (0.0, 1.0, bandEnergy / totalEnergy);
    }

    FeatureVector FrameFeatureExtractor::analyzeSegment (const float* segmentSamples, int numSegmentSamples,
                                                           double sampleRate,
                                                           float* outMagnitudes)
    {
        FeatureVector fv;

        std::vector<float> frame ((size_t) fftSize_, 0.0f);
        const int copyCount = std::min (numSegmentSamples, fftSize_);
        std::copy (segmentSamples, segmentSamples + copyCount, frame.begin());
        computeMagnitudeSpectrum (frame.data(), outMagnitudes);

        // Self-contained intra-segment flux: attack spectrum (outMagnitudes, above)
        // vs. a tail spectrum taken from around the segment midpoint.
        float fluxValue = 0.0f;
        if (numSegmentSamples > fftSize_)
        {
            tailFrameScratch_.assign ((size_t) fftSize_, 0.0f);
            tailMagScratch_.assign ((size_t) numBins_, 0.0f);

            int tailStart = juce::jmin (numSegmentSamples - fftSize_, numSegmentSamples / 2);
            tailStart = juce::jmax (0, tailStart);
            const int tailCopy = juce::jmin (fftSize_, numSegmentSamples - tailStart);
            std::copy (segmentSamples + tailStart, segmentSamples + tailStart + tailCopy, tailFrameScratch_.begin());
            computeMagnitudeSpectrum (tailFrameScratch_.data(), tailMagScratch_.data());

            fluxValue = flux (tailMagScratch_.data(), outMagnitudes); // growth from attack -> tail
        }

        fv[FeatureVector::SpectralCentroid] = centroid (outMagnitudes, sampleRate);
        fv[FeatureVector::SpectralFlatness] = flatness (outMagnitudes);
        fv[FeatureVector::SpectralFlux]     = fluxValue;
        fv[FeatureVector::ZeroCrossingRate] = computeZeroCrossingRate (segmentSamples, numSegmentSamples);
        fv[FeatureVector::Rms]              = computeRms (segmentSamples, numSegmentSamples);

        const int peakIdx = findPeakIndex (segmentSamples, numSegmentSamples);
        fv[FeatureVector::AttackTimeMs] = estimateAttackTimeMs (segmentSamples, numSegmentSamples, peakIdx, sampleRate);
        fv[FeatureVector::DecayTimeMs]  = estimateDecayTimeMs (segmentSamples, numSegmentSamples, peakIdx, sampleRate);

        const auto pitch = estimatePitch (segmentSamples, numSegmentSamples, sampleRate);
        fv[FeatureVector::Harmonicity]     = pitch.harmonicity;
        fv[FeatureVector::PitchConfidence] = pitch.confidence;

        fv[FeatureVector::BandEnergyLow]  = bandEnergyFraction (outMagnitudes, sampleRate, 0.0f, 150.0f);
        fv[FeatureVector::BandEnergyMid]  = bandEnergyFraction (outMagnitudes, sampleRate, 150.0f, 6000.0f);
        fv[FeatureVector::BandEnergyHigh] = bandEnergyFraction (outMagnitudes, sampleRate, 6000.0f, (float) (sampleRate * 0.5));

        return fv;
    }

    //==============================================================================
    float computeRms (const float* samples, int numSamples)
    {
        if (numSamples <= 0) return 0.0f;
        double sum = 0.0;
        for (int i = 0; i < numSamples; ++i) sum += (double) samples[i] * (double) samples[i];
        return (float) std::sqrt (sum / numSamples);
    }

    float computeZeroCrossingRate (const float* samples, int numSamples)
    {
        if (numSamples < 2) return 0.0f;
        int crossings = 0;
        for (int i = 1; i < numSamples; ++i)
            if ((samples[i - 1] >= 0.0f) != (samples[i] >= 0.0f))
                ++crossings;
        return (float) crossings / (float) (numSamples - 1);
    }

    juce::AudioBuffer<float> mixToMono (const juce::AudioBuffer<float>& buffer)
    {
        juce::AudioBuffer<float> mono (1, buffer.getNumSamples());
        mono.clear();
        const float scale = 1.0f / (float) juce::jmax (1, buffer.getNumChannels());
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            mono.addFrom (0, 0, buffer, ch, 0, buffer.getNumSamples(), scale);
        return mono;
    }

    int findPeakIndex (const float* samples, int numSamples)
    {
        int bestIdx = 0;
        float bestVal = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float v = std::abs (samples[i]);
            if (v > bestVal) { bestVal = v; bestIdx = i; }
        }
        return bestIdx;
    }

    float estimateAttackTimeMs (const float* samples, int numSamples, int peakIndex, double sampleRate)
    {
        if (peakIndex <= 0 || numSamples <= 0) return 0.0f;
        const float peakLevel = std::abs (samples[peakIndex]);
        if (peakLevel < 1.0e-6f) return 0.0f;
        const float threshold = 0.9f * peakLevel;
        int riseIdx = peakIndex;
        for (int i = 0; i <= peakIndex; ++i)
        {
            if (std::abs (samples[i]) >= threshold) { riseIdx = i; break; }
        }
        const float ms = (float) (riseIdx / sampleRate * 1000.0);
        return juce::jlimit (0.0f, 200.0f, ms);
    }

    float estimateDecayTimeMs (const float* samples, int numSamples, int peakIndex, double sampleRate)
    {
        if (peakIndex < 0 || peakIndex >= numSamples) return 0.0f;
        const float peakLevel = std::abs (samples[peakIndex]);
        if (peakLevel < 1.0e-6f) return 0.0f;
        const float threshold = 0.1f * peakLevel; // -20dB
        int fallIdx = numSamples - 1;
        for (int i = peakIndex; i < numSamples; ++i)
        {
            if (std::abs (samples[i]) <= threshold) { fallIdx = i; break; }
        }
        const float ms = (float) ((fallIdx - peakIndex) / sampleRate * 1000.0);
        return juce::jlimit (0.0f, 1000.0f, ms);
    }

    PitchEstimate estimatePitch (const float* samples, int numSamples, double sampleRate, float minHz, float maxHz)
    {
        PitchEstimate result;
        const int minLag = (int) std::floor (sampleRate / maxHz);
        const int maxLag = (int) std::ceil (sampleRate / minHz);
        if (minLag < 1 || maxLag >= numSamples || minLag >= maxLag)
            return result;

        double energy0 = 0.0;
        for (int i = 0; i < numSamples; ++i) energy0 += (double) samples[i] * samples[i];
        if (energy0 < 1.0e-9) return result;

        double bestR = -1.0;
        int bestLag = minLag;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double cross = 0.0, energyLag = 0.0;
            const int n = numSamples - lag;
            if (n <= 0) break;
            for (int i = 0; i < n; ++i)
            {
                cross += (double) samples[i] * samples[i + lag];
                energyLag += (double) samples[i + lag] * samples[i + lag];
            }
            const double denom = std::sqrt (energy0 * energyLag) + 1.0e-9;
            const double r = cross / denom;
            if (r > bestR) { bestR = r; bestLag = lag; }
        }

        result.harmonicity = (float) juce::jlimit (0.0, 1.0, bestR);
        result.confidence  = result.harmonicity;
        result.hz = (float) (sampleRate / bestLag);
        return result;
    }

    //==============================================================================
    std::vector<int64_t> detectOnsets (const juce::AudioBuffer<float>& monoBuffer, double sampleRate,
                                        int fftOrder, int hopSize, float sensitivity, int minGapSamples)
    {
        FrameFeatureExtractor extractor (fftOrder);
        const int fftSize = extractor.fftSize();
        const int numBins = fftSize / 2 + 1;

        const float* data = monoBuffer.getReadPointer (0);
        const int numSamples = monoBuffer.getNumSamples();

        std::vector<float> prevMag ((size_t) numBins, 0.0f);
        std::vector<float> curMag ((size_t) numBins, 0.0f);
        std::vector<float> frame ((size_t) fftSize, 0.0f);
        std::vector<float> novelty;
        std::vector<int64_t> framePositions;
        novelty.reserve ((size_t) (numSamples / hopSize) + 1);
        framePositions.reserve (novelty.capacity());

        bool first = true;
        for (int pos = 0; pos + fftSize <= numSamples; pos += hopSize)
        {
            std::copy (data + pos, data + pos + fftSize, frame.begin());
            extractor.computeMagnitudeSpectrum (frame.data(), curMag.data());

            float fluxVal = 0.0f;
            if (! first)
            {
                for (int i = 0; i < numBins; ++i)
                    fluxVal += std::max (0.0f, curMag[(size_t) i] - prevMag[(size_t) i]);
            }
            first = false;

            novelty.push_back (fluxVal);
            framePositions.push_back (pos);
            std::swap (prevMag, curMag);
        }

        std::vector<int64_t> onsets;
        if (novelty.empty()) return onsets;

        const int medianWindow = std::max (1, (int) (sampleRate / hopSize)); // ~1 second of frames
        int64_t lastOnsetSample = -((int64_t) minGapSamples);

        std::vector<float> windowBuf, devBuf;
        for (size_t i = 0; i < novelty.size(); ++i)
        {
            const size_t lo = (i >= (size_t) medianWindow) ? (i - (size_t) medianWindow) : 0;
            const size_t hi = std::min (novelty.size(), i + (size_t) medianWindow + 1);

            windowBuf.assign (novelty.begin() + (long) lo, novelty.begin() + (long) hi);
            std::vector<float> sorted (windowBuf);
            std::nth_element (sorted.begin(), sorted.begin() + (long) sorted.size() / 2, sorted.end());
            const float median = sorted[sorted.size() / 2];

            devBuf.resize (windowBuf.size());
            for (size_t k = 0; k < windowBuf.size(); ++k) devBuf[k] = std::abs (windowBuf[k] - median);
            std::nth_element (devBuf.begin(), devBuf.begin() + (long) devBuf.size() / 2, devBuf.end());
            const float mad = devBuf[devBuf.size() / 2];

            const float threshold = median + sensitivity * mad + 1.0e-6f;
            const bool isLocalPeak =
                novelty[i] > threshold
                && (i == 0 || novelty[i] >= novelty[i - 1])
                && (i + 1 == novelty.size() || novelty[i] >= novelty[i + 1]);

            if (isLocalPeak)
            {
                const int64_t samplePos = framePositions[i];
                if (samplePos - lastOnsetSample >= minGapSamples)
                {
                    onsets.push_back (samplePos);
                    lastOnsetSample = samplePos;
                }
            }
        }

        return onsets;
    }

    double estimateBpmFromOnsets (const std::vector<int64_t>& onsetSamples, double sampleRate,
                                   double minBpm, double maxBpm)
    {
        if (onsetSamples.size() < 2) return 0.0;

        std::vector<double> iois;
        iois.reserve (onsetSamples.size() - 1);
        for (size_t i = 1; i < onsetSamples.size(); ++i)
        {
            const double sec = (double) (onsetSamples[i] - onsetSamples[i - 1]) / sampleRate;
            if (sec > 0.05) iois.push_back (sec);
        }
        if (iois.empty()) return 0.0;

        const int numBuckets = (int) (maxBpm - minBpm) + 1;
        std::vector<int> hist ((size_t) numBuckets, 0);
        for (double sec : iois)
        {
            double bpm = 60.0 / sec;
            while (bpm < minBpm) bpm *= 2.0;
            while (bpm > maxBpm) bpm /= 2.0;
            int bucket = (int) std::round (bpm - minBpm);
            bucket = juce::jlimit (0, numBuckets - 1, bucket);
            hist[(size_t) bucket]++;
        }

        const auto bestIt = std::max_element (hist.begin(), hist.end());
        const int bestBucket = (int) std::distance (hist.begin(), bestIt);
        return minBpm + bestBucket;
    }

    namespace
    {
        double pearsonCorrelation (const std::array<double, 12>& chroma, const float* profile, int rotation)
        {
            std::array<double, 12> rotated {};
            for (int i = 0; i < 12; ++i) rotated[(size_t) i] = profile[(i - rotation + 12) % 12];

            double meanC = 0.0, meanP = 0.0;
            for (int i = 0; i < 12; ++i) { meanC += chroma[(size_t) i]; meanP += rotated[(size_t) i]; }
            meanC /= 12.0; meanP /= 12.0;

            double num = 0.0, denC = 0.0, denP = 0.0;
            for (int i = 0; i < 12; ++i)
            {
                const double dc = chroma[(size_t) i] - meanC;
                const double dp = rotated[(size_t) i] - meanP;
                num += dc * dp;
                denC += dc * dc;
                denP += dp * dp;
            }
            const double denom = std::sqrt (denC * denP) + 1.0e-9;
            return num / denom;
        }
    }

    juce::String estimateKey (const juce::AudioBuffer<float>& monoBuffer, double sampleRate)
    {
        static const float majorProfile[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
                                                  2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        static const float minorProfile[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
                                                  2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
        static const char* pitchNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        FrameFeatureExtractor extractor (12); // 4096-point FFT for pitch resolution
        const int fftSize = extractor.fftSize();
        const int numBins = fftSize / 2 + 1;
        std::vector<float> mag ((size_t) numBins, 0.0f);
        std::array<double, 12> chroma {}; chroma.fill (0.0);

        const float* data = monoBuffer.getReadPointer (0);
        const int numSamples = monoBuffer.getNumSamples();
        const int hop = fftSize;
        const int maxFrames = 500; // cap analysis cost on very long tracks
        int frameCount = 0;
        std::vector<float> frame ((size_t) fftSize, 0.0f);

        for (int pos = 0; pos + fftSize <= numSamples && frameCount < maxFrames; pos += hop, ++frameCount)
        {
            std::copy (data + pos, data + pos + fftSize, frame.begin());
            extractor.computeMagnitudeSpectrum (frame.data(), mag.data());

            for (int i = 1; i < numBins; ++i)
            {
                const double freq = i * sampleRate / fftSize;
                if (freq < 55.0 || freq > 5000.0) continue;
                const double midi = 69.0 + 12.0 * std::log2 (freq / 440.0);
                int pitchClass = ((int) std::lround (midi)) % 12;
                if (pitchClass < 0) pitchClass += 12;
                chroma[(size_t) pitchClass] += mag[(size_t) i];
            }
        }

        double bestScore = -1.0e9;
        int bestPc = 0;
        bool bestIsMajor = true;
        for (int rot = 0; rot < 12; ++rot)
        {
            const double corrMaj = pearsonCorrelation (chroma, majorProfile, rot);
            const double corrMin = pearsonCorrelation (chroma, minorProfile, rot);
            if (corrMaj > bestScore) { bestScore = corrMaj; bestPc = rot; bestIsMajor = true; }
            if (corrMin > bestScore) { bestScore = corrMin; bestPc = rot; bestIsMajor = false; }
        }

        return juce::String (pitchNames[bestPc]) + (bestIsMajor ? " major" : " minor");
    }
}
