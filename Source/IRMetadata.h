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
    // Display text is presentation metadata, never an asset ID or a host parameter.
    juce::String displayLabel;
    juce::String sourceFilename,sourceHash,preparation;
    juce::var json() const {auto* object=new juce::DynamicObject();for(size_t i=0;i<keys.size();++i)object->setProperty(keys[i],values[i]);if(displayLabel.isNotEmpty())object->setProperty("display_name",displayLabel);if(sourceFilename.isNotEmpty())object->setProperty("original_file",sourceFilename);if(sourceHash.isNotEmpty())object->setProperty("original_sha256",sourceHash);if(preparation.isNotEmpty())object->setProperty("processing",preparation);return object;}
    static IRMetadata fromJSON(const juce::var& json) {IRMetadata result;for(size_t i=0;i<keys.size();++i)result.values[i]=json.getProperty(keys[i],{}).toString().substring(0,i==11 ? 2048 : 512);result.displayLabel=json.getProperty("display_name",{}).toString().substring(0,96);result.sourceFilename=leafName(json.getProperty("original_file",{}).toString()).substring(0,512);result.sourceHash=json.getProperty("original_sha256",{}).toString().substring(0,64);result.preparation=json.getProperty("processing",{}).toString().substring(0,1024);return result;}
    static juce::String leafName(const juce::String& name) {return name.replaceCharacter('\\','/').fromLastOccurrenceOf("/",false,false).replaceCharacters("\r\n\t","   ").trim();}
    static juce::String boundedLabel(const juce::String& text,int maximum=56) {const auto clean=text.replaceCharacters("\r\n\t","   ").trim();return clean.length()>maximum ? clean.substring(0,maximum-1).trimEnd()+juce::String::fromUTF8("\xe2\x80\xa6") : clean;}
    juce::String shortLabel(const juce::String& filename) const {
        if(displayLabel.isNotEmpty()) return boundedLabel(displayLabel);
        auto cabinet=values[1].upToFirstOccurrenceOf(" (",false,false).upToFirstOccurrenceOf(";",false,false).trim();
        auto mic=values[3].upToFirstOccurrenceOf(" (",false,false).trim();
        for(const auto* brand:{"Audio Technica ","Audio-Technica ","Beyerdynamic ","Sennheiser ","Shure ","sE Electronics "})
            if(mic.startsWithIgnoreCase(brand))mic=mic.substring((int)std::char_traits<char>::length(brand));
        if(cabinet.isNotEmpty() && !cabinet.equalsIgnoreCase("Unspecified"))
            return boundedLabel(cabinet+(mic.isNotEmpty() ? juce::String::fromUTF8(" \xe2\x80\x94 ")+mic : juce::String{}));
        auto leaf=leafName(filename);
        if(leaf.endsWithIgnoreCase(".wav") || leaf.endsWithIgnoreCase(".aif"))leaf=leaf.dropLastCharacters(4);
        else if(leaf.endsWithIgnoreCase(".aiff"))leaf=leaf.dropLastCharacters(5);
        return boundedLabel(leaf.replaceCharacter('_',' '));
    }
    juce::String details(const juce::String& filename) const {
        juce::String text="File: "+leafName(filename);
        if(sourceFilename.isNotEmpty())text+="\nSource WAV: "+sourceFilename;
        if(sourceHash.isNotEmpty())text+="\nSource SHA-256: "+sourceHash;
        if(preparation.isNotEmpty())text+="\nPreparation: "+preparation;
        for(size_t i=0;i<values.size();++i)if(values[i].isNotEmpty())text+="\n"+juce::String(labels[i])+": "+values[i];
        return text;
    }
    static juce::String factoryFilename(int index) {return index==0 ? "Engl Celestion V30 SM57 center-01.wav" : "Jensen Cab SM57 center.wav";}
    static IRMetadata factory(int index) {
        IRMetadata m;m.displayLabel=index==0 ? juce::String::fromUTF8("V30 — SM57") : juce::String::fromUTF8("Jensen — SM57");
        m.values[0]=index==0 ? "Celestion Vintage 30" : "Jensen (model unspecified)";
        m.values[1]=index==0 ? "ENGL (configuration unspecified)" : "Unspecified";m.values[3]="Shure SM57";m.values[4]="Center";
        m.values[8]="jesterdyne";m.values[9]=index==0 ? "https://freesound.org/s/116735/" : "https://freesound.org/s/116743/";m.values[10]="CC BY 4.0";m.values[11]="Factory guitar IR. Original audio unchanged. Distance, angle and speaker diameter are not documented by this asset.";
        return m;
    }
    juce::String summary() const {juce::StringArray parts;for(int i:{0,2,3,4})if(values[(size_t)i].isNotEmpty())parts.add(values[(size_t)i]+(i==2 ? " in" : ""));return parts.isEmpty() ? "IR details unknown - add tags" : parts.joinIntoString(" / ");}
    static IRMetadata filenameHints(const juce::String& name) {
        static const auto catalog=juce::JSON::parse(referenceIRCatalog);
        static const auto external=juce::JSON::parse(externalBassIRCatalog);
        if(const auto* entries=external.getArray())for(const auto& entry:*entries)if(entry.getProperty("file",{}).toString()==name){auto known=fromJSON(entry);known.values[11]+=" Filename association only; the catalog records the original SHA-256.";return known;}
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
