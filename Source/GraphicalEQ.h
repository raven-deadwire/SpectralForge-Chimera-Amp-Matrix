#pragma once
#include "CommonDynamics.h"
#include "SpectrumTransport.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <chrono>
#include <complex>
#include <algorithm>

namespace spectralforge {
inline constexpr int graphicalEQBands=12;
inline constexpr int graphicalEQParameterCount=2*(1+5*graphicalEQBands);
inline constexpr std::array<float,12> graphicalEQFrequencies{30,60,120,240,480,750,1000,2000,4000,6000,10000,16000};
inline juce::String eqID(int instance,int band,const char* field) {
    const juce::String prefix=instance==0 ? "toneEQ_" : "finalEQ_";
    return band<0 ? prefix+field : prefix+"b"+juce::String(band+1)+"_"+field;
}
inline juce::StringArray eqFilterNames() {return {"Bell","Low shelf","High shelf","High pass 12","Low pass 12","Notch","Band pass"};}
inline void appendGraphicalEQParameters(juce::AudioProcessorValueTreeState::ParameterLayout& p) {
    for(int e=0;e<2;++e) {
        const juce::String title=e==0 ? "Tone EQ " : "Final EQ ";
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{eqID(e,-1,"bypass"),10},title+"bypass",true));
        for(int b=0;b<graphicalEQBands;++b) {
            const auto label=title+juce::String(b+1)+" ";
            p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{eqID(e,b,"enabled"),10},label+"enabled",false));
            juce::NormalisableRange<float> frequency{20,20000,.01f};frequency.setSkewForCentre(1000);
            juce::NormalisableRange<float> q{.1f,18.f,.001f};q.setSkewForCentre(1);
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{eqID(e,b,"frequency"),10},label+"frequency",frequency,graphicalEQFrequencies[(size_t)b]));
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{eqID(e,b,"gain"),10},label+"gain",juce::NormalisableRange<float>{-24,24,.01f},0));
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{eqID(e,b,"q"),10},label+"Q",q,.70710678f));
            p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{eqID(e,b,"type"),10},label+"filter",eqFilterNames(),0));
        }
    }
}
struct EQCoefficients {
    double b0{1},b1{},b2{},a1{},a2{};
    double magnitude(double frequency,double rate) const noexcept {
        const auto z=std::polar(1.,-2*juce::MathConstants<double>::pi*frequency/rate);
        return std::abs((b0+b1*z+b2*z*z)/(1.+a1*z+a2*z*z));
    }
};
// Independent RBJ biquads. No FabFilter code, assets or proprietary transfer data.
inline EQCoefficients eqCoefficients(int type,double frequency,double gain,double q,double rate) noexcept {
    rate=std::max(8000.,rate);frequency=std::clamp(frequency,5.,rate*.475);q=std::clamp(q,.1,18.);
    const double w=2*juce::MathConstants<double>::pi*frequency/rate,c=std::cos(w),s=std::sin(w),alpha=s/(2*q);
    const double A=std::pow(10.,std::clamp(gain,-24.,24.)/40.),r=2*std::sqrt(A)*alpha;
    double b0=1,b1=0,b2=0,a0=1,a1=0,a2=0;
    switch(type) {
        case 0:b0=1+alpha*A;b1=-2*c;b2=1-alpha*A;a0=1+alpha/A;a1=-2*c;a2=1-alpha/A;break;
        case 1:b0=A*((A+1)-(A-1)*c+r);b1=2*A*((A-1)-(A+1)*c);b2=A*((A+1)-(A-1)*c-r);a0=(A+1)+(A-1)*c+r;a1=-2*((A-1)+(A+1)*c);a2=(A+1)+(A-1)*c-r;break;
        case 2:b0=A*((A+1)+(A-1)*c+r);b1=-2*A*((A-1)+(A+1)*c);b2=A*((A+1)+(A-1)*c-r);a0=(A+1)-(A-1)*c+r;a1=2*((A-1)-(A+1)*c);a2=(A+1)-(A-1)*c-r;break;
        case 3:b0=(1+c)/2;b1=-(1+c);b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
        case 4:b0=(1-c)/2;b1=1-c;b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
        case 5:b0=1;b1=-2*c;b2=1;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
        case 6:b0=alpha;b1=0;b2=-alpha;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
        default:break;
    }
    return {b0/a0,b1/a0,b2/a0,a1/a0,a2/a0};
}
class GraphicalEQ {
public:
    struct Values {bool enabled{};float frequency{1000},gain{},q{.707f};int type{};};
    void bind(juce::AudioProcessorValueTreeState& state,int instance) {
        bank=instance;dynamicParameters.bind(state);
        bypass=state.getRawParameterValue(eqID(instance,-1,"bypass"));
        constexpr std::array<const char*,5> fields{"enabled","frequency","gain","q","type"};
        for(int b=0;b<graphicalEQBands;++b)for(size_t f=0;f<fields.size();++f)params[(size_t)b][f]=state.getRawParameterValue(eqID(instance,b,fields[f]));
    }
    Values values(int band) const noexcept {
        const auto& p=params[(size_t)band];
        const auto read=[](const std::atomic<float>* v,float fallback){const float f=v->load();return std::isfinite(f)?f:fallback;};
        return {read(p[0],0)>.5f,std::clamp(read(p[1],1000),20.f,20000.f),std::clamp(read(p[2],0),-24.f,24.f),std::clamp(read(p[3],.70710678f),.1f,18.f),int(std::clamp(read(p[4],0),0.f,6.f))};
    }
    bool isBypassed() const noexcept {return bypass->load()>.5f;}
    double sampleRate() const noexcept {return uiRate.load();}
    float dynamicReduction(int band) const noexcept {return dynamicMeters[size_t(std::clamp(band,0,11))].load();}
    void prepare(double sr) noexcept {rate=std::max(8000.,sr);uiRate.store(rate);reset();average.store(0);peak.store(0);}
    void reset() noexcept {
        wet.reset(rate,.01);wet.setCurrentAndTargetValue(isBypassed()?0.f:1.f);clock=0;
        for(int i=0;i<graphicalEQBands;++i) {
            auto& b=bands[(size_t)i];const auto v=values(i);
            b.f.reset(rate/32,.015);b.g.reset(rate/32,.015);b.q.reset(rate/32,.015);
            b.f.setCurrentAndTargetValue(v.frequency);b.g.setCurrentAndTargetValue(v.gain);b.q.setCurrentAndTargetValue(v.q);
            b.wet.reset(rate,.01);b.wet.setCurrentAndTargetValue(v.enabled?1.f:0.f);
            b.type=v.type;b.nextType=v.type;b.fade.reset(rate,.01);b.fade.setCurrentAndTargetValue(1);
            b.current={};b.next={};b.current.coeff=eqCoefficients(v.type,v.frequency,v.gain,v.q,rate);
            b.detector={};b.reduction=b.envelope=0;b.detectorFrequency=b.detectorQ=0;dynamicMeters[size_t(i)].store(0);
        }
    }
    void process(juce::AudioBuffer<float>& buffer) noexcept {
        const auto start=std::chrono::steady_clock::now();
        const int channels=std::min(2,buffer.getNumChannels()),samples=buffer.getNumSamples();
        if(channels==0 || samples==0)return;
        juce::ScopedNoDenormals noDenormals;
        const bool bypassed=isBypassed();
        if(!bypassed && wet.getTargetValue()==0)for(auto& b:bands){b.current.clear();b.next.clear();}
        wet.setTargetValue(bypassed?0.f:1.f);
        const bool analyze=analyzer.enabled.load(std::memory_order_relaxed);
        if(bypassed && !wet.isSmoothing()) {
            for(int i=0;i<graphicalEQBands;++i){bands[size_t(i)].reduction=bands[size_t(i)].envelope=0;bands[size_t(i)].detector.clear();dynamicMeters[size_t(i)].store(0);}
            if(analyze)for(int n=0;n<samples;++n) {const auto l=buffer.getSample(0,n),r=buffer.getSample(channels-1,n);analyzer.push({l,l,r,r});}
        } else {
            // Read atomic host values once per block; coefficient work uses a 32-sample clock.
            bool hasDynamics=false;
            const double detectorDecay=std::exp(-1./(rate*.02));
            for(int i=0;i<graphicalEQBands;++i) {
                auto& b=bands[(size_t)i];const auto v=values(i);
                b.dynamic=dynamicParameters.forBand(bank*12+i+1);b.dynamic.enabled=b.dynamic.enabled && v.enabled && v.type<=2;
                if(b.dynamic.enabled || b.reduction>1e-6){b.dynamicAttack=std::exp(-1./(rate*b.dynamic.attack*.001));b.dynamicRelease=std::exp(-1./(rate*b.dynamic.release*.001));}
                hasDynamics|=b.dynamic.enabled || b.reduction>1e-6;
                if(b.dynamic.enabled && (b.detectorFrequency!=v.frequency || b.detectorQ!=v.q)){
                    b.detectorFrequency=v.frequency;b.detectorQ=v.q;b.detector.coeff=eqCoefficients(6,v.frequency,0,v.q,rate);
                }
                b.f.setTargetValue(v.frequency);b.g.setTargetValue(v.gain);b.q.setTargetValue(v.q);
                if(v.enabled && b.wet.getTargetValue()==0) {b.current.clear();b.next.clear();}
                b.wet.setTargetValue(v.enabled?1.f:0.f);
                // Finish the current crossfade before starting another; latest type wins.
                if(v.type!=b.type && !b.fade.isSmoothing()) {
                    b.nextType=v.type;b.next.clear();b.next.coeff=eqCoefficients(v.type,b.f.getCurrentValue(),b.g.getCurrentValue(),b.q.getCurrentValue(),rate);
                    b.fade.setCurrentAndTargetValue(0);b.fade.setTargetValue(1);
                }
            }
            for(int n=0;n<samples;++n) {
                const float inputL=buffer.getSample(0,n),inputR=buffer.getSample(channels-1,n);
                if(hasDynamics)for(auto& b:bands)if(b.dynamic.enabled || b.reduction>1e-6){
                    double peak=0;
                    if(b.dynamic.enabled){peak=std::abs(b.detector.tick(inputL,0));if(channels==2)peak=std::max(peak,std::abs(b.detector.tick(inputR,1)));}
                    b.envelope=std::max(peak,b.envelope*detectorDecay);
                    const double db=20*std::log10(std::max(1e-12,b.envelope));
                    const double target=b.dynamic.enabled?std::clamp((db-b.dynamic.threshold)*(1.-1./b.dynamic.ratio),0.,double(b.dynamic.range)):0.;
                    const double a=target>b.reduction?b.dynamicAttack:b.dynamicRelease;b.reduction=target+(b.reduction-target)*a;
                    if(b.reduction<1e-6)b.reduction=0;
                }
                if(clock==0)for(auto& b:bands) {
                    const auto f=b.f.getNextValue(),g=b.g.getNextValue(),q=b.q.getNextValue();
                    if(b.wet.getCurrentValue()==0 && b.wet.getTargetValue()==0)continue;
                    b.current.coeff=eqCoefficients(b.type,f,g-b.reduction,q,rate);
                    if(b.fade.isSmoothing())b.next.coeff=eqCoefficients(b.nextType,f,g-b.reduction,q,rate);
                }
                clock=(clock+1)%32;
                const float globalWet=wet.getNextValue();
                std::array<float,2> x{inputL,inputR};
                for(auto& b:bands) {
                    const float bandWet=b.wet.getNextValue();const bool transitioning=b.fade.isSmoothing();const float fade=b.fade.getNextValue();
                    if(bandWet>0 || b.wet.isSmoothing())for(int c=0;c<channels;++c) {
                        const double a=b.current.tick(x[(size_t)c],c);
                        const double filtered=transitioning ? a+fade*(b.next.tick(x[(size_t)c],c)-a) : a;
                        x[(size_t)c]+=bandWet*(float(filtered)-x[(size_t)c]);
                    }
                    if(transitioning && !b.fade.isSmoothing()) {b.current=b.next;b.type=b.nextType;}
                }
                const float l=globalWet==0 ? inputL : inputL+globalWet*(x[0]-inputL);
                const float r=channels==1 ? l : globalWet==0 ? inputR : inputR+globalWet*(x[1]-inputR);
                buffer.setSample(0,n,l);if(channels==2)buffer.setSample(1,n,r);
                if(analyze)analyzer.push({inputL,l,inputR,r});
            }
            // Applied coefficient gain, not the detector's requested range.
            for(int i=0;i<graphicalEQBands;++i){const auto& b=bands[size_t(i)];const double g=b.g.getCurrentValue();dynamicMeters[size_t(i)].store(b.type<=2?float((g-std::clamp(g-b.reduction,-24.,24.))*b.wet.getCurrentValue()*wet.getCurrentValue()):0.f);}
        }
        const auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        const float load=float(us*rate/(samples*10000.));
        average.store(.95f*average.load()+.05f*load);peak.store(std::max(load,.995f*peak.load()));
    }
    double tailSeconds() const noexcept {
        if(isBypassed())return 0;
        double tail=0;
        for(int i=0;i<graphicalEQBands;++i) {const auto v=values(i);if(!v.enabled)continue;const auto dynamic=dynamicParameters.forBand(bank*12+i+1);
            // Both the uncompressed and fully reduced filter can ring. Do not
            // under-report a boosted node's tail while its detector is quiet.
            const float reduced=v.gain-(dynamic.enabled&&v.type<=2?dynamic.range:0.f);double longest=0;
            for(float gain:{v.gain,reduced}){
                if(v.type<=2 && gain==0)continue;
                const auto c=eqCoefficients(v.type,v.frequency,gain,v.q,sampleRate());
                const auto root=std::sqrt(std::complex<double>(c.a1*c.a1-4*c.a2,0));
                const auto radius=std::max(std::abs((-c.a1+root)/2.),std::abs((-c.a1-root)/2.));
                if(radius>0 && radius<1)longest=std::max(longest,std::log(1.e-6)/(sampleRate()*std::log(radius)));
            }
            tail+=longest;
        }
        return tail;
    }
    EQAnalyzerFIFO analyzer;
    std::atomic<float> average{0},peak{0}; // Callback budget percent, EQ only.
private:
    struct Filter {
        EQCoefficients coeff;
        std::array<double,2> z1{},z2{};
        void clear() noexcept {z1={};z2={};}
        double tick(double x,int c) noexcept {const size_t i=(size_t)c;const double y=coeff.b0*x+z1[i];z1[i]=coeff.b1*x-coeff.a1*y+z2[i];z2[i]=coeff.b2*x-coeff.a2*y;return y;}
    };
    struct Band {Filter current,next,detector;juce::SmoothedValue<float> f,g,q,wet,fade;int type{},nextType{};DynamicEQConfig dynamic;double reduction{},envelope{},dynamicAttack{},dynamicRelease{},detectorFrequency{},detectorQ{};};
    std::array<Band,graphicalEQBands> bands;
    DynamicEQBindings dynamicParameters;std::array<std::atomic<float>,graphicalEQBands> dynamicMeters{};int bank{};
    std::atomic<float>* bypass{};
    std::array<std::array<std::atomic<float>*,5>,graphicalEQBands> params{};
    juce::SmoothedValue<float> wet;
    double rate{48000};std::atomic<double> uiRate{48000};int clock{};
};
}
