#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <complex>

namespace spectralforge {

// Power Wah (PRE model 43, docs/PRE_EXPANSION_A_1_4.md): an original wah and
// clean boost laid out after the published Tel-Ray / Morley Power Wah Boost:
// a treadle wah and a boost (up to 40 dB on the current reissue), each with
// its own switch, usable alone or together. The sweep range, Q, the treadle
// lag standing in for the electro-optical control and the order (wah, then
// boost) are Chimera choices; no hardware response is claimed.
class PedalPowerWah {
public:
    static constexpr float lowHz = 350.f, highHz = 2200.f, q = 4.f, maximumBoostDb = 40.f, lagMs = 25.f;
    static float centre(float position) noexcept { return lowHz * std::pow(highHz / lowHz, juce::jlimit(0.f, 1.f, position)); }

    void prepare(double sampleRate) noexcept {
        rate = sampleRate;
        lag = float(std::exp(-1. / (rate * lagMs * .001)));
        boost.reset(rate, .02);
        wahMix.reset(rate, .015);
        reset();
    }
    void reset() noexcept {
        for (auto& s : state) s = {};
        primed = false;
    }
    // controls: position 0-1, boost dB, wah on, boost on.
    void process(juce::AudioBuffer<float>& buffer, const std::array<float, 12>& controls) noexcept {
        const float target = std::log(centre(controls[0]));
        boost.setTargetValue(controls[3] > .5f ? juce::jlimit(0.f, maximumBoostDb, controls[1]) : 0.f);
        wahMix.setTargetValue(controls[2] > .5f ? 1.f : 0.f);
        if (!primed) {
            logHz = target;
            boost.setCurrentAndTargetValue(boost.getTargetValue());
            wahMix.setCurrentAndTargetValue(wahMix.getTargetValue());
            primed = true;
        }
        const int channels = std::min(2, buffer.getNumChannels());
        for (int n = 0; n < buffer.getNumSamples(); ++n) {
            logHz = lag * logHz + (1.f - lag) * target;   // treadle lag
            if (clock++ % 8 == 0) coefficients = bandPass(std::exp(double(logHz)));
            const float wet = wahMix.getNextValue(), gain = juce::Decibels::decibelsToGain(boost.getNextValue());
            for (int c = 0; c < channels; ++c) {
                const float x = buffer.getSample(c, n);
                const float wah = tick(coefficients, state[size_t(c)], x);
                buffer.setSample(c, n, gain * (wet * wah + (1.f - wet) * x));
            }
        }
        clock %= 8;
    }
    // The lagged treadle's current centre frequency (tests).
    double currentCentre() const noexcept { return std::exp(double(logHz)); }
    // Settled small-signal gain in dB (tests, display).
    double magnitudeDb(const std::array<float, 12>& controls, double hz) const noexcept {
        double db = controls[3] > .5f ? juce::jlimit(0.f, maximumBoostDb, controls[1]) : 0.f;
        if (controls[2] > .5f) {
            const auto c = bandPass(centre(controls[0]));
            const auto z = std::polar(1., -juce::MathConstants<double>::twoPi * hz / rate);
            db += 20 * std::log10(std::abs((c.b0 + c.b1 * z + c.b2 * z * z) / (1. + c.a1 * z + c.a2 * z * z)));
        }
        return db;
    }

private:
    struct Coefficients { double b0{1}, b1{}, b2{}, a1{}, a2{}; };
    struct State { double z1{}, z2{}; };
    // RBJ band-pass, 0 dB peak gain.
    Coefficients bandPass(double hz) const noexcept {
        const double w = juce::MathConstants<double>::twoPi * std::min(hz, rate * .45) / rate, alpha = std::sin(w) / (2. * q), a0 = 1. + alpha;
        return {alpha / a0, 0., -alpha / a0, -2. * std::cos(w) / a0, (1. - alpha) / a0};
    }
    static float tick(const Coefficients& c, State& s, float x) noexcept {
        const double y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return float(y);
    }
    double rate{48000.};
    float lag{}, logHz{};
    int clock{};
    bool primed{};
    Coefficients coefficients{};
    std::array<State, 2> state{};
    juce::SmoothedValue<float> boost, wahMix;
};

} // namespace spectralforge
