#pragma once

// Test-only paired evidence. Both thread-clock reads belong outside the
// existing steady_clock interval; neither these calls nor CSV I/O is DSP work.
#if defined(_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0600
 #endif
 #include <windows.h>
 #include <realtimeapiset.h>
#elif defined(__APPLE__)
 #include <sys/qos.h>
#else
 #include <pthread.h>
#endif

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace cabTiming
{
constexpr size_t measuredCallbacks = 1600;
constexpr size_t workerCallbacks = 1200;
constexpr size_t automationCallbacks = 80;

// Read-only execution context, queried before or after the complete measured
// callback population. A shell/environment collector cannot stand in for the
// priority of the actual CTest process and its calling thread.
inline void appendCallerScheduling(std::ostream& out)
{
#if defined(_WIN32)
    const auto processClass = ::GetPriorityClass(::GetCurrentProcess());
    const auto threadPriority = ::GetThreadPriority(::GetCurrentThread());
    DWORD_PTR processMask = 0, systemMask = 0;
    const bool affinityValid = ::GetProcessAffinityMask(::GetCurrentProcess(), &processMask, &systemMask) != 0;
    out << " caller_context=windows_native"
        << " caller_process_priority_class=" << processClass
        << " caller_thread_priority=" << threadPriority
        << " caller_priority_valid=" << (processClass != 0 && threadPriority != THREAD_PRIORITY_ERROR_RETURN)
        << " caller_process_affinity_mask=" << static_cast<uint64_t>(processMask)
        << " caller_affinity_valid=" << affinityValid;
#elif defined(__APPLE__)
    // Base QoS is an observation, not a trace of temporary OS priority boosts.
    out << " caller_context=macos_base_qos caller_thread_qos=" << static_cast<unsigned>(qos_class_self());
#else
    int policy = 0;
    sched_param parameters{};
    const auto error = pthread_getschedparam(pthread_self(), &policy, &parameters);
    out << " caller_context=pthread_native caller_policy=" << policy
        << " caller_thread_priority=" << parameters.sched_priority << " caller_query_error=" << error;
#endif
}

struct ThreadClock
{
    double cpuUs = -1;
    uint64_t cycles = 0;
    bool cyclesValid = false;
    unsigned long error = 0;
};

inline ThreadClock readThreadClock() noexcept
{
    ThreadClock result;
#if defined(_WIN32)
    // User + kernel CPU cycles. Microsoft explicitly prohibits conversion to
    // elapsed time: timer behavior can depend on the CPU implementation.
    // https://learn.microsoft.com/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime
    ULONG64 cycles = 0;
    if (::QueryThreadCycleTime(::GetCurrentThread(), &cycles))
    {
        result.cycles = cycles;
        result.cyclesValid = true;
    }
    else result.error = ::GetLastError();
#else
    timespec value{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) == 0)
        result.cpuUs = double(value.tv_sec) * 1e6 + double(value.tv_nsec) * .001;
    else result.error = static_cast<unsigned long>(errno);
#endif
    return result;
}

struct Sample
{
    size_t index = 0;
    double wallUs = 0;
    ThreadClock begin, end;

    bool cpuValid() const noexcept { return begin.cpuUs >= 0 && end.cpuUs >= begin.cpuUs; }
    double cpuUs() const noexcept { return cpuValid() ? end.cpuUs - begin.cpuUs : -1; }
    bool cyclesValid() const noexcept
    {
        return begin.cyclesValid && end.cyclesValid && end.cycles >= begin.cycles;
    }
    uint64_t cycles() const noexcept { return cyclesValid() ? end.cycles - begin.cycles : 0; }
    const char* phase() const noexcept
    {
        return index < workerCallbacks ? "worker_and_automation" : "forced_six_slot_publication";
    }
    size_t phaseIndex() const noexcept { return index < workerCallbacks ? index : index - workerCallbacks; }
};

struct Configuration
{
    const char* engine;
    const char* suite;
    double sampleRate;
    int blockSize;
    int channels;
    double budgetUs() const noexcept { return 1e6 * blockSize / sampleRate; }
};

inline void validate(const std::vector<Sample>& samples)
{
    if (samples.size() != measuredCallbacks)
        throw std::runtime_error("CAB timing evidence must preserve all 1600 callback samples");
    for (size_t i = 0; i < samples.size(); ++i)
        if (samples[i].index != i || !std::isfinite(samples[i].wallUs) || samples[i].wallUs < 0)
            throw std::runtime_error("CAB timing evidence callback index/wall sample mismatch");
}

struct WallPairs { size_t p99, maximum; };

inline WallPairs wallPairs(const std::vector<Sample>& samples, size_t first, size_t last)
{
    if (first >= last || last > samples.size())
        throw std::runtime_error("CAB timing evidence phase range is invalid");
    std::vector<size_t> indices(last - first);
    std::iota(indices.begin(), indices.end(), first);
    std::sort(indices.begin(), indices.end(), [&samples](size_t a, size_t b)
    {
        return samples[a].wallUs == samples[b].wallUs ? a < b : samples[a].wallUs < samples[b].wallUs;
    });
    // Same order statistic as the unchanged wall gate: [1584] of 1600 samples.
    return {indices[size_t(double(indices.size()) * .99)], indices.back()};
}

inline void appendPair(std::ostream& out, const char* prefix, const Sample& sample)
{
    out << ' ' << prefix << "_callback_index=" << sample.index
        << ' ' << prefix << "_wall_us=" << sample.wallUs
        << ' ' << prefix << "_thread_cpu_us=" << sample.cpuUs()
        << ' ' << prefix << "_thread_cpu_valid=" << sample.cpuValid()
        << ' ' << prefix << "_thread_cycles=" << sample.cycles()
        << ' ' << prefix << "_thread_cycles_valid=" << sample.cyclesValid();
}

inline void reportPairs(std::ostream& out, const Configuration& config, const char* phase,
                        const std::vector<Sample>& samples, size_t first, size_t last)
{
    const auto selected = wallPairs(samples, first, last);
    size_t validCpu = 0, validCycles = 0, misses = 0, missesWithCycles = 0;
    for (size_t i = first; i < last; ++i)
    {
        validCpu += samples[i].cpuValid();
        validCycles += samples[i].cyclesValid();
        if (samples[i].wallUs > config.budgetUs())
        {
            ++misses;
            missesWithCycles += samples[i].cyclesValid();
        }
    }
    out << "PAIRED_TIMING engine=" << config.engine << " suite=" << config.suite
        << " sr=" << config.sampleRate << " block=" << config.blockSize << " channels=" << config.channels
        << " phase=" << phase << " samples=" << last - first
        << " thread_cpu_valid_samples=" << validCpu << " thread_cycles_valid_samples=" << validCycles
        << " misses=" << misses << " misses_with_thread_cycles=" << missesWithCycles;
    appendPair(out, "wall_p99", samples[selected.p99]);
    appendPair(out, "wall_max", samples[selected.maximum]);
    out << '\n';
}

// Deadline misses caused by the thread not running rather than by CAB work.
// A callback's own work is estimated from its thread clock (CPU cycles on
// Windows, thread CPU time elsewhere) relative to the route's median callback:
// work / median work x median wall time. Cycles are only ever divided by
// cycles of the same run; no fixed cycle rate is assumed. A miss is
// "descheduled" when that estimate fits in half the block period: the rest of
// its wall time went to the thread waiting for a CPU. A route whose misses are
// all descheduled, and whose median callback is itself within half the
// budget, may measure once more; a miss with heavy work (real CAB cost) or
// without thread clocks never qualifies.
inline constexpr double deschedulingWorkShare = .5;

struct MissClassification
{
    size_t misses = 0, descheduled = 0;
    double wallP50 = 0, wallP99 = 0, maxMissWorkUs = 0;
    const char* workClock = "none";
    bool clocksValid = false;

    bool withinBudget(double budget) const noexcept { return wallP99 < budget; }
    bool onlyDescheduling(double budget) const noexcept
    {
        return misses > 0 && clocksValid && descheduled == misses && wallP50 < budget * deschedulingWorkShare;
    }
};

inline MissClassification classifyMisses(const std::vector<Sample>& samples, double budget)
{
    MissClassification result;
    if (samples.empty()) return result;
    std::vector<double> walls;
    walls.reserve(samples.size());
    for (const auto& sample : samples) walls.push_back(sample.wallUs);
    std::sort(walls.begin(), walls.end());
    result.wallP50 = walls[walls.size() / 2];
    result.wallP99 = walls[size_t(double(walls.size()) * .99)];  // the gate's [1584] of 1600
    const bool cycles = std::all_of(samples.begin(), samples.end(), [](const Sample& s) { return s.cyclesValid(); });
    const bool cpu = std::all_of(samples.begin(), samples.end(), [](const Sample& s) { return s.cpuValid(); });
    result.workClock = cycles ? "thread_cycles" : cpu ? "thread_cpu" : "none";
    const auto work = [cycles](const Sample& s) { return cycles ? double(s.cycles()) : s.cpuUs(); };
    std::vector<double> typical;
    if (cycles || cpu)
    {
        typical.reserve(samples.size());
        for (const auto& sample : samples) typical.push_back(work(sample));
        std::sort(typical.begin(), typical.end());
    }
    const double median = typical.empty() ? 0 : typical[typical.size() / 2];
    result.clocksValid = (cycles || cpu) && std::isfinite(median) && median > 0;
    for (const auto& sample : samples)
    {
        if (!(sample.wallUs > budget)) continue;
        ++result.misses;
        if (!result.clocksValid) continue;
        const double workUs = work(sample) / median * result.wallP50;
        result.maxMissWorkUs = std::max(result.maxMissWorkUs, workUs);
        if (workUs <= budget * deschedulingWorkShare) ++result.descheduled;
    }
    return result;
}

inline void writeCsv(std::ostream& out, const Configuration& config, const std::vector<Sample>& samples)
{
    validate(samples);
    out << std::setprecision(17)
        << "schema_version,engine,suite,sample_rate,block_size,channels,callback_index,phase,phase_index,"
           "automation_requested,wall_us,budget_us,deadline_miss,thread_cpu_us,thread_cpu_valid,"
           "thread_cycles,thread_cycles_valid,clock_start_error,clock_end_error,"
           "thread_cpu_start_us,thread_cpu_end_us,thread_cycles_start,thread_cycles_end\n";
    for (const auto& sample : samples)
        out << "1," << config.engine << ',' << config.suite << ',' << config.sampleRate << ','
            << config.blockSize << ',' << config.channels << ',' << sample.index << ',' << sample.phase() << ','
            << sample.phaseIndex() << ',' << (sample.index < automationCallbacks) << ',' << sample.wallUs << ','
            << config.budgetUs() << ',' << (sample.wallUs > config.budgetUs()) << ',' << sample.cpuUs() << ','
            << sample.cpuValid() << ',' << sample.cycles() << ',' << sample.cyclesValid() << ','
            << sample.begin.error << ',' << sample.end.error << ',' << sample.begin.cpuUs << ','
            << sample.end.cpuUs << ',' << sample.begin.cycles << ',' << sample.end.cycles << '\n';
    if (!out) throw std::runtime_error("CAB timing evidence CSV write failed");
}

inline std::filesystem::path saveCsv(const Configuration& config, const std::vector<Sample>& samples)
{
    // Distinct paths prevent ordinary, extended and profiling CTest runs from
    // overwriting one another. All I/O occurs after the 1600 measured callbacks.
    const auto directory = std::filesystem::path("cab-timing-diagnostics") / config.suite / config.engine;
    std::filesystem::create_directories(directory);
    const auto path = directory / (std::to_string(static_cast<int>(config.sampleRate)) + "-"
                                  + std::to_string(config.blockSize) + "-" + std::to_string(config.channels) + ".csv");
    std::ofstream file(path, std::ios::trunc);
    if (!file) throw std::runtime_error("CAB timing evidence CSV could not be opened");
    writeCsv(file, config, samples);
    file.close();
    if (!file) throw std::runtime_error("CAB timing evidence CSV could not be flushed");
    return path;
}
}
