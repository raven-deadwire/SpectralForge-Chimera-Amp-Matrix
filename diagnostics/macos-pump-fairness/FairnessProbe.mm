#include "NativeMessageLoop.h"
#import <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>

#if ! JUCE_MAC
 #error This scheduling diagnostic requires macOS.
#endif

namespace {
constexpr int callbackCount = 12;
constexpr int budgetMs = 10;
constexpr int artificialPollMs = 20;
struct ProbeState {
    std::array<int, callbackCount> order {};
    std::array<int, callbackCount> firstPollOrder {};
    int delivered {}, posted {}, polls {}, firstPollDelivered {-1};
    bool fence {}, firstPollFence {}, wrongThread {};
    double started {}, firstPollMs {-1}, fenceMs {-1}, pollBlockMs {};
};
ProbeState* armedProbe = nullptr;
bool orderedPrefix(const std::array<int, callbackCount>& order, int count) {
    if (count < 0 || count > callbackCount) return false;
    for (int i = 0; i < count; ++i) if (order[size_t(i)] != i) return false;
    return true;
}
juce::var asArray(const std::array<int, callbackCount>& order, int count) {
    juce::Array<juce::var> result;
    for (int i = 0; i < juce::jlimit(0, callbackCount, count); ++i) result.add(order[size_t(i)]);
    return result;
}
int writeReport(const juce::var& report, const char* path, int exitCode) {
    const auto json = juce::JSON::toString(report, true).toStdString();
    std::cout << report.getDynamicObject()->getProperty("marker").toString() << '\n' << json << std::endl;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << json << '\n';
    file.close();
    if (!file) { std::cerr << "INCONCLUSIVE_PROBE: result write failed\n"; return 2; }
    return exitCode;
}
}

// Deliberate test workload: only the AppKit poll is substituted. JUCE's real
// MessageManager, CF source, FIFO and the actual native helper stay in use.
@interface ChimeraFairnessApplication : NSApplication
@end
@implementation ChimeraFairnessApplication
- (NSEvent*)nextEventMatchingMask:(NSEventMask)mask
                       untilDate:(NSDate*)expiration
                          inMode:(NSRunLoopMode)mode
                         dequeue:(BOOL)dequeue {
    if (armedProbe == nullptr)
        return [super nextEventMatchingMask:mask untilDate:expiration inMode:mode dequeue:dequeue];
    auto& state = *armedProbe;
    const auto started = juce::Time::getMillisecondCounterHiRes();
    if (++state.polls == 1) {
        state.firstPollDelivered = state.delivered;
        state.firstPollFence = state.fence;
        state.firstPollOrder = state.order;
        state.firstPollMs = started - state.started;
    }
    // This blocks the message thread; it never pumps events or runs callbacks.
    [NSThread sleepForTimeInterval:double(artificialPollMs) * .001];
    if (state.polls == 1)
        state.pollBlockMs = juce::Time::getMillisecondCounterHiRes() - started;
    return nil;
}
@end

int main(int argc, char** argv) {
    @autoreleasepool {
        if (argc != 2) { std::cerr << "Usage: fairness-probe result.json\n"; return 2; }
        // A JUCE application macro would create ordinary NSApplication first.
        [ChimeraFairnessApplication sharedApplication];
        juce::ScopedJuceInitialiser_GUI gui;
        auto* messages = juce::MessageManager::getInstance();
        const bool correctApplication = [NSApp isMemberOfClass:[ChimeraFairnessApplication class]];
        const bool correctThread = [NSThread isMainThread] && messages->isThisTheMessageThread();
        if (!correctApplication || !correctThread) {
            std::cerr << "INCONCLUSIVE_PROBE: native application or message-thread setup\n";
            return 2;
        }

        // Initialize the helper's one-time logging only: zero budget runs no
        // CF/AppKit iteration, and no workload callbacks have been posted yet.
        if (!chimeraTest::dispatchFor(0)) return 2;
        struct StartupFence { bool delivered {}, wrongThread {}; };
        auto startup = std::make_shared<StartupFence>();
        const bool warmupPosted = juce::MessageManager::callAsync([startup] {
            startup->wrongThread = ![NSThread isMainThread]
                || !juce::MessageManager::getInstance()->isThisTheMessageThread();
            startup->delivered = true;
        });
        // Identical, one-shot startup drain in both binaries. The measured
        // queue does not exist yet; no AppKit interception or retry is armed.
        const auto warmupStarted = juce::Time::getMillisecondCounterHiRes();
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, .010, false);
        const auto warmupMs = juce::Time::getMillisecondCounterHiRes() - warmupStarted;
        const bool warmupPassed = warmupPosted && startup->delivered && !startup->wrongThread
            && !messages->hasStopMessageBeenSent();
        juce::var report(new juce::DynamicObject());
        auto& out = *report.getDynamicObject();
        out.setProperty("variant", FAIRNESS_VARIANT);
        out.setProperty("source_sha", "7c782fda9e2d7cb255e70b6d1d1857fc78300c46");
        out.setProperty("warmup_requested_ms", 10);
        out.setProperty("warmup_elapsed_ms", warmupMs);
        out.setProperty("warmup_posted", warmupPosted);
        out.setProperty("warmup_fence", startup->delivered);
        out.setProperty("warmup_wrong_thread", startup->wrongThread);
        out.setProperty("warmup_passed", warmupPassed);
        out.setProperty("release_acceptance", false);
        if (!warmupPassed) {
            out.setProperty("marker", "INCONCLUSIVE_WARMUP");
            out.setProperty("exit_code", 2);
            return writeReport(report, argv[1], 2);
        }
        auto state = std::make_shared<ProbeState>();
        state->order.fill(-1);
        state->firstPollOrder.fill(-1);
        for (int i = 0; i < callbackCount; ++i)
            state->posted += juce::MessageManager::callAsync([state, i] {
                state->wrongThread |= ![NSThread isMainThread]
                    || !juce::MessageManager::getInstance()->isThisTheMessageThread();
                if (state->delivered < callbackCount) state->order[size_t(state->delivered)] = i;
                ++state->delivered;
            }) ? 1 : 0;
        state->posted += juce::MessageManager::callAsync([state] {
            state->wrongThread |= ![NSThread isMainThread]
                || !juce::MessageManager::getInstance()->isThisTheMessageThread();
            state->fence = true;
            state->fenceMs = juce::Time::getMillisecondCounterHiRes() - state->started;
        }) ? 1 : 0;

        const bool untouchedAtArm = state->delivered == 0 && !state->fence;
        state->started = juce::Time::getMillisecondCounterHiRes();
        armedProbe = state.get();
        const bool continues = chimeraTest::dispatchFor(budgetMs); // exactly one workload run
        const double dispatchMs = juce::Time::getMillisecondCounterHiRes() - state->started;
        const ProbeState frozen = *state;
        const bool stopSent = messages->hasStopMessageBeenSent();
        const bool fifo = orderedPrefix(frozen.order, frozen.delivered)
            && orderedPrefix(frozen.firstPollOrder, frozen.firstPollDelivered);
        const bool credible = frozen.posted == callbackCount + 1 && untouchedAtArm
            && continues && !stopSent && !frozen.wrongThread && fifo && frozen.polls == 1
            && frozen.pollBlockMs >= budgetMs
            && frozen.firstPollDelivered == frozen.delivered && frozen.firstPollFence == frozen.fence;
        // Identical predicate and thresholds in BOTH executables. The build
        // variant is a report label only; it cannot select an expected result.
        const bool passed = credible && frozen.firstPollDelivered == callbackCount
            && frozen.firstPollFence && frozen.fenceMs >= 0 && frozen.fenceMs <= budgetMs;
        const bool controlledRed = credible && frozen.firstPollDelivered == 4
            && !frozen.firstPollFence && frozen.fenceMs < 0;
        const int exitCode = passed ? 0 : controlledRed ? 1 : 2;
        const char* marker = passed ? "PASS_FIFO_FENCE_BEFORE_FIRST_APPKIT_POLL"
            : controlledRed ? "FAIL_FENCE_DEFERRED_BY_FIRST_APPKIT_POLL" : "INCONCLUSIVE_PROBE";
        // The verdict is sealed before disabling interception or GUI teardown.
        // Pending red callbacks own their state; teardown cannot change frozen.
        armedProbe = nullptr;

        out.setProperty("schema", 1);
        out.setProperty("marker", marker);
        out.setProperty("exit_code", exitCode);
        out.setProperty("budget_ms", budgetMs);
        out.setProperty("artificial_poll_ms", artificialPollMs);
        out.setProperty("posts_succeeded", frozen.posted);
        out.setProperty("untouched_at_arm", untouchedAtArm);
        out.setProperty("polls", frozen.polls);
        out.setProperty("first_poll_delivered", frozen.firstPollDelivered);
        out.setProperty("first_poll_fence", frozen.firstPollFence);
        out.setProperty("first_poll_order", asArray(frozen.firstPollOrder, frozen.firstPollDelivered));
        out.setProperty("return_delivered", frozen.delivered);
        out.setProperty("return_fence", frozen.fence);
        out.setProperty("return_order", asArray(frozen.order, frozen.delivered));
        out.setProperty("fifo", fifo);
        out.setProperty("first_poll_ms", frozen.firstPollMs);
        out.setProperty("fence_ms", frozen.fenceMs);
        out.setProperty("poll_block_ms", frozen.pollBlockMs);
        out.setProperty("dispatch_ms", dispatchMs);
        out.setProperty("pump_continues", continues);
        out.setProperty("stop_sent", stopSent);
        out.setProperty("wrong_thread", frozen.wrongThread);
        out.setProperty("native_subclass_verified", correctApplication);
        out.setProperty("main_thread_verified", correctThread);
        out.setProperty("outcome_frozen_before_cleanup", true);
        out.setProperty("release_acceptance", false);
        return writeReport(report, argv[1], exitCode);
    }
}
