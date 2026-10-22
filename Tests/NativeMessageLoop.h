#pragma once
#include <juce_events/juce_events.h>
#include <chrono>

namespace chimeraTest {
// Opt-in test-harness telemetry. CF/AppKit durations are inclusive of any
// callbacks they dispatch and therefore must not be added to analyzer CPU.
struct DispatchTelemetry {
    bool enabled{};
    uint64_t calls{},iterations{},nativeEvents{},appKitPollsDeferredAtDeadline{};
    double totalMicros{},maxMicros{},cfMicros{},cfMaxMicros{},pollMicros{},pollMaxMicros{},sendMicros{},sendMaxMicros{};
};
inline DispatchTelemetry& dispatchTelemetry(){static DispatchTelemetry value;return value;}
inline void beginDispatchTelemetry(){dispatchTelemetry()={};dispatchTelemetry().enabled=true;}
inline DispatchTelemetry endDispatchTelemetry(){auto result=dispatchTelemetry();dispatchTelemetry().enabled=false;return result;}
inline double dispatchMicros(std::chrono::steady_clock::time_point started){return std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count();}
// Test-only event pump. Shipping plug-ins keep their host's normal event loop.
#if JUCE_MAC
bool dispatchFor(int milliseconds);
#else
inline bool dispatchFor(int milliseconds) {
    auto& telemetry=dispatchTelemetry();
    if(!telemetry.enabled)return juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
    const auto started=std::chrono::steady_clock::now();
    const bool result=juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
    const double elapsed=dispatchMicros(started);++telemetry.calls;telemetry.totalMicros+=elapsed;
    telemetry.maxMicros=std::max(telemetry.maxMicros,elapsed);return result;
}
#endif
}
