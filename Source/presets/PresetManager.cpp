#include "PresetManager.h"

namespace afq
{
    namespace
    {
        juce::File presetsRootDirectory()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("AkwardFreQ").getChildFile ("Presets");
        }
    }

    PresetManager::PresetManager (const juce::String& categoryFolderName)
        : categoryDir_ (presetsRootDirectory().getChildFile (categoryFolderName)),
          defaultMarkerFile_ (categoryDir_.getChildFile ("_default.txt"))
    {
        categoryDir_.createDirectory();
    }

    juce::File PresetManager::fileFor (const juce::String& name) const
    {
        auto legal = juce::File::createLegalFileName (name.trim());
        if (legal.isEmpty()) legal = "Untitled";
        return categoryDir_.getChildFile (legal + ".xml");
    }

    juce::StringArray PresetManager::getPresetNames() const
    {
        juce::StringArray names;
        for (const auto& f : categoryDir_.findChildFiles (juce::File::findFiles, false, "*.xml"))
            names.add (f.getFileNameWithoutExtension());
        names.sort (true);
        return names;
    }

    juce::String PresetManager::getDefaultPresetName() const
    {
        if (! defaultMarkerFile_.existsAsFile()) return {};
        return defaultMarkerFile_.loadFileAsString().trim();
    }

    bool PresetManager::savePreset (const juce::String& name, const juce::XmlElement& state) const
    {
        if (name.trim().isEmpty()) return false;
        return state.writeTo (fileFor (name));
    }

    std::unique_ptr<juce::XmlElement> PresetManager::loadPreset (const juce::String& name) const
    {
        const auto f = fileFor (name);
        if (! f.existsAsFile()) return nullptr;
        return juce::XmlDocument::parse (f);
    }

    bool PresetManager::deletePreset (const juce::String& name) const
    {
        const bool ok = fileFor (name).deleteFile();
        if (ok && getDefaultPresetName() == name.trim())
            setDefaultPreset ({});
        return ok;
    }

    void PresetManager::setDefaultPreset (const juce::String& name) const
    {
        if (name.trim().isEmpty())
            defaultMarkerFile_.deleteFile();
        else
            defaultMarkerFile_.replaceWithText (name.trim());
    }

    std::unique_ptr<juce::XmlElement> PresetManager::loadDefaultPresetIfAny() const
    {
        const auto name = getDefaultPresetName();
        if (name.isEmpty()) return nullptr;
        return loadPreset (name);
    }
}
