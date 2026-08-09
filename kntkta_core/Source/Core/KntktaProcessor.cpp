/*
  KntktaProcessor.cpp — KNTKTA JUCE AudioProcessor implementation
*/
#include "KntktaProcessor.h"

KntktaProcessor::KntktaProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

KntktaProcessor::~KntktaProcessor() {}

void KntktaProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    sampleRate_ = sampleRate;
}

void KntktaProcessor::releaseResources() {}

void KntktaProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiMessages)
{
    // Pass audio through unmodified — KNTKTA is a controller/preset tool,
    // not an audio effect. MIDI messages are forwarded.
    juce::ignoreUnused (midiMessages);
    // Audio passthrough
}

juce::AudioProcessorEditor* KntktaProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void KntktaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPresetJson", currentPresetJson, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void KntktaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xmlState);
        if (state.isValid())
        {
            apvts.replaceState (state);
            currentPresetJson = state.getProperty ("currentPresetJson").toString();
        }
    }
}

void KntktaProcessor::setTempo (double bpm)
{
    currentBpm = juce::jlimit (20.0, 300.0, bpm);
}

void KntktaProcessor::loadPreset (const juce::String& kpsJson)
{
    currentPresetJson = kpsJson;
}

juce::String KntktaProcessor::exportPreset() const
{
    return currentPresetJson;
}

juce::AudioProcessorValueTreeState::ParameterLayout
KntktaProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "master_volume", "Master Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    return layout;
}

// -------------------------------------------------------------------- //
//  Plugin entry point
// -------------------------------------------------------------------- //
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KntktaProcessor();
}
