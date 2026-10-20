#pragma once
// Adapter building block, NOT instrumentation installed into the released plugin.
// Single audio-thread writer. Construct before playback; export only after the
// callback has stopped. Timing scope must enclose the complete N-instance graph.
#include <chrono>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

namespace chimera_perf {
inline std::int64_t wallNs() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline std::int64_t threadCpuNs() noexcept {
#if defined(_WIN32)
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) return -1;
    ULARGE_INTEGER k{}, u{};
    k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
    return static_cast<std::int64_t>((k.QuadPart + u.QuadPart) * 100);
#elif defined(CLOCK_THREAD_CPUTIME_ID)
    timespec value{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) != 0) return -1;
    return value.tv_sec * INT64_C(1000000000) + value.tv_nsec;
#else
    return -1; // Unsupported counter stays unavailable, never zero.
#endif
}
struct Stamp { std::int64_t wall, cpu; };
class CallbackRecorder {
    struct Row { std::int64_t start{}, end{}, cpu{}, deadline{}; int frames{}; };
    std::vector<Row> rows;
    std::size_t used{}, dropped{};
public:
    explicit CallbackRecorder(std::size_t capacity) : rows(capacity) {}
    static Stamp begin() noexcept { return {wallNs(), threadCpuNs()}; }
    void end(Stamp start, int frames, std::int64_t scheduledDeadlineNs = 0) noexcept {
        const auto cpuEnd = threadCpuNs();
        const auto wallEnd = wallNs();
        if (used == rows.size()) { ++dropped; return; }
        rows[used++] = {start.wall, wallEnd,
                       start.cpu >= 0 && cpuEnd >= start.cpu ? cpuEnd - start.cpu : -1,
                       scheduledDeadlineNs, frames};
    }
    std::size_t count() const noexcept { return used; }
    std::size_t droppedRows() const noexcept { return dropped; }
    void writeCsv(const char* path) const {
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Cannot create callback trace");
        out << "index,start_ns,end_ns,thread_cpu_ns,deadline_ns,frames\n";
        for (std::size_t i = 0; i < used; ++i) {
            const auto& r = rows[i];
            out << i << ',' << r.start << ',' << r.end << ',';
            if (r.cpu >= 0) out << r.cpu;
            out << ',';
            if (r.deadline > 0) out << r.deadline;
            out << ',' << r.frames << '\n';
        }
        if (!out) throw std::runtime_error("Callback trace write failed");
    }
};
} // namespace chimera_perf
