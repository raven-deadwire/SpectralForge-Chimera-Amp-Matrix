#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace spectralforge::commonOrder {

// 1.4 constrained signal-order editor, first region (docs/SIGNAL_ORDER_1_4.md):
// the common pre-POST block after the rig merge (and post-rig gates) and
// before POST. Its four stages keep their parameters and automation; only the
// order changes. The order is saved as a state property, not a host parameter.
enum class Stage { toneEQ = 0, compressor = 1, multiband = 2, deesser = 3 };
inline constexpr int stageCount = 4;
using Order = std::array<Stage, stageCount>;
// The released order: Tone EQ > Clean Comp > Multiband > De-esser.
inline constexpr Order defaultOrder{Stage::toneEQ, Stage::compressor, Stage::multiband, Stage::deesser};
inline constexpr const char* propertyName = "commonOrder";

// Stable identities used in saved state, independent of display labels.
inline const char* stageId(Stage stage) noexcept {
    switch (stage) {
        case Stage::toneEQ: return "toneEQ";
        case Stage::compressor: return "cleanComp";
        case Stage::multiband: return "multiband";
        case Stage::deesser: return "deEsser";
    }
    return "";
}
inline bool valid(const Order& order) noexcept {
    std::array<bool, stageCount> seen{};
    for (auto stage : order) {
        const int index = int(stage);
        if (index < 0 || index >= stageCount || seen[size_t(index)]) return false;
        seen[size_t(index)] = true;
    }
    return true;
}
inline juce::String toString(const Order& order) {
    juce::StringArray ids;
    for (auto stage : order) ids.add(stageId(stage));
    return ids.joinIntoString(",");
}
// Anything but a complete permutation of the four known identities restores
// the released order, so a damaged or future state never drops a stage.
inline Order fromString(const juce::String& text) {
    const auto ids = juce::StringArray::fromTokens(text, ",", {});
    if (ids.size() != stageCount) return defaultOrder;
    Order order{};
    for (int i = 0; i < stageCount; ++i) {
        int found = -1;
        for (int s = 0; s < stageCount; ++s) if (ids[i].trim() == stageId(Stage(s))) found = s;
        if (found < 0) return defaultOrder;
        order[size_t(i)] = Stage(found);
    }
    return valid(order) ? order : defaultOrder;
}
// 2 bits per position, for lock-free publication to the audio thread.
inline int pack(const Order& order) noexcept {
    int code = 0;
    for (int i = 0; i < stageCount; ++i) code |= int(order[size_t(i)]) << (2 * i);
    return code;
}
inline Order unpack(int code) noexcept {
    Order order{};
    for (int i = 0; i < stageCount; ++i) order[size_t(i)] = Stage((code >> (2 * i)) & 3);
    return valid(order) ? order : defaultOrder;
}
// Move one stage a single position earlier (-1) or later (+1). Returns the
// input unchanged when the move would leave the region.
inline Order moved(Order order, Stage stage, int direction) noexcept {
    for (int i = 0; i < stageCount; ++i)
        if (order[size_t(i)] == stage) {
            const int j = i + (direction < 0 ? -1 : 1);
            if (j >= 0 && j < stageCount) std::swap(order[size_t(i)], order[size_t(j)]);
            break;
        }
    return order;
}

// Audio-thread order switch. A reorder changes which signal feeds every
// stateful stage, so it fades the block's output out (10 ms), swaps the order
// while silent, keeps the output muted while the stages settle on their new
// inputs (10 ms), then fades in (10 ms). With no change pending the block is
// processed untouched.
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
    // The order to run for this block, given the latest published code.
    Order begin(int requested) noexcept {
        if (active < 0) active = requested;
        if (requested != active) {
            if (phase == Phase::idle || phase == Phase::in) { phase = Phase::out; gain.setTargetValue(0.f); }
            if ((phase == Phase::out && !gain.isSmoothing()) || phase == Phase::settle) {
                active = requested; phase = Phase::settle; settleRemaining = settleLength;
            }
        }
        else if (phase == Phase::out) { phase = Phase::in; gain.setTargetValue(1.f); }   // change undone
        return unpack(active);
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

} // namespace spectralforge::commonOrder
