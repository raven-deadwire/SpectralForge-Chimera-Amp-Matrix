#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <cmath>

// SYNTH: a second filter, filter routing and a per-voice distortion
// (docs/SYNTH_FILTERS.md). Thirteen parameters appended at version hint 39.
// With filter 2 and the distortion off (the defaults) the voice is the
// released one, sample for sample.
namespace spectralforge {

inline constexpr int synthFiltersParameterVersion = 39;
inline constexpr int synthFiltersParameterCount = 13;

// Filter 2: the six released types, then two combs. (Filter 1 keeps its six:
// a longer list would change what its automation values mean.)
inline constexpr int synthFilter2TypeCount = 8, synthFilterCombPlus = 6, synthFilterCombMinus = 7;
inline juce::StringArray synthFilter2TypeNames() { return {"Low-pass 12", "Low-pass 24", "High-pass 12", "High-pass 24", "Band-pass", "Notch", "Comb +", "Comb -"}; }
inline juce::StringArray synthFilterRoutingNames() { return {"Serial  1 > 2", "Parallel  1 + 2"}; }

inline constexpr int synthDistTypeCount = 6;
inline juce::StringArray synthDistTypeNames() { return {"Soft clip", "Hard clip", "Wavefold", "Tube", "Sine fold", "Bit crush"}; }
// Where the distortion sits in the voice. In Parallel, "After filter 1"
// distorts filter 1's branch only.
enum SynthDistPlace { synthDistBeforeFilters, synthDistAfterFilter1, synthDistAfterFilters, synthDistPlaceCount };
inline juce::StringArray synthDistPlaceNames() { return {"Before filters", "After filter 1", "After filters"}; }

// DRIVE 0-1 is 0 to +36 dB into the shaper; Bit crush reads it as 16 bits
// down to 3 instead.
inline double synthDistGain(double drive) noexcept { return std::pow(10.0, 1.8 * std::clamp(drive, 0.0, 1.0)); }
inline double synthDistSteps(double drive) noexcept { return std::pow(2.0, 15.0 - 13.0 * std::clamp(drive, 0.0, 1.0)); }
// One sample through the shaper. Every shape is bounded by +-1 for any input
// (Bit crush: by the input's own peak, rounded).
inline double synthDistort(int type, double gain, double steps, double x) noexcept {
    switch (type) {
        case 0: return std::tanh(gain * x);
        case 1: return std::clamp(gain * x, -1.0, 1.0);
        case 2: { double v = gain * x * .25 + .25;v -= std::floor(v);return 1.0 - 4.0 * std::abs(v - .5); }   // triangle fold
        case 3: { const double b = std::tanh(.2 * gain);return (std::tanh(gain * (x + .2)) - b) / (1.0 + b); }   // biased: even harmonics at any drive (the engine removes its DC)
        case 4: return std::sin(1.5707963267948966 * gain * x);
        default: return std::round(x * steps) / steps;
    }
}

// The comb's feedback for a resonance of 0-1 (never silent: 0.4 to 0.98), and
// its magnitude at `hz`. The output is scaled so white noise keeps its level.
inline double synthCombFeedback(double resonance, bool negative) noexcept { return (negative ? -1.0 : 1.0) * (.4 + .58 * std::clamp(resonance, 0.0, 1.0)); }
inline double synthCombMagnitude(bool negative, double cutoffHz, double resonance, double hz, double rate) noexcept {
    const double fb = synthCombFeedback(resonance, negative);
    const double delay = std::max(1.0, rate / std::clamp(cutoffHz, 20.0, rate * .45));
    const double w = 6.283185307179586 * hz / rate * delay;
    return std::sqrt(1.0 - fb * fb) / std::sqrt(std::max(1e-12, 1.0 - 2.0 * fb * std::cos(w) + fb * fb));
}

// Parallel balance 0-1: both filters at unity in the centre, one alone at each end.
inline double synthParallelGain(double balance, bool second) noexcept { return std::min(1.0, 2.0 * (second ? balance : 1.0 - balance)); }

inline void appendSynthFiltersParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    constexpr int v = synthFiltersParameterVersion;
    const auto number = [&](const char* id, const char* name, float lo, float hi, float initial, float centre, const char* unit) {
        juce::NormalisableRange<float> range{lo, hi, 1.f};
        if (centre > lo && centre < hi) range.setSkewForCentre(centre);
        const juce::String suffix(unit);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, v}, name, range, initial,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([suffix](float value, int) { return juce::String(juce::roundToInt(value)) + suffix; })));
    };
    const auto toggle = [&](const char* id, const char* name) { layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, v}, name, false)); };
    const auto choice = [&](const char* id, const char* name, const juce::StringArray& options, int initial) {
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, v}, name, options, initial));
    };
    toggle("synthFilter2On", "Synth filter 2 on");
    choice("synthFilter2Type", "Synth filter 2 type", synthFilter2TypeNames(), 2);
    number("synthFilter2Cutoff", "Synth filter 2 cutoff", 20.f, 20000.f, 200.f, 1000.f, " Hz");
    number("synthFilter2Resonance", "Synth filter 2 resonance", 0.f, 100.f, 20.f, 0.f, " %");
    number("synthFilter2KeyTrack", "Synth filter 2 key tracking", 0.f, 100.f, 0.f, 0.f, " %");
    number("synthFilter2Env", "Synth filter 2 envelope amount", -100.f, 100.f, 0.f, 0.f, " %");
    choice("synthFilterRouting", "Synth filter routing", synthFilterRoutingNames(), 0);
    number("synthFilterBalance", "Synth filter balance", 0.f, 100.f, 50.f, 0.f, " %");
    toggle("synthDistOn", "Synth distortion on");
    choice("synthDistType", "Synth distortion type", synthDistTypeNames(), 0);
    choice("synthDistPlace", "Synth distortion position", synthDistPlaceNames(), 0);
    number("synthDistDrive", "Synth distortion drive", 0.f, 100.f, 30.f, 0.f, " %");
    number("synthDistMix", "Synth distortion mix", 0.f, 100.f, 100.f, 0.f, " %");
}
} // namespace spectralforge
