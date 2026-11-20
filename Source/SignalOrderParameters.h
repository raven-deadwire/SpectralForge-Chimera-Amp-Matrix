#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SignalOrder.h"

// Host parameters and their reader for the 1.4 signal order
// (docs/SIGNAL_ORDER_1_4.md). Kept apart from SignalOrder.h so DSP-only code
// can use the order without the plugin framework.
namespace spectralforge {
inline void appendSignalOrderParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    for (const auto region : {OrderRegion::tone, OrderRegion::post})
        for (int slot = 0; slot < moduleCount(region); ++slot)
            layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{orderSlotID(region, slot), signalOrderParameterVersion},
                juce::String(region == OrderRegion::tone ? "Tone / dynamics" : "POST") + " position " + juce::String(slot + 1),
                0, moduleCount(region) - 1, slot, juce::AudioParameterIntAttributes().withAutomatable(false)
                    .withStringFromValueFunction([region](int module, int) { return moduleName(region, juce::jlimit(0, moduleCount(region) - 1, module)); })));
}
inline constexpr int signalOrderParameterCount = toneModuleCount + postModuleCount;

// Reads the slot parameters. A slot set mid-edit can leave a duplicate for a
// moment: an invalid region keeps `fallback` rather than jumping elsewhere.
class SignalOrderParameters {
public:
    void bind(juce::AudioProcessorValueTreeState& state) {
        for (int i = 0; i < toneModuleCount; ++i) tone[size_t(i)] = state.getRawParameterValue(orderSlotID(OrderRegion::tone, i));
        for (int i = 0; i < postModuleCount; ++i) post[size_t(i)] = state.getRawParameterValue(orderSlotID(OrderRegion::post, i));
    }
    SignalOrder read(const SignalOrder& fallback) const noexcept {
        SignalOrder o;
        for (int i = 0; i < toneModuleCount; ++i) o.tone.slots[size_t(i)] = tone[size_t(i)] ? juce::roundToInt(tone[size_t(i)]->load()) : i;
        for (int i = 0; i < postModuleCount; ++i) o.post.slots[size_t(i)] = post[size_t(i)] ? juce::roundToInt(post[size_t(i)]->load()) : i;
        if (!o.tone.valid()) o.tone = fallback.tone;
        if (!o.post.valid()) o.post = fallback.post;
        return o;
    }
private:
    std::array<std::atomic<float>*, toneModuleCount> tone{};
    std::array<std::atomic<float>*, postModuleCount> post{};
};

} // namespace spectralforge
