#include "IRCollection.h"
#include "MicrophoneCatalogTests.h"
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
        runMicrophoneCatalogTests();
        using C=spectralforge::IRCollection;
        require(root.createDirectory().wasOk(),"Cannot create scan fixture directory");
        const auto bass=root.getChildFile("Bass cabinet 10.wav"),guitar=root.getChildFile("Guitar cabinet 12.wav"),bad=root.getChildFile("Corrupt bass.wav"),silent=root.getChildFile("Silent.wav");
        writeIR(bass);writeIR(guitar);writeIR(silent,true);bad.replaceWithText("This is not audio");
        auto tags=spectralforge::IRMetadata::filenameHints(bass.getFileName());tags.values[2]="10";tags.instrument=spectralforge::IRMetadata::Instrument::bass;
        auto guitarTags=spectralforge::IRMetadata{};guitarTags.instrument=spectralforge::IRMetadata::Instrument::guitar;
        juce::File(guitar.getFullPathName()+".json").replaceWithText(juce::JSON::toString(guitarTags.json()));
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
        {
            auto capture=C::Entry{};capture.file=guitar;capture.name="Cab_SM57.wav";
            capture.tags.values[3]="Audio-Technica AT4050 (creator tag)";
            const auto original=juce::JSON::toString(capture.tags.json());
            require(C::matches(capture,"Condenser 4050",{},C::Instrument::all,C::Availability::ready,"condenser-4050")
                && !C::matches(capture,{},{},C::Instrument::all,C::Availability::all,"dynamic-57"),
                "Explicit microphone metadata did not override a conflicting filename or alias search failed");
            require(juce::JSON::toString(capture.tags.json())==original,"Catalog matching rewrote capture provenance");
            capture.tags.values[3]="Audix i5";
            require(C::matches(capture,{},{},C::Instrument::all,C::Availability::ready,"other")
                && !C::matches(capture,{},{},C::Instrument::all,C::Availability::ready,"chimera-strike"),
                "Non-roster capture was hidden or mislabeled as a Chimera original");
            capture.tags.values[3]="Chimera Strike";capture.file=juce::File{};
            require(C::matches(capture,{},{},C::Instrument::all,C::Availability::missing,"chimera-strike")
                && !C::matches(capture,{},{},C::Instrument::all,C::Availability::ready,"chimera-strike"),
                "A catalog identity made a missing microphone capture loadable");
        }
        auto missing=C::Entry{};missing.name="Bass reference.wav";missing.external=true;missing.tags.instrument=spectralforge::IRMetadata::Instrument::bass;
        require(C::matches(missing,{},{},C::Instrument::bass,C::Availability::missing),"Missing bass reference hidden");
        require(C::matches(missing,{},{},C::Instrument::bass,C::Availability::external),"External bass download hidden");
        require(!C::matches(missing,{},{},C::Instrument::bass,C::Availability::ready),"Missing bass reference shown as ready");
        const auto references=C::scan({},true,nullptr,root.getChildFile("release-empty-preferences.json"));
        int factories=0;for(const auto& e:references)if(C::matches(e,{},{},C::Instrument::all,C::Availability::factory))++factories;
        require(factories==2,"Factory assets lost or counted as imported files");
        require(references.size()==2,"Release catalog includes an unapproved/private reference capture");
        for(const auto& e:references)
            require(e.ready() && !e.reference && !e.external && e.tags.values[10]=="CC BY 4.0",
                "Release catalog contains an unavailable or unattributed factory entry");
        const auto formerPrivateHints=spectralforge::IRMetadata::filenameHints("Mar1960_Raven_SM57_In.wav");
        require(formerPrivateHints.values[8].isEmpty() && formerPrivateHints.values[9].isEmpty()
            && formerPrivateHints.values[10].isEmpty(),"A private filename inherited bundled provenance");
        const auto bassHash=juce::SHA256(bass).toHexString();
        require(C::matchesExpectedHash(bass,bassHash),"Expected per-IR hash did not match the fixture");
        require(!C::matchesExpectedHash(bass,juce::String::repeatedString("0",64)),"Wrong per-IR hash was accepted");
        const auto makeArchive=[&](const juce::File& archive,std::initializer_list<std::pair<juce::File,juce::String>> files) {
            juce::ZipFile::Builder builder;
            for(const auto& item:files)builder.addFile(item.first,6,item.second);
            auto stream=archive.createOutputStream();
            require(stream!=nullptr && builder.writeToStream(*stream,nullptr),"Cannot create user IR archive fixture");
        };
        const auto personalZip=root.getChildFile("user-irs.zip"),destination=root.getChildFile("user-import");
        makeArchive(personalZip,{{bass,"collection/Bass.wav"},{guitar,"collection/Guitar.wav"},{bad,"unused/model.nam"}});
        int imported=0;
        require(C::importPersonalPack(personalZip,destination,imported).wasOk() && imported==2,
            "User-selected generic WAV archive could not be imported");
        require(!destination.getChildFile("model.nam").exists(),"IR import copied model weights from the archive");
        const auto importedBass=destination.getChildFile("Bass.wav");
        require(juce::SHA256(importedBass).toHexString()==bassHash,"User archive import changed original IR bytes");
        const auto importedMetadata=juce::File(importedBass.getFullPathName()+".json");
        const auto firstMetadata=juce::JSON::parse(importedMetadata);
        require(firstMetadata["audio_sha256"].toString()==bassHash
            && firstMetadata["source_archive_sha256"].toString()==juce::SHA256(personalZip).toHexString()
            && firstMetadata["license"].toString().isEmpty(),"Generic import fabricated a license or lost byte provenance");
        require(importedMetadata.replaceWithText("{\"notes\":\"User-owned metadata\"}"),"Cannot author user import tags");
        require(C::importPersonalPack(personalZip,destination,imported).wasOk() && imported==2
            && juce::JSON::parse(importedMetadata)["notes"].toString()=="User-owned metadata",
            "Re-import overwrote existing user IR metadata");
        const auto installed=C::scan({destination},true,nullptr,root.getChildFile("release-empty-preferences.json"));
        require(installed.size()==4,"Release library did not show two explicit user imports plus two factory captures");
        for(const auto& e:installed)require(e.ready() && !e.reference && !e.external,
            "User import was converted into a private catalog reference");
        for(int test=0;test<3;++test) {
            const auto archive=root.getChildFile("rejected-"+juce::String(test)+".zip");
            const auto output=root.getChildFile("rejected-"+juce::String(test));
            if(test==0)makeArchive(archive,{{bass,"valid.wav"},{bad,"invalid.wav"}});
            if(test==1)makeArchive(archive,{{bass,"one/same.wav"},{guitar,"two/SAME.wav"}});
            if(test==2)makeArchive(archive,{{bass,"../unsafe.wav"}});
            require(C::importPersonalPack(archive,output,imported).failed() && imported==0 && !output.exists(),
                "Invalid/duplicate/traversal IR archive wrote a partial import");
        }
        const auto collision=root.getChildFile("collision");
        require(collision.createDirectory().wasOk(),"Cannot create import collision fixture");
        const auto existing=collision.getChildFile("Bass.wav");writeIR(existing,true);
        const auto before=juce::SHA256(existing).toHexString();
        require(C::importPersonalPack(personalZip,collision,imported).failed() && imported==0
            && juce::SHA256(existing).toHexString()==before,"User import overwrote a different existing IR");
        std::cout<<"PASS: release catalog contains only two attributed factory IRs; generic user import preserves bytes and metadata\n";
        {
            using M=spectralforge::IRMetadata;using P=spectralforge::IRUserPreferences;
            const auto folder=root.getChildFile("personal"),other=root.getChildFile("other-personal");
            require(folder.createDirectory().wasOk() && other.createDirectory().wasOk(),"Cannot create user-library fixtures");
            const auto a=folder.getChildFile("capture.wav"),b=other.getChildFile("capture.wav"),settings=root.getChildFile("library.json");
            writeIR(a);writeIR(b);const auto originalHash=juce::SHA256(a).toHexString();
            auto items=C::scan({folder,other},false,nullptr,settings);
            require(items.size()==2 && items[0].tags.instrument==M::Instrument::unspecified && !C::matches(items[0],{},{},C::Instrument::guitarOther,C::Availability::ready),"Unknown personal IR silently classified as guitar");
            require(M::filenameHints("Bass 8x10.wav").instrument==M::Instrument::unspecified,"Filename guessed personal instrument type");
            require(P::update(folder,"folders","bass",false,false,settings).wasOk(),"Cannot assign folder instrument");
            items=C::scan({folder,other},false,nullptr,settings);
            require(items[0].bass() && !items[1].bass(),"Folder classification leaked across equal basenames");
            require(C::classify(items[0],M::Instrument::guitar,settings).wasOk(),"Cannot override existing user IR classification");
            items=C::scan({folder,other},false,nullptr,settings);
            require(items[0].tags.instrument==M::Instrument::guitar,"Per-file instrument did not override folder or survive scan");
            require(C::classify(items[0],M::Instrument::unspecified,settings).wasOk(),"Cannot reset instrument to unspecified");
            items=C::scan({folder,other},false,nullptr,settings);
            require(items[0].tags.instrument==M::Instrument::unspecified,"Explicit unspecified incorrectly inherited folder type");
            require(C::remove(items[0],settings).wasOk(),"Cannot remove imported IR");
            items=C::scan({folder,other},false,nullptr,settings);
            require(items.size()==1 && items[0].file==b && a.existsAsFile() && juce::SHA256(a).toHexString()==originalHash,"Remove deleted source data, failed persistence, or removed equal basename");
            require(C::remove(references[0],settings).failed() && C::classify(references[0],M::Instrument::bass,settings).failed(),"Factory IR can be removed or reclassified");
            require(C::rememberFile(a,M::Instrument::bass,settings).wasOk(),"Cannot re-add removed standalone IR");
            items=C::scan({},true,nullptr,settings);int standalone=0;
            for(const auto& item:items)if(item.file==a){require(item.bass(),"Explicit standalone bass type lost");++standalone;}
            require(standalone==1,"OPEN IR did not register standalone file");
            items=C::scan({folder},true,nullptr,settings);standalone=0;for(const auto& item:items)if(item.file==a)++standalone;
            require(standalone==1,"Standalone file duplicated by folder discovery");
            auto metadata=M{};P::apply(P::read(settings),a,metadata);
            require(M::fromJSON(metadata.json()).instrument==M::Instrument::bass,"Instrument lost in embedded preset metadata roundtrip");
            auto copy=metadata;copy.values[11]="Bass cabinet";copy.instrument=M::Instrument::guitar;
            require(M::fromJSON(copy.json()).instrument==M::Instrument::guitar,"Legacy notes overrode explicit instrument");
            const auto corrupt=root.getChildFile("broken-preferences.json");corrupt.replaceWithText("broken");
            require(C::rememberFile(a,M::Instrument::bass,corrupt).failed() && corrupt.loadFileAsString()=="broken","Corrupt preferences overwritten");
            require(C::rememberFile(bad,M::Instrument::bass,settings).failed(),"Invalid audio registered as a standalone IR");
            const auto autoSettings=root.getChildFile("auto-library.json");
            require(C::rememberFile(bass,M::Instrument::unspecified,autoSettings,true).wasOk(),"Auto import failed");
            auto autoItems=C::scan({},true,nullptr,autoSettings);bool preserved=false;
            for(const auto& item:autoItems)if(item.file==bass)preserved=item.bass();
            require(preserved,"Default import overwrote existing sidecar Bass type");
            require(C::rememberFile(a,M::Instrument::unspecified,autoSettings,true).wasOk(),"Unknown auto import failed");
            autoItems=C::scan({},true,nullptr,autoSettings);bool unknown=false;
            for(const auto& item:autoItems)if(item.file==a)unknown=item.tags.instrument==M::Instrument::unspecified;
            require(unknown,"Auto import guessed an unspecified personal file type");
            std::cout<<"PASS: unspecified personal IRs, explicit persistent classification, folder/file precedence, equal-basename isolation, non-destructive remove/re-add, factory protection and embedded metadata roundtrip\n";
        }
        const auto capped=root.getChildFile("large");require(capped.createDirectory().wasOk(),"Cannot create capped scan folder");
        for(int i=0;i<512;++i)require(bass.copyFileTo(capped.getChildFile("IR-"+juce::String(i)+".wav")),"Cannot create scan limit fixture");
        auto rows=C::scan({capped},false,&report);
        require(rows.size()==512 && report.examinedFiles==512 && !report.truncated,"Exactly 512 files incorrectly reported as truncated");
        require(bass.copyFileTo(capped.getChildFile("IR-extra.wav")),"Cannot create overflow fixture");
        rows=C::scan({capped},false,&report);
        require(rows.size()==512 && report.examinedFiles==512 && report.truncated,"Scan silently omitted files beyond its limit");
        std::cout<<"PASS: independent instrument/status/diameter filters, public factory-only catalog, explicit user imports, corrupt/silent file rejection, duplicate-root suppression, explicit 512 vs 513 scan limit\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
