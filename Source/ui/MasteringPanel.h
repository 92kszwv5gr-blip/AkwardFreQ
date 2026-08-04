#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "PluginChainPanel.h"

namespace afq
{
    class MasteringPanel : public juce::Component
    {
    public:
        explicit MasteringPanel (AkwardFreQProcessor& processor);

        // Fired when the user picks a reference file via the button — the editor
        // wires this to load the audio and hand it to the processor's
        // MasteringChain::setReferenceTrack (off the audio thread).
        std::function<void (juce::File)> onLoadReferenceTrack;

        void setReferenceLabel (const juce::String& text);
        void setMeasuredLoudnessLufs (float lufs);

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        AkwardFreQProcessor& processor_;
        juce::AudioProcessorValueTreeState& apvts_;

        PluginChainPanel vstInsertsPanel_;

        juce::Slider targetLoudnessSlider_, compAmountSlider_, limiterCeilingSlider_, eqMatchAmountSlider_;
        juce::Label targetLoudnessLabel_ { {}, "Target Loudness" };
        juce::Label compAmountLabel_ { {}, "Multiband Comp" };
        juce::Label limiterCeilingLabel_ { {}, "Limiter Ceiling" };
        juce::Label eqMatchAmountLabel_ { {}, "EQ Match Amount" };
        juce::Label referenceLabel_ { {}, "No reference loaded" };
        juce::Label measuredLoudnessLabel_ { {}, "Measured: --" };
        juce::ToggleButton bypassToggle_ { "Bypass Mastering" };
        juce::TextButton loadReferenceButton_ { "Load Reference Track..." };

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
            targetLoudnessAttachment_, compAmountAttachment_, limiterCeilingAttachment_, eqMatchAmountAttachment_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment_;

        std::unique_ptr<juce::FileChooser> fileChooser_;
    };
}
