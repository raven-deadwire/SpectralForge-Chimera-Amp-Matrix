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
