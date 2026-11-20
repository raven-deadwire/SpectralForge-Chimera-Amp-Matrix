#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

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
// POST: seven sections, each with two slots (docs/POST_SECOND_SLOTS.md). The
// POST region orders 14 blocks: block b is section b % 7 in slot b / 7, so
// blocks 0-6 are the released modules (slot 1) and 7-13 their second slots.
inline constexpr int toneModuleCount = 6, postModuleCount = 7, postBlockCount = 2 * postModuleCount;
enum ToneModule { toneEQModule, compressorModule, multibandModule, saturationModule, filterModule, deesserModule };
enum PostModule { postDynamicsModule, postPreampModule, postEQModule, postModulationModule, postDelayModule, postCreativeModule, postReverbModule };
inline constexpr int postBlock(int section, int slot) noexcept { return section + slot * postModuleCount; }
inline constexpr int postBlockSection(int block) noexcept { return block % postModuleCount; }
inline constexpr int postBlockSlot(int block) noexcept { return block / postModuleCount; }
// Movable blocks in a region (POST: 14).
inline int moduleCount(OrderRegion r) noexcept { return r == OrderRegion::tone ? toneModuleCount : postBlockCount; }
inline juce::StringArray toneModuleNames() { return {"Tone EQ", "Clean compressor", "Multiband", "Saturation", "Filter", "De-esser"}; }
inline juce::StringArray postModuleNames() { return {"POST dynamics", "POST preamp", "POST EQ", "Modulation", "Delay", "Creative", "Reverb"}; }
// "Delay" for slot 1 (the released name), "Delay 2" for slot 2.
inline juce::String postBlockName(int block) { return postModuleNames()[postBlockSection(block)] + (postBlockSlot(block) > 0 ? " 2" : ""); }
inline juce::String moduleName(OrderRegion r, int module) { return r == OrderRegion::tone ? toneModuleNames()[module] : postBlockName(module); }
inline juce::String orderSlotID(OrderRegion r, int slot) { return juce::String(r == OrderRegion::tone ? "orderTone" : "orderPost") + juce::String(slot + 1); }
// Signal-path node of each module (slot 2 blocks: "post2.section4", ...).
inline juce::String moduleNodeID(OrderRegion r, int module) {
    static const std::array<const char*, toneModuleCount> tone{"tone.eq", "common.comp", "common.multiband", "common.saturation", "common.filter", "common.deesser"};
    static const std::array<const char*, postModuleCount> post{"post.section0", "post.section1", "post.section2", "post.section3", "post.section4", "post.creative", "post.section5"};
    if (r == OrderRegion::tone) return tone[size_t(module)];
    const juce::String id = post[size_t(postBlockSection(module))];
    return postBlockSlot(module) > 0 ? id.replace("post.", "post2.") : id;
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
// The POST order holds all 14 blocks. Its initial order is the released one
// followed by the second slots, which is also what an old project (without
// orderPost8-14) loads.
using PostOrder = RegionOrder<postBlockCount>;
struct SignalOrder {
    ToneOrder tone = ToneOrder::initial();
    PostOrder post = PostOrder::initial();
    bool operator==(const SignalOrder&) const = default;
};

// 1.9 routing polish (docs/MODULATION_1_9.md): named orders for each region,
// applied as one order command (one undo entry).
// A preset names the order of the seven POST sections (or six tone modules).
// Applied to POST it orders the slot 1 blocks and puts the second slots after
// them, in the same section order (presetOrder()).
struct OrderPreset { const char* name; std::array<int, postModuleCount> slots; };
inline const std::array<OrderPreset, 4>& tonePresets() {
    static const std::array<OrderPreset, 4> presets{{
        {"Released order", {toneEQModule, compressorModule, multibandModule, saturationModule, filterModule, deesserModule}},
        {"Dynamics before Tone EQ", {compressorModule, multibandModule, toneEQModule, saturationModule, filterModule, deesserModule}},
        {"Colour last (saturation and filter after the dynamics)", {toneEQModule, compressorModule, multibandModule, deesserModule, filterModule, saturationModule}},
        {"De-ess first", {deesserModule, toneEQModule, compressorModule, multibandModule, saturationModule, filterModule}}}};
    return presets;
}
inline const std::array<OrderPreset, 4>& postPresets() {
    static const std::array<OrderPreset, 4> presets{{
        {"Released order", {postDynamicsModule, postPreampModule, postEQModule, postModulationModule, postDelayModule, postCreativeModule, postReverbModule}},
        {"POST EQ before dynamics", {postEQModule, postDynamicsModule, postPreampModule, postModulationModule, postDelayModule, postCreativeModule, postReverbModule}},
        {"Modulation after the delay and reverb", {postDynamicsModule, postPreampModule, postEQModule, postDelayModule, postCreativeModule, postReverbModule, postModulationModule}},
        {"Reverb into delay", {postDynamicsModule, postPreampModule, postEQModule, postModulationModule, postReverbModule, postDelayModule, postCreativeModule}}}};
    return presets;
}
inline const std::array<OrderPreset, 4>& orderPresets(OrderRegion r) { return r == OrderRegion::tone ? tonePresets() : postPresets(); }
// The full slot list a preset applies (tone: 6 modules, POST: 14 blocks).
inline std::vector<int> presetOrder(OrderRegion r, const OrderPreset& preset) {
    std::vector<int> slots;
    if (r == OrderRegion::tone) { for (int i = 0; i < toneModuleCount; ++i) slots.push_back(preset.slots[size_t(i)]); return slots; }
    for (int slot = 0; slot < 2; ++slot) for (int i = 0; i < postModuleCount; ++i) slots.push_back(postBlock(preset.slots[size_t(i)], slot));
    return slots;
}

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
