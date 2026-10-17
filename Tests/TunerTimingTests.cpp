#include "TunerTiming.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace spectralforge::tuning;
void check(bool ok) { if(!ok)throw std::runtime_error("Timing attribution contract failed"); }
int main() { try {
    for(double wake:{90.,100.,112.}) {
        TimingObservation r{7,100,wake,115,116,123,124,110,.99f,true};
        check(r.wakeDelay()>=0 && r.serviceDelay()>=0);
        check(r.compute()==7 && r.publishAge()==24);
        check(r.computeCpu()==-1);
        r.cpuBegin=20;r.cpuEnd=22;check(r.computeCpu()==2);
        check(r.compute()-r.computeCpu()==5);
        r.cpuEnd=19;check(r.computeCpu()==-1);
        check(std::abs(r.wakeDelay()+r.serviceDelay()+r.preparation()+r.compute()+r.publication()-r.publishAge())<1e-10);
    }
    TimingTrace trace;
    for(std::size_t i=0;i<TimingTrace::capacity+3;++i)trace.append({unsigned(i)});
    check(trace.size==TimingTrace::capacity && trace.dropped==3);
    check(trace.rows.front().generation==0 && trace.rows.back().generation==TimingTrace::capacity-1);
    trace.reset();check(trace.size==0 && trace.dropped==0);
    trace.append({99});check(trace.size==1 && trace.rows[0].generation==99);
    std::cout<<"PASS timing decomposition, wake/input overlap, bounded retention and reset\n";
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; } }
