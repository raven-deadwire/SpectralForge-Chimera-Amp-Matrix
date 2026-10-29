// Measurements for the two-filter modulation processor (docs/FILTER_MODULATION_1_7.md).
#include "PluginProcessor.h"
#include "FilterModulation.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace { std::atomic<bool> allocationWatch{false}; std::atomic<size_t> watchedAllocations{0}; }
void* operator new(std::size_t size) { if(allocationWatch.load(std::memory_order_relaxed))++watchedAllocations;if(auto* p=std::malloc(std::max(size,std::size_t{1})))return p;throw std::bad_alloc(); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

namespace {
using namespace spectralforge;
constexpr double pi=3.14159265358979323846,rate=48000;
void require(bool yes,const std::string& message){if(!yes)throw std::runtime_error(message);}
double db(double v){return 20*std::log10(std::max(v,1.e-30));}
FilterSettings base() {
    FilterSettings s;s.on=true;
    s.filters[0]={true,0,0,1000.f,0.f,0.f,0.f};s.filters[1]={false,1,0,80.f,0.f,0.f,0.f};
    return s;
}
struct Stereo {std::vector<float> left,right;};
template<class Input> Stereo run(const FilterSettings& s,int samples,Input input,double bpm=120,const FilterSettings* initial=nullptr) {
    FilterModulationCore core;core.prepare(rate);core.setInitialState(initial?*initial:s);
    Stereo out{std::vector<float>((size_t)samples),std::vector<float>((size_t)samples)};
    for(int n=0;n<samples;++n){out.left[(size_t)n]=input(n,0);out.right[(size_t)n]=input(n,1);}
    for(int start=0;start<samples;start+=256) {
        const int count=std::min(256,samples-start);std::array<float*,2> p{out.left.data()+start,out.right.data()+start};
        core.process(p.data(),2,count,s,bpm);
    }
    return out;
}
auto sine(double hz,double amplitude=.1){return [=](int n,int){return float(amplitude*std::sin(2*pi*hz*n/rate));};}
double tone(const std::vector<float>& v,double hz,int from) {
    const int length=int(std::round(int((v.size()-size_t(from))*hz/rate)*rate/hz))-1;double re=0,im=0;
    for(int n=0;n<length;++n){const double w=2*pi*hz*n/rate;re+=v[size_t(from+n)]*std::cos(w);im+=v[size_t(from+n)]*std::sin(w);}
    return 2*std::sqrt(re*re+im*im)/length;
}
double gainDb(const FilterSettings& s,double hz){const auto out=run(s,48000,sine(hz));return db(tone(out.left,hz,24000)/.1);}
// Analog prototype at the bilinear-warped frequency (the SVF is prewarped at the cutoff).
double analyticDb(int type,int slope,double fc,double q,double f) {
    const double r=std::tan(pi*f/rate)/std::tan(pi*fc/rate);const std::complex<double> s(0,r);
    const auto section=[&](double sectionQ)->std::complex<double> {
        const auto d=s*s+s/sectionQ+1.;
        switch(type){case 1:return s*s/d;case 2:return (s/sectionQ)/d;case 3:return (s*s+1.)/d;default:return 1./d;}
    };
    const auto h=slope==0?section(q):section(.5411961)*section(q*1.3065630/.70710678);
    return db(std::abs(h));
}

void response() {
    double worst=0;
    for(int type=0;type<4;++type)for(int slope=0;slope<2;++slope)for(double r:{0.,.5}) {
        auto s=base();s.filters[0].type=type;s.filters[0].slope=slope;s.filters[0].resonance=float(r*100);
        const double q=filter_detail::resonanceQ(r);
        for(double hz:{125.,500.,1000.,2000.,8000.}) {
            if(type==3 && hz==1000.)continue;   // the notch's zero is checked separately
            worst=std::max(worst,std::abs(gainDb(s,hz)-analyticDb(type,slope,1000,q,hz)));
        }
    }
    auto lp=base();
    const double cutoff=gainDb(lp,1000),octave=gainDb(lp,2000)-gainDb(lp,1000);
    lp.filters[0].slope=1;const double octave24=gainDb(lp,4000)-gainDb(lp,2000);
    auto bp=base();bp.filters[0].type=2;const double peak=gainDb(bp,1000);
    auto notch=base();notch.filters[0].type=3;const double depth=gainDb(notch,1000);
    auto resonant=base();resonant.filters[0].resonance=100;const double ringing=gainDb(resonant,1000);
    std::cout<<"MEASURE response vs analytic (4 types x 2 slopes x 2 resonances x 5 frequencies): largest error "<<worst<<" dB\n";
    std::cout<<"MEASURE LP12 at cutoff "<<cutoff<<" dB; LP12 2k re 1k "<<octave<<" dB; LP24 4k re 2k "<<octave24<<" dB; BP peak "<<peak<<" dB; notch at 1 kHz "<<depth<<" dB; LP12 resonance 100% at cutoff "<<ringing<<" dB\n";
    require(worst<.05,"Filter response differs from the analytic prototype");
    require(std::abs(cutoff+3.01)<.05 && std::abs(peak)<.05 && depth<-60 && std::abs(ringing-db(20))<.1,"Cutoff, band-pass peak, notch or resonance off");

    // Routing: serial LP 1k into HP 200 is a band; parallel averages them.
    auto serial=base();serial.filters[1]={true,1,0,200.f,0.f,0.f,0.f};
    auto parallel=serial;parallel.routing=1;
    const double serial100=gainDb(serial,100),serial5k=gainDb(serial,5000),serialMid=gainDb(serial,450);
    const double parallelLow=gainDb(parallel,50),parallelHigh=gainDb(parallel,8000);
    std::cout<<"MEASURE serial LP 1k > HP 200: 100 Hz "<<serial100<<", 450 Hz "<<serialMid<<", 5 kHz "<<serial5k<<" dB; parallel: 50 Hz "<<parallelLow<<", 8 kHz "<<parallelHigh<<" dB\n";
    require(serial100<-6 && serial5k<-12 && serialMid>-2,"Serial routing is not A into B");
    require(std::abs(parallelLow+6.02)<1 && std::abs(parallelHigh+6.02)<1,"Parallel routing does not average the two filters");
    std::cout<<"PASS response, slopes, resonance, routing\n";
}

void offAndSwitching() {
    auto off=base();off.on=false;
    const auto input=[](int n,int c){return float(.3*std::sin(2*pi*(c?330:220)*n/rate)+.1*std::sin(2*pi*4100*n/rate));};
    const auto out=run(off,24000,input);
    bool exact=true;for(int n=0;n<24000;++n)exact=exact&&out.left[(size_t)n]==input(n,0)&&out.right[(size_t)n]==input(n,1);
    auto zeroMix=base();zeroMix.mix=0;const auto dry=run(zeroMix,24000,input);
    bool mixExact=true;for(int n=0;n<24000;++n)mixExact=mixExact&&dry.left[(size_t)n]==input(n,0);
    require(exact,"Off changed the signal");require(mixExact,"Mix 0 changed the signal");

    // Turning on mid-stream: 10 ms crossfade. Compare the largest second
    // difference across the switch with the settled processed signal's.
    // A single low tone, so the input's own curvature is small and a step in
    // the output would stand out.
    FilterModulationCore core;core.prepare(rate);core.setInitialState(off);
    auto on=base();on.filters[0].cutoff=3000;
    const auto low=[](int n,int){return float(.3*std::sin(2*pi*220*n/rate));};
    double steady=0,switching=0;std::vector<float> y(48000);
    for(int n=0;n<48000;++n)y[(size_t)n]=low(n,0);
    for(int start=0;start<48000;start+=128){std::array<float*,1> p{y.data()+start};core.process(p.data(),1,128,start<12000?off:on,120);}
    for(int n=2;n<48000;++n){const double d=std::abs(y[(size_t)n]-2*y[(size_t)n-1]+y[(size_t)n-2]);if(n>=11800 && n<13000)switching=std::max(switching,d);if(n>30000)steady=std::max(steady,d);}
    double bypassSteady=0;for(int n=2;n<11800;++n)bypassSteady=std::max(bypassSteady,std::abs(double(low(n,0))-2*low(n-1,0)+low(n-2,0)));
    std::cout<<"MEASURE switching on: largest second difference "<<switching<<" against "<<bypassSteady<<" (off) and "<<steady<<" (on, settled)\n";
    require(switching<std::max(steady,bypassSteady)*1.5,"Switching on clicks");
    std::cout<<"PASS off and Mix 0 sample-exact; click-free switching\n";
}

void modulation() {
    // LFO: a 2 kHz tone through LP 1 kHz with a one-octave sweep is amplitude
    // modulated at the LFO rate. Count the envelope's cycles over 4 s.
    const auto cycles=[](const FilterSettings& s,double bpm) {
        const auto out=run(s,4*48000,sine(2000),bpm);
        std::vector<double> env;for(size_t n=0;n+480<=out.left.size();n+=480){double m=0;for(size_t k=n;k<n+480;++k)m=std::max(m,double(std::abs(out.left[k])));env.push_back(m);}
        double mean=0;for(size_t i=10;i<env.size();++i)mean+=env[i];mean/=double(env.size()-10);
        int crossings=0;for(size_t i=11;i<env.size();++i)if(env[i-1]<mean && env[i]>=mean)++crossings;
        return crossings;
    };
    auto free=base();free.filters[0].lfoOct=1;free.lfoRate=3;
    auto synced=free;synced.lfoSync=3;   // 1/4 at 90 BPM = 1.5 Hz
    auto eighthTriplet=free;eighthTriplet.lfoSync=7;   // 1/8 T at 90 BPM = 4.5 Hz
    const int freeCycles=cycles(free,90),syncCycles=cycles(synced,90),tripletCycles=cycles(eighthTriplet,90);
    std::cout<<"MEASURE LFO cycles in 3.9 s: free 3 Hz "<<freeCycles<<"; 1/4 at 90 BPM (1.5 Hz) "<<syncCycles<<"; 1/8 T at 90 BPM (4.5 Hz) "<<tripletCycles<<"\n";
    require(std::abs(freeCycles-12)<=1 && std::abs(syncCycles-6)<=1 && std::abs(tripletCycles-18)<=1,"LFO rate or tempo sync wrong");
    // Exact rate from the phase accumulator: 1/8 D at 100 BPM is 0.75 beats,
    // 2.222 Hz, so 1 s advances 2.2222 cycles (fraction 0.2222).
    {FilterModulationCore core;auto dotted=free;dotted.lfoSync=9;core.prepare(rate);core.setInitialState(dotted);std::vector<float> silence(480);std::array<float*,1> p{silence.data()};
     for(int i=0;i<100;++i)core.process(p.data(),1,480,dotted,100);
     const double fraction=core.lfoPhase(),expected=100./60./.75-2.;
     std::cout<<"MEASURE 1/8 D at 100 BPM after 1 s: LFO phase "<<fraction<<" (expected "<<expected<<")\n";
     require(std::abs(fraction-expected)<1.e-6,"Dotted-eighth sync rate wrong");}

    // Stereo phase 180: the right channel sweeps opposite to the left.
    auto stereo=free;stereo.lfoStereoDeg=180;
    const auto out=run(stereo,48000,sine(2000));
    double corr=0,ll=0,rr=0;std::vector<double> l,r;
    for(size_t n=4800;n+480<=out.left.size();n+=480){double a=0,b=0;for(size_t k=n;k<n+480;++k){a=std::max(a,double(std::abs(out.left[k])));b=std::max(b,double(std::abs(out.right[k])));}l.push_back(a);r.push_back(b);}
    double ml=0,mr=0;for(size_t i=0;i<l.size();++i){ml+=l[i];mr+=r[i];}ml/=double(l.size());mr/=double(r.size());
    for(size_t i=0;i<l.size();++i){corr+=(l[i]-ml)*(r[i]-mr);ll+=(l[i]-ml)*(l[i]-ml);rr+=(r[i]-mr)*(r[i]-mr);}
    const double correlation=corr/std::sqrt(ll*rr);
    std::cout<<"MEASURE stereo 180 deg: correlation of left/right envelopes "<<correlation<<"\n";
    require(correlation<-.9,"Stereo LFO phase offset not applied");

    // Envelope: LP 250 Hz opened +3 octaves by full-scale envelope; a loud
    // 2 kHz tone passes more than a quiet one (level-normalized).
    auto env=base();env.filters[0].cutoff=250;env.filters[0].envOct=3;env.envSensitivityDb=12;
    const auto loud=run(env,48000,sine(2000,.5)),quiet=run(env,48000,sine(2000,.005));
    const double opening=db(tone(loud.left,2000,24000)/.5)-db(tone(quiet.left,2000,24000)/.005);
    std::cout<<"MEASURE envelope +3 oct on LP 250 Hz, 2 kHz tone: loud passes "<<opening<<" dB more than quiet (relative to input)\n";
    require(opening>20,"Envelope follower does not open the filter");

    std::cout<<"PASS LFO free/synced rates, stereo phase, envelope opening\n";
}

void realtimeAndParameters() {
    FilterModulationCore core;core.prepare(rate);auto s=base();s.filters[1].on=true;s.filters[0].lfoOct=2;s.filters[1].envOct=-2;s.filters[0].slope=1;s.lfoShape=2;core.setInitialState(s);
    std::vector<float> data(512);for(int n=0;n<512;++n)data[(size_t)n]=float(std::sin(n*.1));std::array<float*,1> p{data.data()};
    allocationWatch=true;for(int i=0;i<50;++i)core.process(p.data(),1,512,s,140);allocationWatch=false;
    bool finite=true;for(float v:data)finite=finite&&std::isfinite(v);
    require(watchedAllocations==0 && finite,"Filter processing allocated or became non-finite");
    ChimeraProcessor processor;int seen=0;const int total=processor.getParameters().size();
    const int later=total-spectralforge::postExpansionParameterCount()-spectralforge::effectExtensionParameterCount-spectralforge::creativeParameterCount;   // banks appended after the filters
    for(auto* raw:processor.getParameters())if(auto* id=dynamic_cast<juce::AudioProcessorParameterWithID*>(raw);id && id->paramID.startsWith("flt")){
        require(raw->getVersionHint()==filterModulationParameterVersion && raw->getParameterIndex()>=later-filterModulationParameterCount && raw->getParameterIndex()<later,"Filter parameters are not appended at their version");++seen;}
    require(seen==filterModulationParameterCount,"Filter parameter bank incomplete");
    std::cout<<"PASS no allocation while processing; "<<seen<<" parameters appended at version "<<filterModulationParameterVersion<<"\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI juce;
    try {response();offAndSwitching();modulation();realtimeAndParameters();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
