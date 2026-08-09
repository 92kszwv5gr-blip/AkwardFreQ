/*
  KntktaProcessor.h — KNTKTA JUCE AudioProcessor

  The main VST3/AU/Standalone audio processor.
  Hosts the preset manager, AI client, link bridge, and UI.
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

class KntktaProcessor : public juce::AudioProcessor
{
public:
    KntktaProcessor();
    ~KntktaProcessor() override;

    // ---------------------------------------------------------------- //
    //  AudioProcessor interface
    // ---------------------------------------------------------------- //
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "KNTKTA"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---------------------------------------------------------------- //
    //  KNTKTA-specific API
    // ---------------------------------------------------------------- //

    /** Set the session tempo (BPM), synced to Link. */
    void setTempo (double bpm);

    /** Returns current BPM. */
    double getTempo() const { return currentBpm; }

    /** Load a KPS preset from JSON string. */
    void loadPreset (const juce::String& kpsJson);

    /** Export current state as KPS JSON. */
    juce::String exportPreset() const;

    /** Number of connected Link peers. */
    int getLinkPeers() const { return linkPeers; }

private:
    double currentBpm  { 120.0 };
    int    linkPeers   { 0 };
    double sampleRate_ { 44100.0 };

    // Preset state (KPS JSON)
    juce::String currentPresetJson;

    // APVTS for DAW automation
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KntktaProcessor)
};
