#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SignalOrder.h"
#include "PostSlots.h"

// Host parameters and their reader for the 1.4 signal order
// (docs/SIGNAL_ORDER_1_4.md). Kept apart from SignalOrder.h so DSP-only code
// can use the order without the plugin framework.
namespace spectralforge {
namespace signal_order_detail {
inline void addSlot(juce::AudioProcessorValueTreeState::ParameterLayout& layout, OrderRegion region, int slot, int version) {
    const int last = moduleCount(region) - 1;
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{orderSlotID(region, slot), version},
        juce::String(region == OrderRegion::tone ? "Tone / dynamics" : "POST") + " position " + juce::String(slot + 1),
        0, last, slot, juce::AudioParameterIntAttributes().withAutomatable(false)
            .withStringFromValueFunction([region, last](int module, int) { return moduleName(region, juce::jlimit(0, last, module)); })));
}
} // namespace signal_order_detail
// The released bank (version 29): orderTone1-6 and orderPost1-7. The POST
// slots hold a block 0-13 since the second slots (docs/POST_SECOND_SLOTS.md);
// stored values 0-6 keep their meaning. Not automatable.
inline void appendSignalOrderParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    for (int slot = 0; slot < toneModuleCount; ++slot) signal_order_detail::addSlot(layout, OrderRegion::tone, slot, signalOrderParameterVersion);
    for (int slot = 0; slot < postModuleCount; ++slot) signal_order_detail::addSlot(layout, OrderRegion::post, slot, signalOrderParameterVersion);
}
inline constexpr int signalOrderParameterCount = toneModuleCount + postModuleCount;
// POST positions 8-14 (orderPost8-14), version 35, part of the second-slot
// bank. Their defaults (blocks 7-13) put the second slots after the released
// order, so a project without them loads the released order unchanged.
inline void appendPostSecondSlotOrderParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    for (int slot = postModuleCount; slot < postBlockCount; ++slot) signal_order_detail::addSlot(layout, OrderRegion::post, slot, postSecondSlotParameterVersion);
}
inline constexpr int postSecondSlotOrderParameterCount = postBlockCount - postModuleCount;

// Reads the slot parameters. A slot set mid-edit can leave a duplicate for a
// moment: an invalid region keeps `fallback` rather than jumping elsewhere.
class SignalOrderParameters {
public:
    void bind(juce::AudioProcessorValueTreeState& state) {
        for (int i = 0; i < toneModuleCount; ++i) tone[size_t(i)] = state.getRawParameterValue(orderSlotID(OrderRegion::tone, i));
        for (int i = 0; i < postBlockCount; ++i) post[size_t(i)] = state.getRawParameterValue(orderSlotID(OrderRegion::post, i));
    }
    SignalOrder read(const SignalOrder& fallback) const noexcept {
        SignalOrder o;
        for (int i = 0; i < toneModuleCount; ++i) o.tone.slots[size_t(i)] = tone[size_t(i)] ? juce::roundToInt(tone[size_t(i)]->load()) : i;
        for (int i = 0; i < postBlockCount; ++i) o.post.slots[size_t(i)] = post[size_t(i)] ? juce::roundToInt(post[size_t(i)]->load()) : i;
        if (!o.tone.valid()) o.tone = fallback.tone;
        if (!o.post.valid()) o.post = fallback.post;
        return o;
    }
private:
    std::array<std::atomic<float>*, toneModuleCount> tone{};
    std::array<std::atomic<float>*, postBlockCount> post{};
};

} // namespace spectralforge
