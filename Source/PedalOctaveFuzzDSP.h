#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <memory>

namespace spectralforge {

// Octave Fuzz (PRE model 48, docs/PRE_EXPANSION_B_1_5.md): an original octave
// fuzz laid out after the published MXR SF01 controls: VOLUME (the direct
// signal, or the fuzz once SUB INTO FUZZ is on; dry at most +1.6 dB), FUZZ
// (a 40 dB gain range), TONE (brighter clockwise), SUB OCTAVE and OCTAVE UP
// levels, the SUB INTO FUZZ switch, the OCTAVE UP footswitch and the internal
// octave-up GAIN (20 dB range) and TONE trims; the sub voice is low-passed at
// the published 149 Hz. The divider, the clipping, the rectifier and the tone
// curves are Chimera choices; no hardware response is claimed. Monophonic
// sub (single notes); everything runs at 4x, so the slot latency is that of
// the other oversampled drives.
class PedalOctaveFuzz {
public:
    static constexpr float minimumVolumeDb = -60.f, maximumVolumeDb = 1.6f, minimumFuzzDb = 10.f, fuzzRangeDb = 40.f;
    static constexpr float upGainRangeDb = 20.f, subCornerHz = 149.f, fuzzOutputDb = -12.f, clipOffset = .2f;
    static float fuzzDb(float knob) noexcept { return minimumFuzzDb + fuzzRangeDb * juce::jlimit(0.f, 1.f, knob); }
    static float toneHz(float knob) noexcept { return 500.f * std::pow(12.f, juce::jlimit(0.f, 1.f, knob)); }    // 500 Hz - 6 kHz
    static float upToneHz(float knob) noexcept { return 1000.f * std::pow(8.f, juce::jlimit(0.f, 1.f, knob)); }  // 1 - 8 kHz

    void prepare(double sampleRate, int maximumBlock) {
        rate = sampleRate;
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampling->initProcessing(size_t(maximumBlock));
        const double os = rate * 4.;
        trackingPole = float(1. - std::exp(-juce::MathConstants<double>::twoPi * 650. / os));
        attack = float(std::exp(-1. / (os * .002))); release = float(std::exp(-1. / (os * .065)));
        fuzzDcPole = float(1. - std::exp(-juce::MathConstants<double>::twoPi * 10. / os));
        upDcPole = float(1. - std::exp(-juce::MathConstants<double>::twoPi * 20. / os));
        subLowPass = lowPass(subCornerHz, os);
        for (auto* s : {&volume, &drive, &tone, &sub, &up, &upGain, &upTone, &fuzzMix}) s->reset(os, .02);
        reset();
    }
    void reset() noexcept {
        if (oversampling) oversampling->reset();
        channels = {};
        primed = false;
        clock = 0;
    }
    int latency() const noexcept { return oversampling ? juce::roundToInt(oversampling->getLatencyInSamples()) : 0; }

    // controls: volume dB, fuzz, tone, sub level, up level, sub into fuzz, up on, up gain dB, up tone.
    void process(juce::AudioBuffer<float>& buffer, const std::array<float, 12>& c) noexcept {
        volume.setTargetValue(juce::Decibels::decibelsToGain(juce::jlimit(minimumVolumeDb, maximumVolumeDb, c[0])));
        drive.setTargetValue(juce::Decibels::decibelsToGain(fuzzDb(c[1])));
        tone.setTargetValue(juce::jlimit(0.f, 1.f, c[2]));
        sub.setTargetValue(juce::jlimit(0.f, 1.f, c[3]));
        up.setTargetValue(c[6] > .5f ? juce::jlimit(0.f, 1.f, c[4]) : 0.f);
        fuzzMix.setTargetValue(c[5] > .5f ? 1.f : 0.f);
        upGain.setTargetValue(juce::Decibels::decibelsToGain(juce::jlimit(0.f, upGainRangeDb, c[7])));
        upTone.setTargetValue(juce::jlimit(0.f, 1.f, c[8]));
        if (!primed) {
            for (auto* s : {&volume, &drive, &tone, &sub, &up, &upGain, &upTone, &fuzzMix}) s->setCurrentAndTargetValue(s->getTargetValue());
            primed = true;
        }
        const int active = std::min(2, buffer.getNumChannels());
        juce::dsp::AudioBlock<float> block(buffer);
        auto subset = block.getSubsetChannelBlock(0, size_t(active));
        auto os = oversampling->processSamplesUp(subset);
        const double osRate = rate * 4.;
        const float offset = std::tanh(clipOffset), outputGain = juce::Decibels::decibelsToGain(fuzzOutputDb);
        for (size_t n = 0; n < os.getNumSamples(); ++n) {
            const float v = volume.getNextValue(), d = drive.getNextValue(), t = tone.getNextValue(), s = sub.getNextValue();
            const float u = up.getNextValue(), g = upGain.getNextValue(), ut = upTone.getNextValue(), mix = fuzzMix.getNextValue();
            if (clock++ % 32 == 0) { toneLowPass = lowPass(toneHz(t), osRate); upLowPass = lowPass(upToneHz(ut), osRate); }
            for (size_t ch = 0; ch < os.getNumChannels(); ++ch) {
                auto& st = channels[ch];
                const float x = os.getSample(int(ch), int(n));
                // Sub octave: a flip-flop on the tracked zero crossings, at the
                // input's envelope, low-passed at 149 Hz.
                const float magnitude = std::abs(x), pole = magnitude > st.envelope ? attack : release;
                st.envelope = pole * st.envelope + (1.f - pole) * magnitude;
                st.tracker1 += trackingPole * (x - st.tracker1); st.tracker2 += trackingPole * (st.tracker1 - st.tracker2);
                const float threshold = std::max(1.0e-5f, st.envelope * .035f);
                if (st.tracker2 < -threshold) st.armed = true;
                if (st.armed && st.tracker2 > threshold) { st.flip = !st.flip; st.armed = false; }
                const float subVoice = s * tick(subLowPass, st.sub, st.flip ? st.envelope : -st.envelope);
                // Fuzz: asymmetric soft clip of input and sub, DC removed, TONE.
                float f = std::tanh(d * (x + subVoice) + clipOffset) - offset;
                st.fuzzDc += fuzzDcPole * (f - st.fuzzDc); f -= st.fuzzDc;
                f = outputGain * tick(toneLowPass, st.tone, f);
                // Octave up: full-wave rectified input, DC removed, the GAIN
                // trim into a soft clip, the TONE trim.
                float r = std::abs(x);
                st.upDc += upDcPole * (r - st.upDc); r -= st.upDc;
                r = .5f * tick(upLowPass, st.up, std::tanh(4.f * g * r));
                os.setSample(int(ch), int(n), (1.f - mix) * (v * x + subVoice) + mix * v * f + u * r);
            }
        }
        clock %= 32;
        oversampling->processSamplesDown(subset);
    }

private:
    struct Coefficients { double b0{1}, b1{}, b2{}, a1{}, a2{}; };
    struct State { double z1{}, z2{}; };
    struct Channel { float envelope{}, tracker1{}, tracker2{}, fuzzDc{}, upDc{}; bool armed{}, flip{}; State sub{}, tone{}, up{}; };
    static Coefficients lowPass(double hz, double sampleRate) noexcept {
        const double w = juce::MathConstants<double>::twoPi * std::min(hz, sampleRate * .45) / sampleRate, alpha = std::sin(w) / (2. * .707), cw = std::cos(w), a0 = 1. + alpha;
        return {(1. - cw) / 2. / a0, (1. - cw) / a0, (1. - cw) / 2. / a0, -2. * cw / a0, (1. - alpha) / a0};
    }
    static float tick(const Coefficients& c, State& s, float x) noexcept {
        const double y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return float(y);
    }
    double rate{48000.};
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    float trackingPole{}, attack{}, release{}, fuzzDcPole{}, upDcPole{};
    bool primed{};
    int clock{};
    Coefficients subLowPass{}, toneLowPass{}, upLowPass{};
    std::array<Channel, 2> channels{};
    juce::SmoothedValue<float> volume, drive, tone, sub, up, upGain, upTone, fuzzMix;
};

} // namespace spectralforge
