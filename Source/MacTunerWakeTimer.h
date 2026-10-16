#pragma once
#if JUCE_MAC
#include <dispatch/dispatch.h>

namespace spectralforge {
// Owned and changed by the tuner worker only. No dispatch calls on the audio
// callback. A strict timer signals the existing interruptible Thread event;
// it does not run the estimator or change host/process scheduling policy.
class MacTunerWakeTimer final {
public:
    explicit MacTunerWakeTimer(juce::Thread& target,bool deliver=true) : worker(target),deliverNotifications(deliver) {}
    ~MacTunerWakeTimer() { stop(); }
    MacTunerWakeTimer(const MacTunerWakeTimer&) = delete;
    MacTunerWakeTimer& operator=(const MacTunerWakeTimer&) = delete;
    bool start() {
        if(source) return true;
        cancelled.reset();
        source=dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,0,DISPATCH_TIMER_STRICT,
                                      dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE,0));
        if(!source) return false;
        dispatch_set_context(source,this);
        dispatch_source_set_event_handler_f(source,[](void* context) {
            auto& timer=*static_cast<MacTunerWakeTimer*>(context);
            if(timer.deliverNotifications)timer.worker.notify();
        });
        dispatch_source_set_cancel_handler_f(source,[](void* context) {
            static_cast<MacTunerWakeTimer*>(context)->cancelled.signal();
        });
        constexpr uint64_t interval=10*NSEC_PER_MSEC;
        dispatch_source_set_timer(source,dispatch_time(DISPATCH_TIME_NOW,interval),interval,0);
        dispatch_resume(source);
        return true;
    }
    void stop() {
        if(!source) return;
        dispatch_source_cancel(source);
        // Cancellation completion runs only after the last event handler.
        // Keep the owner/context alive until no notification can be in flight.
        cancelled.wait(-1);
        dispatch_release(source);source=nullptr;
    }
    bool active() const noexcept { return source!=nullptr; }
private:
    juce::Thread& worker;
    const bool deliverNotifications;
    juce::WaitableEvent cancelled;
    dispatch_source_t source{};
};
}
#endif
