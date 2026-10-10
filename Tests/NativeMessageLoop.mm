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
            CFRunLoopRunInMode(kCFRunLoopDefaultMode,juce::jmin(.01,remaining*.001),true);
            // On Tahoe, JUCE's future untilDate can defer callbacks until a
            // later mouse event: https://github.com/juce-framework/JUCE/issues/1574
            // Poll AppKit without waiting; CFRunLoop above still yields normally.
            if(NSEvent* event=[NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate distantPast]
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES])
                if(juce::isEventBlockedByModalComps==nullptr
                    || !juce::isEventBlockedByModalComps(event))
                    [NSApp sendEvent:event];
        }
    }
    return !messages->hasStopMessageBeenSent();
}
}
