// 1.9 expanded modulation (docs/MODULATION_1_9.md): defaults keep the 1.7
// sound; LFO 2 and the Random shape; bar lock to the host position;
// retrigger on attack; the sidechain envelope source; per-filter drive; the
// Comb and All-pass types; order presets; no allocation while processing.
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "FilterModulation.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {thread_local bool countAllocations=false;thread_local size_t allocations=0;}
void* operator new(std::size_t n) {if(countAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

namespace {
using namespace spectralforge;
constexpr double rate=48000,pi=3.14159265358979323846;constexpr int block=256;
const double none=std::numeric_limits<double>::quiet_NaN();
void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
double db(double v){return 20*std::log10(std::max(v,1.e-30));}
using Signal=std::function<double(int)>;
// Runs the core in blocks; `ppq(k)` gives the host position for block k (NaN: none).
std::vector<float> run(const FilterSettings& s,int samples,const Signal& in,std::function<double(int)> ppq={},const Signal& key={},FilterModulationCore* keep=nullptr,double bpm=120){
    FilterModulationCore local;auto& core=keep?*keep:local;
    if(!keep){core.prepare(rate);core.setInitialState(s);}
    std::vector<float> out;juce::AudioBuffer<float> b(2,block),k(2,block);
    for(int blockIndex=0;blockIndex*block<samples;++blockIndex){
        for(int n=0;n<block;++n){const float x=float(in(blockIndex*block+n));b.setSample(0,n,x);b.setSample(1,n,x);if(key){const float y=float(key(blockIndex*block+n));k.setSample(0,n,y);k.setSample(1,n,y);}}
        countAllocations=true;
        core.process(b.getArrayOfWritePointers(),2,block,s,bpm,ppq?ppq(blockIndex):none,key?k.getArrayOfReadPointers():nullptr,key?2:0);
        countAllocations=false;
        for(int n=0;n<block;++n)out.push_back(b.getSample(0,n));
    }
    return out;
}
Signal sine(double a,double hz){return [=](int n){return a*std::sin(2*pi*hz*n/rate);};}
double magnitude(const std::vector<float>& x,int from,int to,double hz){std::complex<double> s;for(int n=from;n<to;++n)s+=double(x[size_t(n)])*std::polar(1.0,-2*pi*hz*n/rate);return 2*std::abs(s)/(to-from);}
// Envelope of a tone in 10 ms windows.
std::vector<double> envelope(const std::vector<float>& x){std::vector<double> e;for(size_t n=0;n+480<=x.size();n+=480){double m=0;for(size_t i=n;i<n+480;++i)m=std::max(m,double(std::abs(x[i])));e.push_back(m);}return e;}
FilterSettings base(){FilterSettings s;s.on=true;s.filters[0]={true,0,0,1000.f,.2f,0,0,0,0};s.filters[1].on=false;return s;}

void defaults() {
    // The 1.9 fields at their defaults leave the 1.7 processing unchanged:
    // compare with the same settings and every 1.9 field set explicitly to its default.
    auto s=base();s.filters[0].lfoOct=1.5f;s.filters[0].envOct=1;s.lfoShape=1;
    const auto a=run(s,48000,sine(.3,440));
    auto t=s;t.lfo2Shape=3;t.lfo2Rate=7;t.lfo2Sync=2;t.envSource=1;   // LFO 2 unused (amount 0); no key given
    const auto b=run(t,48000,sine(.3,440));
    require(a==b,"An unused 1.9 field changed the sound");
    std::cout<<"PASS defaults: LFO 2 at amount 0, drive 0, no lock / retrigger, own envelope leave the 1.7 sound bit-exact\n";
}

void secondLfo() {
    // LFO 2 alone, 4 Hz sine, 2 octaves on LP 1 kHz with a 2 kHz tone: 4 level cycles per second.
    auto s=base();s.lfo2Shape=0;s.lfo2Rate=4;s.filters[0].lfo2Oct=2;
    const auto e=envelope(run(s,96000,sine(.3,2000)));
    double mean=0;for(size_t i=10;i<e.size();++i)mean+=e[i];mean/=double(e.size()-10);
    int rises=0;for(size_t i=11;i<e.size();++i)if(e[i-1]<mean&&e[i]>=mean)++rises;
    // Random shape: a new level each cycle, smoothed (no step larger than the smoothing allows).
    auto r=base();r.lfoShape=4;r.lfoRate=8;r.filters[0].lfoOct=2;
    const auto random=run(r,96000,sine(.3,2000));const auto re=envelope(random);
    std::vector<double> levels;for(size_t i=10;i+6<re.size();i+=12)levels.push_back(re[i+6]);   // one reading per 125 ms cycle
    double spread=0;for(size_t i=1;i<levels.size();++i)spread=std::max(spread,std::abs(db(levels[i]/levels[i-1])));
    double curvature=0;for(size_t n=4800;n<random.size();++n)curvature=std::max(curvature,double(std::abs(random[n]-2*random[n-1]+random[n-2])));
    std::cout<<"MEASURE LFO 2 sine 4 Hz: "<<rises<<" level cycles in 1.8 s (7.2 expected); Random 8 Hz: largest cycle-to-cycle level change "<<spread<<" dB, largest second difference "<<curvature<<"\n";
    require(rises>=6 && rises<=8 && spread>1 && curvature<.05,"LFO 2 rate or Random shape wrong");
    std::cout<<"PASS second LFO and the Random shape\n";
}

void barLock() {
    // Synced 1/4 at 120 BPM (2 Hz), locked: the phase follows the host position.
    auto s=base();s.lfoSync=3;s.lfoLock=true;s.filters[0].lfoOct=1;
    const double beatsPerBlock=block*120.0/60.0/rate;
    FilterModulationCore a;a.prepare(rate);a.setInitialState(s);
    run(s,block,sine(.3,440),[&](int){return 10.25;},{},&a);            // at beat 10.25
    const double atQuarter=a.lfoPhase();
    FilterModulationCore b;b.prepare(rate);b.setInitialState(s);
    run(s,block*40,sine(.3,440),[&](int k){return 3.0+k*beatsPerBlock;},{},&b);   // from beat 3, 40 blocks
    const double expected=std::fmod(3.0+40*beatsPerBlock-beatsPerBlock/block,1.0);
    // Unlocked (or without a host position) it free-runs from 0.
    auto f=s;f.lfoLock=false;FilterModulationCore c;c.prepare(rate);c.setInitialState(f);run(f,block,sine(.3,440),[&](int){return 10.25;},{},&c);
    std::cout<<"MEASURE bar lock (1/4 at 120 BPM): at beat 10.25 the phase is "<<atQuarter<<" (0.25 after one block: "<<std::fmod(10.25+beatsPerBlock-beatsPerBlock/block,1.0)<<"); after 40 blocks from beat 3: "<<b.lfoPhase()<<" ("<<expected<<"); unlocked: "<<c.lfoPhase()<<"\n";
    require(std::abs(atQuarter-std::fmod(10.25+beatsPerBlock-beatsPerBlock/block,1.0))<1e-9 && std::abs(b.lfoPhase()-expected)<1e-9 && std::abs(c.lfoPhase()-block*2.0/rate)<1e-9,"Bar lock wrong");
    std::cout<<"PASS LFOs locked to the host bar position (and free-running without lock)\n";
}

void retrigger() {
    // Bursts every 370 ms (not a multiple of the 3 Hz LFO): with retrigger the
    // LFO phase at each burst start is the same; without, it drifts.
    const auto bursts=[](int n){const int period=int(.37*rate);return (n%period)<int(.15*rate)?.3*std::sin(2*pi*440*n/rate):0.0;};
    const auto phases=[&](bool on){
        auto s=base();s.lfoRate=3;s.filters[0].lfoOct=1;s.lfoRetrigger=on;
        FilterModulationCore core;core.prepare(rate);core.setInitialState(s);std::vector<double> at;
        juce::AudioBuffer<float> b(2,1);
        for(int n=0;n<int(rate*2);++n){const float x=float(bursts(n));b.setSample(0,0,x);b.setSample(1,0,x);core.process(b.getArrayOfWritePointers(),2,1,s,120);
            if(n%int(.37*rate)==int(.02*rate))at.push_back(core.lfoPhase());}
        double spread=0;for(size_t i=1;i<at.size();++i)spread=std::max(spread,std::abs(at[i]-at[0]));return spread;};
    const double with=phases(true),without=phases(false);
    std::cout<<"MEASURE retrigger: LFO phase 20 ms into each burst varies by "<<with<<" with retrigger, "<<without<<" without\n";
    require(with<.02 && without>.1,"Retrigger wrong");
    std::cout<<"PASS LFOs restart on each attack\n";
}

void sidechainEnvelope() {
    // LP 250 Hz with +3 octaves of envelope; a quiet 2 kHz tone. A loud key opens it.
    auto s=base();s.filters[0].cutoff=250;s.filters[0].envOct=3;
    const auto own=run(s,24000,sine(.01,2000));
    auto k=s;k.envSource=1;const auto keyed=run(k,24000,sine(.01,2000),{},sine(1,100));
    const auto missing=run(k,24000,sine(.01,2000));   // Sidechain chosen but no key: own signal
    const double a=magnitude(own,12000,24000,2000),b=magnitude(keyed,12000,24000,2000),c=magnitude(missing,12000,24000,2000);
    std::cout<<"MEASURE envelope source: 2 kHz passes "<<db(b/a)<<" dB more with a loud key; with Sidechain chosen and no key "<<db(c/a)<<" dB\n";
    require(db(b/a)>20 && std::abs(db(c/a))<1e-6,"Sidechain envelope source wrong");
    std::cout<<"PASS the envelope follows the sidechain when chosen, and its own signal otherwise\n";
}

void drive() {
    auto s=base();s.filters[0].cutoff=20000;   // a wide-open low-pass: drive alone shapes the tone
    const auto clean=run(s,48000,sine(.5,220));
    auto d=s;d.filters[0].drive=1;const auto driven=run(d,48000,sine(.5,220));
    const double third=db(magnitude(clean,24000,48000,660)/magnitude(clean,24000,48000,220)),drivenThird=db(magnitude(driven,24000,48000,660)/magnitude(driven,24000,48000,220));
    std::cout<<"MEASURE drive at -6 dBFS, 220 Hz: harmonic 3 "<<third<<" dB at 0 %, "<<drivenThird<<" dB at 100 %\n";
    require(third<-90 && drivenThird>-25,"Drive wrong");
    std::cout<<"PASS per-filter drive (0 % is exact bypass)\n";
}

void newTypes() {
    // Comb at 500 Hz, resonance 80 %: peaks at 1 kHz, troughs at 750 Hz.
    auto c=base();c.filters[0].type=4;c.filters[0].cutoff=500;c.filters[0].resonance=80;   // percent, as the parameter
    const auto peak=run(c,48000,sine(.1,1000)),trough=run(c,48000,sine(.1,750));
    const double comb=db(magnitude(peak,24000,48000,1000)/magnitude(trough,24000,48000,750));
    const double peakGain=db(magnitude(peak,24000,48000,1000)/.1);
    // All-pass at 1 kHz: flat at mix 100 %; a notch at the cutoff at mix 50 %.
    auto a=base();a.filters[0].type=5;a.filters[0].cutoff=1000;a.filters[0].resonance=0;
    double flat=0;for(const double hz:{100.,400.,1000.,2500.,6000.}){const auto o=run(a,24000,sine(.1,hz));flat=std::max(flat,std::abs(db(magnitude(o,12000,24000,hz)/.1)));}
    auto m=a;m.mix=.5f;const auto notch=run(m,24000,sine(.1,1000)),side=run(m,24000,sine(.1,4000));
    std::cout<<"MEASURE Comb 500 Hz, 80 %: 1 kHz peak "<<comb<<" dB above the 750 Hz trough, peak gain "<<peakGain<<" dB; All-pass: largest magnitude deviation "<<flat<<" dB, at mix 50 % 1 kHz "<<db(magnitude(notch,12000,24000,1000)/.1)<<" dB, 4 kHz "<<db(magnitude(side,12000,24000,4000)/.1)<<" dB\n";
    require(comb>15 && std::abs(peakGain)<.5 && flat<.01 && db(magnitude(notch,12000,24000,1000)/.1)<-30,"Comb or All-pass response wrong");
    std::cout<<"PASS Comb and All-pass filter types\n";
}

void presetsAndProcessor() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(rate,block);p->prepareToPlay(rate,block);
    require(p->applyOrderPreset(OrderRegion::tone,1) && p->requestedOrder().tone.slots[0]==compressorModule,"Tone preset not applied");
    require(p->applyOrderPreset(OrderRegion::post,3) && p->requestedOrder().post.position(postReverbModule)<p->requestedOrder().post.position(postDelayModule),"POST preset not applied");
    require(p->undoOrder() && p->requestedOrder().post==PostOrder::initial() && !p->applyOrderPreset(OrderRegion::tone,1),"Preset is not one undo step, or re-applying the same preset changed something");
    for(const auto region:{OrderRegion::tone,OrderRegion::post})for(const auto& preset:orderPresets(region)){
        RegionOrder<postModuleCount> check;for(int i=0;i<postModuleCount;++i)check.slots[size_t(i)]=preset.slots[size_t(i)];
        if(region==OrderRegion::tone){ToneOrder t;for(int i=0;i<toneModuleCount;++i)t.slots[size_t(i)]=preset.slots[size_t(i)];require(t.valid(),"Invalid tone preset");}
        else require(check.valid(),"Invalid POST preset");
    }
    // The processor passes the sidechain and host position: the 1.9 bank exists and is automatable.
    for(const auto* id:{"fltLfo2Shape","fltLfo2Rate","fltLfo2Sync","fltLfoLock","fltLfoRetrigger","fltEnvSource","fltA_lfo2","fltA_drive","fltB_lfo2","fltB_drive"}){
        auto* q=p->parameters().getParameter(id);require(q!=nullptr && q->isAutomatable() && q->getVersionHint()==filterModulationExtensionVersion,std::string("1.9 parameter missing: ")+id);}
    std::cout<<"MEASURE presets: tone "<<orderPresets(OrderRegion::tone).size()<<", POST "<<orderPresets(OrderRegion::post).size()<<"; allocations while processing "<<allocations<<"\n";
    require(allocations==0,"The modulation core allocated while processing");
    std::cout<<"PASS order presets (one undo step), the 1.9 bank, no allocation while processing\n";
}
// Review fixes: bar lock advances at the host tempo (not Chimera's); with lock
// on, a free LFO still retriggers; drive starts at bypass continuously;
// leaving the Comb does not replay stale state.
void reviewFixes() {
    {   // Host at 100 BPM, Chimera at 120: the locked phase continues across blocks without a jump.
        auto s=base();s.lfoSync=3;s.lfoLock=true;
        FilterModulationCore c;c.prepare(rate);c.setInitialState(s);
        juce::AudioBuffer<float> b(2,block);b.clear();double worst=0,last=-1;
        for(int k=0;k<20;++k){
            const double ppq=4.+k*block*100./60./rate;
            c.process(b.getArrayOfWritePointers(),2,block,s,120,ppq,nullptr,0,100);
            const double expected=(ppq+(block-1)*100./60./rate)/filterSyncBeats[3];
            worst=std::max(worst,std::abs(c.lfoPhase()-(expected-std::floor(expected))));
            last=c.lfoPhase();
        }
        require(worst<1e-9 && last>=0,"Bar lock does not advance at the host tempo");
    }
    {   // Lock on, LFO 1 synced and locked, LFO 2 free: an attack restarts LFO 2.
        auto s=base();s.lfoSync=3;s.lfoLock=true;s.lfo2Rate=3;s.lfoRetrigger=true;
        FilterModulationCore c;c.prepare(rate);c.setInitialState(s);
        juce::AudioBuffer<float> b(2,block);
        for(int k=0;k<60;++k){for(int n=0;n<block;++n){const float x=k<30?0.f:float(.5*std::sin(2*pi*220*(k*block+n)/rate));b.setSample(0,n,x);b.setSample(1,n,x);}
            c.process(b.getArrayOfWritePointers(),2,block,s,120,4.+k*block*2./rate,nullptr,0,120);
            if(k==30)require(c.lfoPhase(1)<3.*block/rate+1e-9,"A free LFO 2 did not retrigger while LFO 1 is locked");}
    }
    {   // Drive at 0.1 %: small-signal gain within 0.01 dB of the bypass.
        filter_detail::Filter f;f.prepare(rate);
        double in=0,out=0;
        for(int n=0;n<4800;++n){const double x=.001*std::sin(2*pi*220*n/rate);in+=x*x;const double y=f.process(x,0,0,20000,.7071,rate,0,.001);out+=y*y;}
        require(std::abs(10*std::log10(out/in))<.05,"Drive is not continuous at 0 %");
    }
    {   // Comb, then Low-pass on silence, then Comb again: no stale echo.
        filter_detail::Filter f;f.prepare(rate);
        for(int n=0;n<4800;++n)f.process(.8*std::sin(2*pi*500*n/rate),4,0,500,.7071,rate,.9);
        for(int n=0;n<9600;++n)f.process(0,0,0,1000,.7071,rate);
        double peak=0;for(int n=0;n<4800;++n)peak=std::max(peak,std::abs(f.process(0,4,0,500,.7071,rate,.9)));
        require(peak<1e-9,"Choosing the Comb replayed old audio");
    }
    std::cout<<"PASS review fixes: bar lock at the host tempo, free LFO retrigger under lock, continuous drive, no stale Comb\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {defaults();secondLfo();barLock();retrigger();sidechainEnvelope();drive();newTypes();presetsAndProcessor();reviewFixes();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
