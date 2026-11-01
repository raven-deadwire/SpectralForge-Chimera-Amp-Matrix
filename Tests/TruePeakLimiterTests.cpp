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
    // Releases from the parameter's 5 ms minimum; the programmes as written and 12 dB hotter.
    for(double releaseMs:{5.,20.,50.})for(float drive:{1.f,3.98f}){
    float worstHere=-200.f;
    for(double rate:{44100.,48000.,88200.,96000.,192000.})for(double ceilingDb:{-1.,-3.,-.1})for(int kind=0;kind<6;++kind){
        TruePeakLimiter limiter;limiter.prepare(rate);
        auto x=programme(kind,rate,1.5);for(auto& f:x){f[0]*=drive;f[1]*=drive;}
        const double ceiling=std::pow(10.,ceilingDb/20.),release=std::exp(-1./(rate*releaseMs*.001));
        double minimum=1;
        for(auto& f:x)minimum=std::min(minimum,limiter.process(f.data(),2,ceiling,release));
        // The whole output, from the silence before the programme starts.
        const float tp=meterPeak(rate,x,0);
        const float over=tp-float(ceilingDb);worst=std::max(worst,over);worstHere=std::max(worstHere,over);
        require(minimum<1.,"the limiter did not act on programme "+std::to_string(kind));
        if(over>0.f)std::cout<<"OVER "<<over<<" dB: release "<<releaseMs<<" ms, drive "<<drive<<", "<<rate<<" Hz, ceiling "<<ceilingDb<<", programme "<<kind<<"\n";
        require(over<=0.f,"true peak "+std::to_string(tp)+" dBTP over the "+std::to_string(ceilingDb)+" dB ceiling at "+std::to_string(int(rate))+" Hz, programme "+std::to_string(kind));
    }
    std::cout<<"MEASURE release "<<releaseMs<<" ms, drive "<<(drive>1.f?"+12 dB":"0 dB")<<": worst output true peak relative to the ceiling "<<worstHere<<" dB\n";
    }
    std::cout<<"MEASURE worst output true peak relative to the ceiling over 3 releases x 2 drives x 5 rates x 3 ceilings x 6 programmes: "<<worst<<" dB\n";
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
        const int expected=juce::roundToInt(rate*.001)+2*spectralforge::truePeakDelay;
        require(limiter.latency()==expected,"latency is not 1 ms + twice the interpolator delay");
        auto x=programme(1,rate,.5);for(auto& f:x){f[0]*=.4f;f[1]*=.4f;}   // true peak about -2 dBTP
        auto y=x;for(auto& f:y)require(limiter.process(f.data(),2,1.,.999)==1.,"gain below 1 under the ceiling");
        for(size_t i=0;i<x.size();++i){const auto& in=i>=size_t(expected)?x[i-size_t(expected)]:std::array<float,2>{0.f,0.f};
            require(y[i][0]==in[0] && y[i][1]==in[1],"output is not the input delayed by the latency, bit for bit");}
    }
    std::cout<<"PASS below the ceiling the output is the input delayed by the reported latency, bit for bit (latency 1 ms + 16 samples)\n";
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

// The released Live limiter, verbatim (CommonDynamics::limit before 1.5),
// as a reference for the bit-exact check below.
struct ReleasedLiveLimiter {
    double rate{48000},limiterGain{1};
    void limit(juce::AudioBuffer<float>& audio,bool on,float ceilingDb,float releaseMs) noexcept {
        if(!on && limiterGain>=1.)return;
        const double ceiling=juce::Decibels::decibelsToGain(ceilingDb);
        const double release=std::exp(-1./(rate*(on?releaseMs:5.f)*.001));
        const int channels=std::min(2,audio.getNumChannels());
        for(int n=0;n<audio.getNumSamples();++n){
            double peak=0;for(int c=0;c<channels;++c)peak=std::max(peak,std::abs(double(audio.getSample(c,n))));
            const double target=on && peak>ceiling?ceiling/peak:1.;
            limiterGain=std::min(target,1.+(limiterGain-1.)*release);if(!on && limiterGain>1.-1e-7)limiterGain=1.;
            for(int c=0;c<channels;++c)audio.setSample(c,n,float(audio.getSample(c,n)*limiterGain));
        }
    }
};
void set(ChimeraProcessor& p,const char* id,float value){auto* q=p.parameters().getParameter(id);require(q!=nullptr,std::string("missing ")+id);q->setValueNotifyingHost(q->convertTo0to1(value));}
void render(ChimeraProcessor& p,juce::AudioBuffer<float>& audio,long& t,double amplitude,std::vector<float>* out=nullptr) {
    juce::MidiBuffer midi;
    for(int n=0;n<audio.getNumSamples();++n,++t){const float x=float(amplitude*std::sin(2*pi*220.*double(t)/48000.));audio.setSample(0,n,x);audio.setSample(1,n,x);}
    p.processBlock(audio,midi);
    if(out)for(int n=0;n<audio.getNumSamples();++n)out->push_back(audio.getSample(0,n));
}

void liveIsTheReleasedLimiter() {
    // CommonDynamics with Live selected against the released algorithm, on
    // the same parameters: loud noise limited at several ceilings and
    // releases, then switched off mid-release.
    auto p=std::make_unique<ChimeraProcessor>();spectralforge::CommonDynamics d;d.bind(p->parameters());d.prepare(48000);
    ReleasedLiveLimiter reference;reference.rate=48000;
    auto x=programme(2,48000.,3.);size_t t=0;long checked=0;
    for(int stage=0;stage<6;++stage){
        const bool on=stage<5;const float ceiling=-1.f-float(stage),release=stage%2?5.f:180.f;
        set(*p,"liveLimiterOn",on?1.f:0.f);set(*p,"liveLimiterCeiling",ceiling);set(*p,"liveLimiterRelease",release);
        for(int block=0;block<40;++block){
            juce::AudioBuffer<float> a(2,256),b(2,256);
            for(int n=0;n<256;++n,++t)for(int c=0;c<2;++c){a.setSample(c,n,x[t%x.size()][size_t(c)]);b.setSample(c,n,x[t%x.size()][size_t(c)]);}
            const auto stored=[&](const char* id){return p->parameters().getRawParameterValue(id)->load();};   // the values the plugin reads
            d.limit(a);reference.limit(b,stored("liveLimiterOn")>.5f,stored("liveLimiterCeiling"),stored("liveLimiterRelease"));
            for(int c=0;c<2;++c)for(int n=0;n<256;++n){require(a.getSample(c,n)==b.getSample(c,n),"Live differs from the released limiter");++checked;}
        }
    }
    std::cout<<"PASS Live is the released limiter bit for bit ("<<checked<<" samples, 5 ceilings, 5 and 180 ms release, switched off mid-release)\n";
}

// Mode switches on a pure 220 Hz sine through CommonDynamics: the largest
// sample-to-sample step around each switch against the sine's own.
void switchesAreClickFree() {
    auto p=std::make_unique<ChimeraProcessor>();spectralforge::CommonDynamics d;d.bind(p->parameters());d.prepare(48000);
    set(*p,"liveLimiterOn",1);set(*p,"liveLimiterCeiling",-1);
    long t=0;float last=0;bool first=true;
    const auto run=[&](int blocks){float step=0;juce::AudioBuffer<float> a(2,256);
        for(int b=0;b<blocks;++b){for(int n=0;n<256;++n,++t){const float x=.6f*float(std::sin(2*pi*220.*double(t)/48000.));a.setSample(0,n,x);a.setSample(1,n,x);}
            d.limit(a);for(int n=0;n<256;++n){const float y=a.getSample(0,n);if(!first)step=std::max(step,std::abs(y-last));last=y;first=false;}}
        return step;};
    const float own=run(40);
    set(*p,"limiterMode",1);const float into=run(40);
    set(*p,"limiterMode",0);const float back=run(40);
    std::cout<<"MEASURE 220 Hz sine, largest step: alone "<<own<<", switching Live to Studio "<<into<<", Studio to Live "<<back<<"\n";
    require(into<=own*1.01f && back<=own*1.01f,"a limiter mode switch makes a step");
    std::cout<<"PASS mode switches in both directions add no step\n";
}

void processor() {
    // Live (the default) and an explicit Live are the same; Live itself is
    // checked against the released algorithm in liveIsTheReleasedLimiter().
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
    require(studio==live+48+2*spectralforge::truePeakDelay,"Studio does not report its latency");
    a->resetOutputLoudness();for(int i=0;i<800;++i)render(*a,audio,ta,.8);
    const float tp=a->outputLoudness().truePeak();
    std::cout<<"MEASURE processor final output with Studio at -3 dBTP and +12 dB output: "<<tp<<" dBTP\n";
    require(tp<=-3.f+.02f,"the processor's final output exceeds the true-peak ceiling");
    // Back to Live restores the Live latency. (Clicks are measured on a pure
    // sine in switchesAreClickFree(): through the whole processor the
    // largest step comes from elsewhere in the chain and hides them.)
    set(*a,"output",-6);set(*a,"limiterMode",0);for(int i=0;i<40;++i)render(*a,audio,ta,.3);
    require(a->getLatencySamples()==live,"Live latency not restored after the switch");
    std::cout<<"PASS processor: Live unchanged; Studio reports "<<studio-live<<" samples and holds the ceiling; Live latency comes back\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {holdsTheCeiling();transparentBelowTheCeiling();recoversToExactlyOne();noAllocation();cpu();liveIsTheReleasedLimiter();switchesAreClickFree();processor();return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
