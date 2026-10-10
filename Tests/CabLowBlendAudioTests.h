#pragma once
#include "PluginProcessor.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace cabLowBlendAudioTests {
inline void require(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}

inline void set(ChimeraProcessor& processor,const juce::String& id,float value) {
    auto* parameter=processor.parameters().getParameter(id);
    require(parameter!=nullptr,"Missing Matrix LOW blend test parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

// Observe the production processor output. In particular the amplifier is ON:
// a state roundtrip with ampon1=0 cannot detect a second CAB wet/dry blend.
inline std::vector<float> render(float mix,bool movedMic=false,bool amplifier=true,bool cabinet=true) {
    auto processor=std::make_unique<ChimeraProcessor>();
    set(*processor,"mode",2);set(*processor,"lowampmix",mix);set(*processor,"lowcomp",0);
    for(const auto* id:{"gateon","transposeon","tuneron","boardEnabled","doubleron","metronome","oversampling"})
        set(*processor,id,0);
    set(*processor,"input",0);set(*processor,"output",0);
    for(const auto& spec:spectralforge::fxSpecs)if(spec.toggle)set(*processor,spec.id,0);
    for(int context=0;context<spectralforge::ampNativeContextCount;++context)
        set(*processor,spectralforge::ampNativeEnabledID(context),0);
    for(int section=0;section<3;++section)set(*processor,spectralforge::postNativeModeID(section),0);
    for(int lane=1;lane<=3;++lane) {
        const auto suffix=juce::String(lane);
        set(*processor,"ampon"+suffix,lane==1 && amplifier ? 1.f : 0.f);
        set(*processor,"cab"+suffix,lane==1 && cabinet ? 1.f : 0.f);
        set(*processor,"cabtype"+suffix,0);set(*processor,"cabBtype"+suffix,0);
        set(*processor,"level"+suffix,0);set(*processor,"bandtone"+suffix,0);
        set(*processor,"solo"+suffix,lane==1 ? 1.f : 0.f);
    }
    set(*processor,"cabblend1",0);set(*processor,"cablow1",30);set(*processor,"cabhigh1",9000);
    set(*processor,"ocab1_design",1);set(*processor,"ocab1_Aon",1);set(*processor,"ocab1_Bon",0);
    set(*processor,"ocab1_Amic",movedMic ? 2.f : 0.f);
    set(*processor,"ocab1_Aposition",movedMic ? .83f : .2f);
    set(*processor,"ocab1_Adistance",movedMic ? 46.f : 5.f);
    processor->prepareToPlay(48000,128);
    juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
    // Let the actual parameter, crossover, gain and convolution transitions
    // settle before applying the same deterministic input to every fixture.
    for(int block=0;block<64;++block){audio.clear();processor->processBlock(audio,midi);}
    std::vector<float> output;output.reserve(128*64*2);
    double energy=0;
    for(int block=0;block<64;++block) {
        for(int channel=0;channel<2;++channel)for(int n=0;n<128;++n) {
            const double t=double(block*128+n)/48000.;
            const float x=float(.05*std::sin(juce::MathConstants<double>::twoPi*(channel ? 53. : 41.)*t)
                               +.019*std::sin(juce::MathConstants<double>::twoPi*113.*t));
            audio.setSample(channel,n,x);
        }
        processor->processBlock(audio,midi);
        for(int n=0;n<128;++n)for(int channel=0;channel<2;++channel) {
            const float value=audio.getSample(channel,n);
            require(std::isfinite(value),"Matrix LOW blend produced a non-finite output");
            energy+=double(value)*value;output.push_back(value);
        }
    }
    processor->releaseResources();
    require(processor->backgroundResourcesReleased(),"Matrix LOW fixture retained its audio workers");
    require(energy>1e-8,"Matrix LOW fixture produced silence");
    return output;
}

inline float delta(const std::vector<float>& first,const std::vector<float>& second) {
    require(first.size()==second.size(),"Matrix LOW render lengths differ");
    float result=0;
    for(size_t n=0;n<first.size();++n)result=std::max(result,std::abs(first[n]-second[n]));
    return result;
}

inline void run() {
    const std::array<float,5> mixes{{0.f,.25f,.5f,.75f,1.f}};
    std::array<std::vector<float>,5> output;
    for(size_t i=0;i<mixes.size();++i)output[i]=render(mixes[i]);
    require(delta(output.front(),output.back())>1e-5f,
        "Matrix LOW endpoint fixtures do not distinguish clean DI from AMP + CAB");
    float residual=0;
    for(size_t i=1;i<4;++i)for(size_t n=0;n<output[i].size();++n) {
        const float expected=(1.f-mixes[i])*output.front()[n]+mixes[i]*output.back()[n];
        residual=std::max(residual,std::abs(output[i][n]-expected));
    }
    require(residual<2e-6f,"Matrix LOW must mix DI and the complete AMP + CAB branch exactly once");
    require(delta(output.front(),render(0,true))<2e-6f,
        "Moving the cabinet microphone altered Matrix LOW's clean DI endpoint");
    require(delta(output.back(),render(1,true))>1e-5f,
        "Moving the cabinet microphone does not reach Matrix LOW's AMP + CAB endpoint");
    require(delta(output.front(),render(.75f,true,false))<2e-6f,
        "Matrix LOW amplifier bypass did not return the stored blend to clean DI");
    require(delta(output.back(),render(1,false,true,false))>1e-5f,
        "Matrix LOW cabinet bypass did not remove the cabinet from the amplifier branch");
    std::cout<<"PASS Matrix LOW production blend: 0/25/50/75/100, complete AMP + CAB endpoint, DI isolation, amplifier/cabinet bypass; maximum interpolation residual="
        <<residual<<'\n';
}
}
