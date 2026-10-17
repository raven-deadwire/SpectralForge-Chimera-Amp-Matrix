// Diagnostic only: a same-executable kernel comparison, not a realtime gate.
// Build: c++ -std=c++17 -O3 -DNDEBUG -I Source Tools/tuner_refinement_benchmark.cpp -o tuner-bench
// Run: ./tuner-bench output.csv
// Linux-only: std::clock is process CPU here; do not mislabel the Windows clock.
#if !defined(__linux__)
#error "This diagnostic is Linux-only; native Windows/macOS qualification is separate."
#endif
#include "../Tests/PrecisionTunerInvariantTests.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#if defined(_MSC_VER)
#define TUNER_NOINLINE __declspec(noinline)
#else
#define TUNER_NOINLINE __attribute__((noinline))
#endif
using Estimate=std::pair<float,float>;
TUNER_NOINLINE Estimate baseline(const float* x,int n,double r){return spectralforge_tuner52f7::tuning::estimate(x,n,r);}
TUNER_NOINLINE Estimate candidate(const float* x,int n,double r){return spectralforge::tuning::estimate(x,n,r);}
struct Fixture {double rate,hz;int kind;std::array<float,4096> samples;};
volatile double observed=0;
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Usage: tuner-bench output.csv");
    std::ofstream out(argv[1]);if(!out)throw std::runtime_error("Cannot open output CSV");
    out<<"phase,round,fixture,rate,hz,kind,position,variant,repetitions,wall_us,process_cpu_us\n"<<std::setprecision(17);
    std::vector<Fixture> fixtures;
    for(double rate:{22050.,24000.,24500.})for(double hz:{16.352,27.5,30.8677,41.2034,82.4069,110.,440.,1318.51})for(int kind=0;kind<2;++kind){
        Fixture f{rate,hz,kind,{}};
        for(size_t n=0;n<f.samples.size();++n){
            const double t=n/rate,p=6.283185307179586*hz*t+.7;
            f.samples[n]=float(kind==0?.2*std::sin(p)+.1*std::sin(2*p)+.07*std::sin(3*p)
                             :(.015*std::sin(p)+.2*std::sin(2*p))*std::exp(-3*t)+.17);
        }
        spectralforge::tuning::tests::compareFrozen(candidate(f.samples.data(),4096,rate),f.samples.data(),4096,rate);
        fixtures.push_back(f);
    }
    const auto run=[&](const char* phase,int round,size_t index,int position,char variant,int repetitions){
        const auto& f=fixtures[index];auto function=variant=='A'?baseline:candidate;
        double checksum=0;const auto cpu0=std::clock();const auto wall0=std::chrono::steady_clock::now();
        for(int i=0;i<repetitions;++i){const auto r=function(f.samples.data(),4096,f.rate);checksum+=r.first+r.second;}
        const auto wall1=std::chrono::steady_clock::now();const auto cpu1=std::clock();observed=checksum;
        if(cpu0==std::clock_t(-1)||cpu1==std::clock_t(-1)||cpu1<cpu0)throw std::runtime_error("Invalid process CPU clock");
        out<<phase<<','<<round<<','<<index<<','<<f.rate<<','<<f.hz<<','<<f.kind<<','<<position<<','<<variant<<','<<repetitions<<','
           <<std::chrono::duration<double,std::micro>(wall1-wall0).count()<<','<<double(cpu1-cpu0)*1e6/CLOCKS_PER_SEC<<'\n';
        if(!out)throw std::runtime_error("CSV write failed");
    };
    for(size_t i=0;i<fixtures.size();++i){run("warmup",-1,i,0,'A',1);run("warmup",-1,i,1,'B',1);}
    std::vector<size_t> order(fixtures.size());std::iota(order.begin(),order.end(),size_t{0});std::mt19937 random(20261014);
    for(int round=0;round<8;++round){
        std::shuffle(order.begin(),order.end(),random);const std::string arms=round%2==0?"ABBA":"BAAB";
        for(const auto index:order)for(int position=0;position<4;++position)run("measured",round,index,position,arms[size_t(position)],4);
    }
    out.flush();if(!out)throw std::runtime_error("CSV flush failed");
    std::cout<<"COMPLETE 48 fixtures / 1536 measured rows / 6144 measured estimates / 96 warmup rows; no performance acceptance\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
