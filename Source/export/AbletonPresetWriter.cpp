#include "AbletonPresetWriter.h"
// juce::GZIPDecompressorInputStream / GZIPCompressorOutputStream / XmlElement /
// XmlDocument are all part of juce_core's public API, already pulled in via
// the umbrella header included from AbletonPresetWriter.h — JUCE modules
// aren't designed to have their internal per-class headers included directly.

namespace afq
{
    bool AbletonPresetWriter::loadGzipXml (const juce::File& file, std::unique_ptr<juce::XmlElement>& outRoot,
                                            juce::String& errorMessage)
    {
        if (! file.existsAsFile())
        {
            errorMessage = "Template not found: " + file.getFullPathName();
            return false;
        }

        auto fileStream = file.createInputStream();
        if (fileStream == nullptr)
        {
            errorMessage = "Could not open " + file.getFullPathName();
            return false;
        }

        // Ableton's .adv/.adg/.als files are gzip-compressed XML.
        juce::GZIPDecompressorInputStream gunzip (fileStream.release(), true,
                                                    juce::GZIPDecompressorInputStream::gzipFormat);
        const juce::String xmlText = gunzip.readEntireStreamAsString();
        if (xmlText.isEmpty())
        {
            errorMessage = "Template did not decompress to any XML — is it a valid .adv/.adg file? "
                            + file.getFullPathName();
            return false;
        }

        outRoot = juce::XmlDocument::parse (xmlText);
        if (outRoot == nullptr)
        {
            errorMessage = "Decompressed template did not parse as XML: " + file.getFullPathName();
            return false;
        }

        return true;
    }

    bool AbletonPresetWriter::saveGzipXml (const juce::XmlElement& root, const juce::File& outFile,
                                            juce::String& errorMessage)
    {
        outFile.getParentDirectory().createDirectory();
        outFile.deleteFile();

        std::unique_ptr<juce::FileOutputStream> fileStream (outFile.createOutputStream());
        if (fileStream == nullptr)
        {
            errorMessage = "Could not open " + outFile.getFullPathName() + " for writing.";
            return false;
        }

        // windowBits = 15 + 16 selects a real RFC1952 gzip header/trailer
        // (rather than JUCE's default zlib-style wrapper) — this is the
        // documented trick for getting standards-compliant gzip out of
        // GZIPCompressorOutputStream. Ableton's own files are real gzip
        // (verifiable with `file some.adv` -> "gzip compressed data"), so
        // this matters for compatibility, not just correctness in the abstract.
        auto* rawStream = fileStream.release();
        {
            juce::GZIPCompressorOutputStream gzip (rawStream, 9, true, 15 + 16);
            const auto xmlText = root.toString();
            if (! gzip.writeText (xmlText, false, false, nullptr))
            {
                errorMessage = "Failed writing compressed preset data.";
                return false;
            }
        } // gzip stream flushes + closes on scope exit

        return true;
    }

    juce::XmlElement* AbletonPresetWriter::findFirstByTag (juce::XmlElement* root, const juce::String& tag)
    {
        if (root == nullptr) return nullptr;
        if (root->hasTagName (tag)) return root;

        for (auto* child : root->getChildIterator())
        {
            if (auto* found = findFirstByTag (child, tag))
                return found;
        }
        return nullptr;
    }

    void AbletonPresetWriter::patchValueAttribute (juce::XmlElement* element, const juce::String& newValue)
    {
        if (element == nullptr) return;
        if (element->hasAttribute ("Value"))
            element->setAttribute ("Value", newValue);
    }

    bool AbletonPresetWriter::writeSimplerPreset (const SimplerPatch& patch, const juce::File& outAdvFile,
                                                   juce::String& errorMessage)
    {
        std::unique_ptr<juce::XmlElement> root;
        if (! loadGzipXml (patch.templateAdvFile, root, errorMessage))
            return false;

        // Best-confidence patch: the sample's absolute file path, inside the
        // template's <SampleRef><FileRef><Path Value="..."/></FileRef></SampleRef>
        // subtree. This is the field most reverse-engineering write-ups of
        // Ableton's format agree on, and Ableton is known to fall back to the
        // absolute Path when a relative path doesn't resolve.
        if (auto* sampleRef = findFirstByTag (root.get(), "SampleRef"))
        {
            if (auto* fileRef = findFirstByTag (sampleRef, "FileRef"))
            {
                const auto absPath = patch.sampleWavFile.getFullPathName();
                bool patchedAny = false;

                if (auto* pathEl = findFirstByTag (fileRef, "Path"))
                {
                    patchValueAttribute (pathEl, absPath);
                    patchedAny = true;
                }
                if (auto* relPathEl = findFirstByTag (fileRef, "RelativePath"))
                {
                    // Leaving this pointed at the template's original relative
                    // path would make Ableton try (and fail) to resolve it
                    // first — clear it so Path (absolute) is used directly.
                    relPathEl->setAttribute ("Value", "");
                }
                if (auto* sizeEl = findFirstByTag (fileRef, "OriginalFileSize"))
                    sizeEl->setAttribute ("Value", (int) patch.sampleWavFile.getSize());

                if (! patchedAny)
                {
                    errorMessage = "Template's <SampleRef><FileRef> didn't contain a <Path> node — "
                                    "this template's schema doesn't match what this writer expects. "
                                    "Falling back to SFZ/folder export is the reliable path; consider "
                                    "sending the template's decompressed XML to adjust this writer.";
                    return false;
                }
            }
            else
            {
                errorMessage = "Template's <SampleRef> had no <FileRef> — unexpected schema, aborting rather "
                                "than guessing.";
                return false;
            }
        }
        else
        {
            errorMessage = "Could not find a <SampleRef> node in the template — is this really a Simpler "
                            "preset (.adv) with a sample already loaded?";
            return false;
        }

        // Best-effort, non-fatal: root key / key range. Skipped silently (this
        // is opportunistic, not part of the guaranteed contract) if the
        // expected node names aren't present in this template.
        if (auto* rootKeyEl = findFirstByTag (root.get(), "RootKey"))
            patchValueAttribute (rootKeyEl, juce::String (patch.rootKey));

        if (auto* keyRange = findFirstByTag (root.get(), "KeyRange"))
        {
            if (auto* minEl = findFirstByTag (keyRange, "Min")) patchValueAttribute (minEl, juce::String (patch.lowKey));
            if (auto* maxEl = findFirstByTag (keyRange, "Max")) patchValueAttribute (maxEl, juce::String (patch.highKey));
        }

        return saveGzipXml (*root, outAdvFile, errorMessage);
    }
}
