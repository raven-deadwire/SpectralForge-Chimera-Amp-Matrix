#include "PluginEditor.h"
#include "PrecisionTunerPanel.h"
#include "NativeMessageLoop.h"
#include "ChimeraBuildVersion.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <filesystem>
#include <numeric>
#include <sstream>
using Clock=std::chrono::steady_clock;
static void set(ChimeraProcessor& p,const char* id,float v){auto* parameter=p.parameters().getParameter(id);if(!parameter)throw std::runtime_error(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(v));}
static void pump(int ms){if(!chimeraTest::dispatchFor(ms))throw std::runtime_error("Message pump stopped");}
struct Row{double wall{},late{},push{},hz{};cabTiming::ThreadClock begin,end,pushBegin,pushEnd;};
int main(int argc,char** argv){try{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto directory=argc>1?juce::File(argv[1]):juce::File::getCurrentWorkingDirectory().getChildFile("tuner-realtime");
    if(!directory.createDirectory().wasOk())throw std::runtime_error("Cannot create evidence directory");
    std::cout<<"PROVENANCE source_sha="<<CHIMERA_TUNER_TEST_SOURCE_SHA<<" diagnostics=1 fixture=production_default_A0 scheduling=normal_priority_no_driver\n";
    std::ofstream csv(directory.getChildFile("callbacks.csv").getFullPathName().toStdString()),summaries(directory.getChildFile("summary.csv").getFullPathName().toStdString()),workers(directory.getChildFile("workers.csv").getFullPathName().toStdString());
    csv<<std::setprecision(12)<<"repeat,rate,block,view,tuner,index,warmup,wall_us,budget_us,cpu_us,cycles,late_us,finish_late_us,push_wall_us,push_cpu_us,hz\n";
    summaries<<std::setprecision(12)<<"repeat,rate,block,view,tuner,count,wall_mean_us,wall_p99_us,wall_max_us,max_paired_cpu_us,cpu_mean_us,cpu_max_us,wall_misses,cpu_misses,finish_misses,nonfinite,zero_blocks,peak,max_sample_jump,reference_max_diff,trace_dropped\n";
    workers<<std::setprecision(12)<<"repeat,rate,block,view,tuner,index,begin_us,end_us,cpu_us,queued,discarded,filled,generation,newest_input_frame,hz,analysed,published\n";
    int timingFailures=0,signalFailures=0;
    for(const auto config:{std::pair{48000,256},std::pair{96000,64}}){
        const auto [rate,block]=config;const int warmup=rate/block,measured=1600,count=warmup+measured;
        juce::AudioBuffer<float> reference;
        for(int repeat=0;repeat<2;++repeat)for(int state=0;state<8;++state){
            const int scenario=repeat?7-state:state,view=scenario/2;const bool active=scenario%2;
            auto p=std::make_unique<ChimeraProcessor>();set(*p,"tuneron",active?1.f:0.f);set(*p,"tunermute",0.f);set(*p,"tunerDisplay",view==2?1.f:0.f);
            p->prepareToPlay(rate,block);
            std::unique_ptr<ChimeraEditor> editor;std::unique_ptr<PrecisionTunerPanel> panel;
            if(view){editor=std::make_unique<ChimeraEditor>(*p);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);}
            if(view>=2){panel=std::make_unique<PrecisionTunerPanel>(*p);panel->addToDesktop(juce::ComponentPeer::windowIsTemporary);panel->setVisible(true);}
            pump(100);
            std::vector<Row> rows(size_t(count),Row{});juce::AudioBuffer<float> source(2,count*block),rendered(2,count*block),audio(2,block);juce::MidiBuffer midi;
            for(int n=0;n<source.getNumSamples();++n){const double phase=juce::MathConstants<double>::twoPi*27.5*n/rate;const float x=float(.2*std::sin(phase)+.1*std::sin(2*phase));for(int c=0;c<2;++c)source.setSample(c,n,x);}
            std::atomic<bool> done{false};double epochUs=0;std::string scheduling;
            std::jthread feeder([&]{
                std::ostringstream context;cabTiming::appendCallerScheduling(context);scheduling=context.str();
                const auto epoch=Clock::now();epochUs=tunerDiagnostics::nowUs();
                for(int i=0;i<count;++i){
                    const auto due=epoch+std::chrono::nanoseconds(int64_t(double(i)*block/rate*1e9));std::this_thread::sleep_until(due);
                    for(int c=0;c<2;++c)audio.copyFrom(c,0,source,c,i*block,block);
                    auto&r=rows[size_t(i)];r.begin=tunerDiagnostics::readClock();const auto start=Clock::now();r.late=std::chrono::duration<double,std::micro>(start-due).count();
                    p->processBlock(audio,midi);
                    const auto end=Clock::now();r.end=tunerDiagnostics::readClock();r.wall=std::chrono::duration<double,std::micro>(end-start).count();r.hz=p->tuningFrequency();const auto push=tunerDiagnostics::lastPush;r.push=push.wallUs;r.pushBegin=push.begin;r.pushEnd=push.end;
                    for(int c=0;c<2;++c)rendered.copyFrom(c,i*block,audio,c,0,block);
                }
                done.store(true);
            });
            while(!done.load())pump(10);feeder.join();
            panel.reset();editor.reset();p->releaseResources();
            if(!p->backgroundResourcesReleased())throw std::runtime_error("Worker teardown failed");
            const double budget=1e6*block/rate;double sum=0,cpuSum=0,cpuMax=0;int wallMisses=0,cpuMisses=0,finishMisses=0;std::vector<double> walls;size_t maxIndex=0;
            for(size_t i=0;i<rows.size();++i){const auto&r=rows[i];const cabTiming::Sample s{i,r.wall,r.begin,r.end},push{i,r.push,r.pushBegin,r.pushEnd};
                csv<<repeat<<','<<rate<<','<<block<<','<<view<<','<<active<<','<<i<<','<<(i<size_t(warmup))<<','<<r.wall<<','<<budget<<','<<s.cpuUs()<<','<<s.cycles()<<','<<r.late<<','<<r.late+r.wall-budget<<','<<r.push<<','<<push.cpuUs()<<','<<r.hz<<'\n';
                // Includes startup, without resetting peaks or hiding warm-up misses.
                sum+=r.wall;cpuSum+=s.cpuUs();cpuMax=std::max(cpuMax,s.cpuUs());walls.push_back(r.wall);if(r.wall>rows[maxIndex].wall)maxIndex=i;
                wallMisses+=r.wall>budget;cpuMisses+=s.cpuUs()>budget;finishMisses+=r.late+r.wall>budget;
            }
            std::sort(walls.begin(),walls.end());const auto&m=rows[maxIndex];const cabTiming::Sample pair{maxIndex,m.wall,m.begin,m.end};
            int nonfinite=0,zeroBlocks=0;double peak=0,jump=0,diff=0; // Output comparison is separate from deadline verdict.
            if(reference.getNumSamples()==0)reference.makeCopyOf(rendered);
            for(int i=0;i<count;++i){bool allZero=true;for(int c=0;c<2;++c)for(int n=i*block;n<(i+1)*block;++n){const float x=rendered.getSample(c,n);nonfinite+=!std::isfinite(x);allZero&=x==0;peak=std::max(peak,double(std::abs(x)));if(n)jump=std::max(jump,double(std::abs(x-rendered.getSample(c,n-1))));diff=std::max(diff,double(std::abs(x-reference.getSample(c,n))));}if(i>=warmup)zeroBlocks+=allZero;}
            const auto& trace=p->tunerTrace();
            for(size_t i=0;i<trace.size;++i){const auto&e=trace.events[i];cabTiming::Sample s{i,e.endUs-e.beginUs,e.beginCPU,e.endCPU};workers<<repeat<<','<<rate<<','<<block<<','<<view<<','<<active<<','<<i<<','<<e.beginUs-epochUs<<','<<e.endUs-epochUs<<','<<s.cpuUs()<<','<<e.queued<<','<<e.discarded<<','<<e.filled<<','<<e.generation<<','<<e.newestInputFrame<<','<<e.hz<<','<<e.analysed<<','<<e.published<<'\n';}
            summaries<<repeat<<','<<rate<<','<<block<<','<<view<<','<<active<<','<<count<<','<<sum/count<<','<<walls[size_t(count*.99)]<<','<<walls.back()<<','<<pair.cpuUs()<<','<<cpuSum/count<<','<<cpuMax<<','<<wallMisses<<','<<cpuMisses<<','<<finishMisses<<','<<nonfinite<<','<<zeroBlocks<<','<<peak<<','<<jump<<','<<diff<<','<<trace.dropped<<'\n';summaries.flush();csv.flush();workers.flush();
            std::cout<<"OBSERVE repeat="<<repeat<<" rate="<<rate<<" block="<<block<<" view="<<view<<" tuner="<<active<<" max_wall_us="<<walls.back()<<" paired_cpu_us="<<pair.cpuUs()<<" cpu_max_us="<<cpuMax<<" wall_misses="<<wallMisses<<" finish_misses="<<finishMisses<<" output_diff="<<diff<<scheduling<<std::endl;
            timingFailures+=wallMisses>0||finishMisses>0;signalFailures+=nonfinite>0||zeroBlocks>0||diff>1e-6||trace.dropped>0;
            if(rate==48000&&repeat==0){
                auto stream=directory.getChildFile("output-view"+juce::String(view)+"-tuner"+juce::String(int(active))+".wav").createOutputStream();juce::WavAudioFormat format;auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.get(),rate,2,24,{},0));if(!writer)throw std::runtime_error("WAV creation failed");stream.release();if(!writer->writeFromAudioSampleBuffer(rendered,0,rendered.getNumSamples()))throw std::runtime_error("WAV write failed");
            }
        }
    }
    if(!csv||!workers||!summaries)throw std::runtime_error("Evidence write failed");
    std::cout<<"VERDICT timing_failed_scenarios="<<timingFailures<<" signal_failed_scenarios="<<signalFailures<<" release_approved=false physical_audio=NOT_TESTED\n";
    return timingFailures||signalFailures?1:0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 2;}}
