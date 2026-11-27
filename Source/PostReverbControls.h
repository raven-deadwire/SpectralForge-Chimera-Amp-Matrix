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

// The tempo shape (docs/POST_REVERB_SHAPE.md): shape, depth and length for every slot's reverb, appended at
// version hint 44. With the shape Off, the default, the reverb renders as it did before.
inline constexpr int postReverbShapeParameterVersion = 44, postReverbShapePerSlot = 3;
inline constexpr int postReverbShapeParameterCount = postReverbShapePerSlot * postSlotCount;
inline constexpr std::array<const char*, postReverbShapePerSlot> reverbShapeIDs{{"reverbshape", "reverbshapedepth", "reverbshapelength"}};
inline constexpr std::array<const char*, postReverbShapePerSlot> reverbShapeNames{{"Reverb shape", "Reverb shape depth", "Reverb shape length"}};
inline juce::StringArray reverbShapeChoices() { return {"Off", "Swell", "Reverse", "Gate", "Wave", "Pump"}; }
inline juce::StringArray reverbShapeLengthChoices() { return {"1/4", "1/2", "1 bar", "2 bars"}; }
inline constexpr int reverbShapeDefaultLength = 2;
inline juce::String reverbShapeID(int slot, int control) { return postSlotID(slot, reverbShapeIDs[size_t(control)]); }
inline void appendPostReverbShapeParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    constexpr int v = postReverbShapeParameterVersion;
    for (int slot = 0; slot < postSlotCount; ++slot) {
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{reverbShapeID(slot, 0), v}, postSlotName(slot, reverbShapeNames[0]), reverbShapeChoices(), 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{reverbShapeID(slot, 1), v}, postSlotName(slot, reverbShapeNames[1]), juce::NormalisableRange<float>{0.f, 1.f}, 1.f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{reverbShapeID(slot, 2), v}, postSlotName(slot, reverbShapeNames[2]), reverbShapeLengthChoices(), reverbShapeDefaultLength));
    }
}

struct PostReverbControlParameters {
    std::array<std::atomic<float>*, postReverbControlsPerSlot> values{};
    std::array<std::atomic<float>*, postReverbShapePerSlot> shape{};
    void bind(juce::AudioProcessorValueTreeState& state, int slot) {
        for (int i = 0; i < postReverbControlsPerSlot; ++i) values[size_t(i)] = state.getRawParameterValue(reverbControlID(slot, i));
        for (int i = 0; i < postReverbShapePerSlot; ++i) shape[size_t(i)] = state.getRawParameterValue(reverbShapeID(slot, i));
    }
    // tempo and ppq are the session's, set by the caller.
    ReverbControls read() const noexcept {
        ReverbControls k;
        k.preDelayMs = values[0]->load();k.lowCutHz = values[1]->load();k.tone = values[2]->load();k.width = values[3]->load();k.freeze = values[4]->load() > .5f;
        k.shape = juce::roundToInt(shape[0]->load());k.shapeDepth = shape[1]->load();k.shapeLength = juce::roundToInt(shape[2]->load());
        return k;
    }
};
} // namespace spectralforge
