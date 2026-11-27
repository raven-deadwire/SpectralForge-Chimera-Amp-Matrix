#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PostSlots.h"
#include "ReverbControls.h"

// POST reverb controls (docs/POST_REVERB_CONTROLS.md): pre-delay, low cut,
// tone, width and freeze for the POST reverb of every slot. Appended at
// version hint 43, after every earlier parameter. At their defaults the
// reverb renders exactly as it did before they existed.
namespace spectralforge {
inline constexpr int postReverbControlsParameterVersion = 43, postReverbControlsPerSlot = 5;
inline constexpr int postReverbControlsParameterCount = postReverbControlsPerSlot * postSlotCount;
struct ReverbControlSpec {const char* id;const char* name;float minimum,maximum,initial;bool toggle;};
inline constexpr std::array<ReverbControlSpec, postReverbControlsPerSlot> reverbControlSpecs{{
    {"reverbpredelay", "Reverb pre-delay", 0.f, reverbPreDelayMaxMs, 0.f, false},
    {"reverblowcut", "Reverb low cut", reverbLowCutMinHz, reverbLowCutMaxHz, reverbLowCutMinHz, false},
    {"reverbtone", "Reverb tone", -1.f, 1.f, 0.f, false},
    {"reverbwidth", "Reverb width", 0.f, reverbWidthMax, 1.f, false},
    {"reverbfreeze", "Reverb freeze", 0.f, 1.f, 0.f, true}}};
inline juce::String reverbControlID(int slot, int control) { return postSlotID(slot, reverbControlSpecs[size_t(control)].id); }

// Slot 1 first, then slots 2 to 4, five parameters each.
inline void appendPostReverbControlParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    for (int slot = 0; slot < postSlotCount; ++slot)
        for (int control = 0; control < postReverbControlsPerSlot; ++control) {
            const auto& spec = reverbControlSpecs[size_t(control)];
            const juce::ParameterID id{reverbControlID(slot, control), postReverbControlsParameterVersion};const auto name = postSlotName(slot, spec.name);
            if (spec.toggle) layout.add(std::make_unique<juce::AudioParameterBool>(id, name, spec.initial > .5f));
            else layout.add(std::make_unique<juce::AudioParameterFloat>(id, name, juce::NormalisableRange<float>{spec.minimum, spec.maximum}, spec.initial));
        }
}

struct PostReverbControlParameters {
    std::array<std::atomic<float>*, postReverbControlsPerSlot> values{};
    void bind(juce::AudioProcessorValueTreeState& state, int slot) { for (int i = 0; i < postReverbControlsPerSlot; ++i) values[size_t(i)] = state.getRawParameterValue(reverbControlID(slot, i)); }
    ReverbControls read() const noexcept {
        ReverbControls k;
        k.preDelayMs = values[0]->load();k.lowCutHz = values[1]->load();k.tone = values[2]->load();k.width = values[3]->load();k.freeze = values[4]->load() > .5f;
        return k;
    }
};
} // namespace spectralforge
