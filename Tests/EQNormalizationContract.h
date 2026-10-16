#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <cstring>

namespace eqNormalization {
inline bool bits(float a, float b) noexcept { return std::memcmp(&a, &b, sizeof a) == 0; }

// JUCE 8.0.8 ParameterAdapter deliberately constructs its raw cache without
// snapping. setDenormalisedValue can then retain that cache on an approximately
// equal restore. This is a specific representation, NOT an arbitrary ULP band.
inline float constructorRaw(const juce::RangedAudioParameter& p) {
    return p.getNormalisableRange().convertFrom0to1(p.getDefaultValue());
}
inline bool retainedConstructor(const juce::RangedAudioParameter& p, float saved, float actual) {
    const float initial = constructorRaw(p);
    return std::isfinite(saved) && std::isfinite(actual)
        && bits(actual, initial)
        && juce::approximatelyEqual(saved, initial)
        && bits(p.convertTo0to1(saved), p.convertTo0to1(actual));
}
inline bool restoredRepresentation(const juce::RangedAudioParameter& p, float saved, float actual) {
    if (!std::isfinite(saved) || !std::isfinite(actual)) return false;
    if (!bits(p.convertTo0to1(saved), p.convertTo0to1(actual))) return false;
    return bits(saved, actual)
        || bits(p.convertFrom0to1(p.convertTo0to1(saved)), actual)
        || retainedConstructor(p, saved, actual);
}
}
