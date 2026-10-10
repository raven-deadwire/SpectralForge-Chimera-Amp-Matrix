#pragma once
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
struct EQAnalyzerFrame {float inputL{},outputL{},inputR{},outputR{};};
class EQAnalyzerFIFO {
public:
    static constexpr uint32_t capacity=8192;
    void push(EQAnalyzerFrame frame) noexcept {
        const auto w=write.load(std::memory_order_relaxed);
        if(w-read.load(std::memory_order_acquire)>=capacity) {dropped.fetch_add(1,std::memory_order_relaxed);return;}
        frames[w%capacity]=frame;write.store(w+1,std::memory_order_release);
    }
    bool pop(EQAnalyzerFrame& frame) noexcept {
        const auto r=read.load(std::memory_order_relaxed);
        if(r==write.load(std::memory_order_acquire))return false;
        frame=frames[r%capacity];read.store(r+1,std::memory_order_release);return true;
    }
    std::atomic<bool> enabled{false};
    std::atomic<uint64_t> dropped{0};
private:
    std::array<EQAnalyzerFrame,capacity> frames{};
    std::atomic<uint32_t> read{0},write{0}; // Unsigned subtraction also works across wrap.
};
class GraphicalEQ {
public:
    struct Values {bool enabled{};float frequency{1000},gain{},q{.707f};int type{};};
    void bind(juce::AudioProcessorValueTreeState& state,int instance) {
        bypass=state.getRawParameterValue(eqID(instance,-1,"bypass"));
        constexpr std::array<const char*,5> fields{"enabled","frequency","gain","q","type"};
        for(int b=0;b<graphicalEQBands;++b)for(size_t f=0;f<fields.size();++f)params[(size_t)b][f]=state.getRawParameterValue(eqID(instance,b,fields[f]));
    }
    Values values(int band) const noexcept {
        const auto& p=params[(size_t)band];
        return {p[0]->load()>.5f,p[1]->load(),p[2]->load(),p[3]->load(),int(p[4]->load())};
    }
    bool isBypassed() const noexcept {return bypass->load()>.5f;}
    double sampleRate() const noexcept {return uiRate.load();}
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
            if(analyze)for(int n=0;n<samples;++n) {const auto l=buffer.getSample(0,n),r=buffer.getSample(channels-1,n);analyzer.push({l,l,r,r});}
        } else {
            // Read atomic host values once per block; coefficient work uses a 32-sample clock.
            for(int i=0;i<graphicalEQBands;++i) {
                auto& b=bands[(size_t)i];const auto v=values(i);
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
                if(clock==0)for(auto& b:bands) {
                    const auto f=b.f.getNextValue(),g=b.g.getNextValue(),q=b.q.getNextValue();
                    if(b.wet.getCurrentValue()==0 && b.wet.getTargetValue()==0)continue;
                    b.current.coeff=eqCoefficients(b.type,f,g,q,rate);
                    if(b.fade.isSmoothing())b.next.coeff=eqCoefficients(b.nextType,f,g,q,rate);
                }
                clock=(clock+1)%32;
                const float globalWet=wet.getNextValue();
                const float inputL=buffer.getSample(0,n),inputR=buffer.getSample(channels-1,n);
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
        }
        const auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        const float load=float(us*rate/(samples*10000.));
        average.store(.95f*average.load()+.05f*load);peak.store(std::max(load,.995f*peak.load()));
    }
    double tailSeconds() const noexcept {
        if(isBypassed())return 0;
        double tail=0;
        for(int i=0;i<graphicalEQBands;++i) {const auto v=values(i);if(!v.enabled || (v.type<=2 && v.gain==0))continue;
            const auto c=eqCoefficients(v.type,v.frequency,v.gain,v.q,sampleRate());
            const auto root=std::sqrt(std::complex<double>(c.a1*c.a1-4*c.a2,0));
            const auto radius=std::max(std::abs((-c.a1+root)/2.),std::abs((-c.a1-root)/2.));
            if(radius>0 && radius<1)tail+=std::log(1.e-6)/(sampleRate()*std::log(radius));
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
    struct Band {Filter current,next;juce::SmoothedValue<float> f,g,q,wet,fade;int type{},nextType{};};
    std::array<Band,graphicalEQBands> bands;
    std::atomic<float>* bypass{};
    std::array<std::array<std::atomic<float>*,5>,graphicalEQBands> params{};
    juce::SmoothedValue<float> wet;
    double rate{48000};std::atomic<double> uiRate{48000};int clock{};
};
}
