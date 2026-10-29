#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <memory>
#include <utility>
#include <vector>

namespace spectralforge {

// SYNTH (docs/SYNTH_1_10.md, docs/SYNTH_PAGE.md, docs/SYNTH_V2.md): a synth
// played by the guitar (pitch and level tracked from the clean input, one
// voice) and by host MIDI (up to eight voices). Three engine types (Dvergr
// Analog, Seidr Dynamic, Mimir Impact) set the character of a shared voice:
// three oscillators, each a morphing wave with unison or a loaded sample, a
// sub oscillator and noise, a multi-mode filter, two ADSRs, two LFOs and an
// eight-slot modulation matrix. Its output joins the rig at one of four
// points. Off (the default) runs nothing and adds nothing. Version hint 27.
inline constexpr int synthParameterVersion = 27;
inline constexpr int synthMaxVoices = 8;
inline constexpr int synthMaxUnison = 7;
inline constexpr int synthModSlots = 8;
inline constexpr int synthInsertCount = 4;
inline constexpr int synthOscCount = 3;
enum class SynthInsert { ampInput = 0, beforeToneEQ = 1, beforePost = 2, afterPost = 3 };
enum SynthType { synthAnalog, synthDynamic, synthImpact };
inline juce::StringArray synthTypeNames() { return {"Dvergr Analog", "Seidr Dynamic", "Mimir Impact"}; }
inline juce::StringArray synthSourceNames() { return {"Guitar + MIDI", "Guitar", "MIDI"}; }
inline juce::StringArray synthInsertNames() { return {"Amp input", "Before tone / dynamics", "Before POST", "After POST"}; }
inline juce::StringArray synthOscModeNames() { return {"Wave", "Sample"}; }
inline juce::StringArray synthWaveNames() { return {"Sine", "Triangle", "Saw", "Square", "Pulse"}; }
inline juce::StringArray synthFilterTypeNames() { return {"Low-pass 12", "Low-pass 24", "High-pass 12", "High-pass 24", "Band-pass", "Notch"}; }
inline juce::StringArray synthLfoShapeNames() { return {"Sine", "Triangle", "Saw", "Square", "Sample & hold"}; }
inline juce::StringArray synthLfoSyncNames() { return {"Free", "1/1", "1/2", "1/4", "1/8", "1/16", "1/4 T", "1/8 T"}; }
inline constexpr std::array<double, 8> synthLfoSyncBeats{0., 4., 2., 1., .5, .25, 2. / 3., 1. / 3.};
enum SynthModSource { modOff, modEnv2, modLfo1, modLfo2, modVelocity, modGuitarLevel, modNote, modWheel, synthModSourceCount };
inline juce::StringArray synthModSourceNames() { return {"Off", "Env 2", "LFO 1", "LFO 2", "Velocity", "Guitar level", "Note", "Mod wheel"}; }
enum SynthModTarget { targetOff, targetPitchA, targetPitchB, targetPitchC, targetWaveA, targetWaveB, targetWaveC, targetLevelA, targetLevelB, targetLevelC,
                      targetFM, targetCutoff, targetResonance, targetSubLevel, targetNoiseLevel, targetAmp, targetDetune, targetCharacter, synthModTargetCount };
inline juce::StringArray synthModTargetNames() {
    return {"Off", "Pitch A", "Pitch B", "Pitch C", "Wave A", "Wave B", "Wave C", "Level A", "Level B", "Level C",
            "FM", "Cutoff", "Resonance", "Sub level", "Noise level", "Amp", "Detune", "Character"};
}
// Full-scale (amount 100 %) depth of each target, in its own unit:
// semitones, wave positions (no effect in Sample mode), levels, FM index, octaves, resonance, amp, detune, character.
inline constexpr std::array<double, synthModTargetCount> synthModDepth{0, 24, 24, 24, 4, 4, 4, 1, 1, 1, 8, 6, 1, 1, 1, 1, 1, 1};
inline juce::String synthOscID(int osc, const char* field) { return juce::String(osc == 0 ? "synthOscA" : osc == 1 ? "synthOscB" : "synthOscC") + field; }
inline juce::String synthModID(int slot, const char* field) { return "synthMod" + juce::String(slot + 1) + field; }
inline juce::String synthNoteName(int note) {
    static const char* names[]{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return juce::String(names[(note % 12 + 12) % 12]) + juce::String(note / 12 - 1);
}

// Wave position 0-4: sine, triangle, saw, square, 12.5 % pulse; positions in
// between crossfade the two neighbouring shapes. Naive shapes for displays;
// the engine uses band-limited versions of the same shapes.
inline double synthWaveShape(int shape, double phase) noexcept {
    phase -= std::floor(phase);
    switch (shape) {
        case 0: return std::sin(6.283185307179586 * phase);
        case 1: return 4.0 * std::abs(phase - .5) - 1.0;
        case 2: return 1.0 - 2.0 * phase;
        case 3: return phase < .5 ? 1.0 : -1.0;
        default: return phase < .125 ? 1.0 : -1.0;
    }
}
inline double synthWaveDisplay(double position, double phase) noexcept {
    position = std::clamp(position, 0.0, 4.0);
    const int a = std::min(3, int(position));const double t = position - a;
    return (1.0 - t) * synthWaveShape(a, phase) + t * synthWaveShape(a + 1, phase);
}
// "Saw", or "Saw > Square 40 %" between two shapes.
inline juce::String synthWaveLabel(double position) {
    position = std::clamp(position, 0.0, 4.0);
    const int a = std::min(3, int(position));const int t = juce::roundToInt(100.0 * (position - a));
    if (t == 0) return synthWaveNames()[a];
    if (t == 100) return synthWaveNames()[a + 1];
    return synthWaveNames()[a] + " > " + synthWaveNames()[a + 1] + " " + juce::String(t) + " %";
}
// Magnitude of the synth filter at `hz` (linear), exact for the zero-delay
// state-variable filters the engine runs (bilinear, prewarped at the cutoff).
inline double synthFilterMagnitude(int type, double cutoffHz, double resonance, double hz, double rate) noexcept {
    const double nyquist = rate * .5;
    cutoffHz = std::clamp(cutoffHz, 20.0, rate * .45);hz = std::clamp(hz, 1.0, nyquist * .999);
    const double w = std::tan(3.141592653589793 * hz / rate) / std::tan(3.141592653589793 * cutoffHz / rate);
    const double k = 2.0 - 1.9 * std::clamp(resonance, 0.0, 1.0);
    const std::complex<double> s(0.0, w), d = s * s + k * s + 1.0;
    std::complex<double> h;
    switch (type) {
        case 0: h = 1.0 / d;break;
        case 1: h = 1.0 / (d * d);break;
        case 2: h = s * s / d;break;
        case 3: h = (s * s / d) * (s * s / d);break;
        case 4: h = k * s / d;break;
        default: h = (s * s + 1.0) / d;break;
    }
    return std::abs(h);
}
// ADSR shape for graphs and tests: linear attack over A, then decay and
// release move 60 dB of their span in D and R. Times in seconds.
struct SynthEnvelopeSettings { double attack{.005}, decay{.3}, sustain{.8}, release{.3}; };
inline double synthEnvelopeLevel(const SynthEnvelopeSettings& e, double t, double held) noexcept {
    const auto gated = [&](double time) {
        if (time < e.attack) return time / std::max(e.attack, 1e-6);
        const double d = time - e.attack;
        return e.sustain + (1.0 - e.sustain) * std::exp(-6.907755 * d / std::max(e.decay, 1e-6));
    };
    if (t <= held) return gated(t);
    return gated(held) * std::exp(-6.907755 * (t - held) / std::max(e.release, 1e-6));
}
inline double synthLfoShape(int shape, double phase, double held) noexcept {
    phase -= std::floor(phase);
    switch (shape) {
        case 0: return std::sin(6.283185307179586 * phase);
        case 1: return 4.0 * std::abs(phase - .5) - 1.0;
        case 2: return 1.0 - 2.0 * phase;
        case 3: return phase < .5 ? 1.0 : -1.0;
        default: return held;
    }
}

// A loaded sample: mono or stereo audio at its own rate, a 256-column
// min / max overview for the page, and the pitch found when it was loaded.
// Immutable once built; the engine reads it, the message thread swaps it.
struct SynthSample {
    static constexpr double maximumSeconds = 20.0;
    static constexpr int overviewColumns = 256;
    juce::String name;
    double sampleRate{48000}, detectedHz{};
    int length{};
    std::array<std::vector<float>, 2> data;   // data[1] is empty for a mono sample
    std::array<std::pair<float, float>, overviewColumns> overview{};
    bool stereo() const noexcept { return !data[1].empty(); }
};

// Parameters. All automatable; every default is the Init patch.
inline void appendSynthParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    constexpr int v = synthParameterVersion;
    const auto number = [&](const juce::String& id, const juce::String& name, float lo, float hi, float initial, float centre, const juce::String& unit, int decimals) {
        juce::NormalisableRange<float> range{lo, hi, decimals > 0 ? .01f : 1.f};
        if (centre > lo && centre < hi) range.setSkewForCentre(centre);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, v}, name, range, initial,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([unit, decimals](float value, int) { return juce::String(value, decimals) + unit; })));
    };
    const auto toggle = [&](const juce::String& id, const juce::String& name, bool initial) { layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, v}, name, initial)); };
    const auto choice = [&](const juce::String& id, const juce::String& name, const juce::StringArray& options, int initial) {
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, v}, name, options, initial));
    };
    const auto integer = [&](const juce::String& id, const juce::String& name, int lo, int hi, int initial) {
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{id, v}, name, lo, hi, initial));
    };
    // Global
    toggle("synthOn", "Synth", false);
    choice("synthType", "Synth type", synthTypeNames(), synthAnalog);
    number("synthCharacter", "Synth character", 0.f, 100.f, 50.f, 0.f, " %", 0);
    choice("synthSource", "Synth source", synthSourceNames(), 0);
    choice("synthInsert", "Synth insert point", synthInsertNames(), int(SynthInsert::beforePost));
    number("synthLevel", "Synth level", -36.f, 6.f, -6.f, 0.f, " dB", 1);
    integer("synthOctave", "Synth octave", -2, 1, 0);
    number("synthGlide", "Synth glide", 0.f, 1000.f, 0.f, 80.f, " ms", 0);
    number("synthSensitivity", "Synth tracking threshold", -60.f, -12.f, -40.f, 0.f, " dBFS", 0);
    integer("synthVoices", "Synth MIDI voices", 1, synthMaxVoices, 4);
    number("synthWidth", "Synth stereo width", 0.f, 100.f, 50.f, 0.f, " %", 0);
    number("synthFM", "Synth FM B to A", 0.f, 100.f, 0.f, 0.f, " %", 0);
    // Oscillators A, B and C
    for (int osc = 0; osc < synthOscCount; ++osc) {
        const juce::String n = juce::String("Synth osc ") + "ABC"[osc] + " ";
        toggle(synthOscID(osc, "On"), n + "on", osc == 0);
        choice(synthOscID(osc, "Mode"), n + "mode", synthOscModeNames(), 0);
        number(synthOscID(osc, "Wave"), n + "wave", 0.f, 4.f, 2.f, 0.f, "", 2);
        integer(synthOscID(osc, "Octave"), n + "octave", -3, 3, 0);
        integer(synthOscID(osc, "Semi"), n + "semitones", -12, 12, 0);
        number(synthOscID(osc, "Fine"), n + "fine", -100.f, 100.f, 0.f, 0.f, " ct", 0);
        integer(synthOscID(osc, "Unison"), n + "unison voices", 1, synthMaxUnison, 1);
        number(synthOscID(osc, "Detune"), n + "unison detune", 0.f, 100.f, 20.f, 0.f, " %", 0);
        number(synthOscID(osc, "Level"), n + "level", 0.f, 100.f, osc == 0 ? 80.f : osc == 1 ? 60.f : 50.f, 0.f, " %", 0);
        number(synthOscID(osc, "Start"), n + "sample start", 0.f, 100.f, 0.f, 0.f, " %", 1);
        toggle(synthOscID(osc, "Loop"), n + "sample loop", true);
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{synthOscID(osc, "Root"), v}, n + "sample root", 0, 127, 60,
            juce::AudioParameterIntAttributes().withStringFromValueFunction([](int note, int) { return synthNoteName(note); })));
    }
    // Sub and noise
    toggle("synthSubOn", "Synth sub on", false);
    choice("synthSubOctave", "Synth sub octave", {"-1 oct", "-2 oct"}, 0);
    choice("synthSubShape", "Synth sub shape", {"Sine", "Square"}, 0);
    number("synthSubLevel", "Synth sub level", 0.f, 100.f, 50.f, 0.f, " %", 0);
    toggle("synthNoiseOn", "Synth noise on", false);
    number("synthNoiseColor", "Synth noise colour", 0.f, 100.f, 70.f, 0.f, " %", 0);
    number("synthNoiseLevel", "Synth noise level", 0.f, 100.f, 20.f, 0.f, " %", 0);
    // Filter
    toggle("synthFilterOn", "Synth filter on", true);
    choice("synthFilterType", "Synth filter type", synthFilterTypeNames(), 1);
    number("synthCutoff", "Synth filter cutoff", 20.f, 20000.f, 6000.f, 1000.f, " Hz", 0);
    number("synthResonance", "Synth filter resonance", 0.f, 100.f, 20.f, 0.f, " %", 0);
    number("synthDrive", "Synth filter drive", 0.f, 100.f, 0.f, 0.f, " %", 0);
    number("synthKeyTrack", "Synth filter key tracking", 0.f, 100.f, 0.f, 0.f, " %", 0);
    number("synthFilterEnv", "Synth filter envelope amount", -100.f, 100.f, 0.f, 0.f, " %", 0);
    // Envelopes: 1 = amplitude, 2 = filter / modulation
    for (int e = 0; e < 2; ++e) {
        const juce::String base = e == 0 ? "synthAmp" : "synthModEnv", n = e == 0 ? "Synth amp " : "Synth env 2 ";
        number(base + "Attack", n + "attack", .5f, 5000.f, 5.f, 200.f, " ms", 1);
        number(base + "Decay", n + "decay", 5.f, 10000.f, e == 0 ? 300.f : 400.f, 500.f, " ms", 0);
        number(base + "Sustain", n + "sustain", 0.f, 100.f, e == 0 ? 80.f : 0.f, 0.f, " %", 0);
        number(base + "Release", n + "release", 5.f, 10000.f, 300.f, 500.f, " ms", 0);
    }
    // LFOs
    for (int l = 0; l < 2; ++l) {
        const juce::String base = "synthLfo" + juce::String(l + 1), n = "Synth LFO " + juce::String(l + 1) + " ";
        choice(base + "Shape", n + "shape", synthLfoShapeNames(), 0);
        number(base + "Rate", n + "rate", .02f, 30.f, l == 0 ? 2.f : .5f, 2.f, " Hz", 2);
        choice(base + "Sync", n + "sync", synthLfoSyncNames(), 0);
    }
    // Modulation matrix
    for (int slot = 0; slot < synthModSlots; ++slot) {
        const juce::String n = "Synth mod " + juce::String(slot + 1) + " ";
        choice(synthModID(slot, "Source"), n + "source", synthModSourceNames(), 0);
        choice(synthModID(slot, "Target"), n + "target", synthModTargetNames(), 0);
        number(synthModID(slot, "Amount"), n + "amount", -100.f, 100.f, 0.f, 0.f, " %", 0);
    }
}
inline constexpr int synthParameterCount = 12 + synthOscCount * 12 + 4 + 3 + 7 + 2 * 4 + 2 * 3 + synthModSlots * 3;

// Patches. Applying one writes host parameters (a UI action, one gesture per
// parameter); nothing applies a patch on its own. Unlisted parameters return
// to their defaults. Each engine type has its factory patch.
struct SynthPatch { const char* name; std::vector<std::pair<juce::String, float>> values; };
inline std::vector<SynthPatch> synthPatches() {
    return {
        {"Init", {}},
        {"Dvergr Analog", {{"synthType", float(synthAnalog)}, {"synthCharacter", 60.f}, {"synthOscAWave", 2.f}, {"synthOscBOn", 1.f}, {"synthOscBWave", 2.f},
                           {"synthOscBFine", 12.f}, {"synthOscBLevel", 70.f}, {"synthSubOn", 1.f}, {"synthSubShape", 1.f}, {"synthSubLevel", 50.f},
                           {"synthFilterType", 1.f}, {"synthCutoff", 1200.f}, {"synthResonance", 35.f}, {"synthFilterEnv", 50.f}, {"synthModEnvAttack", 2.f},
                           {"synthModEnvDecay", 500.f}, {"synthModEnvSustain", 20.f}, {"synthAmpSustain", 85.f}}},
        {"Seidr Dynamic", {{"synthType", float(synthDynamic)}, {"synthCharacter", 70.f}, {"synthOscAWave", 3.5f}, {"synthOscBOn", 1.f}, {"synthOscBWave", 2.f},
                           {"synthOscBFine", 5.f}, {"synthOscBLevel", 30.f}, {"synthOscCOn", 1.f}, {"synthOscCWave", 0.f}, {"synthOscCOctave", 1.f},
                           {"synthOscCLevel", 20.f}, {"synthFilterType", 0.f}, {"synthCutoff", 600.f}, {"synthAmpAttack", 80.f}, {"synthAmpDecay", 500.f},
                           {"synthAmpSustain", 90.f}, {"synthAmpRelease", 900.f}, {"synthLfo1Rate", .3f}, {"synthMod1Source", float(modLfo1)},
                           {"synthMod1Target", float(targetWaveA)}, {"synthMod1Amount", 12.f}}},
        {"Mimir Impact", {{"synthType", float(synthImpact)}, {"synthCharacter", 70.f}, {"synthOscAWave", 0.f}, {"synthOscBOn", 1.f}, {"synthOscBWave", 0.f},
                          {"synthOscBOctave", 1.f}, {"synthOscBLevel", 0.f}, {"synthFM", 10.f}, {"synthFilterOn", 0.f}, {"synthAmpAttack", 1.f},
                          {"synthAmpDecay", 500.f}, {"synthAmpSustain", 0.f}, {"synthAmpRelease", 500.f}}},
    };
}
// A patch never changes these: switching, routing and level, and each
// oscillator's sample root (it belongs to the loaded sample).
inline bool synthPatchKeeps(const juce::String& id) {
    static const juce::StringArray kept{"synthOn", "synthSource", "synthInsert", "synthLevel", "synthSensitivity", "synthOscARoot", "synthOscBRoot", "synthOscCRoot"};
    return kept.contains(id);
}

// Monophonic pitch tracker: YIN (cumulative mean normalised difference) on
// the input decimated to about 12 kHz, every 64 decimated samples, over
// 55-1400 Hz (a seven-string low B is 61.7 Hz). A median of three estimates
// rejects single-frame octave slips. Allocation-free after prepare().
class PitchTracker {
public:
    static constexpr double lowestHz = 55.0, highestHz = 1400.0;
    void prepare(double sampleRate) {
        rate = sampleRate;
        factor = std::max(1, int(std::lround(rate / 12000.0)));
        decimatedRate = rate / factor;
        tauMax = int(std::ceil(decimatedRate / lowestHz)) + 2;
        tauMin = std::max(2, int(std::floor(decimatedRate / highestHz)));
        window = tauMax;
        ring.assign(size_t(1024), 0.f);
        frame.assign(size_t(window + tauMax + 2), 0.f);
        difference.assign(size_t(tauMax + 2), 0.f);
        // Anti-alias: two cascaded biquad low-passes at 0.2 x the decimated rate.
        const auto c = juce::dsp::IIR::Coefficients<float>::makeLowPass(rate, decimatedRate * .2, .7071);
        for (auto& f : filters) { f.coefficients = c; f.reset(); }
        reset();
    }
    void reset() noexcept {
        std::fill(ring.begin(), ring.end(), 0.f);
        for (auto& f : filters) f.reset();
        write = 0;phase = 0;hop = 0;frequency = 0;confidence = 0;level = 0;history = {};
    }
    // Mono input, any block size.
    void push(const float* left, const float* right, int samples) noexcept {
        for (int n = 0; n < samples; ++n) {
            float x = right != nullptr ? .5f * (left[n] + right[n]) : left[n];
            for (auto& f : filters) x = f.processSample(x);
            if (++phase < factor) continue;
            phase = 0;
            ring[size_t(write)] = x;write = (write + 1) & 1023;
            if (++hop >= 64) { hop = 0;analyse(); }
        }
    }
    double hz() const noexcept { return frequency; }
    double clarity() const noexcept { return confidence; }
    double rms() const noexcept { return level; }
    int analysisWindowSamples() const noexcept { return (window + tauMax) * factor; }
private:
    void analyse() noexcept {
        const int length = window + tauMax;
        for (int i = 0; i < length; ++i) frame[size_t(i)] = ring[size_t((write - length + i) & 1023)];
        double energy = 0;
        for (int i = length - window; i < length; ++i) energy += double(frame[size_t(i)]) * frame[size_t(i)];
        level = std::sqrt(energy / window);
        // d(tau) over the newest window against tau samples earlier.
        double running = 0;
        int best = -1;
        for (int tau = 1; tau <= tauMax; ++tau) {
            double d = 0;
            for (int j = 0; j < window; ++j) { const double e = double(frame[size_t(length - window + j)]) - frame[size_t(length - window + j - tau)];d += e * e; }
            running += d;
            difference[size_t(tau)] = float(running > 0 ? d * tau / running : 1.0);
        }
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (difference[size_t(tau)] < .15f) {
                while (tau + 1 < tauMax && difference[size_t(tau + 1)] < difference[size_t(tau)]) ++tau;
                best = tau;break;
            }
        if (best < 0) {   // no clear period: keep the last pitch, report low clarity
            int arg = tauMin;for (int tau = tauMin; tau < tauMax; ++tau) if (difference[size_t(tau)] < difference[size_t(arg)]) arg = tau;
            confidence = 1.0 - difference[size_t(arg)];
            return;
        }
        // Parabolic interpolation of the minimum.
        const double a = difference[size_t(best - 1)], b = difference[size_t(best)], c = difference[size_t(best + 1)];
        const double denominator = a - 2 * b + c, shift = std::abs(denominator) > 1e-12 ? .5 * (a - c) / denominator : 0.0;
        const double estimate = decimatedRate / (best + std::clamp(shift, -.5, .5));
        confidence = 1.0 - b;
        history = {history[1], history[2], estimate};
        auto sorted = history;std::sort(sorted.begin(), sorted.end());
        frequency = sorted[0] > 0 ? sorted[1] : estimate;
    }
    double rate{48000}, decimatedRate{12000};
    int factor{4}, tauMin{8}, tauMax{220}, window{220}, write{}, phase{}, hop{};
    std::vector<float> ring, frame, difference;
    std::array<juce::dsp::IIR::Filter<float>, 2> filters;
    double frequency{}, confidence{}, level{};
    std::array<double, 3> history{};
};


// The engine. Parameters are read once per block; envelopes, glide, LFOs and
// the modulation matrix run per sample. Allocation-free after prepare().
// Samples are swapped on the message thread under a spin lock that the audio
// thread only tries: a block that meets a swap in progress plays its sample
// oscillators silent.
class SynthLayer {
public:
    struct Settings {
        bool on{}; int type{}, source{}, insert{2}, octave{}, voices{4};
        float character{.5f}, levelDb{-6.f}, glideMs{}, sensitivityDb{-40.f}, width{.5f}, fm{};
        struct Osc { bool on{}; int mode{}; float wave{2.f}; int octave{}, semi{}; float fine{}; int unison{1}; float detune{.2f}, level{.8f}, start{}; bool loop{true}; int root{60}; };
        std::array<Osc, synthOscCount> osc{};
        bool subOn{}; int subOctave{}, subShape{}; float subLevel{.5f};
        bool noiseOn{}; float noiseColor{.7f}, noiseLevel{.2f};
        bool filterOn{true}; int filterType{1}; float cutoff{6000.f}, resonance{.2f}, drive{}, keyTrack{}, filterEnv{};
        std::array<SynthEnvelopeSettings, 2> env{};
        struct Lfo { int shape{}; float rate{2.f}; int sync{}; };
        std::array<Lfo, 2> lfo{};
        struct Slot { int source{}, target{}; float amount{}; };
        std::array<Slot, synthModSlots> mod{};
    };
    void bind(juce::AudioProcessorValueTreeState& state) {
        const auto get = [&](const juce::String& id) { return state.getRawParameterValue(id); };
        p.on = get("synthOn");p.type = get("synthType");p.character = get("synthCharacter");p.source = get("synthSource");p.insert = get("synthInsert");
        p.level = get("synthLevel");p.octave = get("synthOctave");
        p.glide = get("synthGlide");p.sensitivity = get("synthSensitivity");p.voices = get("synthVoices");p.width = get("synthWidth");p.fm = get("synthFM");
        for (int o = 0; o < synthOscCount; ++o) {
            auto& q = p.osc[size_t(o)];
            q.on = get(synthOscID(o, "On"));q.mode = get(synthOscID(o, "Mode"));q.wave = get(synthOscID(o, "Wave"));q.octave = get(synthOscID(o, "Octave"));
            q.semi = get(synthOscID(o, "Semi"));q.fine = get(synthOscID(o, "Fine"));q.unison = get(synthOscID(o, "Unison"));q.detune = get(synthOscID(o, "Detune"));
            q.level = get(synthOscID(o, "Level"));q.start = get(synthOscID(o, "Start"));q.loop = get(synthOscID(o, "Loop"));q.root = get(synthOscID(o, "Root"));
        }
        p.subOn = get("synthSubOn");p.subOctave = get("synthSubOctave");p.subShape = get("synthSubShape");p.subLevel = get("synthSubLevel");
        p.noiseOn = get("synthNoiseOn");p.noiseColor = get("synthNoiseColor");p.noiseLevel = get("synthNoiseLevel");
        p.filterOn = get("synthFilterOn");p.filterType = get("synthFilterType");p.cutoff = get("synthCutoff");p.resonance = get("synthResonance");
        p.drive = get("synthDrive");p.keyTrack = get("synthKeyTrack");p.filterEnv = get("synthFilterEnv");
        for (int e = 0; e < 2; ++e) {
            const juce::String base = e == 0 ? "synthAmp" : "synthModEnv";
            p.env[size_t(e)] = {get(base + "Attack"), get(base + "Decay"), get(base + "Sustain"), get(base + "Release")};
        }
        for (int l = 0; l < 2; ++l) {
            const juce::String base = "synthLfo" + juce::String(l + 1);
            p.lfo[size_t(l)] = {get(base + "Shape"), get(base + "Rate"), get(base + "Sync")};
        }
        for (int s = 0; s < synthModSlots; ++s) p.mod[size_t(s)] = {get(synthModID(s, "Source")), get(synthModID(s, "Target")), get(synthModID(s, "Amount"))};
    }
    void prepare(double sampleRate, int maximumBlock) {
        rate = sampleRate;
        tracker.prepare(rate);
        for (auto& ch : rendered) ch.assign(size_t(std::max(1, maximumBlock)), 0.f);
        // Analog drift: white noise through a one-pole low-pass at 1.5 Hz,
        // scaled to unit RMS (uniform noise has variance 1/3).
        driftCoefficient = 1.0 - std::exp(-6.283185307179586 * 1.5 / rate);
        driftScale = 1.0 / std::sqrt(driftCoefficient / (2.0 - driftCoefficient) / 3.0);
        reset();
    }
    void reset() noexcept {
        tracker.reset();
        for (auto& v : voices) v = Voice{};
        guitar = Voice{};
        midiNotes = {};midiCount = 0;bend = 0;wheel = 0;midiVelocity = 0;midiOnset = false;
        lfoPhase = {};lfoHeld = {};lfoValue = {};seed = 0x2545f491u;clock = 0;
        guitarGate = false;guitarLevel = 0;onsetLevel = 0;active = false;renderedSamples = 0;
        const auto s = read();
        for (int i = 0; i < synthInsertCount; ++i) gain[size_t(i)] = s.on && s.insert == i && toMain ? 1.f : 0.f;
        busGain = s.on && toBus ? 1.f : 0.f;
        scopeWrite.store(0);
    }
    Settings read() const noexcept {
        const auto f = [](const std::atomic<float>* a, float fallback) { const float v = a != nullptr ? a->load() : fallback; return std::isfinite(v) ? v : fallback; };
        const auto i = [&](const std::atomic<float>* a, int fallback, int lo, int hi) { return juce::jlimit(lo, hi, juce::roundToInt(f(a, float(fallback)))); };
        const auto pc = [&](const std::atomic<float>* a, float fallback) { return juce::jlimit(0.f, 1.f, f(a, fallback) * .01f); };
        Settings s;
        s.on = f(p.on, 0.f) > .5f;s.type = i(p.type, 0, 0, 2);s.character = pc(p.character, 50.f);s.source = i(p.source, 0, 0, 2);s.insert = i(p.insert, 2, 0, synthInsertCount - 1);
        s.levelDb = juce::jlimit(-36.f, 6.f, f(p.level, -6.f));s.octave = i(p.octave, 0, -2, 1);s.glideMs = juce::jlimit(0.f, 1000.f, f(p.glide, 0.f));
        s.sensitivityDb = juce::jlimit(-60.f, -12.f, f(p.sensitivity, -40.f));s.voices = i(p.voices, 4, 1, synthMaxVoices);
        s.width = pc(p.width, 50.f);s.fm = pc(p.fm, 0.f);
        for (int o = 0; o < synthOscCount; ++o) {
            const auto& q = p.osc[size_t(o)];auto& r = s.osc[size_t(o)];
            r.on = f(q.on, o == 0 ? 1.f : 0.f) > .5f;r.mode = i(q.mode, 0, 0, 1);r.wave = juce::jlimit(0.f, 4.f, f(q.wave, 2.f));r.octave = i(q.octave, 0, -3, 3);r.semi = i(q.semi, 0, -12, 12);
            r.fine = juce::jlimit(-100.f, 100.f, f(q.fine, 0.f));r.unison = i(q.unison, 1, 1, synthMaxUnison);r.detune = pc(q.detune, 20.f);
            r.level = pc(q.level, o == 0 ? 80.f : o == 1 ? 60.f : 50.f);r.start = pc(q.start, 0.f);r.loop = f(q.loop, 1.f) > .5f;r.root = i(q.root, 60, 0, 127);
        }
        s.subOn = f(p.subOn, 0.f) > .5f;s.subOctave = i(p.subOctave, 0, 0, 1);s.subShape = i(p.subShape, 0, 0, 1);s.subLevel = pc(p.subLevel, 50.f);
        s.noiseOn = f(p.noiseOn, 0.f) > .5f;s.noiseColor = pc(p.noiseColor, 70.f);s.noiseLevel = pc(p.noiseLevel, 20.f);
        s.filterOn = f(p.filterOn, 1.f) > .5f;s.filterType = i(p.filterType, 1, 0, 5);s.cutoff = juce::jlimit(20.f, 20000.f, f(p.cutoff, 6000.f));
        s.resonance = pc(p.resonance, 20.f);s.drive = pc(p.drive, 0.f);s.keyTrack = pc(p.keyTrack, 0.f);s.filterEnv = juce::jlimit(-1.f, 1.f, f(p.filterEnv, 0.f) * .01f);
        for (int e = 0; e < 2; ++e) {
            const auto& q = p.env[size_t(e)];
            s.env[size_t(e)] = {juce::jlimit(.5, 5000.0, double(f(q.attack, 5.f))) * .001, juce::jlimit(5.0, 10000.0, double(f(q.decay, 300.f))) * .001,
                                double(pc(q.sustain, e == 0 ? 80.f : 0.f)), juce::jlimit(5.0, 10000.0, double(f(q.release, 300.f))) * .001};
        }
        for (int l = 0; l < 2; ++l) {
            const auto& q = p.lfo[size_t(l)];
            s.lfo[size_t(l)] = {i(q.shape, 0, 0, 4), juce::jlimit(.02f, 30.f, f(q.rate, 2.f)), i(q.sync, 0, 0, int(synthLfoSyncBeats.size()) - 1)};
        }
        for (int m = 0; m < synthModSlots; ++m) {
            const auto& q = p.mod[size_t(m)];
            s.mod[size_t(m)] = {i(q.source, 0, 0, synthModSourceCount - 1), i(q.target, 0, 0, synthModTargetCount - 1), juce::jlimit(-1.f, 1.f, f(q.amount, 0.f) * .01f)};
        }
        return s;
    }
    // Message thread: installs `next` (or none) in an oscillator and returns
    // the sample it replaces, for the caller to destroy.
    std::unique_ptr<SynthSample> swapSample(int osc, std::unique_ptr<SynthSample> next) {
        const juce::SpinLock::ScopedLockType lock(sampleLock);
        std::swap(samples[size_t(juce::jlimit(0, synthOscCount - 1, osc))], next);
        return next;
    }
    // Message thread (the only writer): the oscillator's sample, or null.
    const SynthSample* sample(int osc) const noexcept { return samples[size_t(juce::jlimit(0, synthOscCount - 1, osc))].get(); }
    // Host MIDI for this block, applied at the block start.
    void handleMidi(const juce::MidiBuffer& midi) noexcept {
        for (const auto metadata : midi) {
            const auto m = metadata.getMessage();
            if (m.isNoteOn()) { pending[size_t(std::min(pendingCount, int(pending.size()) - 1))] = {true, m.getNoteNumber(), m.getFloatVelocity()};pendingCount = std::min(pendingCount + 1, int(pending.size())); }
            else if (m.isNoteOff()) { pending[size_t(std::min(pendingCount, int(pending.size()) - 1))] = {false, m.getNoteNumber(), 0.f};pendingCount = std::min(pendingCount + 1, int(pending.size())); }
            else if (m.isPitchWheel()) bend = (m.getPitchWheelValue() - 8192) / 8192.0 * 2.0;
            else if (m.isController() && m.getControllerNumber() == 1) wheel = m.getControllerValue() / 127.0;
            else if (m.isAllNotesOff() || m.isAllSoundOff()) { pendingCount = 0;allOff = true; }
        }
    }
    // Tracks the clean input (every block, before any processing).
    void analyse(const juce::AudioBuffer<float>& input) noexcept {
        if (!read().on) { if (active) tracker.reset(); return; }
        tracker.push(input.getReadPointer(0), input.getNumChannels() > 1 ? input.getReadPointer(1) : nullptr, input.getNumSamples());
    }
    // Renders the block once, ahead of the first insert point.
    void render(int samples, double tempo) noexcept {
        renderedSamples = 0;
        const auto s = read();
        if (samples > int(rendered[0].size())) return;
        bool anyGain = busGain > 0.f;for (const float g : gain) anyGain |= g > 0.f;
        if (!s.on && !active && !anyGain) { pendingCount = 0;return; }
        active = true;
        const juce::SpinLock::ScopedTryLockType lock(sampleLock);
        for (int o = 0; o < synthOscCount; ++o) playing[size_t(o)] = lock.isLocked() ? samples_(o) : nullptr;
        applyNotes(s);
        trackGuitar(s, samples);
        for (auto& ch : rendered) std::fill(ch.begin(), ch.begin() + samples, 0.f);
        const double bpm = std::isfinite(tempo) && tempo > 20 ? tempo : 120.0;
        for (int n = 0; n < samples; ++n) {
            advanceLfos(s, bpm);
            std::array<double, 2> out{};
            for (int i = 0; i < s.voices; ++i) if (voices[size_t(i)].active) addVoice(voices[size_t(i)], s, out);
            for (int i = s.voices; i < synthMaxVoices; ++i) if (voices[size_t(i)].active) { voices[size_t(i)].amp.noteOff();addVoice(voices[size_t(i)], s, out); }
            if (guitar.active) addVoice(guitar, s, out);
            const double level = juce::Decibels::decibelsToGain(double(s.levelDb));
            // Safety stage: linear up to 0 dBFS, then softly bounded to +6 dBFS.
            const auto bound = [](double x) { const double m = std::abs(x);return m <= 1.0 ? x : std::copysign(1.0 + std::tanh(m - 1.0), x); };
            rendered[0][size_t(n)] = float(bound(out[0] * level));rendered[1][size_t(n)] = float(bound(out[1] * level));
        }
        playing = {};
        renderedSamples = samples;
        // Scope: mono sum of the output for the page.
        int w = scopeWrite.load(std::memory_order_relaxed);
        for (int n = 0; n < samples; ++n) { scope[size_t(w)].store(.5f * (rendered[0][size_t(n)] + rendered[1][size_t(n)]), std::memory_order_relaxed);w = (w + 1) % int(scope.size()); }
        scopeWrite.store(w, std::memory_order_release);
        bool sounding = guitar.active;for (const auto& v : voices) sounding |= v.active;
        if (!s.on && !sounding && !anyGain) active = false;
    }
    // Adds the rendered synth at `point`. A change of insert point crossfades
    // over 20 ms; the synth joins only where its gain is non-zero.
    void addAt(SynthInsert point, juce::AudioBuffer<float>& buffer) noexcept {
        const int index = int(point);const auto s = read();
        const float target = s.on && s.insert == index && toMain ? 1.f : 0.f;
        float& g = gain[size_t(index)];
        if (renderedSamples <= 0) { g = target;return; }
        const int samples = std::min(renderedSamples, buffer.getNumSamples());
        if (g == 0.f && target == 0.f) return;
        const float step = float(1.0 / (.02 * rate));
        const int channels = buffer.getNumChannels();
        for (int n = 0; n < samples; ++n) {
            g = g < target ? std::min(target, g + step) : std::max(target, g - step);
            if (channels == 1) buffer.addSample(0, n, g * .5f * (rendered[0][size_t(n)] + rendered[1][size_t(n)]));
            else for (int c = 0; c < 2; ++c) buffer.addSample(c, n, g * rendered[size_t(c)][size_t(n)]);
        }
    }
    // Where the synth goes this block: into the guitar at its insert point,
    // to the SYNTH output bus, or both. Each change crossfades over 20 ms.
    void setOutputs(bool main, bool bus) noexcept { toMain = main;toBus = bus; }
    // Writes (replaces) the SYNTH bus channels for this block: the rendered
    // synth, or silence. `right` may equal `left` for a mono bus.
    void writeBus(float* left, float* right, int samples) noexcept {
        const float target = toBus && read().on ? 1.f : 0.f;
        const float step = float(1.0 / (.02 * rate));
        const int ready = std::min(samples, renderedSamples);
        for (int n = 0; n < samples; ++n) {
            busGain = busGain < target ? std::min(target, busGain + step) : std::max(target, busGain - step);
            const float l = n < ready ? busGain * rendered[0][size_t(n)] : 0.f, r = n < ready ? busGain * rendered[1][size_t(n)] : 0.f;
            if (right == left || right == nullptr) left[n] = .5f * (l + r);
            else { left[n] = l;right[n] = r; }
        }
    }
    const PitchTracker& pitchTracker() const noexcept { return tracker; }
    bool sounding() const noexcept { bool any = guitar.active;for (const auto& v : voices) any |= v.active;return active && any; }
    int soundingVoices() const noexcept { int n = guitar.active ? 1 : 0;for (const auto& v : voices) n += v.active ? 1 : 0;return n; }
    double voicePitch(int i) const noexcept { return i < 0 ? guitar.hz : voices[size_t(std::clamp(i, 0, synthMaxVoices - 1))].hz; }
    double lfoOutput(int i) const noexcept { return lfoValue[size_t(std::clamp(i, 0, 1))]; }
    // Copies the newest `count` scope samples (oldest first) for the page.
    void readScope(float* destination, int count) const noexcept {
        count = std::min(count, int(scope.size()));
        const int w = scopeWrite.load(std::memory_order_acquire);
        for (int i = 0; i < count; ++i) destination[i] = scope[size_t((w - count + i + int(scope.size())) % int(scope.size()))].load(std::memory_order_relaxed);
    }
    static constexpr int scopeLength = 2048;
private:
    struct Envelope {
        enum Stage { idle, attack, decay, sustain, release };
        Stage stage{idle};double value{};
        void gateOn() noexcept { stage = attack; }
        void noteOff() noexcept { if (stage != idle) stage = Stage::release; }
        double next(const SynthEnvelopeSettings& e, double rate) noexcept {
            switch (stage) {
                case attack: value += 1.0 / std::max(1.0, e.attack * rate);if (value >= 1.0) { value = 1.0;stage = decay; }break;
                case decay: value = e.sustain + (value - e.sustain) * std::exp(-6.907755 / (e.decay * rate));if (std::abs(value - e.sustain) < 1e-5) { value = e.sustain;stage = sustain; }break;
                case sustain: value = e.sustain;break;
                case release: value *= std::exp(-6.907755 / (e.release * rate));if (value < 1e-5) { value = 0;stage = idle; }break;
                default: value = 0;break;
            }
            return value;
        }
    };
    struct Voice {
        bool active{}, gate{}, fromGuitar{};int note{-1};double hz{}, targetHz{}, velocity{1}, noteValue{};std::uint64_t age{};
        // Per oscillator and unison voice: the wave phase (0-1) or, in Sample
        // mode, the read position in sample frames.
        std::array<std::array<double, synthMaxUnison>, synthOscCount> phase{};double subPhase{}, impactPhase{};
        std::array<double, synthOscCount> drift{};
        double struck{};   // samples since the last attack (Mimir Impact)
        Envelope amp, env2;
        std::array<std::array<std::array<double, 2>, 2>, 2> svf{};   // channel x stage x state
        double noiseLow{};
    };
    struct Pending { bool on{};int note{};float velocity{}; };
    struct Pointers {
        std::atomic<float>* on{};std::atomic<float>* type{};std::atomic<float>* character{};std::atomic<float>* source{};std::atomic<float>* insert{};
        std::atomic<float>* level{};std::atomic<float>* octave{};
        std::atomic<float>* glide{};std::atomic<float>* sensitivity{};std::atomic<float>* voices{};std::atomic<float>* width{};std::atomic<float>* fm{};
        struct Osc { std::atomic<float>* on{};std::atomic<float>* mode{};std::atomic<float>* wave{};std::atomic<float>* octave{};std::atomic<float>* semi{};std::atomic<float>* fine{};
                     std::atomic<float>* unison{};std::atomic<float>* detune{};std::atomic<float>* level{};std::atomic<float>* start{};std::atomic<float>* loop{};std::atomic<float>* root{}; };
        std::array<Osc, synthOscCount> osc{};
        std::atomic<float>* subOn{};std::atomic<float>* subOctave{};std::atomic<float>* subShape{};std::atomic<float>* subLevel{};
        std::atomic<float>* noiseOn{};std::atomic<float>* noiseColor{};std::atomic<float>* noiseLevel{};
        std::atomic<float>* filterOn{};std::atomic<float>* filterType{};std::atomic<float>* cutoff{};std::atomic<float>* resonance{};
        std::atomic<float>* drive{};std::atomic<float>* keyTrack{};std::atomic<float>* filterEnv{};
        struct Env { std::atomic<float>* attack{};std::atomic<float>* decay{};std::atomic<float>* sustain{};std::atomic<float>* release{}; };
        std::array<Env, 2> env{};
        struct Lfo { std::atomic<float>* shape{};std::atomic<float>* rate{};std::atomic<float>* sync{}; };
        std::array<Lfo, 2> lfo{};
        struct Slot { std::atomic<float>* source{};std::atomic<float>* target{};std::atomic<float>* amount{}; };
        std::array<Slot, synthModSlots> mod{};
    } p;
    const SynthSample* samples_(int o) const noexcept { return samples[size_t(o)].get(); }
    static double noteHz(double note) noexcept { return 440.0 * std::pow(2.0, (note - 69.0) / 12.0); }
    float random() noexcept { seed = seed * 1664525u + 1013904223u;return float(seed >> 8) / 8388608.f - 1.f; }
    // Sample oscillators start every attack at their start position.
    void restartSamples(Voice& v, const Settings& s) const noexcept {
        for (int o = 0; o < synthOscCount; ++o) {
            const auto* sample = playing[size_t(o)];
            if (s.osc[size_t(o)].mode != 1 || sample == nullptr) continue;
            const double at = std::floor(double(s.osc[size_t(o)].start) * std::max(0, sample->length - 1));
            for (auto& ph : v.phase[size_t(o)]) ph = at;
        }
    }
    void startVoice(Voice& v, double hz, double velocity, int note, const Settings& s, bool legato) noexcept {
        const double target = hz * std::pow(2.0, double(s.octave));
        const bool glide = s.glideMs > 0 && v.active && v.hz > 0;
        v.targetHz = target;if (!glide) v.hz = target;
        v.note = note;v.fromGuitar = note < 0;v.noteValue = note >= 0 ? (note - 60) / 24.0 : (12.0 * std::log2(std::max(hz, 1.0) / 261.63)) / 24.0;
        v.velocity = velocity;v.gate = true;v.age = ++clock;
        if (!v.active) { v.svf = {};for (auto& o : v.phase) for (auto& ph : o) ph = .5 * (random() + 1.f);v.drift = {};v.impactPhase = 0; }
        if (!legato || !v.active) { v.amp.gateOn();v.env2.gateOn();v.struck = 0;restartSamples(v, s); }
        v.active = true;
    }
    void applyNotes(const Settings& s) noexcept {
        if (allOff) { for (auto& v : voices) { v.gate = false;v.amp.noteOff();v.env2.noteOff(); }midiCount = 0;allOff = false; }
        const bool midiAllowed = s.on && s.source != 1;
        for (int k = 0; k < pendingCount; ++k) {
            const auto& e = pending[size_t(k)];
            if (e.on) {
                removeHeld(e.note);
                if (midiCount < int(midiNotes.size())) midiNotes[size_t(midiCount++)] = e.note;
                midiVelocity = e.velocity;
                if (!midiAllowed) continue;
                if (s.voices == 1) startVoice(voices[0], noteHz(e.note + bend), e.velocity, e.note, s, voices[0].gate);
                else {
                    Voice* chosen = nullptr;
                    for (int i = 0; i < s.voices; ++i) if (!voices[size_t(i)].active) { chosen = &voices[size_t(i)];break; }
                    if (chosen == nullptr) { chosen = &voices[0];for (int i = 1; i < s.voices; ++i) if (voices[size_t(i)].age < chosen->age) chosen = &voices[size_t(i)]; }   // steal the oldest
                    if (chosen->active) { chosen->amp = {};chosen->env2 = {};chosen->active = false;chosen->hz = 0; }
                    startVoice(*chosen, noteHz(e.note + bend), e.velocity, e.note, s, false);
                }
            } else {
                removeHeld(e.note);
                if (s.voices == 1) {
                    if (voices[0].note == e.note) {
                        if (midiCount > 0 && midiAllowed) startVoice(voices[0], noteHz(midiNotes[size_t(midiCount - 1)] + bend), midiVelocity, midiNotes[size_t(midiCount - 1)], s, true);   // back to the held note
                        else { voices[0].gate = false;voices[0].amp.noteOff();voices[0].env2.noteOff(); }
                    }
                } else for (auto& v : voices) if (v.gate && v.note == e.note) { v.gate = false;v.amp.noteOff();v.env2.noteOff(); }
            }
        }
        pendingCount = 0;
        for (auto& v : voices) if (v.gate && v.note >= 0) v.targetHz = noteHz(v.note + bend) * std::pow(2.0, double(s.octave));
        if (!s.on) for (auto& v : voices) if (v.gate) { v.gate = false;v.amp.noteOff();v.env2.noteOff(); }
    }
    void trackGuitar(const Settings& s, int samples) noexcept {
        const bool midiHeld = s.source != 1 && midiCount > 0;
        const double threshold = std::pow(10.0, s.sensitivityDb / 20.0);
        const bool pitched = s.on && s.source != 2 && !midiHeld && tracker.rms() > threshold && tracker.hz() > 0 && tracker.clarity() > .8;
        guitarLevel = std::min(1.0, tracker.rms() / .25);
        if (pitched) {
            const bool onset = !guitarGate || tracker.rms() > onsetLevel * 2.0;   // a 6 dB rise is a new pick attack
            if (onset) startVoice(guitar, tracker.hz(), guitarLevel, -1, s, false);
            else guitar.targetHz = tracker.hz() * std::pow(2.0, double(s.octave));
            guitarGate = true;
            if (onset) onsetLevel = tracker.rms();
        } else if (guitarGate) { guitarGate = false;guitar.gate = false;guitar.amp.noteOff();guitar.env2.noteOff(); }
        onsetLevel = std::max(tracker.rms(), onsetLevel * std::exp(-double(samples) / (.05 * rate)));
    }
    void advanceLfos(const Settings& s, double bpm) noexcept {
        for (int l = 0; l < 2; ++l) {
            const auto& q = s.lfo[size_t(l)];
            const double beats = synthLfoSyncBeats[size_t(q.sync)];
            const double hz = beats > 0 ? bpm / 60.0 / beats : double(q.rate);
            auto& ph = lfoPhase[size_t(l)];ph += hz / rate;
            if (ph >= 1.0) { ph -= std::floor(ph);lfoHeld[size_t(l)] = random(); }
            lfoValue[size_t(l)] = synthLfoShape(q.shape, ph, lfoHeld[size_t(l)]);
        }
    }
    static double blep(double t, double dt) noexcept {
        if (t < dt) { t /= dt;return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt;return t * t + t + t + 1.0; }
        return 0.0;
    }
    static double bandLimited(int shape, double phase, double dt) noexcept {
        switch (shape) {
            case 0: return std::sin(6.283185307179586 * phase);
            case 1: return 4.0 * std::abs(phase - .5) - 1.0;
            case 2: return 1.0 - 2.0 * phase + blep(phase, dt);
            case 3: { double v = phase < .5 ? 1.0 : -1.0;double shifted = phase + .5;shifted -= std::floor(shifted);return v - blep(phase, dt) + blep(shifted, dt); }
            default: { double v = phase < .125 ? 1.0 : -1.0;double shifted = phase + .875;shifted -= std::floor(shifted);return v - blep(phase, dt) + blep(shifted, dt); }
        }
    }
    static double morph(double position, double phase, double dt) noexcept {
        position = std::clamp(position, 0.0, 4.0);
        const int a = std::min(3, int(position));const double t = position - a;
        if (t <= 0.0) return bandLimited(a, phase, dt);
        return (1.0 - t) * bandLimited(a, phase, dt) + t * bandLimited(a + 1, phase, dt);
    }
    // Four-point Hermite read of one channel. Frames past the end wrap into
    // the loop [loopStart, length) or read silence.
    static double readSample(const std::vector<float>& d, int length, double position, bool loop, int loopStart) noexcept {
        const int base = int(std::floor(position));const double t = position - base;
        const int span = length - loopStart;
        const auto at = [&](int k) -> double {
            if (k < 0) return 0.0;
            if (k >= length) { if (!loop || span < 2) return 0.0;k = loopStart + (k - length) % span; }
            return d[size_t(k)];
        };
        const double y0 = at(base - 1), y1 = at(base), y2 = at(base + 1), y3 = at(base + 2);
        const double c1 = .5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2.0 * y2 - .5 * y3, c3 = .5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
    double filterSample(std::array<std::array<double, 2>, 2>& st, double x, const Settings& s, double cutoff, double resonance) const noexcept {
        if (s.drive > 0.f) { const double g = 1.0 + 9.0 * s.drive;x = std::tanh(g * x) / std::tanh(g); }
        const double g = std::tan(3.141592653589793 * std::clamp(cutoff, 20.0, rate * .45) / rate), k = 2.0 - 1.9 * std::clamp(resonance, 0.0, 1.0);
        const auto tick = [&](std::array<double, 2>& f, double in) {
            const double a1 = 1. / (1. + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const double v3 = in - f[1], v1 = a1 * f[0] + a2 * v3, v2 = f[1] + a2 * f[0] + a3 * v3;
            f[0] = 2. * v1 - f[0];f[1] = 2. * v2 - f[1];
            return std::array<double, 3>{v2, v1, in - k * v1 - v2};
        };
        switch (s.filterType) {
            case 0: return tick(st[0], x)[0];
            case 1: return tick(st[1], tick(st[0], x)[0])[0];
            case 2: return tick(st[0], x)[2];
            case 3: return tick(st[1], tick(st[0], x)[2])[2];
            case 4: return k * tick(st[0], x)[1];
            default: { const auto o = tick(st[0], x);return o[0] + o[2]; }
        }
    }
    void addVoice(Voice& v, const Settings& s, std::array<double, 2>& out) noexcept {
        const double glide = s.glideMs > 0 ? 1.0 - std::exp(-1.0 / (s.glideMs * .001 * rate)) : 1.0;
        v.hz += (v.targetHz - v.hz) * glide;
        const double ampEnv = v.amp.next(s.env[0], rate), env2 = v.env2.next(s.env[1], rate);
        if (v.amp.stage == Envelope::idle) { v.active = false;v.gate = false;return; }
        // Modulation matrix: sources per voice, summed per target.
        std::array<double, synthModTargetCount> m{};
        for (const auto& slot : s.mod) {
            if (slot.source == modOff || slot.target == targetOff || slot.amount == 0.f) continue;
            double src = 0;
            switch (slot.source) {
                case modEnv2: src = env2;break;case modLfo1: src = lfoValue[0];break;case modLfo2: src = lfoValue[1];break;
                case modVelocity: src = v.velocity;break;case modGuitarLevel: src = guitarLevel;break;case modNote: src = v.noteValue;break;
                case modWheel: src = wheel;break;default: break;
            }
            m[size_t(slot.target)] += slot.amount * src * synthModDepth[size_t(slot.target)];
        }
        // Engine type. Character 0 is the plain voice for every type.
        const double character = std::clamp(double(s.character) + m[targetCharacter], 0.0, 1.0);
        const bool analog = s.type == synthAnalog, dynamic = s.type == synthDynamic, impact = s.type == synthImpact;
        // Seidr Dynamic: the playing level (guitar level, or MIDI velocity)
        // opens the filter, brightens the waves and sets the level.
        const double playing_ = v.fromGuitar ? guitarLevel : v.velocity;
        const double dynamicOctaves = dynamic ? character * 3.0 * (playing_ - .5) : 0.0;
        const double dynamicWave = dynamic ? character * 1.5 * playing_ : 0.0;
        const double dynamicGain = dynamic ? 1.0 - character * .75 * (1.0 - playing_) : 1.0;
        // Mimir Impact: a struck FM index (60 ms) and a click (4 ms) at each attack.
        const double strike = impact ? std::exp(-v.struck / (.06 * rate)) : 0.0, click = impact ? std::exp(-v.struck / (.004 * rate)) : 0.0;
        v.struck += 1.0;
        std::array<double, 2> sum{};
        double oscB = 0;
        for (const int o : {1, 0, 2}) {   // B first: it is A's FM modulator
            const auto& q = s.osc[size_t(o)];
            const auto* sample = q.mode == 1 ? playing[size_t(o)] : nullptr;
            const double level = std::clamp(double(q.level) + m[size_t(targetLevelA + o)], 0.0, 1.0);
            const bool needed = q.on && (level > 0 || (o == 1 && (s.fm > 0 || m[targetFM] != 0 || impact)));
            if (!needed || (q.mode == 1 && sample == nullptr)) continue;
            double cents = 0;
            if (analog && character > 0) {   // Dvergr Analog: slow pitch drift, about 5 cents RMS at 100 %
                v.drift[size_t(o)] += driftCoefficient * (random() - v.drift[size_t(o)]);
                cents = character * 5.0 * v.drift[size_t(o)] * driftScale;
            }
            const double semitones = q.octave * 12.0 + q.semi + (q.fine + cents) * .01 + m[size_t(targetPitchA + o)];
            const double base = v.hz * std::pow(2.0, semitones / 12.0);
            const int n = q.unison;
            const double spread = std::clamp(double(q.detune) + m[targetDetune], 0.0, 1.0) * 50.0;   // +-50 cents at 100 %
            double mono = 0;
            if (sample != nullptr) {
                // Sample: the root note plays at the sample's own speed.
                const double speed = sample->sampleRate / rate / noteHz(q.root);
                const int loopStart = int(std::floor(double(q.start) * std::max(0, sample->length - 1)));
                for (int u = 0; u < n; ++u) {
                    const double position = n > 1 ? 2.0 * u / (n - 1) - 1.0 : 0.0;
                    const double hz = base * std::pow(2.0, spread * position / 1200.0);
                    auto& at = v.phase[size_t(o)][size_t(u)];
                    if (at >= sample->length && !q.loop) continue;   // a one-shot that has ended
                    const double l = readSample(sample->data[0], sample->length, at, q.loop, loopStart) / std::sqrt(double(n));
                    const double r = sample->stereo() ? readSample(sample->data[1], sample->length, at, q.loop, loopStart) / std::sqrt(double(n)) : l;
                    at += std::min(16.0, hz * speed);
                    if (at >= sample->length) { if (q.loop && sample->length - loopStart > 1) at = loopStart + std::fmod(at - loopStart, double(sample->length - loopStart));else at = sample->length; }
                    mono += .5 * (l + r);
                    const double pan = .5 + .5 * s.width * position;
                    sum[0] += level * l * std::sqrt(2.0 * (1.0 - pan));
                    sum[1] += level * r * std::sqrt(2.0 * pan);
                }
            } else {
                const double wave = std::clamp(double(q.wave) + m[size_t(targetWaveA + o)] + dynamicWave, 0.0, 4.0);
                const double fmIndex = o == 0 ? std::max(0.0, s.fm * 8.0 + m[targetFM] + character * 6.0 * strike) : 0.0;
                double modulator = oscB;
                if (o == 0 && impact && !(s.osc[1].on && s.osc[1].mode == 0)) {   // no oscillator B: an internal sine at twice A
                    modulator = std::sin(6.283185307179586 * v.impactPhase);
                    v.impactPhase += 2.0 * base / rate;v.impactPhase -= std::floor(v.impactPhase);
                }
                for (int u = 0; u < n; ++u) {
                    const double position = n > 1 ? 2.0 * u / (n - 1) - 1.0 : 0.0;
                    const double hz = base * std::pow(2.0, spread * position / 1200.0), dt = std::min(.5, hz / rate);
                    auto& ph = v.phase[size_t(o)][size_t(u)];
                    double readPhase = ph;
                    if (fmIndex > 0) { readPhase += fmIndex * modulator / 6.283185307179586;readPhase -= std::floor(readPhase); }
                    const double y = morph(wave, readPhase, dt) / std::sqrt(double(n));
                    ph += hz / rate;ph -= std::floor(ph);
                    mono += y;
                    const double pan = .5 + .5 * s.width * position;   // equal-power spread across the width
                    sum[0] += level * y * std::sqrt(2.0 * (1.0 - pan));   // unity in each channel at the centre
                    sum[1] += level * y * std::sqrt(2.0 * pan);
                }
            }
            if (o == 1) oscB = mono;
        }
        if (s.subOn) {
            const double level = std::clamp(double(s.subLevel) + m[targetSubLevel], 0.0, 1.0);
            const double hz = v.hz * (s.subOctave == 0 ? .5 : .25), dt = std::min(.5, hz / rate);
            const double y = level * (s.subShape == 0 ? std::sin(6.283185307179586 * v.subPhase) : bandLimited(3, v.subPhase, dt));
            v.subPhase += hz / rate;v.subPhase -= std::floor(v.subPhase);
            sum[0] += y;sum[1] += y;
        }
        if (s.noiseOn || (impact && character > 0)) {
            const double level = s.noiseOn ? std::clamp(double(s.noiseLevel) + m[targetNoiseLevel], 0.0, 1.0) : 0.0;
            const double cutoff = 500.0 * std::pow(40.0, double(s.noiseColor)), c = 1.0 - std::exp(-6.283185307179586 * std::min(cutoff, rate * .45) / rate);
            const float white = random();
            v.noiseLow += c * (white - v.noiseLow);
            const double y = level * v.noiseLow + character * .6 * click * white;
            sum[0] += y;sum[1] += y;
        }
        if (analog && character > 0) {   // Dvergr Analog: gentle saturation into the filter, unity for small signals
            const double g = .5 * character;   // at 100 %: a full-scale peak loses 0.4 dB, a peak of 2 loses 2.4 dB
            for (auto& x : sum) x = std::tanh(g * x) / g;
        }
        if (s.filterOn) {
            const double tracking = s.keyTrack * std::log2(std::max(v.hz, 1.0) / 261.63);
            const double cutoff = s.cutoff * std::pow(2.0, tracking + s.filterEnv * 5.0 * env2 + m[targetCutoff] + dynamicOctaves);
            const double resonance = s.resonance + m[targetResonance];
            for (int c = 0; c < 2; ++c) sum[size_t(c)] = filterSample(v.svf[size_t(c)], sum[size_t(c)], s, cutoff, resonance);
        }
        const double amp = ampEnv * dynamicGain * std::clamp(1.0 + m[targetAmp], 0.0, 2.0);
        out[0] += sum[0] * amp;out[1] += sum[1] * amp;
    }
    void removeHeld(int note) noexcept {
        int k = 0;for (int i = 0; i < midiCount; ++i) if (midiNotes[size_t(i)] != note) midiNotes[size_t(k++)] = midiNotes[size_t(i)];
        midiCount = k;
    }
    double rate{48000}, driftCoefficient{1e-4}, driftScale{1.0};
    PitchTracker tracker;
    std::array<Voice, synthMaxVoices> voices{};Voice guitar{};
    std::array<int, 16> midiNotes{};int midiCount{};double bend{}, wheel{};float midiVelocity{};bool midiOnset{}, allOff{};
    std::array<Pending, 64> pending{};int pendingCount{};
    std::array<double, 2> lfoPhase{}, lfoHeld{}, lfoValue{};
    std::uint32_t seed{0x2545f491u};std::uint64_t clock{};
    bool guitarGate{}, active{};double guitarLevel{}, onsetLevel{};
    std::array<std::vector<float>, 2> rendered;int renderedSamples{};
    std::array<float, synthInsertCount> gain{};
    bool toMain{true}, toBus{};float busGain{};
    std::array<std::atomic<float>, scopeLength> scope{};std::atomic<int> scopeWrite{0};
    juce::SpinLock sampleLock;
    std::array<std::unique_ptr<SynthSample>, synthOscCount> samples;
    std::array<const SynthSample*, synthOscCount> playing{};   // valid during render() only
};

} // namespace spectralforge
