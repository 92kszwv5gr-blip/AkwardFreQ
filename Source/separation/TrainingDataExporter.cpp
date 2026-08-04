#include "TrainingDataExporter.h"

namespace afq
{
    bool TrainingDataExporter::save (const SeparationResult& result, const juce::File& sourceAudioFile,
                                      const juce::File& trainingDataFolder, juce::String& errorMessage)
    {
        if (! trainingDataFolder.isDirectory() && ! trainingDataFolder.createDirectory())
        {
            errorMessage = "Could not create training data folder: " + trainingDataFolder.getFullPathName();
            return false;
        }

        juce::Array<juce::var> regionsJson;
        for (const auto& region : result.regions)
        {
            if (region.type == LayerType::Unclassified) continue;

            auto* obj = new juce::DynamicObject();
            obj->setProperty ("label", layerName (region.type));
            obj->setProperty ("labelIndex", (int) region.type);
            obj->setProperty ("startSample", (int64_t) region.startSample);
            obj->setProperty ("endSample", (int64_t) region.endSample);
            obj->setProperty ("confidence", region.confidence);
            obj->setProperty ("userCorrected", region.userCorrected);

            juce::Array<juce::var> featuresJson;
            for (float f : region.features.values) featuresJson.add (f);
            obj->setProperty ("features", juce::var (featuresJson));

            regionsJson.add (juce::var (obj));
        }

        auto* root = new juce::DynamicObject();
        root->setProperty ("featureSpecVersion", 1);
        root->setProperty ("sourceAudioFile", sourceAudioFile.getFullPathName());
        root->setProperty ("sampleRate", result.sampleRate);
        root->setProperty ("estimatedBpm", result.estimatedBpm);
        root->setProperty ("estimatedKey", result.estimatedKey);
        root->setProperty ("savedAt", juce::Time::getCurrentTime().toISO8601 (true));
        root->setProperty ("regions", juce::var (regionsJson));

        const juce::String baseName = sourceAudioFile.getFileNameWithoutExtension().isEmpty()
                                           ? "track" : sourceAudioFile.getFileNameWithoutExtension();
        const juce::String timestamp = juce::String (juce::Time::getCurrentTime().toMilliseconds());
        const juce::File outFile = trainingDataFolder.getChildFile (baseName + "_" + timestamp + ".json");

        if (! outFile.replaceWithText (juce::JSON::toString (juce::var (root))))
        {
            errorMessage = "Could not write training data file: " + outFile.getFullPathName();
            return false;
        }

        return true;
    }
}
