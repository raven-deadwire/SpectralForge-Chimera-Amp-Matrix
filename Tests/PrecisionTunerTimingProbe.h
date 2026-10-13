#pragma once
#include "PrecisionTunerUITests.h"
#include "CabTimingDiagnostics.h"
#include <fstream>
#include <iomanip>
#include <sstream>

// Diagnostic, not a DAW/driver qualification gate. Keep every callback,
// including warm-up, and pair wall time with the same callback's thread clock.
namespace precisionTunerTimingProbe {
using namespace nativeUITests;
inline void run(const juce::File& directory) {
    std::cout<<"PROVENANCE source_sha="<<CHIMERA_SOURCE_SHA<<'\n';
    std::ofstream csv(directory.getChildFile("tuner-callbacks.csv").getFullPathName().toStdString());
    require(bool(csv),"Cannot write tuner callback observations");
    csv<<"repeat,rate,block,editor,tuner,index,warmup,wall_us,budget_us,thread_cpu_us,thread_cycles,late_start_us,cpu_valid,cycles_valid,clock_begin_error,clock_end_error\n"<<std::setprecision(12);
    for(int repeat=0;repeat<2;++repeat)for(const auto config:{std::pair{48000,256},std::pair{96000,64}})
    for(bool visible:{false,true})for(int pair=0;pair<2;++pair) {
        const bool active=(pair==0)==(repeat==0); // ON/OFF then OFF/ON.
        const auto [rate,block]=config;
        auto p=std::make_unique<ChimeraProcessor>();
        set(*p,"tuneron",active?1.f:0.f);set(*p,"tunermute",0.f);
        p->prepareToPlay(rate,block);
        std::unique_ptr<ChimeraEditor> editor;
        if(visible){editor=std::make_unique<ChimeraEditor>(*p);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);find<juce::TextButton>(*editor,"tunerOpen")->triggerClick();settle(100);}
        const int warmup=rate/block;constexpr int measured=1600;
        struct Row {double wall,late;cabTiming::ThreadClock begin,end;};
        std::vector<Row> rows(size_t(warmup+measured));
        juce::AudioBuffer<float> input(2,(warmup+measured)*block),audio(2,block);juce::MidiBuffer midi;
        for(int n=0;n<input.getNumSamples();++n){const double phase=juce::MathConstants<double>::twoPi*27.5*n/rate;const float x=float(.2*std::sin(phase)+.1*std::sin(2*phase));input.setSample(0,n,x);input.setSample(1,n,x);}
        std::atomic<bool> done{false};std::string callerScheduling;
        std::jthread feeder([&]{
            std::ostringstream scheduling;cabTiming::appendCallerScheduling(scheduling);callerScheduling=scheduling.str();
            const auto epoch=std::chrono::steady_clock::now();
            for(size_t i=0;i<rows.size();++i){
                const auto due=epoch+std::chrono::nanoseconds(int64_t(double(i)*block/rate*1e9));
                std::this_thread::sleep_until(due); // Retain clock debt; never discard/rebase source time.
                for(int c=0;c<2;++c)audio.copyFrom(c,0,input,c,int(i)*block,block);
                const auto cpuBegin=cabTiming::readThreadClock();const auto start=std::chrono::steady_clock::now();
                p->processBlock(audio,midi);
                const auto end=std::chrono::steady_clock::now();const auto cpuEnd=cabTiming::readThreadClock();
                rows[i]={std::chrono::duration<double,std::micro>(end-start).count(),std::chrono::duration<double,std::micro>(start-due).count(),cpuBegin,cpuEnd};
            }
            done.store(true);
        });
        while(!done.load())settle(20);
        feeder.join();
        std::vector<double> wall;wall.reserve(measured);int misses=0,cpuCount=0,cycleCount=0;double totalCPU=0,totalCycles=0;const double budget=1e6*block/rate;
        for(size_t i=0;i<rows.size();++i){const auto& r=rows[i];cabTiming::Sample sample{i,r.wall,r.begin,r.end};
            csv<<repeat<<','<<rate<<','<<block<<','<<visible<<','<<active<<','<<i<<','<<(i<size_t(warmup))<<','<<r.wall<<','<<budget<<','<<sample.cpuUs()<<','<<sample.cycles()<<','<<r.late<<','<<sample.cpuValid()<<','<<sample.cyclesValid()<<','<<r.begin.error<<','<<r.end.error<<'\n';
            if(i>=size_t(warmup)){wall.push_back(r.wall);misses+=r.wall>budget;if(sample.cpuValid()){totalCPU+=sample.cpuUs();++cpuCount;}if(sample.cyclesValid()){totalCycles+=double(sample.cycles());++cycleCount;}}}
        std::sort(wall.begin(),wall.end());
        std::cout<<"OBSERVE tuner timing repeat="<<repeat<<" rate="<<rate<<" block="<<block<<" editor="<<visible<<" tuner="<<active<<" p50_us="<<wall[800]<<" p99_us="<<wall[1584]<<" max_us="<<wall.back()<<" budget_us="<<budget<<" misses="<<misses<<"/1600 thread_cpu_mean_us="<<(cpuCount?totalCPU/cpuCount:-1)<<" cpu_valid_count="<<cpuCount<<" thread_cycle_mean="<<(cycleCount?totalCycles/cycleCount:-1)<<" cycles_valid_count="<<cycleCount<<" displayed_peak="<<p->cpuPeakLoad()<<callerScheduling<<'\n';
        editor.reset();p->releaseResources();require(p->backgroundResourcesReleased(),"Timing probe worker teardown failed");
    }
    require(bool(csv),"Tuner callback observations could not be saved");
}
}
