#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace spectralforge {
struct EQAnalyzerFrame {float inputL{},outputL{},inputR{},outputR{};};
class EQAnalyzerFIFO {
public:
    // Analysis-only scheduling headroom: 341.33 ms at 192 kHz. The old 8192
    // frames provided only 2.67 ms spare time beyond the nominal 40 ms UI tick.
    // A finite queue still reports real losses after longer consumer stalls.
    static constexpr uint32_t capacity=65536;
    void push(EQAnalyzerFrame frame) noexcept {
        const auto w=write.load(std::memory_order_relaxed);
        if(w-read.load(std::memory_order_acquire)>=capacity) {dropped.fetch_add(1,std::memory_order_relaxed);return;}
        frames[w%capacity]=frame;write.store(w+1,std::memory_order_release);
    }
    bool pop(EQAnalyzerFrame& frame) noexcept {
        const auto r=read.load(std::memory_order_relaxed);
        if(r==write.load(std::memory_order_acquire))return false;
        frame=frames[r%capacity];read.store(r+1,std::memory_order_release);return true;
    }
    // Consumer only. Snapshot/drain never reset the producer's index or touch
    // unpublished sample storage, so the audio producer can continue running.
    uint32_t pendingSamples() const noexcept {
        return write.load(std::memory_order_acquire)-read.load(std::memory_order_relaxed);
    }
    uint32_t discardPending() noexcept {
        const auto r=read.load(std::memory_order_relaxed);
        const auto w=write.load(std::memory_order_acquire);
        read.store(w,std::memory_order_release);return w-r;
    }
    std::atomic<bool> enabled{false};
    std::atomic<uint64_t> dropped{0};
private:
    std::array<EQAnalyzerFrame,capacity> frames{};
    std::atomic<uint32_t> read{0},write{0}; // Unsigned subtraction also works across wrap.
};
}
