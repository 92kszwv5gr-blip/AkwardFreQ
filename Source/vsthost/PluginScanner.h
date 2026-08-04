#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace afq
{
    // Scans for installed VST3 plugins and caches the result to disk so
    // AkwardFreQ doesn't rescan on every launch. VST3 only — VST2's SDK was
    // discontinued by Steinberg years ago and isn't cleanly redistributable
    // any more, and VST3 is what JUCE hosts natively and what most current
    // plugins ship as anyway.
    class PluginScanner
    {
    public:
        PluginScanner();
        ~PluginScanner();

        // Synchronous — instantiates each candidate plugin briefly to read
        // its description. Can take several seconds for a large plugin
        // folder; call from a background thread, not the message thread.
        // Pass no folders to scan the platform's default VST3 install
        // locations.
        void scan (const juce::Array<juce::File>& extraFolders = {});

        bool loadCache();
        bool saveCache() const;

        const juce::KnownPluginList& getKnownPlugins() const noexcept { return knownPlugins_; }
        juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager_; }

        static juce::File getDefaultCacheFile();
        static juce::Array<juce::File> getDefaultVst3SearchPaths();

    private:
        juce::AudioPluginFormatManager formatManager_;
        juce::KnownPluginList knownPlugins_;

        juce::AudioPluginFormat* getVst3Format() const;
    };
}
