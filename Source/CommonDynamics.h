#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

namespace spectralforge {
inline constexpr int commonDynamicsParameterCount=40;
inline juce::String dynamicSlotID(int slot,const char* field){return "dynamicEQ"+juce::String(slot+1)+"_"+field;}
inline juce::StringArray dynamicTargetNames(){
    juce::StringArray names{"Unassigned"};
    for(const auto* bank:{"Tone","Final"})for(int b=1;b<=12;++b)names.add(juce::String(bank)+" band "+juce::String(b));
    return names;
}
class DynamicTargetParameter final : public juce::AudioParameterChoice {
public:
    DynamicTargetParameter(int slot):AudioParameterChoice(juce::ParameterID{dynamicSlotID(slot,"target"),12},"Dynamic EQ "+juce::String(slot+1)+" target",dynamicTargetNames(),0){}
    bool isAutomatable() const override{return false;}
};
inline void appendCommonDynamicsParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout){
    const auto number=[&](const juce::String& id,const juce::String& name,float lo,float hi,float initial){
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,12},name,juce::NormalisableRange<float>{lo,hi,.001f},initial));};
    const auto toggle=[&](const char* id,const char* name){layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,12},name,false));};
    for(int s=0;s<4;++s){
        layout.add(std::make_unique<DynamicTargetParameter>(s));const auto name="Dynamic EQ "+juce::String(s+1)+" ";
        number(dynamicSlotID(s,"threshold"),name+"threshold",-72,0,-24);
        number(dynamicSlotID(s,"range"),name+"maximum cut",0,24,6);
        number(dynamicSlotID(s,"attack"),name+"attack",.1f,100,10);
        number(dynamicSlotID(s,"release"),name+"release",10,2000,120);
        number(dynamicSlotID(s,"ratio"),name+"ratio",1,20,2);
    }
    toggle("cleanCompOn","Clean compressor enabled");
    number("cleanCompThreshold","Clean compressor threshold",-60,0,-24);
    number("cleanCompRatio","Clean compressor ratio",1,20,4);
    number("cleanCompAttack","Clean compressor attack",.1f,100,10);
    number("cleanCompRelease","Clean compressor release",10,2000,120);
    number("cleanCompKnee","Clean compressor knee",0,24,6);
    number("cleanCompMakeup","Clean compressor makeup",-12,24,0);
    number("cleanCompMix","Clean compressor parallel mix",0,1,1);
    number("cleanCompHP","Clean compressor detector high pass",20,500,20);
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"cleanCompDetector",12},"Clean compressor detector",juce::StringArray{"Peak","RMS"},1));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"gateMode",12},"Gate or downward expander",juce::StringArray{"Gate","Expander"},0));
    number("gateRatio","Expander ratio",1,20,2);
    number("gateAttack","Gate and expander attack",.1f,20,.5f);
    toggle("liveLimiterOn","Live output limiter enabled");
    number("liveLimiterCeiling","Live output limiter sample peak ceiling",-18,0,-1);
    number("liveLimiterRelease","Live output limiter release",5,1000,100);
}
struct DynamicEQConfig {bool enabled{};float threshold{-24},range{6},attack{10},release{120},ratio{2};};
class DynamicEQBindings {
public:
    void bind(juce::AudioProcessorValueTreeState& state){
        constexpr std::array<const char*,6> fields{"target","threshold","range","attack","release","ratio"};
        for(int s=0;s<4;++s)for(size_t f=0;f<fields.size();++f){const auto id=dynamicSlotID(s,fields[f]);p[size_t(s)][f]=state.getParameter(id)?state.getRawParameterValue(id):nullptr;}
    }
    DynamicEQConfig forBand(int target) const noexcept {
        int matched=-1;
        // A malformed/legacy duplicate uses the first slot deterministically;
        // UI reports the duplicate without deactivating an already active node.
        for(int s=0;s<4;++s)if(p[size_t(s)][0] && int(read(p[size_t(s)][0],0,24,0))==target){matched=s;break;}
        if(matched<0)return {};
        const auto& v=p[size_t(matched)];
        return {true,read(v[1],-72,0,-24),read(v[2],0,24,6),read(v[3],.1f,100,10),read(v[4],10,2000,120),read(v[5],1,20,2)};
    }
    static float read(const std::atomic<float>* p,float lo,float hi,float fallback) noexcept {const float x=p?p->load():fallback;return std::isfinite(x)?std::clamp(x,lo,hi):fallback;}
private:
    std::array<std::array<std::atomic<float>*,6>,4> p{};
};

// Stereo-linked, feed-forward common dynamics. Bypassed defaults preserve
// existing audio exactly. These stages add no audio delay or lookahead.
class CommonDynamics {
public:
    void bind(juce::AudioProcessorValueTreeState& state){for(size_t i=0;i<ids.size();++i)p[i]=state.getRawParameterValue(ids[i]);}
    void prepare(double sr){rate=std::max(8000.,sr);wet.reset(rate,.005);makeup.reset(rate,.01);mix.reset(rate,.01);reset();}
    void reset(){hpIn={};hpOut={};energy=peakEnvelope=gr=0;limiterGain=1;wet.setCurrentAndTargetValue(get(0,0,1,0));makeup.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(get(6,-12,24,0)));mix.setCurrentAndTargetValue(get(7,0,1,1));compReduction.store(0);limitReduction.store(0);}
    bool expander() const noexcept{return get(10,0,1,0)>.5f;}
    float gateRatio() const noexcept{return get(11,1,20,2);}
    float gateAttack() const noexcept{return get(12,.1f,20,.5f);}
    static double compressionDB(double db,double threshold,double ratio,double knee) noexcept {
        const double over=db-threshold,slope=1.-1./ratio;
        if(knee>0 && over>-knee*.5 && over<knee*.5)return slope*(over+knee*.5)*(over+knee*.5)/(2*knee);
        return std::max(0.,over)*slope;
    }
    void compress(juce::AudioBuffer<float>& audio) noexcept {
        const bool on=get(0,0,1,0)>.5f;
        if(on && wet.getTargetValue()==0){hpIn={};hpOut={};energy=peakEnvelope=gr=0;}
        wet.setTargetValue(on?1.f:0.f);
        if(!on && !wet.isSmoothing()){compReduction.store(0);return;}
        const double threshold=get(1,-60,0,-24),ratio=get(2,1,20,4),knee=get(5,0,24,6);
        const double attack=std::exp(-1./(rate*get(3,.1f,100,10)*.001)),release=std::exp(-1./(rate*get(4,10,2000,120)*.001));
        const double hp=std::exp(-juce::MathConstants<double>::twoPi*get(8,20,500,20)/rate),rms=std::exp(-1./(rate*.01)),decay=std::exp(-1./(rate*.02));
        const bool useRMS=get(9,0,1,1)>.5f;const int channels=std::min(2,audio.getNumChannels());
        makeup.setTargetValue(juce::Decibels::decibelsToGain(get(6,-12,24,0)));mix.setTargetValue(get(7,0,1,1));
        for(int n=0;n<audio.getNumSamples();++n){
            double detector=0;
            for(int c=0;c<channels;++c){const auto i=size_t(c);const double x=audio.getSample(c,n);hpOut[i]=hp*(hpOut[i]+x-hpIn[i]);hpIn[i]=x;detector=std::max(detector,std::abs(hpOut[i]));}
            energy=rms*energy+(1-rms)*detector*detector;peakEnvelope=std::max(detector,peakEnvelope*decay);
            const double level=useRMS?std::sqrt(energy):peakEnvelope;
            const double desired=compressionDB(20*std::log10(std::max(1e-12,level)),threshold,ratio,knee);
            const double a=desired>gr?attack:release;gr=desired+(gr-desired)*a;
            const double m=mix.getNextValue(),branch=std::pow(10.,-gr*.05)*makeup.getNextValue();
            const double gain=1.+wet.getNextValue()*m*(branch-1.);
            for(int c=0;c<channels;++c)audio.setSample(c,n,float(audio.getSample(c,n)*gain));
        }
        compReduction.store(float(gr));
    }
    void limit(juce::AudioBuffer<float>& audio) noexcept {
        const bool on=get(13,0,1,0)>.5f;
        if(!on && limiterGain>=1.){limitReduction.store(0);return;}
        const double ceiling=juce::Decibels::decibelsToGain(get(14,-18,0,-1));
        const double release=std::exp(-1./(rate*(on?get(15,5,1000,100):5.f)*.001));
        const int channels=std::min(2,audio.getNumChannels());double minimum=1;
        for(int n=0;n<audio.getNumSamples();++n){
            double peak=0;for(int c=0;c<channels;++c)peak=std::max(peak,std::abs(double(audio.getSample(c,n))));
            const double target=on && peak>ceiling?ceiling/peak:1.;
            limiterGain=std::min(target,1.+(limiterGain-1.)*release);if(!on && limiterGain>1.-1e-7)limiterGain=1.;
            minimum=std::min(minimum,limiterGain);
            for(int c=0;c<channels;++c)audio.setSample(c,n,float(audio.getSample(c,n)*limiterGain));
        }
        limitReduction.store(float(-20*std::log10(std::max(1e-12,minimum))));
    }
    std::atomic<float> compReduction{0},limitReduction{0};
private:
    inline static constexpr std::array<const char*,16> ids{"cleanCompOn","cleanCompThreshold","cleanCompRatio","cleanCompAttack","cleanCompRelease","cleanCompKnee","cleanCompMakeup","cleanCompMix","cleanCompHP","cleanCompDetector","gateMode","gateRatio","gateAttack","liveLimiterOn","liveLimiterCeiling","liveLimiterRelease"};
    float get(size_t i,float lo,float hi,float fallback) const noexcept{return DynamicEQBindings::read(p[i],lo,hi,fallback);}
    std::array<std::atomic<float>*,16> p{};double rate{48000},energy{},peakEnvelope{},gr{},limiterGain{1};
    std::array<double,2> hpIn{},hpOut{};juce::SmoothedValue<float> wet,makeup,mix;
};
}
