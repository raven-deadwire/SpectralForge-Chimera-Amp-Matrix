#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SupportPanel.h"
#include "IRStateAssets.h"
#include <iostream>
#include <memory>
#include <limits>
#include <stdexcept>
#include <cstdlib>
#include <ctime>

// Private constructor injection: no environment variable or serialized trial
// flag can change policy in shipped plugin entry points.
struct ChimeraBetaTestAccess {
    static std::unique_ptr<ChimeraProcessor> create(const spectralforge::beta::Metadata& m,
                                                   spectralforge::beta::Clock c) {
        return std::unique_ptr<ChimeraProcessor>(new ChimeraProcessor(m,c));
    }
    static std::unique_ptr<ChimeraProcessor> create(const spectralforge::beta::Metadata& m,spectralforge::beta::Clock c,
                                                   const spectralforge::beta::tester::Grant& g) {
        return std::unique_ptr<ChimeraProcessor>(new ChimeraProcessor(m,c,g));
    }
};
struct ChimeraSupportPanelTestAccess {
    static void tick(ChimeraSupportPanel& panel) {
        if(!panel.isTimerRunning())throw std::runtime_error("SETTINGS timer is not running");
        panel.timerCallback();
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
        // Both sides must have identical histories: compare recovery against a
        // fresh uninterrupted processor, not the prior reprepare/offline case.
        auto recoveryReference=ChimeraBetaTestAccess::create(noRestriction,clockNow);
        sound(*restricted,mode,dual);sound(*recoveryReference,mode,dual);
        prepare(*restricted,channels,48000,128);prepare(*recoveryReference,channels,48000,128);
        require(restricted->getLatencySamples()==recoveryReference->getLatencySamples(),"Muted instance changed reported latency");
        juce::MidiBuffer midi;juce::AudioBuffer<float> out(channels,128), comparison(channels,128);
        for(int i=0;i<12;++i) {input(out,i);comparison.makeCopyOf(out);restricted->processBlock(out,midi);recoveryReference->processBlock(comparison,midi);require(out.getMagnitude(0,out.getNumSamples())==0,"Expired new instance emits audio");}
        require(!restricted->betaProcessingPermitted(),"Expired new instance admitted");
        now=expiry-1;restricted->recheckBetaClock();require(restricted->betaProcessingPermitted(),"Corrected clock cannot recover");
        double energy=0;
        for(int i=0;i<32;++i) {
            input(out,i);comparison.makeCopyOf(out);restricted->processBlock(out,midi);recoveryReference->processBlock(comparison,midi);
            for(int c=0;c<channels;++c)for(int n=0;n<128;++n) {
                const float expectedGain=juce::jmin(1.f,float(i*128+n+1)/2400.f);
                require(std::isfinite(out.getSample(c,n)) && std::abs(out.getSample(c,n))<1,"Clock recovery is unbounded");
                // Each float addition can round; bound accumulated gain error
                // by N * epsilon. A 2400-step ramp differs from the analytical
                // line by up to 3.48e-5 in a direct IEEE-float reproduction.
                // After sample 2400 the smoother snaps exactly to unity.
                const int elapsed=i*128+n+1;
                const float numericalBound=elapsed<2400
                    ? std::numeric_limits<float>::epsilon()*float(elapsed)*std::abs(comparison.getSample(c,n))+1e-7f
                    : 1e-7f;
                const float error=std::abs(out.getSample(c,n)-comparison.getSample(c,n)*expectedGain);
                if(error>numericalBound) {
                    std::cerr<<"Recovery mismatch route="<<route<<" channels="<<channels<<" sample="<<elapsed
                        <<" processed="<<comparison.getSample(c,n)<<" actual="<<out.getSample(c,n)
                        <<" expectedGain="<<expectedGain<<" error="<<error<<" bound="<<numericalBound<<'\n';
                    require(false,"Clock recovery did not use the 50 ms processed-signal fade");
                }
                energy+=out.getSample(c,n)*out.getSample(c,n);
            }
        }
        require(energy>1e-8,"Clock recovery remained silent");
        restricted->releaseResources();recoveryReference->releaseResources();reference->releaseResources();
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
            require(rows && rows->size()==4967 && p->getParameters().size()==4967+19+spectralforge::commonDynamicsParameterCount+spectralforge::multibandParameterCount+spectralforge::deEsserParameterCount+spectralforge::extraGateParameterCount+spectralforge::multibandStereoParameterCount+spectralforge::saturationParameterCount+spectralforge::referenceAmpParameterCount()+spectralforge::filterModulationParameterCount+spectralforge::postExpansionParameterCount()+spectralforge::effectExtensionParameterCount,"Historical or appended parameter inventory mismatch");
            for(const auto& row:*rows) {
                auto* parameter=p->parameters().getParameter(row["id"].toString());
                require(parameter && parameter->getParameterIndex()==int(row["ordinal"])
                        && parameter->getVersionHint()==int(row["version_hint"]),"Historical automation ID/order changed");
                require(std::abs(double(parameter->getValue())-double(row["value"]))<2e-6,"Historical A/B value changed");
            }
            for(int i=4967;i<p->getParameters().size();++i){const auto* parameter=p->getParameters()[i];require(std::abs(parameter->getValue()-parameter->getDefaultValue())<2e-6,"Historical state inherited new tuner/dynamics values");}
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
// Change only the process timezone, never the OS clock. POSIX TZ strings also
// exercise the Windows CRT. Restore the user's environment on every exit path.
struct ScopedTimezone {
    bool hadValue = std::getenv("TZ") != nullptr;
    std::string previous = hadValue ? std::getenv("TZ") : "";
    static void set(const char* zone) {
#ifdef _WIN32
        _putenv_s("TZ", zone ? zone : "");_tzset();
#else
        if(zone)setenv("TZ",zone,1);else unsetenv("TZ");tzset();
#endif
    }
    ~ScopedTimezone(){set(hadValue ? previous.c_str() : nullptr);}
};
void settingsCountdown(const juce::File& directory) {
    const auto metadata=spectralforge::beta::compiledMetadata();
    require(metadata.betaBuild,"SETTINGS identity test requires approved or qualification metadata");
    now=metadata.expiresAtUnix-90061;
    auto active=ChimeraBetaTestAccess::create(metadata,clockNow);
    ChimeraSupportPanel panel({}, {}, [&]{return active->betaNotice();}, [&]{active->recheckBetaClock();});
    auto* label=dynamic_cast<juce::TextEditor*>(panel.findChildWithID("betaLifecycleNotice"));
    require(label!=nullptr,"SETTINGS countdown field is absent");
    const auto first=label->getText();
    require(first.contains("Remaining: 1 day 01 hours 01 minutes 01 seconds."),"SETTINGS does not show exact remaining time");
    require(first.contains(CHIMERA_BETA_EXPIRY_UTC) && first.contains(CHIMERA_BETA_EXPIRY_KST)
            && first.contains(CHIMERA_PREVIEW_VERSION) && first.contains(CHIMERA_SOURCE_SHA)
            && first.contains(CHIMERA_BUILD_REVISION),"SETTINGS differs from compiled identity");
    require(directory.getChildFile("settings-compiled-notice.txt").replaceWithText(first),"Cannot write SETTINGS identity evidence");
    {
        ScopedTimezone restore;
        for(const auto* zone:{"UTC0","KST-9","PST8PDT,M3.2.0/2,M11.1.0/2","NPT-5:45"}) {
            ScopedTimezone::set(zone);ChimeraSupportPanelTestAccess::tick(panel);
            require(label->getText()==first,"System timezone changed UTC expiry or countdown");
        }
    }
    // Repeated ticks sample the injected UTC clock, including forward jumps and
    // correction backwards. A timer must not maintain its own decrementing clock.
    const struct {std::int64_t remaining;const char* expected;} rows[]={
        {86400,"Remaining: 1 day 00 hours 00 minutes 00 seconds."},
        {86399,"Remaining: 0 days 23 hours 59 minutes 59 seconds."},
        {1,"Remaining: 0 days 00 hours 00 minutes 01 seconds."},
        {0,"Remaining: 0 days 00 hours 00 minutes 00 seconds (expired)."},
        {-86400,"Remaining: 0 days 00 hours 00 minutes 00 seconds (expired)."},
        {90061,"Remaining: 1 day 01 hours 01 minutes 01 seconds."}};
    for(const auto& row:rows) {
        now=metadata.expiresAtUnix-row.remaining;ChimeraSupportPanelTestAccess::tick(panel);
        require(label->getText().contains(row.expected),"SETTINGS stale across clock/expiry boundary");
        require(active->betaProcessingPermitted(),"SETTINGS refresh revoked active instance grace");
        if(row.remaining<=0)require(label->getText().contains("keeps its sound"),"Grace notice contradicts active audio");
    }
    now=metadata.expiresAtUnix;
    auto muted=ChimeraBetaTestAccess::create(metadata,clockNow);
    now=0;
    require(muted->betaNotice().contains("Remaining: unavailable") && muted->betaNotice().contains("remains muted"),
            "Invalid clock notice contradicts muted instance");
    now=metadata.expiresAtUnix-60;
    require(muted->betaNotice().contains("still muted") && !muted->betaProcessingPermitted(),
            "Clock correction implicitly recovered audio or displayed a false permission");
    muted->recheckBetaClock();
    require(muted->betaProcessingPermitted() && !muted->betaNotice().contains("still muted"),"Explicit recovery left stale UI");
    now=metadata.expiresAtUnix-90061;ChimeraSupportPanelTestAccess::tick(panel);
    auto screenshot=directory.getChildFile("Beta-Countdown-Settings.png").createOutputStream();
    require(screenshot && juce::PNGImageFormat().writeImageToStream(panel.createComponentSnapshot(panel.getLocalBounds()),*screenshot),"Cannot save countdown SETTINGS evidence");
    std::cout<<"PASS: compiled SETTINGS countdown, UTC/KST, timezone changes, live clock jumps, exact expiry and recovery status\n";
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
    require(tree.getChildWithName("MIDI_MAP").getChildWithProperty("cc",40).getProperty("id").toString()=="output",
        "Expired save lost the exact MIDI controller-to-parameter identity");
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
    prepare(*reopened,2,48000,128);
    auto* mappedParameter=reopened->parameters().getParameter("output");
    for(const int value:{0,64,127}) {
        midi.clear();midi.addEvent(juce::MidiMessage::controllerEvent(1,40,value),0);reopened->processBlock(samples,midi);
        // JUCE snaps this 0.01 dB range using float arithmetic. ARM's fused
        // multiply-add yields 11.999999046 at CC127, while x86 yields 12.
        // Require the exact canonical parameter value, as NativeStateTests do;
        // a literal decimal comparison incorrectly reports a missing MIDI map.
        const float expected=mappedParameter->convertFrom0to1(float(value)/127.f);
        const float actual=reopened->parameters().getRawParameterValue("output")->load();
        std::cout<<"Restored CC40="<<value<<" output="<<juce::String(actual,9)
                 <<" canonical="<<juce::String(expected,9)<<'\n';
        require(actual==expected,"Expired instance lost MIDI automation map");
    }
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
    // This is a synthetic successor, not a next-release expiry approval.
    // Read state saved by the muted instance into a fresh permitted processor.
    now=expiry+1;
    constexpr spectralforge::beta::Metadata successor{"99.0.0-beta.1",sha,expiry+86400,1209600,true};
    auto updated=ChimeraBetaTestAccess::create(successor,clockNow);
    updated->setStateInformation(after.getData(),int(after.getSize()));
    compareParameters(*expired,*updated);
    for(int slot:{1,0}){expired->selectComparison(slot);updated->selectComparison(slot);compareParameters(*expired,*updated);}
    require(updated->betaProcessingPermitted(),"Expired permission leaked into successor state");
    require(updated->userIRName(0)==ir.getFileName() && updated->parameters().state.getProperty("irFolder")==directory.getFullPathName(),
            "Successor lost IR identity or personal data path");
    prepare(*updated,2,48000,128);
    midi.clear();midi.addEvent(juce::MidiMessage::controllerEvent(1,40,64),0);updated->processBlock(samples,midi);
    auto* updatedOutput=updated->parameters().getParameter("output");
    require(updated->parameters().getRawParameterValue("output")->load()==updatedOutput->convertFrom0to1(64.f/127.f),
            "Successor lost MIDI automation");
    juce::MemoryBlock updatedState;updated->getStateInformation(updatedState);
    const auto updatedXml=juce::AudioProcessor::getXmlFromBinary(updatedState.getData(),int(updatedState.getSize()));
    require(updatedXml!=nullptr,"Successor cannot save state");
    auto updatedTree=juce::ValueTree::fromXml(*updatedXml);require(spectralforge::irState::unpack(updatedTree),"Successor IR unpack failed");
    juce::MemoryBlock updatedIR;
    require(updatedIR.fromBase64Encoding(updatedTree.getChildWithName("USER_IRS").getChildWithProperty("lane",0).getProperty("data").toString()) && updatedIR==originalIR,
            "Successor changed embedded IR after source file removal");
    auto updatedReopen=ChimeraBetaTestAccess::create(successor,clockNow);
    updatedReopen->setStateInformation(updatedState.getData(),int(updatedState.getSize()));compareParameters(*updated,*updatedReopen);
    updated->releaseResources();updatedReopen->releaseResources();
    std::cout<<"PASS: expired save to synthetic successor, parameters/A-B/MIDI/personal+embedded IR and second save/reopen (not installed DAW acceptance)\n";
    source->releaseResources();expired->releaseResources();reopened->releaseResources();
    require(source->backgroundResourcesReleased()&&expired->backgroundResourcesReleased()&&reopened->backgroundResourcesReleased(),"Lifecycle introduced teardown resources");
    std::cout<<"PASS: expired binary/.chimera roundtrip, all host IDs/values, A/B, personal+embedded IR, paths, MIDI, editor/settings and teardown\n";
}
}
// A verified tester code lifts the expiry: a new instance opened long after
// expiry processes exactly like an unrestricted one and reports no expiry.
void testerExtension() {
    now=expiry+400*86400;
    spectralforge::beta::tester::Grant grant;grant.valid=true;grant.name="Fixture Tester";
    auto tester=ChimeraBetaTestAccess::create(beta,clockNow,grant);
    auto reference=ChimeraBetaTestAccess::create(noRestriction,clockNow);
    auto expired=ChimeraBetaTestAccess::create(beta,clockNow);
    for(auto* p:{tester.get(),reference.get(),expired.get()}){sound(*p,0,0);prepare(*p,2,48000,128);}
    require(tester->betaProcessingPermitted() && tester->betaStatus().status==spectralforge::beta::Status::testerExtended,
            "tester code did not lift the expiry for a new instance");
    require(tester->testerCodeHolder()=="Fixture Tester" && tester->betaNotice().contains("Beta tester code: Fixture Tester"),
            "tester code holder missing from the beta notice");
    require(!expired->betaProcessingPermitted() && expired->testerCodeHolder().isEmpty(),"control instance was not expired");
    juce::MidiBuffer midi;juce::AudioBuffer<float> out(2,128),comparison(2,128),muted(2,128);
    for(int i=0;i<8;++i) {
        input(out,i);comparison.makeCopyOf(out);muted.makeCopyOf(out);
        tester->processBlock(out,midi);reference->processBlock(comparison,midi);expired->processBlock(muted,midi);
        for(int c=0;c<2;++c)for(int n=0;n<128;++n)require(out.getSample(c,n)==comparison.getSample(c,n),"tester instance differs from unrestricted audio");
        require(muted.getMagnitude(0,128)==0,"expired control emitted audio");
    }
    now=expiry-1;
    std::cout<<"PASS: tester code lifts expiry for new instances with unrestricted audio and notice\n";
}

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        if(argc==3 && juce::String(argv[1])=="--historical") {historical(juce::File(argv[2]));return 0;}
        require(argc==2,"Expected evidence directory");const juce::File directory(argv[1]);require(directory.createDirectory().wasOk(),"Cannot create evidence directory");compiledIdentity(directory);settingsCountdown(directory);offlineUpdate();audioSafety();testerExtension();preservation(directory);return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
