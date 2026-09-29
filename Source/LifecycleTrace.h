#pragma once
#include <juce_core/juce_core.h>
#include <cstdint>
#include <mutex>

namespace spectralforge::lifecycle {
// Opt-in diagnostics only. Call from construction, preparation and teardown,
// never from processBlock or a per-block DSP method. The selected file contains
// event names, instance/thread identifiers and monotonic timings; no user data.
inline const juce::File& traceFile()
{
    static const juce::File file = [] {
        const auto path = juce::SystemStats::getEnvironmentVariable("CHIMERA_LIFECYCLE_TRACE", {});
        return path.isNotEmpty() && juce::File::isAbsolutePath(path) ? juce::File(path) : juce::File{};
    }();
    return file;
}

inline bool enabled() { return traceFile() != juce::File{}; }

inline double microseconds() noexcept
{
    return juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks()) * 1.0e6;
}

inline void write(const char* event, const void* instance, double elapsed = -1.0)
{
    if (!enabled()) return;
    // Keep records intact when the IR and host threads finish concurrently.
    static std::mutex outputMutex;
    const std::lock_guard<std::mutex> lock(outputMutex);
    auto line = juce::String(microseconds(), 0) + " us thread="
        + juce::String::toHexString(static_cast<juce::int64>(reinterpret_cast<std::uintptr_t>(juce::Thread::getCurrentThreadId())))
        + " instance=" + juce::String::toHexString(static_cast<juce::int64>(reinterpret_cast<std::uintptr_t>(instance)))
        + " " + event;
    if (elapsed >= 0.0) line += " elapsed_us=" + juce::String(elapsed, 0);
    line += "\n";
    // Failure to open the explicitly requested path must not affect the plugin.
    traceFile().appendText(line, false, false, nullptr);
}

class Scope final {
public:
    Scope(const char* eventName, const void* instanceIn)
        : event(eventName), instance(instanceIn), started(enabled() ? microseconds() : -1.0)
    {
        if (started >= 0.0) write((juce::String(event) + ".begin").toRawUTF8(), instance);
    }
    ~Scope()
    {
        if (started >= 0.0) write((juce::String(event) + ".end").toRawUTF8(), instance, microseconds() - started);
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
private:
    const char* event;
    const void* instance;
    double started;
};
}
