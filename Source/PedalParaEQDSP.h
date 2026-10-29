#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <complex>

namespace spectralforge {

// Para EQ (PRE model 40, docs/PRE_EXPANSION_A_1_4.md): an original three-band
// parametric EQ with a clean boost, laid out after the published ParaEQ MKII
// controls. Band ranges, +-15 dB and the 30 dB boost follow the public
// specification; the three Q values are Chimera choices (the hardware's are
// not published) and no hardware response is claimed.
class PedalParaEQ {
public:
    struct Band { float low, high; };
    static constexpr std::array<Band, 3> bands{{{35.f, 500.f}, {250.f, 5000.f}, {1000.f, 20000.f}}};
    static constexpr std::array<float, 3> qValues{.6f, 1.4f, 4.f};   // WIDE, MEDIUM, NARROW
    static constexpr float gainRangeDb = 15.f, boostRangeDb = 30.f;

    // Knob position (0-1) to band frequency, logarithmic across the band.
    static float frequency(int band, float position) noexcept {
        const auto& b = bands[size_t(juce::jlimit(0, 2, band))];
        return b.low * std::pow(b.high / b.low, juce::jlimit(0.f, 1.f, position));
    }
    static float q(float choice) noexcept { return qValues[size_t(juce::jlimit(0, 2, juce::roundToInt(choice)))]; }

    void prepare(double sampleRate) noexcept {
        rate = sampleRate;
        for (auto& p : frequencySmooth) p.reset(rate, .02);
        for (auto& p : gainSmooth) p.reset(rate, .02);
        for (auto& p : qSmooth) p.reset(rate, .02);
        boostSmooth.reset(rate, .02);
        reset();
    }
    void reset() noexcept {
        for (auto& band : state) for (auto& channel : band) channel = {};
        primed = false;
    }
    // controls: low freq/gain/q, mid freq/gain/q, high freq/gain/q, boost dB, boost on.
    void process(juce::AudioBuffer<float>& buffer, const std::array<float, 12>& controls) noexcept {
        for (int b = 0; b < 3; ++b) {
            const float hz = std::min(frequency(b, controls[size_t(3 * b)]), float(rate * .45));
            frequencySmooth[size_t(b)].setTargetValue(std::log(hz));
            gainSmooth[size_t(b)].setTargetValue(juce::jlimit(-gainRangeDb, gainRangeDb, controls[size_t(3 * b + 1)]));
            qSmooth[size_t(b)].setTargetValue(std::log(q(controls[size_t(3 * b + 2)])));
        }
        boostSmooth.setTargetValue(controls[10] > .5f ? juce::jlimit(0.f, boostRangeDb, controls[9]) : 0.f);
        if (!primed) {
            for (int b = 0; b < 3; ++b) {
                frequencySmooth[size_t(b)].setCurrentAndTargetValue(frequencySmooth[size_t(b)].getTargetValue());
                gainSmooth[size_t(b)].setCurrentAndTargetValue(gainSmooth[size_t(b)].getTargetValue());
                qSmooth[size_t(b)].setCurrentAndTargetValue(qSmooth[size_t(b)].getTargetValue());
            }
            boostSmooth.setCurrentAndTargetValue(boostSmooth.getTargetValue());
            primed = true;
        }
        const int channels = std::min(2, buffer.getNumChannels()), samples = buffer.getNumSamples();
        // Coefficients follow the smoothed controls every 32 samples.
        for (int start = 0; start < samples; start += updateInterval) {
            const int length = std::min(updateInterval, samples - start);
            for (int b = 0; b < 3; ++b) {
                coefficients[size_t(b)] = peak(std::exp(frequencySmooth[size_t(b)].getCurrentValue()),
                                               std::exp(qSmooth[size_t(b)].getCurrentValue()),
                                               gainSmooth[size_t(b)].getCurrentValue());
                frequencySmooth[size_t(b)].skip(length);gainSmooth[size_t(b)].skip(length);qSmooth[size_t(b)].skip(length);
            }
            for (int n = start; n < start + length; ++n) {
                const float boost = juce::Decibels::decibelsToGain(boostSmooth.getNextValue());
                for (int c = 0; c < channels; ++c) {
                    float x = buffer.getSample(c, n);
                    for (int b = 0; b < 3; ++b) x = tick(coefficients[size_t(b)], state[size_t(b)][size_t(c)], x);
                    buffer.setSample(c, n, x * boost);
                }
            }
        }
    }
    // The analytic response of the settled EQ (tests, display).
    double magnitudeDb(const std::array<float, 12>& controls, double hz) const noexcept {
        double db = controls[10] > .5f ? juce::jlimit(0.f, boostRangeDb, controls[9]) : 0.;
        for (int b = 0; b < 3; ++b) {
            const auto c = peak(std::min(frequency(b, controls[size_t(3 * b)]), float(rate * .45)), q(controls[size_t(3 * b + 2)]), controls[size_t(3 * b + 1)]);
            const auto z = std::polar(1., -juce::MathConstants<double>::twoPi * hz / rate);
            db += 20 * std::log10(std::abs((c.b0 + c.b1 * z + c.b2 * z * z) / (1. + c.a1 * z + c.a2 * z * z)));
        }
        return db;
    }

private:
    struct Coefficients { double b0{1}, b1{}, b2{}, a1{}, a2{}; };
    struct State { double z1{}, z2{}; };
    // RBJ peaking EQ.
    Coefficients peak(double hz, double qValue, double gainDb) const noexcept {
        const double a = std::pow(10., gainDb / 40.), w = juce::MathConstants<double>::twoPi * hz / rate;
        const double alpha = std::sin(w) / (2. * qValue), cw = std::cos(w), a0 = 1. + alpha / a;
        return {(1. + alpha * a) / a0, -2. * cw / a0, (1. - alpha * a) / a0, -2. * cw / a0, (1. - alpha / a) / a0};
    }
    static float tick(const Coefficients& c, State& s, float x) noexcept {
        const double y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return float(y);
    }
    static constexpr int updateInterval = 32;
    double rate{48000.};
    bool primed{};
    std::array<juce::SmoothedValue<float>, 3> frequencySmooth, gainSmooth, qSmooth;
    juce::SmoothedValue<float> boostSmooth;
    std::array<Coefficients, 3> coefficients{};
    std::array<std::array<State, 2>, 3> state{};
};

} // namespace spectralforge
