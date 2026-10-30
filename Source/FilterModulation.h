#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include <limits>

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
// 1.9 adds Comb (feedback comb tuned to the cutoff; resonance is its
// feedback) and All-pass (flat magnitude, phase turn at the cutoff: a phaser
// notch when mixed with the dry signal).
inline juce::StringArray filterTypeNames() { return {"Low-pass", "High-pass", "Band-pass", "Notch", "Comb", "All-pass"}; }
inline constexpr int filterTypeCount = 6;
inline juce::StringArray filterSlopeNames() { return {"12 dB/oct", "24 dB/oct"}; }
inline juce::StringArray filterRoutingNames() { return {"Serial A > B", "Parallel A + B"}; }
inline juce::StringArray filterLfoShapeNames() { return {"Sine", "Triangle", "Square", "Saw down", "Random"}; }   // Random: 1.9, a new value each cycle
inline constexpr int filterLfoShapeCount = 5;
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

// 1.9 expanded modulation (docs/MODULATION_1_9.md), appended at version hint
// 30: a second LFO, LFO phase locked to the host bar position, retrigger on
// attack, a sidechain envelope source, and per-filter drive and LFO 2 amount.
inline constexpr int filterModulationExtensionVersion = 30;
inline juce::StringArray filterEnvelopeSourceNames() { return {"Own signal", "Sidechain"}; }
inline void appendFilterModulationExtensionParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    constexpr int v = filterModulationExtensionVersion;
    const auto number = [&](const juce::String& id, const juce::String& name, float lo, float hi, float initial, float centre, const juce::String& unit, int decimals) {
        juce::NormalisableRange<float> range{lo, hi, .001f};
        if (centre > lo && centre < hi) range.setSkewForCentre(centre);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, v}, name, range, initial,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([unit, decimals](float value, int) { return juce::String(value, decimals) + unit; })));
    };
    const auto choice = [&](const juce::String& id, const juce::String& name, const juce::StringArray& options, int initial) {
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, v}, name, options, initial));
    };
    choice("fltLfo2Shape", "Filter LFO 2 shape", filterLfoShapeNames(), 1);
    number("fltLfo2Rate", "Filter LFO 2 rate", .05f, 20.f, .25f, 1.f, " Hz", 2);
    choice("fltLfo2Sync", "Filter LFO 2 sync", filterLfoSyncNames(), 0);
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"fltLfoLock", v}, "Filter LFOs locked to the host bar", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"fltLfoRetrigger", v}, "Filter LFOs restart on each attack", false));
    choice("fltEnvSource", "Filter envelope source", filterEnvelopeSourceNames(), 0);
    for (int f = 0; f < filterCount; ++f) {
        const auto name = juce::String("Filter ") + (f == 0 ? "A " : "B ");
        number(filterID(f, "lfo2"), name + "LFO 2 amount", -4.f, 4.f, 0.f, 0.f, " oct", 2);
        number(filterID(f, "drive"), name + "drive", 0.f, 100.f, 0.f, 0.f, " %", 0);
    }
}
inline constexpr int filterModulationExtensionParameterCount = 6 + filterCount * 2;

struct FilterSettings {
    bool on{false};
    float mix{1.f}, outputDb{0.f};
    int routing{0}, lfoShape{0}, lfoSync{0};
    float lfoRate{1.f}, lfoStereoDeg{0.f}, envAttackMs{10.f}, envReleaseMs{150.f}, envSensitivityDb{0.f};
    // 1.9
    int lfo2Shape{1}, lfo2Sync{0}, envSource{0};
    float lfo2Rate{.25f};
    bool lfoLock{false}, lfoRetrigger{false};
    struct Unit { bool on{}; int type{}, slope{}; float cutoff{1000.f}, resonance{.2f}, lfoOct{}, envOct{}, lfo2Oct{}, drive{}; };
    std::array<Unit, filterCount> filters{{{true, 0, 0, 1000.f, .2f, 0.f, 0.f, 0.f, 0.f}, {false, 1, 0, 80.f, .2f, 0.f, 0.f, 0.f, 0.f}}};
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
    std::vector<float> comb;int write{};   // Comb: sized in prepare(), never in the audio callback
    void prepare(double rate) { comb.assign(size_t(rate / 10.) + 4, 0.f); write = 0; }
    // `resonance` (0-1) is the Comb's feedback; `drive` (0-1) a tanh stage
    // ahead of the filter, unity at full scale. d = 10 x drive, so the stage
    // approaches the bypass (drive 0, exact) continuously.
    double process(double x, int type, int slope, double hz, double q, double rate, double resonance = 0., double drive = 0.) noexcept {
        if (drive > 0.) { const double d = 10. * drive;x = std::tanh(d * x) / std::tanh(d); }
        const double g = std::tan(juce::MathConstants<double>::pi * juce::jlimit(10., rate * .45, hz) / rate);
        if (type == 4) {
            // The SVF stages keep following the input, so leaving the Comb
            // does not resume from stale state.
            stages[0].tick(x, g, 1. / q);stages[1].tick(x, g, 1. / q);
            return combTick(x, juce::jlimit(10., rate * .45, hz), resonance, rate);
        }
        // The Comb's delay keeps the recent input (no feedback), so choosing
        // the Comb does not replay old audio.
        if (comb.size() >= 4) { comb[size_t(write)] = float(x);write = (write + 1) % int(comb.size()); }
        const auto one = [&](SVF& svf, double input, double stageQ) {
            const auto out = svf.tick(input, g, 1. / stageQ);
            switch (type) {
                case 1: return out[2];
                case 2: return out[1];
                case 3: return out[0] + out[2];
                case 5: return input - 2. * out[1];   // all-pass: low - band + high
                default: return out[0];
            }
        };
        if (slope == 0) return one(stages[0], x, q);
        // 24 dB/oct: a fourth-order Butterworth pair (Q 0.541, 1.307) at zero
        // resonance; resonance raises the second stage's Q.
        return one(stages[1], one(stages[0], x, .5411961), q * 1.3065630 / .70710678);
    }
    // Feedback comb: peaks at multiples of the cutoff, scaled so the peaks
    // stay at unity. Linear interpolation of the delay.
    double combTick(double x, double hz, double resonance, double rate) noexcept {
        if (comb.size() < 4) return x;
        const double feedback = .97 * juce::jlimit(0., 1., resonance);
        const double delay = juce::jlimit(2., double(comb.size() - 2), rate / hz);
        double read = double(write) - delay;if (read < 0.) read += double(comb.size());
        const int i0 = int(read);const double t = read - i0;const int i1 = (i0 + 1) % int(comb.size());
        const double delayed = (1. - t) * comb[size_t(i0)] + t * comb[size_t(i1)];
        const double y = x + feedback * delayed;
        comb[size_t(write)] = float(y);write = (write + 1) % int(comb.size());
        return y * (1. - feedback);
    }
    void reset() noexcept { stages = {};std::fill(comb.begin(), comb.end(), 0.f);write = 0; }
};
inline double lfoValue(int shape, double phase, double held = 0.) noexcept {   // phase 0..1, value -1..1
    switch (shape) {
        case 4: return held;   // Random: the value drawn at the start of the cycle
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
        for (auto& c : drive) c.reset(rate, .02);
        for (auto& r : resonance) r.reset(rate, .02);
        squareSmoothing = 1. - std::exp(-1. / (rate * .002));
        const auto pole = [&](double seconds) { return 1. - std::exp(-1. / (rate * seconds)); };
        onsetFastAttack = pole(.001);onsetFastRelease = pole(.03);onsetSlowAttack = pole(.03);onsetSlowRelease = pole(.3);
        for (auto& channel : filters) for (auto& f : channel) f.prepare(rate);
        reset();
    }
    void setInitialState(const FilterSettings& s) {
        fade.setCurrentAndTargetValue(s.on ? 1.f : 0.f);
        mix.setCurrentAndTargetValue(s.mix);
        output.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(s.on ? s.outputDb : 0.f));
        for (int f = 0; f < filterCount; ++f) {
            logCutoff[size_t(f)].setCurrentAndTargetValue(std::log2(s.filters[size_t(f)].cutoff));
            resonance[size_t(f)].setCurrentAndTargetValue(s.filters[size_t(f)].resonance);
            drive[size_t(f)].setCurrentAndTargetValue(s.filters[size_t(f)].drive);
        }
    }
    void reset() noexcept {
        for (auto& channel : filters) for (auto& f : channel) f.reset();
        envelope = {}; squareState = {}; phase = {}; held = {}; armed = true; seed = 0x1234567u; onsetFast = onsetSlow = 0.;
    }
    bool idle() const noexcept { return fade.getCurrentValue() <= 0.f && !fade.isSmoothing(); }
    double lfoPhase(int lfo = 0) const noexcept { return phase[size_t(juce::jlimit(0, 1, lfo))]; }

    // `ppq`: the host position in quarter notes at the block start (NaN when
    // there is none); `ppqBpm`: the host tempo that position advances at
    // (NaN: `bpm`). `key` / `keyChannels`: an external envelope source.
    void process(float* const* data, int channels, int samples, const FilterSettings& s, double bpm,
                 double ppq = std::numeric_limits<double>::quiet_NaN(), const float* const* key = nullptr, int keyChannels = 0,
                 double ppqBpm = std::numeric_limits<double>::quiet_NaN()) noexcept {
        fade.setTargetValue(s.on ? 1.f : 0.f);
        mix.setTargetValue(s.mix);
        output.setTargetValue(juce::Decibels::decibelsToGain(s.on ? s.outputDb : 0.f));
        for (int f = 0; f < filterCount; ++f) {
            logCutoff[size_t(f)].setTargetValue(std::log2(s.filters[size_t(f)].cutoff));
            resonance[size_t(f)].setTargetValue(s.filters[size_t(f)].resonance);
            drive[size_t(f)].setTargetValue(s.filters[size_t(f)].drive);
        }
        const std::array<int, 2> sync{s.lfoSync, s.lfo2Sync}, shape{s.lfoShape, s.lfo2Shape};
        const std::array<double, 2> rates{s.lfoRate, s.lfo2Rate};
        std::array<double, 2> step{};
        for (int l = 0; l < 2; ++l) {
            const double hz = sync[size_t(l)] > 0 && bpm > 0. ? bpm / 60. / filterSyncBeats[size_t(sync[size_t(l)])] : rates[size_t(l)];
            step[size_t(l)] = juce::jlimit(0., .5, hz / rate);
        }
        // Bar lock: a synced LFO's phase is the host position divided by its
        // cycle length, so it lands on the same beat every time.
        const double positionBpm = std::isfinite(ppqBpm) && ppqBpm > 0. ? ppqBpm : bpm;
        const bool hostPosition = s.lfoLock && std::isfinite(ppq) && positionBpm > 0.;
        const double beatsPerSample = positionBpm / 60. / rate;
        const double attack = 1. - std::exp(-1. / (rate * s.envAttackMs * .001));
        const double release = 1. - std::exp(-1. / (rate * s.envReleaseMs * .001));
        const double sensitivity = juce::Decibels::decibelsToGain(double(s.envSensitivityDb));
        const double offset = s.lfoStereoDeg / 360.;
        const bool anyOn = s.filters[0].on || s.filters[1].on;
        const bool sidechain = s.envSource == 1 && key != nullptr && keyChannels > 0;
        for (int n = 0; n < samples; ++n) {
            const float g = fade.getNextValue(), wetMix = mix.getNextValue(), gain = output.getNextValue();
            std::array<double, filterCount> octave{}, q{}, res{}, drv{};
            for (int f = 0; f < filterCount; ++f) {
                octave[size_t(f)] = logCutoff[size_t(f)].getNextValue();
                res[size_t(f)] = resonance[size_t(f)].getNextValue() * .01;
                drv[size_t(f)] = drive[size_t(f)].getNextValue();   // 20 ms, so automating drive does not step
                q[size_t(f)] = filter_detail::resonanceQ(res[size_t(f)]);
            }
            for (int l = 0; l < 2; ++l)
                if (hostPosition && sync[size_t(l)] > 0) {
                    const double cycle = (ppq + n * beatsPerSample) / filterSyncBeats[size_t(sync[size_t(l)])];
                    const double next = cycle - std::floor(cycle);
                    if (next < phase[size_t(l)]) held[size_t(l)] = random();
                    phase[size_t(l)] = next;
                }
            double loudest = 0.;   // the loudest input this sample (the envelope's source)
            for (int c = 0; c < channels; ++c) {
                const double x = data[c][n];
                // Each channel follows its own input envelope, or the sidechain's.
                const double source = sidechain ? key[std::min(c, keyChannels - 1)][n] : x;
                const double level = std::abs(source) * sensitivity;
                auto& e = envelope[size_t(c)];
                e += (level > e ? attack : release) * (level - e);
                loudest = std::max(loudest, std::abs(source));
                std::array<double, 2> lfo{};
                for (int l = 0; l < 2; ++l) {
                    double p = phase[size_t(l)] + (c == 1 ? offset : 0.);
                    p -= std::floor(p);
                    double value = filter_detail::lfoValue(shape[size_t(l)], p, held[size_t(l)]);
                    // Square and Random step through a 2 ms one-pole so their edges do not click.
                    auto& smoothed = squareState[size_t(l)][size_t(c)];
                    smoothed += squareSmoothing * (value - smoothed);
                    if (shape[size_t(l)] == 2 || shape[size_t(l)] == 4) value = smoothed;
                    lfo[size_t(l)] = value;
                }
                const double env = juce::jmin(1., e);
                std::array<double, filterCount> out{};
                double y = x;
                for (int f = 0; f < filterCount; ++f) {
                    const auto& unit = s.filters[size_t(f)];
                    const double cutoff = std::exp2(octave[size_t(f)] + unit.lfoOct * lfo[0] + unit.lfo2Oct * lfo[1] + unit.envOct * env);
                    const double input = s.routing == 0 ? y : x;
                    // A disabled filter keeps running on its input, so turning it on
                    // starts from a settled state; its output is simply not used.
                    out[size_t(f)] = filters[size_t(c)][size_t(f)].process(input, unit.type, unit.slope, cutoff, q[size_t(f)], rate, res[size_t(f)], drv[size_t(f)]);
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
            // Retrigger (when not locked to the bar): an attack restarts both
            // LFOs. An attack is a fast follower (1 / 30 ms) rising 4 dB above a
            // slow one (30 / 300 ms) from above -40 dBFS; it re-arms once the
            // fast follower settles within 1.2 dB of the slow one.
            onsetFast += (loudest > onsetFast ? onsetFastAttack : onsetFastRelease) * (loudest - onsetFast);
            onsetSlow += (loudest > onsetSlow ? onsetSlowAttack : onsetSlowRelease) * (loudest - onsetSlow);
            // A bar-locked LFO follows the host position instead.
            if (s.lfoRetrigger) {
                if (armed && onsetFast > .01 && onsetFast > 1.6 * onsetSlow) {
                    for (int l = 0; l < 2; ++l) if (!(hostPosition && sync[size_t(l)] > 0)) { phase[size_t(l)] = 0.; held[size_t(l)] = random(); }
                    armed = false;
                }
                else if (onsetFast < 1.15 * onsetSlow) armed = true;
            }
            for (int l = 0; l < 2; ++l)
                if (!(hostPosition && sync[size_t(l)] > 0)) {
                    phase[size_t(l)] += step[size_t(l)];
                    if (phase[size_t(l)] >= 1.) { phase[size_t(l)] -= 1.;held[size_t(l)] = random(); }
                }
        }
    }

private:
    double random() noexcept { seed = seed * 1664525u + 1013904223u;return double(seed >> 8) / 8388608. - 1.; }
    double rate{48000.}, squareSmoothing{};
    std::array<double, 2> phase{}, held{};
    bool armed{true};std::uint32_t seed{0x1234567u};
    double onsetFast{}, onsetSlow{}, onsetFastAttack{}, onsetFastRelease{}, onsetSlowAttack{}, onsetSlowRelease{};
    std::array<std::array<filter_detail::Filter, filterCount>, 2> filters{};
    std::array<double, 2> envelope{};
    std::array<std::array<double, 2>, 2> squareState{};   // per LFO, per channel
    juce::SmoothedValue<float> fade, mix, output;
    std::array<juce::SmoothedValue<float>, filterCount> logCutoff, resonance, drive;
};

class FilterModulation {
public:
    void bind(juce::AudioProcessorValueTreeState& state) {
        for (size_t i = 0; i < globalIDs.size(); ++i) globals[i] = state.getRawParameterValue(globalIDs[i]);
        for (int f = 0; f < filterCount; ++f)
            for (size_t i = 0; i < fieldIDs.size(); ++i) fields[size_t(f)][i] = state.getRawParameterValue(filterID(f, fieldIDs[i]));
        // 1.9 (absent in hosts that build only the 1.7 bank: the defaults apply).
        for (size_t i = 0; i < extensionIDs.size(); ++i) extension[i] = state.getParameter(extensionIDs[i]) ? state.getRawParameterValue(extensionIDs[i]) : nullptr;
        for (int f = 0; f < filterCount; ++f)
            for (size_t i = 0; i < extensionFieldIDs.size(); ++i)
                extensionFields[size_t(f)][i] = state.getParameter(filterID(f, extensionFieldIDs[i])) ? state.getRawParameterValue(filterID(f, extensionFieldIDs[i])) : nullptr;
    }
    void prepare(double sampleRate) { core.prepare(sampleRate); core.setInitialState(settings()); }
    void reset() noexcept { core.reset(); }
    // `ppq`: host position at the block start (NaN when unknown). `key`:
    // the external sidechain, used when the envelope source is Sidechain.
    void process(juce::AudioBuffer<float>& buffer, double bpm, double ppq = std::numeric_limits<double>::quiet_NaN(),
                 const juce::AudioBuffer<float>* key = nullptr, double ppqBpm = std::numeric_limits<double>::quiet_NaN()) noexcept {
        const auto s = settings();
        const int channels = juce::jmin(2, buffer.getNumChannels()), samples = buffer.getNumSamples();
        if (channels == 0 || samples == 0) return;
        if (core.idle()) {
            if (!s.on) return;   // Off: the input stays untouched.
            core.reset();        // Turning on starts from cleared filter state.
        }
        const bool keyed = key != nullptr && key->getNumSamples() == samples && key->getNumChannels() > 0;
        core.process(buffer.getArrayOfWritePointers(), channels, samples, s, bpm, ppq,
                     keyed ? key->getArrayOfReadPointers() : nullptr, keyed ? key->getNumChannels() : 0, ppqBpm);
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
        s.lfoShape = juce::roundToInt(read(globals[4], 0.f, float(filterLfoShapeCount - 1), 0.f));
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
            u.type = juce::roundToInt(read(p[1], 0.f, float(filterTypeCount - 1), float(d.type)));
            u.slope = juce::roundToInt(read(p[2], 0.f, 1.f, 0.f));
            u.cutoff = read(p[3], 20.f, 20000.f, d.cutoff);
            u.resonance = read(p[4], 0.f, 100.f, 20.f);
            u.lfoOct = read(p[5], -4.f, 4.f, 0.f);
            u.envOct = read(p[6], -4.f, 4.f, 0.f);
            const auto& x = extensionFields[size_t(f)];
            u.lfo2Oct = read(x[0], -4.f, 4.f, 0.f);
            u.drive = read(x[1], 0.f, 100.f, 0.f) / 100.f;
        }
        s.lfo2Shape = juce::roundToInt(read(extension[0], 0.f, float(filterLfoShapeCount - 1), 1.f));
        s.lfo2Rate = read(extension[1], .05f, 20.f, .25f);
        s.lfo2Sync = juce::roundToInt(read(extension[2], 0.f, 9.f, 0.f));
        s.lfoLock = read(extension[3], 0.f, 1.f, 0.f) > .5f;
        s.lfoRetrigger = read(extension[4], 0.f, 1.f, 0.f) > .5f;
        s.envSource = juce::roundToInt(read(extension[5], 0.f, 1.f, 0.f));
        return s;
    }

private:
    inline static constexpr std::array<const char*, filterGlobalParameterCount> globalIDs{
        "fltOn", "fltMix", "fltOutput", "fltRouting", "fltLfoShape", "fltLfoRate", "fltLfoSync", "fltLfoStereo",
        "fltEnvAttack", "fltEnvRelease", "fltEnvSensitivity"};
    inline static constexpr std::array<const char*, filterFieldCount> fieldIDs{"on", "type", "slope", "cutoff", "resonance", "lfo", "env"};
    inline static constexpr std::array<const char*, 6> extensionIDs{"fltLfo2Shape", "fltLfo2Rate", "fltLfo2Sync", "fltLfoLock", "fltLfoRetrigger", "fltEnvSource"};
    inline static constexpr std::array<const char*, 2> extensionFieldIDs{"lfo2", "drive"};
    std::array<std::atomic<float>*, 6> extension{};
    std::array<std::array<std::atomic<float>*, 2>, filterCount> extensionFields{};
    std::array<std::atomic<float>*, filterGlobalParameterCount> globals{};
    std::array<std::array<std::atomic<float>*, filterFieldCount>, filterCount> fields{};
    FilterModulationCore core;
};

} // namespace spectralforge
