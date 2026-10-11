#include "NativeMessageLoop.h"
#import <Cocoa/Cocoa.h>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>

#if ! JUCE_MAC
 #error This queue compatibility diagnostic requires macOS.
#endif

namespace {
constexpr int callbackCount = 12;
constexpr int sliceMs = 20;
constexpr double deliveryLimitMs = 2000.0;
constexpr short eventSubtype = 173;
constexpr NSInteger eventTag = 0x43485143;
constexpr NSInteger eventPayload = 31716056;

struct ProbeState {
    std::array<int, callbackCount> order {};
    int delivered {}, posts {}, nativeDeliveries {}, nativeSuperReturns {}, slices {};
    bool fence {}, wrongThread {}, nativeWrongThread {}, stopWrongThread {};
    bool stopCalled {}, stopCallReturned {};
    double started {}, fenceMs {-1}, nativeMs {-1}, stopStarted {}, stopMs {-1};
};
ProbeState* activeProbe = nullptr;

bool messageThread() {
    return [NSThread isMainThread]
        && juce::MessageManager::getInstance()->isThisTheMessageThread();
}

bool fifo(const ProbeState& state) {
    if (state.delivered != callbackCount) return false;
    for (int i = 0; i < callbackCount; ++i)
        if (state.order[size_t(i)] != i) return false;
    return true;
}

juce::var orderArray(const ProbeState& state) {
    juce::Array<juce::var> result;
    for (int i = 0; i < juce::jlimit(0, callbackCount, state.delivered); ++i)
        result.add(state.order[size_t(i)]);
    return result;
}

int writeReport(const juce::var& report, const char* path, int result) {
    const auto text = juce::JSON::toString(report, true).toStdString();
    std::cout << text << std::endl;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text << '\n';
    file.close();
    if (!file) {
        std::cerr << "QUEUE_COMPATIBILITY_REPORT_WRITE_FAILED\n";
        return 2;
    }
    return result;
}
}

// Observe delivery at NSApplication's public dispatch entry point. The native
// event queue and nextEventMatchingMask implementation are not replaced.
// This is an application-defined event, not a hardware mouse or key event.
@interface ChimeraQueueCompatibilityApplication : NSApplication
@end
@implementation ChimeraQueueCompatibilityApplication
- (void)sendEvent:(NSEvent*)event {
    ProbeState* const observed = activeProbe;
    const bool ours = observed != nullptr
        && event.type == NSEventTypeApplicationDefined
        && event.subtype == eventSubtype
        && event.data1 == eventTag && event.data2 == eventPayload;
    if (ours) {
        ++observed->nativeDeliveries;
        observed->nativeWrongThread |= !messageThread();
        observed->nativeMs = juce::Time::getMillisecondCounterHiRes() - observed->started;
    }
    [super sendEvent:event];
    if (ours) ++observed->nativeSuperReturns;
}
@end

int main(int argc, char** argv) {
    @autoreleasepool {
        if (argc != 2) {
            std::cerr << "Usage: queue-compatibility result.json\n";
            return 2;
        }
        [ChimeraQueueCompatibilityApplication sharedApplication];
        juce::ScopedJuceInitialiser_GUI gui;
        auto* messages = juce::MessageManager::getInstance();
        const auto dequeueSelector = @selector(nextEventMatchingMask:untilDate:inMode:dequeue:);
        const bool originalDequeue = [NSApp methodForSelector:dequeueSelector]
            == [NSApplication instanceMethodForSelector:dequeueSelector];
        const bool correctApplication = [NSApp isMemberOfClass:[ChimeraQueueCompatibilityApplication class]];
        const bool correctThread = messageThread();
        if (!originalDequeue || !correctApplication || !correctThread) {
            std::cerr << "QUEUE_COMPATIBILITY_SETUP_FAILED: application/thread/dequeue identity\n";
            return 2;
        }

        // Initializes only the helper's one-time log. A zero-budget call runs
        // neither native phase, and no measured workload has been posted yet.
        const bool initialPumpContinues = chimeraTest::dispatchFor(0);
        auto state = std::make_shared<ProbeState>();
        state->order.fill(-1);
        state->started = juce::Time::getMillisecondCounterHiRes();
        activeProbe = state.get();
        NSEvent* const event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
            location:NSZeroPoint modifierFlags:0
            timestamp:[NSProcessInfo processInfo].systemUptime
            windowNumber:0 context:nil subtype:eventSubtype data1:eventTag data2:eventPayload];
        const bool eventCreated = event != nil;
        if (eventCreated) [NSApp postEvent:event atStart:NO]; // one actual queue post

        for (int i = 0; i < callbackCount; ++i) {
            state->posts += juce::MessageManager::callAsync([state, i] {
                state->wrongThread |= !messageThread();
                if (state->delivered < callbackCount) state->order[size_t(state->delivered)] = i;
                ++state->delivered;
            }) ? 1 : 0;
        }
        state->posts += juce::MessageManager::callAsync([state] {
            state->wrongThread |= !messageThread();
            state->fence = true;
            state->fenceMs = juce::Time::getMillisecondCounterHiRes() - state->started;
        }) ? 1 : 0;
        const bool untouchedBeforePump = state->delivered == 0
            && state->nativeDeliveries == 0 && !state->fence;
        bool progressPumpContinues = initialPumpContinues;
        const auto complete = [&] {
            return state->delivered == callbackCount && state->fence
                && state->nativeDeliveries == 1 && state->nativeSuperReturns == 1;
        };
        const auto progressDeadline = state->started + deliveryLimitMs;
        while (!complete() && juce::Time::getMillisecondCounterHiRes() < progressDeadline) {
            ++state->slices;
            if (!chimeraTest::dispatchFor(sliceMs)) {
                progressPumpContinues = false;
                break;
            }
        }
        const double progressReturnMs = juce::Time::getMillisecondCounterHiRes() - state->started;
        const ProbeState progress = *state; // freeze before the separate stop phase
        const bool stopBeforeStopPhase = messages->hasStopMessageBeenSent();
        const bool progressPassed = eventCreated && untouchedBeforePump && progressPumpContinues
            && !stopBeforeStopPhase && progress.posts == callbackCount + 1 && fifo(progress)
            && progress.fence && progress.fenceMs >= 0 && progress.fenceMs <= deliveryLimitMs
            && progress.nativeDeliveries == 1 && progress.nativeSuperReturns == 1
            && progress.nativeMs >= 0 && progress.nativeMs <= deliveryLimitMs
            && !progress.wrongThread && !progress.nativeWrongThread;

        bool stopPhaseExecuted = false, stopPosted = false, stopPumpContinues = true;
        bool zeroBudgetAfterStopContinues = true;
        double stopDispatchMs = -1;
        if (progressPassed) {
            stopPhaseExecuted = true;
            state->stopStarted = juce::Time::getMillisecondCounterHiRes();
            stopPosted = juce::MessageManager::callAsync([state] {
                state->stopWrongThread = !messageThread();
                state->stopCalled = true;
                state->stopMs = juce::Time::getMillisecondCounterHiRes() - state->stopStarted;
                juce::MessageManager::getInstance()->stopDispatchLoop();
                state->stopCallReturned = true;
            });
            stopPumpContinues = chimeraTest::dispatchFor(sliceMs); // one stop-phase call
            stopDispatchMs = juce::Time::getMillisecondCounterHiRes() - state->stopStarted;
            zeroBudgetAfterStopContinues = chimeraTest::dispatchFor(0);
        }
        const ProbeState finalState = *state;
        const bool stopSent = messages->hasStopMessageBeenSent();
        const bool stopPassed = stopPhaseExecuted && stopPosted && finalState.stopCalled
            && finalState.stopCallReturned && !finalState.stopWrongThread && stopSent
            && !stopPumpContinues && !zeroBudgetAfterStopContinues;
        const bool passed = progressPassed && stopPassed;
        activeProbe = nullptr; // freeze all observations before native/GUI cleanup

        juce::var report(new juce::DynamicObject());
        auto& out = *report.getDynamicObject();
        out.setProperty("schema", 1);
        out.setProperty("variant", COMPAT_VARIANT);
        out.setProperty("original_source_sha", "3171605606e224c1ca63fc0f3827e992a19b481f");
        out.setProperty("candidate_source_sha", "c291d7132ba33561db922e7d5a992ab40212345d");
        out.setProperty("compiled_helper_source_sha", COMPAT_HELPER_SOURCE_SHA);
        out.setProperty("purpose", "Application-defined NSEvent queue and JUCE stop compatibility");
        out.setProperty("marker", passed ? "PASS_APPLICATION_EVENT_FIFO_AND_STOP" : "FAIL_QUEUE_COMPATIBILITY");
        out.setProperty("exit_code", passed ? 0 : 1);
        out.setProperty("slice_ms", sliceMs);
        out.setProperty("delivery_limit_ms", deliveryLimitMs);
        out.setProperty("initial_zero_budget_continues", initialPumpContinues);
        out.setProperty("native_subclass_verified", correctApplication);
        out.setProperty("main_thread_verified", correctThread);
        out.setProperty("native_dequeue_unmodified", originalDequeue);
        out.setProperty("native_event_type", "NSEventTypeApplicationDefined");
        out.setProperty("native_event_created", eventCreated);
        out.setProperty("native_event_posts", eventCreated ? 1 : 0);
        out.setProperty("untouched_before_pump", untouchedBeforePump);
        out.setProperty("callback_posts", progress.posts);
        out.setProperty("callback_count", progress.delivered);
        out.setProperty("callback_fifo", fifo(progress));
        out.setProperty("callback_order", orderArray(progress));
        out.setProperty("fence_delivered", progress.fence);
        out.setProperty("fence_ms", progress.fenceMs);
        out.setProperty("native_deliveries", progress.nativeDeliveries);
        out.setProperty("native_super_returns", progress.nativeSuperReturns);
        out.setProperty("native_delivery_ms", progress.nativeMs);
        out.setProperty("native_wrong_thread", progress.nativeWrongThread);
        out.setProperty("callback_wrong_thread", progress.wrongThread);
        out.setProperty("progress_slices", progress.slices);
        out.setProperty("progress_return_ms", progressReturnMs);
        out.setProperty("progress_pump_continues", progressPumpContinues);
        out.setProperty("stop_before_stop_phase", stopBeforeStopPhase);
        out.setProperty("progress_passed", progressPassed);
        out.setProperty("stop_phase_executed", stopPhaseExecuted);
        out.setProperty("stop_posted", stopPosted);
        out.setProperty("stop_callback_called", finalState.stopCalled);
        out.setProperty("stop_callback_returned", finalState.stopCallReturned);
        out.setProperty("stop_wrong_thread", finalState.stopWrongThread);
        out.setProperty("stop_callback_ms", finalState.stopMs);
        out.setProperty("stop_dispatch_ms", stopDispatchMs);
        out.setProperty("stop_flag", stopSent);
        out.setProperty("stop_dispatch_return", stopPumpContinues);
        out.setProperty("zero_budget_after_stop_return", zeroBudgetAfterStopContinues);
        out.setProperty("stop_passed", stopPassed);
        out.setProperty("outcome_frozen_before_cleanup", true);
        out.setProperty("poll_substitution", false);
        out.setProperty("sleep_injection", false);
        out.setProperty("hardware_input_test", false);
        out.setProperty("gui_failure_reproduced", false);
        out.setProperty("release_acceptance", false);
        out.setProperty("process_attempts", 1);
        const int result = writeReport(report, argv[1], passed ? 0 : 1);
        [NSEvent stopPeriodicEvents]; // cleanup after the result is sealed
        return result;
    }
}
