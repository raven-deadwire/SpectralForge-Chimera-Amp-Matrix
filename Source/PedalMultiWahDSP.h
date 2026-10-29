#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <complex>

namespace spectralforge {

// Multi Wah (PRE model 47, docs/PRE_EXPANSION_B_1_5.md): an original wah laid
// out after the published Dunlop Cry Baby 535Q: a six-position RANGE with the
// published sweep ranges, a Q control for the band-pass width, 15 dB filter
// gain at maximum Q and a switchable boost up to 16 dB. The filter shape, Q
// span and the gain-versus-Q law are Chimera choices; no hardware response is
// claimed.
class PedalMultiWah {
public:
    struct Range { float low, high; };
    // As published, position 1 (full CCW) to 6. Position 5's low end is listed
    // as 395 Hz (out of sequence; kept as published).
    static constexpr std::array<Range, 6> ranges{{{440.f, 2200.f}, {400.f, 1900.f}, {375.f, 1800.f}, {345.f, 1600.f}, {395.f, 1400.f}, {250.f, 1200.f}}};
    static constexpr float minimumQ = 2.f, maximumQ = 10.f, minimumPeakDb = 6.f, maximumPeakDb = 15.f, maximumBoostDb = 16.f;
    static float centre(int range, float position) noexcept {
        const auto& r = ranges[size_t(juce::jlimit(0, 5, range))];
        return r.low * std::pow(r.high / r.low, juce::jlimit(0.f, 1.f, position));
    }
    static float q(float knob) noexcept { return minimumQ * std::pow(maximumQ / minimumQ, juce::jlimit(0.f, 1.f, knob)); }
    static float peakDb(float knob) noexcept { return minimumPeakDb + (maximumPeakDb - minimumPeakDb) * juce::jlimit(0.f, 1.f, knob); }

    void prepare(double sampleRate) noexcept {
        rate = sampleRate;
        for (auto* s : {&logHz, &qKnob, &boost}) s->reset(rate, .01);
        reset();
    }
    void reset() noexcept {
        for (auto& s : state) s = {};
        primed = false;
        clock = 0;
    }
    // controls: position, range (0-5), q (0-1), boost dB, boost on.
    void process(juce::AudioBuffer<float>& buffer, const std::array<float, 12>& c) noexcept {
        const int range = juce::jlimit(0, 5, juce::roundToInt(c[1]));
        logHz.setTargetValue(std::log(centre(range, c[0])));
        qKnob.setTargetValue(juce::jlimit(0.f, 1.f, c[2]));
        boost.setTargetValue(c[4] > .5f ? juce::jlimit(0.f, maximumBoostDb, c[3]) : 0.f);
        if (!primed) {
            for (auto* s : {&logHz, &qKnob, &boost}) s->setCurrentAndTargetValue(s->getTargetValue());
            primed = true;
        }
        const int channels = std::min(2, buffer.getNumChannels());
        for (int n = 0; n < buffer.getNumSamples(); ++n) {
            const float hz = std::exp(logHz.getNextValue()), k = qKnob.getNextValue();
            if (clock++ % 8 == 0) coefficients = bandPass(hz, q(k), peakDb(k));
            const float gain = juce::Decibels::decibelsToGain(boost.getNextValue());
            for (int ch = 0; ch < channels; ++ch) buffer.setSample(ch, n, gain * tick(coefficients, state[size_t(ch)], buffer.getSample(ch, n)));
        }
        clock %= 8;
    }
    // Settled small-signal gain in dB (tests, display).
    double magnitudeDb(const std::array<float, 12>& c, double hz) const noexcept {
        const int range = juce::jlimit(0, 5, juce::roundToInt(c[1]));
        const float k = juce::jlimit(0.f, 1.f, c[2]);
        const auto f = bandPass(centre(range, c[0]), q(k), peakDb(k));
        const auto z = std::polar(1., -juce::MathConstants<double>::twoPi * hz / rate);
        return (c[4] > .5f ? juce::jlimit(0.f, maximumBoostDb, c[3]) : 0.f)
             + 20 * std::log10(std::abs((f.b0 + f.b1 * z + f.b2 * z * z) / (1. + f.a1 * z + f.a2 * z * z)));
    }

private:
    struct Coefficients { double b0{1}, b1{}, b2{}, a1{}, a2{}; };
    struct State { double z1{}, z2{}; };
    // RBJ band-pass (0 dB peak) scaled to the Q-dependent filter gain.
    Coefficients bandPass(double hz, double qValue, double peak) const noexcept {
        const double w = juce::MathConstants<double>::twoPi * std::min(hz, rate * .45) / rate, alpha = std::sin(w) / (2. * qValue), a0 = 1. + alpha;
        const double g = std::pow(10., peak / 20.);
        return {g * alpha / a0, 0., -g * alpha / a0, -2. * std::cos(w) / a0, (1. - alpha) / a0};
    }
    static float tick(const Coefficients& c, State& s, float x) noexcept {
        const double y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return float(y);
    }
    double rate{48000.};
    bool primed{};
    int clock{};
    Coefficients coefficients{};
    std::array<State, 2> state{};
    juce::SmoothedValue<float> logHz, qKnob, boost;
};

} // namespace spectralforge
