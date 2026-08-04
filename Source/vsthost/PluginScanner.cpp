#include "PluginScanner.h"

namespace afq
{
    PluginScanner::PluginScanner()
    {
        // With JUCE_PLUGINHOST_VST3=1 and no other JUCE_PLUGINHOST_* defined,
        // this registers exactly one format: VST3.
        formatManager_.addDefaultFormats();
    }

    PluginScanner::~PluginScanner() = default;

    juce::AudioPluginFormat* PluginScanner::getVst3Format() const
    {
        for (int i = 0; i < formatManager_.getNumFormats(); ++i)
        {
            auto* f = formatManager_.getFormat (i);
            if (f != nullptr && f->getName() == "VST3") return f;
        }
        return nullptr;
    }

    juce::File PluginScanner::getDefaultCacheFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("AkwardFreQ").getChildFile ("KnownVst3Plugins.xml");
    }

    juce::Array<juce::File> PluginScanner::getDefaultVst3SearchPaths()
    {
        juce::Array<juce::File> paths;
       #if JUCE_WINDOWS
        paths.add (juce::File ("C:\\Program Files\\Common Files\\VST3"));
        paths.add (juce::File ("C:\\Program Files (x86)\\Common Files\\VST3"));
       #elif JUCE_MAC
        paths.add (juce::File ("/Library/Audio/Plug-Ins/VST3"));
        paths.add (juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Audio/Plug-Ins/VST3"));
       #else
        paths.add (juce::File ("/usr/lib/vst3"));
        paths.add (juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".vst3"));
       #endif
        return paths;
    }

    void PluginScanner::scan (const juce::Array<juce::File>& extraFolders)
    {
        auto* vst3Format = getVst3Format();
        if (vst3Format == nullptr) return; // VST3 hosting unavailable in this build

        const auto searchPaths = extraFolders.isEmpty() ? getDefaultVst3SearchPaths() : extraFolders;
        const auto deadMansPedal = getDefaultCacheFile().getSiblingFile ("DeadMansPedal.txt");

        for (const auto& folder : searchPaths)
        {
            if (! folder.isDirectory()) continue;

            juce::FileSearchPath searchPath (folder.getFullPathName());
            juce::PluginDirectoryScanner scanner (knownPlugins_, *vst3Format, searchPath, true, deadMansPedal);

            juce::String pluginBeingScanned;
            while (scanner.scanNextFile (true, pluginBeingScanned)) { /* keep going */ }
        }

        saveCache();
    }

    bool PluginScanner::loadCache()
    {
        const auto file = getDefaultCacheFile();
        if (! file.existsAsFile()) return false;

        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr) return false;

        knownPlugins_.recreateFromXml (*xml);
        return true;
    }

    bool PluginScanner::saveCache() const
    {
        const auto file = getDefaultCacheFile();
        file.getParentDirectory().createDirectory();

        std::unique_ptr<juce::XmlElement> xml (knownPlugins_.createXml());
        if (xml == nullptr) return false;

        return file.replaceWithText (xml->toString());
    }
}
