#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "FXParameters.h"
#include "PostNativeParameters.h"
#include "CreativeFX.h"
#include "SignalOrderParameters.h"
#include "PostSlots.h"

// POST second slots (docs/POST_SECOND_SLOTS.md): the appended parameter bank,
// version hint 35, after every released parameter. It holds orderPost8-14 and
// a complete copy of every POST section's parameters under "slot2_" IDs:
// the released POST fields (bus compressor, preamp, EQ, modulation, delay,
// reverb), their model choices and model extensions, the native POST banks
// (models 0-5 of POST dynamics, preamp and EQ) and CREATIVE. Every default
// equals slot 1's default, so every second slot starts OFF / bypassed with
// its section's default model.
namespace spectralforge {
// The fxSpecs entries the POST rack reads (delay, reverb, bus compressor,
// preamp, EQ, modulation, delay sync).
inline constexpr std::array<int, 29> postLegacyFXIndices{{4, 5, 6, 7, 8, 9, 10, 11, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48}};
// Model families of the POST sections: bus, preamp, EQ, modulation, delay, reverb.
inline constexpr std::array<int, 6> postModelFamilies{{7, 8, 9, 10, 1, 2}};
inline constexpr std::array<int, 3> postExtendedFamilies{{1, 2, 10}};
inline bool isPostLegacyFXIndex(int index) noexcept { for (const int i : postLegacyFXIndices) if (i == index) return true; return false; }
inline bool isPostModelFamily(int family) noexcept { for (const int f : postModelFamilies) if (f == family) return true; return false; }

inline int postSecondSlotParameterCount() noexcept {
    return postSecondSlotOrderParameterCount + int(postLegacyFXIndices.size()) + int(postModelFamilies.size()) + int(postExtendedFamilies.size())
         + postNativeReleasedParameterCount() + postExpansionParameterCount() + creativeParameterCount;
}

inline void appendPostSecondSlotParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    constexpr int v = postSecondSlotParameterVersion;
    appendPostSecondSlotOrderParameters(layout);
    // Slot 1's host names for the fields registered by name (the rest use their ID).
    const auto legacyName = [](const juce::String& id) -> juce::String {
        static const std::array<std::pair<const char*, const char*>, 8> names{{{"delayon", "Post delay"}, {"delaytime", "Delay time"}, {"delayfeedback", "Delay feedback"}, {"delaymix", "Delay mix"},
            {"reverbon", "Post reverb"}, {"reverbsize", "Reverb room size"}, {"reverbdamping", "Reverb damping"}, {"reverbmix", "Reverb mix"}}};
        for (const auto& [key, name] : names) if (id == key) return name;
        return id;
    };
    for (const int index : postLegacyFXIndices) {
        const auto& spec = fxSpecs[size_t(index)];
        const juce::ParameterID id{postSlotID(1, spec.id), v};const auto name = postSlotName(1, legacyName(spec.id));
        if (spec.toggle) layout.add(std::make_unique<juce::AudioParameterBool>(id, name, spec.initial > .5f));
        else layout.add(std::make_unique<juce::AudioParameterFloat>(id, name, juce::NormalisableRange<float>{spec.minimum, spec.maximum, spec.interval}, spec.initial));
    }
    for (const int family : postModelFamilies) {
        const auto& f = modelFamilies[size_t(family)];
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{postSlotID(1, f.parameter), v}, postSlotName(1, juce::String(f.category) + " model"), modelNames(family), 0));
    }
    for (const int family : postExtendedFamilies)
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{postSlotID(1, effectExtensionID(family)), v},
            postSlotName(1, juce::String(modelFamilies[size_t(family)].category) + " model extension"), 0, 15, 0, juce::AudioParameterIntAttributes().withAutomatable(false)));
    addPostNativeParameters(layout, 1, v);
    appendPostExpansionParameters(layout, 1, v);
    appendCreativeParameters(layout, 1, v);
}

// The legacy POST fields and model choices of one slot, as the audio thread
// reads them. Slot 2 shares slot 1's pointers for everything that is not POST
// (PRE fields are never read by the POST rack).
struct PostSlotFXParameters {
    std::array<std::atomic<float>*, fxSpecs.size()> fx{};
    std::array<std::atomic<float>*, modelFamilies.size()> models{}, extensions{};
    void bind(juce::AudioProcessorValueTreeState& state, int slot) {
        for (size_t i = 0; i < fxSpecs.size(); ++i) fx[i] = state.getRawParameterValue(isPostLegacyFXIndex(int(i)) ? postSlotID(slot, fxSpecs[i].id) : juce::String(fxSpecs[i].id));
        for (size_t i = 0; i < modelFamilies.size(); ++i) {
            const bool post = isPostModelFamily(int(i));
            models[i] = state.getRawParameterValue(post ? postSlotID(slot, modelFamilies[i].parameter) : juce::String(modelFamilies[i].parameter));
            extensions[i] = effectFamilyExtended(int(i)) ? state.getRawParameterValue(post ? postSlotID(slot, effectExtensionID(int(i))) : effectExtensionID(int(i))) : nullptr;
        }
    }
};
} // namespace spectralforge
