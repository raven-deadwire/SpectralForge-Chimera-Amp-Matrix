#pragma once

#include "MultibandDynamicsCore.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace spectralforge {

inline constexpr int multibandParameterCount = 46;
inline constexpr int multibandParameterVersion = 13;

inline juce::String multibandID(int band, const char* field) {
    return "mbBand" + juce::String(band + 1) + "_" + field;
}

inline juce::StringArray multibandModeNames() {
    return {"Downward compression", "Upward compression", "Downward expansion", "Upward expansion"};
}

// Append only. The default global off switch leaves old sessions unchanged.
// Do not insert these between any already released host parameters.
inline void appendMultibandParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    const auto number = [&](const juce::String& id, const juce::String& name,
                            float lo, float hi, float initial, float centre = 0.f) {
        juce::NormalisableRange<float> range{lo, hi, .001f};
        if (centre > lo && centre < hi) range.setSkewForCentre(centre);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id, multibandParameterVersion}, name, range, initial));
    };
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"mbOn", multibandParameterVersion}, "Multiband dynamics enabled", false));
    number("mbMix", "Multiband parallel mix", 0.f, 1.f, 1.f);
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mbDetector", multibandParameterVersion}, "Multiband linked detector",
        juce::StringArray{"Peak", "RMS"}, 1));
    constexpr std::array<float, 3> defaults{120.f, 700.f, 3500.f};
    for (int i = 0; i < 3; ++i)
        number("mbCrossover" + juce::String(i + 1), "Multiband crossover " + juce::String(i + 1),
               40.f, 16000.f, defaults[size_t(i)], 1000.f);
    for (int i = 0; i < multibandBandCount; ++i) {
        const auto name = "Multiband band " + juce::String(i + 1) + " ";
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{multibandID(i, "bypass"), multibandParameterVersion}, name + "bypass", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{multibandID(i, "mode"), multibandParameterVersion}, name + "mode", multibandModeNames(), 0));
        number(multibandID(i, "threshold"), name + "threshold", -72.f, 0.f, -24.f);
        number(multibandID(i, "range"), name + "maximum gain change", 0.f, 36.f, 6.f);
        number(multibandID(i, "ratio"), name + "ratio", 1.f, 20.f, 2.f, 4.f);
        number(multibandID(i, "attack"), name + "attack", .1f, 200.f, 10.f, 10.f);
        number(multibandID(i, "release"), name + "release", 5.f, 2000.f, 120.f, 120.f);
        number(multibandID(i, "knee"), name + "knee", 0.f, 24.f, 6.f);
        number(multibandID(i, "makeup"), name + "makeup", -24.f, 24.f, 0.f);
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{multibandID(i, "solo"), multibandParameterVersion}, name + "solo", false));
    }
}

// Stereo link and M/S (docs/MULTIBAND_STEREO_1_5.md): appended after every
// released parameter at their own version; the defaults (L/R, 100 % link,
// both channels) reproduce the original linked processing exactly.
inline constexpr int multibandStereoParameterVersion = 18;
inline constexpr int multibandStereoParameterCount = 2 + multibandBandCount;
inline juce::StringArray multibandChannelNames() { return {"Both", "Mid / Left", "Side / Right"}; }
inline void appendMultibandStereoParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mbStereoMode", multibandStereoParameterVersion}, "Multiband stereo mode",
        juce::StringArray{"Left / Right", "Mid / Side"}, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mbLink", multibandStereoParameterVersion}, "Multiband stereo link",
        juce::NormalisableRange<float>{0.f, 100.f, .1f}, 100.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v)) + " %"; })));
    for (int i = 0; i < multibandBandCount; ++i)
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{multibandID(i, "channels"), multibandStereoParameterVersion},
            "Multiband band " + juce::String(i + 1) + " channels", multibandChannelNames(), 0));
}

// Six-band expansion (docs/SIX_BAND_1_5.md): appended after every other bank
// at its own version. The default (4 bands) leaves sessions unchanged; six
// bands add crossovers 4 and 5 above crossover 3, and bands 5 and 6.
inline constexpr int multibandSixBandParameterVersion = 31;
inline constexpr int multibandSixBandParameterCount = 3 + 2 * 11;
inline void appendMultibandSixBandParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    const auto number = [&](const juce::String& id, const juce::String& name,
                            float lo, float hi, float initial, float centre = 0.f) {
        juce::NormalisableRange<float> range{lo, hi, .001f};
        if (centre > lo && centre < hi) range.setSkewForCentre(centre);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id, multibandSixBandParameterVersion}, name, range, initial));
    };
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mbBandCount", multibandSixBandParameterVersion}, "Multiband band count",
        juce::StringArray{"4 bands", "6 bands"}, 0));
    number("mbCrossover4", "Multiband crossover 4", 40.f, 16000.f, 6000.f, 1000.f);
    number("mbCrossover5", "Multiband crossover 5", 40.f, 16000.f, 11000.f, 1000.f);
    for (int i = multibandBandCount; i < multibandMaxBandCount; ++i) {
        const auto name = "Multiband band " + juce::String(i + 1) + " ";
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{multibandID(i, "bypass"), multibandSixBandParameterVersion}, name + "bypass", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{multibandID(i, "mode"), multibandSixBandParameterVersion}, name + "mode", multibandModeNames(), 0));
        number(multibandID(i, "threshold"), name + "threshold", -72.f, 0.f, -24.f);
        number(multibandID(i, "range"), name + "maximum gain change", 0.f, 36.f, 6.f);
        number(multibandID(i, "ratio"), name + "ratio", 1.f, 20.f, 2.f, 4.f);
        number(multibandID(i, "attack"), name + "attack", .1f, 200.f, 10.f, 10.f);
        number(multibandID(i, "release"), name + "release", 5.f, 2000.f, 120.f, 120.f);
        number(multibandID(i, "knee"), name + "knee", 0.f, 24.f, 6.f);
        number(multibandID(i, "makeup"), name + "makeup", -24.f, 24.f, 0.f);
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{multibandID(i, "solo"), multibandSixBandParameterVersion}, name + "solo", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{multibandID(i, "channels"), multibandSixBandParameterVersion},
            name + "channels", multibandChannelNames(), 0));
    }
}

class MultibandDynamics {
public:
    static constexpr int latencySamples() noexcept { return MultibandDynamicsCore::latencySamples(); }

    void bind(juce::AudioProcessorValueTreeState& state) {
        for (size_t i = 0; i < globalIDs.size(); ++i)
            globals[i] = state.getRawParameterValue(globalIDs[i]);
        for (int band = 0; band < multibandMaxBandCount; ++band)
            for (size_t field = 0; field < bandFields.size(); ++field)
                parameters[size_t(band)][field] = state.getRawParameterValue(multibandID(band, bandFields[field]));
        stereoMode = state.getRawParameterValue("mbStereoMode");
        link = state.getRawParameterValue("mbLink");
        for (int band = 0; band < multibandMaxBandCount; ++band)
            bandChannels[size_t(band)] = state.getRawParameterValue(multibandID(band, "channels"));
        bandCountParameter = state.getRawParameterValue("mbBandCount");
        upper[0] = state.getRawParameterValue("mbCrossover4");
        upper[1] = state.getRawParameterValue("mbCrossover5");
    }

    void prepare(double rate, int /*maximumBlockSize*/ = 0) noexcept {
        uiRate.store(multiband_detail::validRate(rate), std::memory_order_relaxed);
        core.prepare(rate);
        core.setSettings(settings());
        core.reset();
        publish();
    }
    void reset() noexcept { core.setSettings(settings()); core.reset(); publish(); }

    // `key`: optional external detector input, the same length as `audio`.
    void process(juce::AudioBuffer<float>& audio, const juce::AudioBuffer<float>* key = nullptr) noexcept {
        const juce::ScopedNoDenormals noDenormals;
        core.setSettings(settings());
        const bool keyed = key != nullptr && key->getNumSamples() == audio.getNumSamples();
        core.process(audio.getArrayOfWritePointers(), audio.getNumChannels(), audio.getNumSamples(),
                     keyed ? key->getArrayOfReadPointers() : nullptr, keyed ? key->getNumChannels() : 0);
        publish();
    }

    bool enabled() const noexcept { return read(globals[0], 0.f, 1.f, 0.f) > .5f; }
    bool isBypassed() const noexcept { return !enabled(); }
    float reduction(int band) const noexcept {
        return meters[size_t(std::clamp(band, 0, multibandMaxBandCount - 1))].load(std::memory_order_relaxed);
    }
    // 4 or 6 (the setting; the audio switches after a short fade).
    int bandCount() const noexcept { return read(bandCountParameter, 0.f, 1.f, 0.f) > .5f ? 6 : 4; }
    // UI markers use the validated target frequencies, including while bypassed.
    // Applied frequencies expose the smoothed audio state separately. With four
    // bands the last two entries are crossovers 4 and 5 as stored (unused).
    std::array<float, multibandMaxCrossovers> allCrossovers() const noexcept {
        const std::array<float, multibandMaxCrossovers> raw{
            read(globals[3], 40.f, 16000.f, 120.f), read(globals[4], 40.f, 16000.f, 700.f),
            read(globals[5], 40.f, 16000.f, 3500.f), read(upper[0], 40.f, 16000.f, 6000.f),
            read(upper[1], 40.f, 16000.f, 11000.f)};
        const double rate = uiRate.load(std::memory_order_relaxed);
        if (bandCount() == 6) return MultibandDynamicsCore::validatedSixBandCrossovers(raw, rate);
        const auto four = MultibandDynamicsCore::validatedCrossovers({raw[0], raw[1], raw[2]}, rate);
        return {four[0], four[1], four[2], raw[3], raw[4]};
    }
    std::array<float, 3> crossovers() const noexcept {
        const auto all = allCrossovers();
        return {all[0], all[1], all[2]};
    }
    std::array<float, 3> appliedCrossovers() const noexcept {
        return {applied[0].load(std::memory_order_relaxed), applied[1].load(std::memory_order_relaxed),
                applied[2].load(std::memory_order_relaxed)};
    }
    std::array<float, multibandMaxCrossovers> appliedAllCrossovers() const noexcept {
        std::array<float, multibandMaxCrossovers> out{};
        for (size_t i = 0; i < out.size(); ++i) out[i] = applied[i].load(std::memory_order_relaxed);
        return out;
    }
    int activeBandCount() const noexcept { return activeBands.load(std::memory_order_relaxed); }
    double sampleRate() const noexcept { return uiRate.load(std::memory_order_relaxed); }

    MultibandBandSettings values(int band) const noexcept {
        const auto& p = parameters[size_t(std::clamp(band, 0, multibandMaxBandCount - 1))];
        const int b = std::clamp(band, 0, multibandMaxBandCount - 1);
        return {read(p[0], 0.f, 1.f, 0.f) > .5f, int(read(p[1], 0.f, 3.f, 0.f)),
                read(p[2], -72.f, 0.f, -24.f), read(p[3], 0.f, 36.f, 6.f),
                read(p[4], 1.f, 20.f, 2.f), read(p[5], .1f, 200.f, 10.f),
                read(p[6], 5.f, 2000.f, 120.f), read(p[7], 0.f, 24.f, 6.f),
                read(p[8], -24.f, 24.f, 0.f), read(p[9], 0.f, 1.f, 0.f) > .5f,
                juce::roundToInt(read(bandChannels[size_t(b)], 0.f, 2.f, 0.f))};
    }

private:
    static float read(const std::atomic<float>* p, float lo, float hi, float fallback) noexcept {
        return float(multiband_detail::finiteClamp(
            p ? p->load(std::memory_order_relaxed) : fallback, lo, hi, fallback));
    }
    MultibandSettings settings() const noexcept {
        MultibandSettings s;
        s.on = enabled();
        s.mix = read(globals[1], 0.f, 1.f, 1.f);
        s.detector = int(read(globals[2], 0.f, 1.f, 1.f));
        s.stereo = juce::roundToInt(read(stereoMode, 0.f, 1.f, 0.f));
        s.link = read(link, 0.f, 100.f, 100.f) / 100.f;
        s.bandCount = bandCount();
        const auto all = allCrossovers();
        s.crossovers = {all[0], all[1], all[2]};
        s.upperCrossovers = {all[3], all[4]};
        for (int i = 0; i < multibandMaxBandCount; ++i) s.bands[size_t(i)] = values(i);
        return s;
    }
    void publish() noexcept {
        for (int i = 0; i < multibandMaxBandCount; ++i)
            meters[size_t(i)].store(core.reduction(i), std::memory_order_relaxed);
        const auto frequencies = core.crossovers();
        const auto upperFrequencies = core.upperCrossovers();
        for (size_t i = 0; i < 3; ++i) applied[i].store(frequencies[i], std::memory_order_relaxed);
        for (size_t i = 0; i < 2; ++i) applied[i + 3].store(upperFrequencies[i], std::memory_order_relaxed);
        activeBands.store(core.activeBandCount(), std::memory_order_relaxed);
    }

    inline static constexpr std::array<const char*, 6> globalIDs{
        "mbOn", "mbMix", "mbDetector", "mbCrossover1", "mbCrossover2", "mbCrossover3"};
    inline static constexpr std::array<const char*, 10> bandFields{
        "bypass", "mode", "threshold", "range", "ratio", "attack", "release", "knee", "makeup", "solo"};
    std::array<std::atomic<float>*, 6> globals{};
    std::array<std::array<std::atomic<float>*, 10>, multibandMaxBandCount> parameters{};
    std::atomic<float>* stereoMode{};
    std::atomic<float>* link{};
    std::atomic<float>* bandCountParameter{};
    std::array<std::atomic<float>*, 2> upper{};
    std::array<std::atomic<float>*, multibandMaxBandCount> bandChannels{};
    std::array<std::atomic<float>, multibandMaxBandCount> meters{};
    std::array<std::atomic<float>, multibandMaxCrossovers> applied{};
    std::atomic<int> activeBands{4};
    std::atomic<double> uiRate{48000.};
    MultibandDynamicsCore core;
};

} // namespace spectralforge
