#pragma once
#include "PluginProcessor.h"
#include "FXParameters.h"
#include "OriginalCabParameters.h"
#include "AmpNativeParameters.h"
#include "PostNativeParameters.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

// Included beside the other full-processor tests. The enclosing translation
// unit supplies require() and set(). No audio warmup is discarded: every cold
// output sample is compared with an independent unity reference processor.
namespace cabPreparedStateTests {
constexpr int sampleRate=48000,blockSize=128;
constexpr float tolerance=1.e-6f;
using Audio=std::array<std::vector<float>,2>;

inline float value(ChimeraProcessor& p,const juce::String& id) {
    auto* parameter=p.parameters().getRawParameterValue(id);
    require(parameter!=nullptr,"Prepared CAB test parameter missing");return parameter->load();
}
inline void neutralLinear(ChimeraProcessor& p) {
    using namespace spectralforge;
    for(const auto& spec:fxSpecs)if(spec.toggle)set(p,spec.id,0);
    for(const auto* id:{"gateon","gateAfterRig","boardEnabled","transposeon","tuneron",
                       "tunermute","doubleron","metronome","input","output","inputmode",
                       "mode","oversampling","lowcomp","lowampmix"})set(p,id,0);
    for(int context=0;context<ampNativeContextCount;++context)set(p,ampNativeEnabledID(context),0);
    for(int section=0;section<3;++section)set(p,postNativeModeID(section),0);
    for(int lane=0;lane<3;++lane) {
        const auto suffix=juce::String(lane+1);
        set(p,"ampon"+suffix,0);set(p,"cab"+suffix,lane==0?1.f:0.f);
        set(p,"level"+suffix,0);set(p,"mute"+suffix,0);set(p,"solo"+suffix,0);set(p,"polarity"+suffix,0);
        set(p,"cabtype"+suffix,0);set(p,"cabBtype"+suffix,0);set(p,"cabblend"+suffix,0);
        set(p,"cablow"+suffix,70);set(p,"cabhigh"+suffix,9000);
        set(p,"cabBlow"+suffix,70);set(p,"cabBhigh"+suffix,9000);
        for(const auto* slot:{"A","B"}) {
            set(p,originalCabID(lane,(juce::String(slot)+"on").toRawUTF8()),0);
            for(const auto* control:{"gain","delay","invert"})set(p,"cab"+juce::String(slot)+control+suffix,0);
        }
    }
}
inline juce::String micID(int slot,const char* control) {
    return "cab"+juce::String(slot?"B":"A")+control+"1";
}
inline void selectMic(ChimeraProcessor& p,int slot,float gainDb,float delayMs,bool invert) {
    set(p,"cabblend1",slot?1.f:0.f);
    set(p,micID(slot,"gain"),gainDb);set(p,micID(slot,"delay"),delayMs);
    set(p,micID(slot,"invert"),invert?1.f:0.f);
}
inline Audio renderPrepared(ChimeraProcessor& p,int samples,int offset=0,bool impulse=true) {
    Audio result;for(auto& channel:result)channel.reserve(size_t(samples));
    juce::AudioBuffer<float> block(2,blockSize);juce::MidiBuffer midi;
    for(int base=0;base<samples;base+=blockSize) {
        const int count=std::min(blockSize,samples-base);block.setSize(2,count,false,false,true);
        for(int n=0;n<count;++n) {
            const int position=offset+base+n;
            const float x=impulse?(position==0?.2f:0.f):.12f*std::sin(float(position)*.098174770424681f);
            block.setSample(0,n,x);block.setSample(1,n,-.65f*x);
        }
        p.processBlock(block,midi);
        for(int channel=0;channel<2;++channel)for(int n=0;n<count;++n) {
            const float x=block.getSample(channel,n);
            require(std::isfinite(x),"Prepared CAB produced nonfinite audio");result[size_t(channel)].push_back(x);
        }
    }
    return result;
}
inline void coldCase(ChimeraProcessor& reference,ChimeraProcessor& subject,int slot,
                     float gainDb,float delayMs,bool invert,const char* phase) {
    neutralLinear(reference);neutralLinear(subject);
    selectMic(reference,slot,0,0,false);selectMic(subject,slot,gainDb,delayMs,invert);
    reference.prepareToPlay(sampleRate,blockSize);subject.prepareToPlay(sampleRate,blockSize);
    const auto unity=renderPrepared(reference,2048),actual=renderPrepared(subject,2048);
    const double gain=std::pow(10.,double(value(subject,micID(slot,"gain")))/20.)*(invert?-1.:1.);
    // The host stores float milliseconds. Compare against that exact public
    // value converted to float samples, including a possible fractional part.
    const double delay=float(sampleRate*.001)*value(subject,micID(slot,"delay"));
    const int whole=int(std::floor(delay));const double fraction=delay-whole;
    float error=0,referencePeak=0;int firstReference=-1,firstActual=-1;
    for(int channel=0;channel<2;++channel)for(size_t n=0;n<unity[size_t(channel)].size();++n) {
        const auto at=[&](int index){return index>=0?double(unity[size_t(channel)][size_t(index)]):0.;};
        const double expected=gain*((1.-fraction)*at(int(n)-whole)+fraction*at(int(n)-whole-1));
        error=std::max(error,float(std::abs(double(actual[size_t(channel)][n])-expected)));
        referencePeak=std::max(referencePeak,std::abs(unity[size_t(channel)][n]));
        if(channel==0 && firstReference<0 && std::abs(unity[0][n])>tolerance)firstReference=int(n);
        if(channel==0 && firstActual<0 && std::abs(actual[0][n])>tolerance)firstActual=int(n);
    }
    if(error>=tolerance)std::cerr<<"CAB_PREPARED_FAIL,"<<phase<<",slot="<<slot
        <<",max_error="<<error<<",reference_onset="<<firstReference<<",actual_onset="<<firstActual<<'\n';
    require(referencePeak>.01f,"Prepared CAB impulse reference is silent");
    require(error<tolerance,"Cold CAB lost saved gain, inversion, delay or selected mic");
    require(firstActual>=whole,"Cold CAB emitted audio before the saved mic delay");
    reference.releaseResources();subject.releaseResources();
    std::cout<<"PASS prepared CAB "<<phase<<" slot="<<slot<<" max_error="<<error<<'\n';
}
inline double ratio(const Audio& reference,const Audio& actual,int start,int count) {
    double energy=0,cross=0;
    for(int channel=0;channel<2;++channel)for(int n=start;n<start+count;++n) {
        const double x=reference[size_t(channel)][size_t(n)];
        energy+=x*x;cross+=x*actual[size_t(channel)][size_t(n)];
    }
    require(energy>1.e-5,"Prepared CAB gain-ramp reference is silent");return cross/energy;
}
inline void automationCase(int slot) {
    auto reference=std::make_unique<ChimeraProcessor>(),subject=std::make_unique<ChimeraProcessor>();
    neutralLinear(*reference);neutralLinear(*subject);
    selectMic(*reference,slot,0,0,false);selectMic(*subject,slot,-12,0,false);
    reference->prepareToPlay(sampleRate,blockSize);subject->prepareToPlay(sampleRate,blockSize);
    const double initial=std::pow(10.,double(value(*subject,micID(slot,"gain")))/20.);
    const auto unity=renderPrepared(*reference,2048,0,false),cold=renderPrepared(*subject,2048,0,false);
    for(int channel=0;channel<2;++channel)for(size_t n=0;n<unity[size_t(channel)].size();++n)
        require(std::abs(double(cold[size_t(channel)][n])-initial*unity[size_t(channel)][n])<tolerance,
                "Cold CAB gain starts at unity before reaching its saved value");
    set(*subject,micID(slot,"gain"),-6);
    const double target=std::pow(10.,double(value(*subject,micID(slot,"gain")))/20.),change=target-initial;
    const auto continued=renderPrepared(*reference,1536,2048,false),automated=renderPrepared(*subject,1536,2048,false);
    // POST's bypass alignment may delay observation. Account for at most the
    // reported pipeline latency; every earlier sample remains in the checks.
    const int latency=subject->getLatencySamples(),rampSamples=sampleRate/50;
    require(latency>=0 && rampSamples+latency<1536,"Prepared CAB ramp fixture is too short");
    const double immediate=ratio(continued,automated,0,32);
    const double middle=ratio(continued,automated,rampSamples/2+latency/2,32);
    require(immediate>=initial-tolerance && immediate<initial+.10*change,
            "CAB gain automation jumped instead of retaining its 20 ms smoothing");
    require(middle>initial+.35*change && middle<initial+.65*change,
            "CAB gain automation no longer follows the 20 ms transition");
    for(int channel=0;channel<2;++channel)for(int n=rampSamples+latency;n<1536;++n)
        require(std::abs(double(automated[size_t(channel)][size_t(n)])-target*continued[size_t(channel)][size_t(n)])<tolerance,
                "CAB gain automation did not reach its target after 20 ms and pipeline latency");
    reference->releaseResources();subject->releaseResources();
    std::cout<<"PASS CAB runtime gain ramp slot="<<slot<<" start="<<immediate<<" middle="<<middle<<'\n';
}
inline void run() {
    for(int slot=0;slot<2;++slot) {
        auto reference=std::make_unique<ChimeraProcessor>(),subject=std::make_unique<ChimeraProcessor>();
        coldCase(*reference,*subject,slot,-12,4,true,"fresh");
        // Switch to the other mic while stopped. Its prior controls and blend
        // must not seed the next prepare or an A/B-restored session.
        coldCase(*reference,*subject,1-slot,-6,7,false,"stop/change/reprepare");
        automationCase(slot);
    }
}
} // namespace cabPreparedStateTests
