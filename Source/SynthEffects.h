#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
#include <vector>

// SYNTH: an effect rack after the voices (docs/SYNTH_EFFECTS.md): chorus,
// phaser, delay, reverb and a two-band EQ, in that order. Twenty-two
// parameters appended at version hint 41. With every effect off the rack is
// not run and the synth's output is the released one, sample for sample.
namespace spectralforge {

inline constexpr int synthEffectsParameterVersion = 41;
inline constexpr int synthEffectsParameterCount = 4 + 5 + 6 + 4 + 3;
inline constexpr int synthEffectCount = 5;
enum SynthEffect { synthFxChorus, synthFxPhaser, synthFxDelay, synthFxReverb, synthFxEq };
inline juce::StringArray synthEffectNames() { return {"Chorus", "Phaser", "Delay", "Reverb", "EQ"}; }

inline juce::StringArray synthFxDelaySyncNames() { return {"Free", "1/2", "1/4", "1/4 D", "1/8", "1/8 D", "1/16", "1/4 T", "1/8 T"}; }
// Beats (quarter notes) per repeat; 0 is Free (the TIME control).
inline constexpr std::array<double, 9> synthFxDelaySyncBeats{0., 2., 1., 1.5, .5, .75, .25, 2. / 3., 1. / 3.};
inline constexpr double synthFxDelayMaxSeconds = 2.0;
inline constexpr double synthFxEqLowHz = 200.0, synthFxEqHighHz = 4000.0;

inline void appendSynthEffectsParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    constexpr int v = synthEffectsParameterVersion;
    const auto toggle = [&](const char* id, const char* name) { layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, v}, name, false)); };
    const auto number = [&](const char* id, const char* name, float lo, float hi, float step, float initial, float centre, const char* unit, int decimals) {
        juce::NormalisableRange<float> range{lo, hi, step};
        if (centre > lo) range.setSkewForCentre(centre);
        const juce::String suffix(unit);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, v}, name, range, initial,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([suffix, decimals](float value, int) { return juce::String(value, decimals) + suffix; })));
    };
    const auto percent = [&](const char* id, const char* name, float hi, float initial) { number(id, name, 0.f, hi, 1.f, initial, 0.f, " %", 0); };
    toggle("synthFxChorusOn", "Synth chorus");
    number("synthFxChorusRate", "Synth chorus rate", .05f, 5.f, .01f, .6f, .8f, " Hz", 2);
    percent("synthFxChorusDepth", "Synth chorus depth", 100.f, 50.f);
    percent("synthFxChorusMix", "Synth chorus mix", 100.f, 50.f);
    toggle("synthFxPhaserOn", "Synth phaser");
    number("synthFxPhaserRate", "Synth phaser rate", .02f, 8.f, .01f, .4f, .8f, " Hz", 2);
    percent("synthFxPhaserDepth", "Synth phaser depth", 100.f, 70.f);
    percent("synthFxPhaserFeedback", "Synth phaser feedback", 90.f, 40.f);
    percent("synthFxPhaserMix", "Synth phaser mix", 100.f, 50.f);
    toggle("synthFxDelayOn", "Synth delay");
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"synthFxDelaySync", v}, "Synth delay tempo sync", synthFxDelaySyncNames(), 4));
    number("synthFxDelayTime", "Synth delay time", 1.f, float(synthFxDelayMaxSeconds * 1000.0), 1.f, 350.f, 350.f, " ms", 0);
    percent("synthFxDelayFeedback", "Synth delay feedback", 95.f, 35.f);
    toggle("synthFxDelayPingPong", "Synth delay ping-pong");
    percent("synthFxDelayMix", "Synth delay mix", 100.f, 30.f);
    toggle("synthFxReverbOn", "Synth reverb");
    number("synthFxReverbDecay", "Synth reverb decay", .2f, 12.f, .01f, 2.f, 2.f, " s", 2);
    percent("synthFxReverbDamp", "Synth reverb damping", 100.f, 40.f);
    percent("synthFxReverbMix", "Synth reverb mix", 100.f, 25.f);
    toggle("synthFxEqOn", "Synth EQ");
    number("synthFxEqLow", "Synth EQ low shelf", -12.f, 12.f, .1f, 0.f, 0.f, " dB", 1);
    number("synthFxEqHigh", "Synth EQ high shelf", -12.f, 12.f, .1f, 0.f, 0.f, " dB", 1);
}

// RBJ shelf (slope 1) as b0 b1 b2 a1 a2, normalised.
inline std::array<double, 5> synthShelf(bool high, double hz, double gainDb, double rate) noexcept {
    const double a = std::pow(10.0, gainDb / 40.0), w = 6.283185307179586 * std::min(hz, rate * .45) / rate, cs = std::cos(w), alpha = std::sin(w) * .7071067811865476;
    const double root = 2.0 * std::sqrt(a) * alpha, sign = high ? -1.0 : 1.0;
    const double b0 = a * ((a + 1) - sign * (a - 1) * cs + root), b1 = sign * 2 * a * ((a - 1) - sign * (a + 1) * cs), b2 = a * ((a + 1) - sign * (a - 1) * cs - root);
    const double a0 = (a + 1) + sign * (a - 1) * cs + root, a1 = -sign * 2 * ((a - 1) + sign * (a + 1) * cs), a2 = (a + 1) + sign * (a - 1) * cs - root;
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}
// The EQ's gain at `hz` (the tests).
inline double synthFxEqMagnitude(double lowDb, double highDb, double hz, double rate) noexcept {
    double gain = 1.0;
    for (int k = 0; k < 2; ++k) {
        const auto c = synthShelf(k == 1, k == 0 ? synthFxEqLowHz : synthFxEqHighHz, k == 0 ? lowDb : highDb, rate);
        const double w = 6.283185307179586 * hz / rate, c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2 * w), s2 = std::sin(2 * w);
        const double nr = c[0] + c[1] * c1 + c[2] * c2, ni = -c[1] * s1 - c[2] * s2, dr = 1 + c[3] * c1 + c[4] * c2, di = -c[3] * s1 - c[4] * s2;
        gain *= std::sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
    }
    return gain;
}

class SynthEffects {
public:
    struct Settings {
        bool chorusOn{}; float chorusRate{.6f}, chorusDepth{.5f}, chorusMix{.5f};
        bool phaserOn{}; float phaserRate{.4f}, phaserDepth{.7f}, phaserFeedback{.4f}, phaserMix{.5f};
        bool delayOn{}; int delaySync{4}; float delayMs{350.f}, delayFeedback{.35f}; bool delayPingPong{}; float delayMix{.3f};
        bool reverbOn{}; float reverbDecay{2.f}, reverbDamp{.4f}, reverbMix{.25f};
        bool eqOn{}; float eqLow{}, eqHigh{};
        bool any() const noexcept { return chorusOn || phaserOn || delayOn || reverbOn || eqOn; }
    };
    struct Pointers { std::array<std::atomic<float>*, synthEffectsParameterCount> a{}; };
    static juce::StringArray parameterIDs() {
        return {"synthFxChorusOn", "synthFxChorusRate", "synthFxChorusDepth", "synthFxChorusMix",
                "synthFxPhaserOn", "synthFxPhaserRate", "synthFxPhaserDepth", "synthFxPhaserFeedback", "synthFxPhaserMix",
                "synthFxDelayOn", "synthFxDelaySync", "synthFxDelayTime", "synthFxDelayFeedback", "synthFxDelayPingPong", "synthFxDelayMix",
                "synthFxReverbOn", "synthFxReverbDecay", "synthFxReverbDamp", "synthFxReverbMix",
                "synthFxEqOn", "synthFxEqLow", "synthFxEqHigh"};
    }
    void bind(juce::AudioProcessorValueTreeState& state) {
        const auto ids = parameterIDs();
        for (int i = 0; i < synthEffectsParameterCount; ++i) p.a[size_t(i)] = state.getRawParameterValue(ids[i]);
    }
    Settings read() const noexcept {
        const Settings d;int k = 0;
        const auto f = [&](float fallback, float lo, float hi) { const auto* a = p.a[size_t(k++)];const float v = a != nullptr ? a->load() : fallback;return std::isfinite(v) ? juce::jlimit(lo, hi, v) : fallback; };
        const auto on = [&] { return f(0.f, 0.f, 1.f) > .5f; };
        Settings s;
        s.chorusOn = on();s.chorusRate = f(d.chorusRate, .05f, 5.f);s.chorusDepth = f(50.f, 0.f, 100.f) * .01f;s.chorusMix = f(50.f, 0.f, 100.f) * .01f;
        s.phaserOn = on();s.phaserRate = f(d.phaserRate, .02f, 8.f);s.phaserDepth = f(70.f, 0.f, 100.f) * .01f;s.phaserFeedback = f(40.f, 0.f, 90.f) * .01f;s.phaserMix = f(50.f, 0.f, 100.f) * .01f;
        s.delayOn = on();s.delaySync = juce::roundToInt(f(4.f, 0.f, float(synthFxDelaySyncBeats.size() - 1)));s.delayMs = f(d.delayMs, 1.f, float(synthFxDelayMaxSeconds * 1000.0));
        s.delayFeedback = f(35.f, 0.f, 95.f) * .01f;s.delayPingPong = on();s.delayMix = f(30.f, 0.f, 100.f) * .01f;
        s.reverbOn = on();s.reverbDecay = f(d.reverbDecay, .2f, 12.f);s.reverbDamp = f(40.f, 0.f, 100.f) * .01f;s.reverbMix = f(25.f, 0.f, 100.f) * .01f;
        s.eqOn = on();s.eqLow = f(0.f, -12.f, 12.f);s.eqHigh = f(0.f, -12.f, 12.f);
        return s;
    }
    void prepare(double sampleRate) {
        rate = sampleRate;
        ramp = 1.0 / (.02 * rate);
        chorusLength = int(std::ceil(.04 * rate)) + 8;
        for (auto& b : chorus) b.assign(size_t(chorusLength), 0.f);
        delayLength = int(std::ceil(synthFxDelayMaxSeconds * rate)) + 8;
        for (auto& b : delay) b.assign(size_t(delayLength), 0.f);
        // Reverb: eight delay lines of 23-60 ms (prime lengths at 48 kHz) and two input all-passes a channel.
        static constexpr std::array<int, reverbLines> primes{1123, 1327, 1559, 1801, 2053, 2311, 2579, 2857};
        for (int i = 0; i < reverbLines; ++i) { lineLength[size_t(i)] = std::max(8, int(std::lround(primes[size_t(i)] * rate / 48000.0)));line[size_t(i)].assign(size_t(lineLength[size_t(i)]), 0.f); }
        static constexpr std::array<int, 4> diffusion{241, 83, 263, 89};
        for (int i = 0; i < 4; ++i) { diffuseLength[size_t(i)] = std::max(4, int(std::lround(diffusion[size_t(i)] * rate / 48000.0)));diffuse[size_t(i)].assign(size_t(diffuseLength[size_t(i)]), 0.f); }
        delayDamp = 1.0 - std::exp(-6.283185307179586 * 8000.0 / rate);
        delaySlew = 1.0 - std::exp(-1.0 / (.05 * rate));
        reset();
    }
    void reset() noexcept {
        wet = {};
        for (int e = 0; e < synthEffectCount; ++e) clear(e);
        dirty = {};
        delaySamples = 0;
    }
    // True while any effect is on or still fading out: the rack must run.
    bool running(const Settings& s) const noexcept {
        if (s.any()) return true;
        for (const double g : wet) if (g > 0) return true;
        return false;
    }
    // Once a block, before process(): the block's coefficients.
    void begin(const Settings& s) noexcept {
        if (s.eqOn || wet[synthFxEq] > 0) { low = synthShelf(false, synthFxEqLowHz, s.eqLow, rate);high = synthShelf(true, synthFxEqHighHz, s.eqHigh, rate); }
        if (s.reverbOn || wet[synthFxReverb] > 0) {
            double energy = 0;
            for (int i = 0; i < reverbLines; ++i) { lineGain[size_t(i)] = std::pow(10.0, -3.0 * lineLength[size_t(i)] / (rate * double(s.reverbDecay)));energy += 1.0 - lineGain[size_t(i)] * lineGain[size_t(i)]; }
            // The tail's level stays about the input's whatever the decay time.
            reverbScale = reverbLevel * std::sqrt(energy / reverbLines);
            reverbDamp = .7 * double(s.reverbDamp);
        }
    }
    // One stereo sample through the rack.
    void process(double& l, double& r, const Settings& s, double bpm) noexcept {
        if (step(synthFxChorus, s.chorusOn, s.chorusMix)) chorusSample(l, r, s);
        if (step(synthFxPhaser, s.phaserOn, s.phaserMix)) phaserSample(l, r, s);
        if (step(synthFxDelay, s.delayOn, s.delayMix)) delaySample(l, r, s, bpm);
        if (step(synthFxReverb, s.reverbOn, s.reverbMix)) reverbSample(l, r);
        if (step(synthFxEq, s.eqOn, 1.f)) eqSample(l, r);
    }
    // The delay time in seconds for these settings (the page and the tests).
    static double delaySeconds(const Settings& s, double bpm) noexcept {
        const double beats = synthFxDelaySyncBeats[size_t(juce::jlimit(0, int(synthFxDelaySyncBeats.size()) - 1, s.delaySync))];
        return std::min(synthFxDelayMaxSeconds, beats > 0 ? beats * 60.0 / std::max(20.0, bpm) : double(s.delayMs) * .001);
    }
private:
    static constexpr int reverbLines = 8, phaserStages = 6;
    static constexpr double reverbLevel = .8;   // measured: held sines A1-C7 average about 3 dB under the dry note at 100 % mix
    // Moves the effect's wet share towards its target over 20 ms (full scale).
    // False when the effect is off and faded out; its memory is cleared once.
    bool step(int e, bool on, float mix) noexcept {
        double& g = wet[size_t(e)];const double target = on ? double(mix) : 0.0;
        if (g == 0 && !on) { if (dirty[size_t(e)]) { clear(e);dirty[size_t(e)] = false; } return false; }
        g = g < target ? std::min(target, g + ramp) : std::max(target, g - ramp);
        dirty[size_t(e)] = true;
        return true;
    }
    void clear(int e) noexcept {
        switch (e) {
            case synthFxChorus: for (auto& b : chorus) std::fill(b.begin(), b.end(), 0.f);chorusWrite = 0;chorusPhase = 0;break;
            case synthFxPhaser: phaserState = {};phaserLast = {};phaserPhase = 0;break;
            case synthFxDelay: for (auto& b : delay) std::fill(b.begin(), b.end(), 0.f);delayWrite = 0;delayLow = {};delaySamples = 0;break;
            case synthFxReverb: for (auto& b : line) std::fill(b.begin(), b.end(), 0.f);for (auto& b : diffuse) std::fill(b.begin(), b.end(), 0.f);lineWrite = {};diffuseWrite = {};lineLow = {};break;
            default: eqState = {};break;
        }
    }
    static double hermite(const std::vector<float>& b, int length, double position) noexcept {
        const int i = int(std::floor(position));const double t = position - i;
        const auto at = [&](int k) { int n = (i + k) % length;if (n < 0) n += length;return double(b[size_t(n)]); };
        const double a = at(-1), c = at(0), d = at(1), e = at(2);
        return c + .5 * t * (d - a + t * (2 * a - 5 * c + 4 * d - e + t * (3 * (c - d) + e - a)));
    }
    // Chorus: a 12 ms delay swept by up to 6 ms, the right channel a quarter cycle behind.
    void chorusSample(double& l, double& r, const Settings& s) noexcept {
        const double g = wet[synthFxChorus];
        chorus[0][size_t(chorusWrite)] = float(l);chorus[1][size_t(chorusWrite)] = float(r);
        const double in[2]{l, r};double out[2];
        for (int c = 0; c < 2; ++c) {
            const double lfo = std::sin(6.283185307179586 * (chorusPhase + .25 * c));
            const double d = rate * (.012 + .006 * double(s.chorusDepth) * lfo);
            out[c] = in[c] * (1 - g) + hermite(chorus[size_t(c)], chorusLength, chorusWrite - d) * g;
        }
        chorusWrite = chorusWrite + 1 < chorusLength ? chorusWrite + 1 : 0;
        chorusPhase += double(s.chorusRate) / rate;if (chorusPhase >= 1) chorusPhase -= 1;
        l = out[0];r = out[1];
    }
    // Phaser: six all-pass stages swept around 800 Hz by up to two octaves each way.
    void phaserSample(double& l, double& r, const Settings& s) noexcept {
        const double g = wet[synthFxPhaser];
        const double in[2]{l, r};double out[2];
        for (int c = 0; c < 2; ++c) {
            const double lfo = std::sin(6.283185307179586 * (phaserPhase + .25 * c));
            const double hz = std::min(800.0 * std::exp2(2.0 * double(s.phaserDepth) * lfo), rate * .45);
            const double t = std::tan(3.141592653589793 * hz / rate), a = (t - 1) / (t + 1);
            double x = in[c] + double(s.phaserFeedback) * phaserLast[size_t(c)];
            for (int k = 0; k < phaserStages; ++k) { double& z = phaserState[size_t(c)][size_t(k)];const double y = a * x + z;z = x - a * y;x = y; }
            phaserLast[size_t(c)] = x;
            out[c] = in[c] * (1 - g) + x * g;
        }
        phaserPhase += double(s.phaserRate) / rate;if (phaserPhase >= 1) phaserPhase -= 1;
        l = out[0];r = out[1];
    }
    // Delay: stereo, or ping-pong (the mono input enters on the left and the
    // repeats cross). The repeats lose treble above 8 kHz each time round. A
    // change of time glides over about 50 ms.
    void delaySample(double& l, double& r, const Settings& s, double bpm) noexcept {
        const double g = wet[synthFxDelay];
        const double target = std::clamp(delaySeconds(s, bpm) * rate, 1.0, double(delayLength - 4));
        if (delaySamples <= 0) delaySamples = target;else delaySamples += (target - delaySamples) * delaySlew;
        const double tapL = hermite(delay[0], delayLength, delayWrite - delaySamples), tapR = hermite(delay[1], delayLength, delayWrite - delaySamples);
        delayLow[0] += (tapL - delayLow[0]) * delayDamp;delayLow[1] += (tapR - delayLow[1]) * delayDamp;
        const double fb = double(s.delayFeedback);
        if (s.delayPingPong) { delay[0][size_t(delayWrite)] = float(.5 * (l + r) + fb * delayLow[1]);delay[1][size_t(delayWrite)] = float(fb * delayLow[0]); }
        else { delay[0][size_t(delayWrite)] = float(l + fb * delayLow[0]);delay[1][size_t(delayWrite)] = float(r + fb * delayLow[1]); }
        delayWrite = delayWrite + 1 < delayLength ? delayWrite + 1 : 0;
        l = l * (1 - g) + tapL * g;r = r * (1 - g) + tapR * g;
    }
    // Reverb: a feedback delay network of eight lines (Householder mixing),
    // each damped by a one-pole low-pass, behind two all-passes a channel.
    void reverbSample(double& l, double& r) noexcept {
        const double g = wet[synthFxReverb];
        double in[2]{l, r};
        for (int c = 0; c < 2; ++c) for (int k = 0; k < 2; ++k) {
            const int i = c * 2 + k;auto& b = diffuse[size_t(i)];int& w = diffuseWrite[size_t(i)];
            const double delayed = b[size_t(w)], y = delayed - .6 * in[c];
            b[size_t(w)] = float(in[c] + .6 * y);w = w + 1 < diffuseLength[size_t(i)] ? w + 1 : 0;in[c] = y;
        }
        std::array<double, reverbLines> tap;double sum = 0;
        for (int i = 0; i < reverbLines; ++i) {
            double& z = lineLow[size_t(i)];
            z += (double(line[size_t(i)][size_t(lineWrite[size_t(i)])]) - z) * (1 - reverbDamp);
            tap[size_t(i)] = z * lineGain[size_t(i)];sum += tap[size_t(i)];
        }
        sum *= 2.0 / reverbLines;
        double out[2]{};
        for (int i = 0; i < reverbLines; ++i) {
            out[i & 1] += (i & 2) ? -tap[size_t(i)] : tap[size_t(i)];
            int& w = lineWrite[size_t(i)];
            line[size_t(i)][size_t(w)] = float(in[i & 1] + tap[size_t(i)] - sum);w = w + 1 < lineLength[size_t(i)] ? w + 1 : 0;
        }
        l = l * (1 - g) + out[0] * reverbScale * g;r = r * (1 - g) + out[1] * reverbScale * g;
    }
    // EQ: a low shelf at 200 Hz and a high shelf at 4 kHz.
    void eqSample(double& l, double& r) noexcept {
        const double g = wet[synthFxEq];
        const double in[2]{l, r};double out[2];
        for (int c = 0; c < 2; ++c) {
            double x = in[c];
            for (int k = 0; k < 2; ++k) {
                const auto& q = k == 0 ? low : high;auto& z = eqState[size_t(c)][size_t(k)];
                const double y = q[0] * x + z[0];z[0] = q[1] * x - q[3] * y + z[1];z[1] = q[2] * x - q[4] * y;x = y;
            }
            out[c] = in[c] * (1 - g) + x * g;
        }
        l = out[0];r = out[1];
    }
    Pointers p;
    double rate{48000}, ramp{1e-3};
    std::array<double, synthEffectCount> wet{};std::array<bool, synthEffectCount> dirty{};
    std::array<std::vector<float>, 2> chorus;int chorusLength{8}, chorusWrite{};double chorusPhase{};
    std::array<std::array<double, phaserStages>, 2> phaserState{};std::array<double, 2> phaserLast{};double phaserPhase{};
    std::array<std::vector<float>, 2> delay;int delayLength{8}, delayWrite{};double delaySamples{}, delayDamp{.6}, delaySlew{1e-3};std::array<double, 2> delayLow{};
    std::array<std::vector<float>, reverbLines> line;std::array<int, reverbLines> lineLength{}, lineWrite{};std::array<double, reverbLines> lineGain{}, lineLow{};
    std::array<std::vector<float>, 4> diffuse;std::array<int, 4> diffuseLength{}, diffuseWrite{};double reverbScale{}, reverbDamp{};
    std::array<double, 5> low{1, 0, 0, 0, 0}, high{1, 0, 0, 0, 0};std::array<std::array<std::array<double, 2>, 2>, 2> eqState{};
};
} // namespace spectralforge
