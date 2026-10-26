#pragma once
#include "PostRigGate.h"
#include <array>
#include <atomic>

namespace spectralforge {
// The Gate page has a PRE and a POST gate. The released gate parameters keep
// their meaning: gateAfterRig places that gate, and Gate 2 takes the other
// side, so pre-rig and post-rig gating run together. The Expander is a separate
// stage with its own placement. Every unit detects on the clean input; a
// post-rig unit is latency aligned to the rig output.
inline constexpr int extraGateParameterVersion=14;
inline constexpr int extraGateParameterCount=16;
enum class ExtraGateUnit {gate2,expander};

inline void appendExtraGateParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout){
    const auto number=[&](const char* id,const char* name,float lo,float hi,float initial,float step=.01f){
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,extraGateParameterVersion},name,juce::NormalisableRange<float>{lo,hi,step},initial));};
    const auto toggle=[&](const char* id,const char* name,bool initial){
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,extraGateParameterVersion},name,initial));};
    const auto range=[&](const char* id,const char* name,float initial){
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,extraGateParameterVersion},name,
            juce::NormalisableRange<float>{0.f,NoiseGate::fullRangeDb,.1f},initial,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float v,int){return v>=NoiseGate::fullRangeDb?juce::String("Full"):juce::String(v,1)+" dB";})
                .withValueFromStringFunction([](const juce::String& text){return text.trim().equalsIgnoreCase("Full")?NoiseGate::fullRangeDb:text.getFloatValue();})));};
    toggle("gate2On","Gate 2 enabled (opposite side of the rig from the main gate)",false);
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"gate2Mode",extraGateParameterVersion},"Gate 2 gate or downward expander",juce::StringArray{"Gate","Expander"},0));
    number("gate2Threshold","Gate 2 threshold",-90,-15,-70);
    number("gate2Ratio","Gate 2 expander ratio",1,20,2);
    number("gate2Attack","Gate 2 attack",.1f,20,.5f);
    number("gate2Hold","Gate 2 hold",0,150,20);
    number("gate2Release","Gate 2 release",5,500,80);
    range("gate2RangeDb","Gate 2 range",NoiseGate::fullRangeDb);
    toggle("expOn","Expander enabled",false);
    toggle("expAfterRig","Expander after rig (input detector)",false);
    number("expThreshold","Expander threshold",-90,-15,-55);
    number("expRatio","Expander ratio",1,20,2);
    number("expAttack","Expander attack",.1f,20,2);
    number("expHold","Expander hold",0,150,10);
    number("expRelease","Expander release",5,500,150);
    range("expRangeDb","Expander range",24);
}

class ExtraGate {
public:
    explicit ExtraGate(ExtraGateUnit which):unit(which){}
    void bind(juce::AudioProcessorValueTreeState& state){
        const juce::String prefix=unit==ExtraGateUnit::gate2?"gate2":"exp";
        constexpr std::array<const char*,9> fields{"On","AfterRig","Threshold","Attack","Hold","Release","RangeDb","Ratio","Mode"};
        for(size_t i=0;i<fields.size();++i){const auto id=prefix+fields[i];p[i]=state.getParameter(id)?state.getRawParameterValue(id):nullptr;}
        // Gate 2 sits on the side of the rig the main gate does not use.
        if(unit==ExtraGateUnit::gate2)p[1]=state.getRawParameterValue("gateAfterRig");
    }
    void prepare(const juce::dsp::ProcessSpec& spec,int maximumLatencySamples){gate.prepare(spec,maximumLatencySamples);}
    void reset(){gate.reset();}
    bool enabled() const noexcept {return get(0,0,1,0)>.5f;}
    bool afterRig() const noexcept {
        if(unit==ExtraGateUnit::gate2)return get(1,0,1,0)<.5f;
        return get(1,0,1,0)>.5f;
    }
    // Call once per block on the clean input, before anything else processes it.
    void detect(const juce::AudioBuffer<float>& cleanInput,int postRigDelaySamples){
        const bool expander=unit==ExtraGateUnit::expander;
        gate.setMode(expander || get(8,0,1,0)>.5f,get(7,1,20,2),get(3,.1f,20,expander?2.f:.5f));
        gate.detect(cleanInput,enabled(),get(2,-90,-15,expander?-55.f:-70.f),get(5,5,500,expander?150.f:80.f),get(4,0,150,expander?10.f:20.f),
                    afterRig()?postRigDelaySamples:0,get(6,0,NoiseGate::fullRangeDb,expander?24.f:NoiseGate::fullRangeDb));
    }
    // Unity gains are skipped once settled, so a disabled unit stays bit exact.
    void apply(juce::AudioBuffer<float>& audio,juce::AudioBuffer<float>* roomBus=nullptr){if(enabled() || gate.reduction()<1.f)gate.apply(audio,roomBus);}
    float reduction() const {return gate.reduction();}
private:
    float get(size_t i,float lo,float hi,float fallback) const noexcept {
        if(!p[i])return fallback;const float x=p[i]->load();return std::isfinite(x)?juce::jlimit(lo,hi,x):fallback;
    }
    ExtraGateUnit unit;PostRigGate gate;std::array<std::atomic<float>*,9> p{};
};
}
