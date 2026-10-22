#pragma once

#include "MultibandDynamicsCore.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace spectralforge {

inline constexpr int deEsserParameterCount = 9;
inline constexpr int deEsserParameterVersion = 13;

inline void appendDeEsserParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    const auto number = [&](const char* id, const char* name, float lo, float hi, float initial,
                            float centre = 0.f) {
        juce::NormalisableRange<float> range{lo, hi, .001f};
        if (centre > lo && centre < hi) range.setSkewForCentre(centre);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id, deEsserParameterVersion}, name, range, initial));
    };
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"deEsserOn", deEsserParameterVersion}, "De-esser enabled", false));
    number("deEsserFrequency", "De-esser split frequency", 1500.f, 16000.f, 6000.f, 6000.f);
    number("deEsserThreshold", "De-esser threshold", -72.f, 0.f, -24.f);
    number("deEsserRange", "De-esser maximum high-band cut", 0.f, 24.f, 6.f);
    number("deEsserAttack", "De-esser attack", .1f, 100.f, 2.f, 5.f);
    number("deEsserRelease", "De-esser release", 5.f, 1000.f, 80.f, 100.f);
    number("deEsserRatio", "De-esser ratio", 1.f, 20.f, 4.f, 4.f);
    number("deEsserKnee", "De-esser knee", 0.f, 24.f, 6.f);
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"deEsserListen", deEsserParameterVersion}, "De-esser high-band listen", false));
}

class DeEsser {
public:
    static constexpr int latencySamples() noexcept { return DeEsserCore::latencySamples(); }
    void bind(juce::AudioProcessorValueTreeState& state) {
        for (size_t i = 0; i < ids.size(); ++i) parameters[i] = state.getRawParameterValue(ids[i]);
    }
    void prepare(double rate, int /*maximumBlockSize*/ = 0) noexcept {
        uiRate.store(multiband_detail::validRate(rate), std::memory_order_relaxed);
        core.prepare(rate);
        core.setSettings(values());
        core.reset();
        publish();
    }
    void reset() noexcept { core.setSettings(values()); core.reset(); publish(); }
    void process(juce::AudioBuffer<float>& audio) noexcept {
        const juce::ScopedNoDenormals noDenormals;
        core.setSettings(values());
        core.process(audio.getArrayOfWritePointers(), audio.getNumChannels(), audio.getNumSamples());
        publish();
    }

    bool enabled() const noexcept { return read(0, 0.f, 1.f, 0.f) > .5f; }
    bool isBypassed() const noexcept { return !enabled(); }
    float reduction() const noexcept { return meter.load(std::memory_order_relaxed); }
    float frequency() const noexcept {
        return std::min(read(1, 1500.f, 16000.f, 6000.f), float(sampleRate() * .4));
    }
    float appliedFrequency() const noexcept { return applied.load(std::memory_order_relaxed); }
    double sampleRate() const noexcept { return uiRate.load(std::memory_order_relaxed); }
    DeEsserSettings values() const noexcept {
        return {enabled(), frequency(), read(2, -72.f, 0.f, -24.f), read(3, 0.f, 24.f, 6.f),
                read(4, .1f, 100.f, 2.f), read(5, 5.f, 1000.f, 80.f),
                read(6, 1.f, 20.f, 4.f), read(7, 0.f, 24.f, 6.f), read(8, 0.f, 1.f, 0.f) > .5f};
    }
private:
    float read(size_t index, float lo, float hi, float fallback) const noexcept {
        const auto* p = parameters[index];
        return float(multiband_detail::finiteClamp(
            p ? p->load(std::memory_order_relaxed) : fallback, lo, hi, fallback));
    }
    void publish() noexcept {
        meter.store(core.reduction(), std::memory_order_relaxed);
        applied.store(core.frequency(), std::memory_order_relaxed);
    }
    inline static constexpr std::array<const char*, deEsserParameterCount> ids{
        "deEsserOn", "deEsserFrequency", "deEsserThreshold", "deEsserRange", "deEsserAttack",
        "deEsserRelease", "deEsserRatio", "deEsserKnee", "deEsserListen"};
    std::array<std::atomic<float>*, deEsserParameterCount> parameters{};
    std::atomic<float> meter{0.f}, applied{6000.f};
    std::atomic<double> uiRate{48000.};
    DeEsserCore core;
};

} // namespace spectralforge
