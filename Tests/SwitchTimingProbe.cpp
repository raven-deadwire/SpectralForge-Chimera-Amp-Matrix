#include <juce_cryptography/juce_cryptography.h>
#include "PluginProcessor.h"
#include "AmpNativeCatalog.h"
#include "AmpNativeParameters.h"
#include "FactoryPresets.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>
#include <ctime>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Switch timing probe: how a switch sounds and costs on the real processor.
// For each scenario three renders share one fixture: the old sound held
// throughout (o), the switch at t0 (y), and the new sound from the start (r).
// Metrics (definitions in docs/SWITCH_TIMING_METRICS.md):
//   response_ms  first 1 ms window after t0 where y has moved a tenth (-20 dB)
//                of the way from o toward r
//   settle_ms    end of the last 1 ms window where y still differs from r
//   residual_db  y - r energy in 5 ms windows at +20/50/100/250 ms
//   click_db     y's sharpest 1 ms (second-difference energy) from t0 - 2 ms
//                to settle + 5 ms, over the sharpest 1 ms anywhere in o or r;
//                above 0 dB the switch is sharper than any note (click_at_ms: where)
//   overshoot_db worst 5 ms window where y is louder than both o and r
//   gap_ms       longest silence in y after t0 while r plays (a recall's mute)
//   cpu          processBlock wall time: steady p50/p99 before t0, worst
//                block from t0 to settle, as a share of the block budget
// Time effects (delay, reverb, chorus) are off so tails do not hide the switch.
struct ChimeraRecallTestAccess {
    static void policy(ChimeraProcessor& p,spectralforge::IRLibrary::PreparationPolicy value,bool instant=true) {p.library.preparationPolicy=value;p.instantRecallCabinetSwap=instant;}
    static unsigned cacheHits(ChimeraProcessor& p) {return p.library.kernelCacheHits();}
};
namespace {
double processCpuMilliseconds() {
#if JUCE_WINDOWS
    FILETIME created{},exited{},kernel{},user{};
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))return 0.;
    const auto ticks=[](FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|uint64_t(t.dwLowDateTime);};
    return double(ticks(kernel)+ticks(user))*.0001;
#else
    return 1000.*double(std::clock())/CLOCKS_PER_SEC;
#endif
}
using Policy=spectralforge::IRLibrary::PreparationPolicy;
Policy policy=Policy::audibleCached;
bool warmAB=false,warmRequested=false,instantSwap=true;
void require(bool ok,const juce::String& message) {if(!ok)throw std::runtime_error(message.toStdString());}
void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* q=p.parameters().getParameter(id);require(q!=nullptr,"Missing parameter "+id);q->setValueNotifyingHost(q->convertTo0to1(value));
}
float get(ChimeraProcessor& p,const juce::String& id) {return p.parameters().getRawParameterValue(id)->load();}

int block=256;double rate=48000;
bool recallsOnly=false;
constexpr double preRoll=.6,horizon=1.;
constexpr double thresholdDb=-40;
// Response gates: a crossfaded switch (channel, amp model) within its 20 ms
// crossfade; a recall from the UI thread (preset, A/B) within 60 ms, which
// includes that thread's scheduling and building the recalled state.
constexpr double crossfadeResponseMs=20,recallResponseMs=60;

float fixture(int n) {
    const int period=int(rate*.16),local=n%period;const double t=double(local)/rate;
    const double accent=(n/period)%2==0 ? .35 : .18,envelope=accent*std::exp(-t*9);
    double x=0;for(int h=1;h<=6;++h)x+=std::sin(juce::MathConstants<double>::twoPi*55*h*t)/h;
    return float(envelope*x);
}

struct Render {std::vector<float> audio;std::vector<double> blockMs,completionMs;unsigned cacheHits{};double actionMs{},processCpuMs{},wallMs{};};
using Setup=std::function<void(ChimeraProcessor&)>;
// setup: before prepare; change: at the block boundary nearest t0 (null for a
// held sound). messageThread: the change runs on another thread while blocks
// keep coming at real-time pace, as from a host's UI (presets, A/B); otherwise
// it runs between blocks on the audio thread, as host automation does.
Render render(const Setup& setup,const Setup& change,bool messageThread=false) {
    auto p=std::make_unique<ChimeraProcessor>();ChimeraRecallTestAccess::policy(*p,policy,instantSwap);setup(*p);
    for(const auto* id:{"delayon","reverbon","choruson"})set(*p,id,0);
    p->prepareToPlay(rate,block);
    if(warmAB) {
        // Warm BOTH saved A/B states through the real processor/library, then
        // return to A and clear DSP histories without invalidating prepared IRs.
        juce::AudioBuffer<float> warm(2,block);juce::MidiBuffer empty;
        const int selected=p->comparisonSlot();
        for(int slot:{1-selected,selected}) {
            p->selectComparison(slot);
            const auto start=std::chrono::steady_clock::now();
            for(int k=0;k<int(rate/block);++k) {
                warm.clear();p->processBlock(warm,empty);
                std::this_thread::sleep_until(start+std::chrono::microseconds(int64_t(1e6*(k+1)*block/rate)));
            }
        }
        p->reset();
    }
    const int blocks=int((preRoll+horizon)*rate/block),switchBlock=int(preRoll*rate/block);
    juce::AudioBuffer<float> b(2,block);juce::MidiBuffer midi;Render r;
    std::thread ui;unsigned hitsAtSwitch{};
    const auto performChange=[&]{change(*p);for(const auto* id:{"delayon","reverbon","choruson"})set(*p,id,0);};
    auto paceFrom=std::chrono::steady_clock::now();
    auto renderStart=paceFrom;auto cpuStart=processCpuMilliseconds();
    for(int k=0;k<blocks;++k) {
        if(k==switchBlock){hitsAtSwitch=ChimeraRecallTestAccess::cacheHits(*p);renderStart=std::chrono::steady_clock::now();cpuStart=processCpuMilliseconds();}
        if(k==switchBlock && change) {
            if(messageThread){paceFrom=std::chrono::steady_clock::now();ui=std::thread([&]{const auto start=std::chrono::steady_clock::now();performChange();r.actionMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();});}
            else performChange();
        }
        if(messageThread && k>=switchBlock) // Pace the ENTIRE recall, including background CAB preparation.
            std::this_thread::sleep_until(paceFrom+std::chrono::microseconds(int64_t(1e6*(k-switchBlock)*block/rate)));
        for(int i=0;i<block;++i){const float x=fixture(k*block+i);b.setSample(0,i,x);b.setSample(1,i,x);}
        const auto start=std::chrono::steady_clock::now();
        p->processBlock(b,midi);
        const auto completed=std::chrono::steady_clock::now();
        r.blockMs.push_back(std::chrono::duration<double,std::milli>(completed-start).count());
        r.completionMs.push_back(std::chrono::duration<double,std::milli>(completed-renderStart).count());
        for(int i=0;i<block;++i){const float l=b.getSample(0,i),rr=b.getSample(1,i);require(std::isfinite(l)&&std::isfinite(rr),"Non-finite output");r.audio.push_back(.5f*(l+rr));}
    }
    if(ui.joinable())ui.join();
    r.processCpuMs=processCpuMilliseconds()-cpuStart;
    r.wallMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-renderStart).count();
    r.cacheHits=ChimeraRecallTestAccess::cacheHits(*p)-hitsAtSwitch;
    p->releaseResources();
    return r;
}

struct Metrics {double responseMs{-1},settleMs{-1},clickDb{},clickAtMs{},overshootDb{},gapMs{},cpuP50{},cpuP99{},cpuSwitch{},budgetMs{};std::array<double,4> residualDb{};bool settled{};double audibleMs{-1},recoveryMs{-1},responseWallMs{-1},audibleWallMs{-1},recoveryWallMs{-1},actionMs{},cpuPercent{},cpuPostP99{};int overruns{};unsigned cacheHits{};};
double energy(const std::vector<float>& a,const std::vector<float>* b,size_t from,size_t n) {
    double e=0;for(size_t i=from;i<std::min(a.size(),from+n);++i){const double d=a[i]-(b?(*b)[i]:0.f);e+=d*d;}return e;
}
double d2energy(const std::vector<float>& a,size_t from,size_t n) {
    double e=0;for(size_t i=std::max<size_t>(from,2);i<std::min(a.size(),from+n);++i){const double d=a[i]-2.*a[i-1]+a[i-2];e+=d*d;}return e;
}
double percentile(std::vector<double> v,double q){std::sort(v.begin(),v.end());return v.empty()?0:v[size_t(q*double(v.size()-1))];}

Metrics measure(const Render& o,const Render& y,const Render& r) {
    Metrics m;
    const size_t t0=size_t(int(preRoll*rate/block))*size_t(block),ms=size_t(rate/1000),total=y.audio.size();
    const double post=std::max(energy(r.audio,nullptr,t0,total-t0)/double(total-t0),1e-20);
    const double windowThreshold=post*double(ms)*std::pow(10.,thresholdDb/10);
    // Response: y has moved a tenth of the way (-20 dB) from o toward r, in a
    // window where the two sounds differ at all. Relative, so a switch between
    // nearly identical channels is not reported late.
    for(size_t s=t0;s+ms<=total;s+=ms) {
        const double apart=energy(r.audio,&o.audio,s,ms);
        if(apart>windowThreshold && energy(y.audio,&o.audio,s,ms)>=.01*apart){m.responseMs=double(s-t0)/double(ms);break;}
    }
    // Acoustic recovery is independent of the first change (which can be a mute).
    // Require 5 consecutive 1 ms windows within +/-3 dB of the destination's
    // RMS. This is level recovery, not waveform convergence or listening approval.
    int recovered=0;bool lostLevel=false;
    for(size_t s=t0;s+ms<=total;s+=ms) {
        const double dest=energy(r.audio,nullptr,s,ms),actual=energy(y.audio,nullptr,s,ms);
        if(actual<post*double(ms)*1e-6)lostLevel=true;
        if(lostLevel && dest>windowThreshold && actual>post*double(ms)*1e-6 && m.audibleMs<0)
            m.audibleMs=double(s-t0)/double(ms);
        if(lostLevel && dest>windowThreshold && actual>=dest*.5 && actual<=dest*2.)++recovered;else recovered=0;
        if(recovered==5 && m.recoveryMs<0)m.recoveryMs=double(s-t0)/double(ms)-4.;
    }
    size_t last=t0;bool any=false;
    for(size_t s=t0;s+ms<=total;s+=ms)if(energy(y.audio,&r.audio,s,ms)>windowThreshold){last=s+ms;any=true;}
    m.settleMs=any?double(last-t0)/double(ms):0.;m.settled=last+20*ms<total;
    const std::array<double,4> at{20,50,100,250};
    for(size_t i=0;i<at.size();++i) {
        const size_t s=t0+size_t(at[i])*ms;
        m.residualDb[i]=10*std::log10(energy(y.audio,&r.audio,s,5*ms)/(post*5*double(ms))+1e-30);
    }
    // Click: y's sharpest 1 ms (second-difference energy) during the switch
    // against the sharpest 1 ms anywhere in o or r, i.e. the hardest pick
    // attack either sound produces on its own. Positive: the switch is sharper
    // than any note. A louder new passage is reported as overshoot instead.
    double sharpest=1e-30;
    for(size_t s=ms*20;s+ms<=total;s+=ms)sharpest=std::max({sharpest,d2energy(o.audio,s,ms),d2energy(r.audio,s,ms)});
    m.clickDb=-300;
    for(size_t s=t0-2*ms;s+ms<=std::min(total,last+5*ms);s+=ms) {
        const double db=10*std::log10((d2energy(y.audio,s,ms)+1e-30)/sharpest);
        if(db>m.clickDb){m.clickDb=db;m.clickAtMs=(double(s)-double(t0))/double(ms);}
    }
    // Gap: the longest run after t0 where y is silent (below -60 dB re post)
    // while r is not, i.e. how long a recall's mute lasts.
    {
        const double silent=post*double(ms)*1e-6;double run=0;
        for(size_t s=t0;s+ms<=total;s+=ms) {
            if(energy(y.audio,nullptr,s,ms)<silent && energy(r.audio,nullptr,s,ms)>=silent){run+=1;m.gapMs=std::max(m.gapMs,run);}
            else run=0;
        }
    }
    // Overshoot: y louder than both the old and the new sound, in 5 ms windows.
    m.overshootDb=-300;
    for(size_t s=t0;s+5*ms<=total;s+=5*ms)
        m.overshootDb=std::max(m.overshootDb,10*std::log10((energy(y.audio,nullptr,s,5*ms)+1e-30)/(std::max(energy(o.audio,nullptr,s,5*ms),energy(r.audio,nullptr,s,5*ms))+1e-30)));
    const size_t switchBlock=size_t(preRoll*rate/block);
    std::vector<double> steady(y.blockMs.begin()+20,y.blockMs.begin()+long(switchBlock));
    m.cpuP50=percentile(steady,.5);m.cpuP99=percentile(steady,.99);
    const size_t settleBlocks=size_t(std::ceil(m.settleMs*rate/1000/block))+1;
    for(size_t k=switchBlock;k<std::min(y.blockMs.size(),switchBlock+settleBlocks);++k)m.cpuSwitch=std::max(m.cpuSwitch,y.blockMs[k]);
    m.budgetMs=1000.*block/rate;
    m.cacheHits=y.cacheHits;m.actionMs=y.actionMs;m.cpuPercent=100.*y.processCpuMs/std::max(1.,y.wallMs);
    std::vector<double> postTimes(y.blockMs.begin()+long(switchBlock),y.blockMs.end());
    m.cpuPostP99=percentile(postTimes,.99);
    for(auto v:postTimes)if(v>m.budgetMs)++m.overruns;
    // Availability of the buffer carrying each event, measured on the wall
    // clock. When callbacks overrun, sample time is NOT elapsed host time.
    // This still is not a physical device/acoustic propagation measurement.
    const auto availableAt=[&](double sampleMs) {
        if(sampleMs<0)return -1.;
        const size_t k=(t0+size_t(sampleMs*rate/1000.))/size_t(block);
        return k<y.completionMs.size()?y.completionMs[k]:-1.;
    };
    m.responseWallMs=availableAt(m.responseMs);m.audibleWallMs=availableAt(m.audibleMs);
    m.recoveryWallMs=availableAt(m.recoveryMs);
    return m;
}

struct Result {juce::String kind,name;Metrics m;bool recall{};};
std::vector<Result> results;
void report(const Result& result) {
    const auto& m=result.m;
    std::cout<<"SWITCH kind="<<result.kind.toStdString()<<" name=\""<<result.name.toStdString()<<"\" response_ms="<<m.responseMs<<" settle_ms="<<m.settleMs<<(m.settled?"":"+")
             <<" residual_db@20/50/100/250="<<juce::String(m.residualDb[0],1)<<"/"<<juce::String(m.residualDb[1],1)<<"/"<<juce::String(m.residualDb[2],1)<<"/"<<juce::String(m.residualDb[3],1)
             <<" click_db="<<juce::String(m.clickDb,1)<<" click_at_ms="<<m.clickAtMs<<" overshoot_db="<<juce::String(m.overshootDb,1)<<" gap_ms="<<m.gapMs<<" cpu_ms p50="<<juce::String(m.cpuP50,3)<<" p99="<<juce::String(m.cpuP99,3)<<" switch="<<juce::String(m.cpuSwitch,3)
             <<" budget="<<juce::String(m.budgetMs,3)<<'\n';
}
void scenario(const juce::String& kind,const juce::String& name,const Setup& base,const Setup& change,bool messageThread=false) {
    const auto o=render(base,{});
    const auto y=render(base,change,messageThread);
    const auto r=render([&](ChimeraProcessor& p){base(p);change(p);},{});
    double eo=0,er=0;for(size_t i=0;i<o.audio.size();++i){const double d=o.audio[i]-r.audio[i];eo+=d*d;er+=double(r.audio[i])*r.audio[i];}
    if(eo<er*1e-6){std::cout<<"SKIP "<<kind<<" "<<name<<": old and new sound alike\n";return;}
    results.push_back({kind,name,measure(o,y,r),messageThread});report(results.back());
}

void scenarios() {
    using namespace spectralforge;
    if(!recallsOnly) {
    // Native amp channels: every factory preset's first native amp whose panel has channels.
    std::vector<int> seen;
    for(int preset=0;preset<factoryPresetCount && seen.size()<6;++preset) {
        auto probe=std::make_unique<ChimeraProcessor>();probe->loadFactoryPreset(preset);
        if(get(*probe,ampNativeEnabledID(0))<.5f)continue;
        const int model=int(get(*probe,ampNativeModelID(0)));const auto& panel=ampNativePanel(model);
        if(panel.channels.size()<2 || std::find(seen.begin(),seen.end(),model)!=seen.end())continue;
        seen.push_back(model);
        const int from=int(get(*probe,ampNativeChannelID(0,model))),to=(from+1)%int(panel.channels.size());
        // Model numbers, not the catalogue's reference names: logs and evidence stay
        // within the public naming policy (docs/PUBLIC_PRODUCT_NAMING.md).
        scenario("channel","model "+juce::String(model)+" "+juce::String::fromUTF8(panel.channels[size_t(from)])+" > "+juce::String::fromUTF8(panel.channels[size_t(to)]),
                 [=](ChimeraProcessor& p){p.loadFactoryPreset(preset);},
                 [=](ChimeraProcessor& p){set(p,ampNativeChannelID(0,model),float(to));});
    }
    // Native amp model, factory preset, A/B.
    {
        auto probe=std::make_unique<ChimeraProcessor>();probe->loadFactoryPreset(0);
        const int model=int(get(*probe,ampNativeModelID(0))),other=seen.empty()||seen.front()==model?(model+1)%ampModelCount:seen.front();
        scenario("amp-model","preset 0 model "+juce::String(model)+" > "+juce::String(other),
                 [](ChimeraProcessor& p){p.loadFactoryPreset(0);},[=](ChimeraProcessor& p){set(p,ampNativeModelID(0),float(other));});
    }
    }
    for(const auto& [a,b]:std::vector<std::pair<int,int>>{{0,factoryPresetCount/2},{factoryPresetCount/2,factoryPresetCount-1}})
        scenario("preset","preset "+juce::String(a)+" > "+juce::String(b),[=](ChimeraProcessor& p){p.loadFactoryPreset(a);},[=](ChimeraProcessor& p){p.loadFactoryPreset(b);},true);
    // A/B: B holds another preset. The new-sound reference is slot B itself,
    // selected before playback: a factory recall keeps session settings such
    // as input gain, so "that preset loaded fresh" is not the same sound.
    for(bool soloThird:{false,true}) {
        const int other=factoryPresetCount-1;
        const Setup base=[=](ChimeraProcessor& p){p.loadFactoryPreset(0);p.selectComparison(1);p.loadFactoryPreset(other);
            if(soloThird){set(p,"mode",2);set(p,"solo1",0);set(p,"solo2",0);set(p,"solo3",1);set(p,"cab3",1);}
            for(const auto* id:{"delayon","reverbon","choruson"})set(p,id,0);p.selectComparison(0);};
        warmAB=warmRequested;
        const auto o=render(base,{}),y=render(base,[](ChimeraProcessor& p){p.selectComparison(1);},true);
        const auto r=render([=](ChimeraProcessor& p){base(p);p.selectComparison(1);},{});
        results.push_back({"ab",soloThird ? "A > B lane 3 solo" : "A > B",measure(o,y,r),true});report(results.back());
    }
}

void writeCsv(const juce::File& file) {
    juce::String csv="kind,name,block,rate,response_ms,settle_ms,settled,residual_20_db,residual_50_db,residual_100_db,residual_250_db,click_db,click_at_ms,overshoot_db,gap_ms,cpu_p50_ms,cpu_p99_ms,cpu_switch_ms,budget_ms,audible_ms,recovery_ms,action_ms,process_cpu_percent,post_p99_ms,overruns,cache_hits,response_ready_wall_ms,audible_ready_wall_ms,recovery_ready_wall_ms\n";
    for(const auto& r:results) {
        const auto& m=r.m;
        csv<<r.kind<<",\""<<r.name<<"\","<<block<<","<<rate<<","<<m.responseMs<<","<<m.settleMs<<","<<(m.settled?1:0);
        for(const auto v:m.residualDb)csv<<","<<v;
        csv<<","<<m.clickDb<<","<<m.clickAtMs<<","<<m.overshootDb<<","<<m.gapMs<<","<<m.cpuP50<<","<<m.cpuP99<<","<<m.cpuSwitch<<","<<m.budgetMs<<","<<m.audibleMs<<","<<m.recoveryMs<<","<<m.actionMs<<","<<m.cpuPercent<<","<<m.cpuPostP99<<","<<m.overruns<<","<<int(m.cacheHits)<<","<<m.responseWallMs<<","<<m.audibleWallMs<<","<<m.recoveryWallMs<<"\n";
    }
    require(file.replaceWithText(csv),"Cannot write "+file.getFullPathName());
}
}

int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try {
        juce::File out;
        for(int i=1;i<argc;++i) {
            const juce::String a(argv[i]);
            if(a=="--block" && i+1<argc)block=juce::String(argv[++i]).getIntValue();
            else if(a=="--rate" && i+1<argc)rate=juce::String(argv[++i]).getDoubleValue();
            else if(a=="--policy" && i+1<argc) {
                const juce::String value(argv[++i]);
                require(value=="baseline" || value=="serial" || value=="priority" || value=="parallel" || value=="cache","Unknown policy");
                instantSwap=value!="baseline";
                policy=(value=="serial" || value=="baseline") ? Policy::serial : value=="priority" ? Policy::audibleFirst : value=="parallel" ? Policy::parallel : Policy::audibleCached;
            }
            else if(a=="--warm-ab")warmRequested=true;
            else if(a=="--recalls-only")recallsOnly=true;
            else out=juce::File(a);
        }
        require(block>=16 && rate>=22050,"Bad --block/--rate");
        std::cout<<"SWITCH_PROTOCOL rate="<<rate<<" block="<<block<<" preroll_s="<<preRoll<<" horizon_s="<<horizon<<" threshold_db="<<thresholdDb<<" time_effects=off\n";
        scenarios();
        require(!results.empty(),"No switch scenario ran");
        if(out!=juce::File())writeCsv(out); // also when a gate fails: it is the evidence
        // The gate that holds on any machine: every switch begins changing within its
        // response limit. Settle, residual, click and CPU are reported for review, not
        // gated: a modulated channel (vibrato, tremolo) never matches a reference
        // whose LFO started earlier, and wall time depends on the runner.
        for(const auto& r:results) {
            const double limit=r.recall?recallResponseMs:crossfadeResponseMs;
            require(r.m.responseMs>=0 && r.m.responseMs<=limit,r.kind+" \""+r.name+"\" did not respond within "+juce::String(limit)+" ms");
            if(r.recall) {
                require(r.m.audibleMs>=0 && r.m.recoveryMs>=0,"Recall never recovered audible destination level in the measured horizon");
                require(r.m.overshootDb<=1.0,"Recall exceeded the predeclared 1 dB overshoot regression limit");
            }
        }
        std::cout<<"PASS switch timing probe: "<<results.size()<<" switches measured\n";
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    return 0;
}
