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
    int iterations{},nativeEvents{},deferredPolls{};SInt32 lastCFResult{};NSInteger lastNativeEvent=-1;
    while(!messages->hasStopMessageBeenSent()) {
        const double remaining=deadline-juce::Time::getMillisecondCounterHiRes();
        if(remaining<=0)break;
        double beforeAutorelease{};
        @autoreleasepool {
            // Return after one ready source so the outer loop can re-check its
            // deadline and alternate CF and AppKit service. A time slice with
            // returnAfterSourceHandled=false can drain successive ready sources
            // without returning control to the test's bounded event pump.
            // An idle CF wait may use at most half of the remaining slice, so a
            // slice of 10 ms or less still reaches the AppKit poll below before
            // its deadline. Only a CF source that actually overruns defers it.
            const auto beforeCF=juce::Time::getMillisecondCounterHiRes();
            lastCFResult=CFRunLoopRunInMode(kCFRunLoopDefaultMode,juce::jmin(.01,remaining*.0005),true);
            const auto beforePoll=juce::Time::getMillisecondCounterHiRes();
            cfMilliseconds+=beforePoll-beforeCF;
            if(measured){const double us=(beforePoll-beforeCF)*1000;++telemetry.iterations;telemetry.cfMicros+=us;telemetry.cfMaxMicros=std::max(telemetry.cfMaxMicros,us);}
            // On Tahoe, JUCE's future untilDate can defer callbacks until a
            // later mouse event: https://github.com/juce-framework/JUCE/issues/1574
            // Poll AppKit without waiting; CFRunLoop above still yields normally.
            // A native source can overrun its requested slice. Do not start
            // another potentially expensive AppKit operation after this pump's
            // deadline has already expired. Leave queued events intact for the
            // next slice with budget; never discard/filter them to meet a test.
            const bool mayPoll=beforePoll<deadline;
            NSEvent* event=nil;
            if(mayPoll)event=[NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantPast]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
            else {
                ++deferredPolls;
                if(measured)++telemetry.appKitPollsDeferredAtDeadline;
            }
            const auto beforeEvent=juce::Time::getMillisecondCounterHiRes();
            pollMilliseconds+=beforeEvent-beforePoll;
            if(measured && mayPoll){const double us=(beforeEvent-beforePoll)*1000;telemetry.pollMicros+=us;telemetry.pollMaxMicros=std::max(telemetry.pollMaxMicros,us);}
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
    // Keep overruns observable, including a single native call that exceeds the
    // requested slice. No retry, hidden extra dispatch or clock adjustment.
    const double elapsed=juce::Time::getMillisecondCounterHiRes()-started;
    if(elapsed>juce::jmax(250.0,4.0*milliseconds)) {
        std::cerr<<"TEST HARNESS: macOS dispatch overrun requested_ms="<<milliseconds
                 <<" actual_ms="<<elapsed<<" cf_ms="<<cfMilliseconds
                 <<" appkit_poll_ms="<<pollMilliseconds<<" native_event_ms="<<eventMilliseconds
                 <<" autorelease_ms="<<autoreleaseMilliseconds<<" iterations="<<iterations
                 <<" appkit_deferred_at_deadline="<<deferredPolls
                 <<" last_cf_result="<<lastCFResult<<" native_events="<<nativeEvents
                 <<" last_native_event="<<lastNativeEvent<<'\n';
    }
    if(measured){const double us=dispatchMicros(dispatchStarted);++telemetry.calls;telemetry.totalMicros+=us;telemetry.maxMicros=std::max(telemetry.maxMicros,us);}
    return !messages->hasStopMessageBeenSent();
}
}

