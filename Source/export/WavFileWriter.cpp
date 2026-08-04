#include "WavFileWriter.h"

namespace afq
{
    bool writeWavSlice (const juce::AudioBuffer<float>& source, int64_t startSample, int64_t endSample,
                         double sampleRate, const juce::File& outFile, juce::String& errorMessage)
    {
        const int len = (int) (endSample - startSample);
        if (len <= 0)
        {
            errorMessage = "Empty sample range.";
            return false;
        }

        outFile.getParentDirectory().createDirectory();
        outFile.deleteFile();

        std::unique_ptr<juce::FileOutputStream> stream (outFile.createOutputStream());
        if (stream == nullptr)
        {
            errorMessage = "Could not open " + outFile.getFullPathName() + " for writing.";
            return false;
        }

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wavFormat.createWriterFor (stream.get(), sampleRate, (unsigned int) source.getNumChannels(), 24, {}, 0));
        if (writer == nullptr)
        {
            errorMessage = "Could not create WAV writer for " + outFile.getFullPathName();
            return false;
        }
        stream.release(); // writer now owns the stream

        juce::AudioBuffer<float> slice (source.getNumChannels(), len);
        for (int ch = 0; ch < source.getNumChannels(); ++ch)
            slice.copyFrom (ch, 0, source, ch, (int) startSample, len);

        if (! writer->writeFromAudioSampleBuffer (slice, 0, len))
        {
            errorMessage = "Failed writing audio to " + outFile.getFullPathName();
            return false;
        }

        return true;
    }
}
