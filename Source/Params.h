#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace afq::params
{
    // Genre presets bias the Tier B/C classification thresholds (e.g. expected
    // BPM range, expected onset density) — see OtherSubSplitter::applyGenreBias.
    enum class GenrePreset
    {
        GoaTrance = 0,
        PsyTrance,
        ForestPsy,
        ProgressivePsyTrance,
        MinimalPsyTrance,
        Generic
    };

    inline const juce::StringArray& genrePresetNames()
    {
        static const juce::StringArray names {
            "Goa Trance", "Psy-Trance", "Psychedelic Forest",
            "Progressive Psy-Trance", "Minimal Psy-Trance", "Generic / Other"
        };
        return names;
    }

    // Parameter IDs
    static const juce::String genrePresetId   = "genrePreset";
    static const juce::String captureEnableId = "captureEnable";

    static const juce::String masterTargetLoudnessId = "masterTargetLoudness";
    static const juce::String masterCompAmountId      = "masterCompAmount";
    static const juce::String masterLimiterCeilingId   = "masterLimiterCeiling";
    static const juce::String masterEqMatchAmountId    = "masterEqMatchAmount";
    static const juce::String masterBypassId           = "masterBypass";

    // Per-layer preview gain/mute — id is "layerGain_<index>" / "layerMute_<index>"
    inline juce::String layerGainId (int layerIndex) { return "layerGain_" + juce::String (layerIndex); }
    inline juce::String layerMuteId (int layerIndex) { return "layerMute_" + juce::String (layerIndex); }
    inline juce::String layerSoloId (int layerIndex) { return "layerSolo_" + juce::String (layerIndex); }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
