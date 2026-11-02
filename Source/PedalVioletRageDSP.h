#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <complex>
#include <memory>

namespace spectralforge {

// Violet Rage (PRE model 49, docs/PRE_EXPANSION_B_1_5.md): an original amp-
// in-a-box distortion laid out after the published Revv G3 controls: VOLUME,
// GAIN, BASS, MIDDLE, TREBLE and a three-position AGGRESSION switch (off,
// red, blue) that adds gain and volume at two intensities. The amounts, the
// clipping stages, the tight pre-filter and the tone-stack shapes are Chimera
// choices; no hardware response is claimed. Runs at 4x like the other drives.
class PedalVioletRage {
public:
    enum Aggression { off = 0, red = 1, blue = 2 };
    static constexpr float minimumDriveDb = 6.f, maximumDriveDb = 40.f, rangeDb = 12.f, bassHz = 100.f, middleHz = 700.f, trebleHz = 3000.f, tightHz = 90.f, clipOffset = .15f;
    static constexpr std::array<float, 3> aggressionGainDb{0.f, 6.f, 12.f}, aggressionVolumeDb{0.f, 2.f, 4.f};
    static float driveDb(float gain, int aggression) noexcept {
        return minimumDriveDb + (maximumDriveDb - minimumDriveDb) * juce::jlimit(0.f, 1.f, gain) + aggressionGainDb[size_t(juce::jlimit(0, 2, aggression))];
    }

    void prepare(double sampleRate, int maximumBlock) {
        rate = sampleRate;
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampling->initProcessing(size_t(maximumBlock));
        interstagePole = float(1. - std::exp(-juce::MathConstants<double>::twoPi * 150. / (rate * 4.)));
        drive.reset(rate, .02); output.reset(rate, .02);
        reset();
    }
    void reset() noexcept {
        if (oversampling) oversampling->reset();
        for (auto* bank : {&tight, &bass, &middle, &treble, &top}) for (auto& s : *bank) s = {};
        interstage = {};
        primed = false;
    }
    int latency() const noexcept { return oversampling ? juce::roundToInt(oversampling->getLatencyInSamples()) : 0; }

    // controls: volume dB, gain, bass dB, middle dB, treble dB, aggression (0 off, 1 red, 2 blue).
    void process(juce::AudioBuffer<float>& buffer, const std::array<float, 12>& c) noexcept {
        const int mode = juce::jlimit(0, 2, juce::roundToInt(c[5]));
        drive.setTargetValue(juce::Decibels::decibelsToGain(driveDb(c[1], mode)));
        output.setTargetValue(juce::Decibels::decibelsToGain(c[0] + aggressionVolumeDb[size_t(mode)]));
        if (!primed) { drive.setCurrentAndTargetValue(drive.getTargetValue()); output.setCurrentAndTargetValue(output.getTargetValue()); primed = true; }
        const int channels = std::min(2, buffer.getNumChannels()), samples = buffer.getNumSamples();
        const auto pre = highPass(tightHz);
        for (int n = 0; n < samples; ++n) {
            const float d = drive.getNextValue();
            for (int ch = 0; ch < channels; ++ch) buffer.setSample(ch, n, d * tick(pre, tight[size_t(ch)], buffer.getSample(ch, n)));
        }
        // Two clipping stages at 4x with a 150 Hz coupling between them.
        juce::dsp::AudioBlock<float> block(buffer);
        auto sub = block.getSubsetChannelBlock(0, size_t(channels));
        auto up = oversampling->processSamplesUp(sub);
        const float offset = std::tanh(clipOffset);
        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            for (size_t n = 0; n < up.getNumSamples(); ++n) {
                float x = std::tanh(up.getSample(int(ch), int(n)) + clipOffset) - offset;
                auto& z = interstage[ch]; z += interstagePole * (x - z);
                up.setSample(int(ch), int(n), std::tanh(3.f * (x - z)));
            }
        oversampling->processSamplesDown(sub);
        const auto low = shelf(bassHz, c[2], false), mid = peak(middleHz, .7, c[3]), high = shelf(trebleHz, c[4], true), cut = lowPass(7500.);
        for (int n = 0; n < samples; ++n) {
            const float g = output.getNextValue();
            for (int ch = 0; ch < channels; ++ch) {
                float x = buffer.getSample(ch, n);
                x = tick(low, bass[size_t(ch)], x); x = tick(mid, middle[size_t(ch)], x); x = tick(high, treble[size_t(ch)], x); x = tick(cut, top[size_t(ch)], x);
                buffer.setSample(ch, n, g * x);
            }
        }
    }
    // The tone stack's response in dB (BASS, MIDDLE, TREBLE; tests, display).
    double toneStackDb(const std::array<float, 12>& c, double hz) const noexcept {
        double db = 0;
        for (const auto& f : {shelf(bassHz, c[2], false), peak(middleHz, .7, c[3]), shelf(trebleHz, c[4], true)}) db += 20 * std::log10(std::abs(response(f, hz)));
        return db;
    }

private:
    struct Coefficients { double b0{1}, b1{}, b2{}, a1{}, a2{}; };
    struct State { double z1{}, z2{}; };
    Coefficients highPass(double hz) const noexcept {
        const double w = juce::MathConstants<double>::twoPi * hz / rate, alpha = std::sin(w) / (2. * .707), cw = std::cos(w), a0 = 1. + alpha;
        return {(1. + cw) / 2. / a0, -(1. + cw) / a0, (1. + cw) / 2. / a0, -2. * cw / a0, (1. - alpha) / a0};
    }
    Coefficients lowPass(double hz) const noexcept {
        const double w = juce::MathConstants<double>::twoPi * std::min(hz, rate * .45) / rate, alpha = std::sin(w) / (2. * .707), cw = std::cos(w), a0 = 1. + alpha;
        return {(1. - cw) / 2. / a0, (1. - cw) / a0, (1. - cw) / 2. / a0, -2. * cw / a0, (1. - alpha) / a0};
    }
    Coefficients peak(double hz, double q, double gainDb) const noexcept {
        const double a = std::pow(10., juce::jlimit(-double(rangeDb), double(rangeDb), gainDb) / 40.), w = juce::MathConstants<double>::twoPi * hz / rate;
        const double alpha = std::sin(w) / (2. * q), cw = std::cos(w), a0 = 1. + alpha / a;
        return {(1. + alpha * a) / a0, -2. * cw / a0, (1. - alpha * a) / a0, -2. * cw / a0, (1. - alpha / a) / a0};
    }
    // RBJ shelf, slope 1.
    Coefficients shelf(double hz, double gainDb, bool highShelf) const noexcept {
        const double a = std::pow(10., juce::jlimit(-double(rangeDb), double(rangeDb), gainDb) / 40.), w = juce::MathConstants<double>::twoPi * hz / rate, cw = std::cos(w), sw = std::sin(w);
        const double alpha = sw / 2. * std::sqrt(2.), s = 2. * std::sqrt(a) * alpha, sign = highShelf ? 1. : -1.;
        const double a0 = (a + 1.) - sign * (a - 1.) * cw + s;
        return {a * ((a + 1.) + sign * (a - 1.) * cw + s) / a0, -sign * 2. * a * ((a - 1.) + sign * (a + 1.) * cw) / a0,
                a * ((a + 1.) + sign * (a - 1.) * cw - s) / a0, sign * 2. * ((a - 1.) - sign * (a + 1.) * cw) / a0, ((a + 1.) - sign * (a - 1.) * cw - s) / a0};
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
    double rate{48000.};
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    float interstagePole{};
    bool primed{};
    std::array<State, 2> tight{}, bass{}, middle{}, treble{}, top{};
    std::array<float, 2> interstage{};
    juce::SmoothedValue<float> drive, output;
};

} // namespace spectralforge
