#pragma once
#include <juce_core/juce_core.h>
#include <cstdint>
#include <mutex>
#if JUCE_WINDOWS
 #include <process.h>
#else
 #include <unistd.h>
#endif

namespace spectralforge::lifecycle {
// Opt-in diagnostics only. Call from construction, preparation and teardown,
// never from processBlock or a per-block DSP method. The selected file contains
// event names, process/instance/thread identifiers and monotonic timings; no
// user data. Never call this logger from DllMain or a loader notification.
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

struct Context {
    const void* related{};
    juce::Thread::ThreadID messageThread{};
    bool messageThreadObserved{};
};

// The caller supplies getInstanceWithoutCreating(). Keep juce_events out of
// this DSP-shared header; observing a boundary must not create a message loop,
// dispatch callbacks, acquire a MessageManagerLock or change the message thread.
template <typename MessageManager>
Context messageContext(const MessageManager* manager, const void* related = nullptr) noexcept
{
    return {related, manager != nullptr ? manager->getCurrentMessageThread() : nullptr, true};
}

inline juce::int64 processId() noexcept
{
#if JUCE_WINDOWS
    return static_cast<juce::int64>(::_getpid());
#else
    return static_cast<juce::int64>(::getpid());
#endif
}

inline juce::String hexId(const void* value)
{
    return juce::String::toHexString(static_cast<juce::int64>(reinterpret_cast<std::uintptr_t>(value)));
}

inline void write(const char* event, const void* instance, double elapsed = -1.0, Context context = {})
{
    if (!enabled()) return;
    // Keep records intact when the IR and host threads finish concurrently.
    static std::mutex outputMutex;
    const std::lock_guard<std::mutex> lock(outputMutex);
    const auto thread = juce::Thread::getCurrentThreadId();
    // String(double, 0) may choose scientific notation and lose low-order
    // timestamp digits. Integral microseconds keep diagnostic boundaries exact.
    auto line = juce::String(static_cast<juce::int64>(microseconds())) + " us thread=" + hexId(thread)
        + " instance=" + hexId(instance) + " " + event
        + " pid=" + juce::String(processId());
#if JUCE_WINDOWS
    // In JUCE's Windows backend ThreadID wraps GetCurrentThreadId(). Supply a
    // decimal native TID for direct correlation with a Windows thread dump.
    line += " tid=" + juce::String(static_cast<juce::int64>(reinterpret_cast<std::uintptr_t>(thread)));
#endif
    if (context.related != nullptr) line += " related=" + hexId(context.related);
    if (context.messageThreadObserved)
    {
        line += " message_thread=" + (context.messageThread != nullptr ? hexId(context.messageThread) : juce::String("absent"));
        if (context.messageThread != nullptr)
            line += " on_message_thread=" + juce::String(thread == context.messageThread ? 1 : 0);
    }
    if (elapsed >= 0.0) line += " elapsed_us=" + juce::String(static_cast<juce::int64>(elapsed));
    line += "\n";
    // Failure to open the explicitly requested path must not affect the plugin.
    traceFile().appendText(line, false, false, nullptr);
}

class Scope final {
public:
    Scope(const char* eventName, const void* instanceIn, Context contextIn = {})
        : event(eventName), instance(instanceIn), context(contextIn), started(enabled() ? microseconds() : -1.0)
    {
        if (started >= 0.0) write((juce::String(event) + ".begin").toRawUTF8(), instance, -1.0, context);
    }
    ~Scope()
    {
        if (started >= 0.0) write((juce::String(event) + ".end").toRawUTF8(), instance, microseconds() - started, context);
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
private:
    const char* event;
    const void* instance;
    Context context;
    double started;
};

// Declare this BEFORE every other data member, and arm it at destructor body
// entry. Its point event then follows all derived members, but PRECEDES base
// destruction. It stores numeric identities only and never dereferences the
// owner/related objects after their members have been destroyed. The host's
// IPlugView::removed()/release() return remains the complete public boundary.
class AfterMembers final {
public:
    AfterMembers(const char* eventName, const void* instanceIn)
        : event(eventName), instance(instanceIn) {}
    void arm(Context contextIn)
    {
        context = contextIn;
        started = enabled() ? microseconds() : -1.0;
    }
    ~AfterMembers()
    {
        if (started >= 0.0) write(event, instance, microseconds() - started, context);
    }
    AfterMembers(const AfterMembers&) = delete;
    AfterMembers& operator=(const AfterMembers&) = delete;
private:
    const char* event;
    const void* instance;
    Context context;
    double started{-1.0};
};
}
