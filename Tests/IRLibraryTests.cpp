#include "IRCollection.h"
#include "IRLibrary.h"
#include "CabLayoutModel.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
constexpr juce::dsp::ProcessSpec spec{48000,256,2};

void writeIR(const juce::File& file) {
    juce::WavAudioFormat format;
    auto stream=file.createOutputStream();require(stream!=nullptr,"Cannot create IR fixture");
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.release(),48000,1,24,{},0));
    require(writer!=nullptr,"Cannot encode IR fixture");
    juce::AudioBuffer<float> samples(1,1024);samples.clear();samples.setSample(0,0,.4f);
    for(int n=1;n<samples.getNumSamples();++n)
        samples.setSample(0,n,float(.08*std::exp(-n/170.)*std::sin(n*.19)));
    require(writer->writeFromAudioSampleBuffer(samples,0,samples.getNumSamples()),"Cannot write IR fixture");
}

struct Rig {
    std::array<spectralforge::Cab,3> cabinets;
    spectralforge::IRLibrary library{{&cabinets[0],&cabinets[1],&cabinets[2]}};
    Rig() {for(auto& cabinet:cabinets)cabinet.prepare(spec);}
    void prepare() {library.prepare(spec,{3,0,0});library.stop();}
};

std::vector<float> render(spectralforge::Cab& cabinet) {
    cabinet.reset();std::vector<float> output;
    juce::AudioBuffer<float> buffer(2,256);
    for(int offset=0;offset<4096;offset+=256) {
        buffer.clear();if(offset==0)buffer.setSample(0,0,.25f);
        cabinet.process(buffer);
        for(int n=0;n<256;++n) {
            require(std::isfinite(buffer.getSample(0,n)),"Cabinet emitted a non-finite sample");
            require(std::abs(buffer.getSample(1,n))<1.e-8f,"Left-only IR test leaked into right channel");
            output.push_back(buffer.getSample(0,n));
        }
    }
    return output;
}

void equalAudio(const std::vector<float>& before,const std::vector<float>& after) {
    require(before.size()==after.size(),"Restored audio length changed");
    double energy=0;float difference=0;
    for(size_t i=0;i<before.size();++i) {
        energy+=double(before[i])*before[i];
        difference=std::max(difference,std::abs(before[i]-after[i]));
    }
    require(energy>1.e-8,"IR lifecycle check rendered silence");
    require(difference<1.e-7f,"Library removal or project recall changed loaded IR audio");
}

void modelWorkerCancellation() {
    namespace layout=spectralforge::cabLayout;
    const juce::dsp::ProcessSpec settings{96000,64,1};
    std::array<spectralforge::Cab,3> cabinets;
    spectralforge::IRLibrary library{{&cabinets[0],&cabinets[1],&cabinets[2]}};
    const auto keyAt=[](double position) {
        return layout::key({{{true,1,1,1,2,.4,position,18.3},9,10,2},7,7});
    };
    const auto first=keyAt(.25);
    for(auto& cabinet:cabinets) {
        cabinet.blend=.5f;cabinet.requestedModel=first;cabinet.secondMic()->requestedModel=first;
        cabinet.prepare(settings);
    }
    library.prepare(settings,{0,0,0});
    for(const auto& cabinet:cabinets)
        require(cabinet.activeModel==first && cabinet.secondMic()->activeModel==first,
                "initial modeled prepare was cancelled by the previous stop flag");
    require(library.cancelledBuildCount()==0,"prepare did not reset cancellation diagnostics");
    juce::AudioBuffer<float> audio(1,64);
    const auto processAll=[&] {
        for(auto& cabinet:cabinets) {
            audio.clear();cabinet.process(audio);
            require(cabinet.transitioningMicCount()<=1,"worker cancellation overlaps mic fades");
            for(int sample=0;sample<audio.getNumSamples();++sample)
                require(std::isfinite(audio.getSample(0,sample)),"worker cancellation emits non-finite audio");
        }
    };
    // Keep changing a real request while the worker performs an expensive 6x10
    // generation. Observe actual worker abandonment, not just eventual success
    // that the old build-to-completion implementation could also provide.
    for(unsigned step=0;step<4000 && library.cancelledBuildCount()==0;++step) {
        const auto next=keyAt(.30+double(step%500)*.001);
        for(auto& cabinet:cabinets) {
            cabinet.requestedModel=next;cabinet.secondMic()->requestedModel=next;
        }
        processAll();juce::Thread::sleep(1);
    }
    require(library.cancelledBuildCount()>0,"automation never cancelled an obsolete worker build");
    const auto latest=keyAt(.913);
    for(auto& cabinet:cabinets) {
        cabinet.requestedModel=latest;cabinet.secondMic()->requestedModel=latest;
    }
    bool settled=false;
    for(int step=0;step<10000 && !settled;++step) {
        processAll();settled=true;
        for(const auto& cabinet:cabinets)
            settled=settled && cabinet.activeModel==latest && cabinet.secondMic()->activeModel==latest
                && cabinet.transitioningMicCount()==0;
        if(!settled)juce::Thread::sleep(1);
    }
    require(settled,"stable latest request never converges after worker cancellation");
    for(int slot=0;slot<6;++slot)
        require(!library.status(slot).containsIgnoreCase("failed"),"obsolete work contaminated IR error status");
    const auto abandoned=library.cancelledBuildCount();

    // Request stop after another model change, join, and recreate saved state.
    // Depending on scheduling the worker may be building or in its poll wait;
    // this is a lifecycle contract, not proof of an in-generation stop deadline.
    // Stop latency is evidence only; platform scheduling is not a unit-test gate.
    for(auto& cabinet:cabinets)cabinet.requestedModel=keyAt(.177);
    juce::Thread::sleep(1);
    const auto stopping=std::chrono::steady_clock::now();library.stop();
    const auto stopUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-stopping).count();
    for(auto& cabinet:cabinets)cabinet.clear();
    require(library.resourcesReleased(),"cancelled worker retains resources after stop/clear");
    for(auto& cabinet:cabinets) {
        cabinet.requestedModel=latest;cabinet.secondMic()->requestedModel=latest;cabinet.prepare(settings);
    }
    library.prepare(settings,{0,0,0});library.stop();
    require(library.cancelledBuildCount()==0,"synchronous reprepare was counted as cancelled work");
    for(const auto& cabinet:cabinets)
        require(cabinet.activeModel==latest && cabinet.secondMic()->activeModel==latest,
                "stop/reprepare failed to install the saved complete model");
    for(auto& cabinet:cabinets)cabinet.clear();
    require(library.resourcesReleased(),"reprepared worker resources survive clear");
    std::cout<<"PASS modeled_worker_cancellation cancelled_builds="<<abandoned
        <<" latest_six_mics=converged stop_us="<<stopUs<<" reprepare=synchronous resources=released\n";
}
}

int main() {
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Chimera-IR-lifecycle",{},false);
    struct Cleanup {juce::File folder;~Cleanup(){folder.deleteRecursively();}} cleanup{folder};
    try {
        using C=spectralforge::IRCollection;using M=spectralforge::IRMetadata;
        require(folder.createDirectory().wasOk(),"Cannot create isolated IR lifecycle directory");
        const auto file=folder.getChildFile("capture.wav"),preferences=folder.getChildFile("library.json");
        writeIR(file);juce::MemoryBlock original;require(file.loadFileAsData(original),"Cannot read original fixture");
        require(C::rememberFile(file,M::Instrument::bass,preferences).wasOk(),"Cannot register isolated IR");
        auto entries=C::scan({folder},false,nullptr,preferences);
        require(entries.size()==1 && entries[0].bass(),"Registered bass classification missing");
        Rig active;require(active.library.importFile(0,file).wasOk(),"Cannot load IR into active rig");
        // Apply exactly the isolated library's metadata, without writing any
        // application preferences or depending on the user's existing folder rules.
        active.library.setMetadata(0,entries[0].tags);active.prepare();
        const auto audio=render(active.cabinets[0]);const auto saved=active.library.save();
        require(saved.getNumChildren()==1,"Loaded IR state has wrong lane count");
        juce::MemoryBlock embedded;
        require(embedded.fromBase64Encoding(saved.getChild(0)["data"].toString()) && embedded==original,"Project changed original WAV bytes");
        require(C::remove(entries[0],preferences).wasOk(),"Cannot remove imported IR from isolated list");
        require(C::scan({folder},false,nullptr,preferences).empty(),"Removed IR reappeared after library refresh");
        juce::MemoryBlock retained;require(file.loadFileAsData(retained) && retained==original,"List removal changed the source WAV");
        require(active.library.userName(0)==file.getFileName() && active.library.metadata(0,3).instrument==M::Instrument::bass,"List removal changed active project identity or type");
        require(active.library.save().isEquivalentTo(saved),"List removal changed embedded project state");
        equalAudio(audio,render(active.cabinets[0]));

        juce::MemoryOutputStream serialized;saved.writeToStream(serialized);
        const auto recalled=juce::ValueTree::readFromData(serialized.getData(),serialized.getDataSize());
        require(recalled.isValid(),"Cannot decode saved IR state");
        // Only this generated fixture is removed: saved project audio must be
        // independent of both library visibility and the external source path.
        require(file.deleteFile(),"Cannot remove generated source before project recall");
        Rig restored;restored.library.restore(recalled);restored.prepare();
        require(restored.library.metadata(0,3).instrument==M::Instrument::bass,"Instrument type lost across binary project save/restore");
        require(restored.library.save().isEquivalentTo(saved),"Restored project changed original embedded bytes or metadata");
        equalAudio(audio,render(restored.cabinets[0]));
        require(restored.cabinets[1].activeSource.load()==0 && restored.cabinets[2].activeSource.load()==0,"IR restore changed unrelated lanes");
        modelWorkerCancellation();
        std::cout<<"PASS: remove-from-list preserves source WAV, active convolution, embedded project bytes and bass classification; binary recall renders identical audio without the source file\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
