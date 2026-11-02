#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace spectralforge {
// A processor the POST chain runs between two of its stages (the 1.9 CREATIVE
// slot, docs/CREATIVE_1_9.md). Kept free of the processor and GUI modules so
// DSP-only targets can include the chain.
struct PostInsert {
    virtual ~PostInsert() = default;
    virtual void process(juce::AudioBuffer<float>& buffer, double tempo) noexcept = 0;
};
}
