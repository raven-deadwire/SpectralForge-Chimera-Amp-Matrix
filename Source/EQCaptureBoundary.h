#pragma once
#include <atomic>
#include <cstdint>

namespace spectralforge {

// Analysis-only handoff for the processor's serialized audio callbacks.
// Consumer: disable the selected bank's capture, request a drain, wait by
// polling isDrained(), then discard its FIFO/partial window before re-enabling.
// An in-flight callback acknowledges only the request seen at its own entry,
// so it cannot release a later consumer request while still publishing samples.
class EQCaptureBoundary {
public:
    uint64_t requestDrain() noexcept {
        return requested.fetch_add(1,std::memory_order_acq_rel)+1;
    }
    uint64_t latestRequest() const noexcept {return requested.load(std::memory_order_acquire);}
    bool isDrained(uint64_t request) const noexcept {
        return acknowledged.load(std::memory_order_acquire)>=request;
    }
    class CallbackScope {
    public:
        explicit CallbackScope(EQCaptureBoundary& owner) noexcept
            :boundary(owner),request(owner.requested.load(std::memory_order_acquire)){}
        ~CallbackScope() noexcept {boundary.acknowledged.store(request,std::memory_order_release);}
        CallbackScope(const CallbackScope&)=delete;
        CallbackScope& operator=(const CallbackScope&)=delete;
    private:
        EQCaptureBoundary& boundary;const uint64_t request;
    };
private:
    std::atomic<uint64_t> requested{0},acknowledged{0};
};
}
