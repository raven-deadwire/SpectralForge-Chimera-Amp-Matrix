#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace spectralforge {

// 1.4 constrained signal-order editor (docs/SIGNAL_ORDER_1_4.md). Each
// editable region is a short serial chain whose stages keep their parameters
// and automation; only the order changes. Orders are saved as state
// properties, not host parameters.
namespace signalOrder {

// A region of `N` stages identified by index; `ids` are the stable saved names.
template<int N> struct Region {
    using Order = std::array<int, N>;
    static constexpr Order identity() noexcept {Order o{};for (int i = 0; i < N; ++i) o[size_t(i)] = i;return o;}
    static bool valid(const Order& order) noexcept {
        std::array<bool, N> seen{};
        for (int stage : order) {
            if (stage < 0 || stage >= N || seen[size_t(stage)]) return false;
            seen[size_t(stage)] = true;
        }
        return true;
    }
    static juce::String toString(const Order& order, const std::array<const char*, N>& ids) {
        juce::StringArray names;
        for (int stage : order) names.add(ids[size_t(stage)]);
        return names.joinIntoString(",");
    }
    // Anything but a complete permutation of the known identities restores
    // the released order, so a damaged or future state never drops a stage.
    static Order fromString(const juce::String& text, const std::array<const char*, N>& ids) {
        const auto names = juce::StringArray::fromTokens(text, ",", {});
        if (names.size() != N) return identity();
        Order order{};
        for (int i = 0; i < N; ++i) {
            int found = -1;
            for (int s = 0; s < N; ++s) if (names[i].trim() == ids[size_t(s)]) found = s;
            if (found < 0) return identity();
            order[size_t(i)] = found;
        }
        return valid(order) ? order : identity();
    }
    // 3 bits per position, for lock-free publication to the audio thread.
    static int pack(const Order& order) noexcept {
        int code = 0;
        for (int i = 0; i < N; ++i) code |= order[size_t(i)] << (3 * i);
        return code;
    }
    static Order unpack(int code) noexcept {
        Order order{};
        for (int i = 0; i < N; ++i) order[size_t(i)] = (code >> (3 * i)) & 7;
        return valid(order) ? order : identity();
    }
    // Place `stage` at position `slot`, shifting the stages in between (a
    // drag-and-drop). An out-of-range slot leaves the order unchanged.
    static Order placed(Order order, int stage, int slot) noexcept {
        if (slot < 0 || slot >= N) return order;
        int from = -1;
        for (int i = 0; i < N; ++i) if (order[size_t(i)] == stage) from = i;
        if (from < 0) return order;
        while (from < slot) { std::swap(order[size_t(from)], order[size_t(from + 1)]); ++from; }
        while (from > slot) { std::swap(order[size_t(from)], order[size_t(from - 1)]); --from; }
        return order;
    }
    // Move one stage a single position earlier (-1) or later (+1). Returns the
    // input unchanged when the move would leave the region.
    static Order moved(Order order, int stage, int direction) noexcept {
        for (int i = 0; i < N; ++i)
            if (order[size_t(i)] == stage) {
                const int j = i + (direction < 0 ? -1 : 1);
                if (j >= 0 && j < N) std::swap(order[size_t(i)], order[size_t(j)]);
                break;
            }
        return order;
    }
};

// Audio-thread order switch. A reorder changes which signal feeds every
// stateful stage, so it fades the region's output out (10 ms), swaps the order
// while silent, keeps the output muted while the stages settle on their new
// inputs (10 ms), then fades in (10 ms). With no change pending the region is
// processed untouched. Works on packed codes.
class Switch {
public:
    void prepare(double sampleRate) noexcept {
        gain.reset(sampleRate, .01);
        settleLength = juce::jmax(1, int(sampleRate * .01));
        reset();
    }
    // Starts in the published order (after prepare, reset or state restore).
    void reset() noexcept {
        gain.setCurrentAndTargetValue(1.f);
        phase = Phase::idle;
        active = -1;
    }
    // The code to run for this block, given the latest published one.
    int begin(int requested) noexcept {
        if (active < 0) active = requested;
        if (requested != active) {
            if (phase == Phase::idle || phase == Phase::in) { phase = Phase::out; gain.setTargetValue(0.f); }
            if ((phase == Phase::out && !gain.isSmoothing()) || phase == Phase::settle) {
                active = requested; phase = Phase::settle; settleRemaining = settleLength;
            }
        }
        else if (phase == Phase::out) { phase = Phase::in; gain.setTargetValue(1.f); }   // change undone
        return active;
    }
    // Applies the transition gain to the processed block.
    void end(juce::AudioBuffer<float>& buffer) noexcept {
        if (phase == Phase::idle) return;
        for (int n = 0; n < buffer.getNumSamples(); ++n) {
            const float g = phase == Phase::settle ? 0.f : gain.getNextValue();
            for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.setSample(c, n, buffer.getSample(c, n) * g);
        }
        if (phase == Phase::settle && (settleRemaining -= buffer.getNumSamples()) <= 0) { phase = Phase::in; gain.setTargetValue(1.f); }
        else if (phase == Phase::in && !gain.isSmoothing()) phase = Phase::idle;
    }
    bool switching() const noexcept { return phase != Phase::idle; }

private:
    enum class Phase { idle, out, settle, in };
    juce::SmoothedValue<float> gain;
    Phase phase{Phase::idle};
    int active{-1}, settleRemaining{}, settleLength{1};
};

} // namespace signalOrder

// Region 1: the common block after the rig sum (and post-rig gates), before
// POST.
namespace commonOrder {
enum class Stage { toneEQ = 0, compressor = 1, multiband = 2, deesser = 3 };
inline constexpr int stageCount = 4;
using Region = signalOrder::Region<stageCount>;
using Order = std::array<Stage, stageCount>;
// The released order: Tone EQ > Clean Comp > Multiband > De-esser.
inline constexpr Order defaultOrder{Stage::toneEQ, Stage::compressor, Stage::multiband, Stage::deesser};
inline constexpr const char* propertyName = "commonOrder";
inline constexpr std::array<const char*, stageCount> ids{"toneEQ", "cleanComp", "multiband", "deEsser"};

inline const char* stageId(Stage stage) noexcept { return ids[size_t(stage)]; }
inline Region::Order raw(const Order& order) noexcept {Region::Order r{};for (int i = 0; i < stageCount; ++i) r[size_t(i)] = int(order[size_t(i)]);return r;}
inline Order typed(const Region::Order& order) noexcept {Order t{};for (int i = 0; i < stageCount; ++i) t[size_t(i)] = Stage(order[size_t(i)]);return t;}
inline bool valid(const Order& order) noexcept { return Region::valid(raw(order)); }
inline juce::String toString(const Order& order) { return Region::toString(raw(order), ids); }
inline Order fromString(const juce::String& text) { return typed(Region::fromString(text, ids)); }
inline int pack(const Order& order) noexcept { return Region::pack(raw(order)); }
inline Order unpack(int code) noexcept { return typed(Region::unpack(code)); }
inline Order moved(const Order& order, Stage stage, int direction) noexcept { return typed(Region::moved(raw(order), int(stage), direction)); }
inline Order placed(const Order& order, Stage stage, int slot) noexcept { return typed(Region::placed(raw(order), int(stage), slot)); }
} // namespace commonOrder

// Region 2: the POST rack's six slots. Stage numbers are the rack's section
// indices (meters, native sections and editor navigation stay keyed by them).
namespace postOrder {
enum class Stage { busComp = 0, preamp = 1, consoleEQ = 2, modulation = 3, delay = 4, reverb = 5 };
inline constexpr int stageCount = 6;
using Region = signalOrder::Region<stageCount>;
using Order = std::array<Stage, stageCount>;
// The released order: Bus Comp > Preamp > Console EQ > Modulation > Delay > Reverb.
inline constexpr Order defaultOrder{Stage::busComp, Stage::preamp, Stage::consoleEQ, Stage::modulation, Stage::delay, Stage::reverb};
inline constexpr const char* propertyName = "postOrder";
inline constexpr std::array<const char*, stageCount> ids{"busComp", "preamp", "consoleEQ", "modulation", "delay", "reverb"};

inline const char* stageId(Stage stage) noexcept { return ids[size_t(stage)]; }
inline Region::Order raw(const Order& order) noexcept {Region::Order r{};for (int i = 0; i < stageCount; ++i) r[size_t(i)] = int(order[size_t(i)]);return r;}
inline Order typed(const Region::Order& order) noexcept {Order t{};for (int i = 0; i < stageCount; ++i) t[size_t(i)] = Stage(order[size_t(i)]);return t;}
inline bool valid(const Order& order) noexcept { return Region::valid(raw(order)); }
inline juce::String toString(const Order& order) { return Region::toString(raw(order), ids); }
inline Order fromString(const juce::String& text) { return typed(Region::fromString(text, ids)); }
inline int pack(const Order& order) noexcept { return Region::pack(raw(order)); }
inline Order unpack(int code) noexcept { return typed(Region::unpack(code)); }
inline Order moved(const Order& order, Stage stage, int direction) noexcept { return typed(Region::moved(raw(order), int(stage), direction)); }
inline Order placed(const Order& order, Stage stage, int slot) noexcept { return typed(Region::placed(raw(order), int(stage), slot)); }
} // namespace postOrder

} // namespace spectralforge
