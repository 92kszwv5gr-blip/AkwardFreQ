#include "AudioToMidiConverter.h"
#include "../separation/AnalysisUtils.h"
#include <cmath>

namespace afq
{
    namespace
    {
        // Shared between transcribe() and writeMidiFile() — both must agree on
        // the tick rate for timestamps to line up.
        constexpr int kSmpteFps = 25;
        constexpr int kSmpteSubframes = 80;
        constexpr double kTicksPerSecond = kSmpteFps * kSmpteSubframes;
    }

    juce::MidiMessageSequence AudioToMidiConverter::transcribe (const juce::AudioBuffer<float>& buffer,
                                                                  int64_t startSample, int64_t endSample,
                                                                  const Settings& settings)
    {
        juce::MidiMessageSequence seq;
        const int64_t rangeLen = endSample - startSample;
        if (rangeLen <= 0) return seq;

        juce::AudioBuffer<float> mono (1, (int) rangeLen);
        mono.clear();
        const float scale = 1.0f / (float) juce::jmax (1, buffer.getNumChannels());
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            mono.addFrom (0, 0, buffer, ch, (int) startSample, (int) rangeLen, scale);

        auto onsets = detectOnsets (mono, settings.sampleRate);
        if (onsets.empty() || onsets.front() != 0) onsets.insert (onsets.begin(), 0);

        const int minNoteDurationSamples = (int) (settings.minNoteDurationMs * 0.001 * settings.sampleRate);
        const float* monoData = mono.getReadPointer (0);

        for (size_t i = 0; i < onsets.size(); ++i)
        {
            const int64_t segStart = onsets[i];
            const int64_t segEnd = (i + 1 < onsets.size()) ? onsets[i + 1] : rangeLen;
            const int segLen = (int) (segEnd - segStart);
            if (segLen < minNoteDurationSamples) continue;

            const float* segData = monoData + segStart;
            const float rms = computeRms (segData, segLen);
            if (rms < settings.silenceRms) continue; // rest

            const auto pitch = estimatePitch (segData, segLen, settings.sampleRate);
            if (pitch.confidence < settings.minConfidence || pitch.hz <= 0.0f) continue; // unpitched — treated as a rest

            const double midiNoteD = 69.0 + 12.0 * std::log2 (pitch.hz / 440.0);
            const int midiNote = juce::jlimit (0, 127, (int) std::lround (midiNoteD));
            const int velocity = juce::jlimit (1, 127, (int) (rms * 4.0f * 127.0f));

            const double startTicks = (double) segStart / settings.sampleRate * kTicksPerSecond;
            const double endTicks = (double) segEnd / settings.sampleRate * kTicksPerSecond;

            auto noteOn = juce::MidiMessage::noteOn (1, midiNote, (juce::uint8) velocity);
            noteOn.setTimeStamp (startTicks);
            seq.addEvent (noteOn);

            auto noteOff = juce::MidiMessage::noteOff (1, midiNote);
            noteOff.setTimeStamp (endTicks);
            seq.addEvent (noteOff);
        }

        seq.updateMatchedPairs();
        return seq;
    }

    bool AudioToMidiConverter::writeMidiFile (const juce::MidiMessageSequence& sequence, double sampleRate, double bpm,
                                               const juce::File& outFile, juce::String& errorMessage)
    {
        juce::ignoreUnused (sampleRate, bpm);

        juce::MidiFile midiFile;
        midiFile.setSmpteTimeFormat (kSmpteFps, kSmpteSubframes);
        midiFile.addTrack (sequence);

        outFile.deleteFile();
        std::unique_ptr<juce::FileOutputStream> stream (outFile.createOutputStream());
        if (stream == nullptr)
        {
            errorMessage = "Could not open " + outFile.getFullPathName() + " for writing.";
            return false;
        }

        if (! midiFile.writeTo (*stream))
        {
            errorMessage = "Failed writing MIDI data to " + outFile.getFullPathName();
            return false;
        }

        return true;
    }
}
