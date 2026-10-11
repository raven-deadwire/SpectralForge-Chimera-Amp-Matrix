#pragma once
#include <cstdint>

// Diagnostic-only POD records. No logging, allocation, queue work, or locks.
namespace coldClickTrace {
enum Event : std::uint64_t {
    sessionBegin=1, sessionEnd,
    sceneBegin=10, sceneEnd, showingBegin, showingEnd,
    clickBegin=20, clickEnd, triggerBegin, triggerEnd, listener,
    predicateBegin, predicateEnd, originalFailure, failureProbeBegin, failureProbeEnd,
    dispatchBegin=40, dispatchEnd, cfBegin, cfEnd, nextBegin, nextEnd,
    sendBegin, sendEnd, modalFilterBegin, modalFilterEnd, autoreleaseBegin, autoreleaseEnd,
    queuePostBegin=60, queuePostEnd, sourceBegin, sourceEnd, callbackBegin, callbackEnd,
    observerEarly=80, observerLate
};
std::uint64_t record(Event, std::uint64_t=0, std::uint64_t=0,
                     std::uint64_t=0, std::uint64_t=0) noexcept;
inline std::uint64_t address(const void* p) noexcept {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p));
}
struct Span {
    const Event ending;
    const std::uint64_t token;
    explicit Span(Event start, Event end, std::uint64_t a=0, std::uint64_t b=0) noexcept
        : ending(end), token(record(start,a,b)) {}
    ~Span() noexcept { record(ending,token); }
};
// Created before any test cases; destroyed after the original main result.
struct Session {
    Session();
    ~Session() noexcept;
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
};
}
