#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SupportPanel.h"
#include "IRStateAssets.h"
#include <iostream>
#include <memory>
#include <stdexcept>

// Private constructor injection: no environment variable or serialized trial
// flag can change policy in shipped plugin entry points.
struct ChimeraBetaTestAccess {
    static std::unique_ptr<ChimeraProcessor> create(const spectralforge::beta::Metadata& m,
                                                   spectralforge::beta::Clock c) {
        return std::unique_ptr<ChimeraProcessor>(new ChimeraProcessor(m,c));
    }
};
namespace spectralforge::release {
struct ReleaseSupportTestAccess {
    static std::shared_ptr<ReleaseSupport> offline() {
        return std::shared_ptr<ReleaseSupport>(new ReleaseSupport(
            [](const juce::String&,int,juce::String&){return juce::Result::fail("Cannot reach GitHub. Check your connection and try again.");}));
    }
};
}
namespace {
constexpr auto sha="0123456789abcdef0123456789abcdef01234567";
constexpr std::int64_t expiry=1800000000;
constexpr spectralforge::beta::Metadata beta{"1.3.1-beta.3",sha,expiry,1209600,true};
constexpr spectralforge::beta::Metadata noRestriction{"1.3.1",sha,0,0,false};
std::int64_t now=expiry-1;
std::int64_t clockNow() noexcept {return now;}
void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
void set(ChimeraProcessor& p,const juce::String& id,float v) {
    auto* parameter=p.parameters().getParameter(id);require(parameter!=nullptr,"Missing test parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(v));
}
void prepare(ChimeraProcessor& p,int channels,double rate,int block) {
    auto layout=p.getBusesLayout();
    layout.inputBuses.set(0,channels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
    layout.outputBuses=layout.inputBuses;
    require(p.setBusesLayout(layout),"Unsupported lifecycle bus layout");
    p.setRateAndBufferSizeDetails(rate,block);p.prepareToPlay(rate,block);
}
void sound(ChimeraProcessor& p,int mode,int dual) {
    set(p,"mode",float(mode));set(p,"dualtype",float(dual));set(p,"gateon",0);
    set(p,"boardEnabled",0);set(p,"preon",0);set(p,"delayon",0);set(p,"reverbon",0);
    for(int lane=1;lane<=3;++lane) {set(p,"cab"+juce::String(lane),0);set(p,"ampon"+juce::String(lane),0);}
}
void input(juce::AudioBuffer<float>& b,int block) {
    for(int c=0;c<b.getNumChannels();++c)for(int n=0;n<b.getNumSamples();++n)
        b.setSample(c,n,.03f*std::sin(float(block*b.getNumSamples()+n)*(.031f+c*.009f)));
}
struct Transport final : juce::AudioPlayHead {
    bool playing{}, recording{};
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p;p.setIsPlaying(playing);p.setIsRecording(recording);p.setBpm(120.0);return p;
    }
};
void audioSafety() {
    for(int channels:{1,2})for(int route=0;route<4;++route) {
        const int mode=route==0?0:route==3?2:1,dual=route==2?1:0;
        now=expiry-1;Transport transport;
        auto active=ChimeraBetaTestAccess::create(beta,clockNow);
        auto reference=ChimeraBetaTestAccess::create(noRestriction,clockNow);
        active->setPlayHead(&transport);reference->setPlayHead(&transport);
        sound(*active,mode,dual);sound(*reference,mode,dual);
        for(int cycle=0;cycle<3;++cycle) {
            const double rate=cycle==0?44100:cycle==1?48000:96000;
            const int block=cycle==0?64:cycle==1?257:128;
            prepare(*active,channels,rate,block);prepare(*reference,channels,rate,block);
            require(active->getLatencySamples()==reference->getLatencySamples(),"Expiry changed host latency");
            active->setNonRealtime(cycle==2);reference->setNonRealtime(cycle==2);
            juce::MidiBuffer midi;juce::AudioBuffer<float> a(channels,block+17),b(channels,block+17);
            for(int i=0;i<24;++i) {
                transport.playing=i%3!=0;transport.recording=i%3==2;
                if(i==8)now=expiry;
                if(i==16)now=expiry+1;
                input(a,i);b.makeCopyOf(a);active->processBlock(a,midi);reference->processBlock(b,midi);
                for(int c=0;c<channels;++c)for(int n=0;n<a.getNumSamples();++n) {
                    require(std::isfinite(a.getSample(c,n)),"Lifecycle output is non-finite");
                    require(a.getSample(c,n)==b.getSample(c,n),"Admitted audio differs before/at/after expiry");
                }
                require(active->betaProcessingPermitted(),"Active-instance grace was reconstructed");
            }
            // With no editor open, prepare/release, transport and offline render
            // cannot turn an admitted instance into a newly expired one.
            active->releaseResources();reference->releaseResources();
        }
        if(route==0 && channels==2) {
            {auto editor=std::unique_ptr<juce::AudioProcessorEditor>(active->createEditor());require(editor!=nullptr,"Active expired editor cannot open");}
            require(active->betaProcessingPermitted(),"Active grace lost on editor close");
        }
        auto restricted=ChimeraBetaTestAccess::create(beta,clockNow);
        sound(*restricted,mode,dual);prepare(*restricted,channels,48000,128);
        prepare(*reference,channels,48000,128);
        require(restricted->getLatencySamples()==reference->getLatencySamples(),"Muted instance changed reported latency");
        juce::MidiBuffer midi;juce::AudioBuffer<float> out(channels,128), comparison(channels,128);
        for(int i=0;i<12;++i) {input(out,i);comparison.makeCopyOf(out);restricted->processBlock(out,midi);reference->processBlock(comparison,midi);require(out.getMagnitude(0,out.getNumSamples())==0,"Expired new instance emits audio");}
        require(!restricted->betaProcessingPermitted(),"Expired new instance admitted");
        now=expiry-1;restricted->recheckBetaClock();require(restricted->betaProcessingPermitted(),"Corrected clock cannot recover");
        double energy=0;
        for(int i=0;i<32;++i) {
            input(out,i);comparison.makeCopyOf(out);restricted->processBlock(out,midi);reference->processBlock(comparison,midi);
            for(int c=0;c<channels;++c)for(int n=0;n<128;++n) {
                const float expectedGain=juce::jmin(1.f,float(i*128+n+1)/2400.f);
                require(std::isfinite(out.getSample(c,n)) && std::abs(out.getSample(c,n))<1,"Clock recovery is unbounded");
                require(std::abs(out.getSample(c,n)-comparison.getSample(c,n)*expectedGain)<5e-7f,"Clock recovery did not use the 50 ms processed-signal fade");
                energy+=out.getSample(c,n)*out.getSample(c,n);
            }
        }
        require(energy>1e-8,"Clock recovery remained silent");
        restricted->releaseResources();reference->releaseResources();
    }
    std::cout<<"PASS: eight mono/stereo routing cases; sample-rate/buffer/reprepare/offline/grace/zero-output/clock recovery\n";
}
void compareParameters(ChimeraProcessor& a,ChimeraProcessor& b) {
    require(a.getParameters().size()==b.getParameters().size(),"Host parameter count changed");
    for(int i=0;i<a.getParameters().size();++i) {
        const auto* p=dynamic_cast<juce::AudioProcessorParameterWithID*>(a.getParameters()[i]);
        const auto* q=dynamic_cast<juce::AudioProcessorParameterWithID*>(b.getParameters()[i]);
        require(p && q && p->paramID==q->paramID && p->getVersionHint()==q->getVersionHint(),"Host parameter identity changed");
        require(std::abs(p->getValue()-q->getValue())<2e-6f,"Expired state restore changed a host value");
    }
}
void historical(const juce::File& directory) {
    const auto file=directory.getChildFile("beta2-released-source.chimera");
    juce::MemoryBlock bytes;require(file.loadFileAsData(bytes),"Historical beta.2 fixture is missing");
    const auto manifest=juce::JSON::parse(directory.getChildFile("beta2-fixture.json"));
    require(manifest["source_sha"].toString()=="6033236cb81820a43679cbbd34be020804f9658d"
            && manifest["state_sha256"].toString()==juce::SHA256(bytes).toHexString(),"Historical fixture provenance mismatch");
    for(const auto instant:{expiry-1,expiry,expiry+1}) {
        now=instant;auto p=ChimeraBetaTestAccess::create(beta,clockNow);p->setStateInformation(bytes.getData(),int(bytes.getSize()));
        for(int slot:{0,1,0}) {
            p->selectComparison(slot);const auto* rows=manifest[slot==0?"A":"B"].getArray();
            require(rows && rows->size()==p->getParameters().size(),"Historical parameter inventory mismatch");
            for(const auto& row:*rows) {
                auto* parameter=p->parameters().getParameter(row["id"].toString());
                require(parameter && parameter->getParameterIndex()==int(row["ordinal"])
                        && parameter->getVersionHint()==int(row["version_hint"]),"Historical automation ID/order changed");
                require(std::abs(double(parameter->getValue())-double(row["value"]))<2e-6,"Historical A/B value changed");
            }
        }
        juce::MemoryBlock saved;p->getStateInformation(saved);
        const auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));require(xml!=nullptr,"Historical state resave failed");
        auto tree=juce::ValueTree::fromXml(*xml);require(spectralforge::irState::unpack(tree),"Historical IR unpack failed");
        juce::MemoryBlock ir;require(ir.fromBase64Encoding(tree.getChildWithName("USER_IRS").getChildWithProperty("lane",0).getProperty("data").toString())
            && juce::SHA256(ir).toHexString()==manifest["ir_sha256"].toString(),"Historical embedded IR bytes changed");
        auto reopened=ChimeraBetaTestAccess::create(beta,clockNow);reopened->setStateInformation(saved.getData(),int(saved.getSize()));compareParameters(*p,*reopened);
        require(p->betaProcessingPermitted()==(instant<expiry),"Historical state altered instance policy");
    }
    std::cout<<"PASS: released beta.2 source fixture, all host values/ordinals/A-B/IR before-at-after expiry and resave/reopen (not DAW acceptance)\n";
}
void offlineUpdate() {
    using Service=spectralforge::release::ReleaseSupport;
    now=expiry-1;auto active=ChimeraBetaTestAccess::create(beta,clockNow);
    auto service=spectralforge::release::ReleaseSupportTestAccess::offline();
    require(service->checkNow(),"Offline update could not be queued");
    const auto started=juce::Time::getMillisecondCounter();
    while(service->snapshot().state!=Service::State::error && juce::Time::getMillisecondCounter()-started<2000)juce::Thread::sleep(5);
    require(service->snapshot().state==Service::State::error,"Injected GitHub failure did not reach the real worker error state");
    require(active->betaProcessingPermitted(),"Offline update failure invalidated a valid beta");
    ChimeraSupportPanel panel(service,{},[&]{return active->betaNotice();},[&]{active->recheckBetaClock();});
    auto* status=dynamic_cast<juce::TextEditor*>(panel.findChildWithID("supportStatus"));
    require(status && status->getText().contains("Cannot reach GitHub"),"Offline update error missing from SETTINGS");
    require(!panel.findChildWithID("supportDownload")->isEnabled(),"Offline update offered an unverified download");
    now=expiry;require(active->betaProcessingPermitted(),"Offline error revoked active instance grace");
    std::cout<<"PASS: real update worker injected offline failure, useful SETTINGS error, no account/network dependency of audio policy\n";
}
void compiledIdentity(const juce::File& directory) {
    const auto build=directory.getParentDirectory();
    const auto metadata=juce::JSON::parse(build.getChildFile("chimera-beta-lifecycle.json"));
    const auto version=juce::JSON::parse(build.getChildFile("chimera-build-version.json"));
    require(metadata["version"].toString()==CHIMERA_PREVIEW_VERSION && metadata["source_sha"].toString()==CHIMERA_SOURCE_SHA
        && metadata["mode"].toString()==CHIMERA_BETA_MODE && metadata["expires_at_utc"].toString()==CHIMERA_BETA_EXPIRY_UTC
        && juce::int64(metadata["expires_at_unix"])==CHIMERA_BETA_EXPIRY_UNIX,"Compiled beta identity differs from package JSON");
    juce::MemoryBlock bytes;require(build.getChildFile("chimera-beta-lifecycle.json").loadFileAsData(bytes),"Missing compiled metadata");
    require(version["beta_lifecycle_sha256"].toString()==juce::SHA256(bytes).toHexString(),"Configured beta fingerprint mismatch");
    require(directory.getChildFile("compiled-beta-identity.json").replaceWithText(juce::JSON::toString(metadata,true)),"Cannot write compiled identity evidence");
}
void preservation(const juce::File& directory) {
    now=expiry-10;auto source=ChimeraBetaTestAccess::create(beta,clockNow);
    const auto ir=directory.getChildFile("personal-lifecycle.wav");
    juce::AudioBuffer<float> impulse(1,128);impulse.clear();impulse.setSample(0,0,.8f);impulse.setSample(0,35,.1f);
    {
        juce::WavAudioFormat format;auto stream=ir.createOutputStream();require(stream!=nullptr,"Cannot create IR fixture");
        auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.get(),48000,1,24,{},0));
        require(writer!=nullptr,"Cannot create IR writer");stream.release();require(writer->writeFromAudioSampleBuffer(impulse,0,128),"Cannot encode IR");
    }
    juce::MemoryBlock originalIR;require(ir.loadFileAsData(originalIR),"Cannot read IR");
    require(source->loadIR(0,ir).wasOk(),"Cannot load personal IR");
    source->parameters().state.setProperty("irFolder",directory.getFullPathName(),nullptr);
    set(*source,"mode",2);set(*source,"drive1",.21f);source->copyComparison();source->selectComparison(1);set(*source,"drive1",.79f);source->selectComparison(0);
    source->learnMidi("output");prepare(*source,2,48000,128);
    juce::AudioBuffer<float> samples(2,128);samples.clear();juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::controllerEvent(1,40,64),0);source->processBlock(samples,midi);midi.clear();
    juce::MemoryBlock before;source->getStateInformation(before);
    const auto preset=directory.getChildFile("personal-lifecycle.chimera");require(preset.replaceWithData(before.getData(),before.getSize()),"Cannot export preset");
    now=expiry;auto expired=ChimeraBetaTestAccess::create(beta,clockNow);
    expired->setStateInformation(before.getData(),int(before.getSize()));compareParameters(*source,*expired);
    require(!expired->betaProcessingPermitted(),"Project state bypassed expired decision");
    source->selectComparison(1);expired->selectComparison(1);compareParameters(*source,*expired);
    source->selectComparison(0);expired->selectComparison(0);compareParameters(*source,*expired);
    require(expired->parameters().state.getProperty("irFolder")==directory.getFullPathName(),"Personal data path changed");
    require(expired->userIRName(0)==ir.getFileName(),"Personal IR identity changed");
    juce::MemoryBlock after;expired->getStateInformation(after);
    auto xml=juce::AudioProcessor::getXmlFromBinary(after.getData(),int(after.getSize()));require(xml!=nullptr,"Expired save is invalid");
    auto tree=juce::ValueTree::fromXml(*xml);require(spectralforge::irState::unpack(tree),"Expired IR assets invalid");
    juce::MemoryBlock embedded;require(embedded.fromBase64Encoding(tree.getChildWithName("USER_IRS").getChildWithProperty("lane",0).getProperty("data").toString()) && embedded==originalIR,"Expired save changed embedded IR bytes");
    // JUCE loadFileAsData appends to its destination. Independent buffers
    // are required to compare the two unchanged files, not their concatenation.
    juce::MemoryBlock diskIR,diskPreset;
    require(ir.loadFileAsData(diskIR)&&diskIR==originalIR,"Expiry rewrote personal IR file");
    require(preset.loadFileAsData(diskPreset)&&diskPreset==before,"Expiry rewrote personal preset file");
    // The user source can be unavailable; the embedded data remains sufficient.
    require(ir.deleteFile(),"Cannot remove fixture IR");
    auto reopened=ChimeraBetaTestAccess::create(beta,clockNow);reopened->setStateInformation(after.getData(),int(after.getSize()));compareParameters(*expired,*reopened);
    require(reopened->userIRName(0)==ir.getFileName(),"Reopen requires the original IR file");
    prepare(*reopened,2,48000,128);midi.addEvent(juce::MidiMessage::controllerEvent(1,40,127),0);reopened->processBlock(samples,midi);
    require(reopened->parameters().getRawParameterValue("output")->load()==12,"Expired instance lost MIDI automation map");
    require(samples.getMagnitude(0,128)==0,"Automation unmuted expired instance");
    // Exercise the actual editor lifecycle without opening an HTTP service.
    {auto editor=std::unique_ptr<juce::AudioProcessorEditor>(reopened->createEditor());require(editor!=nullptr,"Expired editor cannot open");}
    require(!reopened->betaProcessingPermitted(),"Closing editor changed expiry");
    {auto editor=std::unique_ptr<juce::AudioProcessorEditor>(reopened->createEditor());require(editor!=nullptr,"Expired editor cannot reopen");}
    ChimeraSupportPanel panel({}, {}, [&]{return reopened->betaNotice();}, [&]{reopened->recheckBetaClock();});
    auto* notice=dynamic_cast<juce::TextEditor*>(panel.findChildWithID("betaLifecycleNotice"));
    require(notice && notice->getText().contains("muted output"),"Expired SETTINGS explanation missing");
    require(panel.findChildWithID("supportUpdates")->isEnabled(),"Expired update action disabled");
    require(panel.findChildWithID("betaRecheckClock")->isEnabled(),"Clock recovery action disabled");
    const auto screenshot=directory.getChildFile("Beta-Expired-Settings.png").createOutputStream();
    require(screenshot && juce::PNGImageFormat().writeImageToStream(panel.createComponentSnapshot(panel.getLocalBounds()),*screenshot),"Cannot save SETTINGS evidence");
    source->releaseResources();expired->releaseResources();reopened->releaseResources();
    require(source->backgroundResourcesReleased()&&expired->backgroundResourcesReleased()&&reopened->backgroundResourcesReleased(),"Lifecycle introduced teardown resources");
    std::cout<<"PASS: expired binary/.chimera roundtrip, all host IDs/values, A/B, personal+embedded IR, paths, MIDI, editor/settings and teardown\n";
}
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        if(argc==3 && juce::String(argv[1])=="--historical") {historical(juce::File(argv[2]));return 0;}
        require(argc==2,"Expected evidence directory");const juce::File directory(argv[1]);require(directory.createDirectory().wasOk(),"Cannot create evidence directory");compiledIdentity(directory);offlineUpdate();audioSafety();preservation(directory);return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
