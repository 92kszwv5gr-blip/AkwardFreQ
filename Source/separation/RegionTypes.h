#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <array>

namespace afq
{
    // See docs/FEATURE_SPEC.md for the canonical ordering — keep this in sync
    // with tools/retrain_layer_classifier.py.
    enum class LayerType
    {
        Kick = 0,
        Bass,
        HiHat,
        Percussion,
        Breakbeat,
        SynthLead,
        Stabs,
        Atmosphere,
        FX,
        Zap,
        Glitch,
        Unclassified,
        Count
    };

    constexpr int kNumClassifiedLayers = static_cast<int>(LayerType::Unclassified); // excludes Unclassified itself
    constexpr int kFeatureVectorSize = 12;

    inline const juce::String& layerName (LayerType t)
    {
        static const std::array<juce::String, (size_t) LayerType::Count> names {
            "Kick", "Bass", "HiHat", "Percussion", "Breakbeat",
            "SynthLead", "Stabs", "Atmosphere", "FX", "Zap", "Glitch", "Unclassified"
        };
        return names[(size_t) t];
    }

    inline juce::Colour layerColour (LayerType t)
    {
        static const std::array<juce::Colour, (size_t) LayerType::Count> colours {
            juce::Colour (0xffe74c3c), // Kick - red
            juce::Colour (0xff9b59b6), // Bass - purple
            juce::Colour (0xfff1c40f), // HiHat - yellow
            juce::Colour (0xffe67e22), // Percussion - orange
            juce::Colour (0xffd35400), // Breakbeat - dark orange
            juce::Colour (0xff2ecc71), // SynthLead - green
            juce::Colour (0xff1abc9c), // Stabs - teal
            juce::Colour (0xff3498db), // Atmosphere - blue
            juce::Colour (0xff95a5a6), // FX - grey
            juce::Colour (0xffec407a), // Zap - pink
            juce::Colour (0xff7f8c8d), // Glitch - dark grey
            juce::Colour (0xff34495e)  // Unclassified - navy
        };
        return colours[(size_t) t];
    }

    // Which "bus" (Tier A stem) a layer is derived from — used to route the
    // right audio into the Tier B/C sub-splitters and by the exporter to know
    // which isolated buffer to slice from.
    enum class SourceBus { Bass, Drums, Other };

    inline SourceBus sourceBusFor (LayerType t)
    {
        switch (t)
        {
            case LayerType::Bass:
                return SourceBus::Bass;
            case LayerType::Kick:
            case LayerType::HiHat:
            case LayerType::Percussion:
            case LayerType::Breakbeat:
                return SourceBus::Drums;
            default:
                return SourceBus::Other;
        }
    }

    // A feature vector extracted from one analysis frame or one onset-bounded
    // segment. Order MUST match docs/FEATURE_SPEC.md.
    struct FeatureVector
    {
        std::array<float, kFeatureVectorSize> values {};

        enum Index
        {
            SpectralCentroid = 0,
            SpectralFlatness,
            SpectralFlux,
            ZeroCrossingRate,
            Rms,
            AttackTimeMs,
            DecayTimeMs,
            Harmonicity,
            BandEnergyLow,
            BandEnergyMid,
            BandEnergyHigh,
            PitchConfidence
        };

        float& operator[] (Index i)       { return values[(size_t) i]; }
        float  operator[] (Index i) const { return values[(size_t) i]; }
    };

    // A detected/corrected time region tagged with a layer.
    struct Region
    {
        LayerType type = LayerType::Unclassified;
        int64_t startSample = 0;
        int64_t endSample = 0;       // exclusive
        float confidence = 0.0f;     // 0-1, classifier confidence (1.0 if user-corrected)
        bool userCorrected = false;
        FeatureVector features;      // features used to classify this region (for training export)

        int64_t lengthSamples() const { return endSample - startSample; }
    };

    // Result of the full Tier A -> B/C -> D pipeline for one imported/captured track.
    struct SeparationResult
    {
        double sampleRate = 44100.0;
        int numChannels = 2;

        juce::AudioBuffer<float> bassBuffer;
        juce::AudioBuffer<float> drumsBuffer;
        juce::AudioBuffer<float> otherBuffer;

        // Per-layer reconstructed (masked) audio, keyed by LayerType index.
        std::array<juce::AudioBuffer<float>, (size_t) LayerType::Count> layerBuffers;

        std::vector<Region> regions;

        double estimatedBpm = 0.0;
        juce::String estimatedKey; // e.g. "A minor"
    };
}
