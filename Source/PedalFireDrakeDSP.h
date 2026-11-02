#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <complex>
#include <memory>

namespace spectralforge {

// Fire Drake (PRE model 50, docs/PRE_EXPANSION_B_1_5.md): an original high-
// gain distortion laid out after the published Wampler Dracarys controls:
// VOLUME, GAIN, an active BASS / MIDS / TREBLE and an OPEN / TIGHT switch
// selecting two clipping modes (OPEN with little compression, TIGHT with more
// compression and sustain). The EQ ranges and shapes, the clipping and how
// TIGHT changes it are Chimera choices; no hardware response is claimed.
// Runs at 4x like the other drives.
class PedalFireDrake {
public:
    enum Mode { open = 0, tight = 1 };
    static constexpr float minimumDriveDb = 12.f, maximumDriveDb = 48.f, rangeDb = 12.f, bassHz = 100.f, midsHz = 800.f, trebleHz = 2500.f, clipOffset = .1f;
    static constexpr std::array<float, 2> preHighPassHz{60.f, 150.f}, secondStageGain{2.f, 8.f};
    static float driveDb(float gain) noexcept { return minimumDriveDb + (maximumDriveDb - minimumDriveDb) * juce::jlimit(0.f, 1.f, gain); }

    void prepare(double sampleRate, int maximumBlock) {
        rate = sampleRate;
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampling->initProcessing(size_t(maximumBlock));
        interstagePole = float(1. - std::exp(-juce::MathConstants<double>::twoPi * 100. / (rate * 4.)));
        drive.reset(rate, .02); output.reset(rate * 4., .02); stageGain.reset(rate * 4., .02);
        reset();
    }
    void reset() noexcept {
        if (oversampling) oversampling->reset();
        for (auto* bank : {&pre, &bass, &mids, &treble, &top}) for (auto& s : *bank) s = {};
        interstage = {};
        primed = false;
    }
    int latency() const noexcept { return oversampling ? juce::roundToInt(oversampling->getLatencyInSamples()) : 0; }

    // controls: volume dB, gain, bass dB, mids dB, treble dB, mode (0 open, 1 tight).
    void process(juce::AudioBuffer<float>& buffer, const std::array<float, 12>& c) noexcept {
        const int mode = juce::jlimit(0, 1, juce::roundToInt(c[5]));
        drive.setTargetValue(juce::Decibels::decibelsToGain(driveDb(c[1])));
        stageGain.setTargetValue(secondStageGain[size_t(mode)]);
        output.setTargetValue(juce::Decibels::decibelsToGain(c[0]));
        if (!primed) { for (auto* s : {&drive, &stageGain, &output}) s->setCurrentAndTargetValue(s->getTargetValue()); primed = true; }
        const int channels = std::min(2, buffer.getNumChannels()), samples = buffer.getNumSamples();
        // Active EQ ahead of the clipping, so it shapes what distorts.
        const auto cut = highPass(preHighPassHz[size_t(mode)]), low = shelf(bassHz, c[2], false), mid = peak(midsHz, .8, c[3]), high = shelf(trebleHz, c[4], true);
        for (int n = 0; n < samples; ++n) {
            const float d = drive.getNextValue();
            for (int ch = 0; ch < channels; ++ch) {
                float x = buffer.getSample(ch, n);
                x = tick(cut, pre[size_t(ch)], x); x = tick(low, bass[size_t(ch)], x); x = tick(mid, mids[size_t(ch)], x); x = tick(high, treble[size_t(ch)], x);
                buffer.setSample(ch, n, d * x);
            }
        }
        // Two clipping stages at 4x; TIGHT drives the second harder (more compression).
        juce::dsp::AudioBlock<float> block(buffer);
        auto sub = block.getSubsetChannelBlock(0, size_t(channels));
        auto up = oversampling->processSamplesUp(sub);
        const float offset = std::tanh(clipOffset);
        for (size_t n = 0; n < up.getNumSamples(); ++n) {
            const float k = stageGain.getNextValue(), g = output.getNextValue();
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch) {
                float x = std::tanh(up.getSample(int(ch), int(n)) + clipOffset) - offset;
                auto& z = interstage[ch]; z += interstagePole * (x - z);
                up.setSample(int(ch), int(n), g / k * std::tanh(k * (x - z)));
            }
        }
        oversampling->processSamplesDown(sub);
        const auto smooth = lowPass(8000.);
        for (int ch = 0; ch < channels; ++ch)
            for (int n = 0; n < samples; ++n) buffer.setSample(ch, n, tick(smooth, top[size_t(ch)], buffer.getSample(ch, n)));
    }
    // The active EQ's response in dB (BASS, MIDS, TREBLE; tests, display).
    double eqDb(const std::array<float, 12>& c, double hz) const noexcept {
        double db = 0;
        for (const auto& f : {shelf(bassHz, c[2], false), peak(midsHz, .8, c[3]), shelf(trebleHz, c[4], true)}) db += 20 * std::log10(std::abs(response(f, hz)));
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
    std::array<State, 2> pre{}, bass{}, mids{}, treble{}, top{};
    std::array<float, 2> interstage{};
    juce::SmoothedValue<float> drive, output, stageGain;
};

} // namespace spectralforge
