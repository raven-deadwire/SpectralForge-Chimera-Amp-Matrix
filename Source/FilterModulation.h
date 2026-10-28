#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>

namespace spectralforge {

// Two-filter modulation processor (docs/FILTER_MODULATION_1_7.md), a common
// processor after the three-band saturation. Filters A and B are
// zero-delay-feedback state-variable filters (LP / HP / BP / Notch, 12 or
// 24 dB/oct), in series or in parallel. Each cutoff follows its own amount of
// a shared LFO (free or tempo-synced) and envelope follower, in octaves.
// Off leaves the input sample-exact; switching crossfades over 10 ms.
inline constexpr int filterModulationParameterVersion = 21;
inline constexpr int filterCount = 2;
inline constexpr int filterGlobalParameterCount = 11;
inline constexpr int filterFieldCount = 7;
inline constexpr int filterModulationParameterCount = filterGlobalParameterCount + filterCount * filterFieldCount;
inline juce::StringArray filterTypeNames() { return {"Low-pass", "High-pass", "Band-pass", "Notch"}; }
inline juce::StringArray filterSlopeNames() { return {"12 dB/oct", "24 dB/oct"}; }
inline juce::StringArray filterRoutingNames() { return {"Serial A > B", "Parallel A + B"}; }
inline juce::StringArray filterLfoShapeNames() { return {"Sine", "Triangle", "Square", "Saw down"}; }
inline juce::StringArray filterLfoSyncNames() { return {"Free", "1/1", "1/2", "1/4", "1/8", "1/16", "1/4 T", "1/8 T", "1/4 D", "1/8 D"}; }
// Quarter-note beats per LFO cycle for each synced choice (index 0 is free).
inline constexpr std::array<double, 10> filterSyncBeats{0., 4., 2., 1., .5, .25, 2. / 3., 1. / 3., 1.5, .75};
inline juce::String filterID(int filter, const char* field) { return juce::String(filter == 0 ? "fltA_" : "fltB_") + field; }

inline void appendFilterModulationParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    constexpr int v = filterModulationParameterVersion;
    const auto number = [&](const juce::String& id, const juce::String& name, float lo, float hi, float initial, float centre, const juce::String& unit, int decimals) {
        juce::NormalisableRange<float> range{lo, hi, .001f};
        if (centre > lo && centre < hi) range.setSkewForCentre(centre);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, v}, name, range, initial,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([unit, decimals](float value, int) { return juce::String(value, decimals) + unit; })));
    };
    const auto choice = [&](const juce::String& id, const juce::String& name, const juce::StringArray& options, int initial) {
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, v}, name, options, initial));
    };
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"fltOn", v}, "Filter", false));
    number("fltMix", "Filter mix", 0.f, 100.f, 100.f, 0.f, " %", 0);
    number("fltOutput", "Filter output", -24.f, 12.f, 0.f, 0.f, " dB", 1);
    choice("fltRouting", "Filter routing", filterRoutingNames(), 0);
    choice("fltLfoShape", "Filter LFO shape", filterLfoShapeNames(), 0);
    number("fltLfoRate", "Filter LFO rate", .05f, 20.f, 1.f, 1.f, " Hz", 2);
    choice("fltLfoSync", "Filter LFO sync", filterLfoSyncNames(), 0);
    number("fltLfoStereo", "Filter LFO stereo phase", 0.f, 180.f, 0.f, 0.f, " deg", 0);
    number("fltEnvAttack", "Filter envelope attack", .1f, 200.f, 10.f, 10.f, " ms", 1);
    number("fltEnvRelease", "Filter envelope release", 5.f, 2000.f, 150.f, 150.f, " ms", 0);
    number("fltEnvSensitivity", "Filter envelope sensitivity", -24.f, 24.f, 0.f, 0.f, " dB", 1);
    constexpr std::array<int, filterCount> types{0, 1};
    constexpr std::array<float, filterCount> cutoffs{1000.f, 80.f};
    for (int f = 0; f < filterCount; ++f) {
        const auto name = juce::String("Filter ") + (f == 0 ? "A " : "B ");
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{filterID(f, "on"), v}, name + "on", f == 0));
        choice(filterID(f, "type"), name + "type", filterTypeNames(), types[size_t(f)]);
        choice(filterID(f, "slope"), name + "slope", filterSlopeNames(), 0);
        number(filterID(f, "cutoff"), name + "cutoff", 20.f, 20000.f, cutoffs[size_t(f)], 1000.f, " Hz", 0);
        number(filterID(f, "resonance"), name + "resonance", 0.f, 100.f, 20.f, 0.f, " %", 0);
        number(filterID(f, "lfo"), name + "LFO amount", -4.f, 4.f, 0.f, 0.f, " oct", 2);
        number(filterID(f, "env"), name + "envelope amount", -4.f, 4.f, 0.f, 0.f, " oct", 2);
    }
}

struct FilterSettings {
    bool on{false};
    float mix{1.f}, outputDb{0.f};
    int routing{0}, lfoShape{0}, lfoSync{0};
    float lfoRate{1.f}, lfoStereoDeg{0.f}, envAttackMs{10.f}, envReleaseMs{150.f}, envSensitivityDb{0.f};
    struct Unit { bool on{}; int type{}, slope{}; float cutoff{1000.f}, resonance{.2f}, lfoOct{}, envOct{}; };
    std::array<Unit, filterCount> filters{{{true, 0, 0, 1000.f, .2f, 0.f, 0.f}, {false, 1, 0, 80.f, .2f, 0.f, 0.f}}};
};

namespace filter_detail {
// Zero-delay-feedback state-variable filter (trapezoidal integration). Its
// state stays well-behaved under per-sample coefficient changes.
struct SVF {
    double ic1{}, ic2{};
    // Returns {low, band (unity-peak), high}.
    std::array<double, 3> tick(double x, double g, double k) noexcept {
        const double a1 = 1. / (1. + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2. * v1 - ic1; ic2 = 2. * v2 - ic2;
        return {v2, k * v1, x - k * v1 - v2};
    }
};
// Resonance 0..1 maps to Q 0.707..20 on a log scale.
inline double resonanceQ(double r) noexcept { return .70710678 * std::pow(20. / .70710678, juce::jlimit(0., 1., r)); }
struct Filter {
    std::array<SVF, 2> stages;
    double process(double x, int type, int slope, double hz, double q, double rate) noexcept {
        const double g = std::tan(juce::MathConstants<double>::pi * juce::jlimit(10., rate * .45, hz) / rate);
        const auto one = [&](SVF& svf, double input, double stageQ) {
            const auto out = svf.tick(input, g, 1. / stageQ);
            switch (type) {
                case 1: return out[2];
                case 2: return out[1];
                case 3: return out[0] + out[2];
                default: return out[0];
            }
        };
        if (slope == 0) return one(stages[0], x, q);
        // 24 dB/oct: a fourth-order Butterworth pair (Q 0.541, 1.307) at zero
        // resonance; resonance raises the second stage's Q.
        return one(stages[1], one(stages[0], x, .5411961), q * 1.3065630 / .70710678);
    }
    void reset() noexcept { stages = {}; }
};
inline double lfoValue(int shape, double phase) noexcept {   // phase 0..1, value -1..1
    switch (shape) {
        case 1: return phase < .5 ? 4. * phase - 1. : 3. - 4. * phase;
        case 2: return phase < .5 ? 1. : -1.;
        case 3: return 1. - 2. * phase;
        default: return std::sin(juce::MathConstants<double>::twoPi * phase);
    }
}
} // namespace filter_detail

// Sample-level core, separate from the parameter binding so tests can drive it.
class FilterModulationCore {
public:
    void prepare(double sampleRate) {
        rate = sampleRate;
        fade.reset(rate, .01); mix.reset(rate, .02); output.reset(rate, .02);
        for (auto& c : logCutoff) c.reset(rate, .02);
        for (auto& r : resonance) r.reset(rate, .02);
        squareSmoothing = 1. - std::exp(-1. / (rate * .002));
        reset();
    }
    void setInitialState(const FilterSettings& s) {
        fade.setCurrentAndTargetValue(s.on ? 1.f : 0.f);
        mix.setCurrentAndTargetValue(s.mix);
        output.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(s.on ? s.outputDb : 0.f));
        for (int f = 0; f < filterCount; ++f) {
            logCutoff[size_t(f)].setCurrentAndTargetValue(std::log2(s.filters[size_t(f)].cutoff));
            resonance[size_t(f)].setCurrentAndTargetValue(s.filters[size_t(f)].resonance);
        }
    }
    void reset() noexcept {
        for (auto& channel : filters) for (auto& f : channel) f.reset();
        envelope = {}; squareState = {}; phase = 0.;
    }
    bool idle() const noexcept { return fade.getCurrentValue() <= 0.f && !fade.isSmoothing(); }
    double lfoPhase() const noexcept { return phase; }

    void process(float* const* data, int channels, int samples, const FilterSettings& s, double bpm) noexcept {
        fade.setTargetValue(s.on ? 1.f : 0.f);
        mix.setTargetValue(s.mix);
        output.setTargetValue(juce::Decibels::decibelsToGain(s.on ? s.outputDb : 0.f));
        for (int f = 0; f < filterCount; ++f) {
            logCutoff[size_t(f)].setTargetValue(std::log2(s.filters[size_t(f)].cutoff));
            resonance[size_t(f)].setTargetValue(s.filters[size_t(f)].resonance);
        }
        const double hz = s.lfoSync > 0 && bpm > 0. ? bpm / 60. / filterSyncBeats[size_t(s.lfoSync)] : double(s.lfoRate);
        const double step = juce::jlimit(0., .5, hz / rate);
        const double attack = 1. - std::exp(-1. / (rate * s.envAttackMs * .001));
        const double release = 1. - std::exp(-1. / (rate * s.envReleaseMs * .001));
        const double sensitivity = juce::Decibels::decibelsToGain(double(s.envSensitivityDb));
        const double offset = s.lfoStereoDeg / 360.;
        const bool anyOn = s.filters[0].on || s.filters[1].on;
        for (int n = 0; n < samples; ++n) {
            const float g = fade.getNextValue(), wetMix = mix.getNextValue(), gain = output.getNextValue();
            std::array<double, filterCount> octave{}, q{};
            for (int f = 0; f < filterCount; ++f) {
                octave[size_t(f)] = logCutoff[size_t(f)].getNextValue();
                q[size_t(f)] = filter_detail::resonanceQ(resonance[size_t(f)].getNextValue() * .01);
            }
            for (int c = 0; c < channels; ++c) {
                const double x = data[c][n];
                // Each channel follows its own input envelope.
                const double level = std::abs(x) * sensitivity;
                auto& e = envelope[size_t(c)];
                e += (level > e ? attack : release) * (level - e);
                double p = phase + (c == 1 ? offset : 0.);
                p -= std::floor(p);
                double lfo = filter_detail::lfoValue(s.lfoShape, p);
                // The square steps through a 2 ms one-pole so its edges do not click.
                squareState[size_t(c)] += squareSmoothing * (lfo - squareState[size_t(c)]);
                if (s.lfoShape == 2) lfo = squareState[size_t(c)];
                const double env = juce::jmin(1., e);
                std::array<double, filterCount> out{};
                double y = x;
                for (int f = 0; f < filterCount; ++f) {
                    const auto& unit = s.filters[size_t(f)];
                    const double cutoff = std::exp2(octave[size_t(f)] + unit.lfoOct * lfo + unit.envOct * env);
                    const double input = s.routing == 0 ? y : x;
                    // A disabled filter keeps running on its input, so turning it on
                    // starts from a settled state; its output is simply not used.
                    out[size_t(f)] = filters[size_t(c)][size_t(f)].process(input, unit.type, unit.slope, cutoff, q[size_t(f)], rate);
                    if (s.routing == 0 && unit.on) y = out[size_t(f)];
                }
                if (s.routing == 1) {
                    if (s.filters[0].on && s.filters[1].on) y = .5 * (out[0] + out[1]);
                    else if (s.filters[0].on) y = out[0];
                    else if (s.filters[1].on) y = out[1];
                }
                if (!anyOn) y = x;
                const double processed = (x + wetMix * (y - x)) * gain;
                data[c][n] = float(x + g * (processed - x));
            }
            phase += step;
            if (phase >= 1.) phase -= 1.;
        }
    }

private:
    double rate{48000.}, phase{}, squareSmoothing{};
    std::array<std::array<filter_detail::Filter, filterCount>, 2> filters{};
    std::array<double, 2> envelope{}, squareState{};
    juce::SmoothedValue<float> fade, mix, output;
    std::array<juce::SmoothedValue<float>, filterCount> logCutoff, resonance;
};

class FilterModulation {
public:
    void bind(juce::AudioProcessorValueTreeState& state) {
        for (size_t i = 0; i < globalIDs.size(); ++i) globals[i] = state.getRawParameterValue(globalIDs[i]);
        for (int f = 0; f < filterCount; ++f)
            for (size_t i = 0; i < fieldIDs.size(); ++i) fields[size_t(f)][i] = state.getRawParameterValue(filterID(f, fieldIDs[i]));
    }
    void prepare(double sampleRate) { core.prepare(sampleRate); core.setInitialState(settings()); }
    void reset() noexcept { core.reset(); }
    void process(juce::AudioBuffer<float>& buffer, double bpm) noexcept {
        const auto s = settings();
        const int channels = juce::jmin(2, buffer.getNumChannels()), samples = buffer.getNumSamples();
        if (channels == 0 || samples == 0) return;
        if (core.idle()) {
            if (!s.on) return;   // Off: the input stays untouched.
            core.reset();        // Turning on starts from cleared filter state.
        }
        core.process(buffer.getArrayOfWritePointers(), channels, samples, s, bpm);
    }
    FilterSettings settings() const noexcept {
        const auto read = [](const std::atomic<float>* p, float lo, float hi, float fallback) {
            const float value = p ? p->load(std::memory_order_relaxed) : fallback;
            return std::isfinite(value) ? juce::jlimit(lo, hi, value) : fallback;
        };
        FilterSettings s;
        s.on = read(globals[0], 0.f, 1.f, 0.f) > .5f;
        s.mix = read(globals[1], 0.f, 100.f, 100.f) / 100.f;
        s.outputDb = read(globals[2], -24.f, 12.f, 0.f);
        s.routing = juce::roundToInt(read(globals[3], 0.f, 1.f, 0.f));
        s.lfoShape = juce::roundToInt(read(globals[4], 0.f, 3.f, 0.f));
        s.lfoRate = read(globals[5], .05f, 20.f, 1.f);
        s.lfoSync = juce::roundToInt(read(globals[6], 0.f, 9.f, 0.f));
        s.lfoStereoDeg = read(globals[7], 0.f, 180.f, 0.f);
        s.envAttackMs = read(globals[8], .1f, 200.f, 10.f);
        s.envReleaseMs = read(globals[9], 5.f, 2000.f, 150.f);
        s.envSensitivityDb = read(globals[10], -24.f, 24.f, 0.f);
        const FilterSettings defaults;
        for (int f = 0; f < filterCount; ++f) {
            auto& u = s.filters[size_t(f)];const auto& d = defaults.filters[size_t(f)];const auto& p = fields[size_t(f)];
            u.on = read(p[0], 0.f, 1.f, d.on ? 1.f : 0.f) > .5f;
            u.type = juce::roundToInt(read(p[1], 0.f, 3.f, float(d.type)));
            u.slope = juce::roundToInt(read(p[2], 0.f, 1.f, 0.f));
            u.cutoff = read(p[3], 20.f, 20000.f, d.cutoff);
            u.resonance = read(p[4], 0.f, 100.f, 20.f);
            u.lfoOct = read(p[5], -4.f, 4.f, 0.f);
            u.envOct = read(p[6], -4.f, 4.f, 0.f);
        }
        return s;
    }

private:
    inline static constexpr std::array<const char*, filterGlobalParameterCount> globalIDs{
        "fltOn", "fltMix", "fltOutput", "fltRouting", "fltLfoShape", "fltLfoRate", "fltLfoSync", "fltLfoStereo",
        "fltEnvAttack", "fltEnvRelease", "fltEnvSensitivity"};
    inline static constexpr std::array<const char*, filterFieldCount> fieldIDs{"on", "type", "slope", "cutoff", "resonance", "lfo", "env"};
    std::array<std::atomic<float>*, filterGlobalParameterCount> globals{};
    std::array<std::array<std::atomic<float>*, filterFieldCount>, filterCount> fields{};
    FilterModulationCore core;
};

} // namespace spectralforge
