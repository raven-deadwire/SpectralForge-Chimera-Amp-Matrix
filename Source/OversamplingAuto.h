#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// OVERSAMPLING Auto (docs/OVERSAMPLING_AUTO.md): an appended switch, on for
// new instances, off for sessions saved before it existed. With it on, the
// amps oversample to about the same internal rate at every host rate: 4x at
// 44.1/48 kHz (the released default), 2x at 88.2/96 kHz, 1x from 176.4 kHz.
// The released "oversampling" parameter keeps its values and contract; a
// change to it (editor, host or automation, not a restore) turns Auto off.
namespace spectralforge {
inline constexpr int oversamplingAutoParameterVersion = 33;
inline constexpr int oversamplingAutoParameterCount = 1;
inline void appendOversamplingAutoParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"oversamplingAuto", oversamplingAutoParameterVersion}, "Oversampling follows sample rate", true));
}
// Choice index of the "oversampling" parameter: 0 1x, 1 2x, 2 4x, 3 8x.
inline int autoOversamplingChoice(double sampleRate) noexcept {
    return sampleRate >= 160000. ? 0 : sampleRate >= 80000. ? 1 : 2;
}
inline int effectiveOversamplingChoice(int choice, bool automatic, double sampleRate) noexcept {
    return automatic ? autoOversamplingChoice(sampleRate) : juce::jlimit(0, 3, choice);
}
} // namespace spectralforge
