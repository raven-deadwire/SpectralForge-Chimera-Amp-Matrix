#include "GlobalDSP.h"
#include "Fixtures/Tuner20261013.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>

using Clock=std::chrono::steady_clock;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
double error(double measured,double expected){return measured>0?1200*std::log2(measured/expected):99999;}
float tone(double hz,double time,int kind){
    const double phase=6.283185307179586*hz*time+.73;
    double x=.2*std::sin(phase);
    if(kind)x+=.1*std::sin(2*phase)+.07*std::sin(3*phase);
    if(kind==2)x*=std::exp(-time*3); // common exponential decay, no pitch drift
    return float(x);
}
struct NativeResult {double acquire{-1},settle{-1},cents{99999},pushUs{};};
template<class T> NativeResult native(double rate,int block,double hz){
    T tuner;tuner.prepare(rate);juce::AudioBuffer<float> audio(2,block);NativeResult result;
    const auto start=Clock::now();auto next=start;int offset=0,stable=0;
    const int total=int(rate*.6), step=int(rate*.30);
    for(;offset<total;offset+=block){
        const double wanted=offset>=step?hz*std::exp2(10./1200):hz;
        for(int n=0;n<block;++n){audio.setSample(0,n,tone(wanted,(offset+n)/rate,1));audio.setSample(1,n,-.7f*audio.getSample(0,n));}
        const auto pushStart=Clock::now();tuner.push(audio,true);
        result.pushUs=std::max(result.pushUs,std::chrono::duration<double,std::micro>(Clock::now()-pushStart).count());
        next=start+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>((offset+block)/rate));std::this_thread::sleep_until(next);
        const double elapsed=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        const double e=error(tuner.frequency(),wanted);
        if(offset<step && std::abs(e)<.5 && tuner.confidence()>.9 && result.acquire<0)result.acquire=elapsed;
        if(offset>=step){if(std::abs(e)<.5 && tuner.confidence()>.9)++stable;else stable=0;if(stable>=3 && result.settle<0)result.settle=elapsed-1000*step/rate;result.cents=e;}
    }
    tuner.push(audio,false); // must hide stale pitch immediately in the candidate
    if constexpr(std::is_same_v<T,spectralforge::Tuner>)require(tuner.frequency()==0,"Disabled tuner exposed stale reading");
    tuner.stop();require(tuner.stopped(),"Worker did not stop");return result;
}
void channelContinuity(){
    spectralforge::Tuner tuner;tuner.prepare(48000);juce::AudioBuffer<float> audio(2,256);
    const auto feed=[&](bool switchChannel){
        const auto start=Clock::now();
        for(int block=0;block<120;++block){
            for(int n=0;n<256;++n){const double t=(block*256+n)/48000.;
                audio.setSample(0,n,tone(110,t,0)*(switchChannel?.1f:(block%2?.999f:1.001f)));
                audio.setSample(1,n,tone(switchChannel?220:110,t,0)*(switchChannel?3.f:(block%2?1.001f:.999f)));}
            tuner.push(audio,true);
            if(switchChannel && block==0)require(tuner.frequency()==0,"True channel switch retained the previous pitch");
            std::this_thread::sleep_until(start+std::chrono::microseconds((block+1)*5333));
        }
    };
    feed(false);require(std::abs(error(tuner.frequency(),110))<.5,"Tiny stereo balance changes prevent pitch acquisition");
    feed(true);require(std::abs(error(tuner.frequency(),220))<.5,"Dominant replacement channel did not acquire");
    tuner.stop();std::cerr<<"PASS stereo balance continuity and true channel-switch invalidation/reacquisition\n";
}
void lifecycle(){
    spectralforge::Tuner tuner;tuner.prepare(48000);
    juce::AudioBuffer<float> audio(2,256);
    const auto feed=[&]{
        const auto start=Clock::now();
        for(int block=0;block<60;++block){
            for(int n=0;n<256;++n)for(int c=0;c<2;++c)audio.setSample(c,n,tone(110,(block*256+n)/48000.,1));
            tuner.push(audio,true);
            std::this_thread::sleep_until(start+std::chrono::microseconds((block+1)*5333));
        }
        require(std::abs(error(tuner.frequency(),110))<.1,"Lifecycle tone did not acquire");
    };
    feed();tuner.push(audio,false);tuner.push(audio,true);
    require(tuner.frequency()==0,"Rapid re-enable leaked previous reading");
    feed();std::this_thread::sleep_for(std::chrono::milliseconds(280));
    require(tuner.frequency()==0,"Stopped stream left a stationary stale strobe");
    feed();audio.setSample(0,0,std::numeric_limits<float>::quiet_NaN());
    tuner.push(audio,true);require(tuner.frequency()==0,"Nonfinite input retained old pitch");
    feed();tuner.prepare(96000);require(tuner.frequency()==0,"Sample-rate change retained old pitch");
    tuner.stop();require(tuner.stopped(),"Lifecycle shutdown failed");
}
void stalledWorkerFreshness(){
#if CHIMERA_TUNER_TEST_HOOKS
    spectralforge::Tuner tuner;tuner.prepare(48000);juce::AudioBuffer<float> audio(2,256);
    const auto start=Clock::now();
    for(int block=0;block<70;++block){
        for(int n=0;n<256;++n)for(int c=0;c<2;++c)audio.setSample(c,n,tone(110,(block*256+n)/48000.,1));
        tuner.push(audio,true);
        std::this_thread::sleep_until(start+std::chrono::microseconds((block+1)*5333));
    }
    require(std::abs(error(tuner.frequency(),110))<.1,"Stall fixture did not acquire");
    tuner.pauseWorkerForTest(450);
    const auto requested=Clock::now();
    while(!tuner.workerPausedForTest() && Clock::now()-requested<std::chrono::milliseconds(250))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    require(tuner.workerPausedForTest(),"Stall hook did not reach the worker");
    const auto pausedIteration=tuner.completedWorkerIterationsForTest();
    // Leave a coherent final block in the queue, then stop the source. A
    // delayed consumer must not give these old samples a fresh timestamp.
    for(int n=0;n<256;++n)for(int c=0;c<2;++c)audio.setSample(c,n,tone(110,(70*256+n)/48000.,1));
    tuner.push(audio,true);
    std::this_thread::sleep_for(std::chrono::milliseconds(280));
    require(tuner.workerPausedForTest(),"Worker resumed before stale observation check");
    require(tuner.frequency()==0 && tuner.confidence()==0,"Paused worker exposed an expired pitch/confidence");
    const auto expired=Clock::now();
    while(tuner.completedWorkerIterationsForTest()==pausedIteration && Clock::now()-expired<std::chrono::milliseconds(500))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    require(tuner.completedWorkerIterationsForTest()!=pausedIteration,"Worker did not resume after the pause");
    require(tuner.frequency()==0 && tuner.confidence()==0,"Expired queued input was republished after the worker resumed");
    tuner.stop();require(tuner.stopped(),"Paused worker could not stop");
    std::cerr<<"PASS expired observation hidden during worker stall and after queued input resumes\n";
#endif
}
int main(int argc,char** argv){try{
    if(argc==2 && std::string(argv[1])=="--stale-only"){stalledWorkerFreshness();return 0;}
    std::cout<<std::setprecision(10)<<std::unitbuf;
    std::cout<<"kind,engine,rate,block,hz,fixture,error_ct,confidence,compute_us,acquire_ms,step_settle_ms,push_max_us\n";
    int count=0;double worst[3]{},baselineWorst[3]{};
    for(double host:{44100.,48000.,96000.,192000.})for(double hz:{16.352,23.125,27.5,30.867706,41.203445,82.406889,130.81278,440.,1318.5102})for(int kind=0;kind<3;++kind){
        const double wanted=hz*std::exp2(.07/1200);
        const double rate=host/std::max(1,int(std::round(host/24000)));
        const double oldRate=host/std::max(1,int(std::round(host/12000)));
        std::array<float,4096> samples{};std::array<float,2048> previous{};
        for(int n=0;n<4096;++n)samples[size_t(n)]=tone(wanted,n/rate,kind);
        for(int n=0;n<2048;++n)previous[size_t(n)]=tone(wanted,n/oldRate,kind);
        auto before=Clock::now();const auto candidate=spectralforge::Tuner::estimate(samples.data(),4096,rate);
        const auto time=std::chrono::duration<double,std::micro>(Clock::now()-before).count();
        const auto old=frozenTuner20261013::Tuner::estimate(previous.data(),2048,oldRate);
        const auto ce=error(candidate.first,wanted),be=error(old.first,wanted);
        std::cout<<"kernel,new,"<<host<<",0,"<<wanted<<','<<kind<<','<<ce<<','<<candidate.second<<','<<time<<",-1,-1,-1\n";
        std::cout<<"kernel,baseline,"<<host<<",0,"<<wanted<<','<<kind<<','<<be<<','<<old.second<<",0,-1,-1,-1\n";
        worst[kind]=std::max(worst[kind],std::abs(ce));baselineWorst[kind]=std::max(baselineWorst[kind],std::abs(be));++count;
        require(candidate.second>.9f&&std::abs(ce)<.1,"Synthetic candidate error exceeds 0.1 cent");
    }
    int failedRoutes=0;
    for(double rate:{44100.,48000.,96000.,192000.})for(int block:{64,256})for(double hz:{27.5,30.867706,82.406889,440.}){
        const auto a=native<frozenTuner20261013::Tuner>(rate,block,hz),b=native<spectralforge::Tuner>(rate,block,hz);
        for(const auto& pair:{std::make_pair("baseline",a),std::make_pair("new",b)}){const auto& r=pair.second;std::cout<<"worker,"<<pair.first<<','<<rate<<','<<block<<','<<hz<<",1,"<<r.cents<<",0,0,"<<r.acquire<<','<<r.settle<<','<<r.pushUs<<'\n';}
        if(!(b.acquire>0 && b.acquire<290 && b.settle>0 && b.settle<290 && std::abs(b.cents)<.1)){
            ++failedRoutes;std::cerr<<"ROUTE FAIL rate="<<rate<<" block="<<block<<" hz="<<hz<<" acquire="<<b.acquire<<" settle="<<b.settle<<" error="<<b.cents<<'\n';
        }
    }
    channelContinuity();lifecycle();stalledWorkerFreshness();
    require(failedRoutes==0,"Native acquisition/settle/accuracy gate failed; all route rows retained");
    std::cerr<<"PASS "<<count<<" kernel cases and 32 native A/B routes. Synthetic sine/harmonic/exponential-decay maxima: "<<worst[0]<<' '<<worst[1]<<' '<<worst[2]<<" ct. Lifecycle re-enable/stale/nonfinite/rate-change passed. Host clock/DI/GUI latency not included.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
