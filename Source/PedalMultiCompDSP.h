#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

namespace spectralforge {

// Multi Comp (PRE model 41, docs/PRE_EXPANSION_A_1_4.md): an original bass
// compressor laid out after the published EBS MultiComp controls. COMP ratio
// 1:1-5:1, GAIN 0-15 dB, SENS threshold -25 to +6 dB, attack under 10 ms to
// 80 % and the three modes follow the public specification; the crossover,
// release, knee and TubeSim curve are Chimera choices (not published), and no
// hardware response is claimed.
class PedalMultiComp {
public:
    enum Mode { normal = 0, multiband = 1, tubeSim = 2 };
    static constexpr float crossoverHz = 250.f, attackMs = 4.f, releaseMs = 160.f, kneeDb = 6.f;

    // Static gain computer (dB in -> dB of gain change), soft knee.
    static float gainChangeDb(float levelDb, float thresholdDb, float ratio) noexcept {
        const float over = levelDb - thresholdDb, slope = 1.f - 1.f / std::max(1.f, ratio);
        if (2.f * over <= -kneeDb) return 0.f;
        if (2.f * over >= kneeDb) return -slope * over;
        const float x = over + kneeDb * .5f;
        return -slope * x * x / (2.f * kneeDb);
    }

    void prepare(double sampleRate) noexcept {
        rate = sampleRate;
        juce::dsp::ProcessSpec spec{rate, 512, 2};
        split.prepare(spec);
        split.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
        split.setCutoffFrequency(crossoverHz);
        attack = float(std::exp(-1. / (rate * attackMs * .001)));
        release = float(std::exp(-1. / (rate * releaseMs * .001)));
        for (auto* s : {&ratioSmooth, &thresholdSmooth, &makeupSmooth, &balanceSmooth}) s->reset(rate, .02);
        modeMix.reset(rate, .01);modeMix.setCurrentAndTargetValue(1.f);
        reset();
    }
    void reset() noexcept {
        split.reset();
        envelope = {};
        primed = false;
    }
    // controls: comp ratio, gain dB, sens dB, mode, balance dB.
    void process(juce::AudioBuffer<float>& buffer, const std::array<float, 12>& controls) noexcept {
        ratioSmooth.setTargetValue(juce::jlimit(1.f, 5.f, controls[0]));
        makeupSmooth.setTargetValue(juce::jlimit(0.f, 15.f, controls[1]));
        thresholdSmooth.setTargetValue(juce::jlimit(-25.f, 6.f, controls[2]));
        const int mode = juce::jlimit(0, 2, juce::roundToInt(controls[3]));
        balanceSmooth.setTargetValue(juce::jlimit(-6.f, 6.f, controls[4]));
        if (!primed) {
            for (auto* s : {&ratioSmooth, &thresholdSmooth, &makeupSmooth, &balanceSmooth}) s->setCurrentAndTargetValue(s->getTargetValue());
            primed = true; activeMode = mode;
        }
        // A mode change crossfades through a short dip so the band split and
        // the TubeSim stage do not switch in mid-wave.
        if (mode != activeMode) { if (!modeMix.isSmoothing() && modeMix.getCurrentValue() <= 0.f) { activeMode = mode; modeMix.setTargetValue(1.f); } else modeMix.setTargetValue(0.f); }
        else modeMix.setTargetValue(1.f);
        const int channels = std::min(2, buffer.getNumChannels());
        for (int n = 0; n < buffer.getNumSamples(); ++n) {
            const float ratio = ratioSmooth.getNextValue(), threshold = thresholdSmooth.getNextValue();
            const float makeup = juce::Decibels::decibelsToGain(makeupSmooth.getNextValue());
            const float balance = juce::Decibels::decibelsToGain(balanceSmooth.getNextValue());
            const float fade = modeMix.getNextValue();
            if (activeMode == multiband) {
                std::array<float, 2> low{}, high{};
                float peakLow = 0.f, peakHigh = 0.f;
                for (int c = 0; c < channels; ++c) {
                    float l = 0.f, h = 0.f;
                    split.processSample(c, buffer.getSample(c, n), l, h);
                    low[size_t(c)] = l; high[size_t(c)] = h;
                    peakLow = std::max(peakLow, std::abs(l)); peakHigh = std::max(peakHigh, std::abs(h));
                }
                const float gLow = gain(0, peakLow, threshold, ratio), gHigh = gain(1, peakHigh, threshold, ratio);
                for (int c = 0; c < channels; ++c)
                    buffer.setSample(c, n, fade * makeup * (gLow * low[size_t(c)] + balance * gHigh * high[size_t(c)]));
            } else {
                float peak = 0.f;
                for (int c = 0; c < channels; ++c) peak = std::max(peak, std::abs(buffer.getSample(c, n)));
                const float g = gain(0, peak, threshold, ratio);
                for (int c = 0; c < channels; ++c) {
                    float y = g * buffer.getSample(c, n);
                    if (activeMode == tubeSim) y = tube(y);
                    buffer.setSample(c, n, fade * makeup * y);
                }
            }
        }
    }
    float reduction() const noexcept { return lastReduction; }

private:
    // Linked peak detector with attack/release, per band; returns linear gain.
    float gain(int band, float peak, float thresholdDb, float ratio) noexcept {
        auto& e = envelope[size_t(band)];
        e = peak > e ? attack * e + (1.f - attack) * peak : release * e + (1.f - release) * peak;
        const float change = gainChangeDb(juce::Decibels::gainToDecibels(e, -120.f), thresholdDb, ratio);
        if (band == 0) lastReduction = -change;
        return juce::Decibels::decibelsToGain(change);
    }
    // TubeSim: unity at low level, gently asymmetric saturation adding mostly
    // even harmonics as the compressed signal rises.
    static float tube(float x) noexcept {
        constexpr float drive = 1.6f, bias = .18f;
        const float y = std::tanh(drive * x + bias) - std::tanh(bias);
        return y / (drive * (1.f - std::tanh(bias) * std::tanh(bias)));
    }
    double rate{48000.};
    float attack{}, release{}, lastReduction{};
    std::array<float, 2> envelope{};
    juce::dsp::LinkwitzRileyFilter<float> split;
    juce::SmoothedValue<float> ratioSmooth, thresholdSmooth, makeupSmooth, balanceSmooth, modeMix;
    int activeMode{};
    bool primed{};
};

} // namespace spectralforge
