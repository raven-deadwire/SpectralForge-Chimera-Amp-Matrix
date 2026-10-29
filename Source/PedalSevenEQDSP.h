#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <complex>

namespace spectralforge {

// Seven EQ (PRE model 46, docs/PRE_EXPANSION_B_1_5.md): an original 7-band
// graphic EQ laid out after the published BOSS GE-7: bands at 100, 200, 400,
// 800 Hz, 1.6, 3.2 kHz (peaking) and 6.4 kHz (shelving), each +-15 dB, and a
// +-15 dB LEVEL. The bandwidth (Q 1.4, about one octave) and the shelf's
// slope are Chimera choices; no hardware response is claimed.
class PedalSevenEQ {
public:
    static constexpr std::array<float, 7> centres{100.f, 200.f, 400.f, 800.f, 1600.f, 3200.f, 6400.f};
    static constexpr float q = 1.4f, rangeDb = 15.f;

    void prepare(double sampleRate) noexcept {
        rate = sampleRate;
        for (auto& g : gains) g.reset(rate, .02);
        level.reset(rate, .02);
        reset();
    }
    void reset() noexcept {
        for (auto& band : state) for (auto& s : band) s = {};
        primed = false;
    }
    // controls: seven band gains (dB), level (dB).
    void process(juce::AudioBuffer<float>& buffer, const std::array<float, 12>& controls) noexcept {
        for (size_t b = 0; b < gains.size(); ++b) gains[b].setTargetValue(juce::jlimit(-rangeDb, rangeDb, controls[b]));
        level.setTargetValue(juce::jlimit(-rangeDb, rangeDb, controls[7]));
        if (!primed) {
            for (auto& g : gains) g.setCurrentAndTargetValue(g.getTargetValue());
            level.setCurrentAndTargetValue(level.getTargetValue());
            primed = true;
        }
        const int channels = std::min(2, buffer.getNumChannels()), samples = buffer.getNumSamples();
        for (int start = 0; start < samples; start += 32) {
            const int length = std::min(32, samples - start);
            for (size_t b = 0; b < gains.size(); ++b) { coefficients[b] = band(int(b), gains[b].getCurrentValue()); gains[b].skip(length); }
            for (int n = start; n < start + length; ++n) {
                const float out = juce::Decibels::decibelsToGain(level.getNextValue());
                for (int c = 0; c < channels; ++c) {
                    float x = buffer.getSample(c, n);
                    for (size_t b = 0; b < gains.size(); ++b) x = tick(coefficients[b], state[b][size_t(c)], x);
                    buffer.setSample(c, n, x * out);
                }
            }
        }
    }
    // Settled response in dB (tests, display).
    double magnitudeDb(const std::array<float, 12>& controls, double hz) const noexcept {
        double db = juce::jlimit(-rangeDb, rangeDb, controls[7]);
        const auto z = std::polar(1., -juce::MathConstants<double>::twoPi * hz / rate);
        for (int b = 0; b < 7; ++b) {
            const auto c = band(b, juce::jlimit(-rangeDb, rangeDb, controls[size_t(b)]));
            db += 20 * std::log10(std::abs((c.b0 + c.b1 * z + c.b2 * z * z) / (1. + c.a1 * z + c.a2 * z * z)));
        }
        return db;
    }

private:
    struct Coefficients { double b0{1}, b1{}, b2{}, a1{}, a2{}; };
    struct State { double z1{}, z2{}; };
    Coefficients band(int index, double gainDb) const noexcept {
        const double hz = std::min(double(centres[size_t(index)]), rate * .45), a = std::pow(10., gainDb / 40.);
        const double w = juce::MathConstants<double>::twoPi * hz / rate, cw = std::cos(w), sw = std::sin(w);
        if (index < 6) {   // RBJ peaking
            const double alpha = sw / (2. * q), a0 = 1. + alpha / a;
            return {(1. + alpha * a) / a0, -2. * cw / a0, (1. - alpha * a) / a0, -2. * cw / a0, (1. - alpha / a) / a0};
        }
        // RBJ high shelf, slope 1, corner at the band frequency.
        const double alpha = sw / 2. * std::sqrt(2.), s = 2. * std::sqrt(a) * alpha, a0 = (a + 1.) - (a - 1.) * cw + s;
        return {a * ((a + 1.) + (a - 1.) * cw + s) / a0, -2. * a * ((a - 1.) + (a + 1.) * cw) / a0, a * ((a + 1.) + (a - 1.) * cw - s) / a0,
                2. * ((a - 1.) - (a + 1.) * cw) / a0, ((a + 1.) - (a - 1.) * cw - s) / a0};
    }
    static float tick(const Coefficients& c, State& s, float x) noexcept {
        const double y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return float(y);
    }
    double rate{48000.};
    bool primed{};
    std::array<juce::SmoothedValue<float>, 7> gains;
    juce::SmoothedValue<float> level;
    std::array<Coefficients, 7> coefficients{};
    std::array<std::array<State, 2>, 7> state{};
};

} // namespace spectralforge
