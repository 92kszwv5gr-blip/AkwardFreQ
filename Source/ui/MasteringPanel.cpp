#include "MasteringPanel.h"
#include "../Params.h"

namespace afq
{
    namespace
    {
        void setupSlider (juce::Slider& s, juce::Label& l, juce::Component& parent, const juce::String& labelText)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
            parent.addAndMakeVisible (s);

            l.setText (labelText, juce::dontSendNotification);
            l.setJustificationType (juce::Justification::centred);
            l.setFont (12.0f);
            parent.addAndMakeVisible (l);
        }
    }

    MasteringPanel::MasteringPanel (AkwardFreQProcessor& processor)
        : processor_ (processor), apvts_ (processor.apvts),
          vstInsertsPanel_ (processor, processor.getMasteringVstChain(), "VST Inserts (real-time, ahead of the stages below)")
    {
        setupSlider (targetLoudnessSlider_, targetLoudnessLabel_, *this, "Target Loudness");
        setupSlider (compAmountSlider_, compAmountLabel_, *this, "Multiband Comp");
        setupSlider (limiterCeilingSlider_, limiterCeilingLabel_, *this, "Limiter Ceiling");
        setupSlider (eqMatchAmountSlider_, eqMatchAmountLabel_, *this, "EQ Match Amount");

        targetLoudnessAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts_, params::masterTargetLoudnessId, targetLoudnessSlider_);
        compAmountAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts_, params::masterCompAmountId, compAmountSlider_);
        limiterCeilingAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts_, params::masterLimiterCeilingId, limiterCeilingSlider_);
        eqMatchAmountAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts_, params::masterEqMatchAmountId, eqMatchAmountSlider_);

        addAndMakeVisible (bypassToggle_);
        bypassAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts_, params::masterBypassId, bypassToggle_);

        addAndMakeVisible (loadReferenceButton_);
        loadReferenceButton_.onClick = [this]
        {
            fileChooser_ = std::make_unique<juce::FileChooser> (
                "Select a reference track", juce::File(), "*.wav;*.mp3;*.aiff;*.flac");

            fileChooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    const auto file = fc.getResult();
                    if (file.existsAsFile() && onLoadReferenceTrack)
                        onLoadReferenceTrack (file);
                });
        };

        addAndMakeVisible (referenceLabel_);
        referenceLabel_.setFont (12.0f);
        referenceLabel_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

        addAndMakeVisible (measuredLoudnessLabel_);
        measuredLoudnessLabel_.setFont (12.0f);
        measuredLoudnessLabel_.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

        addAndMakeVisible (vstInsertsPanel_);
    }

    void MasteringPanel::setReferenceLabel (const juce::String& text) { referenceLabel_.setText (text, juce::dontSendNotification); }

    void MasteringPanel::setMeasuredLoudnessLufs (float lufs)
    {
        measuredLoudnessLabel_.setText (juce::String::formatted ("Measured: %.1f LUFS", lufs), juce::dontSendNotification);
    }

    void MasteringPanel::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff1e1e1e));
    }

    void MasteringPanel::resized()
    {
        auto area = getLocalBounds().reduced (12);

        auto topRow = area.removeFromTop (24);
        bypassToggle_.setBounds (topRow.removeFromLeft (160));

        area.removeFromTop (8);
        auto knobRow = area.removeFromTop (110);
        const int knobWidth = knobRow.getWidth() / 4;
        auto layoutKnob = [&] (juce::Slider& s, juce::Label& l)
        {
            auto col = knobRow.removeFromLeft (knobWidth);
            l.setBounds (col.removeFromTop (18));
            s.setBounds (col.reduced (6));
        };
        layoutKnob (targetLoudnessSlider_, targetLoudnessLabel_);
        layoutKnob (compAmountSlider_, compAmountLabel_);
        layoutKnob (limiterCeilingSlider_, limiterCeilingLabel_);
        layoutKnob (eqMatchAmountSlider_, eqMatchAmountLabel_);

        area.removeFromTop (12);
        auto refRow = area.removeFromTop (28);
        loadReferenceButton_.setBounds (refRow.removeFromLeft (180));
        refRow.removeFromLeft (8);
        referenceLabel_.setBounds (refRow);

        area.removeFromTop (6);
        measuredLoudnessLabel_.setBounds (area.removeFromTop (20));

        area.removeFromTop (14);
        vstInsertsPanel_.setBounds (area);
    }
}
