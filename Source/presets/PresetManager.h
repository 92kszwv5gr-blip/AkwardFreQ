#pragma once

#include <juce_core/juce_core.h>
#include <memory>

namespace afq
{
    // Disk-backed named-preset store for one settings category (e.g. "Mastering",
    // "VstChain", "TagProfile"). Deliberately NOT part of the plugin's per-project
    // state (APVTS) — presets live under the user's application-data directory so
    // they're available across every Ableton project and every session, which is
    // the whole point of "save this as default so I don't have to set it up every
    // time." Each category also has an optional "default" preset that panels
    // auto-apply on construction.
    //
    // Presets are stored as one XML file per preset under:
    //   <userAppData>/AkwardFreQ/Presets/<category>/<sanitized name>.xml
    // and the default marker as a plain-text file:
    //   <userAppData>/AkwardFreQ/Presets/<category>/_default.txt
    class PresetManager
    {
    public:
        explicit PresetManager (const juce::String& categoryFolderName);

        juce::StringArray getPresetNames() const;
        juce::String getDefaultPresetName() const; // "" = no default set

        bool savePreset (const juce::String& name, const juce::XmlElement& state) const;
        std::unique_ptr<juce::XmlElement> loadPreset (const juce::String& name) const; // nullptr if missing
        bool deletePreset (const juce::String& name) const;

        // "" clears the default.
        void setDefaultPreset (const juce::String& name) const;
        std::unique_ptr<juce::XmlElement> loadDefaultPresetIfAny() const; // nullptr if none set/missing

    private:
        juce::File categoryDir_;
        juce::File defaultMarkerFile_;

        juce::File fileFor (const juce::String& name) const;
    };
}
