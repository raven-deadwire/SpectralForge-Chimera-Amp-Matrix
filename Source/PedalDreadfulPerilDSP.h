#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <complex>
#include <memory>

namespace spectralforge {

// Dreadful Peril (PRE model 45, docs/PRE_EXPANSION_A_1_4.md): an original
// high-gain distortion laid out after the published Empress Heavy Menace
// controls: GAIN, WEIGHT (a high-pass inside the distortion), OUTPUT, LOW,
// MID with a 200 Hz-2.5 kHz MID FREQ, HIGH, three modes (Lite / Heavy /
// Heavier), an adaptive gate with its own threshold and switch, and a 35 Hz-
// 17 kHz response. The clipping stages, ranges, filter shapes and gate timing
// are Chimera choices; no hardware response is claimed.
class PedalDreadfulPeril {
public:
    enum Mode { lite = 0, heavy = 1, heavier = 2 };
    static constexpr float minimumDriveDb = 6.f, maximumDriveDb = 42.f;
    static float weightHz(float knob) noexcept { return 300.f * std::pow(30.f / 300.f, juce::jlimit(0.f, 1.f, knob)); }   // more weight, lower cut
    static float midHz(float knob) noexcept { return 200.f * std::pow(2500.f / 200.f, juce::jlimit(0.f, 1.f, knob)); }
    static float driveDb(float gain) noexcept { return minimumDriveDb + (maximumDriveDb - minimumDriveDb) * juce::jlimit(0.f, 1.f, gain); }
    static int stages(int mode) noexcept { return 1 + juce::jlimit(0, 2, mode); }

    void prepare(double sampleRate, int maximumBlock) {
        rate = sampleRate;
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampling->initProcessing(size_t(maximumBlock));
        interstagePole = float(1. - std::exp(-juce::MathConstants<double>::twoPi * 120. / (rate * 4.)));
        gateAttack = float(std::exp(-1. / (rate * .0005))); gateRelease = float(std::exp(-1. / (rate * .04)));
        detectorAttack = float(std::exp(-1. / (rate * .001))); detectorRelease = float(std::exp(-1. / (rate * .03)));
        holdSamples = int(rate * .01);
        reset();
    }
    void reset() noexcept {
        if (oversampling) oversampling->reset();
        for (auto* bank : {&pre, &low, &mid, &high, &bandLow, &bandHigh}) for (auto& s : *bank) s = {};
        interstage = {};
        gateGain = 0.f; gateEnvelope = 0.f; gateOpen = false; hold = 0;
    }
    int latency() const noexcept { return oversampling ? juce::roundToInt(oversampling->getLatencyInSamples()) : 0; }

    // controls: gain, weight, output dB, low dB, mid dB, high dB, gate threshold dB, mid freq, mode, gate on.
    void process(juce::AudioBuffer<float>& buffer, const std::array<float, 12>& c) noexcept {
        const int channels = std::min(2, buffer.getNumChannels()), samples = buffer.getNumSamples();
        const float threshold = c[6], drive = juce::Decibels::decibelsToGain(driveDb(c[0]));
        const int mode = juce::jlimit(0, 2, juce::roundToInt(c[8]));
        const bool gateOn = c[9] > .5f;
        const auto preHP = highPass(weightHz(c[1]), .707), lowShelf = shelf(100., c[3], false), midPeak = peak(midHz(c[7]), .8, c[4]);
        const auto highShelf = shelf(3000., c[5], true), bandHP = highPass(35., .707), bandLP = lowPass(17000., .707);
        const float output = juce::Decibels::decibelsToGain(c[2]);
        // Gate on the input (adaptive: the open threshold follows the setting,
        // it closes 6 dB lower after a 10 ms hold, so decaying notes do not chatter).
        for (int n = 0; n < samples; ++n) {
            float level = 0.f;
            for (int ch = 0; ch < channels; ++ch) level = std::max(level, std::abs(buffer.getSample(ch, n)));
            gateEnvelope = level > gateEnvelope ? detectorAttack * gateEnvelope + (1.f - detectorAttack) * level : detectorRelease * gateEnvelope + (1.f - detectorRelease) * level;
            const float db = juce::Decibels::gainToDecibels(gateEnvelope, -150.f);
            if (db > threshold) { gateOpen = true; hold = holdSamples; }
            else if (db < threshold - 6.f) { if (hold > 0) --hold; else gateOpen = false; }
            const float target = !gateOn || gateOpen ? 1.f : 0.f;
            gateGain = target > gateGain ? gateAttack * gateGain + (1.f - gateAttack) * target : gateRelease * gateGain + (1.f - gateRelease) * target;
            for (int ch = 0; ch < channels; ++ch)
                buffer.setSample(ch, n, gateGain * tick(preHP, pre[size_t(ch)], buffer.getSample(ch, n)) * drive);
        }
        // Clipping stages at 4x, a 120 Hz interstage coupling between them.
        juce::dsp::AudioBlock<float> block(buffer);
        auto sub = block.getSubsetChannelBlock(0, size_t(channels));
        auto up = oversampling->processSamplesUp(sub);
        const int count = stages(mode);
        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            for (size_t n = 0; n < up.getNumSamples(); ++n) {
                float x = up.getSample(int(ch), int(n));
                for (int s = 0; s < count; ++s) {
                    if (s > 0) { auto& z = interstage[size_t(s - 1)][ch]; z += interstagePole * (x - z); x = (x - z) * 4.f; }
                    x = clip(x, s);
                }
                up.setSample(int(ch), int(n), x);
            }
        oversampling->processSamplesDown(sub);
        // Tone stack, band limits and output at the base rate.
        for (int ch = 0; ch < channels; ++ch)
            for (int n = 0; n < samples; ++n) {
                float x = buffer.getSample(ch, n);
                x = tick(lowShelf, low[size_t(ch)], x); x = tick(midPeak, mid[size_t(ch)], x); x = tick(highShelf, high[size_t(ch)], x);
                x = tick(bandHP, bandLow[size_t(ch)], x); x = tick(bandLP, bandHigh[size_t(ch)], x);
                buffer.setSample(ch, n, x * output);
            }
    }
    // Small-signal gain in dB in LITE mode with the gate open (tests): the
    // single soft stage has slope 1 - tanh^2(0.1) at zero, so the linear path
    // is the filters, the drive and that slope.
    double smallSignalDb(const std::array<float, 12>& c, double hz) const noexcept {
        const double slope = 1. - std::tanh(.1) * std::tanh(.1);
        double db = driveDb(c[0]) + c[2] + 20 * std::log10(slope);
        for (const auto& f : {highPass(weightHz(c[1]), .707), shelf(100., c[3], false), peak(midHz(c[7]), .8, c[4]), shelf(3000., c[5], true), highPass(35., .707), lowPass(17000., .707)})
            db += 20 * std::log10(std::abs(response(f, hz)));
        return db;
    }

private:
    struct Coefficients { double b0{1}, b1{}, b2{}, a1{}, a2{}; };
    struct State { double z1{}, z2{}; };
    // Stage 1 soft and slightly asymmetric; later stages harder.
    static float clip(float x, int stage) noexcept {
        if (stage == 0) return std::tanh(x + .1f) - std::tanh(.1f);
        // Cubic soft clip, flat (slope 0) at +-1 and held there beyond.
        const float y = juce::jlimit(-1.f, 1.f, x);
        return 1.5f * y - .5f * y * y * y;
    }
    Coefficients highPass(double hz, double q) const noexcept {
        const double w = juce::MathConstants<double>::twoPi * hz / rate, alpha = std::sin(w) / (2. * q), cw = std::cos(w), a0 = 1. + alpha;
        return {(1. + cw) / 2. / a0, -(1. + cw) / a0, (1. + cw) / 2. / a0, -2. * cw / a0, (1. - alpha) / a0};
    }
    Coefficients lowPass(double hz, double q) const noexcept {
        const double w = juce::MathConstants<double>::twoPi * std::min(hz, rate * .45) / rate, alpha = std::sin(w) / (2. * q), cw = std::cos(w), a0 = 1. + alpha;
        return {(1. - cw) / 2. / a0, (1. - cw) / a0, (1. - cw) / 2. / a0, -2. * cw / a0, (1. - alpha) / a0};
    }
    Coefficients peak(double hz, double q, double gainDb) const noexcept {
        const double a = std::pow(10., gainDb / 40.), w = juce::MathConstants<double>::twoPi * hz / rate;
        const double alpha = std::sin(w) / (2. * q), cw = std::cos(w), a0 = 1. + alpha / a;
        return {(1. + alpha * a) / a0, -2. * cw / a0, (1. - alpha * a) / a0, -2. * cw / a0, (1. - alpha / a) / a0};
    }
    // RBJ shelf, slope 1.
    Coefficients shelf(double hz, double gainDb, bool highShelf) const noexcept {
        const double a = std::pow(10., gainDb / 40.), w = juce::MathConstants<double>::twoPi * hz / rate, cw = std::cos(w), sw = std::sin(w);
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
    float interstagePole{}, gateAttack{}, gateRelease{}, detectorAttack{}, detectorRelease{}, gateGain{}, gateEnvelope{};
    bool gateOpen{};
    int hold{}, holdSamples{};
    std::array<State, 2> pre{}, low{}, mid{}, high{}, bandLow{}, bandHigh{};
    std::array<std::array<float, 2>, 2> interstage{};
};

} // namespace spectralforge
