#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <ctime>

namespace spectralforge::tuning {
inline double workerCpuMilliseconds() noexcept {
#if defined(__APPLE__) || defined(__linux__)
    timespec value{};
    if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0)
        return double(value.tv_sec)*1000.+double(value.tv_nsec)*1.e-6;
#endif
    return -1.;
}
// Test-only, worker-owned storage. Inspect only after stop() joins the worker.
// Times share JUCE's monotonic millisecond clock; no audio-device timestamp is
// implied. windowComplete is the arrival of the selected window's final block.
struct TimingObservation {
    unsigned generation{};
    double windowComplete{}, wake{}, service{}, estimateBegin{}, estimateEnd{}, publish{};
    float hz{}, confidence{};
    bool published{};
    double cpuBegin{-1},cpuEnd{-1};
    double computeCpu() const { return cpuBegin>=0 && cpuEnd>=cpuBegin?cpuEnd-cpuBegin:-1.; }
    double wakeDelay() const { return std::max(0.0,wake-windowComplete); }
    // A block may arrive after wake but before the FIFO snapshot. Assign that
    // overlap to service, never a negative wake delay.
    double serviceDelay() const { return service-std::max(wake,windowComplete); }
    double preparation() const { return estimateBegin-service; }
    double compute() const { return estimateEnd-estimateBegin; }
    double publication() const { return publish-estimateEnd; }
    double publishAge() const { return publish-windowComplete; }
};
struct TimingTrace {
    static constexpr std::size_t capacity=2048;
    std::array<TimingObservation,capacity> rows{};
    std::size_t size{}, dropped{};
    void append(const TimingObservation& row) {
        if(size<capacity)rows[size++]=row;else ++dropped;
    }
    void reset() { size=dropped=0; }
};
}
