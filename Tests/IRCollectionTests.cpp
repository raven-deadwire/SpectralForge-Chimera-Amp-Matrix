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
int main(int argc,char** argv) {
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
        int ravenMissing=0;juce::StringArray ravenLabels;
        for(const auto& e:references)if(e.name.startsWith("Mar1960_")) {
            require(e.reference && e.external && !e.ready(),"Uninstalled Raven capture incorrectly marked as bundled audio");
            require(e.tags.values[8]=="The other John Browne" && e.tags.values[9].contains("_TzZ_FVMGfg"),"Raven capture provenance lost");
            require(!ravenLabels.contains(e.displayName()),"Raven microphone positions have duplicate labels");
            ravenLabels.add(e.displayName());++ravenMissing;
        }
        require(ravenMissing==4,"Raven three positions or V30 comparison missing from library catalog");
        const auto ravenHints=spectralforge::IRMetadata::filenameHints("Mar1960_Raven_SM57_In.wav");
        require(ravenHints.values[0]=="Celestion G12-100 Raven" && ravenHints.values[4]=="In","Raven filename metadata not recognized");
        const auto hartkeMeta=C::approvedPackMetadata(C::ApprovedPack::hartkeHyDrive410,"Hartke HyDrive 410 _ SM57.wav");
        require(hartkeMeta.isObject() && hartkeMeta["cabinet"].toString()=="Hartke HyDrive 410"
            && hartkeMeta["microphone"].toString()=="Shure SM57"
            && hartkeMeta["source"].toString().contains("66570"),"Hartke approved-pack metadata mapping failed");
        const auto marshallMeta=C::approvedPackMetadata(C::ApprovedPack::marshall1960BV,"Marshall G12 1 SM57 3.wav");
        require(marshallMeta.isObject() && marshallMeta["speaker"].toString()=="G12T75"
            && marshallMeta["cabinet"].toString()=="Marshall 1960BV 4x12"
            && marshallMeta["position"].toString().contains("Speaker 1")
            && marshallMeta["source"].toString().contains("51086"),"Marshall approved-pack metadata mapping failed");
        require(!C::approvedPackMetadata(C::ApprovedPack::marshall1960BV,"unexpected.wav").isObject(),"Approved pack accepted an unexpected filename");
        const auto fakeZip=root.getChildFile("invalid-raven.zip"),fakeDestination=root.getChildFile("invalid-raven-import");
        {
            juce::ZipFile::Builder builder;builder.addFile(bass,6,"IR/Raven/Mar1960_Raven_SM57_In.wav");
            auto stream=fakeZip.createOutputStream();require(stream!=nullptr && builder.writeToStream(*stream,nullptr),"Cannot create Raven mismatch fixture");
        }
        int imported=0;
        require(C::importPersonalPack(fakeZip,fakeDestination,imported).failed() && imported==0 && !fakeDestination.exists(),"Raven hash mismatch created an installed capture");
        // Optional private pack exercises the actual four WAVs without checking
        // them into the repository or making them a public CI fixture.
        if(argc>1) {
            const auto privatePack=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
            const auto destination=root.getChildFile("raven-import");
            require(C::importPersonalPack(privatePack,destination,imported).wasOk() && imported==4,"Private Raven pack did not import all four verified captures");
            const auto restored=C::scan({destination},true);
            int ravenReady=0,ready=0;
            for(const auto& e:restored) {
                if(e.ready())++ready;
                if(e.name.startsWith("Mar1960_")) {require(e.ready() && e.reference && e.validationError.isEmpty(),"Imported Raven file did not become selectable");++ravenReady;}
            }
            require(ravenReady==4 && ready==6,"Private Raven library scan did not restore four imports plus two factory captures");
            std::cout<<"PASS: private Raven ZIP imported four hash-verified WAVs; library scan found six ready captures including factory IRs\n";
        }
        const auto capped=root.getChildFile("large");require(capped.createDirectory().wasOk(),"Cannot create capped scan folder");
        for(int i=0;i<512;++i)require(bass.copyFileTo(capped.getChildFile("IR-"+juce::String(i)+".wav")),"Cannot create scan limit fixture");
        auto rows=C::scan({capped},false,&report);
        require(rows.size()==512 && report.examinedFiles==512 && !report.truncated,"Exactly 512 files incorrectly reported as truncated");
        require(bass.copyFileTo(capped.getChildFile("IR-extra.wav")),"Cannot create overflow fixture");
        rows=C::scan({capped},false,&report);
        require(rows.size()==512 && report.examinedFiles==512 && report.truncated,"Scan silently omitted files beyond its limit");
        std::cout<<"PASS: independent instrument/status/diameter filters, missing/external/factory separation, Raven capture metadata and hash rejection, corrupt/silent file rejection, duplicate-root suppression, explicit 512 vs 513 scan limit\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
