// Studio true-peak limiter (docs/TRUE_PEAK_LIMITER_1_5.md): the ceiling holds
// the true peak the loudness meter reads (4x / 2x interpolation) at 44.1-192
// kHz on inter-sample-peak sines, overs, noise and bursts; below the ceiling
// the output is the input delayed by the reported latency, bit for bit; the
// processor reports that latency only while Studio is selected, keeps the
// default Live limiter bit-identical, holds the ceiling on its final output,
// switches modes without a step, and allocates nothing on the audio thread.
#include "PluginProcessor.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

namespace {thread_local bool countAllocations=false;thread_local size_t allocations=0;}
void* operator new(std::size_t n) {if(countAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

namespace {
using spectralforge::LoudnessMeter;
using spectralforge::TruePeakLimiter;
void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
constexpr double pi=juce::MathConstants<double>::pi;

// Test programmes, stereo, `seconds` long.
std::vector<std::array<float,2>> programme(int kind,double rate,double seconds) {
    const size_t n=size_t(seconds*rate);std::vector<std::array<float,2>> x(n);unsigned s=12345u;
    const auto noise=[&]{s=s*1664525u+1013904223u;return float((s>>8)*(1./16777216.)*2.-1.);};
    for(size_t i=0;i<n;++i){
        const double t=double(i)/rate;float l=0,r=0;
        switch(kind){
            case 0:l=r=float(std::sin(pi*.5*double(i)+pi*.25));break;                                   // fs/4 at 45 degrees: samples 0.707, true peak 1.0
            case 1:l=float(2.*std::sin(2*pi*997.*t));r=float(1.4*std::sin(2*pi*1499.*t+1.));break;          // overs, +6 dBFS
            case 2:l=1.8f*noise();r=1.8f*noise();break;                                                    // loud noise
            case 3:{const bool on=std::fmod(t,.05)<.004;l=on?1.6f:.05f*noise();r=on?-1.6f:.05f*noise();break;} // square bursts with full-scale edges
            case 4:l=r=float(1.3*std::sin(2*pi*(rate*.23)*t));break;                                       // high sine near fs/4
            default:{const double env=.5+.5*std::sin(2*pi*3.*t);l=float(env*1.5*std::sin(2*pi*8000.*t));r=float(env*1.5*std::sin(2*pi*11025.*t+.3));}
        }
        x[i]={l,r};
    }
    return x;
}
float meterPeak(double rate,const std::vector<std::array<float,2>>& y,size_t skip) {
    LoudnessMeter m;m.prepare(rate);std::vector<float> l,r;
    for(size_t i=skip;i<y.size();++i){l.push_back(y[i][0]);r.push_back(y[i][1]);}
    const float* data[2]{l.data(),r.data()};
    for(size_t done=0;done<l.size();done+=4096){const int n=int(std::min<size_t>(4096,l.size()-done));const float* d[2]{data[0]+done,data[1]+done};m.process(d,2,n);}
    return m.truePeak();
}

void holdsTheCeiling() {
    float worst=-200.f;
    for(double rate:{44100.,48000.,88200.,96000.,192000.})for(double ceilingDb:{-1.,-3.,-.1})for(int kind=0;kind<6;++kind){
        TruePeakLimiter limiter;limiter.prepare(rate);
        auto x=programme(kind,rate,1.5);const double ceiling=std::pow(10.,ceilingDb/20.),release=std::exp(-1./(rate*.05));
        double minimum=1;
        for(auto& f:x)minimum=std::min(minimum,limiter.process(f.data(),2,ceiling,release));
        // The whole output, from the silence before the programme starts.
        const float tp=meterPeak(rate,x,0);
        const float over=tp-float(ceilingDb);worst=std::max(worst,over);
        require(minimum<1.,"the limiter did not act on programme "+std::to_string(kind));
        require(over<=0.f,"true peak "+std::to_string(tp)+" dBTP over the "+std::to_string(ceilingDb)+" dB ceiling at "+std::to_string(int(rate))+" Hz, programme "+std::to_string(kind));
    }
    std::cout<<"MEASURE worst output true peak relative to the ceiling over 5 rates x 3 ceilings x 6 programmes: "<<worst<<" dB\n";
    // The same fs/4 sine meeting a -1 dBFS sample-peak ceiling exactly keeps its +3 dB of inter-sample peak.
    {
        auto x=programme(0,48000.,1.);for(auto& f:x)for(auto& v:f)v*=float(std::pow(10.,-1./20.)/std::sqrt(.5));
        std::cout<<"MEASURE fs/4 at 45 degrees with -1 dBFS sample peaks (what a sample-peak ceiling passes): "<<meterPeak(48000.,x,0)<<" dBTP\n";
    }
    std::cout<<"PASS Studio mode holds the true-peak ceiling at 44.1-192 kHz\n";
}

void transparentBelowTheCeiling() {
    for(double rate:{44100.,48000.,96000.,192000.}){
        TruePeakLimiter limiter;limiter.prepare(rate);
        const int expected=juce::roundToInt(rate*.001)+spectralforge::truePeakDelay;
        require(limiter.latency()==expected,"latency is not 1 ms + the interpolator delay");
        auto x=programme(1,rate,.5);for(auto& f:x){f[0]*=.4f;f[1]*=.4f;}   // true peak about -2 dBTP
        auto y=x;for(auto& f:y)require(limiter.process(f.data(),2,1.,.999)==1.,"gain below 1 under the ceiling");
        for(size_t i=0;i<x.size();++i){const auto& in=i>=size_t(expected)?x[i-size_t(expected)]:std::array<float,2>{0.f,0.f};
            require(y[i][0]==in[0] && y[i][1]==in[1],"output is not the input delayed by the latency, bit for bit");}
    }
    std::cout<<"PASS below the ceiling the output is the input delayed by the reported latency, bit for bit (latency 1 ms + 8 samples)\n";
}

void recoversToExactlyOne() {
    TruePeakLimiter limiter;limiter.prepare(48000.);const double release=std::exp(-1./(48000.*.02));
    auto x=programme(1,48000.,.3);for(auto& f:x)limiter.process(f.data(),2,.5,release);
    std::array<float,2> f{};double g=0;for(int i=0;i<48000;++i){f={.1f,-.1f};g=limiter.process(f.data(),2,.5,release);}
    require(g==1. && f[0]==.1f,"the gain does not return to exactly 1 after a release");
    std::cout<<"PASS after the release the gain is exactly 1 again\n";
}

void noAllocation() {
    TruePeakLimiter limiter;limiter.prepare(48000.);auto x=programme(2,48000.,.2);
    allocations=0;countAllocations=true;for(auto& f:x)limiter.process(f.data(),2,.8,.999);countAllocations=false;
    require(allocations==0,"the limiter allocates while processing");
    std::cout<<"PASS no allocation while processing\n";
}

void cpu() {
    TruePeakLimiter limiter;limiter.prepare(48000.);auto x=programme(2,48000.,10.);
    const auto start=std::chrono::steady_clock::now();for(auto& f:x)limiter.process(f.data(),2,.8,.9995);
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"MEASURE stereo Studio limiter at 48 kHz: "<<100.*seconds/10.<<"% of one core (10 s of loud noise in "<<seconds*1000.<<" ms)\n";
    require(seconds<1.,"the Studio limiter takes more than 10% of a core");
}

void set(ChimeraProcessor& p,const char* id,float value){auto* q=p.parameters().getParameter(id);require(q!=nullptr,std::string("missing ")+id);q->setValueNotifyingHost(q->convertTo0to1(value));}
void render(ChimeraProcessor& p,juce::AudioBuffer<float>& audio,long& t,double amplitude,std::vector<float>* out=nullptr) {
    juce::MidiBuffer midi;
    for(int n=0;n<audio.getNumSamples();++n,++t){const float x=float(amplitude*std::sin(2*pi*220.*double(t)/48000.));audio.setSample(0,n,x);audio.setSample(1,n,x);}
    p.processBlock(audio,midi);
    if(out)for(int n=0;n<audio.getNumSamples();++n)out->push_back(audio.getSample(0,n));
}

void processor() {
    // Default Live: the reported latency and the audio are those of a processor with no mode parameter set.
    auto a=std::make_unique<ChimeraProcessor>();auto b=std::make_unique<ChimeraProcessor>();
    a->prepareToPlay(48000,256);b->prepareToPlay(48000,256);juce::AudioBuffer<float> audio(2,256);long ta=0,tb=0;
    set(*a,"liveLimiterOn",1);set(*b,"liveLimiterOn",1);set(*a,"limiterMode",0);
    std::vector<float> ya,yb;for(int i=0;i<200;++i){render(*a,audio,ta,.3,&ya);render(*b,audio,tb,.3,&yb);}
    require(ya==yb,"selecting Live changes the audio");
    const int live=a->getLatencySamples();
    // Studio: the latency grows by the limiter's, the final output holds the ceiling.
    set(*a,"limiterMode",1);set(*a,"liveLimiterCeiling",-3);set(*a,"output",12);
    for(int i=0;i<400;++i)render(*a,audio,ta,.8);
    const int studio=a->getLatencySamples();
    std::cout<<"MEASURE processor latency at 48 kHz: Live "<<live<<", Studio "<<studio<<" samples\n";
    require(studio==live+48+spectralforge::truePeakDelay,"Studio does not report its latency");
    a->resetOutputLoudness();for(int i=0;i<800;++i)render(*a,audio,ta,.8);
    const float tp=a->outputLoudness().truePeak();
    std::cout<<"MEASURE processor final output with Studio at -3 dBTP and +12 dB output: "<<tp<<" dBTP\n";
    require(tp<=-3.f+.02f,"the processor's final output exceeds the true-peak ceiling");
    // Switching back fades through silence: no sample-to-sample step above the programme's own.
    std::vector<float> y;set(*a,"output",-6);set(*a,"liveLimiterOn",0);for(int i=0;i<80;++i)render(*a,audio,ta,.3);
    for(int i=0;i<40;++i)render(*a,audio,ta,.3,&y);
    float normal=0;for(size_t i=1;i<y.size();++i)normal=std::max(normal,std::abs(y[i]-y[i-1]));
    y.clear();set(*a,"limiterMode",0);for(int i=0;i<40;++i)render(*a,audio,ta,.3,&y);
    float step=0;for(size_t i=1;i<y.size();++i)step=std::max(step,std::abs(y[i]-y[i-1]));
    std::cout<<"MEASURE largest step switching Studio to Live: "<<step<<" (programme alone "<<normal<<")\n";
    require(step<=normal*1.05f+1e-6f,"the mode switch makes a step");
    require(a->getLatencySamples()==live,"Live latency not restored after the switch");
    std::cout<<"PASS processor: Live unchanged; Studio reports "<<studio-live<<" samples and holds the ceiling; switching is click-free\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {holdsTheCeiling();transparentBelowTheCeiling();recoversToExactlyOne();noAllocation();cpu();processor();return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
