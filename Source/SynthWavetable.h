#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

// SYNTH wavetable oscillators (docs/SYNTH_WAVETABLE.md). Each oscillator in
// Wave mode plays either the five classic shapes (the released behaviour) or
// one of the built-in wavetables: sixteen single-cycle frames that the
// oscillator's wave position moves through. A warp reshapes the read phase.
//
// Every frame is stored band-limited at 21 levels, half an octave apart
// (1024, 724, 512, ... 1 harmonics), so a note reads only harmonics below
// half the sample rate and keeps those up to at least half of that. The
// tables are computed once, when the first synth is created (about 25 MB);
// nothing is read from disk.
namespace spectralforge {
inline constexpr int synthWavetableParameterVersion = 38;
inline constexpr int synthTableFrames = 16, synthTableOrder = 11, synthTableSize = 1 << synthTableOrder, synthTableLevels = 21;
// Harmonics kept at a level: 1024 at level 0, then half an octave fewer per level, down to 1.
inline int synthTableHarmonics(int level) noexcept { return std::max(1, int(std::floor(1024.0 * std::pow(2.0, -.5 * level) + 1.0e-9))); }
inline constexpr int synthWavetableParametersPerOsc = 3;
inline constexpr int synthTableImportParameterVersion = 42, synthTableImportParameterCount = 3;   // one switch per oscillator

struct SynthTableInfo { const char* name; const char* character; };
// Index 0 is "no table": the oscillator plays the classic morphing shapes.
// Names are short: they are shown in a narrow menu in each oscillator's header.
inline constexpr std::array<SynthTableInfo, 10> synthTables{{
    {"Classic", "Sine, triangle, saw, square and pulse (the released oscillator)"},
    {"Harmonics", "A saw that opens from its fundamental to 64 harmonics"},
    {"Pulse", "A pulse from a square down to a 3 % needle"},
    {"Vowels", "Formant spectra: A, E, I, O, U"},
    {"Metal FM", "Two-operator FM at a 3:1 ratio, index 0 to 8"},
    {"Saw Fold", "A saw driven through a wavefolder"},
    {"Sync Saw", "A hard-synced saw, slave ratio 1 to 8"},
    {"Drawbars", "Organ registrations from a flute to full drawbars"},
    {"Bit Steps", "A sine reduced from 64 steps to 3"},
    {"Growl", "Odd harmonics under a moving low formant"}}};
inline constexpr int synthTableCount = int(synthTables.size());
inline juce::StringArray synthTableNames() { juce::StringArray names; for (const auto& t : synthTables) names.add(t.name); return names; }

enum SynthWarp { synthWarpOff, synthWarpBendUp, synthWarpBendDown, synthWarpSqueeze, synthWarpSync, synthWarpMirror, synthWarpQuantize, synthWarpCount };
inline juce::StringArray synthWarpNames() { return {"Off", "Bend +", "Bend -", "Squeeze", "Sync", "Mirror", "Quantize"}; }

// The read phase (0-1) of a warp at `amount` (0-1). Off, or amount 0, is the identity.
inline double synthWarpPhase(int warp, double amount, double phase) noexcept {
    if (amount <= 0.0) return phase;
    switch (warp) {
        case synthWarpBendUp: return std::pow(phase, 1.0 + 3.0 * amount);
        case synthWarpBendDown: return 1.0 - std::pow(1.0 - phase, 1.0 + 3.0 * amount);
        case synthWarpSqueeze: { const double w = .5 - .45 * amount; return phase < w ? .5 * phase / w : .5 + .5 * (phase - w) / (1.0 - w); }
        case synthWarpSync: { const double x = phase * (1.0 + 7.0 * amount); return x - std::floor(x); }
        case synthWarpMirror: { const double folded = 1.0 - std::abs(2.0 * phase - 1.0); return phase + amount * (folded - phase); }
        case synthWarpQuantize: { const double steps = std::round(64.0 * std::pow(4.0 / 64.0, amount)); return std::floor(phase * steps) / steps; }
        default: return phase;
    }
}
// How much faster than the note the warped phase can run: the reader skips
// that many more octaves of harmonics.
inline double synthWarpSlope(int warp, double amount) noexcept {
    if (amount <= 0.0) return 1.0;
    switch (warp) {
        case synthWarpBendUp: case synthWarpBendDown: return 1.0 + 3.0 * amount;
        case synthWarpSqueeze: return .5 / (.5 - .45 * amount);
        case synthWarpSync: return 1.0 + 7.0 * amount;
        case synthWarpMirror: return 1.0 + amount;
        default: return 1.0;
    }
}

// Reads a table stored as `frames` frames of synthTableLevels levels of
// synthTableSize samples, starting at `base`. `position` 0-1 across the
// frames, `phase` 0-1, `dt` the phase step per sample (frequency / rate)
// times the warp slope. Neighbouring frames and levels are blended.
inline double synthTableRead(const float* base, int frames, double position, double phase, double dt) noexcept {
    const double p = std::clamp(position, 0.0, 1.0) * (frames - 1);
    const int f0 = std::max(0, std::min(frames - 2, int(p))), f1 = std::min(frames - 1, f0 + 1);const double ft = p - f0;
    // Level L keeps 1024 / 2^(L/2) harmonics. x is the level whose top
    // harmonic sits at half the sample rate; L = ceil(x) is the first
    // safe level. Reading between L and L + 1 keeps the tone continuous
    // while the pitch moves.
    const double x = 2.0 * std::log2(std::max(1.0e-9, 2048.0 * dt));
    const int level = std::clamp(int(std::ceil(x)), 0, synthTableLevels - 1), next = std::min(synthTableLevels - 1, level + 1);
    const double lt = x <= 0.0 ? 0.0 : std::clamp(x - (level - 1), 0.0, 1.0);
    const double at = phase * synthTableSize;const int i0 = int(at) & (synthTableSize - 1), i1 = (i0 + 1) & (synthTableSize - 1);const double it = at - std::floor(at);
    const auto sample = [&](int f, int l) { const float* d = base + (size_t(f) * synthTableLevels + size_t(l)) * synthTableSize; return double(d[i0]) + it * (double(d[i1]) - double(d[i0])); };
    const auto atLevel = [&](int l) { const double a = sample(f0, l); return a + ft * (sample(f1, l) - a); };
    const double a = atLevel(level);
    return lt > 0.0 && next != level ? a + lt * (atLevel(next) - a) : a;
}

// An imported wavetable (docs/SYNTH_WAVETABLE_IMPORT.md): up to
// synthUserTableMaxFrames single-cycle frames, stored like a built-in table.
// Built from `cycles` (frames x synthTableSize samples, one cycle each), which
// is what the project keeps. Immutable once built; the engine reads it, the
// message thread swaps it.
inline constexpr int synthUserTableMaxFrames = 64;
struct SynthUserTable {
    juce::String name;
    int frames{};
    int sourceFrames{}, sourceFrameSize{};   // what the file held (for the page)
    std::vector<float> cycles;               // frames x synthTableSize, as imported
    std::vector<float> data;                 // frames x synthTableLevels x synthTableSize, band-limited and normalised
    double read(double position, double phase, double dt) const noexcept { return synthTableRead(data.data(), frames, position, phase, dt); }
    const float* frame(int index, int level) const noexcept { return data.data() + (size_t(index) * synthTableLevels + size_t(level)) * synthTableSize; }

    // Null when `source` is not frames x synthTableSize finite samples, or is silent.
    static std::unique_ptr<SynthUserTable> build(std::vector<float> source, int frameCount, const juce::String& tableName) {
        if (frameCount < 1 || frameCount > synthUserTableMaxFrames || source.size() != size_t(frameCount) * synthTableSize) return nullptr;
        for (const float x : source) if (!std::isfinite(x)) return nullptr;
        auto table = std::make_unique<SynthUserTable>();
        table->name = tableName;table->frames = frameCount;table->sourceFrames = frameCount;table->sourceFrameSize = synthTableSize;
        table->data.assign(size_t(frameCount) * synthTableLevels * synthTableSize, 0.f);
        juce::dsp::FFT fft(synthTableOrder);
        std::vector<float> spectrum(size_t(2 * synthTableSize)), work(size_t(2 * synthTableSize));
        float peak = 0;
        for (int index = 0; index < frameCount; ++index) {
            std::fill(spectrum.begin(), spectrum.end(), 0.f);
            std::copy_n(source.data() + size_t(index) * synthTableSize, synthTableSize, spectrum.data());
            fft.performRealOnlyForwardTransform(spectrum.data());
            spectrum[0] = spectrum[1] = 0.f;   // no DC
            for (int level = 0; level < synthTableLevels; ++level) {
                const int harmonics = std::min(synthTableSize / 2 - 1, synthTableHarmonics(level));
                std::fill(work.begin(), work.end(), 0.f);
                for (int h = 1; h <= harmonics; ++h) {
                    work[size_t(2 * h)] = spectrum[size_t(2 * h)];work[size_t(2 * h + 1)] = spectrum[size_t(2 * h + 1)];
                    const int mirror = synthTableSize - h;   // the conjugate half
                    work[size_t(2 * mirror)] = spectrum[size_t(2 * h)];work[size_t(2 * mirror + 1)] = -spectrum[size_t(2 * h + 1)];
                }
                fft.performRealOnlyInverseTransform(work.data());
                if (level == 0) for (int n = 0; n < synthTableSize; ++n) peak = std::max(peak, std::abs(work[size_t(n)]));
                std::copy_n(work.data(), synthTableSize, table->data.data() + (size_t(index) * synthTableLevels + size_t(level)) * synthTableSize);
            }
        }
        if (!(peak > 1.0e-6f)) return nullptr;
        // One gain for the whole table: the loudest frame peaks at 1 and the others keep their level against it.
        const float gain = 1.f / peak;
        for (float& x : table->data) x *= gain;
        table->cycles = std::move(source);
        return table;
    }
};

class SynthWavetables {
public:
    // The shared set, built on first use (call it off the audio thread first).
    static const SynthWavetables& get() { static const SynthWavetables tables; return tables; }

    // One frame of a table at a level, synthTableSize samples (table 1 .. synthTableCount - 1).
    const float* frame(int table, int index, int level) const noexcept {
        return data.data() + ((size_t(table - 1) * synthTableFrames + size_t(index)) * synthTableLevels + size_t(level)) * synthTableSize;
    }
    // `position` 0-1 across the frames, `phase` 0-1, `dt` the phase step per
    // sample (frequency / rate) times the warp slope.
    double read(int table, double position, double phase, double dt) const noexcept {
        if (table <= 0 || table >= synthTableCount) return 0.0;
        return synthTableRead(frame(table, 0, 0), synthTableFrames, position, phase, dt);
    }
    // The raw cycle of a frame for drawing (full bandwidth).
    void draw(int table, double position, std::vector<float>& out, int points) const {
        out.assign(size_t(points), 0.f);
        for (int n = 0; n < points; ++n) out[size_t(n)] = float(read(table, position, double(n) / points, 1.0 / 4096.0));
    }

private:
    std::vector<float> data;

    static double cycle(int table, double t, double phase) {
        constexpr double twoPi = 6.283185307179586;
        switch (table) {
            case 1: { const int harmonics = 1 + int(std::round(63.0 * t * t));double y = 0;for (int h = 1; h <= harmonics; ++h) y += std::sin(twoPi * h * phase) / h;return y; }
            case 2: { const double width = .5 - .47 * t;return phase < width ? 1.0 : -1.0; }
            case 3: {   // vowels: three formants over a 110 Hz voice
                static const std::array<std::array<double, 3>, 5> formants{{{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}}};
                const double v = t * 4.0;const int a = std::min(3, int(v));const double mix = v - a;double y = 0;
                for (int h = 1; h <= 60; ++h) {
                    const double hz = 110.0 * h;double gain = 0;
                    for (int k = 0; k < 3; ++k) {
                        const double centre = formants[size_t(a)][size_t(k)] + mix * (formants[size_t(a + 1)][size_t(k)] - formants[size_t(a)][size_t(k)]);
                        const double width = 90.0 + 40.0 * k, d = (hz - centre) / width;
                        gain += (k == 0 ? 1.0 : k == 1 ? .6 : .35) / (1.0 + d * d);
                    }
                    y += gain * std::sin(twoPi * h * phase) / std::sqrt(double(h));
                }
                return y;
            }
            case 4: return std::sin(twoPi * phase + 8.0 * t * std::sin(twoPi * 3.0 * phase));
            case 5: { const double x = (1.0 + 5.0 * t) * (2.0 * phase - 1.0);return std::sin(1.5707963267948966 * x); }
            case 6: { const double ratio = 1.0 + 7.0 * t, x = phase * ratio;const double saw = 1.0 - 2.0 * (x - std::floor(x));return saw * (1.0 - phase); }
            case 7: {   // drawbars 16', 5 1/3', 8', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1' come in one after another
                static const std::array<double, 9> ratios{.5, 1.5, 1, 2, 3, 4, 5, 6, 8};double y = 0;
                for (int k = 0; k < 9; ++k) { const double in = std::clamp(t * 9.0 - k + 1.0, 0.0, 1.0);y += in * std::sin(twoPi * 2.0 * ratios[size_t(k)] * phase); }
                return y;
            }
            case 8: { const double steps = std::round(64.0 * std::pow(3.0 / 64.0, t));return std::round(std::sin(twoPi * phase) * steps * .5) / (steps * .5); }
            default: {   // doom growl: odd harmonics, one wide formant moving from 180 Hz to 1.4 kHz over a 55 Hz voice
                const double centre = 180.0 * std::pow(1400.0 / 180.0, t);double y = 0;
                for (int h = 1; h <= 96; h += 2) { const double d = (55.0 * h - centre) / (centre * .45);y += (1.0 / (1.0 + d * d) + .08) * std::sin(twoPi * h * phase + .7 * h) / std::sqrt(double(h)); }
                return y;
            }
        }
    }

    SynthWavetables() {
        data.assign(size_t(synthTableCount - 1) * synthTableFrames * synthTableLevels * synthTableSize, 0.f);
        juce::dsp::FFT fft(synthTableOrder);
        std::vector<float> spectrum(size_t(2 * synthTableSize)), work(size_t(2 * synthTableSize));
        for (int table = 1; table < synthTableCount; ++table)
            for (int index = 0; index < synthTableFrames; ++index) {
                const double t = double(index) / (synthTableFrames - 1);
                std::fill(spectrum.begin(), spectrum.end(), 0.f);
                for (int n = 0; n < synthTableSize; ++n) spectrum[size_t(n)] = float(cycle(table, t, (n + .5) / synthTableSize));
                fft.performRealOnlyForwardTransform(spectrum.data());
                spectrum[0] = spectrum[1] = 0.f;   // no DC
                float peak = 0;
                for (int level = 0; level < synthTableLevels; ++level) {
                    const int harmonics = std::min(synthTableSize / 2 - 1, synthTableHarmonics(level));
                    std::fill(work.begin(), work.end(), 0.f);
                    for (int h = 1; h <= harmonics; ++h) {
                        work[size_t(2 * h)] = spectrum[size_t(2 * h)];work[size_t(2 * h + 1)] = spectrum[size_t(2 * h + 1)];
                        const int mirror = synthTableSize - h;   // the conjugate half
                        work[size_t(2 * mirror)] = spectrum[size_t(2 * h)];work[size_t(2 * mirror + 1)] = -spectrum[size_t(2 * h + 1)];
                    }
                    fft.performRealOnlyInverseTransform(work.data());
                    if (level == 0) for (int n = 0; n < synthTableSize; ++n) peak = std::max(peak, std::abs(work[size_t(n)]));
                    const float gain = peak > 1.0e-9f ? 1.f / peak : 0.f;   // every level shares the full-band frame's gain
                    auto* out = data.data() + ((size_t(table - 1) * synthTableFrames + size_t(index)) * synthTableLevels + size_t(level)) * synthTableSize;
                    for (int n = 0; n < synthTableSize; ++n) out[n] = work[size_t(n)] * gain;
                }
            }
    }
};

inline juce::String synthWavetableID(int osc, const char* field) { return juce::String("synthOsc") + "ABC"[osc] + field; }
// Appended at version hint 38, after every released parameter: per
// oscillator the table, the warp and the warp amount. Defaults (Classic
// shapes, Off, 0 %) are the released oscillator.
inline void appendSynthWavetableParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, int oscillators) {
    constexpr int v = synthWavetableParameterVersion;
    for (int osc = 0; osc < oscillators; ++osc) {
        const juce::String n = juce::String("Synth osc ") + "ABC"[osc] + " ";
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{synthWavetableID(osc, "Table"), v}, n + "wavetable", synthTableNames(), 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{synthWavetableID(osc, "Warp"), v}, n + "warp", synthWarpNames(), 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{synthWavetableID(osc, "WarpAmount"), v}, n + "warp amount", juce::NormalisableRange<float>{0.f, 100.f, 1.f}, 0.f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float value, int) { return juce::String(juce::roundToInt(value)) + " %"; })));
    }
}
// Appended at version hint 42, after the effect rack: per oscillator, whether
// it plays the table imported into it (docs/SYNTH_WAVETABLE_IMPORT.md). Off
// is the oscillator as it was.
inline void appendSynthTableImportParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, int oscillators) {
    for (int osc = 0; osc < oscillators; ++osc)
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{synthWavetableID(osc, "Imported"), synthTableImportParameterVersion},
            juce::String("Synth osc ") + "ABC"[osc] + " imported wavetable", false));
}
} // namespace spectralforge
