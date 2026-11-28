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
inline juce::StringArray reverbShapeChoices() { return {"Off", "Rise", "Rewind", "Chop", "Tide", "Duck"}; }
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

// DECAY (docs/POST_REVERB_DECAY.md): the reverb's decay time in seconds for every slot, appended at version
// hint 45. Below reverbDecayMinSeconds, the default, SIZE sets the decay and the reverb renders as it did.
inline constexpr int postReverbDecayParameterVersion = 45, postReverbDecayPerSlot = 1;
inline constexpr int postReverbDecayParameterCount = postReverbDecayPerSlot * postSlotCount;
inline juce::String reverbDecayID(int slot) { return postSlotID(slot, "reverbdecay"); }
inline juce::String reverbDecayText(float seconds) { return seconds < reverbDecayMinSeconds ? juce::String("SIZE") : juce::String(seconds, seconds < 10.f ? 2 : 1) + " s"; }
inline void appendPostReverbDecayParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    juce::NormalisableRange<float> range{0.f, reverbDecayMaxSeconds};range.setSkewForCentre(2.f);
    for (int slot = 0; slot < postSlotCount; ++slot)
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{reverbDecayID(slot), postReverbDecayParameterVersion}, postSlotName(slot, "Reverb decay"), range, 0.f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return reverbDecayText(v); })
                .withValueFromStringFunction([](const juce::String& t) { return t.trim().equalsIgnoreCase("SIZE") ? 0.f : t.getFloatValue(); })));
}
// More of the tempo shape (docs/POST_REVERB_SHAPE_MORE.md): bars or seconds, the length in seconds, the curve and
// the tone morph for every slot, appended at version hint 46. At the defaults the shape is what it was.
inline constexpr int postReverbShapeMoreParameterVersion = 46, postReverbShapeMorePerSlot = 4;
inline constexpr int postReverbShapeMoreParameterCount = postReverbShapeMorePerSlot * postSlotCount;
inline constexpr std::array<const char*, postReverbShapeMorePerSlot> reverbShapeMoreIDs{{"reverbshapeunit", "reverbshapeseconds", "reverbshapecurve", "reverbshapemorph"}};
inline constexpr std::array<const char*, postReverbShapeMorePerSlot> reverbShapeMoreNames{{"Reverb shape unit", "Reverb shape seconds", "Reverb shape curve", "Reverb shape morph"}};
inline juce::StringArray reverbShapeUnitChoices() { return {"Bars", "Seconds"}; }
inline juce::String reverbShapeMoreID(int slot, int control) { return postSlotID(slot, reverbShapeMoreIDs[size_t(control)]); }
inline void appendPostReverbShapeMoreParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    constexpr int v = postReverbShapeMoreParameterVersion;
    juce::NormalisableRange<float> seconds{reverbShapeMinSeconds, reverbShapeMaxSeconds};seconds.setSkewForCentre(1.f);
    for (int slot = 0; slot < postSlotCount; ++slot) {
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{reverbShapeMoreID(slot, 0), v}, postSlotName(slot, reverbShapeMoreNames[0]), reverbShapeUnitChoices(), 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{reverbShapeMoreID(slot, 1), v}, postSlotName(slot, reverbShapeMoreNames[1]), seconds, 1.f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{reverbShapeMoreID(slot, 2), v}, postSlotName(slot, reverbShapeMoreNames[2]), juce::NormalisableRange<float>{-1.f, 1.f}, 0.f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{reverbShapeMoreID(slot, 3), v}, postSlotName(slot, reverbShapeMoreNames[3]), juce::NormalisableRange<float>{-1.f, 1.f}, 0.f));
    }
}
// Character (docs/POST_REVERB_CHARACTER.md): converter, dirt and drift for every slot's reverb, appended at
// version hint 47. At the defaults (Off, 0, 0) the reverb renders as it did.
inline constexpr int postReverbCharacterParameterVersion = 47, postReverbCharacterPerSlot = 3;
inline constexpr int postReverbCharacterParameterCount = postReverbCharacterPerSlot * postSlotCount;
inline constexpr std::array<const char*, postReverbCharacterPerSlot> reverbCharacterIDs{{"reverbconverter", "reverbdirt", "reverbdrift"}};
inline constexpr std::array<const char*, postReverbCharacterPerSlot> reverbCharacterNames{{"Reverb converter", "Reverb dirt", "Reverb drift"}};
inline juce::StringArray reverbConverterChoices() { return {"Off", "Early", "Middle", "Late"}; }
inline juce::String reverbCharacterID(int slot, int control) { return postSlotID(slot, reverbCharacterIDs[size_t(control)]); }
inline void appendPostReverbCharacterParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    constexpr int v = postReverbCharacterParameterVersion;
    for (int slot = 0; slot < postSlotCount; ++slot) {
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{reverbCharacterID(slot, 0), v}, postSlotName(slot, reverbCharacterNames[0]), reverbConverterChoices(), 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{reverbCharacterID(slot, 1), v}, postSlotName(slot, reverbCharacterNames[1]), juce::NormalisableRange<float>{0.f, 1.f}, 0.f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{reverbCharacterID(slot, 2), v}, postSlotName(slot, reverbCharacterNames[2]), juce::NormalisableRange<float>{0.f, 1.f}, 0.f));
    }
}
// Every reverb bank appended after the shape's. The parameter-count checks of the tests count from the end
// through this, so a later reverb bank is added here and nowhere else.
inline constexpr int postReverbAfterShapeParameterCount = postReverbDecayParameterCount + postReverbShapeMoreParameterCount + postReverbCharacterParameterCount;
// The reverb banks appended to every slot after the slots' own banks.
inline constexpr bool isPostReverbAddedVersion(int version) noexcept { return version == postReverbControlsParameterVersion || version == postReverbShapeParameterVersion || version == postReverbDecayParameterVersion || version == postReverbShapeMoreParameterVersion || version == postReverbCharacterParameterVersion; }

struct PostReverbControlParameters {
    std::array<std::atomic<float>*, postReverbControlsPerSlot> values{};
    std::array<std::atomic<float>*, postReverbShapePerSlot> shape{};
    std::atomic<float>* decay{};
    std::array<std::atomic<float>*, postReverbShapeMorePerSlot> more{};
    std::array<std::atomic<float>*, postReverbCharacterPerSlot> character{};
    void bind(juce::AudioProcessorValueTreeState& state, int slot) {
        for (int i = 0; i < postReverbControlsPerSlot; ++i) values[size_t(i)] = state.getRawParameterValue(reverbControlID(slot, i));
        for (int i = 0; i < postReverbShapePerSlot; ++i) shape[size_t(i)] = state.getRawParameterValue(reverbShapeID(slot, i));
        decay = state.getRawParameterValue(reverbDecayID(slot));
        for (int i = 0; i < postReverbShapeMorePerSlot; ++i) more[size_t(i)] = state.getRawParameterValue(reverbShapeMoreID(slot, i));
        for (int i = 0; i < postReverbCharacterPerSlot; ++i) character[size_t(i)] = state.getRawParameterValue(reverbCharacterID(slot, i));
    }
    // tempo and ppq are the session's, set by the caller.
    ReverbControls read() const noexcept {
        ReverbControls k;
        k.preDelayMs = values[0]->load();k.lowCutHz = values[1]->load();k.tone = values[2]->load();k.width = values[3]->load();k.freeze = values[4]->load() > .5f;
        k.shape = juce::roundToInt(shape[0]->load());k.shapeDepth = shape[1]->load();k.shapeLength = juce::roundToInt(shape[2]->load());
        k.decaySeconds = decay->load();
        k.shapeInSeconds = more[0]->load() > .5f;k.shapeSeconds = more[1]->load();k.shapeCurve = more[2]->load();k.shapeMorph = more[3]->load();
        k.converter = juce::roundToInt(character[0]->load());k.dirt = character[1]->load();k.drift = character[2]->load();
        return k;
    }
};
} // namespace spectralforge
