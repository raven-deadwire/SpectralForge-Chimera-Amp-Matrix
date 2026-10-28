#pragma once

// Opt-in measurement harness. This file is compiled only by the native EQ test
// executable and never changes the product's audio-callback implementation.
#if JUCE_MAC
#include <mach/mach.h>
#include <pthread.h>
#endif
#include <iomanip>

namespace {
namespace eqAttribution {
using Clock=std::chrono::steady_clock;
using Stamp=Clock::time_point;
inline double micros(Clock::duration value){return std::chrono::duration<double,std::micro>(value).count();}
inline double seconds(Clock::duration value){return std::chrono::duration<double>(value).count();}

double threadCPUSeconds() {
#if JUCE_WINDOWS
    FILETIME created{},exited{},kernel{},user{};
    check(GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user)!=0,"Thread CPU query");
    ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;return double(k.QuadPart+u.QuadPart)*1.e-7;
#elif JUCE_MAC
    thread_basic_info_data_t info{};mach_msg_type_number_t count=THREAD_BASIC_INFO_COUNT;
    check(thread_info(pthread_mach_thread_np(pthread_self()),THREAD_BASIC_INFO,
                      reinterpret_cast<thread_info_t>(&info),&count)==KERN_SUCCESS,"Thread CPU query");
    return double(info.user_time.seconds+info.system_time.seconds)
         +double(info.user_time.microseconds+info.system_time.microseconds)*1.e-6;
#else
    timespec value{};check(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0,"Thread CPU query");
    return double(value.tv_sec)+double(value.tv_nsec)*1.e-9;
#endif
}

struct Metric {
    std::vector<double> values;double sum{},maximum{};uint64_t count{};
    explicit Metric(size_t capacity=8192){values.reserve(capacity);}
    void add(double value){sum+=value;maximum=std::max(maximum,value);++count;if(values.size()<values.capacity())values.push_back(value);}
    double mean()const{return count?sum/double(count):0.;}
    double quantile(double fraction)const{return values.empty()?0.:percentile(values,fraction);}
};
enum class Stage {NoOp,EQPair,Full};
enum class Pacing {LegacyRebase,DueCatchUp};
enum class Consumer {Direct,NativeNoPanelPaint,NativePaint};
const char* name(Stage value){return value==Stage::NoOp?"producer_noop":value==Stage::EQPair?"eq_pair":"full_processor";}
const char* name(Pacing value){return value==Pacing::LegacyRebase?"legacy_sleep_rebase":"due_samples_catch_up";}
const char* name(Consumer value){return value==Consumer::Direct?"headless_direct":value==Consumer::NativeNoPanelPaint?"native_panel_paint_suppressed":"native_paint";}
struct Case {
    std::string group;double rate{192000.};int fft{},interval{40};Stage stage{Stage::Full};
    Pacing pacing{Pacing::DueCatchUp};Consumer consumer{Consumer::Direct};bool cachedSignal{};
    double duration{2.};int repeat{};
};
struct CallbackSample {
    double entryMicros{},entryIntervalMicros{},generationMicros{},processorMicros{},scheduleLateMicros{},originalClockLateMicros{};
    double sleepRequestedMicros{},sleepActualMicros{},wakeLateMicros{},rebaseLostMicros{};
    bool rebased{},catchUp{};
};

class Producer {
public:
    Producer(ChimeraProcessor& p,const Case& c):processor(p),config(c),first(2,128),second(2,128) {
        samples.reserve(65536);
        if(config.cachedSignal) {
            cached.resize(size_t(std::ceil((config.duration+2.)*config.rate))+128);
            double phase=0.;for(auto& sample:cached){sample=float(.1*std::sin(phase));phase=advance(phase);}
        }
        // Buffers, the waveform cache and all callback records are allocated
        // before the measured source starts. No vector can grow on this thread.
        thread=std::thread([this]{try{run();}catch(...){failure=std::current_exception();failed.store(true,std::memory_order_release);armed.store(true,std::memory_order_release);}});
        while(!armed.load(std::memory_order_acquire))std::this_thread::yield();
    }
    ~Producer(){stop();}
    void start(Stamp epoch){startTime=epoch;go.store(true,std::memory_order_release);}
    void stop(){running.store(false,std::memory_order_release);go.store(true,std::memory_order_release);if(thread.joinable())thread.join();}
    uint64_t callbacks()const{return callbackCount.load(std::memory_order_relaxed);}
    bool hasFailed()const{return failed.load(std::memory_order_acquire);}
    void rethrowFailure()const{if(failure)std::rethrow_exception(failure);}
    const std::vector<CallbackSample>& records()const{return samples;}
    double cpuSeconds{},lifetimeSeconds{};uint64_t rebases{},consecutiveCatchUpMax{};
    double rebasedSourceMicros{};
private:
    double advance(double phase)const{return std::fmod(phase+2*juce::MathConstants<double>::pi*1007.8125/config.rate,2*juce::MathConstants<double>::pi);}
    void run() {
        armed.store(true,std::memory_order_release);
        while(!go.load(std::memory_order_acquire))std::this_thread::yield();
        if(!running.load())return;
        const double cpuStart=threadCPUSeconds();
        const auto interval=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(128./config.rate));
        auto scheduled=startTime,previousEntry=startTime;double phase=0.;size_t cachePosition=0;
        uint64_t completed=0,consecutiveCatchUp=0;
        while(running.load(std::memory_order_acquire)) {
            CallbackSample record;const auto entry=Clock::now();
            record.entryMicros=micros(entry-startTime);record.entryIntervalMicros=completed?micros(entry-previousEntry):0.;previousEntry=entry;
            record.scheduleLateMicros=std::max(0.,micros(entry-scheduled));
            record.originalClockLateMicros=std::max(0.,micros(entry-(startTime+interval*completed)));
            for(int n=0;n<128;++n) {
                float value;
                if(config.cachedSignal){value=cached[cachePosition++];if(cachePosition==cached.size())cachePosition=0;}
                else{value=float(.1*std::sin(phase));phase=advance(phase);}
                first.setSample(0,n,value);first.setSample(1,n,-value);
                second.setSample(0,n,value);second.setSample(1,n,-value);
            }
            const auto processing=Clock::now();record.generationMicros=micros(processing-entry);
            if(config.stage==Stage::Full)processor.processBlock(first,midi);
            else if(config.stage==Stage::EQPair){processor.graphicalEQ(0).process(first);processor.graphicalEQ(1).process(second);}
            const auto processed=Clock::now();record.processorMicros=micros(processed-processing);
            ++completed;callbackCount.store(completed,std::memory_order_relaxed);scheduled+=interval;
            if(config.pacing==Pacing::LegacyRebase && processed>scheduled+interval*2) {
                record.rebased=true;record.rebaseLostMicros=micros(processed-scheduled);
                ++rebases;rebasedSourceMicros+=record.rebaseLostMicros;scheduled=processed;
            }
            record.catchUp=processed>=scheduled;
            if(record.catchUp){++consecutiveCatchUp;consecutiveCatchUpMax=std::max(consecutiveCatchUpMax,consecutiveCatchUp);}
            else consecutiveCatchUp=0;
            record.sleepRequestedMicros=std::max(0.,micros(scheduled-processed));
            const auto sleeping=Clock::now();
            // Diagnostic scheduling, outside the measured processBlock. Due
            // pacing retains source-clock debt; late blocks are visible below.
            if(scheduled>sleeping)std::this_thread::sleep_until(scheduled);
            const auto woke=Clock::now();record.sleepActualMicros=micros(woke-sleeping);
            record.wakeLateMicros=std::max(0.,micros(woke-scheduled));
            if(samples.size()<samples.capacity())samples.push_back(record);
        }
        lifetimeSeconds=seconds(Clock::now()-startTime);cpuSeconds=threadCPUSeconds()-cpuStart;
    }
    ChimeraProcessor& processor;Case config;juce::AudioBuffer<float> first,second;juce::MidiBuffer midi;
    std::vector<float> cached;std::vector<CallbackSample> samples;
    std::thread thread;std::atomic<bool> running{true},armed{},go{},failed{};std::atomic<uint64_t> callbackCount{};Stamp startTime{};
    std::exception_ptr failure;
};

struct UIObservation {
    double elapsedMs{},gapMs{},fftMicros{},updateMicros{},pathMicros{},timerMicros{};
    uint64_t sequence{},pending{},consumed{},windows{};
};
struct ConsumerMetrics {
    Metric timer,paint,update,fft,path,gap;std::vector<UIObservation> observations;
    Stamp epoch{};uint64_t previousSequence{},missingUpdates{},observedHighWater{},paintCalls{};
    ConsumerMetrics(){observations.reserve(4096);}
    static void native(void* context,const GraphicalEQPanel& panel,bool painting,double duration) {
        auto& self=*static_cast<ConsumerMetrics*>(context);
        if(painting){self.paint.add(duration);++self.paintCalls;return;}
        self.timer.add(duration);const auto sequence=panel.analysisUpdates();
        if(sequence==self.previousSequence)return;
        self.missingUpdates+=sequence-self.previousSequence-1;self.previousSequence=sequence;
        self.add(sequence,panel.analysisLastPendingSamples(),panel.analysisLastConsumedSamples(),panel.analysisLastFFTWindows(),
                 panel.analysisLastConsumerIntervalMs(),panel.lastFFTTimeMicros(),panel.analysisUpdateMicros(),panel.spectrumPathMicros(),duration);
    }
    void direct(const EQSpectrumAnalyzer& analyzer,double duration) {
        timer.add(duration);previousSequence=analyzer.updates();
        add(analyzer.updates(),analyzer.lastPendingSamples(),analyzer.lastConsumedSamples(),analyzer.lastFFTWindows(),
            analyzer.lastConsumerIntervalMs(),analyzer.fftMicros(),analyzer.updateMicros(),0.,duration);
    }
    void add(uint64_t sequence,uint64_t pending,uint64_t consumed,uint64_t windows,double interval,double fftTime,double updateTime,double pathTime,double timerTime) {
        observedHighWater=std::max(observedHighWater,pending);update.add(updateTime);fft.add(fftTime);path.add(pathTime);gap.add(interval);
        if(observations.size()<observations.capacity())observations.push_back({seconds(Clock::now()-epoch)*1000.,interval,fftTime,updateTime,pathTime,timerTime,sequence,pending,consumed,windows});
    }
};
class Heartbeat : private juce::Timer {
public:
    Metric gaps;void begin(){previous=Clock::now();startTimer(10);}void end(){stopTimer();}
private:
    void timerCallback()override{const auto now=Clock::now();gaps.add(seconds(now-previous)*1000.);previous=now;}
    Stamp previous{};
};

void attributionHeader(std::ofstream& output) {
    output<<"case_id,group,repeat,rate,block,stage,pacing,signal,consumer,fft_size,requested_update_ms,requested_duration_s,observed_wall_s,producer_lifetime_s,callbacks,produced_samples,expected_samples_by_wall,production_ratio,within_5pct_nominal_rate,source_status,process_cpu_one_core_percent,producer_thread_cpu_one_core_percent,message_thread_cpu_one_core_percent,generation_total_us,generation_p50_us,generation_p99_us,callback_p50_us,callback_p99_us,callback_max_us,callback_budget_us,callback_execution_overruns,source_starts_over_one_period_late,source_clock_late_max_us,entry_gap_p99_us,entry_gap_max_us,sleep_requested_total_us,sleep_actual_total_us,sleep_actual_p99_us,wake_late_p99_us,wake_late_max_us,deadline_rebases,rebased_source_time_us,catch_up_blocks,max_consecutive_catch_up,initial_pending,capture_expected_samples,consumed_samples,discarded_samples,rejected_samples,pending_samples,sample_accounting_error,sample_flow_status,observed_fifo_high_water,fft_windows,analysis_updates,unobserved_updates,consumer_gap_p50_ms,consumer_gap_p99_ms,consumer_gap_max_ms,fft_total_us,analysis_total_us,analysis_p99_us,analysis_max_us,timer_total_us,timer_p99_us,path_total_us,path_p99_us,panel_paint_calls,panel_paint_total_us,panel_paint_p99_us,panel_paint_max_us,heartbeat_gap_p99_ms,heartbeat_gap_max_ms,dispatch_calls,dispatch_total_us,dispatch_max_us,cf_dispatch_inclusive_us,cf_dispatch_max_us,appkit_poll_inclusive_us,appkit_poll_max_us,appkit_send_inclusive_us,appkit_send_max_us,callback_records,latency_samples,performance_acceptance\n";
}

void runCase(ChimeraProcessor& processor,ChimeraEditor* editor,const Case& config,int id,
             std::ofstream& summary,std::ofstream& callbackRows,std::ofstream& updateRows) {
    const bool native=config.consumer!=Consumer::Direct,capture=config.fft!=0 && config.stage!=Stage::NoOp;
    auto& fifo=processor.graphicalEQ(1).analyzer;
    GraphicalEQPanel* panel=nullptr;std::unique_ptr<EQSpectrumAnalyzer> direct;
    if(native) {
        check(editor!=nullptr,"Native attribution requires an editor");
        panel=find<GraphicalEQPanel>(*editor,"finalEQ_panel");check(panel,"Native attribution EQ panel missing");
        panel->clearDiagnostics();
        auto* master=find<juce::TextButton>(*panel,eqID(1,-1,"analyzer"));check(master,"Native attribution LIVE control missing");
        if(master->getToggleState()){master->triggerClick();settle(20);}
    }
    processor.graphicalEQ(0).analyzer.enabled.store(false);fifo.enabled.store(false);
    processor.prepareToPlay(config.rate,128);
    if(native) {
        auto* resolution=find<juce::ComboBox>(*panel,eqID(1,-1,"resolution"));check(resolution,"Native attribution FFT selector missing");
        if(config.fft)resolution->setSelectedId(config.fft==2048?1:config.fft==4096?2:3,juce::sendNotificationSync);
        auto* master=find<juce::TextButton>(*panel,eqID(1,-1,"analyzer"));
        if(capture){master->triggerClick();settle(50);check(fifo.enabled.load(),"Native attribution capture did not start");
            check(waitFor([&]{return panel->analysisSampleRate()==config.rate;}),"Native attribution sample-rate transition did not finish before production");}
        else settle(30);
    } else {
        direct=std::make_unique<EQSpectrumAnalyzer>();if(config.fft)direct->setFFTSize(config.fft);
        direct->reset(fifo);direct->update(fifo,config.rate);fifo.enabled.store(capture);
    }
    ConsumerMetrics consumer;Heartbeat heartbeat;
    if(native) {
        consumer.previousSequence=panel->analysisUpdates();
        panel->configureDiagnostics(config.interval,config.consumer==Consumer::NativePaint,&ConsumerMetrics::native,&consumer);
    }
    const uint64_t initialPending=fifo.pendingSamples(),beforeRejected=fifo.dropped.load();
    const uint64_t beforeConsumed=native?panel->analysisTotalConsumedSamples():direct->totalConsumedSamples();
    const uint64_t beforeDiscarded=native?panel->analysisDiscardedSamples():direct->discardedSamples();
    const uint64_t beforeWindows=native?panel->fftFrames():direct->frames();
    Producer source(processor,config); // All setup and allocations precede epoch.
    if(native){heartbeat.begin();chimeraTest::beginDispatchTelemetry();}
    const double processStart=processCPUSeconds(),messageStart=threadCPUSeconds();
    const auto epoch=Clock::now();consumer.epoch=epoch;source.start(epoch);
    auto next=epoch+std::chrono::milliseconds(config.interval);
    while(seconds(Clock::now()-epoch)<config.duration && !source.hasFailed()) {
        if(native)settle(1);
        else {
            std::this_thread::sleep_until(next);
            if(capture){const auto started=Clock::now();direct->update(fifo,config.rate);consumer.direct(*direct,micros(Clock::now()-started));}
            next+=std::chrono::milliseconds(config.interval);
        }
        consumer.observedHighWater=std::max(consumer.observedHighWater,uint64_t(fifo.pendingSamples()));
    }
    source.stop();const double observed=seconds(Clock::now()-epoch);
    const double processCPU=100.*(processCPUSeconds()-processStart)/observed,messageCPU=100.*(threadCPUSeconds()-messageStart)/observed;
    heartbeat.end();const auto dispatch=native?chimeraTest::endDispatchTelemetry():chimeraTest::DispatchTelemetry{};
    if(native)panel->clearDiagnostics();
    const uint64_t produced=source.callbacks()*128,captured=capture?produced:0,consumed=(native?panel->analysisTotalConsumedSamples():direct->totalConsumedSamples())-beforeConsumed;
    const uint64_t discarded=(native?panel->analysisDiscardedSamples():direct->discardedSamples())-beforeDiscarded,rejected=fifo.dropped.load()-beforeRejected,pending=fifo.pendingSamples();
    const uint64_t windows=(native?panel->fftFrames():direct->frames())-beforeWindows;
    const int64_t accounting=int64_t(initialPending+captured)-int64_t(consumed+discarded+rejected+pending);
    const double ratio=double(produced)/(observed*config.rate),budget=128./config.rate*1.e6;
    Metric generation(65536),callback(65536),entryGap(65536),sleepActual(65536),wakeLate(65536);
    double sleepRequestedTotal=0.,clockLateMax=0.;uint64_t overruns=0,lateStarts=0,catchUp=0;
    for(size_t n=0;n<source.records().size();++n) {
        const auto& record=source.records()[n];generation.add(record.generationMicros);callback.add(record.processorMicros);
        if(n)entryGap.add(record.entryIntervalMicros);sleepActual.add(record.sleepActualMicros);wakeLate.add(record.wakeLateMicros);
        sleepRequestedTotal+=record.sleepRequestedMicros;clockLateMax=std::max(clockLateMax,record.originalClockLateMicros);
        overruns+=record.processorMicros>budget;lateStarts+=record.scheduleLateMicros>budget;catchUp+=record.catchUp;
        callbackRows<<id<<','<<n<<','<<record.entryMicros<<','<<record.entryIntervalMicros<<','<<record.generationMicros<<','<<record.processorMicros<<','
                    <<record.scheduleLateMicros<<','<<record.originalClockLateMicros<<','<<record.sleepRequestedMicros<<','<<record.sleepActualMicros<<','
                    <<record.wakeLateMicros<<','<<record.rebased<<','<<record.rebaseLostMicros<<','<<record.catchUp<<'\n';
    }
    for(const auto& row:consumer.observations)
        updateRows<<id<<','<<row.elapsedMs<<','<<row.sequence<<','<<row.pending<<','<<row.consumed<<','<<row.windows<<','<<row.gapMs<<','<<row.fftMicros<<','<<row.updateMicros<<','<<row.pathMicros<<','<<row.timerMicros<<'\n';
    const bool nominal=ratio>=.95 && ratio<=1.05;
    const char* sourceStatus=source.hasFailed()?"PRODUCER_ERROR":ratio<.95?"UNDERPRODUCED":ratio>1.05?"OVERPRODUCED":"NEAR_NOMINAL_DIAGNOSTIC";
    const char* flow=accounting?"ACCOUNTING_ERROR":!capture?"CAPTURE_OFF":rejected?"FIFO_REJECTION":discarded?"DISCONTINUITY":windows?"NO_LOSS":"NO_FFT_WINDOW";
    summary<<id<<','<<config.group<<','<<config.repeat<<','<<config.rate<<",128,"<<name(config.stage)<<','<<name(config.pacing)<<','
           <<(config.cachedSignal?"precomputed_same_tone":"per_sample_sin_fmod")<<','<<name(config.consumer)<<','<<config.fft<<','<<config.interval<<','<<config.duration<<','
           <<observed<<','<<source.lifetimeSeconds<<','<<source.callbacks()<<','<<produced<<','<<uint64_t(observed*config.rate)<<','<<ratio<<','<<nominal<<','<<sourceStatus<<','
           <<processCPU<<','<<100.*source.cpuSeconds/observed<<','<<messageCPU<<','<<generation.sum<<','<<generation.quantile(.5)<<','<<generation.quantile(.99)<<','
           <<callback.quantile(.5)<<','<<callback.quantile(.99)<<','<<callback.maximum<<','<<budget<<','<<overruns<<','<<lateStarts<<','<<clockLateMax<<','<<entryGap.quantile(.99)<<','<<entryGap.maximum<<','
           <<sleepRequestedTotal<<','<<sleepActual.sum<<','<<sleepActual.quantile(.99)<<','<<wakeLate.quantile(.99)<<','<<wakeLate.maximum<<','<<source.rebases<<','<<source.rebasedSourceMicros<<','<<catchUp<<','<<source.consecutiveCatchUpMax<<','
           <<initialPending<<','<<captured<<','<<consumed<<','<<discarded<<','<<rejected<<','<<pending<<','<<accounting<<','<<flow<<','<<consumer.observedHighWater<<','<<windows<<','<<consumer.update.count<<','<<consumer.missingUpdates<<','
           <<consumer.gap.quantile(.5)<<','<<consumer.gap.quantile(.99)<<','<<consumer.gap.maximum<<','<<consumer.fft.sum<<','<<consumer.update.sum<<','<<consumer.update.quantile(.99)<<','<<consumer.update.maximum<<','
           <<consumer.timer.sum<<','<<consumer.timer.quantile(.99)<<','<<consumer.path.sum<<','<<consumer.path.quantile(.99)<<','<<consumer.paintCalls<<','<<consumer.paint.sum<<','<<consumer.paint.quantile(.99)<<','<<consumer.paint.maximum<<','
           <<heartbeat.gaps.quantile(.99)<<','<<heartbeat.gaps.maximum<<','<<dispatch.calls<<','<<dispatch.totalMicros<<','<<dispatch.maxMicros<<','<<dispatch.cfMicros<<','<<dispatch.cfMaxMicros<<','<<dispatch.pollMicros<<','<<dispatch.pollMaxMicros<<','<<dispatch.sendMicros<<','<<dispatch.sendMaxMicros<<','
           <<source.records().size()<<','<<processor.getLatencySamples()<<",NOT_ASSESSED\n";
    summary.flush();callbackRows.flush();updateRows.flush();
    std::cout<<"ATTRIBUTION case="<<id<<" group="<<config.group<<" rate="<<config.rate<<" fft="<<config.fft<<" stage="<<name(config.stage)<<" pacing="<<name(config.pacing)
             <<" consumer="<<name(config.consumer)<<" ratio="<<ratio<<" rebases="<<source.rebases<<" callback_p99_us="<<callback.quantile(.99)
             <<" flow="<<flow<<" DAW=NOT_ASSESSED"<<std::endl;
    check(accounting==0,"Attribution FIFO accounting mismatch (raw row preserved)");
    check(source.records().size()==source.callbacks(),"Attribution callback telemetry capacity exceeded");
    check(summary.good() && callbackRows.good() && updateRows.good(),"Attribution evidence write failed");
    fifo.enabled.store(false);
    source.rethrowFailure(); // Preserve the partial diagnostic row before failing.
}

int run(int argc,char** argv) {
    const bool headlessOnly=argc>1 && std::string(argv[1])=="--attribution-headless";
    const juce::File directory=juce::File::getCurrentWorkingDirectory().getChildFile(argc>2?argv[2]:"eq-attribution");
    check(directory.createDirectory().wasOk(),"Cannot create attribution directory");
    const double scale=argc>3?std::stod(argv[3]):1.;check(scale>=.05 && scale<=5.,"Attribution duration scale must be 0.05..5");
    std::ofstream summary(directory.getChildFile("eq-attribution-summary.csv").getFullPathName().toStdString());
    std::ofstream callbacks(directory.getChildFile("eq-attribution-callbacks.csv").getFullPathName().toStdString());
    std::ofstream updates(directory.getChildFile("eq-attribution-updates.csv").getFullPathName().toStdString());
    summary<<std::setprecision(12);callbacks<<std::setprecision(12);updates<<std::setprecision(12);attributionHeader(summary);
    callbacks<<"case_id,callback,entry_offset_us,entry_gap_us,generation_us,process_us,schedule_late_us,original_source_clock_late_us,sleep_requested_us,sleep_actual_us,wake_late_us,rebased,rebase_lost_us,catch_up\n";
    updates<<"case_id,elapsed_ms,update_sequence,pending_snapshot,consumed_samples,fft_windows,consumer_gap_ms,fft_us,update_us,path_us,timer_us\n";
    std::ofstream manifest(directory.getChildFile("eq-attribution-method.txt").getFullPathName().toStdString());
    manifest<<"Synthetic diagnostic source; no hardware clock, audio interface or DAW.\n"
              "Scope="<<(headlessOnly?"headless_direct_22_cases; native UI NOT_RUN":"complete_90_case_synthetic_matrix")<<".\n"
              "Historical functional suite and its producer are unchanged. Duration scale="<<scale<<".\n"
              "Legacy source permanently rebases after >2 periods late; due source retains clock debt and can issue catch-up bursts.\n"
              "Near-nominal catch-up production is NOT proof of real-time callback scheduling. Raw late starts and execution overruns are retained.\n"
              "Process/producer/message CPU are actual CPU-time deltas as percent of one core; FFT/update/path times are inclusive wall durations.\n"
              "macOS CF/AppKit timings include their dispatched callbacks; do not sum them with analyzer/paint times.\n"
              "Native no-panel-paint suppresses the EQ panel rendering only; host editor controls and the real message loop remain.\n"
              "Producer signal generation, buffer fills, sleeps and timing records are test-only, outside the product processBlock.\n"
              "All waveform/cache/buffer/telemetry allocations precede measured production. CSV writes occur after joining the producer.\n"
              "Observed FIFO high-water is sampled, not a claim to observe the instantaneous maximum. Pending samples at stop are not losses.\n"
              "DAW/performance acceptance remains NOT_ASSESSED in every row.\n";
    auto processor=std::make_unique<ChimeraProcessor>();
    for(int e=0;e<2;++e){set(*processor,eqID(e,-1,"bypass"),0);for(int b=0;b<graphicalEQBands;++b)set(*processor,eqID(e,b,"enabled"),b==0?1.f:0.f);set(*processor,eqID(e,0,"frequency"),1007.8125f);set(*processor,eqID(e,0,"gain"),6);}
    std::vector<Case> directCases,nativeCases;
    const auto add=[&](std::vector<Case>& list,const char* group,double rate,int fft,int interval,Stage stage,Pacing pacing,Consumer consumer,double duration,bool cached=false,int repeat=0){list.push_back({group,rate,fft,interval,stage,pacing,consumer,cached,duration*scale,repeat});};
    // Stage and source-clock controls have no native event pump or paint load.
    for(auto stage:{Stage::NoOp,Stage::EQPair,Stage::Full})for(auto pacing:{Pacing::LegacyRebase,Pacing::DueCatchUp})
        add(directCases,"stage_clock_control",192000,0,40,stage,pacing,Consumer::Direct,2.);
    for(double rate:{44100.,48000.,96000.})for(auto pacing:{Pacing::LegacyRebase,Pacing::DueCatchUp})
        add(directCases,"source_rate_control",rate,0,40,Stage::Full,pacing,Consumer::Direct,1.2);
    for(auto stage:{Stage::NoOp,Stage::Full})for(auto pacing:{Pacing::LegacyRebase,Pacing::DueCatchUp})
        add(directCases,"generator_control",192000,0,40,stage,pacing,Consumer::Direct,2.,true);
    for(int fft:{2048,4096,8192})for(auto pacing:{Pacing::LegacyRebase,Pacing::DueCatchUp})
        add(directCases,"consumer_control",192000,fft,40,Stage::Full,pacing,Consumer::Direct,2.);
    for(int fft:{0,2048,4096,8192})for(auto pacing:{Pacing::LegacyRebase,Pacing::DueCatchUp})for(auto mode:{Consumer::NativeNoPanelPaint,Consumer::NativePaint})
        add(nativeCases,"consumer_control",192000,fft,40,Stage::Full,pacing,mode,2.);
    for(double rate:{44100.,48000.,96000.,192000.})for(int fft:{0,2048,4096,8192})for(int interval:{20,40,80})
        add(nativeCases,"rate_fft_cadence",rate,fft,interval,Stage::Full,Pacing::DueCatchUp,Consumer::NativePaint,1.2);
    for(int repeat=1;repeat<=2;++repeat)for(auto pacing:{Pacing::LegacyRebase,Pacing::DueCatchUp})
        add(nativeCases,"native_off_repeat",192000,0,40,Stage::Full,pacing,Consumer::NativePaint,2.,false,repeat);
    std::cout<<"ATTRIBUTION opt-in matrix direct="<<directCases.size()<<" native="<<(headlessOnly?0:nativeCases.size())<<" duration_scale="<<scale<<"; DAW=NOT_ASSESSED"<<std::endl;
    int id=0;for(const auto& config:directCases)runCase(*processor,nullptr,config,++id,summary,callbacks,updates);
    if(headlessOnly){processor->releaseResources();check(manifest.good(),"Attribution method manifest write failed");
        std::cout<<"PASS headless attribution accounting; cases="<<id<<"; native_ui=NOT_RUN; performance_acceptance=NOT_ASSESSED"<<std::endl;return 0;}
    auto editor=std::make_unique<ChimeraEditor>(*processor);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(60);
    auto* eqTab=tab(*editor,"EQ");check(eqTab,"Attribution EQ tab missing");eqTab->triggerClick();settle(40);
    auto* finalBank=find<juce::TextButton>(*editor,"path.final.eq");check(finalBank,"Attribution final EQ selector missing");finalBank->triggerClick();
    check(waitFor([&]{auto* panel=find<GraphicalEQPanel>(*editor,"finalEQ_panel");return panel!=nullptr && panel->isShowing();}),"Attribution final EQ panel is hidden");
    for(const auto& config:nativeCases)runCase(*processor,editor.get(),config,++id,summary,callbacks,updates);
    editor.reset();processor->releaseResources();
    check(manifest.good(),"Attribution method manifest write failed");
    std::cout<<"PASS attribution sample-accounting instrumentation; cases="<<id<<"; performance_acceptance=NOT_ASSESSED"<<std::endl;return 0;
}
} // namespace eqAttribution
} // namespace
