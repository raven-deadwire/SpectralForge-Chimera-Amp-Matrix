#include "IRCollection.h"
#include <iostream>
#include <stdexcept>
namespace {
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void writeIR(const juce::File& file,bool silent=false) {
    auto stream=file.createOutputStream();require(stream!=nullptr,"Cannot create IR fixture");
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,1,24,{},0));
    require(writer!=nullptr,"Cannot create WAV writer");
    juce::AudioBuffer<float> samples(1,256);samples.clear();
    if(!silent)samples.setSample(0,0,.5f);
    require(writer->writeFromAudioSampleBuffer(samples,0,256),"Cannot write WAV fixture");
}
}
int main() {
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Chimera-IR-scan",{},false);
    struct Cleanup {juce::File root;~Cleanup(){root.deleteRecursively();}} cleanup{root};
    try {
        using C=spectralforge::IRCollection;
        require(root.createDirectory().wasOk(),"Cannot create scan fixture directory");
        const auto bass=root.getChildFile("Bass cabinet 10.wav"),guitar=root.getChildFile("Guitar cabinet 12.wav"),bad=root.getChildFile("Corrupt bass.wav"),silent=root.getChildFile("Silent.wav");
        writeIR(bass);writeIR(guitar);writeIR(silent,true);bad.replaceWithText("This is not audio");
        auto tags=spectralforge::IRMetadata::filenameHints(bass.getFileName());tags.values[2]="10";
        juce::File(bass.getFullPathName()+".json").replaceWithText(juce::JSON::toString(tags.json()));
        C::ScanReport report;const auto entries=C::scan({root,root},false,&report);
        require(entries.size()==4 && report.examinedFiles==4 && report.invalidFiles==2 && !report.truncated,"Scan counts or duplicate root handling incorrect");
        int bassReady=0,invalid=0,guitarReady=0;
        for(const auto& e:entries) {
            if(C::matches(e,{},"10",C::Instrument::bass,C::Availability::installed))++bassReady;
            if(C::matches(e,{},{},C::Instrument::guitarOther,C::Availability::ready))++guitarReady;
            if(C::matches(e,{},{},C::Instrument::all,C::Availability::invalid)) {++invalid;require(!e.ready() && e.details().contains("INVALID FILE"),"Invalid file is loadable or lacks reason");}
        }
        require(bassReady==1 && guitarReady==1 && invalid==2,"Instrument/availability/diameter filters are not independent");
        auto missing=C::Entry{};missing.name="Bass reference.wav";missing.external=true;
        require(C::matches(missing,{},{},C::Instrument::bass,C::Availability::missing),"Missing bass reference hidden");
        require(C::matches(missing,{},{},C::Instrument::bass,C::Availability::external),"External bass download hidden");
        require(!C::matches(missing,{},{},C::Instrument::bass,C::Availability::ready),"Missing bass reference shown as ready");
        const auto references=C::scan({},true);
        int factories=0;for(const auto& e:references)if(C::matches(e,{},{},C::Instrument::all,C::Availability::factory))++factories;
        require(factories==2,"Factory assets lost or counted as imported files");
        const auto capped=root.getChildFile("large");require(capped.createDirectory().wasOk(),"Cannot create capped scan folder");
        for(int i=0;i<512;++i)require(bass.copyFileTo(capped.getChildFile("IR-"+juce::String(i)+".wav")),"Cannot create scan limit fixture");
        auto rows=C::scan({capped},false,&report);
        require(rows.size()==512 && report.examinedFiles==512 && !report.truncated,"Exactly 512 files incorrectly reported as truncated");
        require(bass.copyFileTo(capped.getChildFile("IR-extra.wav")),"Cannot create overflow fixture");
        rows=C::scan({capped},false,&report);
        require(rows.size()==512 && report.examinedFiles==512 && report.truncated,"Scan silently omitted files beyond its limit");
        std::cout<<"PASS: independent instrument/status/diameter filters, missing/external/factory separation, corrupt/silent file rejection, duplicate-root suppression, explicit 512 vs 513 scan limit\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
