#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

namespace spectralforge {

// Bass Synth (PRE model 44, docs/PRE_EXPANSION_A_1_4.md): an original
// monophonic tracking synth laid out after the published MXR Bass Synth
// controls (MIX, SUB MIX, CUTOFF, RESONANCE, ENVELOPE, MOD, SHAPE
// triangle/saw/square, VOICE extra tuned oscillators). How the hardware
// tracks, its filter and its ranges are not published: the detector, ranges
// and curves here are Chimera choices, and no hardware response is claimed.
// Single notes only; it adds no latency, its tracking follows about one
// period behind a note.
class PedalBassSynth {
public:
    enum Shape { triangle = 0, saw = 1, square = 2 };
    static constexpr float minimumHz = 30.f, maximumHz = 1000.f, gateDb = -50.f, lfoHz = 5.f;
    static float cutoffHz(float knob) noexcept { return 100.f * std::pow(40.f, juce::jlimit(0.f, 1.f, knob)); }   // 100 Hz - 4 kHz
    static float resonanceQ(float knob) noexcept { return .7f * std::pow(8.f / .7f, juce::jlimit(0.f, 1.f, knob)); }
    static float envelopeSeconds(float knob) noexcept { return .03f * std::pow(1.f / .03f, juce::jlimit(0.f, 1.f, knob)); } // 30 ms - 1 s

    void prepare(double sampleRate) noexcept {
        rate = sampleRate;
        detectorPole = float(1. - std::exp(-juce::MathConstants<double>::twoPi * 600. / rate));
        dcPole = float(1. - std::exp(-juce::MathConstants<double>::twoPi * 20. / rate));
        fastAttack = float(std::exp(-1. / (rate * .002))); fastRelease = float(std::exp(-1. / (rate * .08)));
        slowPole = float(std::exp(-1. / (rate * .05)));
        mix.reset(rate, .02); subMix.reset(rate, .02); gate.reset(rate, .01);
        reset();
    }
    void reset() noexcept {
        lowA = lowB = dc = 0.f; armed = false; lastCross = -1.; sampleClock = 0;
        candidate = 0.f; hz = 0.f; envelope = slowEnvelope = filterEnvelope = 0.f;
        phase = subPhase = octavePhase = fifthPhase = lfoPhase = 0.;
        svfIc1 = svfIc2 = 0.f; previous = 0.f; primed = false;
        gate.setCurrentAndTargetValue(0.f);
    }
    // controls: mix, sub mix, cutoff, resonance, envelope, mod, shape, voice.
    void process(juce::AudioBuffer<float>& buffer, const std::array<float, 12>& controls) noexcept {
        mix.setTargetValue(juce::jlimit(0.f, 1.f, controls[0]));
        subMix.setTargetValue(juce::jlimit(0.f, 1.f, controls[1]));
        if (!primed) { mix.setCurrentAndTargetValue(mix.getTargetValue()); subMix.setCurrentAndTargetValue(subMix.getTargetValue()); primed = true; }
        const float baseCutoff = cutoffHz(controls[2]), q = resonanceQ(controls[3]);
        const float envelopeDecay = float(std::exp(-1. / (rate * envelopeSeconds(controls[4]))));
        const float mod = juce::jlimit(0.f, 1.f, controls[5]);
        const int shape = juce::jlimit(0, 2, juce::roundToInt(controls[6]));
        const bool voice = controls[7] > .5f;
        const int channels = std::min(2, buffer.getNumChannels());
        for (int n = 0; n < buffer.getNumSamples(); ++n, ++sampleClock) {
            float in = 0.f;
            for (int c = 0; c < channels; ++c) in += buffer.getSample(c, n);
            in /= float(std::max(1, channels));
            track(in);
            // Note onsets (a fast rise over the slow level) restart the filter sweep.
            const float level = std::abs(in);
            envelope = level > envelope ? fastAttack * envelope + (1.f - fastAttack) * level : fastRelease * envelope + (1.f - fastRelease) * level;
            slowEnvelope = slowPole * slowEnvelope + (1.f - slowPole) * envelope;
            if (envelope > 2.f * slowEnvelope + 1e-4f) filterEnvelope = 1.f;
            else filterEnvelope *= envelopeDecay;
            gate.setTargetValue(hz > 0.f && juce::Decibels::gainToDecibels(envelope, -120.f) > gateDb ? 1.f : 0.f);
            const float open = gate.getNextValue();
            float wet = 0.f, sub = 0.f;
            if (hz > 0.f) {
                const double dt = hz / rate;
                wet = oscillator(phase, dt, shape);
                if (voice) wet = (wet + .5f * oscillator(octavePhase, 2. * dt, shape) + .35f * oscillator(fifthPhase, 1.5 * dt, shape)) / 1.85f;
                sub = subPhase < .5 ? 1.f : -1.f;
                advance(phase, dt); advance(subPhase, .5 * dt); advance(octavePhase, 2. * dt); advance(fifthPhase, 1.5 * dt);
            }
            const float lfo = float(1. - 4. * std::abs(lfoPhase - .5)); advance(lfoPhase, lfoHz / rate);
            const float cutoff = juce::jlimit(30.f, float(rate * .45), baseCutoff * (1.f + 8.f * filterEnvelope) * std::pow(2.f, mod * lfo));
            if (sampleClock % 8 == 0) { svfG = float(std::tan(juce::MathConstants<double>::pi * cutoff / rate)); svfK = 1.f / q; }
            const float filtered = lowPass(wet);
            const float synth = open * envelope * (filtered + subMix.getNextValue() * sub);
            const float m = mix.getNextValue();
            for (int c = 0; c < channels; ++c) buffer.setSample(c, n, (1.f - m) * buffer.getSample(c, n) + m * synth);
        }
    }
    // The tracked fundamental (0 while no note is held).
    float trackedHz() const noexcept { return hz; }

private:
    // Low-passed, DC-free input; a positive-going crossing past a hysteresis
    // band measures one period (sub-sample interpolated). A jump is accepted
    // only when two consecutive periods agree, which rejects octave errors.
    void track(float in) noexcept {
        lowA += detectorPole * (in - lowA); lowB += detectorPole * (lowA - lowB);
        dc += dcPole * (lowB - dc);
        const float x = lowB - dc, threshold = .3f * envelope;
        if (x < -threshold) armed = true;
        if (armed && x >= threshold && previous < threshold && envelope > 1e-4f) {
            const double fraction = (threshold - previous) / double(x - previous);
            const double crossing = double(sampleClock) - 1. + fraction;
            if (lastCross >= 0.) {
                const float estimate = float(rate / (crossing - lastCross));
                if (estimate >= minimumHz && estimate <= maximumHz) {
                    if (hz > 0.f && std::abs(estimate / hz - 1.f) < .06f) hz += .5f * (estimate - hz);
                    else if (candidate > 0.f && std::abs(estimate / candidate - 1.f) < .05f) hz = estimate;
                    candidate = estimate;
                }
            }
            lastCross = crossing; armed = false;
        }
        previous = x;
        if (juce::Decibels::gainToDecibels(envelope, -120.f) < gateDb - 6.f) { hz = 0.f; candidate = 0.f; lastCross = -1.; }
    }
    static double polyBlep(double t, double dt) noexcept {
        if (t < dt) { t /= dt; return t + t - t * t - 1.; }
        if (t > 1. - dt) { t = (t - 1.) / dt; return t * t + t + t + 1.; }
        return 0.;
    }
    static float oscillator(double p, double dt, int shape) noexcept {
        if (shape == triangle) return float(1. - 4. * std::abs(p - .5));
        if (shape == saw) return float(2. * p - 1. - polyBlep(p, dt));
        double y = p < .5 ? 1. : -1.;
        y += polyBlep(p, dt);
        double q = p + .5; if (q >= 1.) q -= 1.;
        y -= polyBlep(q, dt);
        return float(y);
    }
    static void advance(double& p, double dt) noexcept { p += dt; if (p >= 1.) p -= std::floor(p); }
    // TPT state-variable low-pass (stable under modulation).
    float lowPass(float x) noexcept {
        const float g = svfG, k = svfK, a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - svfIc2, v1 = a1 * svfIc1 + a2 * v3, v2 = svfIc2 + a2 * svfIc1 + a3 * v3;
        svfIc1 = 2.f * v1 - svfIc1; svfIc2 = 2.f * v2 - svfIc2;
        return v2;
    }
    double rate{48000.};
    float detectorPole{}, dcPole{}, fastAttack{}, fastRelease{}, slowPole{};
    float lowA{}, lowB{}, dc{}, previous{}, candidate{}, hz{}, envelope{}, slowEnvelope{}, filterEnvelope{};
    float svfG{.1f}, svfK{1.f}, svfIc1{}, svfIc2{};
    bool armed{}, primed{};
    double lastCross{-1.}, phase{}, subPhase{}, octavePhase{}, fifthPhase{}, lfoPhase{};
    long long sampleClock{};
    juce::SmoothedValue<float> mix, subMix, gate;
};

} // namespace spectralforge
