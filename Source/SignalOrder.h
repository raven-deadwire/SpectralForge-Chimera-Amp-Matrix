#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

// 1.4 constrained signal-order editor (docs/SIGNAL_ORDER_1_4.md,
// docs/SIGNAL_PATH_EDITOR.md). Two serial regions can be reordered:
//  - the tone / dynamics block between the rig sum (after the optional
//    post-rig gate and the Room return) and POST;
//  - the POST rack.
// Everything else keeps its fixed place. Each slot parameter holds the module
// in that position; modules keep their own parameter banks, so moving one
// never exchanges banks or redirects automation. The order parameters are not
// automatable. Missing order parameters (old projects) load the old order.
namespace spectralforge {
inline constexpr int signalOrderParameterVersion = 29;
enum class OrderRegion { tone = 0, post = 1 };
inline constexpr int toneModuleCount = 6, postModuleCount = 7;
enum ToneModule { toneEQModule, compressorModule, multibandModule, saturationModule, filterModule, deesserModule };
enum PostModule { postDynamicsModule, postPreampModule, postEQModule, postModulationModule, postDelayModule, postCreativeModule, postReverbModule };
inline int moduleCount(OrderRegion r) noexcept { return r == OrderRegion::tone ? toneModuleCount : postModuleCount; }
inline juce::StringArray toneModuleNames() { return {"Tone EQ", "Clean compressor", "Multiband", "Saturation", "Filter", "De-esser"}; }
inline juce::StringArray postModuleNames() { return {"POST dynamics", "POST preamp", "POST EQ", "Modulation", "Delay", "Creative", "Reverb"}; }
inline juce::String moduleName(OrderRegion r, int module) { return (r == OrderRegion::tone ? toneModuleNames() : postModuleNames())[module]; }
inline juce::String orderSlotID(OrderRegion r, int slot) { return juce::String(r == OrderRegion::tone ? "orderTone" : "orderPost") + juce::String(slot + 1); }
// Signal-path node of each module.
inline juce::String moduleNodeID(OrderRegion r, int module) {
    static const std::array<const char*, toneModuleCount> tone{"tone.eq", "common.comp", "common.multiband", "common.saturation", "common.filter", "common.deesser"};
    static const std::array<const char*, postModuleCount> post{"post.section0", "post.section1", "post.section2", "post.section3", "post.section4", "post.creative", "post.section5"};
    return r == OrderRegion::tone ? tone[size_t(module)] : post[size_t(module)];
}

// One region's order: slot -> module. Valid when it is a permutation.
template <int N> struct RegionOrder {
    std::array<int, N> slots{};
    static RegionOrder initial() noexcept { RegionOrder o; for (int i = 0; i < N; ++i) o.slots[size_t(i)] = i; return o; }
    bool valid() const noexcept {
        std::array<bool, N> seen{};
        for (const int m : slots) { if (m < 0 || m >= N || seen[size_t(m)]) return false; seen[size_t(m)] = true; }
        return true;
    }
    int position(int module) const noexcept { for (int i = 0; i < N; ++i) if (slots[size_t(i)] == module) return i; return -1; }
    // Moves `module` to `target` (clamped), shifting the others; returns the new order.
    RegionOrder placed(int module, int target) const noexcept {
        RegionOrder o = *this;const int from = position(module);
        if (from < 0) return o;
        target = juce::jlimit(0, N - 1, target);
        if (target > from) for (int i = from; i < target; ++i) o.slots[size_t(i)] = slots[size_t(i + 1)];
        else for (int i = from; i > target; --i) o.slots[size_t(i)] = slots[size_t(i - 1)];
        o.slots[size_t(target)] = module;
        return o;
    }
    bool isInitial() const noexcept { return *this == initial(); }
    bool operator==(const RegionOrder&) const = default;
};
using ToneOrder = RegionOrder<toneModuleCount>;
using PostOrder = RegionOrder<postModuleCount>;
struct SignalOrder {
    ToneOrder tone = ToneOrder::initial();
    PostOrder post = PostOrder::initial();
    bool operator==(const SignalOrder&) const = default;
};

// Click-safe change at a region's output: fade out (5 ms, raised cosine),
// switch at silence, fade in (5 ms). The switch happens at a block boundary,
// so a dip lasts at most 10 ms plus the rest of the block in which silence is
// reached.
class OrderSwitch {
public:
    void prepare(double rate) noexcept { length = std::max(1, int(.005 * rate)); position = length; phase = Phase::idle; }
    // Call at the start of a block. Returns true when the new order applies now.
    bool begin(bool changed, bool immediate) noexcept {
        if (!changed) { if (phase == Phase::out || phase == Phase::silent) phase = Phase::in; return false; }   // change reverted: fade back in
        if (immediate) { position = length; phase = Phase::idle; return true; }
        if (phase == Phase::silent) { phase = Phase::in; return true; }
        if (phase != Phase::out) phase = Phase::out;
        return false;
    }
    void apply(juce::AudioBuffer<float>& buffer) noexcept {
        if (phase == Phase::idle) return;
        for (int n = 0; n < buffer.getNumSamples(); ++n) {
            if (phase == Phase::out) { if (position > 0) --position; if (position == 0) phase = Phase::silent; }
            else if (phase == Phase::in) { if (position < length) ++position; if (position == length) phase = Phase::idle; }
            const float gain = phase == Phase::silent ? 0.f : float(.5 - .5 * std::cos(3.141592653589793 * position / length));
            for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.getWritePointer(c)[n] *= gain;
        }
    }
    bool switching() const noexcept { return phase != Phase::idle; }
private:
    enum class Phase { idle, out, silent, in };
    Phase phase{Phase::idle};int length{240}, position{240};
};
} // namespace spectralforge
