#pragma once
#include "IRMetadata.h"
#include <juce_cryptography/juce_cryptography.h>
#include <vector>

namespace spectralforge {
// User library locations are application preferences, independent of A/B and presets.
struct IRCollection {
    struct Entry {
        juce::File file;
        juce::String name;
        IRMetadata tags;
        int factorySource{};
        bool reference{};
        juce::String menuLabel;
        bool external{};
        juce::String displayName() const {return menuLabel.isNotEmpty() ? menuLabel : tags.shortLabel(name);}
        juce::String details() const {return tags.details(name);}
        bool ready() const { return factorySource != 0 || file.existsAsFile(); }
        bool bass() const {
            return tags.values[1].containsIgnoreCase("Ampeg") || tags.values[1].containsIgnoreCase("Bassman")
                || tags.values[11].startsWithIgnoreCase("Bass") || name.containsIgnoreCase("bass");
        }
    };
    static void labelEntries(std::vector<Entry>& entries) {
        // Repeated microphone positions and equal basenames remain distinct.
        // Disambiguation uses ordinal labels, never private filesystem paths.
        juce::StringArray labels;
        for(const auto& entry:entries)labels.add(entry.tags.shortLabel(entry.name));
        juce::StringArray used;
        for(size_t i=0;i<entries.size();++i) {
            int total=0,ordinal=0;
            for(size_t j=0;j<entries.size();++j)if(labels[(int)i].equalsIgnoreCase(labels[(int)j])) {++total;if(j<=i)++ordinal;}
            entries[i].menuLabel=total>1 ? IRMetadata::boundedLabel(labels[(int)i],49)+" ["+juce::String(ordinal)+"]" : labels[(int)i];
            while(used.contains(entries[i].menuLabel,true))entries[i].menuLabel=IRMetadata::boundedLabel(labels[(int)i],49)+" ["+juce::String(++ordinal)+"]";
            used.add(entries[i].menuLabel);
        }
    }
    static juce::File userRoot() {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("SpectralForge/Chimera");
    }
    static juce::File sharedIRRoot() {
        return juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory).getChildFile("SpectralForge/Chimera/IRs");
    }
    static std::vector<juce::File> roots() {
        std::vector<juce::File> result{userRoot().getChildFile("IRs"), sharedIRRoot()};
        // Portable installs may keep the companion folder alongside the EXE.
        const auto executable=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        for(const auto& folder:{executable.getChildFile("Chimera-Personal-IRs"),executable.getParentDirectory().getChildFile("Chimera-Personal-IRs")})
            if(folder.isDirectory()) result.push_back(folder);
        const auto config = userRoot().getChildFile("ir-folders.json");
        if (config.getSize() <= 16384) {
            const auto json = juce::JSON::parse(config);
            if (const auto* paths = json.getArray()) for (const auto& path : *paths) {
                const auto text = path.toString();
                if (juce::File::isAbsolutePath(text) && result.size() < 18) result.emplace_back(text);
            }
        }
        return result;
    }
    static juce::Result rememberFolder(const juce::File& folder) {
        if (!folder.isDirectory()) return juce::Result::fail("Choose an existing IR folder.");
        juce::Array<juce::var> paths;
        paths.add(folder.getFullPathName());
        auto previous = roots();
        for (size_t i=2; i<previous.size() && paths.size()<16; ++i)
            if (previous[i] != folder) paths.add(previous[i].getFullPathName());
        if (auto result=userRoot().createDirectory(); result.failed()) return result;
        return userRoot().getChildFile("ir-folders.json").replaceWithText(juce::JSON::toString(paths))
            ? juce::Result::ok() : juce::Result::fail("Cannot save the IR folder preference.");
    }
    static std::vector<Entry> scan(const std::vector<juce::File>& folders, bool references) {
        std::vector<Entry> result;
        juce::Array<juce::var> catalogEntries;
        for(const auto* raw:{referenceIRCatalog,externalBassIRCatalog}) {
            const auto catalog=juce::JSON::parse(raw);
            if(const auto* entries=catalog.getArray())catalogEntries.addArray(*entries);
        }
        if (references) {
            for (int i=0; i<2; ++i) {
                result.push_back({{},IRMetadata::factoryFilename(i),IRMetadata::factory(i),i+1,false});
            }
            for(const auto& entry:catalogEntries)
                result.push_back({{},entry["file"].toString(),IRMetadata::fromJSON(entry),0,true,{},(bool)entry["external"]});
        }
        juce::StringArray seen;
        for (const auto& folder:folders) {
            if (!folder.isDirectory()) continue;
            for (const auto& item:juce::RangedDirectoryIterator(folder,true,"*",juce::File::findFiles)) {
                const auto f=item.getFile();
                if (!f.hasFileExtension("wav;aif;aiff") || seen.contains(f.getFullPathName())) continue;
                if (seen.size()>=512) {labelEntries(result);return result;}
                seen.add(f.getFullPathName());
                bool matched=false;
                if (references) {
                    for (const auto& expected:catalogEntries) {
                        if (expected["file"].toString()!=f.getFileName() || f.getSize()>4*1024*1024) continue;
                        if (juce::SHA256(f).toHexString()!=expected["sha256"].toString()) continue;
                        for (auto& row:result) if (row.reference && row.name==f.getFileName()) { row.file=f; matched=true; break; }
                    }
                }
                if (matched) continue;
                auto tags=IRMetadata::filenameHints(f.getFileName());
                const auto sidecar=juce::File(f.getFullPathName()+".json");
                if (sidecar.existsAsFile() && sidecar.getSize()<=16384) {
                    const auto json=juce::JSON::parse(sidecar);
                    if (json.isObject()) tags=IRMetadata::fromJSON(json);
                }
                result.push_back({f,f.getFileName(),tags,0,false});
            }
        }
        labelEntries(result);return result;
    }
    // Import only hash-verified catalog IRs; never extract arbitrary ZIP paths,
    // execute files, or copy the NAM weights which can coexist in the personal archive.
    static juce::Result importPersonalPack(const juce::File& file, const juce::File& destination, int& count) {
        count=0;
        if (!file.existsAsFile() || file.getSize()>32*1024*1024) return juce::Result::fail("Choose the personal IR ZIP (maximum 32 MB).");
        juce::ZipFile zip(file);
        if (zip.getNumEntries()>4096) return juce::Result::fail("The archive contains too many entries.");
        struct Pending { juce::String name; juce::MemoryBlock audio; juce::var tags; };
        std::vector<Pending> pending;
        const auto catalog=juce::JSON::parse(referenceIRCatalog);
        for (const auto& expected:*catalog.getArray()) {
            for (int i=0; i<zip.getNumEntries(); ++i) {
                const auto* entry=zip.getEntry(i);
                const auto leaf=entry->filename.replaceCharacter('\\','/').fromLastOccurrenceOf("/",false,false);
                if (leaf!=expected["file"].toString()) continue;
                if (entry->uncompressedSize<44 || entry->uncompressedSize>4*1024*1024) return juce::Result::fail("An IR in this pack has an invalid size.");
                std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(i));
                juce::MemoryBlock data;
                if (!stream || stream->readIntoMemoryBlock(data,entry->uncompressedSize)!=entry->uncompressedSize
                    || juce::SHA256(data).toHexString()!=expected["sha256"].toString())
                    return juce::Result::fail("IR verification failed. Use the original personal pack.");
                pending.push_back({expected["file"].toString(),std::move(data),expected}); break;
            }
        }
        if (pending.empty()) return juce::Result::fail("No reference IRs found. For other IR collections use ADD FOLDER.");
        if (auto result=destination.createDirectory(); result.failed()) return result;
        for (const auto& item:pending) {
            const auto target=destination.getChildFile(item.name);
            if (!target.replaceWithData(item.audio.getData(),item.audio.getSize())
                || !juce::File(target.getFullPathName()+".json").replaceWithText(juce::JSON::toString(item.tags)))
                return juce::Result::fail("Could not save the IR pack. Check folder permissions.");
            ++count;
        }
        return juce::Result::ok();
    }
};
}
