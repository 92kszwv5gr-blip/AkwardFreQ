#include "DrumRackExporter.h"
#include "WavFileWriter.h"
#include "../separation/DrumSlicer.h"
#include "../separation/EqualSlicer.h"
#include "../vsthost/PluginChain.h"
#include "../vsthost/BatchVstRenderer.h"

namespace afq
{
    namespace
    {
        juce::String sanitizeForFilename (const juce::String& s)
        {
            return s.retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_- ")
                    .replace (" ", "_");
        }
    }

    bool DrumRackExporter::exportSlicedDrums (const juce::AudioBuffer<float>& sourceBuffer, double sampleRate,
                                               int64_t rangeStartSample, int64_t rangeEndSample,
                                               const Settings& settings, juce::String& errorMessage,
                                               int* outSliceCount, PluginChain* vstChain)
    {
        const auto slices = (settings.mode == SliceMode::Equal)
            ? EqualSlicer::slice (sourceBuffer.getNumSamples(), settings.sliceCount, rangeStartSample, rangeEndSample)
            : DrumSlicer::slice (sourceBuffer, sampleRate, rangeStartSample, rangeEndSample);
        if (slices.empty())
        {
            errorMessage = "No slices detected in the selected range.";
            return false;
        }

        const juce::String safeKitName = sanitizeForFilename (settings.prefix + settings.kitName);
        const juce::File kitFolder = settings.destinationFolder.getChildFile (safeKitName);
        if (! kitFolder.createDirectory())
        {
            errorMessage = "Could not create folder: " + kitFolder.getFullPathName();
            return false;
        }

        int written = 0;
        for (const auto& s : slices)
        {
            const juce::String fileName = safeKitName + "_" + juce::String (s.index).paddedLeft ('0', 2) + ".wav";
            const juce::File outFile = kitFolder.getChildFile (fileName);

            juce::String sliceError;
            bool ok;
            if (vstChain != nullptr)
            {
                const int len = (int) (s.endSample - s.startSample);
                juce::AudioBuffer<float> rawSlice (sourceBuffer.getNumChannels(), len);
                for (int ch = 0; ch < sourceBuffer.getNumChannels(); ++ch)
                    rawSlice.copyFrom (ch, 0, sourceBuffer, ch, (int) s.startSample, len);

                const auto rendered = BatchVstRenderer::render (rawSlice, sampleRate, *vstChain);
                ok = writeWavSlice (rendered, 0, rendered.getNumSamples(), sampleRate, outFile, sliceError);
            }
            else
            {
                ok = writeWavSlice (sourceBuffer, s.startSample, s.endSample, sampleRate, outFile, sliceError);
            }

            if (ok) ++written;
            // Keep going on a single-slice failure — a partial kit is more
            // useful than aborting the whole export over one bad slice.
        }

        if (outSliceCount != nullptr) *outSliceCount = written;

        if (written == 0)
        {
            errorMessage = "Failed to write any slices.";
            return false;
        }

        return true;
    }
}
