#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace spectralforge {

// 1.10 SYNTH layer (docs/SYNTH_1_10.md): one monophonic synth voice played by
// the guitar (pitch and level tracked from the clean input) and/or host MIDI
// notes, added in parallel after RIG SUM and before Tone EQ. Three layers:
// Dvergr Analog, Seidr Dynamic and Mimir Impact. Off (the default) adds
// nothing and costs nothing. Appended at version hint 27.
inline constexpr int synthParameterVersion = 27;
inline constexpr int synthModelCount = 3;
inline constexpr int synthParameterCount = 11;
struct SynthModelInfo { const char* name; const char* character; const char* toneLabel; const char* characterLabel; juce::uint32 colour; };
inline constexpr std::array<SynthModelInfo, synthModelCount> synthModels{{
    {"Dvergr Analog", "Two detuned saws and a sub octave into a resonant 24 dB low-pass", "CUTOFF", "DETUNE", 0xff8a5a2a},
    {"Seidr Dynamic", "Pulse-width pad whose brightness and level follow the playing", "BRIGHTNESS", "DYNAMICS", 0xff3a5a8a},
    {"Mimir Impact", "Two-operator FM strike with a noise transient", "STRIKE", "RATIO", 0xff6a7a8a}}};
inline juce::StringArray synthModelNames() { juce::StringArray n; for (const auto& m : synthModels) n.add(m.name); return n; }
inline juce::StringArray synthSourceNames() { return {"Guitar + MIDI", "Guitar", "MIDI"}; }

inline void appendSynthParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    constexpr int v = synthParameterVersion;
    const auto number = [&](const char* id, const char* name, float lo, float hi, float initial, float centre, const juce::String& unit, int decimals) {
        juce::NormalisableRange<float> range{lo, hi, .01f};
        if (centre > lo && centre < hi) range.setSkewForCentre(centre);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, v}, name, range, initial,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([unit, decimals](float value, int) { return juce::String(value, decimals) + unit; })));
    };
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"synthOn", v}, "Synth", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"synthModel", v}, "Synth layer", synthModelNames(), 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"synthSource", v}, "Synth source", synthSourceNames(), 0));
    number("synthLevel", "Synth level", -36.f, 6.f, -6.f, 0.f, " dB", 1);
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"synthOctave", v}, "Synth octave", -2, 1, 0));
    number("synthTone", "Synth tone", 0.f, 100.f, 50.f, 0.f, " %", 0);
    number("synthCharacter", "Synth character", 0.f, 100.f, 50.f, 0.f, " %", 0);
    number("synthAttack", "Synth attack", 1.f, 2000.f, 10.f, 100.f, " ms", 0);
    number("synthRelease", "Synth release", 10.f, 4000.f, 300.f, 400.f, " ms", 0);
    number("synthGlide", "Synth glide", 0.f, 500.f, 0.f, 60.f, " ms", 0);
    number("synthSensitivity", "Synth tracking threshold", -60.f, -12.f, -40.f, 0.f, " dBFS", 0);
}

struct SynthSettings {
    bool on{false};
    int model{0}, source{0}, octave{0};
    float levelDb{-6.f}, tone{.5f}, character{.5f}, attackMs{10.f}, releaseMs{300.f}, glideMs{0.f}, sensitivityDb{-40.f};
};

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

// The synth voice and its control from tracking and MIDI.
class SynthLayer {
public:
    void bind(juce::AudioProcessorValueTreeState& state) {
        static constexpr std::array<const char*, synthParameterCount> ids{"synthOn", "synthModel", "synthSource", "synthLevel", "synthOctave", "synthTone",
            "synthCharacter", "synthAttack", "synthRelease", "synthGlide", "synthSensitivity"};
        for (size_t i = 0; i < ids.size(); ++i) values[i] = state.getRawParameterValue(ids[i]);
    }
    void prepare(double sampleRate, int maximumBlock) {
        rate = sampleRate;
        tracker.prepare(rate);
        voice.assign(size_t(std::max(1, maximumBlock)), 0.f);
        reset();
    }
    void reset() noexcept {
        tracker.reset();
        midiNotes = {};midiCount = 0;midiVelocity = 0;bend = 0;
        gate = false;midiGate = false;envelope = 0;amplitude = 0;pitch = 0;targetPitch = 0;
        phases = {};sub = 0;svf = {};modulator = 0;strike = 0;noiseLeft = 0;seed = 0x12345678u;pwm = 0;guitarLevel = 0;active = false;onsetLevel = 0;
    }
    SynthSettings read() const noexcept {
        const auto value = [this](size_t i, float fallback) { const float v = values[i] != nullptr ? values[i]->load() : fallback; return std::isfinite(v) ? v : fallback; };
        SynthSettings s;
        s.on = value(0, 0.f) > .5f;
        s.model = juce::jlimit(0, synthModelCount - 1, juce::roundToInt(value(1, 0.f)));
        s.source = juce::jlimit(0, 2, juce::roundToInt(value(2, 0.f)));
        s.levelDb = juce::jlimit(-36.f, 6.f, value(3, -6.f));
        s.octave = juce::jlimit(-2, 1, juce::roundToInt(value(4, 0.f)));
        s.tone = juce::jlimit(0.f, 1.f, value(5, 50.f) * .01f);
        s.character = juce::jlimit(0.f, 1.f, value(6, 50.f) * .01f);
        s.attackMs = juce::jlimit(1.f, 2000.f, value(7, 10.f));
        s.releaseMs = juce::jlimit(10.f, 4000.f, value(8, 300.f));
        s.glideMs = juce::jlimit(0.f, 500.f, value(9, 0.f));
        s.sensitivityDb = juce::jlimit(-60.f, -12.f, value(10, -40.f));
        return s;
    }
    // Host MIDI for this block (applied at the block start; mono, last note priority).
    void handleMidi(const juce::MidiBuffer& midi) noexcept {
        for (const auto metadata : midi) {
            const auto m = metadata.getMessage();
            if (m.isNoteOn()) {
                removeNote(m.getNoteNumber());
                if (midiCount < int(midiNotes.size())) midiNotes[size_t(midiCount++)] = m.getNoteNumber();
                midiVelocity = m.getFloatVelocity();midiOnset = true;
            } else if (m.isNoteOff()) removeNote(m.getNoteNumber());
            else if (m.isPitchWheel()) bend = (m.getPitchWheelValue() - 8192) / 8192.0 * 2.0;
            else if (m.isAllNotesOff() || m.isAllSoundOff()) midiCount = 0;
        }
    }
    // Tracks the clean input (call every block, before any processing).
    void analyse(const juce::AudioBuffer<float>& input) noexcept {
        if (!read().on) { if (active) tracker.reset(); return; }
        tracker.push(input.getReadPointer(0), input.getNumChannels() > 1 ? input.getReadPointer(1) : nullptr, input.getNumSamples());
    }
    // Adds the synth to `output` (after RIG SUM).
    void process(juce::AudioBuffer<float>& output) noexcept {
        const auto s = read();
        const int samples = output.getNumSamples(), channels = output.getNumChannels();
        if (!s.on) {
            if (!active) return;
            // Switched off: the voice releases and the layer goes idle when silent.
        }
        if (samples > int(voice.size())) return;
        active = true;
        // Note source: a held MIDI note wins; otherwise the tracked guitar.
        const bool useMidi = s.source != 1 && midiCount > 0, useGuitar = s.source != 2;
        const double threshold = std::pow(10.0, s.sensitivityDb / 20.0);
        bool onset = false;double hz = 0, velocity = 1;
        if (s.on && useMidi) {
            hz = 440.0 * std::pow(2.0, (midiNotes[size_t(midiCount - 1)] - 69 + bend) / 12.0);velocity = midiVelocity;
            onset = midiOnset || !gate;gate = true;
        } else if (s.on && useGuitar && tracker.rms() > threshold && tracker.hz() > 0 && tracker.clarity() > .8) {
            hz = tracker.hz();
            // A rise of 6 dB over the recent level is a new pick attack.
            onset = !gate || tracker.rms() > onsetLevel * 2.0;
            gate = true;velocity = std::min(1.0, tracker.rms() / .25);
        } else gate = false;
        midiOnset = false;
        onsetLevel = onset ? tracker.rms() : std::max(tracker.rms(), onsetLevel * std::exp(-double(samples) / (.05 * rate)));
        guitarLevel = useMidi ? midiVelocity : std::min(1.0, tracker.rms() / .25);
        if (gate) {
            targetPitch = hz * std::pow(2.0, s.octave);
            if (pitch <= 0 || (onset && s.glideMs <= 0)) pitch = targetPitch;
            if (onset) { strike = 1;noiseLeft = int(rate * .005);if (s.model == 2) envelope = 0; }
            // Seidr follows the playing level; the other layers take it at the onset.
            if (onset || s.model == 1) peak = velocity;
        }
        const double attack = 1.0 - std::exp(-1.0 / (s.attackMs * .001 * rate));
        const double release = std::exp(-6.91 / (s.releaseMs * .001 * rate));   // 60 dB over Release
        const double glide = s.glideMs > 0 ? 1.0 - std::exp(-1.0 / (s.glideMs * .001 * rate)) : 1.0;
        const float level = juce::Decibels::decibelsToGain(s.levelDb);
        for (int n = 0; n < samples; ++n) {
            pitch += (targetPitch - pitch) * glide;
            // Amplitude envelope: attack towards the note level while held, exponential release.
            if (s.model == 2) {   // Mimir Impact is percussive: attack, then decay over Release even while held
                if (gate && strike > .5) { envelope += (peak - envelope) * attack;if (envelope >= peak * .99) strike = 0; }
                else envelope *= release;
            } else if (gate) envelope += (peak - envelope) * attack;
            else envelope *= release;
            voice[size_t(n)] = float(level * envelope * oscillator(s));
        }
        for (int c = 0; c < channels; ++c) output.addFrom(c, 0, voice.data(), samples);
        if (!s.on && envelope < 1.e-5) { active = false;reset(); }
    }
    const PitchTracker& pitchTracker() const noexcept { return tracker; }
    double voicePitch() const noexcept { return pitch; }
    bool sounding() const noexcept { return active && envelope > 1.e-5; }
private:
    static constexpr double twoPi = 6.283185307179586;
    // PolyBLEP-corrected saw at `phase` (0-1) with increment `dt`.
    static double saw(double phase, double dt) noexcept {
        double v = 2.0 * phase - 1.0;
        if (phase < dt) { const double t = phase / dt;v -= t + t - t * t - 1.0; }
        else if (phase > 1.0 - dt) { const double t = (phase - 1.0) / dt;v -= t * t + t + t + 1.0; }
        return v;
    }
    static double pulse(double phase, double dt, double width) noexcept {
        double shifted = phase + (1.0 - width);shifted -= std::floor(shifted);
        return .5 * (saw(phase, dt) - saw(shifted, dt));
    }
    double advance(size_t i, double hz) noexcept { phases[i] += hz / rate;phases[i] -= std::floor(phases[i]);return phases[i]; }
    double lowpass(double x, double hz, double resonance) noexcept {
        const double g = std::tan(3.141592653589793 * std::clamp(hz, 20.0, rate * .45) / rate), k = 2.0 - 1.9 * resonance;
        for (auto& f : svf) {
            const double a1 = 1. / (1. + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const double v3 = x - f[1], v1 = a1 * f[0] + a2 * v3, v2 = f[1] + a2 * f[0] + a3 * v3;
            f[0] = 2. * v1 - f[0];f[1] = 2. * v2 - f[1];x = v2;
        }
        return x;
    }
    double oscillator(const SynthSettings& s) noexcept {
        const double hz = std::max(1.0, pitch), dt = hz / rate;
        switch (s.model) {
            case 0: {   // Dvergr Analog: saws detuned by Character (0-30 cents) plus a sub square; Tone = cutoff 150 Hz-12 kHz.
                const double cents = 30.0 * s.character, ratio = std::pow(2.0, cents / 1200.0);
                const double a = saw(advance(0, hz * ratio), dt * ratio), b = saw(advance(1, hz / ratio), dt / ratio);
                const double subPhase = advance(2, hz * .5), subOsc = pulse(subPhase, dt * .5, .5);
                const double cutoff = 150.0 * std::pow(80.0, s.tone) * (1.0 + 2.0 * envelope);
                return lowpass(.4 * (a + b) + .35 * subOsc, cutoff, .35);
            }
            case 1: {   // Seidr Dynamic: PWM pulse; Tone = base brightness, Character = how far the playing level opens it (0-4 oct).
                pwm += .3 / rate;pwm -= std::floor(pwm);
                const double width = .5 + .4 * std::sin(twoPi * pwm);
                const double osc = .7 * pulse(advance(0, hz), dt, width) + .3 * saw(advance(1, hz * 1.003), dt * 1.003);
                const double cutoff = 200.0 * std::pow(20.0, s.tone) * std::pow(2.0, 4.0 * s.character * guitarLevel);
                return lowpass(osc, cutoff, .2);
            }
            default: {   // Mimir Impact: carrier at the note, modulator at Character's ratio; Tone = peak index (0-8), decaying 4x faster than the level.
                static constexpr std::array<double, 8> ratios{.5, 1., 2., 3., 3.5, 4., 5., 7.};
                const double modulatorRatio = ratios[size_t(std::clamp(int(s.character * 8.0), 0, 7))];
                const double index = 8.0 * s.tone * std::pow(std::max(envelope / std::max(peak, 1.e-6), 0.0), 4.0);
                modulator += hz * modulatorRatio / rate;modulator -= std::floor(modulator);
                const double carrier = std::sin(twoPi * advance(0, hz) + index * std::sin(twoPi * modulator));
                double noise = 0;
                if (noiseLeft > 0) { --noiseLeft;seed = seed * 1664525u + 1013904223u;noise = (double(seed >> 8) / 8388608.0 - 1.0) * .3 * s.tone; }
                return carrier + noise / std::max(envelope, .05);
            }
        }
    }
    void removeNote(int note) noexcept {
        int k = 0;
        for (int i = 0; i < midiCount; ++i) if (midiNotes[size_t(i)] != note) midiNotes[size_t(k++)] = midiNotes[size_t(i)];
        midiCount = k;
    }
    double rate{48000};
    PitchTracker tracker;
    std::array<std::atomic<float>*, synthParameterCount> values{};
    std::vector<float> voice;
    std::array<int, 16> midiNotes{};int midiCount{};double midiVelocity{}, bend{};bool midiOnset{};
    bool gate{}, midiGate{}, active{};
    double envelope{}, amplitude{}, peak{1}, pitch{}, targetPitch{}, sub{}, modulator{}, strike{}, pwm{}, guitarLevel{}, onsetLevel{};
    std::array<double, 3> phases{};
    std::array<std::array<double, 2>, 2> svf{};
    int noiseLeft{};std::uint32_t seed{0x12345678u};
};

} // namespace spectralforge
