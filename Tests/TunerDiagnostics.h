#pragma once
// Opt-in diagnostic build only. No clocks, storage or hooks in release builds.
#include "CabTimingDiagnostics.h"
#include <array>
#include <chrono>
namespace tunerDiagnostics {
using Clock=std::chrono::steady_clock;
inline double nowUs(){return std::chrono::duration<double,std::micro>(Clock::now().time_since_epoch()).count();}
inline cabTiming::ThreadClock readClock(){
    auto value=cabTiming::readThreadClock();
#if defined(_WIN32)
    FILETIME created{},exited{},kernel{},user{};
    if(GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user)){
        const auto ticks=[](FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
        value.cpuUs=double(ticks(kernel)+ticks(user))*.1;
    }else value.error=GetLastError();
#endif
    return value;
}
struct Push {double wallUs{};cabTiming::ThreadClock begin,end;};
inline thread_local Push lastPush;
struct PushScope {
    double start;cabTiming::ThreadClock cpu;
    PushScope():start(nowUs()),cpu(readClock()){}
    ~PushScope(){lastPush={nowUs()-start,cpu,readClock()};}
};
struct Event {
    double beginUs{},endUs{};cabTiming::ThreadClock beginCPU,endCPU;
    int queued{},discarded{},filled{};unsigned generation{};
    uint64_t newestInputFrame{};float hz{};bool analysed{},published{};
};
struct Trace {
    std::array<Event,4096> events{};size_t size{},dropped{};
    std::array<uint64_t,16384> inputFrames{};uint64_t inputFrame{},newestInputFrame{};
    void append(const Event& e){if(size<events.size())events[size++]=e;else ++dropped;}
};
}
