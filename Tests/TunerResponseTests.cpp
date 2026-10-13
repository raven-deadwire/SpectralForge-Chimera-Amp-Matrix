#include "GlobalDSP.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <filesystem>
using Clock=std::chrono::steady_clock;
int main(int argc,char** argv){
    const std::filesystem::path dir=argc>1?argv[1]:"tuner-response";std::filesystem::create_directories(dir);
    std::ofstream callbacks(dir/"push.csv"),worker(dir/"worker.csv"),summary(dir/"response.csv");
    callbacks<<std::setprecision(12)<<"repeat,continuous,rate,block,index,input_frame,begin_us,end_us,wall_us,cpu_us,cycles,late_us,hz\n";
    worker<<std::setprecision(12)<<"repeat,continuous,rate,block,index,begin_us,end_us,wall_us,cpu_us,queued,discarded,filled,generation,newest_input_frame,hz,analysed,published\n";
    summary<<std::setprecision(12)<<"repeat,continuous,rate,block,acquire_ms,step_ms,final_cents,trace_dropped,pass\n";
    int failures=0;
    for(int repeat=0;repeat<3;++repeat)for(int continuous:{0,1}){
        constexpr int rate=48000,block=256;const double hz=82.406889,changed=hz*std::exp2(10./1200);
        spectralforge::Tuner tuner;tuner.prepare(rate);juce::AudioBuffer<float> audio(2,block);
        struct Row{double begin,end,late,hz;cabTiming::ThreadClock cpu0,cpu1;};
        constexpr int count=(rate+block-1)/block;std::array<Row,count> rows{};
        auto start=Clock::now();const double epoch=tunerDiagnostics::nowUs();int stable=0;double acquire=-1,settle=-1,phase=.73;
        for(int i=0;i<count;++i){
            const int offset=i*block;const bool stepped=offset>=rate*.30;const double wanted=stepped?changed:hz;
            for(int n=0;n<block;++n){const double phi=continuous?phase:6.283185307179586*wanted*(offset+n)/rate+.73;const float x=float(.2*std::sin(phi)+.1*std::sin(2*phi)+.07*std::sin(3*phi));audio.setSample(0,n,x);audio.setSample(1,n,-.7f*x);phase+=6.283185307179586*wanted/rate;}
            auto& row=rows[size_t(i)];row.cpu0=tunerDiagnostics::readClock();row.begin=tunerDiagnostics::nowUs();row.late=row.begin-epoch-double(offset)*1e6/rate;
            tuner.push(audio,true);row.end=tunerDiagnostics::nowUs();row.cpu1=tunerDiagnostics::readClock();
            std::this_thread::sleep_until(start+std::chrono::nanoseconds(int64_t(double(offset+block)*1e9/rate)));
            row.hz=tuner.frequency();const double elapsed=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
            const double cents=row.hz>0?1200*std::log2(row.hz/wanted):99999;
            if(!stepped&&std::abs(cents)<.5&&tuner.confidence()>.9&&acquire<0)acquire=elapsed;
            if(stepped){if(std::abs(cents)<.5&&tuner.confidence()>.9)++stable;else stable=0;if(stable>=3&&settle<0)settle=elapsed-300.;}
        }
        const double finalHz=tuner.frequency();tuner.stop();
        const double finalCents=finalHz>0?1200*std::log2(finalHz/changed):99999;
        const bool pass=acquire>0&&acquire<290&&settle>0&&settle<290&&std::abs(finalCents)<.1&&tuner.diagnosticTrace->dropped==0;
        failures+=!pass;summary<<repeat<<','<<continuous<<','<<rate<<','<<block<<','<<acquire<<','<<settle<<','<<finalCents<<','<<tuner.diagnosticTrace->dropped<<','<<pass<<'\n';
        for(size_t i=0;i<rows.size();++i){const auto&r=rows[i];const cabTiming::Sample s{i,r.end-r.begin,r.cpu0,r.cpu1};callbacks<<repeat<<','<<continuous<<','<<rate<<','<<block<<','<<i<<','<<(i+1)*block<<','<<r.begin-epoch<<','<<r.end-epoch<<','<<s.wallUs<<','<<s.cpuUs()<<','<<s.cycles()<<','<<r.late<<','<<r.hz<<'\n';}
        for(size_t i=0;i<tuner.diagnosticTrace->size;++i){const auto&e=tuner.diagnosticTrace->events[i];const cabTiming::Sample s{i,e.endUs-e.beginUs,e.beginCPU,e.endCPU};worker<<repeat<<','<<continuous<<','<<rate<<','<<block<<','<<i<<','<<e.beginUs-epoch<<','<<e.endUs-epoch<<','<<s.wallUs<<','<<s.cpuUs()<<','<<e.queued<<','<<e.discarded<<','<<e.filled<<','<<e.generation<<','<<e.newestInputFrame<<','<<e.hz<<','<<e.analysed<<','<<e.published<<'\n';}
        std::cout<<"E2 repeat="<<repeat<<" continuous="<<continuous<<" acquire_ms="<<acquire<<" step_ms="<<settle<<" final_cents="<<finalCents<<" pass="<<pass<<std::endl;
    }
    if(!callbacks||!worker||!summary)return 2;
    return failures?1:0;
}
