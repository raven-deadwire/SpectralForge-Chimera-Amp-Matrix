#include "PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

struct ChimeraRecallTestAccess {
    static void stopWorker(ChimeraProcessor& p) {p.library.stop();}
    static void stallStateApplication(ChimeraProcessor& p) {
        p.switchFade=ChimeraProcessor::SwitchFade::hold;p.switchFadePosition=0;
        p.switchHold=int(p.rate*.5);p.holdForSwitch=true;p.fadedSilent=true;
    }
    static spectralforge::Cab& cab(ChimeraProcessor& p,int lane) {return p.engine.cabinet(lane);}
    static bool waiting(ChimeraProcessor& p) {return p.switchFade==ChimeraProcessor::SwitchFade::wait;}
};
namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
std::unique_ptr<spectralforge::Cab::Kernel> kernel(const juce::dsp::ProcessSpec& spec,int source,unsigned generation,uint64_t model=0) {
    juce::AudioBuffer<float> ir(1,256);ir.clear();ir.setSample(0,0,.5f);
    return std::make_unique<spectralforge::Cab::Kernel>(std::move(ir),spec.sampleRate,spec,source,generation,model);
}
void identity(int block) {
    const juce::dsp::ProcessSpec spec{48000,juce::uint32(block),2};
    spectralforge::Cab cab(true);cab.prepare(spec);cab.requestedSource=3;cab.requestedGeneration=1;
    cab.install(kernel(spec,3,1));require(cab.settled(),"initial user IR not ready");
    cab.requestedGeneration=2;require(!cab.settled(),"old user IR accepted after same-slot replacement");
    cab.publish(kernel(spec,3,1));juce::AudioBuffer<float> audio(2,block);audio.clear();cab.process(audio);
    require(cab.activeGeneration==1 && cab.rejectedModels==1 && !cab.settled(),"stale generation published");cab.collect();
    cab.publish(kernel(spec,2,2));audio.clear();cab.process(audio);
    require(cab.activeSource==3 && cab.rejectedModels==2 && !cab.settled(),"superseded captured source published");cab.collect();
    cab.publish(kernel(spec,3,2));
    for(int i=0;i<4800/block+2;++i){audio.clear();cab.process(audio);}
    require(cab.settled() && cab.activeGeneration==2,"latest user IR did not recover");
    auto missing=kernel(spec,3,2);missing->hasIR=false;cab.install(std::move(missing));
    require(!cab.settled(),"missing user IR was accepted as a ready impulse response");
}
void routing(int block) {
    const juce::dsp::ProcessSpec spec{48000,juce::uint32(block),2};
    auto engine=std::make_unique<spectralforge::Engine>();engine->prepare(spec);
    std::array<spectralforge::LaneState,3> states;
    for(int lane=0;lane<3;++lane) {
        auto& cab=engine->cabinet(lane);cab.requestedSource=0;cab.install(kernel(spec,0,1));
    }
    juce::AudioBuffer<float> audio(2,block);
    const auto process=[&](spectralforge::RoutingMode mode,float mix=.5f) {
        for(int i=0;i<1920/block+2;++i){audio.clear();engine->process(audio,mode,150,1200,states,nullptr,false,mix);}
    };
    engine->cabinet(1).requestedSource=3;engine->cabinet(2).requestedSource=3;
    process(spectralforge::RoutingMode::classic);
    require(engine->audibleCabinetsSettled(),"inactive routing lanes blocked Classic recall");
    process(spectralforge::RoutingMode::dual,0);
    require(engine->audibleCabinetsSettled(),"zero-weight Dual lane blocked recall");
    process(spectralforge::RoutingMode::dual,.5f);
    require(!engine->audibleCabinetsSettled(),"audible Dual lane was ignored");
    engine->cabinet(2).requestedSource=0;states[2].solo=true;
    process(spectralforge::RoutingMode::matrix);
    require(engine->audibleCabinetsSettled() && engine->cabinet(2).audiblePriority
            && !engine->cabinet(0).audiblePriority && !engine->cabinet(1).audiblePriority,
            "solo lane did not exclusively receive audible priority");
    states[2].solo=false;states[1].mute=true;states[2].mute=true;
    engine->cabinet(0).requestedSource=3;states[0].lowAmpMix=0;
    process(spectralforge::RoutingMode::matrix);
    require(engine->audibleCabinetsSettled(),"Matrix LOW DI waited for inaudible CAB");
    states[0].lowAmpMix=.5f;process(spectralforge::RoutingMode::matrix);
    require(!engine->audibleCabinetsSettled(),"audible Matrix LOW CAB was ignored");
    for(int lane=0;lane<3;++lane)engine->cabinet(lane).clear();
}
void timeout(int block,bool firstCallback=false,bool stalledState=false) {
    auto p=std::make_unique<ChimeraProcessor>();p->loadFactoryPreset(0);p->prepareToPlay(48000,block);
    juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;int sample=0;
    const auto process=[&] {
        for(int n=0;n<block;++n,++sample)for(int c=0;c<2;++c)
            audio.setSample(c,n,.1f*std::sin(float(sample)*.031f));
        p->processBlock(audio,midi);
        for(int c=0;c<2;++c)for(int n=0;n<block;++n)require(std::isfinite(audio.getSample(c,n)),"non-finite recall output");
        return audio.getMagnitude(0,block);
    };
    if(!firstCallback)for(int i=0;i<4800/block+1;++i)process();
    ChimeraRecallTestAccess::stopWorker(*p);
    if(stalledState) {
        ChimeraRecallTestAccess::stallStateApplication(*p);
        for(int i=0;i<33600/block+1;++i)require(process()==0.f,"state-application timeout exposed old or partially recalled audio");
        require(p->recallStateWaitExceeded(),"stalled state transaction was not diagnosed");
    }
    p->loadFactoryPreset(33);
    // Deliberately leave every requested new cabinet unbuilt. Audio time, not
    // a slow sleep or machine-dependent generation duration, drives the limit.
    for(int i=0;i<33600/block+1;++i) {
        const auto peak=process();
        if(i*block>=26400)require(peak==0.f,"500 ms timeout exposed the previous/unprepared cabinet");
    }
    require(!p->recallStateWaitExceeded() && p->recallCabinetWaitExceeded() && ChimeraRecallTestAccess::waiting(*p),"timeout not diagnosed or wait abandoned");
    p->reset(); // OBS-style filter re-enable / host tail reset cannot unmute it.
    for(int i=0;i<4800/block+1;++i)require(process()==0.f,"host reset bypassed recall readiness");
    // Inject READY kernels late, using the exact requested identities. This is
    // fault-injection coverage of ownership/readiness, not acoustic modeling QA.
    const juce::dsp::ProcessSpec spec{48000,juce::uint32(block),2};
    for(int lane=0;lane<3;++lane) {
        auto& a=ChimeraRecallTestAccess::cab(*p,lane);
        for(auto* cab:{&a,a.secondMic()})cab->publish(kernel(spec,cab->requestedSource,cab->requestedGeneration,cab->requestedModel));
    }
    bool heard=false;
    for(int i=0;i<14400/block+1;++i)heard=process()>1.e-6f || heard;
    require(heard && !ChimeraRecallTestAccess::waiting(*p) && !p->recallCabinetWaitExceeded(),"late valid kernels did not safely resume output");
    p->releaseResources();require(p->backgroundResourcesReleased(),"cached kernels survived release");
}
}
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {
        for(int block:{64,128,256}){identity(block);routing(block);timeout(block);timeout(block,true);timeout(block,false,true);std::cout<<"PASS recall safety 48000/"<<block<<'\n';}
    } catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
