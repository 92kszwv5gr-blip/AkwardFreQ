#include "Params.h"
#include "separation/RegionTypes.h"

namespace afq::params
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

        p.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { genrePresetId, 1 }, "Genre Preset",
            genrePresetNames(), (int) GenrePreset::PsyTrance));

        p.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { captureEnableId, 1 }, "Capture From Track", false));

        p.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { masterBypassId, 1 }, "Mastering Bypass", true));

        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { masterTargetLoudnessId, 1 }, "Target Loudness (LUFS)",
            juce::NormalisableRange<float> (-23.0f, -6.0f, 0.1f), -8.0f));

        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { masterCompAmountId, 1 }, "Multiband Comp Amount",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.35f));

        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { masterLimiterCeilingId, 1 }, "Limiter Ceiling (dB)",
            juce::NormalisableRange<float> (-3.0f, 0.0f, 0.01f), -0.3f));

        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { masterEqMatchAmountId, 1 }, "EQ Match Amount",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

        for (int i = 0; i < (int) LayerType::Count; ++i)
        {
            auto layerName = afq::layerName ((LayerType) i);

            p.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { layerGainId (i), 1 }, layerName + " Gain",
                juce::NormalisableRange<float> (0.0f, 1.5f, 0.01f), 1.0f));

            p.push_back (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { layerMuteId (i), 1 }, layerName + " Mute", false));

            p.push_back (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { layerSoloId (i), 1 }, layerName + " Solo", false));
        }

        return { p.begin(), p.end() };
    }
}
