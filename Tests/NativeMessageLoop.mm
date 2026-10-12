#include "NativeMessageLoop.h"
#import <Cocoa/Cocoa.h>
#include <iostream>

namespace juce {
// Same modal-event filter used by the pinned JUCE 8.0.8 dispatcher. Preserve
// modal input blocking rather than sending events straight past its policy.
extern bool (*isEventBlockedByModalComps)(NSEvent*);
}

namespace chimeraTest {
bool dispatchFor(int milliseconds) {
    auto& telemetry=dispatchTelemetry();const bool measured=telemetry.enabled;
    const auto dispatchStarted=measured?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
    auto* messages=juce::MessageManager::getInstance();
    jassert(messages->isThisTheMessageThread());
    if(!messages->isThisTheMessageThread())return false;
    static const bool announced=[] {
        std::cout<<"TEST HARNESS: non-blocking macOS native event pump (JUCE #1574)\n";
        return true;
    }();
    (void)announced;
    const double deadline=juce::Time::getMillisecondCounterHiRes()+milliseconds;
    while(!messages->hasStopMessageBeenSent()) {
        const double remaining=deadline-juce::Time::getMillisecondCounterHiRes();
        if(remaining<=0)break;
        @autoreleasepool {
            // Test-only fairness: JUCE serves four messages per CF source callback.
            // Keep servicing ready sources for this requested slice before AppKit
            // polling, whose internal native work may outlast its event deadline.
            const auto cfStarted=measured?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
            CFRunLoopRunInMode(kCFRunLoopDefaultMode,juce::jmin(.01,remaining*.001),false);
            if(measured){const double elapsed=dispatchMicros(cfStarted);++telemetry.iterations;telemetry.cfMicros+=elapsed;telemetry.cfMaxMicros=std::max(telemetry.cfMaxMicros,elapsed);}
            // On Tahoe, JUCE's future untilDate can defer callbacks until a
            // later mouse event: https://github.com/juce-framework/JUCE/issues/1574
            // Poll AppKit without waiting; CFRunLoop above still yields normally.
            const auto pollStarted=measured?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
            NSEvent* event=[NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate distantPast]
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES];
            if(measured){const double elapsed=dispatchMicros(pollStarted);telemetry.pollMicros+=elapsed;telemetry.pollMaxMicros=std::max(telemetry.pollMaxMicros,elapsed);}
            if(event)
                if(juce::isEventBlockedByModalComps==nullptr
                    || !juce::isEventBlockedByModalComps(event)) {
                    const auto sendStarted=measured?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
                    [NSApp sendEvent:event];
                    if(measured){const double elapsed=dispatchMicros(sendStarted);++telemetry.nativeEvents;telemetry.sendMicros+=elapsed;telemetry.sendMaxMicros=std::max(telemetry.sendMaxMicros,elapsed);}
                }
        }
    }
    if(measured){const double elapsed=dispatchMicros(dispatchStarted);++telemetry.calls;telemetry.totalMicros+=elapsed;telemetry.maxMicros=std::max(telemetry.maxMicros,elapsed);}
    return !messages->hasStopMessageBeenSent();
}
}
