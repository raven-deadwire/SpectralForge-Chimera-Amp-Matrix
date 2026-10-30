#include "GlobalDSP.h"
#include "Fixtures/Tuner20261013.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#if JUCE_MAC
#include <pthread.h>
#include <pthread/qos.h>
#endif

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
bool normalQoSReference=false;
// Only the synthetic input producer is scoped here. Construct after prepare()
// so the detector thread cannot inherit this requested QoS. Both A/B engines
// receive the same producer scheduling; deadlines and gates remain unchanged.
class ScopedFeederQoS {
public:
    ScopedFeederQoS() {
#if JUCE_MAC
        require(pthread_get_qos_class_np(pthread_self(),&previous,&relative)==0, "Cannot inspect feeder QoS");
        require(previous!=QOS_CLASS_UNSPECIFIED,"Feeder QoS cannot be restored exactly");
        require(pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE,0)==0,
                "Cannot request feeder QoS");
        active=true;
#endif
    }
    ~ScopedFeederQoS() {
#if JUCE_MAC
        if(active && pthread_set_qos_class_self_np(previous,relative)!=0)std::terminate();
#endif
    }
    void restore() {
#if JUCE_MAC
        require(pthread_set_qos_class_self_np(previous,relative)==0,"Cannot restore feeder QoS");
        active=false;
        qos_class_t restored{};int restoredRelative{};
        require(pthread_get_qos_class_np(pthread_self(),&restored,&restoredRelative)==0, "Cannot inspect restored feeder QoS");
        require(restored==previous && restoredRelative==relative,"Feeder QoS restoration mismatch");
#endif
    }
    ScopedFeederQoS(const ScopedFeederQoS&)=delete;
    ScopedFeederQoS& operator=(const ScopedFeederQoS&)=delete;
private:
#if JUCE_MAC
    qos_class_t previous{};int relative{};bool active{};
#endif
};
// Buffer route evidence until every timed A/B route has stopped. std::cerr is
// unit-buffered: streaming thousands of fields between routes lets the CI log
// consumer compete with the next feeder/worker. Keep all records and clocks;
// this removes diagnostic I/O interference, not OS scheduling from the gates.
// The compact per-route summary is written last so it survives log tails.
class DeferredRouteDiagnostics {
public:
    std::ostringstream stream, rows, summary;
    ~DeferredRouteDiagnostics() { std::cout << rows.str(); std::cerr << stream.str() << summary.str(); }
};
// Route gates are judged in audio time: the duration of input fed when the
// deciding reading was taken (the read's nominal deadline), not the wall clock
// at which a late-woken feeder got to read it. The feeder's lateness (wall read
// minus that deadline) is recorded per read and judged separately: an attempt
// whose feeder ran later than kFeederLateLimitMs at any read is not a valid
// real-time measurement and is repeated, never passed. Tuner worker wake, poll
// and compute delay still elapse while input is fed in real time, so they stay
// in the audio-time result. See docs/TUNER_TIMING_ATTRIBUTION.md.
constexpr double kFeederLateLimitMs=50;
constexpr int kMaxRouteAttempts=3;
struct NativeResult {double acquire{-1},settle{-1},cents{99999},pushUs{},
    wallAcquire{-1},wallSettle{-1},acquireLate{-1},settleLate{-1},lateMax{},lateP99{};int feeds{};
    bool feederValid() const {return lateMax<=kFeederLateLimitMs;}};
template<class T> NativeResult native(double rate,int block,double hz,std::ostream& diagnostics){
    T tuner;
#if CHIMERA_TUNER_TEST_HOOKS
    if constexpr(std::is_same_v<T,spectralforge::Tuner>)tuner.useNormalQoSForTest(normalQoSReference);
#endif
    tuner.prepare(rate);juce::AudioBuffer<float> audio(2,block);NativeResult result;
    ScopedFeederQoS feederQoS;
    const double timingEpoch=juce::Time::getMillisecondCounterHiRes();
    const auto start=Clock::now();auto next=start;int offset=0,stable=0;
    const int total=int(rate*.6), step=int(rate*.30);
    struct FeedObservation {int offset;double push,observed;float hz,confidence;double sleepBegin,sleepEnd,readEnd;};
    std::vector<FeedObservation> feed;feed.reserve(size_t((total+block-1)/block));
    std::vector<double> lateness;lateness.reserve(feed.capacity());
    for(;offset<total;offset+=block){
        const double wanted=offset>=step?hz*std::exp2(10./1200):hz;
        for(int n=0;n<block;++n){audio.setSample(0,n,tone(wanted,(offset+n)/rate,1));audio.setSample(1,n,-.7f*audio.getSample(0,n));}
        const auto pushStart=Clock::now();tuner.push(audio,true);
        result.pushUs=std::max(result.pushUs,std::chrono::duration<double,std::micro>(Clock::now()-pushStart).count());
        next=start+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>((offset+block)/rate));
        const auto sleepBegin=Clock::now();
        std::this_thread::sleep_until(next);
        const auto sleepEnd=Clock::now();
        const double elapsed=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        const auto measured=tuner.frequency();const auto confidence=tuner.confidence();
        const auto readEnd=Clock::now();
        const double e=error(measured,wanted);
        feed.push_back({offset,std::chrono::duration<double,std::milli>(pushStart-start).count(),elapsed,measured,confidence,
            std::chrono::duration<double,std::milli>(sleepBegin-start).count(),
            std::chrono::duration<double,std::milli>(sleepEnd-start).count(),
            std::chrono::duration<double,std::milli>(readEnd-start).count()});
        const double fed=1000.*(offset+block)/rate,late=std::max(0.,elapsed-fed);
        lateness.push_back(late);
        if(offset<step && std::abs(e)<.5 && confidence>.9 && result.acquire<0){result.acquire=fed;result.wallAcquire=elapsed;result.acquireLate=late;}
        if(offset>=step){if(std::abs(e)<.5 && confidence>.9)++stable;else stable=0;
            if(stable>=3 && result.settle<0){result.settle=fed-1000*step/rate;result.wallSettle=elapsed-1000*step/rate;result.settleLate=late;}
            result.cents=e;}
    }
    feederQoS.restore();
    std::sort(lateness.begin(),lateness.end());
    result.feeds=int(lateness.size());
    if(!lateness.empty()){result.lateMax=lateness.back();result.lateP99=lateness[size_t(.99*double(lateness.size()-1))];}
    tuner.push(audio,false); // must hide stale pitch immediately in the candidate
    if constexpr(std::is_same_v<T,spectralforge::Tuner>)require(tuner.frequency()==0,"Disabled tuner exposed stale reading");
    tuner.stop();require(tuner.stopped(),"Worker did not stop");
#if CHIMERA_TUNER_TEST_HOOKS
    if constexpr(std::is_same_v<T,spectralforge::Tuner>) {
        for(const auto& f:feed) {
            diagnostics<<std::setprecision(12)<<"FEED,"<<rate<<','<<block<<','<<hz<<','<<f.offset<<','
                <<1000*f.offset/rate<<','<<f.push<<','<<f.observed<<','<<f.hz<<','<<f.confidence<<'\n';
            // Separate diagnostics preserve FEED schema and all gate clocks.
            // No oversleep is discarded, subtracted or retried.
            diagnostics<<"FEED_WAIT,"<<rate<<','<<block<<','<<hz<<','<<f.offset<<','
                <<1000*(f.offset+block)/rate<<','<<f.sleepBegin<<','<<f.sleepEnd<<','<<f.readEnd<<'\n';
        }
        const auto& trace=tuner.timingAfterStopForTest();
        // Separate tagged CSV stream on stderr; retain every estimate, including
        // rejected/generation-discarded observations and routes that fail gates.
        for(size_t i=0;i<trace.size;++i){const auto& r=trace.rows[i];
            diagnostics<<std::setprecision(12)<<"WORKER_CPU,"<<rate<<','<<block<<','<<hz<<','<<i<<','
                <<r.compute()<<','<<r.computeCpu()<<','<<(r.computeCpu()>=0?r.compute()-r.computeCpu():-1.)<<'\n';
            diagnostics<<std::setprecision(12)<<"TIMING,"<<rate<<','<<block<<','<<hz<<','<<i<<','<<r.generation<<','
                <<r.windowComplete-timingEpoch<<','<<r.wake-timingEpoch<<','<<r.service-timingEpoch<<','
                <<r.estimateBegin-timingEpoch<<','<<r.estimateEnd-timingEpoch<<','<<r.publish-timingEpoch<<','
                <<r.wakeDelay()<<','<<r.serviceDelay()<<','<<r.preparation()<<','<<r.compute()<<','<<r.publication()<<','
                <<r.publishAge()<<','<<r.hz<<','<<r.confidence<<','<<r.published<<'\n';
        }
        diagnostics<<"TIMING_END,"<<rate<<','<<block<<','<<hz<<','<<trace.size<<','<<trace.dropped<<'\n';
        require(trace.size>0 && trace.dropped==0,"Incomplete route timing trace");
    }
#endif
    return result;
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
        // Keep publishing source-paced blocks until the lifecycle observation
        // is acquired. This suite validates reset/expiry behavior; the separate
        // 32-route matrix below retains the unchanged <290 ms acquisition gate.
        for(int block=0;block<120;++block){
            for(int n=0;n<256;++n)for(int c=0;c<2;++c)audio.setSample(c,n,tone(110,(block*256+n)/48000.,1));
            tuner.push(audio,true);
            std::this_thread::sleep_until(start+std::chrono::microseconds((block+1)*5333));
            if(std::abs(error(tuner.frequency(),110))<.1 && tuner.confidence()>.9f)return;
        }
        require(false,"Lifecycle tone did not acquire");
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
    for(const auto scenario:{std::pair{1,false},std::pair{1,true},std::pair{40,true}}) {
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
        for(int block=0;block<scenario.first;++block) {
            for(int n=0;n<256;++n)for(int c=0;c<2;++c)audio.setSample(c,n,tone(110,((70+block)*256+n)/48000.,1));
            tuner.push(audio,true);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(280));
        require(tuner.workerPausedForTest(),"Worker resumed before stale observation check");
        require(tuner.frequency()==0 && tuner.confidence()==0,"Paused worker exposed an expired pitch/confidence");
        if(scenario.second) {
            // Fresh input must not refresh retained history or a full stale queued
            // window. One new block is insufficient to acquire a new observation.
            for(int n=0;n<256;++n)for(int c=0;c<2;++c)audio.setSample(c,n,tone(110,((70+scenario.first)*256+n)/48000.,1));
            tuner.push(audio,true);
        }
        const auto expired=Clock::now();
        while(tuner.completedWorkerIterationsForTest()==pausedIteration && Clock::now()-expired<std::chrono::milliseconds(500))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        require(tuner.completedWorkerIterationsForTest()!=pausedIteration,"Worker did not resume after the pause");
        require(tuner.frequency()==0 && tuner.confidence()==0,"Expired queued input was republished after the worker resumed");
        const auto resumed=Clock::now();
        for(int block=0;block<70;++block) {
            for(int n=0;n<256;++n)for(int c=0;c<2;++c)audio.setSample(c,n,tone(220,(block*256+n)/48000.,1));
            tuner.push(audio,true);
            std::this_thread::sleep_until(resumed+std::chrono::microseconds((block+1)*5333));
        }
        require(std::abs(error(tuner.frequency(),220))<.1,"Fresh stream failed to reacquire after a stalled-input gap");
        tuner.stop();require(tuner.stopped(),"Paused worker could not stop");
    }
    std::cerr<<"PASS expired observation hidden through stall/resume, fresh-block and mixed-queue gaps; fresh stream reacquires\n";
#endif
}
void timingAttribution(bool suppressTimerNotifications=false){
#if CHIMERA_TUNER_TEST_HOOKS
    {
        spectralforge::Tuner partial;partial.prepare(48000);
        juce::AudioBuffer<float> shortWindow(1,8190); // 4095 decimated samples
        for(int n=0;n<8190;++n)shortWindow.setSample(0,n,tone(110,n/48000.,1));
        partial.push(shortWindow,true);
        const auto deadline=Clock::now()+std::chrono::seconds(1);
        const auto iteration=partial.completedWorkerIterationsForTest();
        while(partial.completedWorkerIterationsForTest()<iteration+2 && Clock::now()<deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        require(partial.completedWorkerIterationsForTest()>=iteration+2,"Partial-window worker did not run");
        partial.stop();require(partial.timingAfterStopForTest().size==0,"Incomplete input window was estimated");
    }
    spectralforge::Tuner tuner;tuner.suppressTimerNotificationsForTest(suppressTimerNotifications);tuner.prepare(48000);
    tuner.pauseWorkerForTest(120);
    auto deadline=Clock::now()+std::chrono::seconds(1);
    while(!tuner.workerPausedForTest() && Clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    require(tuner.workerPausedForTest(),"Timing worker pause not reached");
    // An entire first window queued before service must survive first activation.
    juce::AudioBuffer<float> audio(1,8192);
    for(int n=0;n<8192;++n)audio.setSample(0,n,tone(110,n/48000.,1));
    const auto before=juce::Time::getMillisecondCounterHiRes();tuner.push(audio,true);
    const auto after=juce::Time::getMillisecondCounterHiRes();
    deadline=Clock::now()+std::chrono::seconds(1);
    while(tuner.frequency()==0 && Clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    require(std::abs(error(tuner.frequency(),110))<.1,"First queued window failed to acquire");
#if JUCE_MAC
    require(tuner.wakeTimerActiveForTest(),"Active macOS wake timer did not start");
    require(tuner.workerPriorityForTest()==int(juce::Thread::Priority::highest),"Active tuner QoS not applied");
    tuner.push(audio,false);
    deadline=Clock::now()+std::chrono::seconds(1);
    while((tuner.workerPriorityForTest()!=int(juce::Thread::Priority::normal) || tuner.wakeTimerActiveForTest()) && Clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    require(tuner.workerPriorityForTest()==int(juce::Thread::Priority::normal),"Disabled tuner QoS not restored");
    require(!tuner.wakeTimerActiveForTest(),"Disabled macOS timer still running");
    require(tuner.frequency()==0 && tuner.confidence()==0,"QoS transition exposed disabled pitch");
#endif
    tuner.stop();require(!tuner.wakeTimerActiveForTest(),"Stopped worker retained wake timer");const auto& trace=tuner.timingAfterStopForTest();
    require(trace.size==1 && trace.dropped==0,"One window must yield one observation");
    const auto& r=trace.rows[0];
    require(r.windowComplete>=before && r.windowComplete<=after,"Completion timestamp not from input publication");
    require(r.published && r.compute()>0 && r.wakeDelay()>0 && r.serviceDelay()>=0 && r.preparation()>=0 && r.publication()>=0,"Invalid worker attribution");
    require(std::abs(r.wakeDelay()+r.serviceDelay()+r.preparation()+r.compute()+r.publication()-r.publishAge())<1e-6,"Attribution does not sum to publish age");
    require(r.publishAge()>r.compute(),"Queue delay lost in estimator time");
#if JUCE_MAC || JUCE_LINUX
    require(r.computeCpu()>=0 && r.computeCpu()<=r.compute()+.01,"Worker CPU attribution unavailable or exceeds wall interval");
#else
    require(r.computeCpu()==-1,"Unavailable CPU clock must remain explicitly unavailable");
#endif
    tuner.prepare(96000);tuner.stop();
    require(tuner.timingAfterStopForTest().size==0,"Prepare retained previous trace");
    std::cerr<<"PASS first-window completion, delayed worker, compute, publish age and trace lifecycle\n";
#if JUCE_MAC
    if(!suppressTimerNotifications)timingAttribution(true);
    else std::cerr<<"PASS macOS progress and disable with timer notifications suppressed\n";
#endif
#endif
}
int main(int argc,char** argv){try{
    normalQoSReference=argc==2 && std::string(argv[1])=="--normal-qos-reference";
    std::cerr<<"WORKER_QOS_REFERENCE,"<<normalQoSReference<<'\n';
    if(argc==2 && std::string(argv[1])=="--timing-only"){timingAttribution();return 0;}
    if(argc==2 && std::string(argv[1])=="--stale-only"){stalledWorkerFreshness();return 0;}
    std::cout<<std::setprecision(10)<<std::unitbuf;
    std::cerr<<"TIMING_HEADER,rate,block,hz,index,generation,window_complete_ms,wake_ms,service_ms,estimate_begin_ms,estimate_end_ms,publish_ms,wake_delay_ms,service_delay_ms,preparation_ms,compute_ms,publication_ms,publish_age_ms,estimated_hz,confidence,published\n";
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
    int failedRoutes=0,feederInvalidRoutes=0,repeatedAttempts=0;
    {
    DeferredRouteDiagnostics routeDiagnostics;
    auto& diagnostics=routeDiagnostics.stream;
    auto& summary=routeDiagnostics.summary;
    diagnostics<<std::setprecision(10);summary<<std::setprecision(10);
    summary<<"ROUTE_TIMING_HEADER,rate,block,hz,attempt,engine,acquire_audio_ms,settle_audio_ms,acquire_wall_ms,settle_wall_ms,"
             "acquire_feeder_late_ms,settle_feeder_late_ms,feeder_late_max_ms,feeder_late_p99_ms,feeds,error_ct,feeder_valid\n";
    for(double rate:{44100.,48000.,96000.,192000.})for(int block:{64,256})for(double hz:{27.5,30.867706,82.406889,440.}){
        NativeResult a,b;
        for(int attempt=1;attempt<=kMaxRouteAttempts;++attempt){
            diagnostics<<"ROUTE_ATTEMPT,"<<rate<<','<<block<<','<<hz<<','<<attempt<<'\n';
            a=native<frozenTuner20261013::Tuner>(rate,block,hz,diagnostics);b=native<spectralforge::Tuner>(rate,block,hz,diagnostics);
            for(const auto& pair:{std::make_pair("baseline",a),std::make_pair("new",b)}){const auto& r=pair.second;
                summary<<"ROUTE_TIMING,"<<rate<<','<<block<<','<<hz<<','<<attempt<<','<<pair.first<<','<<r.acquire<<','<<r.settle<<','
                    <<r.wallAcquire<<','<<r.wallSettle<<','<<r.acquireLate<<','<<r.settleLate<<','<<r.lateMax<<','<<r.lateP99<<','
                    <<r.feeds<<','<<r.cents<<','<<r.feederValid()<<'\n';}
            // Only the feeder's own lateness can repeat an attempt. A valid
            // attempt is final, whether its tuner result passes or fails.
            if(b.feederValid())break;
            summary<<"FEEDER_LATE rate="<<rate<<" block="<<block<<" hz="<<hz<<" attempt="<<attempt<<" late_max_ms="<<b.lateMax
                   <<" limit_ms="<<kFeederLateLimitMs<<" (attempt invalid, not judged)\n";
            if(attempt<kMaxRouteAttempts)++repeatedAttempts;
        }
        for(const auto& pair:{std::make_pair("baseline",a),std::make_pair("new",b)}){const auto& r=pair.second;routeDiagnostics.rows<<std::setprecision(10)<<"worker,"<<pair.first<<','<<rate<<','<<block<<','<<hz<<",1,"<<r.cents<<",0,0,"<<r.acquire<<','<<r.settle<<','<<r.pushUs<<'\n';}
        if(!b.feederValid()){
            ++feederInvalidRoutes;summary<<std::setprecision(12)<<"ROUTE FEEDER INVALID rate="<<rate<<" block="<<block<<" hz="<<hz<<" late_max_ms="<<b.lateMax<<'\n';
        } else if(!(b.acquire>0 && b.acquire<290 && b.settle>0 && b.settle<290 && std::abs(b.cents)<.1)){
            ++failedRoutes;summary<<std::setprecision(12)<<"ROUTE FAIL rate="<<rate<<" block="<<block<<" hz="<<hz<<" acquire="<<b.acquire<<" settle="<<b.settle<<" error="<<b.cents
                <<" wall_acquire="<<b.wallAcquire<<" wall_settle="<<b.wallSettle<<" late_max_ms="<<b.lateMax<<'\n';
        }
    }
    summary<<"ROUTE_TIMING_END,failed="<<failedRoutes<<",feeder_invalid="<<feederInvalidRoutes<<",repeated_attempts="<<repeatedAttempts<<'\n';
    } // Flush complete route evidence before lifecycle assertions can throw.
    channelContinuity();lifecycle();stalledWorkerFreshness();timingAttribution();
    require(failedRoutes==0,"Native acquisition/settle/accuracy gate failed; all route rows retained");
    require(feederInvalidRoutes==0,"Feeder lateness invalidated every attempt of a route; tuner timing not judged");
    std::cerr<<"PASS "<<count<<" kernel cases and 32 native A/B routes (audio-time gates; "<<repeatedAttempts<<" feeder-late attempts repeated). Synthetic sine/harmonic/exponential-decay maxima: "<<worst[0]<<' '<<worst[1]<<' '<<worst[2]<<" ct. Lifecycle re-enable/stale/nonfinite/rate-change passed. Host clock/DI/GUI latency not included.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

