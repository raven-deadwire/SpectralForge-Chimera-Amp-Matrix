#pragma once
#include "FactoryPresets.h"
#include "ChimeraDSP.h"
#include "IRLibrary.h"
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace factoryPresetTests {
inline void require(bool condition,const std::string& message) {
    if(!condition) throw std::runtime_error(message);
}
using Snapshot=std::map<std::string,float>;
inline Snapshot snapshot(int index) {
    Snapshot values;
    require(spectralforge::applyFactoryPreset(index,[&](const char* id,float value){
        require(std::isfinite(value),std::string("Non-finite preset parameter: ")+id);
        values[id]=value;
    }),"Factory preset index rejected");
    return values;
}
inline std::array<spectralforge::LaneState,3> lanes(const Snapshot& values) {
    std::array<spectralforge::LaneState,3> states;
    for(int i=0;i<3;++i) {
        const auto get=[&](const char* id){return values.at(std::string(id)+std::to_string(i+1));};
        auto& lane=states[(size_t)i];
        lane.amp=(int)get("amp");lane.drive=get("drive");lane.levelDb=get("level");
        lane.bass=get("bass");lane.lowMid=get("lowmid");lane.highMid=get("highmid");lane.treble=get("treble");
        lane.presence=get("presence");lane.resonance=get("resonance");lane.bandTone=get("bandtone");
        lane.mute=get("mute")>.5f;lane.solo=get("solo")>.5f;lane.polarity=get("polarity")>.5f;
        lane.cab=get("cab")>.5f;lane.ampEnabled=get("ampon")>.5f;
        lane.cabLow=get("cablow");lane.cabHigh=get("cabhigh");
    }
    states[0].lowComp=values.at("lowcomp");states[0].lowAmpMix=values.at("lowampmix");
    return states;
}
inline spectralforge::FXState effects(const Snapshot& values) {
    std::array<std::atomic<float>,spectralforge::fxSpecs.size()> parameters;
    std::array<std::atomic<float>*,spectralforge::fxSpecs.size()> pointers;
    for(size_t i=0;i<parameters.size();++i) {
        const auto& spec=spectralforge::fxSpecs[i];const auto value=values.at(spec.id);
        require(value>=spec.minimum && value<=spec.maximum,std::string("Out-of-range FX preset value: ")+spec.id);
        parameters[i].store(value);pointers[i]=&parameters[i];
    }
    auto fx=spectralforge::readFX(pointers);fx.envelopeFirst=values.at("preorder")>.5f;fx.boostAfterDrive=values.at("gainorder")>.5f;
    for(size_t i=0;i<spectralforge::modelFamilies.size();++i) {
        const auto& family=spectralforge::modelFamilies[i];fx.models[i]=(int)values.at(family.parameter);
        require(fx.models[i]>=0 && fx.models[i]<family.count,"Factory preset references a missing FX model");
    }
    // Host tempo is deliberately preserved; this fixture supplies 120 BPM.
    if(fx.delaySync) fx.delayMs=500.f;
    return fx;
}
inline float render(int index,double rate,int blockSize) {
    using namespace spectralforge;
    const auto values=snapshot(index);const auto states=lanes(values);const auto fx=effects(values);
    const juce::dsp::ProcessSpec spec{rate,(juce::uint32)blockSize,2};
    Engine engine;PreFXChain pre;PostFXChain post;
    engine.prepare(spec);engine.setOversampling((int)values.at("oversampling"));pre.prepare(spec);post.prepare(spec);
    IRLibrary library({&engine.cabinet(0),&engine.cabinet(1),&engine.cabinet(2)});
    library.prepare(spec,{(int)values.at("cabtype1"),(int)values.at("cabtype2"),(int)values.at("cabtype3")});
    juce::AudioBuffer<float> buffer(2,blockSize);
    const auto mode=(RoutingMode)(int)values.at("mode");const bool dualCross=mode==RoutingMode::dual && values.at("dualtype")>.5f;
    const float output=juce::Decibels::decibelsToGain(values.at("output"));
    const bool guitar=std::string(factoryPresets[(size_t)index].instrument)=="Guitar";
    const std::array<double,5> notes=guitar ? std::array<double,5>{82.4069,110,146.8324,195.9977,329.6276}
                                           : std::array<double,5>{30.8677,41.2034,55,73.4162,98};
    float peak=0;double energy=0;int samples=0;
    for(int offset=0;offset<int(rate*1.2);offset+=blockSize) {
        for(int n=0;n<blockSize;++n) {
            const double seconds=(offset+n)/rate,pluck=std::fmod(seconds,.2),envelope=std::exp(-pluck*9);
            const double note=notes[(size_t)(int(seconds/.2)%5)];
            const double phase=juce::MathConstants<double>::twoPi*note*seconds;
            const float input=seconds<1.0 ? float(.26*envelope*(std::sin(phase)+.25*std::sin(2*phase)+.17*std::sin(5*phase))) : 0.f;
            buffer.setSample(0,n,input);buffer.setSample(1,n,input*.8f);
        }
        pre.process(buffer,values.at("gateon")>.5f,values.at("gatethreshold"),values.at("gaterelease"),values.at("gatehold"),false,0,fx);
        engine.process(buffer,mode,dualCross ? values.at("dualcross") : values.at("x1"),values.at("x2"),states,&pre.cleanOutput(),dualCross,values.at("dualblend"));
        post.process(buffer,fx);
        for(int c=0;c<buffer.getNumChannels();++c)
            for(int n=0;n<blockSize;++n) {
                const float value=buffer.getSample(c,n)*output;
                require(std::isfinite(value),std::string("Non-finite factory preset output: ")+factoryPresets[(size_t)index].name);
                peak=std::max(peak,std::abs(value));energy+=double(value)*value;++samples;
            }
    }
    library.stop();
    require(peak<.99f,std::string("Factory preset clips the calibrated pluck fixture: ")+factoryPresets[(size_t)index].name);
    require(energy/std::max(1,samples)>1e-9,std::string("Silent factory preset: ")+factoryPresets[(size_t)index].name);
    return peak;
}
inline void run() {
    using namespace spectralforge;
    std::set<std::string> names;
    const auto baseline=snapshot(0);
    for(int index=0;index<factoryPresetCount;++index) {
        const auto& preset=factoryPresets[(size_t)index];const auto values=snapshot(index);
        require(names.insert(preset.name).second,"Duplicate factory preset name");
        require(preset.category[0] && preset.instrument[0] && preset.description[0],"Factory preset metadata is incomplete");
        require(values.size()==baseline.size(),"Factory preset modifies a parameter missing from its sound reset");
        require(!values.count("input") && !values.count("inputmode") && !values.count("tempo") && !values.count("temposync") && !values.count("tuneron"),"Factory preset changes performance controls");
        for(int lane=1;lane<=3;++lane) {
            const auto suffix=std::to_string(lane);
            require(values.at("cabtype"+suffix)!=3,"Factory preset depends on a user IR");
            require(values.at("amp"+suffix)>=0 && values.at("amp"+suffix)<ampModelCount,"Factory preset amp is missing");
        }
        if(values.at("mode")==2) require(values.at("drive1")==0 && values.at("lowampmix")==0,"Matrix preset has hidden LOW drive/blend");
        const auto peak44=render(index,44100,257),peak48=render(index,48000,127);
        std::cout<<"MEASURE factory preset "<<index<<" | "<<preset.name<<" | peak44 "<<peak44<<" | peak48 "<<peak48<<'\n';
    }
    int calls=0;require(!applyFactoryPreset(-1,[&](const char*,float){++calls;}) && !applyFactoryPreset(factoryPresetCount,[&](const char*,float){++calls;}) && calls==0,"Invalid factory preset changes sound");
    std::cout<<"PASS: "<<factoryPresetCount<<" portable complete factory presets; finite, non-silent, unclipped pluck output at 44.1/48 kHz\n";
}
}
