#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <regex>
#include "IRReferenceCatalog.h"

namespace spectralforge {
struct IRMetadata {
    static constexpr std::array<const char*,12> keys{"speaker","cabinet","diameter_in","microphone","position","distance","angle","preamp","author","source","license","notes"};
    static constexpr std::array<const char*,12> labels{"Speaker model","Cabinet / configuration","Speaker diameter (in)","Microphone","Position on cone / unit","Distance from grille","Off-axis angle","Mic preamp","Creator","Source URL","License","Provenance / notes"};
    std::array<juce::String,12> values;
    juce::var json() const {auto* object=new juce::DynamicObject();for(size_t i=0;i<keys.size();++i)object->setProperty(keys[i],values[i]);return object;}
    static IRMetadata fromJSON(const juce::var& json) {IRMetadata result;for(size_t i=0;i<keys.size();++i)result.values[i]=json.getProperty(keys[i],{}).toString().substring(0,i==11 ? 2048 : 512);return result;}
    juce::String summary() const {juce::StringArray parts;for(int i:{0,2,3,4})if(values[(size_t)i].isNotEmpty())parts.add(values[(size_t)i]+(i==2 ? " in" : ""));return parts.isEmpty() ? "IR details unknown - add tags" : parts.joinIntoString(" / ");}
    static IRMetadata filenameHints(const juce::String& name) {
        static const auto catalog=juce::JSON::parse(referenceIRCatalog);
        if(const auto* entries=catalog.getArray())for(const auto& entry:*entries)if(entry.getProperty("file",{}).toString()==name){auto known=fromJSON(entry);known.values[11]+=" Catalog association by filename; verify file hash against the source catalog.";return known;}
        IRMetadata result;std::smatch match;const auto s=name.toStdString();
        if(std::regex_search(s,match,std::regex("([12468])x(8|10|12|15|18)([^0-9]|$)",std::regex::icase))){result.values[1]=juce::String(match[1].str()+"x"+match[2].str());result.values[2]=juce::String(match[2].str());}
        if(name.containsIgnoreCase("V30"))result.values[0]="Celestion Vintage 30";
        for(const auto* mic:{"SM57","SM7B","MD421","MD441","M201","AT4050","AT2020","AT2021","R121","R10","M160","RE20","KM184"})if(name.containsIgnoreCase(mic))result.values[3]=mic;
        if(name.containsIgnoreCase("center") || name.containsIgnoreCase("centre"))result.values[4]="Center (filename hint)";
        else if(name.containsIgnoreCase("edge"))result.values[4]="Edge (filename hint)";
        result.values[11]="Filename hints only. Verify against the creator's capture notes; blank fields are unknown.";
        return result;
    }
};
}
