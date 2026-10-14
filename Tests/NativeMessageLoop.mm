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
    const double started=juce::Time::getMillisecondCounterHiRes();
    const double deadline=started+milliseconds;
    double cfMilliseconds{},pollMilliseconds{},eventMilliseconds{},autoreleaseMilliseconds{};
    int iterations{},nativeEvents{};SInt32 lastCFResult{};NSInteger lastNativeEvent=-1;
    while(!messages->hasStopMessageBeenSent()) {
        const double remaining=deadline-juce::Time::getMillisecondCounterHiRes();
        if(remaining<=0)break;
        double beforeAutorelease{};
        @autoreleasepool {
            // Test-only fairness: JUCE serves four messages per CF source callback.
            // Keep servicing ready sources for this requested slice before AppKit
            // polling, whose internal native work may outlast its event deadline.
            const auto beforeCF=juce::Time::getMillisecondCounterHiRes();
            lastCFResult=CFRunLoopRunInMode(kCFRunLoopDefaultMode,juce::jmin(.01,remaining*.001),false);
            const auto beforePoll=juce::Time::getMillisecondCounterHiRes();
            cfMilliseconds+=beforePoll-beforeCF;
            if(measured){const double us=(beforePoll-beforeCF)*1000;++telemetry.iterations;telemetry.cfMicros+=us;telemetry.cfMaxMicros=std::max(telemetry.cfMaxMicros,us);}
            // On Tahoe, JUCE's future untilDate can defer callbacks until a
            // later mouse event: https://github.com/juce-framework/JUCE/issues/1574
            // Poll AppKit without waiting; CFRunLoop above still yields normally.
            NSEvent* event=[NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantPast]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
            const auto beforeEvent=juce::Time::getMillisecondCounterHiRes();
            pollMilliseconds+=beforeEvent-beforePoll;
            if(measured){const double us=(beforeEvent-beforePoll)*1000;telemetry.pollMicros+=us;telemetry.pollMaxMicros=std::max(telemetry.pollMaxMicros,us);}
            if(event) {
                ++nativeEvents;lastNativeEvent=NSInteger(event.type);
                if(juce::isEventBlockedByModalComps==nullptr
                    || !juce::isEventBlockedByModalComps(event))
                    {
                    const auto sendStarted=measured?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
                    [NSApp sendEvent:event];
                    if(measured){const double us=dispatchMicros(sendStarted);++telemetry.nativeEvents;telemetry.sendMicros+=us;telemetry.sendMaxMicros=std::max(telemetry.sendMaxMicros,us);}
                }
            }
            beforeAutorelease=juce::Time::getMillisecondCounterHiRes();
            eventMilliseconds+=beforeAutorelease-beforeEvent;
        }
        autoreleaseMilliseconds+=juce::Time::getMillisecondCounterHiRes()-beforeAutorelease;
        ++iterations;
    }
    // Observation only: preserve the dispatcher and the caller's readiness
    // deadline. Identify which native stage consumed an overlong slice before
    // changing scheduling; no extra dispatch or click is attempted here.
    const double elapsed=juce::Time::getMillisecondCounterHiRes()-started;
    if(elapsed>juce::jmax(250.0,4.0*milliseconds)) {
        std::cerr<<"TEST HARNESS: macOS dispatch overrun requested_ms="<<milliseconds
                 <<" actual_ms="<<elapsed<<" cf_ms="<<cfMilliseconds
                 <<" appkit_poll_ms="<<pollMilliseconds<<" native_event_ms="<<eventMilliseconds
                 <<" autorelease_ms="<<autoreleaseMilliseconds<<" iterations="<<iterations
                 <<" last_cf_result="<<lastCFResult<<" native_events="<<nativeEvents
                 <<" last_native_event="<<lastNativeEvent<<'\n';
    }
    if(measured){const double us=dispatchMicros(dispatchStarted);++telemetry.calls;telemetry.totalMicros+=us;telemetry.maxMicros=std::max(telemetry.maxMicros,us);}
    return !messages->hasStopMessageBeenSent();
}
}
