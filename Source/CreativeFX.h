#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "PostInsert.h"

namespace spectralforge {

// 1.9 Creative (docs/CREATIVE_1_9.md): one POST slot after DELAY and before
// REVERB holding nine creative processors. Each model gives the shared A / B /
// C controls its own meaning; Mix blends the processed signal into the dry
// one. Off (the default) leaves the signal sample-exact and costs nothing.
// Appended at version hint 26.
inline constexpr int creativeParameterVersion = 26;
inline constexpr int creativeModelCount = 9;
inline constexpr int creativeParameterCount = 8;
struct CreativeModel { const char* name; const char* reference; const char* character; std::array<const char*, 3> controls; juce::uint32 colour; };
// Reference fields are internal design provenance and are never shown.
inline constexpr std::array<CreativeModel, creativeModelCount> creativeModels{{
    {"Gate Slicer", "Rhythmic slicer", "Tempo gate / depth and duty", {"RATE", "DEPTH", "DUTY"}, 0xff8a3f4f},
    {"Step Harmonizer", "Arpeggiating harmonizer", "Pitch steps through an interval pattern", {"RATE", "PATTERN", "GLIDE"}, 0xff4f5f9a},
    {"Time Bend", "Time-warp varispeed", "Swept pitch bend / tape-stop ramp", {"RATE", "BEND", "SHAPE"}, 0xff7a6a3a},
    {"Zap Sweep", "Laser filter sweep", "Onset-triggered resonant down-sweep", {"SWEEP", "TRIGGER", "RESONANCE"}, 0xff3a8a7a},
    {"Freq Shift", "Frequency shifter", "Single-sideband shift / feedback and spread", {"SHIFT", "FEEDBACK", "SPREAD"}, 0xff6a3a8a},
    {"Freeze Hold", "Dynamic freeze", "Onset-captured sustain / decaying hold", {"HOLD", "TRIGGER", "GRAIN"}, 0xff3a6a9a},
    {"Glitch Stutter", "Glitch repeater", "Beat repeats / reversed slices", {"RATE", "CHANCE", "REVERSE"}, 0xff9a5a2a},
    {"Bit Crush", "Bit crusher", "Word-length and sample-rate reduction", {"BITS", "RATE", "TONE"}, 0xff5a7a3a},
    {"Grain Cloud", "Granular texture", "Scattered grains / pitch spread", {"DENSITY", "SCATTER", "GRAIN"}, 0xff7a4a7a}}};
inline juce::StringArray creativeModelNames() { juce::StringArray names; for (const auto& m : creativeModels) names.add(m.name); return names; }
inline juce::StringArray creativeSyncNames() { return {"Free", "1/2", "1/4", "1/8", "1/16", "1/8 T", "1/16 T"}; }
// Quarter-note beats per step for each synced choice (index 0 is free).
inline constexpr std::array<double, 7> creativeSyncBeats{0., 2., 1., .5, .25, 1. / 3., 1. / 6.};

inline void appendCreativeParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    constexpr int v = creativeParameterVersion;
    const auto percent = [&](const char* id, const char* name, float initial) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, v}, name, juce::NormalisableRange<float>{0.f, 100.f, .01f}, initial,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float value, int) { return juce::String(juce::roundToInt(value)) + " %"; })));
    };
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"creativeOn", v}, "Creative", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"creativeModel", v}, "Creative model", creativeModelNames(), 0));
    percent("creativeA", "Creative A", 50.f);
    percent("creativeB", "Creative B", 50.f);
    percent("creativeC", "Creative C", 50.f);
    percent("creativeMix", "Creative mix", 50.f);
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"creativeSync", v}, "Creative sync", creativeSyncNames(), 3));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"creativeOutput", v}, "Creative output", juce::NormalisableRange<float>{-24.f, 12.f, .01f}, 0.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float value, int) { return juce::String(value, 1) + " dB"; })));
}

struct CreativeSettings {
    bool on{false};
    int model{0}, sync{3};
    float a{.5f}, b{.5f}, c{.5f}, mix{.5f}, outputDb{0.f};
};

namespace creative_detail {
constexpr double pi = 3.14159265358979323846;
inline double expMap(double lo, double hi, double t) { return lo * std::pow(hi / lo, std::clamp(t, 0.0, 1.0)); }
// Deterministic generator: the same input renders the same output.
struct Random {
    std::uint32_t state{0x9e3779b9u};
    float next() noexcept { state = state * 1664525u + 1013904223u; return float(state >> 8) / 16777216.f; }
};
// Circular stereo history with fractional reads (cubic Hermite).
struct History {
    std::array<std::vector<float>, 2> data;
    int size{1}, write{};
    void prepare(int samples) { size = std::max(4, samples); for (auto& d : data) d.assign(size_t(size), 0.f); write = 0; }
    void clear() noexcept { for (auto& d : data) std::fill(d.begin(), d.end(), 0.f); write = 0; }
    void push(float l, float r) noexcept { data[0][size_t(write)] = l; data[1][size_t(write)] = r; if (++write == size) write = 0; }
    // `ago` samples before the most recent one (0 = newest).
    // Mono sum `ago` whole samples back (no interpolation).
    double mono(int ago) const noexcept {
        int i = write - 1 - std::clamp(ago, 0, size - 1);
        if (i < 0) i += size;
        return double(data[0][size_t(i)]) + double(data[1][size_t(i)]);
    }
    float read(int c, double ago) const noexcept {
        ago = std::clamp(ago, 1.0, double(size - 3));
        double position = double(write - 1) - ago;
        while (position < 0) position += size;
        const int i = int(position);const float t = float(position - i);
        const auto at = [&](int k) { int j = i + k; if (j < 0) j += size; if (j >= size) j -= size; return data[size_t(c)][size_t(j)]; };
        const float y0 = at(-1), y1 = at(0), y2 = at(1), y3 = at(2);
        const float c1 = .5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - .5f * y3, c3 = .5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
};
// Two-head delay-line pitch shifter. Each head's delay moves at (1 - ratio)
// samples per sample for one 50 ms window, and the heads overlap by half a
// window with sin^2 gains (which sum to 1). A head that restarts picks its new
// delay, within 12 ms, by normalised cross-correlation with the playing head,
// so the splice stays in phase instead of cancelling at some frequencies.
struct PitchShifter {
    struct Head { double delay{}, age{}; };
    double window{2400};int search{576}, compare{192};
    std::array<Head, 2> heads{};
    void prepare(double rate) { window = rate * .05;search = int(rate * .012);compare = int(rate * .004);reset(); }
    void reset() noexcept { heads[0] = {double(2 + compare), 0.0};heads[1] = {double(2 + compare), window * .5}; }
    std::array<float, 2> process(const History& h, double ratio) noexcept {
        for (int i = 0; i < 2; ++i) {
            auto& head = heads[size_t(i)];
            if (head.age >= window) { head.age -= window;restart(head, heads[size_t(1 - i)], h, ratio); }
        }
        std::array<float, 2> out{};
        for (auto& head : heads) {
            const float gain = float(std::sin(pi * head.age / window));
            for (int c = 0; c < 2; ++c) out[size_t(c)] += gain * gain * h.read(c, head.delay);
            head.delay = std::max(2.0, head.delay + (1.0 - ratio));head.age += 1.0;
        }
        return out;
    }
private:
    void restart(Head& head, const Head& other, const History& h, double ratio) const noexcept {
        // Start far enough back to stay behind the input for a whole window.
        const double base = 2.0 + compare + std::max(0.0, ratio - 1.0) * window;
        const int reference = int(std::lround(other.delay));
        double best = -2.0;int choice = 0;
        double otherEnergy = 0.0;
        for (int j = 0; j < compare; ++j) { const double v = h.mono(reference + j);otherEnergy += v * v; }
        for (int k = 0; k <= search; k += 2) {
            const int candidate = int(base) + k;
            double cross = 0.0, energy = 0.0;
            for (int j = 0; j < compare; j += 2) { const double v = h.mono(candidate + j);cross += v * h.mono(reference + j);energy += v * v; }
            const double score = cross / std::sqrt(energy * otherEnergy * .5 + 1.e-12);
            if (score > best) { best = score;choice = k; }
        }
        head.delay = base + choice;
    }
};
// Zero-delay-feedback state-variable filter; returns {low, band, high}.
struct SVF {
    double ic1{}, ic2{};
    std::array<double, 3> tick(double x, double g, double k) noexcept {
        const double a1 = 1. / (1. + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2. * v1 - ic1;ic2 = 2. * v2 - ic2;
        return {v2, v1, x - k * v1 - v2};
    }
};
// 90-degree phase-difference network (two four-stage all-pass chains, path A
// delayed one sample): analytic signal for single-sideband shifting.
struct Hilbert {
    static constexpr std::array<double, 4> aPath{.6923878, .9360654322959, .9882295226860, .9987488452737};
    static constexpr std::array<double, 4> bPath{.4021921162426, .8561710882420, .9722909545651, .9952884791278};
    std::array<std::array<double, 2>, 4> ax{}, ay{}, bx{}, by{};
    double aDelay{};
    static double stage(double x, double a, std::array<double, 2>& xs, std::array<double, 2>& ys) noexcept {
        const double y = a * a * (x + ys[1]) - xs[1];
        xs[1] = xs[0];xs[0] = x;ys[1] = ys[0];ys[0] = y;
        return y;
    }
    std::array<double, 2> process(double x) noexcept {
        double re = x, im = x;
        for (size_t i = 0; i < 4; ++i) re = stage(re, aPath[i], ax[i], ay[i]);
        for (size_t i = 0; i < 4; ++i) im = stage(im, bPath[i], bx[i], by[i]);
        const double delayed = aDelay;aDelay = re;
        return {delayed, im};
    }
};
} // namespace creative_detail

// One model's processor: renders the wet signal for a block.
class CreativeVoice {
public:
    void prepare(double sampleRate) {
        rate = sampleRate;
        history.prepare(int(rate * 2.2));
        frozen.prepare(int(rate * .5));
        slice.prepare(int(rate * 2.1));
        shifter.prepare(rate);
        reset();
    }
    void reset() noexcept {
        history.clear();frozen.clear();slice.clear();shifter.reset();
        for (auto& h : hilbert) h = {};
        for (auto& f : svf) f = {};
        shiftPhase = {};feedback = {};phase = 0;gate = 1;captureDelay = 0;stepIndex = 0;ratio = 1;glideRatio = 1;envelope = 0;armed = true;sweep = 1.e9;
        frozenLength = 0;frozenGain = 0;freezeClock = 0;holdSample = {};holdPhase = 1;tone = {};
        repeating = false;reversed = false;sliceStart = 0;sliceClock = 0;sliceLength = 1;
        for (auto& g : grains) g.active = false;
        grainClock = 0;random = {};
    }
    // Processes `samples` frames of `input` (stereo) into `wet`.
    void process(int model, const CreativeSettings& s, double tempo, const float* const* input, std::array<std::vector<float>, 2>& wet, int samples) noexcept {
        using namespace creative_detail;
        const double beats = creativeSyncBeats[size_t(std::clamp(s.sync, 0, int(creativeSyncBeats.size()) - 1))];
        const double bpm = std::isfinite(tempo) && tempo > 20.0 ? tempo : 120.0;
        // Step / cycle rate (Hz): tempo division when synced, else from A.
        const auto stepRate = [&](double lo, double hi) { return beats > 0 ? bpm / 60.0 / beats : expMap(lo, hi, s.a); };
        for (int n = 0; n < samples; ++n) {
            const float l = input[0][n], r = input[1][n];
            history.push(l, r);
            std::array<float, 2> y{l, r};
            switch (model) {
                case 0: { // Gate Slicer: A rate, B depth, C duty; 2 ms edges.
                    phase += stepRate(1.0, 20.0) / rate;phase -= std::floor(phase);
                    const double duty = .1 + .8 * s.c;
                    const double target = phase < duty ? 1.0 : 1.0 - s.b;
                    gate += (target - gate) * (1.0 - std::exp(-1.0 / (.002 * rate)));
                    y = {float(l * gate), float(r * gate)};
                    break;
                }
                case 1: { // Step Harmonizer: A step rate, B pattern, C glide.
                    static constexpr std::array<std::array<int, 4>, 4> patterns{{{0, 12, 0, 12}, {0, 4, 7, 12}, {0, 3, 7, 12}, {0, 7, 12, 19}}};
                    phase += stepRate(.5, 16.0) / rate;
                    if (phase >= 1.0) { phase -= 1.0;stepIndex = (stepIndex + 1) % 4; }
                    const auto& pattern = patterns[size_t(std::clamp(int(s.b * 4.f), 0, 3))];
                    ratio = std::pow(2.0, pattern[size_t(stepIndex)] / 12.0);
                    const double glide = .001 + .1 * s.c;
                    glideRatio += (ratio - glideRatio) * (1.0 - std::exp(-1.0 / (glide * rate)));
                    y = shifter.process(history, glideRatio);
                    break;
                }
                case 2: { // Time Bend: A cycle rate, B bend depth (0-12 st), C sine -> tape-stop ramp.
                    phase += expMap(.05, 4.0, s.a) / rate;phase -= std::floor(phase);
                    const double sine = std::sin(2.0 * pi * phase), ramp = -phase;
                    const double bend = 12.0 * s.b * ((1.0 - s.c) * sine + s.c * ramp);
                    y = shifter.process(history, std::pow(2.0, bend / 12.0));
                    break;
                }
                case 3: { // Zap Sweep: onset restarts an 8 kHz -> 150 Hz resonant band-pass sweep.
                    const double level = std::max(std::abs(l), std::abs(r));
                    envelope = level > envelope ? level : envelope * std::exp(-1.0 / (.05 * rate));
                    const double threshold = std::pow(10.0, (-60.0 + 48.0 * s.b) / 20.0);
                    if (armed && envelope > threshold) { armed = false;sweep = 0; }
                    if (!armed && envelope < threshold * .5) armed = true;
                    const double time = .02 + .58 * s.a;
                    const double hz = sweep < time ? 8000.0 * std::pow(150.0 / 8000.0, sweep / time) : 150.0;
                    sweep += 1.0 / rate;
                    const double g = std::tan(pi * std::min(hz, rate * .45) / rate), q = 2.0 + 18.0 * s.c, k = 1.0 / q;
                    for (int c = 0; c < 2; ++c) y[size_t(c)] = float(svf[size_t(c)].tick(c == 0 ? l : r, g, k)[1]);
                    break;
                }
                case 4: { // Freq Shift: A shift (-1000..+1000 Hz, cubic around 0), B feedback, C spread.
                    const double centred = 2.0 * s.a - 1.0, shift = 1000.0 * centred * centred * centred;
                    const std::array<double, 2> hz{shift, shift * (1.0 - 2.0 * s.c)};
                    for (int c = 0; c < 2; ++c) {
                        const double x = (c == 0 ? l : r) + .9 * s.b * feedback[size_t(c)];
                        const auto analytic = hilbert[size_t(c)].process(x);
                        shiftPhase[size_t(c)] += hz[size_t(c)] / rate;shiftPhase[size_t(c)] -= std::floor(shiftPhase[size_t(c)]);
                        const double w = 2.0 * pi * shiftPhase[size_t(c)];
                        const double out = analytic[0] * std::cos(w) + analytic[1] * std::sin(w);   // positive shift moves up
                        feedback[size_t(c)] = std::tanh(out);
                        y[size_t(c)] = float(out);
                    }
                    break;
                }
                case 5: { // Freeze Hold: an onset captures the last C ms; it loops in two crossfaded grains and decays over A.
                    const double level = std::max(std::abs(l), std::abs(r));
                    envelope = level > envelope ? level : envelope * std::exp(-1.0 / (.05 * rate));
                    const double threshold = std::pow(10.0, (-60.0 + 48.0 * s.b) / 20.0);
                    const int grain = int(rate * (.06 + .34 * s.c));
                    if (armed && envelope > threshold) { armed = false;captureDelay = grain; }
                    if (!armed && envelope < threshold * .5) armed = true;
                    if (captureDelay > 0 && --captureDelay == 0) {   // the grain after the onset
                        for (int k = 0; k < grain; ++k)
                            for (int c = 0; c < 2; ++c) frozen.data[size_t(c)][size_t(k)] = history.read(c, double(grain - k));
                        frozenLength = grain;frozenGain = 1.0;freezeClock = 0;
                    }
                    y = {0.f, 0.f};
                    if (frozenLength > 1) {
                        const double hold = expMap(.5, 30.0, s.a);
                        frozenGain *= std::pow(10.0, -3.0 / (hold * rate));
                        for (int head = 0; head < 2; ++head) {
                            double p = double(freezeClock) / frozenLength + .5 * head;p -= std::floor(p);
                            const float w = float(std::sin(pi * p));
                            const int index = std::clamp(int(p * frozenLength), 0, frozenLength - 1);
                            for (int c = 0; c < 2; ++c) y[size_t(c)] += float(frozenGain) * w * w * frozen.data[size_t(c)][size_t(index)];
                        }
                        if (++freezeClock >= frozenLength) freezeClock = 0;
                    }
                    break;
                }
                case 6: { // Glitch Stutter: each step, with chance B, repeats the previous step (reversed with chance C).
                    const double step = rate / stepRate(1.0, 16.0);
                    sliceLength = std::clamp(int(std::lround(step)), 16, int(rate * 2.0));
                    if (++sliceClock >= sliceLength) {
                        sliceClock = 0;
                        repeating = random.next() < s.b;
                        reversed = repeating && random.next() < s.c;
                        if (repeating) for (int k = 0; k < sliceLength; ++k)
                            for (int c = 0; c < 2; ++c) slice.data[size_t(c)][size_t(k)] = history.read(c, double(sliceLength - k));
                    }
                    if (repeating) {
                        const int index = reversed ? sliceLength - 1 - sliceClock : sliceClock;
                        const double fade = std::min({1.0, sliceClock / (.003 * rate), (sliceLength - sliceClock) / (.003 * rate)});
                        for (int c = 0; c < 2; ++c) {
                            const float input = c == 0 ? l : r;
                            y[size_t(c)] = float(fade * slice.data[size_t(c)][size_t(index)] + (1.0 - fade) * input);
                        }
                    }
                    break;
                }
                case 7: { // Bit Crush: A word length 16 -> 2 bits, B rate 48 kHz -> 500 Hz (hold), C tone 20 kHz -> 1 kHz.
                    const double bits = 16.0 - 14.0 * s.a, steps = std::pow(2.0, bits - 1.0);
                    holdPhase += expMap(rate, 500.0, s.b) / rate;
                    if (holdPhase >= 1.0) {
                        holdPhase -= std::floor(holdPhase);
                        for (int c = 0; c < 2; ++c) holdSample[size_t(c)] = float(std::round(double(c == 0 ? l : r) * steps) / steps);
                    }
                    const double cutoff = expMap(20000.0, 1000.0, s.c), coefficient = 1.0 - std::exp(-2.0 * pi * std::min(cutoff, rate * .45) / rate);
                    // Tone 0 % is no filter: the quantized, held samples pass as they are.
                    for (int c = 0; c < 2; ++c) {
                        tone[size_t(c)] = s.c <= 0.f ? double(holdSample[size_t(c)]) : tone[size_t(c)] + coefficient * (holdSample[size_t(c)] - tone[size_t(c)]);
                        y[size_t(c)] = float(tone[size_t(c)]);
                    }
                    break;
                }
                case 8: { // Grain Cloud: A density 2-60 grains/s, B pitch scatter 0-12 st, C grain 20-200 ms.
                    const double density = expMap(2.0, 60.0, s.a), length = rate * expMap(.02, .2, s.c);
                    grainClock -= 1.0;
                    if (grainClock <= 0.0) {
                        grainClock += rate / density * (.5 + random.next());
                        for (auto& g : grains) if (!g.active) {
                            g.active = true;g.age = 0;g.length = length;
                            g.start = length * 2.0 + random.next() * rate * .5;
                            g.ratio = std::pow(2.0, (2.0 * random.next() - 1.0) * 12.0 * s.b / 12.0);
                            g.pan = random.next();
                            break;
                        }
                    }
                    // K = density x length grains overlap on average; Hann^2 grains carry 3/8
                    // of the power each, so 1 / sqrt(0.75 K) keeps the cloud at half the input
                    // power (-3 dB) at any density. Sparse clouds are capped at +6 dB of gain.
                    const double norm = std::min(2.0, 1.0 / std::sqrt(.75 * density * length / rate));
                    y = {0.f, 0.f};
                    for (auto& g : grains) {
                        if (!g.active) continue;
                        const double w = std::sin(pi * g.age / g.length);
                        const double ago = g.start - g.age * g.ratio + g.age;   // the read point moves at the grain's ratio
                        for (int c = 0; c < 2; ++c) {
                            const double pan = c == 0 ? std::sqrt(1.0 - g.pan) : std::sqrt(g.pan);
                            y[size_t(c)] += float(norm * w * w * pan * 1.41421356 * history.read(c, ago));
                        }
                        if (++g.age >= g.length) g.active = false;
                    }
                    break;
                }
                default: break;
            }
            wet[0][size_t(n)] = y[0];wet[1][size_t(n)] = y[1];
        }
    }
private:
    double rate{48000};
    creative_detail::History history, frozen, slice;
    creative_detail::PitchShifter shifter;
    std::array<creative_detail::Hilbert, 2> hilbert{};
    std::array<creative_detail::SVF, 2> svf{};
    std::array<double, 2> shiftPhase{}, feedback{}, tone{};
    std::array<float, 2> holdSample{};
    double phase{}, ratio{1}, glideRatio{1}, gate{1}, envelope{}, sweep{1.e9}, frozenGain{}, holdPhase{1}, grainClock{};
    int stepIndex{}, frozenLength{}, freezeClock{}, captureDelay{}, sliceStart{}, sliceClock{}, sliceLength{1};
    bool armed{true}, repeating{}, reversed{};
    struct Grain { bool active{}; double age{}, length{1}, start{}, ratio{1}, pan{.5}; };
    std::array<Grain, 24> grains{};
    creative_detail::Random random;
};

// The CREATIVE slot: binds its parameters, crossfades model changes (30 ms)
// and switching on/off (20 ms). Off and settled returns at once.
class CreativeFX final : public PostInsert {
public:
    void bind(juce::AudioProcessorValueTreeState& state) {
        on = state.getRawParameterValue("creativeOn");model = state.getRawParameterValue("creativeModel");
        a = state.getRawParameterValue("creativeA");b = state.getRawParameterValue("creativeB");c = state.getRawParameterValue("creativeC");
        mix = state.getRawParameterValue("creativeMix");sync = state.getRawParameterValue("creativeSync");output = state.getRawParameterValue("creativeOutput");
    }
    void prepare(const juce::dsp::ProcessSpec& spec) {
        rate = spec.sampleRate;
        for (auto& v : voices) v.prepare(rate);
        for (auto& w : wet) for (auto& ch : w) ch.assign(size_t(spec.maximumBlockSize), 0.f);
        silence.assign(size_t(spec.maximumBlockSize), 0.f);
        level.reset(rate, .02);fade.reset(rate, .03);gain.reset(rate, .02);
        reset();
    }
    void reset() noexcept {
        for (auto& v : voices) v.reset();
        const auto s = read();
        level.setCurrentAndTargetValue(s.on ? s.mix : 0.f);fade.setCurrentAndTargetValue(1.f);
        gain.setCurrentAndTargetValue(s.on ? juce::Decibels::decibelsToGain(s.outputDb) : 1.f);
        current = s.model;previous = -1;active = s.on;
    }
    CreativeSettings read() const noexcept {
        CreativeSettings s;
        const auto value = [](const std::atomic<float>* p, float fallback) { const float v = p != nullptr ? p->load() : fallback; return std::isfinite(v) ? v : fallback; };
        s.on = value(on, 0.f) > .5f;
        s.model = juce::jlimit(0, creativeModelCount - 1, juce::roundToInt(value(model, 0.f)));
        s.a = juce::jlimit(0.f, 1.f, value(a, 50.f) * .01f);s.b = juce::jlimit(0.f, 1.f, value(b, 50.f) * .01f);s.c = juce::jlimit(0.f, 1.f, value(c, 50.f) * .01f);
        s.mix = juce::jlimit(0.f, 1.f, value(mix, 50.f) * .01f);
        s.sync = juce::jlimit(0, int(creativeSyncBeats.size()) - 1, juce::roundToInt(value(sync, 3.f)));
        s.outputDb = juce::jlimit(-24.f, 12.f, value(output, 0.f));
        return s;
    }
    void process(juce::AudioBuffer<float>& buffer, double tempo) noexcept override {
        const auto s = read();
        const int samples = buffer.getNumSamples(), channels = buffer.getNumChannels();
        if (!s.on && !active) return;
        if (samples > int(silence.size()) || channels < 1) return;
        if (s.on && !active) {   // switching on: start from clean voices
            for (auto& v : voices) v.reset();
            current = s.model;previous = -1;fade.setCurrentAndTargetValue(1.f);
            level.setCurrentAndTargetValue(0.f);gain.setCurrentAndTargetValue(1.f);
            active = true;
        }
        level.setTargetValue(s.on ? s.mix : 0.f);
        gain.setTargetValue(s.on ? juce::Decibels::decibelsToGain(s.outputDb) : 1.f);
        if (s.model != current && previous < 0) {   // a model change crossfades from the old voice
            previous = current;current = s.model;
            slot = 1 - slot;   // the old voice keeps running in the other slot
            voices[size_t(slot)].reset();
            fade.setCurrentAndTargetValue(0.f);fade.setTargetValue(1.f);
        }
        const float* inputs[2] = {buffer.getReadPointer(0), buffer.getReadPointer(channels > 1 ? 1 : 0)};
        voices[size_t(slot)].process(current, s, tempo, inputs, wet[0], samples);
        const bool crossfading = previous >= 0;
        if (crossfading) voices[size_t(1 - slot)].process(previous, s, tempo, inputs, wet[1], samples);
        for (int n = 0; n < samples; ++n) {
            const float m = level.getNextValue(), g = gain.getNextValue(), f = crossfading ? fade.getNextValue() : 1.f;
            for (int ch = 0; ch < std::min(channels, 2); ++ch) {
                const float dry = buffer.getSample(ch, n);
                const float processed = f * wet[0][size_t(ch)][size_t(n)] + (1.f - f) * (crossfading ? wet[1][size_t(ch)][size_t(n)] : 0.f);
                buffer.setSample(ch, n, g * (dry * (1.f - m) + processed * m));   // exact at Mix 0 and 100 %
            }
        }
        if (crossfading && !fade.isSmoothing()) previous = -1;
        // Off and faded out: the slot goes idle and later blocks are untouched.
        if (!s.on && !level.isSmoothing() && !gain.isSmoothing()) { active = false;for (auto& v : voices) v.reset(); }
    }
    bool isActive() const noexcept { return active; }
private:
    double rate{48000};
    std::array<CreativeVoice, 2> voices;
    std::array<std::array<std::vector<float>, 2>, 2> wet;
    std::vector<float> silence;
    juce::SmoothedValue<float> level, fade, gain;
    int current{}, previous{-1}, slot{};
    bool active{};
    std::atomic<float>* on{};std::atomic<float>* model{};std::atomic<float>* a{};std::atomic<float>* b{};std::atomic<float>* c{};
    std::atomic<float>* mix{};std::atomic<float>* sync{};std::atomic<float>* output{};
};

} // namespace spectralforge
