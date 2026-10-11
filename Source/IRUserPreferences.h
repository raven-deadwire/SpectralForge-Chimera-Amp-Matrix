#pragma once
#include "IRMetadata.h"
#include <vector>

namespace spectralforge {
// Library preferences never rewrite source WAVs/sidecars or the IR embedded in a
// project. Paths are identities here: equal basenames in separate packs differ.
struct IRUserPreferences {
    static juce::File file() {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("SpectralForge/Chimera/ir-library.json");
    }
    static juce::var read(const juce::File& storage=file()) {
        if(!storage.existsAsFile() || storage.getSize()>1024*1024) return {};
        return juce::JSON::parse(storage);
    }
    static juce::String identity(const juce::File& path) {
        auto result=path.getFullPathName();
       #if JUCE_WINDOWS
        result=result.toLowerCase();
       #endif
        return result;
    }
    static juce::var record(const juce::var& settings,const char* group,const juce::File& path) {
        const auto rows=settings.getProperty(group,{});
        if(const auto* array=rows.getArray())for(const auto& row:*array)
            if(row.getProperty("path",{}).toString()==identity(path)) return row;
        return {};
    }
    static bool hidden(const juce::var& settings,const juce::File& path) {
        return (bool)record(settings,"files",path).getProperty("hidden",false);
    }
    static void apply(const juce::var& settings,const juce::File& path,IRMetadata& metadata) {
        auto row=record(settings,"files",path);
        if(!row.hasProperty("instrument")) {
            int longest=-1;
            const auto folders=settings.getProperty("folders",{});
            if(const auto* array=folders.getArray())for(const auto& folder:*array) {
                const auto name=folder.getProperty("path",{}).toString();
                if(folder.hasProperty("instrument") && juce::File::isAbsolutePath(name) && path.isAChildOf(juce::File(name)) && name.length()>longest) {
                    row=folder;longest=name.length();
                }
            }
        }
        if(row.hasProperty("instrument"))metadata.instrument=IRMetadata::parseInstrument(row["instrument"].toString());
    }
    static std::vector<juce::File> standaloneFiles(const juce::var& settings) {
        std::vector<juce::File> result;
        const auto rows=settings.getProperty("files",{});
        if(const auto* array=rows.getArray())for(const auto& row:*array) {
            const auto path=row.getProperty("path",{}).toString();
            if((bool)row.getProperty("standalone",false) && juce::File::isAbsolutePath(path))result.emplace_back(path);
        }
        return result;
    }
    static juce::Result update(const juce::File& path,const char* group,const juce::String& instrument,
                               bool hide,bool standalone,const juce::File& storage=file(),const juce::String& referenceName={}) {
        if(path==juce::File{})return juce::Result::fail("No IR file selected.");
        // Separate plugin instances can edit the same application library.
        juce::InterProcessLock lock("Chimera-IR-library-"+juce::String(storage.getFullPathName().hashCode64()));
        if(!lock.enter(1000))return juce::Result::fail("IR library is busy. Try again.");
        struct Unlock {juce::InterProcessLock& lock;~Unlock(){lock.exit();}} unlock{lock};
        auto settings=read(storage);
        if(storage.existsAsFile() && !settings.isObject())return juce::Result::fail("Cannot read IR library preferences. Existing preferences were kept.");
        if(!settings.isObject())settings=new juce::DynamicObject();
        auto rows=settings.getProperty(group,juce::Array<juce::var>{});
        if(!rows.isArray())return juce::Result::fail("Invalid IR library preferences. Existing preferences were kept.");
        auto* array=rows.getArray();
        juce::var row;int index=-1;
        for(int i=0;i<array->size();++i)if((*array)[i].getProperty("path",{}).toString()==identity(path)){row=(*array)[i];index=i;break;}
        if(!row.isObject())row=new juce::DynamicObject();
        auto* object=row.getDynamicObject();object->setProperty("path",identity(path));
        if(instrument.isNotEmpty())object->setProperty("instrument",instrument);
        if(juce::String(group)=="files") {
            object->setProperty("hidden",hide);
            if(standalone)object->setProperty("standalone",true);
            if(referenceName.isNotEmpty())object->setProperty("reference",referenceName);
        }
        if(index>=0)array->set(index,row);else array->add(row);
        settings.getDynamicObject()->setProperty(group,rows);
        settings.getDynamicObject()->setProperty("version",1);
        const auto json=juce::JSON::toString(settings);
        if(json.getNumBytesAsUTF8()>1024*1024)return juce::Result::fail("The IR library preference limit has been reached.");
        if(auto result=storage.getParentDirectory().createDirectory();result.failed())return result;
        juce::TemporaryFile temporary(storage);
        return temporary.getFile().replaceWithText(json) && temporary.overwriteTargetFileWithTemporary()
            ? juce::Result::ok() : juce::Result::fail("Cannot save IR library preferences.");
    }
};
}
