#include "SamplePackExporter.h"
#include "../vsthost/PluginChain.h"
#include "../vsthost/BatchVstRenderer.h"
#include <cmath>

namespace afq
{
    namespace
    {
        juce::String sanitizeForFilename (const juce::String& s)
        {
            return s.retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_- ")
                    .replace (" ", "_");
        }

        bool writeWav (const juce::AudioBuffer<float>& source, int64_t startSample, int64_t endSample,
                        double sampleRate, const juce::File& outFile, PluginChain* vstChain)
        {
            const int len = (int) (endSample - startSample);
            if (len <= 0) return false;

            juce::AudioBuffer<float> slice (source.getNumChannels(), len);
            for (int ch = 0; ch < source.getNumChannels(); ++ch)
                slice.copyFrom (ch, 0, source, ch, (int) startSample, len);

            // Explicit local (not a ternary bound to a reference) — avoids any
            // ambiguity about which branch's temporary a reference would extend.
            juce::AudioBuffer<float> toWrite = (vstChain != nullptr)
                ? BatchVstRenderer::render (slice, sampleRate, *vstChain)
                : std::move (slice);

            outFile.getParentDirectory().createDirectory();
            outFile.deleteFile();

            std::unique_ptr<juce::FileOutputStream> stream (outFile.createOutputStream());
            if (stream == nullptr) return false;

            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer (
                wavFormat.createWriterFor (stream.get(), sampleRate, (unsigned int) toWrite.getNumChannels(),
                                            24, {}, 0));
            if (writer == nullptr) return false;

            stream.release(); // writer now owns the stream

            return writer->writeFromAudioSampleBuffer (toWrite, 0, toWrite.getNumSamples());
        }
    }

    bool SamplePackExporter::exportPack (const SeparationResult& result, const ExportSettings& settings,
                                          juce::String& errorMessage,
                                          const std::function<void (float, juce::String)>& onProgress,
                                          PluginChain* vstChain)
    {
        if (! settings.destinationFolder.isDirectory() && ! settings.destinationFolder.createDirectory())
        {
            errorMessage = "Could not create destination folder: " + settings.destinationFolder.getFullPathName();
            return false;
        }

        const juce::String safePackName = sanitizeForFilename (settings.packName);
        const juce::File packRoot = settings.destinationFolder.getChildFile (safePackName);
        if (! packRoot.createDirectory())
        {
            errorMessage = "Could not create pack folder: " + packRoot.getFullPathName();
            return false;
        }

        const int64_t minLenSamples = (int64_t) (settings.minRegionLengthMs * 0.001 * result.sampleRate);
        const int bpmRounded = (int) std::round (result.estimatedBpm);

        std::array<int, (size_t) LayerType::Count> counters {};
        counters.fill (0);

        juce::Array<juce::var> manifestFiles;
        int exportedCount = 0;
        const int totalRegions = (int) result.regions.size();
        int processedRegions = 0;

        for (const auto& region : result.regions)
        {
            ++processedRegions;
            if (onProgress)
                onProgress ((float) processedRegions / (float) juce::jmax (1, totalRegions),
                             "Exporting " + layerName (region.type));

            if (region.type == LayerType::Unclassified) continue;
            if (region.lengthSamples() < minLenSamples) continue;

            const auto& sourceBuffer = result.layerBuffers[(size_t) region.type];
            if (sourceBuffer.getNumSamples() == 0) continue;

            const size_t typeIdx = (size_t) region.type;
            const int index = ++counters[typeIdx];
            const juce::String fileName = safePackName + "_" + layerName (region.type)
                                           + "_" + juce::String (index).paddedLeft ('0', 3)
                                           + (bpmRounded > 0 ? ("_" + juce::String (bpmRounded) + "bpm") : juce::String())
                                           + ".wav";

            const juce::File layerFolder = packRoot.getChildFile (layerName (region.type));
            const juce::File outFile = layerFolder.getChildFile (fileName);

            if (! writeWav (sourceBuffer, region.startSample, region.endSample, result.sampleRate, outFile, vstChain))
                continue; // skip failures, keep exporting the rest of the pack

            ++exportedCount;

            auto* obj = new juce::DynamicObject();
            obj->setProperty ("file", layerName (region.type) + "/" + fileName);
            obj->setProperty ("layer", layerName (region.type));
            obj->setProperty ("startSample", (int64_t) region.startSample);
            obj->setProperty ("lengthMs", (double) region.lengthSamples() / result.sampleRate * 1000.0);
            obj->setProperty ("confidence", region.confidence);
            obj->setProperty ("userCorrected", region.userCorrected);
            manifestFiles.add (juce::var (obj));
        }

        if (settings.writeManifest)
        {
            auto* manifest = new juce::DynamicObject();
            manifest->setProperty ("packName", settings.packName);
            manifest->setProperty ("genre", settings.genreTag);
            manifest->setProperty ("estimatedBpm", result.estimatedBpm);
            manifest->setProperty ("estimatedKey", result.estimatedKey);
            manifest->setProperty ("sampleRate", result.sampleRate);
            manifest->setProperty ("exportedFileCount", exportedCount);
            manifest->setProperty ("files", juce::var (manifestFiles));

            const juce::File manifestFile = packRoot.getChildFile ("manifest.json");
            manifestFile.deleteFile();
            manifestFile.create();
            manifestFile.replaceWithText (juce::JSON::toString (juce::var (manifest)));
        }

        if (exportedCount == 0)
        {
            errorMessage = "No regions met the minimum length / classification criteria — nothing was exported.";
            return false;
        }

        return true;
    }
}
