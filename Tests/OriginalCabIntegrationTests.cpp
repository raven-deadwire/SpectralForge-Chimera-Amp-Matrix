#include "CabTimingDiagnostics.h"
#include "IRLibrary.h"
#include "CabLayoutModel.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <new>
#include <stdexcept>
thread_local bool watch=false;
thread_local size_t allocations=0, deletions=0;
void* operator new(std::size_t n){if(watch)++allocations;if(auto* p=std::malloc(std::max(n,size_t(1))))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {if(watch && p)++deletions;std::free(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
using namespace spectralforge;
void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
void process(Cab& c,juce::AudioBuffer<float>& b){watch=true;c.process(b);watch=false;require(c.transitioningMicCount()<=1,"modeled A/B crossfades must be serialized per rig");}
std::vector<float> render(Cab& cab,const juce::dsp::ProcessSpec& spec) {
    juce::AudioBuffer<float> b(int(spec.numChannels),int(spec.maximumBlockSize));
    for(int n=0;n<200;++n){b.clear();process(cab,b);}cab.reset();
    std::vector<float> result;
    for(int block=0;block<150;++block){b.clear();if(block==0)b.setSample(0,0,.25f);process(cab,b);
        for(int n=0;n<b.getNumSamples();++n)result.push_back(b.getSample(0,n));
        if(b.getNumChannels()==2)require(b.getMagnitude(1,0,b.getNumSamples())<1e-8,"no stereo crosstalk");}
    return result;
}
double delta(const std::vector<float>& a,const std::vector<float>& b){double d=0;for(size_t n=0;n<a.size();++n)d=std::max(d,std::abs(double(a[n]-b[n])));return d;}
// Diagnostic partitions of the same 1600 measured callbacks. The acceptance
// gate below still uses every original sample and the unchanged block budget.
void reportPhase(std::ostream& out,const char* name,const std::vector<double>& times,const std::vector<double>& cpuTimes,
                 size_t first,size_t last,double budget) {
    std::vector<double> wall(times.begin()+first,times.begin()+last),cpu(cpuTimes.begin()+first,cpuTimes.begin()+last);
    const auto misses=std::count_if(wall.begin(),wall.end(),[budget](double us){return us>budget;});
    std::sort(wall.begin(),wall.end());std::sort(cpu.begin(),cpu.end());
    const auto p99=size_t(double(wall.size())*.99);
    out<<"PHASE name="<<name<<" blocks="<<wall.size()<<" p50_us="<<wall[wall.size()/2]
        <<" p99_us="<<wall[p99]<<" thread_cpu_p99_us="<<cpu[p99]<<" max_us="<<wall.back()<<" misses="<<misses<<'\n';
}
#if defined(CHIMERA_CAB_PROFILE) && CHIMERA_CAB_PROFILE
void reportProfile(std::ostream& out,const char* engine,double sr,int block,int channels,const char* phase,
                   const std::vector<cabProfile::Counters>& counters,size_t first,size_t last) {
    for(unsigned stage=0;stage<unsigned(cabProfile::Stage::Count);++stage) {
        std::vector<double> elapsed;elapsed.reserve(last-first);
        uint64_t calls=0;double total=0;
        for(size_t n=first;n<last;++n) {
            const double us=double(counters[n].nanoseconds[stage])*.001;
            elapsed.push_back(us);total+=us;calls+=counters[n].calls[stage];
        }
        if(!calls)continue;
        std::sort(elapsed.begin(),elapsed.end());
        out<<"PROFILE engine="<<engine<<" sr="<<sr<<" block="<<block<<" channels="<<channels
            <<" phase="<<phase<<" stage="<<cabProfile::names[stage]<<" callbacks="<<elapsed.size()
            <<" calls="<<calls<<" mean_us="<<total/double(elapsed.size())<<" p50_us="<<elapsed[elapsed.size()/2]
            <<" p99_us="<<elapsed[size_t(double(elapsed.size())*.99)]<<" max_us="<<elapsed.back()<<'\n';
    }
}
#endif
// The retry rule on synthetic callbacks: budget 1000 us, median callback 60 us of
// wall time for 100 units of work, so a miss may hold up to 833 units (500 us).
void deschedulingSelfCheck() {
    const auto samples=[](double wall,double work,size_t misses,double missWall,double missWork) {
        std::vector<cabTiming::Sample> out(cabTiming::measuredCallbacks);
        for(size_t i=0;i<out.size();++i) {
            const bool miss=i%100==7 && i/100<misses;
            auto& s=out[i];s.index=i;s.wallUs=miss ? missWall : wall;
            s.begin.cpuUs=double(i)*10000;s.end.cpuUs=s.begin.cpuUs+(miss ? missWork : work);
        }
        return out;
    };
    // Sixteen 5 ms misses with near-typical work (72 us): descheduled, may measure again.
    auto c=cabTiming::classifyMisses(samples(60,100,16,5000,120),1000);
    require(!c.withinBudget(1000) && c.misses==16 && c.descheduled==16 && c.onlyDescheduling(1000),"descheduled misses not recognised");
    // The same misses with ten times the work (600 us): real CAB cost, no retry.
    c=cabTiming::classifyMisses(samples(60,100,16,5000,1000),1000);
    require(!c.withinBudget(1000) && c.descheduled==0 && !c.onlyDescheduling(1000),"heavy-work misses treated as descheduling");
    // One heavy miss among descheduled ones: no retry.
    auto mixed=samples(60,100,16,5000,120);mixed[7].end.cpuUs=mixed[7].begin.cpuUs+1000;
    c=cabTiming::classifyMisses(mixed,1000);
    require(c.descheduled==15 && !c.onlyDescheduling(1000),"a heavy miss was hidden by descheduled ones");
    // A median callback over half the budget: no retry.
    c=cabTiming::classifyMisses(samples(600,100,16,5000,120),1000);
    require(!c.onlyDescheduling(1000),"a slow route qualified for a retry");
    // No thread clock: no retry.
    auto blind=samples(60,100,16,5000,120);for(auto& s:blind){s.begin.cpuUs=-1;s.end.cpuUs=-1;}
    c=cabTiming::classifyMisses(blind,1000);
    require(!c.clocksValid && !c.onlyDescheduling(1000),"a route without thread clocks qualified for a retry");
    // Within budget: nothing to retry.
    c=cabTiming::classifyMisses(samples(60,100,8,5000,120),1000);
    require(c.withinBudget(1000),"budget verdict");
    std::cout<<"PASS descheduling classifier: descheduled misses retried; heavy, mixed, slow-route and clockless misses not\n";
}
int main(int argc,char** argv){try {
    std::cout<<std::setprecision(12);
    require(!lifecycle::enabled(),"CAB timing requires lifecycle tracing disabled; run the isolated CTest entries");
    deschedulingSelfCheck();
    std::cout<<"TIMING_PROTOCOL lifecycle_trace=disabled measured_blocks=1600 worker_blocks=1200 forced_publication_blocks=400\n";
    bool expanded=false,layouts=false,extendedBuffers=false;
    for(int i=1;i<argc;++i) {
        const std::string argument=argv[i];
        if(argument=="--expanded")expanded=true;
        else if(argument=="--layouts")layouts=true;
        else if(argument=="--extended-buffers")extendedBuffers=true;
        else require(false,"usage: ChimeraOriginalCabIntegrationTests [--expanded | --layouts] [--extended-buffers]");
    }
    require(!(expanded && layouts),"select one CAB model engine");
    const char* timingSuite=extendedBuffers ? "extended" : "integration";
#if defined(CHIMERA_CAB_PROFILE) && CHIMERA_CAB_PROFILE
    timingSuite=extendedBuffers ? "profile-extended" : "profile";
    std::cout<<"PROFILE_PROTOCOL diagnostic_only=1 stage_clocks_add_overhead=1 parameter_refresh_per_callback=1 nested_stage_times=1\n";
#endif
    // The verdict is still wall p99 against one block period. The thread clock
    // only decides whether a failed route may measure once more (classifyMisses).
    std::cout<<"PAIRED_TIMING_PROTOCOL thread_clock_use=retry_eligibility_only retry_limit=1 wall_gate_unchanged=1 thread_clocks_outside_wall=1 "
        "thread_clock_reads_per_callback=2 extra_measurement_overhead=1 "
        "cycles_to_time=disabled cycles_compared_with=route_median thread_cpu_p99_is_independent=1 raw_samples=csv "
        "thread_clock="
#if defined(_WIN32)
        <<"QueryThreadCycleTime"
#else
        <<"CLOCK_THREAD_CPUTIME_ID"
#endif
        <<'\n';
    std::cout<<"CALLER_SCHEDULING phase=entry";
    cabTiming::appendCallerScheduling(std::cout);std::cout<<'\n';
    const char* engineName=layouts ? "array-v3-6x10" : expanded ? "expanded-v2" : "original-v1";
    const auto modelKey=[expanded,layouts](originalCab::Settings p){if(layouts)return cabLayout::key({{p,9,p.mic==1 ? 10 : 20,2},7,p.mic==1 ? 7 : p.unit});return expanded ? cabExpansion::key({p,12,p.mic==1 ? 10 : 20,2}) : originalCab::key(p);};
    if(layouts) {
        originalCab::Settings preview{};preview.cabinet=1;preview.mic=1;
        const auto restored=cabLayout::settings(modelKey(preview));
        require(cabLayout::geometry(restored).count==6 && cabLayout::effectiveUnit(restored)==5,
            "production 6x10 / previous bottom-right microphone migration");
    }
    const juce::ScopedNoDenormals noDenormals; // Same floating-point mode as processBlock.
    bool cpuWithinBudget=true,workerSchedulingValid=true;
    const std::vector<int> blocks=extendedBuffers ? std::vector<int>{32,64,128,256,512} : std::vector<int>{64,256};
    // Passes 0 and 1 check the retry rule end to end on one route, with this
    // platform's thread clock, before the measurement that counts (pass 2):
    // twenty callbacks of the first attempt overrun by three block periods,
    // asleep (no CPU work: must measure again) or spinning (CPU work: must fail
    // without a retry). Their rows and CSVs carry their own names.
    int selfTestRetries[2]{};bool selfTestWithin[2]{true,true};
    for(int pass=0;pass<3;++pass) {
    const bool selfTest=pass<2;
    const std::string passSuite=selfTest ? std::string(timingSuite)+(pass==0 ? "-selftest-descheduled" : "-selftest-work") : std::string(timingSuite);
    const std::vector<double> passRates=selfTest ? std::vector<double>{48000.} : std::vector<double>{44100.,48000.,96000.};
    const std::vector<int> passBlocks=selfTest ? std::vector<int>{64} : blocks;
    const std::vector<int> passChannels=selfTest ? std::vector<int>{1} : std::vector<int>{1,2};
    for(double sr:passRates)for(int block:passBlocks)for(int channels:passChannels)
    // A route measures once more only when its first p99 failure was the
    // thread not running (classifyMisses); the second attempt's verdict stands.
    for(int attempt=1;;++attempt) {
        std::ostringstream out;out<<std::setprecision(12);
        try {
        juce::dsp::ProcessSpec spec{sr,juce::uint32(block),juce::uint32(channels)};
        std::array<Cab,3> cabs;for(auto& c:cabs)c.prepare(spec);
#if defined(CHIMERA_CAB_PROFILE) && CHIMERA_CAB_PROFILE
        // Counters outlive the library's worker even if a contract throws.
        cabProfile::Counters workerProfile;
        std::vector<cabProfile::Counters> blockProfiles(1600);
#endif
        IRLibrary library({&cabs[0],&cabs[1],&cabs[2]});
#if defined(CHIMERA_CAB_PROFILE) && CHIMERA_CAB_PROFILE
        library.profileSink=&workerProfile;
#endif
        originalCab::Settings a{true,1,0,0,0,.3,.25,10},b=a;b.unit=1;b.mic=1;b.distanceCm=18;
        for(auto& cab:cabs){cab.requestedModel=modelKey(a);cab.secondMic()->requestedModel=modelKey(b);cab.blend=.5;}
        library.prepare(spec,{0,0,0});library.stop();
        auto& cab=cabs[0];cab.blend=0;const auto outA=render(cab,spec);cab.blend=1;const auto outB=render(cab,spec);
        require(delta(outA,outB)>1e-4,"independent same cabinet / different unit / mic");
        cab.blend=.5;const auto mix=render(cab,spec);auto expected=outA;
        for(size_t n=0;n<expected.size();++n)expected[n]=.5f*(outA[n]+outB[n]);require(delta(mix,expected)<2e-6,"modeled constant-sum blend");
        cab.secondMic()->requestedModel=modelKey(a);library.prepare(spec,{0,0,0});library.stop();
        require(delta(outA,render(cab,spec))<2e-6,"same unit and same mic unity at midpoint");
        // Six stereo mic paths, including publication overlap and latest-request convergence.
        for(auto& c:cabs)c.blend=.5;
        library.prepare(spec,{0,0,0});
        juce::AudioBuffer<float> audio(channels,block);
        std::array<juce::AudioBuffer<float>,3> laneAudio;for(auto& lane:laneAudio)lane.setSize(channels,block);
        std::vector<double> times,cpuTimes;times.reserve(1600);cpuTimes.reserve(1600);
        std::vector<cabTiming::Sample> timingSamples;timingSamples.reserve(1600);
        unsigned misses=0;double peak=0,maxStep=0;float previous=0;
        for(int n=0;n<1200;++n) {
            if(n<80)for(auto& c:cabs){a.position=double(n%60)/60;c.requestedModel=modelKey(a);b.distanceCm=5+n*.2;c.secondMic()->requestedModel=modelKey(b);}
            for(int ch=0;ch<channels;++ch)for(int k=0;k<block;++k)audio.setSample(ch,k,float(.05*std::sin((n*block+k)*.07)));
            for(auto& lane:laneAudio)lane.makeCopyOf(audio,true);
#if defined(CHIMERA_CAB_PROFILE) && CHIMERA_CAB_PROFILE
            cabProfile::ThreadCapture profileCapture(&blockProfiles[size_t(n)]);
#endif
            const auto clockBefore=cabTiming::readThreadClock();const auto start=std::chrono::steady_clock::now();
#if defined(CHIMERA_CAB_PROFILE) && CHIMERA_CAB_PROFILE
            // The production processor refreshes both mic filters each block.
            // These extra clocks/calls belong only to the diagnostic executable.
            for(auto& c:cabs) {c.setCuts(70,9000);c.secondMic()->setCuts(70,9000);}
#endif
            // Independent lane inputs; copying is outside this CAB-only measurement.
            for(int lane=0;lane<3;++lane)process(cabs[lane],laneAudio[lane]);
            if(selfTest && attempt==1 && n%60==5) {
                const auto until=std::chrono::steady_clock::now()+std::chrono::microseconds(int64_t(3e6*block/sr));
                if(pass==0)std::this_thread::sleep_until(until);else while(std::chrono::steady_clock::now()<until){}
            }
            const auto end=std::chrono::steady_clock::now();const auto clockAfter=cabTiming::readThreadClock();
            const double us=std::chrono::duration<double,std::micro>(end-start).count();
            timingSamples.push_back({size_t(n),us,clockBefore,clockAfter});
            times.push_back(us);cpuTimes.push_back(timingSamples.back().cpuUs());if(us>1e6*block/sr)++misses;
            for(int k=0;k<block;++k){const float x=laneAudio[0].getSample(0,k);require(std::isfinite(x),"finite transitions");peak=std::max(peak,std::abs(double(x)));maxStep=std::max(maxStep,std::abs(double(x-previous)));previous=x;}
            if(n%8==0)juce::Thread::sleep(1);
        }
        bool ready=false;
        for(int n=0;n<10000 && !ready;++n) {
            ready=true;for(auto& c:cabs){audio.clear();process(c,audio);ready=ready && c.activeModel==c.requestedModel && c.secondMic()->activeModel==c.secondMic()->requestedModel;}
            if(!ready)juce::Thread::sleep(1);
        }
        require(ready,"latest generation converges after automation storm");require(peak<2 && maxStep<.3,"bounded swap transition");
        // Finish any just-accepted fade before stopping collection. This makes
        // the following six publications eligible, rather than leaving a retired
        // engine that could silently suppress the supposed worst-case swap.
        for(int n=0;n<200;++n)for(auto& c:cabs){audio.clear();process(c,audio);}
        for(auto& c:cabs)require(c.transitioningMicCount()==0,"pre-publication fades drained");
        library.stop();
        // Six complete engines ready before the same callback. The actual
        // scheduler must cap overlap at one fading mic per rig (nine engines).
        // Construction is outside the watched/timed callback.
        a.position=.85;const auto nextKey=modelKey(a);const auto wave=cabLayout::generate(nextKey,sr);
        // Prepared convolution must reproduce the generated response and add
        // no processing latency; acoustic arrival remains in the kernel itself.
        juce::AudioBuffer<float> referenceSamples(1,int(wave.size()));referenceSamples.copyFrom(0,0,wave.data(),int(wave.size()));
        Cab::Kernel reference(std::move(referenceSamples),sr,spec,0,1,nextKey);
        require(reference.getLatency()==0,"modeled convolution adds processing latency");
        juce::AudioBuffer<float> impulse(channels,block);double kernelResidual=0;
        for(int offset=0;offset<int(wave.size())+block;offset+=block) {
            impulse.clear();if(offset==0)impulse.setSample(0,0,1);watch=true;reference.process(impulse);watch=false;
            for(int k=0;k<block;++k)kernelResidual=std::max(kernelResidual,std::abs(double(impulse.getSample(0,k))-(offset+k<int(wave.size()) ? wave[size_t(offset+k)] : 0)));
        }
        require(kernelResidual<3e-6,"prepared kernel differs from generated acoustic response");
        for(auto& c:cabs)for(auto* slot:{&c,c.secondMic()}) {
            juce::AudioBuffer<float> samples(1,int(wave.size()));samples.copyFrom(0,0,wave.data(),int(wave.size()));
            slot->requestedModel=nextKey;slot->publish(std::make_unique<Cab::Kernel>(std::move(samples),sr,spec,0,100,nextKey));
        }
        for(int n=0;n<400;++n) {
            for(auto& lane:laneAudio)for(int ch=0;ch<channels;++ch)for(int k=0;k<block;++k)lane.setSample(ch,k,float(.05*std::sin(((n+1200)*block+k)*.07)));
#if defined(CHIMERA_CAB_PROFILE) && CHIMERA_CAB_PROFILE
            cabProfile::ThreadCapture profileCapture(&blockProfiles[size_t(n+1200)]);
#endif
            const auto clockBefore=cabTiming::readThreadClock();const auto start=std::chrono::steady_clock::now();
#if defined(CHIMERA_CAB_PROFILE) && CHIMERA_CAB_PROFILE
            for(auto& c:cabs) {c.setCuts(70,9000);c.secondMic()->setCuts(70,9000);}
#endif
            for(int lane=0;lane<3;++lane)process(cabs[lane],laneAudio[lane]);
            const auto end=std::chrono::steady_clock::now();const auto clockAfter=cabTiming::readThreadClock();
            const double us=std::chrono::duration<double,std::micro>(end-start).count();
            timingSamples.push_back({size_t(n+1200),us,clockBefore,clockAfter});
            times.push_back(us);cpuTimes.push_back(timingSamples.back().cpuUs());if(us>1e6*block/sr)++misses;
            for(int k=0;k<block;++k){const auto x=laneAudio[0].getSample(0,k);require(std::isfinite(x),"finite six-slot swap");peak=std::max(peak,std::abs(double(x)));maxStep=std::max(maxStep,std::abs(double(x-previous)));previous=x;}
        }
        require(peak<2 && maxStep<.3,"bounded six-slot swap");
        for(auto& c:cabs)require(c.activeModel==nextKey && c.secondMic()->activeModel==nextKey,"six queued mic swaps converge");
        for(auto& c:cabs)c.clear();require(library.resourcesReleased(),"worker teardown");
        const auto scheduling=library.workerScheduling();
        // Preserve every measured route and the actual context before failing
        // this diagnostic contract, just as the wall gate reports all routes.
        workerSchedulingValid=workerSchedulingValid && scheduling.startSucceeded && scheduling.enteredRun
            && scheduling.observedPriority==int(juce::Thread::Priority::low);
        out<<"WORKER_SCHEDULING engine="<<engineName<<" suite="<<timingSuite
            <<" sr="<<sr<<" block="<<block<<" channels="<<channels
            <<" start_succeeded="<<scheduling.startSucceeded<<" entered_run="<<scheduling.enteredRun
            <<" requested_juce_priority="<<int(juce::Thread::Priority::low)
            <<" observed_juce_priority="<<scheduling.observedPriority
#if JUCE_WINDOWS || JUCE_MAC
            <<" worker_priority_query=native"
#else
            <<" worker_priority_query=requested_only linux_nonrealtime_request_ignored=1"
#endif
            ;
        cabTiming::appendCallerScheduling(out);out<<'\n';
        out<<"WORKER_ACTIVITY engine="<<engineName<<" suite="<<timingSuite
            <<" sr="<<sr<<" block="<<block<<" channels="<<channels
            <<" cancelled_builds="<<library.cancelledBuildCount()<<'\n';
        // Decided before any evidence is written: a discarded attempt keeps its
        // CSV and rows under distinct names, so each route still has exactly
        // one TIMING / PHASE / PAIRED_TIMING row set and one CSV at its usual path.
        const double budgetUs=1e6*block/sr;
        const auto missClasses=cabTiming::classifyMisses(timingSamples,budgetUs);
        const bool retry=attempt==1 && !missClasses.withinBudget(budgetUs) && missClasses.onlyDescheduling(budgetUs);
        const std::string suite=retry ? passSuite+"-discarded" : passSuite;
        const cabTiming::Configuration timingConfig{engineName,suite.c_str(),sr,block,channels};
        const auto timingPath=cabTiming::saveCsv(timingConfig,timingSamples);
        out<<"TIMING_EVIDENCE engine="<<engineName<<" suite="<<suite
            <<" sr="<<sr<<" block="<<block<<" channels="<<channels<<" samples="<<timingSamples.size()
            <<" path="<<timingPath.generic_string()<<'\n';
        cabTiming::reportPairs(out,timingConfig,"all",timingSamples,0,1600);
        cabTiming::reportPairs(out,timingConfig,"worker_and_automation",timingSamples,0,1200);
        cabTiming::reportPairs(out,timingConfig,"forced_six_slot_publication",timingSamples,1200,1600);
        out<<"PHASE_CONFIG engine="<<engineName
            <<" sr="<<sr<<" block="<<block<<" channels="<<channels<<'\n';
        reportPhase(out,"worker_and_automation",times,cpuTimes,0,1200,1e6*block/sr);
        reportPhase(out,"forced_six_slot_publication",times,cpuTimes,1200,1600,1e6*block/sr);
#if defined(CHIMERA_CAB_PROFILE) && CHIMERA_CAB_PROFILE
        reportProfile(out,engineName,sr,block,channels,"worker_and_automation",blockProfiles,0,1200);
        reportProfile(out,engineName,sr,block,channels,"forced_six_slot_publication",blockProfiles,1200,1600);
        for(unsigned stage=0;stage<unsigned(cabProfile::Stage::Count);++stage)if(workerProfile.calls[stage])
            out<<"WORKER_PROFILE engine="<<engineName<<" sr="<<sr<<" block="<<block<<" channels="<<channels
                <<" stage="<<cabProfile::names[stage]<<" calls="<<workerProfile.calls[stage]
                <<" total_us="<<double(workerProfile.nanoseconds[stage])*.001<<'\n';
#endif
        std::sort(times.begin(),times.end());std::sort(cpuTimes.begin(),cpuTimes.end());
        out<<"TIMING engine="<<engineName<<" sr="<<sr<<" block="<<block<<" channels="<<channels<<" three_cabs_six_mics p50_us="<<times[800]<<" p99_us="<<times[1584]<<" thread_cpu_p99_us="<<cpuTimes[1584]<<" max_us="<<times.back()<<" misses="<<misses<<"/1600 kernel_residual="<<kernelResidual<<" peak="<<peak<<" max_step="<<maxStep<<'\n';
        const char* prefix=selfTest ? "GATE_SELF_TEST " : "";
        std::istringstream lines(out.str());
        if(retry) {
            for(std::string line;std::getline(lines,line);)std::cout<<prefix<<"DISCARDED_ATTEMPT "<<line<<'\n';
            std::cout<<prefix<<"TIMING_RETRY engine="<<engineName<<" sr="<<sr<<" block="<<block<<" channels="<<channels
                <<" reason=descheduled first_p99_us="<<missClasses.wallP99<<" first_p50_us="<<missClasses.wallP50<<" budget_us="<<budgetUs
                <<" misses="<<missClasses.misses<<" descheduled_misses="<<missClasses.descheduled
                <<" work_clock="<<missClasses.workClock<<" max_miss_estimated_work_us="<<missClasses.maxMissWorkUs
                <<" work_limit_us="<<budgetUs*cabTiming::deschedulingWorkShare<<" discarded_csv_suite="<<suite<<'\n';
            if(selfTest)++selfTestRetries[pass];
            continue;
        }
        for(std::string line;std::getline(lines,line);)std::cout<<prefix<<line<<'\n';
        if(selfTest)selfTestWithin[pass]=missClasses.withinBudget(budgetUs);
        else cpuWithinBudget=cpuWithinBudget && missClasses.withinBudget(budgetUs);
        break;
        } catch(...) {std::cout<<out.str();throw;}
    }
    }
    require(selfTestRetries[0]==1,"gate self-test: sleeping callbacks were not measured again as descheduling");
    require(selfTestRetries[1]==0 && !selfTestWithin[1],"gate self-test: spinning callbacks passed the gate or were measured again");
    std::cout<<"PASS gate self-test: sleeping overruns measured again once; spinning overruns failed without a retry\n";
    require(allocations==0 && deletions==0,"callback new/delete observed");
    std::cout<<"CALLBACK callback_new="<<allocations<<" callback_delete="<<deletions<<" (thread-local C++ operators only; not a universal malloc/lock tracer)\n";
    require(workerSchedulingValid,"IR worker did not start/run with the low priority request/observation");
    require(cpuWithinBudget,"CAB-only p99 exceeds one block period (runner CPU gate)");
    std::cout<<"PASS original CAB integration and runner CPU gate\n";return 0;
}catch(const std::exception& e){watch=false;std::cerr<<e.what()<<'\n';return 1;}}
