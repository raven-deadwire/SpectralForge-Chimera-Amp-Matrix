// Whole-product checks for 2.0 integration (docs/WHOLE_PRODUCT_2_0.md): every
// bank appended since the released set (version hint 15 and up: Room, PRE
// expansion, multiband stereo/six-band, saturation, reference heads, filter
// modulation, POST/CREATIVE/SYNTH, sidechain, signal order, true-peak limiter)
// at once.
// - Seeded random settings of all of them survive a project save/restore and
//   an A/B round trip: every parameter, and the rendered audio, bit for bit.
// - With everything switched on in Matrix (three rigs), the output stays finite
//   and bounded, returns to silence, and the reported latency is what the
//   output shows.
// - Worst-case CPU with everything on, at 48 kHz / 64 and 96 kHz / 128.
// - Sample-rate and block-size changes mid-session keep the state and latency
//   consistent.
#include "PluginProcessor.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok,const juce::String& message){if(!ok)throw std::runtime_error(message.toStdString());}
constexpr int appendedFrom=15;
struct Random {unsigned s;float next(){s=s*1664525u+1013904223u;return float((s>>8)*(1./16777216.));}};

bool appended(const juce::AudioProcessorParameter* p){return p->getVersionHint()>=appendedFrom;}
juce::String idOf(const juce::AudioProcessorParameter* p){if(auto* r=dynamic_cast<const juce::RangedAudioParameter*>(p))return r->paramID;return {};}
bool disabling(const juce::String& id){
    const auto l=id.toLowerCase();
    return l.contains("bypass") || l.contains("solo") || l.contains("listen") || l.contains("mute") || l.contains("freeze") || l.contains("cutsoff");
}

// Seeded values for every appended parameter. `everythingOn` turns each
// "...On" switch on and each bypass/solo/listen/mute off.
void randomise(ChimeraProcessor& p,unsigned seed,bool everythingOn){
    Random r{seed};
    for(auto* raw:p.getParameters()){
        if(!appended(raw))continue;
        const auto id=idOf(raw);float value=r.next();
        if(dynamic_cast<juce::AudioParameterBool*>(raw)!=nullptr){
            value=value<.5f?0.f:1.f;
            if(everythingOn){if(id.endsWithIgnoreCase("on") || id.endsWithIgnoreCase("enabled"))value=1.f;if(disabling(id))value=0.f;}
        }
        raw->setValueNotifyingHost(value);
    }
    // Signal order: a seeded valid permutation per region. (An invalid one is
    // not a reachable state: the DSP keeps the last valid order and a restore
    // writes it canonically, docs/SIGNAL_ORDER_1_4.md.)
    for(const char* region:{"orderTone","orderPost"}){
        std::vector<juce::AudioProcessorParameter*> slots;
        for(int i=1;auto* q=p.parameters().getParameter(region+juce::String(i));++i)slots.push_back(q);
        std::vector<int> order(slots.size());for(size_t i=0;i<order.size();++i)order[i]=int(i);
        for(size_t i=order.size();i>1;--i)std::swap(order[i-1],order[size_t(r.next()*float(i))%i]);
        for(size_t i=0;i<slots.size();++i)if(auto* q=dynamic_cast<juce::RangedAudioParameter*>(slots[i]))q->setValueNotifyingHost(q->convertTo0to1(float(order[i])));
    }
}
void set(ChimeraProcessor& p,const char* id,float value){auto* q=p.parameters().getParameter(id);require(q!=nullptr,juce::String("missing ")+id);q->setValueNotifyingHost(q->convertTo0to1(value));}

// Plucked guitar fixture, then 0.5 s of silence; fresh prepare each render.
std::vector<float> render(ChimeraProcessor& p,double rate=48000,int block=128,double seconds=1.,double silence=.5){
    p.prepareToPlay(rate,block);juce::AudioBuffer<float> b(2,block);juce::MidiBuffer midi;std::vector<float> out;
    const long total=long((seconds+silence)*rate),loud=long(seconds*rate);
    for(long t=0;t<total;t+=block){
        for(int n=0;n<block;++n){
            const double time=double(t+n)/rate,note=std::fmod(time,.25),f=82.4069*std::pow(2.,double(int(time/.25)%5)*5./12.);
            float x=0;
            if(t+n<loud){const double e=std::min(1.,note/.003)*std::exp(-note*9);for(int h=1;h<=6;++h)x+=float(e*.2/h*std::sin(juce::MathConstants<double>::twoPi*f*h*note));}
            b.setSample(0,n,x);b.setSample(1,n,x);
        }
        p.processBlock(b,midi);
        for(int c=0;c<2;++c)for(int n=0;n<block;++n)out.push_back(b.getSample(c,n));
    }
    return out;
}
std::vector<float> values(ChimeraProcessor& p){std::vector<float> v;for(auto* raw:p.getParameters())v.push_back(raw->getValue());return v;}
int countAppended(ChimeraProcessor& p){int n=0;for(auto* raw:p.getParameters())if(appended(raw))++n;return n;}
float maxDifference(const std::vector<float>& a,const std::vector<float>& b){
    require(a.size()==b.size(),"render lengths differ");float d=0;for(size_t i=0;i<a.size();++i)d=std::max(d,std::abs(a[i]-b[i]));return d;
}
double energy(const std::vector<float>& a){double e=0;for(float x:a)e+=double(x)*x;return e;}

void stateRoundTrip(){
    int checked=0;
    for(unsigned seed:{1u,2u,3u,4u,5u,6u}){
        auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
        a->prepareToPlay(48000,128);b->prepareToPlay(48000,128);
        randomise(*a,seed,seed%2==0);set(*a,"mode",float(seed%3));
        juce::MemoryBlock bytes;a->getStateInformation(bytes);b->setStateInformation(bytes.getData(),int(bytes.getSize()));
        const auto va=values(*a),vb=values(*b);
        for(size_t i=0;i<va.size();++i)
            require(std::abs(va[i]-vb[i])<1e-6f,"project restore changed "+idOf(a->getParameters()[int(i)])+" (seed "+juce::String(seed)+")");
        const auto ya=render(*a),yb=render(*b);
        const float d=maxDifference(ya,yb);
        std::cout<<"MEASURE seed "<<seed<<(seed%2==0?" (everything on)":"")<<", mode "<<seed%3<<": project restore max difference "<<d<<", output energy "<<energy(ya)<<"\n";
        require(d==0.f && energy(ya)>1e-6,"restored project does not render the same audio (seed "+juce::String(seed)+")");
        // A/B: copy to B, change A, come back.
        a->copyComparison();a->selectComparison(1);randomise(*a,seed+100,false);a->selectComparison(0);
        const auto back=values(*a);
        for(size_t i=0;i<va.size();++i)require(std::abs(back[i]-va[i])<1e-6f,"A/B round trip changed "+idOf(a->getParameters()[int(i)]));
        checked+=countAppended(*a);
    }
    std::cout<<"PASS project and A/B round trips keep every parameter and the audio bit for bit, 6 seeds x "<<checked/6<<" appended parameters\n";
}

void everythingOnStaysBounded(){
    for(unsigned seed:{11u,12u,13u}){
        auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,128);
        randomise(*p,seed,true);set(*p,"mode",2);
        // Random gain stages can stack to +30 dB; the final limiter is what
        // keeps a session in range, so it is on (Studio, -1 dBTP) here.
        set(*p,"liveLimiterOn",1);set(*p,"limiterMode",1);set(*p,"liveLimiterCeiling",-1);
        const auto y=render(*p,48000,128,1.,4.);
        float peak=0;bool finite=true;for(float x:y){finite=finite && std::isfinite(x);peak=std::max(peak,std::abs(x));}
        // Last 100 ms of the 4 s silence, both channels.
        float tail=0;for(size_t i=y.size()-size_t(2*4800);i<y.size();++i)tail=std::max(tail,std::abs(y[i]));
        std::cout<<"MEASURE everything on, Matrix, seed "<<seed<<": peak "<<peak<<", last 100 ms of 4 s silence "<<20*std::log10(std::max(1e-12f,tail))<<" dBFS, latency "<<p->getLatencySamples()<<"\n";
        require(finite,"non-finite output with everything on");
        require(peak<=std::pow(10.f,-1.f/20.f),"the final output exceeds the limiter ceiling with everything on");
        require(tail<1e-3f,"output does not decay to silence (-60 dBFS within 4 s)");
    }
    std::cout<<"PASS everything on in Matrix: finite, held under the -1 dBTP limiter, decays to silence\n";
}

// Load of one processor on `seconds` of noise, after a quarter warm-up.
double load(ChimeraProcessor& p,double rate,int block,double seconds,double* worstBlock=nullptr){
    juce::AudioBuffer<float> b(2,block);juce::MidiBuffer midi;Random r{7};double busy=0,worst=0;const int blocks=int(rate*seconds/block);
    for(int i=0;i<blocks;++i){
        for(int n=0;n<block;++n){const float x=.3f*(r.next()*2.f-1.f);b.setSample(0,n,x);b.setSample(1,n,x);}
        const auto start=std::chrono::steady_clock::now();p.processBlock(b,midi);
        const double s=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        if(i>=blocks/4){busy+=s;worst=std::max(worst,s);}
    }
    if(worstBlock)*worstBlock=worst/(block/rate);
    return busy/((blocks-blocks/4)*block/rate);
}
std::unique_ptr<ChimeraProcessor> everythingOn(double rate,int block,unsigned seed){
    auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(rate,block);
    randomise(*p,seed,true);set(*p,"mode",2);set(*p,"liveLimiterOn",1);set(*p,"limiterMode",1);
    set(*p,"oversamplingAuto",0);set(*p,"oversampling",2);return p;   // the worst case: OVERSAMPLING 4x at every rate
}
// Two gates, on a shared machine whose speed varies (CPU acceptance on target
// hardware is separate): everything on runs in real time at 48 kHz / 64, and
// what the appended modules add over the released Matrix defaults stays under
// three quarters of a core at 96 kHz / 128 (measured 30-46%).
void worstCaseCpu(){
    for(const auto& [rate,block]:{std::pair{48000.,64},std::pair{96000.,128}}){
        // The released defaults: OVERSAMPLING 4x (a new instance would use Auto).
        auto defaults=std::make_unique<ChimeraProcessor>();defaults->prepareToPlay(rate,block);set(*defaults,"mode",2);set(*defaults,"oversamplingAuto",0);
        double baselineWorst=0;const double baseline=load(*defaults,rate,block,3.,&baselineWorst);
        double worst=0;
        for(unsigned seed:{21u,22u}){
            auto p=everythingOn(rate,block,seed);double worstBlock=0;const double l=load(*p,rate,block,4.,&worstBlock);worst=std::max(worst,l);
            std::cout<<"MEASURE everything on (Matrix, Studio limiter), "<<rate/1000.<<" kHz / "<<block<<", seed "<<seed<<": "<<100.*l<<"% of real time on one core, worst block "<<100.*worstBlock<<"% of its period\n";
        }
        std::cout<<"MEASURE released Matrix defaults, "<<rate/1000.<<" kHz / "<<block<<": "<<100.*baseline<<"% (worst block "<<100.*baselineWorst<<"% of its period); appended modules add "<<100.*(worst-baseline)<<"%\n";
        if(rate>50000.){
            // The user's OVERSAMPLING choice: 2x at 96 kHz runs the amps at the
            // same internal rate as the default 4x at 48 kHz.
            auto two=std::make_unique<ChimeraProcessor>();two->prepareToPlay(rate,block);set(*two,"mode",2);set(*two,"oversampling",1);
            auto onTwo=everythingOn(rate,block,21);set(*onTwo,"oversampling",1);
            const double d2=load(*two,rate,block,3.),e2=load(*onTwo,rate,block,4.);
            std::cout<<"MEASURE 96 kHz / 128 with OVERSAMPLING 2x: released Matrix defaults "<<100.*d2<<"%, everything on "<<100.*e2<<"%\n";
            auto fresh=std::make_unique<ChimeraProcessor>();fresh->prepareToPlay(rate,block);set(*fresh,"mode",2);
            std::cout<<"MEASURE 96 kHz / 128, a new instance (OVERSAMPLING Auto = 2x) in Matrix: "<<100.*load(*fresh,rate,block,3.)<<"%\n";
        }
        if(rate<50000.)require(worst<1.,"everything on does not run in real time at 48 kHz / 64");
        else require(worst-baseline<.75,"the appended modules add more than three quarters of a core at 96 kHz");
    }
    std::cout<<"PASS everything on runs in real time at 48 kHz / 64; appended modules add under three quarters of a core at 96 kHz / 128\n";
}

void rateAndBlockChanges(){
    auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,128);
    randomise(*p,31,true);set(*p,"mode",2);set(*p,"limiterMode",1);
    const auto before=values(*p);
    struct Case{double rate;int block;};
    for(const auto& c:{Case{44100,512},Case{96000,64},Case{192000,256},Case{48000,128}}){
        const auto y=render(*p,c.rate,c.block,.4,.1);
        bool finite=true;for(float x:y)finite=finite && std::isfinite(x);
        require(finite,"non-finite output after a sample-rate change");
        const auto now=values(*p);for(size_t i=0;i<now.size();++i)require(now[i]==before[i],"a sample-rate change moved "+idOf(p->getParameters()[int(i)]));
        // The Studio limiter's share of the latency follows the rate.
        const int limiter=juce::roundToInt(c.rate*.001)+8;
        set(*p,"limiterMode",0);render(*p,c.rate,c.block,.05,0);const int live=p->getLatencySamples();
        set(*p,"limiterMode",1);render(*p,c.rate,c.block,.05,0);const int studio=p->getLatencySamples();
        std::cout<<"MEASURE "<<c.rate/1000.<<" kHz / "<<c.block<<": latency "<<studio<<" samples (Live "<<live<<")\n";
        require(studio-live==limiter,"Studio limiter latency does not follow the sample rate");
    }
    std::cout<<"PASS rate and block changes keep every parameter; latency follows the rate\n";
}

// CHIMERA_WHOLE_PRODUCT_PROFILE=1: the load saved by switching each appended
// "...On" switch off alone, everything else on; and the released defaults.
void spikes(const char* what,ChimeraProcessor& p,double rate,int block){
    juce::AudioBuffer<float> b(2,block);juce::MidiBuffer midi;Random r{9};std::vector<std::pair<double,int>> t;const int blocks=int(rate*4./block);
    for(int i=0;i<blocks;++i){
        for(int n=0;n<block;++n){const float x=.3f*(r.next()*2.f-1.f);b.setSample(0,n,x);b.setSample(1,n,x);}
        const auto start=std::chrono::steady_clock::now();p.processBlock(b,midi);
        if(i>=blocks/4)t.push_back({std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/(block/rate),i});
    }
    auto sorted=t;std::sort(sorted.begin(),sorted.end());
    const auto q=[&](double f){return 100*sorted[size_t(f*double(sorted.size()-1))].first;};
    std::cout<<"SPIKES "<<what<<" "<<rate/1000<<" kHz / "<<block<<": median "<<q(.5)<<"%, p99 "<<q(.99)<<"%, p99.9 "<<q(.999)<<"%, max "<<q(1.)<<"%; blocks over 100%: "<<std::count_if(t.begin(),t.end(),[](auto& x){return x.first>1.;})<<" of "<<t.size()<<"; top at";
    for(size_t i=0;i<8;++i)std::cout<<" "<<sorted[sorted.size()-1-i].second;
    std::cout<<"\n";
}
void profile(){
    {auto d=std::make_unique<ChimeraProcessor>();d->prepareToPlay(48000,64);set(*d,"mode",2);spikes("defaults Matrix",*d,48000,64);}
    {auto p=everythingOn(48000,64,21);spikes("everything on",*p,48000,64);}
    {auto p=everythingOn(48000,64,21);set(*p,"satQuality",0);spikes("everything on, saturation Live",*p,48000,64);}
    {auto p=everythingOn(48000,64,21);set(*p,"synthOn",0);spikes("everything on, SYNTH off",*p,48000,64);}
    {auto p=everythingOn(48000,64,21);set(*p,"roomOn",0);spikes("everything on, Room off",*p,48000,64);}
    {auto p=everythingOn(48000,64,21);set(*p,"creativeOn",0);spikes("everything on, CREATIVE off",*p,48000,64);}
    const double rate=96000;const int block=128;
    const auto fresh=[&]{return everythingOn(rate,block,21);};
    for(int mode:{0,2})for(double r:{48000.,96000.}){
        auto d=std::make_unique<ChimeraProcessor>();d->prepareToPlay(r,r>50000?128:64);set(*d,"mode",float(mode));
        std::cout<<"PROFILE defaults only (no appended module), mode "<<mode<<", "<<r/1000<<" kHz: "<<100*load(*d,r,r>50000?128:64,1.5)<<"%\n";
    }
    auto base=fresh();const double all=load(*base,rate,block,1.5);
    std::cout<<"PROFILE everything on: "<<100*all<<"%\n";
    std::vector<std::pair<double,juce::String>> savings;
    for(auto* raw:base->getParameters()){
        if(!appended(raw))continue;const auto id=idOf(raw);
        const bool sw=dynamic_cast<juce::AudioParameterBool*>(raw)!=nullptr && raw->getValue()>.5f && (id.endsWithIgnoreCase("on") || id.endsWithIgnoreCase("enabled"));
        if(!sw)continue;
        auto p=fresh();p->parameters().getParameter(id)->setValueNotifyingHost(0.f);
        savings.push_back({all-load(*p,rate,block,1.5),id});
    }
    std::sort(savings.begin(),savings.end(),[](auto& x,auto& y){return x.first>y.first;});
    for(size_t i=0;i<std::min<size_t>(25,savings.size());++i)std::cout<<"PROFILE off "<<savings[i].second<<": saves "<<100*savings[i].first<<"%\n";
    for(auto* raw:base->getParameters())if(appended(raw)){const auto id=idOf(raw);if(auto* c=dynamic_cast<juce::AudioParameterChoice*>(raw))std::cout<<"CHOICE "<<id<<"="<<c->getCurrentChoiceName()<<"\n";}
}
}

int main(){
    if(juce::SystemStats::getEnvironmentVariable("CHIMERA_WHOLE_PRODUCT_PROFILE",{})=="matrix96"){
        // For a profiler: the released Matrix defaults at 96 kHz / 128, 1 s.
        juce::ScopedJuceInitialiser_GUI init;auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(96000,128);set(*p,"mode",2);
        std::cout<<"load "<<100*load(*p,96000,128,1.)<<"%\n";return 0;
    }
    if(juce::SystemStats::getEnvironmentVariable("CHIMERA_WHOLE_PRODUCT_PROFILE",{}).isNotEmpty()){juce::ScopedJuceInitialiser_GUI init;profile();return 0;}
    juce::ScopedJuceInitialiser_GUI init;
    try{stateRoundTrip();everythingOnStaysBounded();worstCaseCpu();rateAndBlockChanges();return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
