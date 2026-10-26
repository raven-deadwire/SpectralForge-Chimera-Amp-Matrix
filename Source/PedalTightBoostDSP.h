#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <complex>
#include <memory>

namespace spectralforge {

// Tight Boost (PRE model 42, docs/PRE_EXPANSION_A_1_4.md): an original
// single-knob boost laid out after the published Fortin 33: up to +22 dB,
// with more midrange focus, a tighter low end and added harmonics as it is
// turned up. The low-cut and mid curves and the saturation stage are Chimera
// choices (the hardware's are not published); no hardware response is claimed.
class PedalTightBoost {
public:
    static constexpr float maximumDb = 22.f, midHz = 1000.f, midQ = .8f, maximumMidDb = 4.f;
    // Low cut moves from 60 Hz (knob 0) to 220 Hz (knob full).
    static float lowCutHz(float boostDb) noexcept { return 60.f * std::pow(220.f / 60.f, juce::jlimit(0.f, 1.f, boostDb / maximumDb)); }
    static float midDb(float boostDb) noexcept { return maximumMidDb * juce::jlimit(0.f, 1.f, boostDb / maximumDb); }

    void prepare(double sampleRate, int maximumBlock) {
        rate = sampleRate;
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampling->initProcessing(size_t(maximumBlock));
        boost.reset(rate, .02);
        reset();
    }
    void reset() noexcept {
        if (oversampling) oversampling->reset();
        for (auto& s : lowState) s = {};
        for (auto& s : midState) s = {};
        primed = false;
    }
    int latency() const noexcept { return oversampling ? juce::roundToInt(oversampling->getLatencyInSamples()) : 0; }

    void process(juce::AudioBuffer<float>& buffer, float boostDb) noexcept {
        boost.setTargetValue(juce::jlimit(0.f, maximumDb, boostDb));
        if (!primed) { boost.setCurrentAndTargetValue(boost.getTargetValue()); primed = true; }
        const int channels = std::min(2, buffer.getNumChannels()), samples = buffer.getNumSamples();
        // Filters and gain at the base rate, coefficients every 32 samples.
        for (int start = 0; start < samples; start += 32) {
            const int length = std::min(32, samples - start);
            const float db = boost.getCurrentValue();
            const auto low = highPass(lowCutHz(db)), mid = peak(midHz, midQ, midDb(db));
            boost.skip(length);
            // The mid bell is part of the boost: the knob's dB is the gain at 1 kHz.
            const float flat = juce::Decibels::decibelsToGain(db - midDb(db)) / float(std::abs(response(low, midHz)));
            for (int c = 0; c < channels; ++c)
                for (int n = start; n < start + length; ++n) {
                    float x = buffer.getSample(c, n);
                    x = tick(low, lowState[size_t(c)], x);
                    x = tick(mid, midState[size_t(c)], x);
                    buffer.setSample(c, n, x * flat);
                }
        }
        // Soft saturation at 4x: transparent at low level, harmonics when hot.
        juce::dsp::AudioBlock<float> block(buffer);
        auto sub = block.getSubsetChannelBlock(0, size_t(channels));
        auto up = oversampling->processSamplesUp(sub);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
            for (size_t n = 0; n < up.getNumSamples(); ++n) up.setSample(int(c), int(n), saturate(up.getSample(int(c), int(n))));
        oversampling->processSamplesDown(sub);
    }
    // Small-signal gain in dB at `hz` (tests, display).
    double smallSignalDb(float boostDb, double hz) const noexcept {
        const auto low = highPass(lowCutHz(boostDb)), mid = peak(midHz, midQ, midDb(boostDb));
        const double flat = boostDb - midDb(boostDb) - 20 * std::log10(std::abs(response(low, midHz)));
        return flat + 20 * std::log10(std::abs(response(low, hz)) * std::abs(response(mid, hz)));
    }

private:
    struct Coefficients { double b0{1}, b1{}, b2{}, a1{}, a2{}; };
    struct State { double z1{}, z2{}; };
    Coefficients highPass(double hz) const noexcept {
        const double w = juce::MathConstants<double>::twoPi * hz / rate, alpha = std::sin(w) / (2. * .707), cw = std::cos(w), a0 = 1. + alpha;
        return {(1. + cw) / 2. / a0, -(1. + cw) / a0, (1. + cw) / 2. / a0, -2. * cw / a0, (1. - alpha) / a0};
    }
    Coefficients peak(double hz, double q, double gainDb) const noexcept {
        const double a = std::pow(10., gainDb / 40.), w = juce::MathConstants<double>::twoPi * hz / rate;
        const double alpha = std::sin(w) / (2. * q), cw = std::cos(w), a0 = 1. + alpha / a;
        return {(1. + alpha * a) / a0, -2. * cw / a0, (1. - alpha * a) / a0, -2. * cw / a0, (1. - alpha / a) / a0};
    }
    std::complex<double> response(const Coefficients& c, double hz) const noexcept {
        const auto z = std::polar(1., -juce::MathConstants<double>::twoPi * hz / rate);
        return (c.b0 + c.b1 * z + c.b2 * z * z) / (1. + c.a1 * z + c.a2 * z * z);
    }
    static float tick(const Coefficients& c, State& s, float x) noexcept {
        const double y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return float(y);
    }
    // Linear below 0.5; above it a soft knee towards 1.0, slightly asymmetric.
    static float saturate(float x) noexcept {
        const float knee = .5f, a = std::abs(x);
        if (a <= knee) return x;
        const float limit = x > 0 ? 1.f : .9f, over = (a - knee) / (limit - knee);
        const float y = knee + (limit - knee) * std::tanh(over);
        return x > 0 ? y : -y;
    }
    double rate{48000.};
    bool primed{};
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::SmoothedValue<float> boost;
    std::array<State, 2> lowState{}, midState{};
};

} // namespace spectralforge
