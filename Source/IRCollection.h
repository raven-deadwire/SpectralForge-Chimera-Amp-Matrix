#pragma once
#include "IRMetadata.h"
#include "IRUserPreferences.h"
#include <juce_cryptography/juce_cryptography.h>
#include <juce_audio_formats/juce_audio_formats.h>
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
        juce::String validationError;
        juce::String displayName() const {return menuLabel.isNotEmpty() ? menuLabel : tags.shortLabel(name);}
        juce::String details() const {return tags.details(name)+(validationError.isEmpty() ? juce::String{} : "\n\nINVALID FILE: "+validationError);}
        bool ready() const { return factorySource != 0 || (validationError.isEmpty() && file.existsAsFile()); }
        bool removable() const { return factorySource==0 && file!=juce::File{}; }
        bool bass() const {return tags.instrument==IRMetadata::Instrument::bass;}
    };
    enum class Instrument {all,bass,guitarOther,unspecified};
    enum class Availability {all,ready,factory,installed,missing,external,invalid};
    struct ScanReport {int examinedFiles{},invalidFiles{};bool truncated{};};
    static bool matches(const Entry& e,const juce::String& query,const juce::String& inches,Instrument instrument,Availability availability,
                        const juce::String& microphoneId={}) {
        const auto* microphone=e.tags.microphoneModel(e.name);
        if(microphoneId=="other") {if(microphone)return false;}
        else if(microphoneId.isNotEmpty() && (!microphone || microphoneId!=microphone->id))return false;
        auto text=e.name+" "+e.displayName()+" "+juce::JSON::toString(e.tags.json(),true);
        if(microphone)text+=" "+juce::String(microphone->alias)+" "+microphone->reference+" "+micCatalog::kindLabel(microphone->kind);
        if(query.isNotEmpty() && !text.containsIgnoreCase(query)) return false;
        if(inches.isNotEmpty() && e.tags.values[2]!=inches) return false;
        if(instrument==Instrument::bass && !e.bass()) return false;
        if(instrument==Instrument::guitarOther && e.tags.instrument!=IRMetadata::Instrument::guitar) return false;
        if(instrument==Instrument::unspecified && e.tags.instrument!=IRMetadata::Instrument::unspecified) return false;
        switch(availability) {
            case Availability::ready:return e.ready();
            case Availability::factory:return e.factorySource!=0;
            case Availability::installed:return e.factorySource==0 && e.ready();
            case Availability::missing:return !e.ready() && e.validationError.isEmpty();
            case Availability::external:return e.external && !e.ready() && e.validationError.isEmpty();
            case Availability::invalid:return e.validationError.isNotEmpty();
            case Availability::all:return true;
        }
        return false;
    }
    // Discovery is not proof that a filename contains a usable cabinet IR.
    // These bounds match the loader; the loader validates again when selected.
    static juce::String validateFile(const juce::File& file) {
        if(file.getSize()<44 || file.getSize()>4*1024*1024) return "Use a WAV/AIFF file under 4 MB.";
        juce::AudioFormatManager formats;formats.registerFormat(new juce::WavAudioFormat(),true);formats.registerFormat(new juce::AiffAudioFormat(),false);
        auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
        if(!reader) return "Cannot decode this WAV/AIFF file.";
        if(reader->numChannels<1 || reader->numChannels>2 || reader->sampleRate<8000 || reader->sampleRate>384000 || reader->lengthInSamples<8 || reader->lengthInSamples>juce::int64(reader->sampleRate))
            return "Use mono/stereo, 8-384 kHz, 8 samples to 1 second.";
        juce::AudioBuffer<float> block((int)reader->numChannels,1024);double energy=0;
        for(juce::int64 offset=0;offset<reader->lengthInSamples;offset+=1024) {
            const int samples=(int)juce::jmin(juce::int64(1024),reader->lengthInSamples-offset);
            if(!reader->read(&block,0,samples,offset,true,true)) return "IR data is incomplete.";
            for(int c=0;c<block.getNumChannels();++c)for(int n=0;n<samples;++n) {
                const float value=block.getSample(c,n);
                if(!std::isfinite(value) || std::abs(value)>32.f) return "IR contains invalid samples.";
                energy+=double(value)*value;
            }
        }
        return energy<1e-12 ? "IR is silent." : juce::String{};
    }
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
        // Additional libraries must be explicitly selected by the user.
        // Do not discover private/development companion packs beside binaries.
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
    static juce::Result classify(const Entry& entry,IRMetadata::Instrument instrument,const juce::File& preferences=IRUserPreferences::file()) {
        if(!entry.removable())return juce::Result::fail("Only imported IRs can be classified.");
        return IRUserPreferences::update(entry.file,"files",IRMetadata::instrumentKey(instrument),false,false,preferences);
    }
    static juce::Result remove(const Entry& entry,const juce::File& preferences=IRUserPreferences::file()) {
        if(!entry.removable())return juce::Result::fail("Factory and uninstalled reference IRs cannot be removed.");
        return IRUserPreferences::update(entry.file,"files",{},true,false,preferences,entry.reference ? entry.name : juce::String{});
    }
    static juce::Result rememberFile(const juce::File& file,IRMetadata::Instrument instrument,const juce::File& preferences=IRUserPreferences::file(),bool keepExisting=false) {
        const auto error=validateFile(file);if(error.isNotEmpty())return juce::Result::fail(error);
        return IRUserPreferences::update(file,"files",keepExisting ? juce::String{} : IRMetadata::instrumentKey(instrument),false,true,preferences);
    }
    static std::vector<Entry> scan(const std::vector<juce::File>& folders, bool references,ScanReport* report=nullptr,
                                   const juce::File& preferences=IRUserPreferences::file()) {
        if(report) *report={};
        std::vector<Entry> result;
        const auto settings=IRUserPreferences::read(preferences);
        if (references) {
            for (int i=0; i<publicIRFactoryCount; ++i)
                result.push_back({{},IRMetadata::factoryFilename(i),IRMetadata::factory(i),i+1,false});
        }
        juce::StringArray seen;
        const auto addFile=[&](const juce::File& f) {
            if(!f.existsAsFile() || !f.hasFileExtension("wav;aif;aiff") || seen.contains(IRUserPreferences::identity(f)) || IRUserPreferences::hidden(settings,f))return true;
            if(seen.size()>=512) {if(report)report->truncated=true;return false;}
            seen.add(IRUserPreferences::identity(f));
            if(report)++report->examinedFiles;
            const auto validationError=validateFile(f);
            if(report && validationError.isNotEmpty())++report->invalidFiles;
            auto tags=IRMetadata::filenameHints(f.getFileName());
            const auto sidecar=juce::File(f.getFullPathName()+".json");
            if(sidecar.existsAsFile() && sidecar.getSize()<=16384) {
                const auto json=juce::JSON::parse(sidecar);
                if(json.isObject())tags=IRMetadata::fromJSON(json);
            }
            IRUserPreferences::apply(settings,f,tags);
            result.push_back({f,f.getFileName(),tags,0,false,{},false,validationError});
            return true;
        };
        for(const auto& folder:folders) {
            if(!folder.isDirectory())continue;
            for(const auto& item:juce::RangedDirectoryIterator(folder,true,"*",juce::File::findFiles))
                if(!addFile(item.getFile())) {labelEntries(result);return result;}
        }
        if(references)for(const auto& file:IRUserPreferences::standaloneFiles(settings))if(!addFile(file))break;
        labelEntries(result);return result;
    }
    static bool matchesExpectedHash(const juce::File& file,const juce::String& expected) {
        return expected.isEmpty() || (file.existsAsFile() && juce::SHA256(file).toHexString().equalsIgnoreCase(expected));
    }
    static juce::String validateEncodedIR(const juce::MemoryBlock& data) {
        if(data.getSize()<44 || data.getSize()>4*1024*1024) return "Use a WAV/AIFF file under 4 MB.";
        juce::AudioFormatManager formats;formats.registerFormat(new juce::WavAudioFormat(),true);formats.registerFormat(new juce::AiffAudioFormat(),false);
        auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(std::make_unique<juce::MemoryInputStream>(data,false)));
        if(!reader) return "Cannot decode this WAV/AIFF file.";
        if(reader->numChannels<1 || reader->numChannels>2 || reader->sampleRate<8000 || reader->sampleRate>384000 || reader->lengthInSamples<8 || reader->lengthInSamples>juce::int64(reader->sampleRate))
            return "Use mono/stereo, 8-384 kHz, 8 samples to 1 second.";
        juce::AudioBuffer<float> block((int)reader->numChannels,1024);double energy=0;
        for(juce::int64 offset=0;offset<reader->lengthInSamples;offset+=1024) {
            const int samples=(int)juce::jmin(juce::int64(1024),reader->lengthInSamples-offset);
            if(!reader->read(&block,0,samples,offset,true,true)) return "IR data is incomplete.";
            for(int c=0;c<block.getNumChannels();++c)for(int n=0;n<samples;++n) {
                const float value=block.getSample(c,n);
                if(!std::isfinite(value) || std::abs(value)>32.f) return "IR contains invalid samples.";
                energy+=double(value)*value;
            }
        }
        return energy<1e-12 ? "IR is silent." : juce::String{};
    }

    // User-initiated ZIP import has no bundled private catalog or pack promotion.
    // Validate every audio member before writing; never extract arbitrary paths
    // or import NAM weights/executables that coexist in an archive.
    static juce::Result importPersonalPack(const juce::File& file,const juce::File& destination,int& count) {
        count=0;
        if(!file.existsAsFile() || file.getSize()>32*1024*1024)
            return juce::Result::fail("Choose your IR ZIP (maximum 32 MB), or use ADD FOLDER.");
        juce::ZipFile zip(file);
        if(zip.getNumEntries()>4096)return juce::Result::fail("The archive contains too many entries.");
        struct Pending {juce::String name;juce::MemoryBlock audio;juce::var tags;};
        std::vector<Pending> pending;juce::StringArray seen;juce::int64 totalBytes=0;
        const auto archiveHash=juce::SHA256(file).toHexString();
        for(int i=0;i<zip.getNumEntries();++i) {
            const auto* entry=zip.getEntry(i);if(!entry)continue;
            const auto path=entry->filename.replaceCharacter('\\','/');
            const auto leaf=path.fromLastOccurrenceOf("/",false,false);
            if(!leaf.endsWithIgnoreCase(".wav") && !leaf.endsWithIgnoreCase(".aif") && !leaf.endsWithIgnoreCase(".aiff"))continue;
            juce::StringArray parts;parts.addTokens(path,"/","");
            if(path.startsWith("/") || path.contains(":") || parts.contains("..") || parts.contains(".")
                || leaf.isEmpty() || seen.contains(leaf,true))
                return juce::Result::fail("The IR archive contains duplicate or unsafe filenames.");
            seen.add(leaf);
            if(entry->uncompressedSize<44 || entry->uncompressedSize>4*1024*1024
                || pending.size()>=128 || (totalBytes+=entry->uncompressedSize)>64*1024*1024)
                return juce::Result::fail("The IR archive exceeds its audio size limits.");
            std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(i));juce::MemoryBlock data;
            if(!stream || stream->readIntoMemoryBlock(data,entry->uncompressedSize)!=entry->uncompressedSize)
                return juce::Result::fail("IR archive data is incomplete.");
            const auto validation=validateEncodedIR(data);
            if(validation.isNotEmpty())return juce::Result::fail(validation);
            const auto target=destination.getChildFile(leaf);
            const auto sidecar=juce::File(target.getFullPathName()+".json");
            if(target.isSymbolicLink() || sidecar.isSymbolicLink() || target.isDirectory() || sidecar.isDirectory()
                || (target.existsAsFile() && juce::SHA256(target)!=juce::SHA256(data)))
                return juce::Result::fail("An existing IR has the same name and different data. Choose another folder.");
            auto metadata=IRMetadata::filenameHints(leaf).json();
            if(auto* object=metadata.getDynamicObject()) {
                object->setProperty("audio_sha256",juce::SHA256(data).toHexString());
                object->setProperty("source_archive_sha256",archiveHash);
                object->setProperty("import_origin","user-selected ZIP; redistribution rights not assessed");
            }
            pending.push_back({leaf,std::move(data),metadata});
        }
        if(pending.empty())return juce::Result::fail("No valid WAV/AIFF files found. Choose your IR archive or use ADD FOLDER.");
        if(auto result=destination.createDirectory();result.failed())return result;
        for(const auto& item:pending) {
            const auto target=destination.getChildFile(item.name);
            const auto sidecar=juce::File(target.getFullPathName()+".json");
            // An identical existing import and its user-authored tags belong to
            // the user. Re-import does not overwrite either.
            if(!target.existsAsFile() && !target.replaceWithData(item.audio.getData(),item.audio.getSize()))
                return juce::Result::fail("Could not save the IR. Check folder permissions.");
            if(!sidecar.existsAsFile() && !sidecar.replaceWithText(juce::JSON::toString(item.tags)))
                return juce::Result::fail("Could not save the IR metadata. Check folder permissions.");
            ++count;
        }
        return juce::Result::ok();
    }
};
}
