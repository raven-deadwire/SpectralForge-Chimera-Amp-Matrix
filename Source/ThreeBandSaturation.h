#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>

namespace spectralforge {

// Three-band saturation (docs/SATURATION_1_6.md), an independent common
// processor after the multiband dynamics. Topology: two Linkwitz-Riley
// (24 dB/oct) crossovers split the signal; each band is saturated and only
// each band's change (saturated band minus band, times Mix) is added to the
// band sum. Off leaves the input sample-exact; on, Mix 0 and Clean bands keep
// every level and only rotate phase around the crossovers. Switching on or
// off moves through a phase-matched all-pass, so nothing cancels (see
// SaturationCore). Live works at the host rate with no latency; Studio runs
// the same at 4x (polyphase IIR) and reports that latency while selected.
inline constexpr int saturationBandCount = 3;
inline constexpr int saturationParameterVersion = 19;
inline constexpr int saturationParameterCount = 6 + 2 * saturationBandCount;
inline juce::StringArray saturationTypeNames() { return {"Clean", "Tape", "Tube", "Clip"}; }
inline juce::String saturationID(int band, const char* field) { return "satBand" + juce::String(band + 1) + "_" + field; }

inline void appendSaturationParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    const auto number = [&](const juce::String& id, const juce::String& name, float lo, float hi, float initial, float centre, const juce::String& unit) {
        juce::NormalisableRange<float> range{lo, hi, .01f};
        if (centre > lo && centre < hi) range.setSkewForCentre(centre);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, saturationParameterVersion}, name, range, initial,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([unit](float v, int) { return juce::String(v, unit == " Hz" ? 0 : 1) + unit; })));
    };
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"satOn", saturationParameterVersion}, "Saturation", false));
    number("satMix", "Saturation mix", 0.f, 100.f, 100.f, 0.f, " %");
    number("satOutput", "Saturation output", -24.f, 12.f, 0.f, 0.f, " dB");
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"satQuality", saturationParameterVersion}, "Saturation quality",
                                                            juce::StringArray{"Live", "Studio 4x"}, 0));
    number("satCrossover1", "Saturation crossover 1", 40.f, 1000.f, 200.f, 200.f, " Hz");
    number("satCrossover2", "Saturation crossover 2", 500.f, 12000.f, 2500.f, 2500.f, " Hz");
    constexpr std::array<int, saturationBandCount> types{0, 2, 1};   // low Clean, mid Tube, high Tape
    for (int b = 0; b < saturationBandCount; ++b) {
        const auto name = "Saturation band " + juce::String(b + 1) + " ";
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{saturationID(b, "type"), saturationParameterVersion}, name + "type",
                                                                saturationTypeNames(), types[size_t(b)]));
        number(saturationID(b, "drive"), name + "drive", 0.f, 36.f, 12.f, 12.f, " dB");
    }
}

struct SaturationSettings {
    bool on{false};
    float mix{1.f}, outputDb{0.f};
    int quality{0};   // 0 Live, 1 Studio 4x
    std::array<float, 2> crossovers{200.f, 2500.f};
    std::array<int, saturationBandCount> types{0, 2, 1};
    std::array<float, saturationBandCount> driveDb{12.f, 12.f, 12.f};
};

namespace saturation_detail {
// Each curve has slope 1 at zero, so drive changes harmonics and compression,
// not the small-signal level: f(g x) / g.
inline double curve(int type, double u) noexcept {
    switch (type) {
        case 1: return std::tanh(u);                                                   // Tape: symmetric, soft
        case 2: {                                                                      // Tube: asymmetric ceilings
            // Slope 1 on both sides of zero, but the negative half saturates
            // at 1/1.6 of the positive one, so it stays asymmetric (even
            // harmonics) however hard it is driven.
            constexpr double k = 1.6;
            return u >= 0. ? std::tanh(u) : std::tanh(k * u) / k;
        }
        case 3: return u >= 1. ? 2. / 3. : u <= -1. ? -2. / 3. : u - u * u * u / 3.;   // Clip: cubic, hard at +-1
        default: return u;
    }
}
struct Biquad {
    double b0{1}, b1{}, b2{}, a1{}, a2{};
    static Biquad butterworth(double hz, double rate, bool highPass) noexcept {
        const double w = juce::MathConstants<double>::twoPi * hz / rate, alpha = std::sin(w) / (2. * std::sqrt(.5)), c = std::cos(w), a0 = 1. + alpha;
        return highPass ? Biquad{(1. + c) / 2. / a0, -(1. + c) / a0, (1. + c) / 2. / a0, -2. * c / a0, (1. - alpha) / a0}
                        : Biquad{(1. - c) / 2. / a0, (1. - c) / a0, (1. - c) / 2. / a0, -2. * c / a0, (1. - alpha) / a0};
    }
};
struct State { double z1{}, z2{}; };
inline double tick(const Biquad& c, State& s, double x) noexcept {
    const double y = c.b0 * x + s.z1;
    s.z1 = c.b1 * x - c.a1 * y + s.z2;
    s.z2 = c.b2 * x - c.a2 * y;
    return y;
}
// Linkwitz-Riley 4th order: two cascaded Butterworth sections.
struct LR4 {
    std::array<State, 2> s{};
    double run(const Biquad& c, double x) noexcept { return tick(c, s[1], tick(c, s[0], x)); }
};
} // namespace saturation_detail

// The DSP at one rate (host rate in Live, 4x in Studio).
//
// On:  out = sum(band_b) + mix * sum(f_b(band_b) - band_b). The bands are
//      Linkwitz-Riley (the low one all-pass compensated at f2), so their sum
//      is the input through two second-order all-passes A(f1) A(f2): level
//      unchanged, phase rotated around the crossovers.
// Off: out = input, untouched.
// Switching: the input crossfades (5 ms) into its own pair of Q 0.707
// all-passes tuned to 0.5 Hz, where they barely rotate anything audible; their
// frequencies then glide to the crossovers (20 ms), where they equal the band
// sum's phase, and the output crossfades to the processed sum (10 ms).
// Switching off reverses this. Each step blends two nearly equal signals or
// moves one magnitude-1 path, so no frequency cancels on the way. (Moving the
// Q from very high instead fails: a high-Q all-pass rings for hundreds of
// cycles at its frequency.)
class SaturationCore {
public:
    static constexpr double enterSeconds = .005, morphSeconds = .02, crossSeconds = .01, startHz = .5, bandR = .7071067811865476;   // R = 1 / (2 Q), Q 0.707
    void prepare(double sampleRate) noexcept {
        rate = sampleRate;
        dcPole = 1. - std::exp(-juce::MathConstants<double>::twoPi * 5. / rate);
        enterLength = std::max(1, int(rate * enterSeconds)); morphLength = std::max(1, int(rate * morphSeconds)); crossLength = std::max(1, int(rate * crossSeconds));
        for (auto& f : logHz) f.reset(rate, .05);
        for (auto& d : drive) d.reset(rate, .02);
        reset();
    }
    void reset() noexcept { channels = {}; primed = false; clock = 0; position = engaged ? total() : 0; }
    // `on` starts at its value after reset, without a transition.
    void setInitialState(bool on) noexcept { engaged = on; position = on ? total() : 0; }
    bool idle() const noexcept { return !engaged && position == 0; }
    void process(float* const* audio, int count, int samples, const SaturationSettings& s, juce::SmoothedValue<float>& mix) noexcept {
        using namespace saturation_detail;
        const double f1 = juce::jlimit(40., 1000., double(s.crossovers[0])), f2 = juce::jlimit(f1 * 1.5, std::min(12000., rate * .4), double(s.crossovers[1]));
        logHz[0].setTargetValue(float(std::log(f1))); logHz[1].setTargetValue(float(std::log(f2)));
        for (int b = 0; b < saturationBandCount; ++b) drive[size_t(b)].setTargetValue(float(juce::jlimit(0., 36., double(s.driveDb[size_t(b)]))));
        if (!primed) { for (auto& f : logHz) f.setCurrentAndTargetValue(f.getTargetValue()); for (auto& d : drive) d.setCurrentAndTargetValue(d.getTargetValue()); primed = true; }
        engaged = s.on;
        count = std::min(count, 2);
        const int end = total();
        for (int n = 0; n < samples; ++n) {
            if (clock++ % 16 == 0) {
                const double a = std::exp(double(logHz[0].getCurrentValue())), b = std::exp(double(logHz[1].getCurrentValue()));
                lp1 = Biquad::butterworth(a, rate, false); hp1 = Biquad::butterworth(a, rate, true);
                lp2 = Biquad::butterworth(b, rate, false); hp2 = Biquad::butterworth(b, rate, true);
                hz1 = a; hz2 = b;
                logHz[0].skip(16); logHz[1].skip(16);
            }
            // Transition position: 0 off, enter (input -> 0.5 Hz all-pass), glide
            // (0.5 Hz -> crossovers), cross (all-pass -> processed), end on.
            position = engaged ? std::min(end, position + 1) : std::max(0, position - 1);
            std::array<double, saturationBandCount> gain{};
            for (int b = 0; b < saturationBandCount; ++b) gain[size_t(b)] = std::pow(10., drive[size_t(b)].getNextValue() / 20.);
            const double m = mix.getNextValue();
            if (position == 0) continue;   // off: untouched
            const double enter = std::min(1., double(position) / enterLength);
            const double glide = std::clamp(double(position - enterLength) / morphLength, 0., 1.);
            const double cross = std::clamp(double(position - enterLength - morphLength) / crossLength, 0., 1.);
            const double ga = std::tan(juce::MathConstants<double>::pi * startHz * std::pow(hz1 / startHz, glide) / rate);
            const double gb = std::tan(juce::MathConstants<double>::pi * startHz * std::pow(hz2 / startHz, glide) / rate);
            for (int c = 0; c < count; ++c) {
                auto& st = channels[size_t(c)];
                const double x = std::isfinite(audio[c][n]) ? audio[c][n] : 0.;
                const double turned = st.ap2.tick(st.ap1.tick(x, ga, bandR), gb, bandR);
                const double reference = enter < 1. ? (1. - enter) * x + enter * turned : turned;
                double processed = reference;
                if (cross > 0.) {
                    const double lowRaw = st.low.run(lp1, x), rest = st.rest.run(hp1, x);
                    const double low = st.allpassLow.run(lp2, lowRaw) + st.allpassHigh.run(hp2, lowRaw);
                    const std::array<double, saturationBandCount> band{low, st.mid.run(lp2, rest), st.high.run(hp2, rest)};
                    double sum = 0., difference = 0.;
                    for (int b = 0; b < saturationBandCount; ++b) {
                        sum += band[size_t(b)];
                        const int type = s.types[size_t(b)];
                        if (type == 0) continue;   // Clean
                        const double g = gain[size_t(b)];
                        double d = curve(type, g * band[size_t(b)]) / g - band[size_t(b)];
                        if (type == 2) { auto& dc = st.dc[size_t(b)]; dc += dcPole * (d - dc); d -= dc; }   // asymmetric: remove its DC
                        difference += d;
                    }
                    processed = (1. - cross) * reference + cross * (sum + m * difference);
                } else {
                    // Keep the band filters running on the input so they are
                    // settled when the crossfade starts.
                    const double lowRaw = st.low.run(lp1, x), rest = st.rest.run(hp1, x);
                    st.allpassLow.run(lp2, lowRaw); st.allpassHigh.run(hp2, lowRaw); st.mid.run(lp2, rest); st.high.run(hp2, rest);
                }
                audio[c][n] = float(processed);
            }
        }
        clock %= 16;
    }

private:
    // Second-order all-pass from a TPT state-variable filter (stable while its
    // R and cutoff move): x - 4 R bandpass.
    struct AllPass {
        double s1{}, s2{};
        double tick(double x, double g, double r) noexcept {
            const double h = 1. / (1. + 2. * r * g + g * g);
            const double hp = (x - (2. * r + g) * s1 - s2) * h, bp = g * hp + s1, lp = g * bp + s2;
            s1 = g * hp + bp; s2 = g * bp + lp;
            return x - 4. * r * bp;
        }
    };
    struct Channel {
        saturation_detail::LR4 low, rest, allpassLow, allpassHigh, mid, high;
        AllPass ap1, ap2;
        std::array<double, saturationBandCount> dc{};
    };
    double rate{48000.}, dcPole{}, hz1{200.}, hz2{2500.};
    int total() const noexcept { return enterLength + morphLength + crossLength; }
    bool primed{}, engaged{};
    int clock{}, position{}, enterLength{240}, morphLength{960}, crossLength{480};
    saturation_detail::Biquad lp1{}, hp1{}, lp2{}, hp2{};
    std::array<Channel, 2> channels{};
    std::array<juce::SmoothedValue<float>, 2> logHz;
    std::array<juce::SmoothedValue<float>, saturationBandCount> drive;
};

class ThreeBandSaturation {
public:
    void bind(juce::AudioProcessorValueTreeState& state) {
        for (size_t i = 0; i < globalIDs.size(); ++i) globals[i] = state.getRawParameterValue(globalIDs[i]);
        for (int b = 0; b < saturationBandCount; ++b) {
            types[size_t(b)] = state.getRawParameterValue(saturationID(b, "type"));
            drives[size_t(b)] = state.getRawParameterValue(saturationID(b, "drive"));
        }
    }
    void prepare(double sampleRate, int maximumBlock) {
        rate = sampleRate;
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampling->initProcessing(size_t(juce::jmax(1, maximumBlock)));
        live.prepare(rate); studio.prepare(rate * 4.);
        wet.reset(rate, .01); wetStudio.reset(rate * 4., .01); output.reset(rate, .02); fade.reset(rate, .01);
        const auto s = settings();
        activeQuality = s.quality;
        wet.setCurrentAndTargetValue(s.mix); wetStudio.setCurrentAndTargetValue(s.mix);
        output.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(s.on ? s.outputDb : 0.f));
        fade.setCurrentAndTargetValue(1.f);
        live.setInitialState(s.on); studio.setInitialState(s.on);
        live.reset(); studio.reset(); oversampling->reset();
    }
    void reset() noexcept { live.reset(); studio.reset(); if (oversampling) oversampling->reset(); }
    // Latency of the selected quality; constant while it is selected, on or off.
    int latency() const noexcept { return activeQuality == 1 && oversampling ? juce::roundToInt(oversampling->getLatencyInSamples()) : 0; }

    void process(juce::AudioBuffer<float>& buffer) noexcept {
        const auto s = settings();
        const int channels = juce::jmin(2, buffer.getNumChannels()), samples = buffer.getNumSamples();
        if (channels == 0 || samples == 0) return;
        // A quality change moves the latency: fade the output out (10 ms),
        // swap, fade in. The processor reports the new latency from the swap.
        if (s.quality != activeQuality) fade.setTargetValue(0.f);
        wet.setTargetValue(s.mix); wetStudio.setTargetValue(s.mix);
        output.setTargetValue(juce::Decibels::decibelsToGain(s.on ? s.outputDb : 0.f));
        if (activeQuality == 0) {
            if (!s.on && live.idle() && !output.isSmoothing() && output.getCurrentValue() == 1.f && !fade.isSmoothing() && fade.getCurrentValue() == 1.f && s.quality == activeQuality) {
                wet.setCurrentAndTargetValue(s.mix);
                return;   // Off in Live: the input stays untouched.
            }
            live.process(buffer.getArrayOfWritePointers(), channels, samples, s, wet);
        } else {
            juce::dsp::AudioBlock<float> block(buffer);
            auto sub = block.getSubsetChannelBlock(0, size_t(channels));
            auto up = oversampling->processSamplesUp(sub);
            std::array<float*, 2> pointers{up.getChannelPointer(0), channels > 1 ? up.getChannelPointer(1) : nullptr};
            studio.process(pointers.data(), channels, int(up.getNumSamples()), s, wetStudio);
            oversampling->processSamplesDown(sub);
        }
        for (int n = 0; n < samples; ++n) {
            const float g = output.getNextValue() * fade.getNextValue();
            for (int c = 0; c < channels; ++c) buffer.setSample(c, n, buffer.getSample(c, n) * g);
            if (s.quality != activeQuality && fade.getCurrentValue() <= 0.f) {
                activeQuality = s.quality; live.setInitialState(s.on); studio.setInitialState(s.on); live.reset(); studio.reset(); oversampling->reset();
                fade.setTargetValue(1.f);
            }
        }
    }

private:
    SaturationSettings settings() const noexcept {
        const auto read = [](const std::atomic<float>* p, float lo, float hi, float fallback) {
            const float v = p ? p->load(std::memory_order_relaxed) : fallback;
            return std::isfinite(v) ? juce::jlimit(lo, hi, v) : fallback;
        };
        SaturationSettings s;
        s.on = read(globals[0], 0.f, 1.f, 0.f) > .5f;
        s.mix = read(globals[1], 0.f, 100.f, 100.f) / 100.f;
        s.outputDb = read(globals[2], -24.f, 12.f, 0.f);
        s.quality = juce::roundToInt(read(globals[3], 0.f, 1.f, 0.f));
        s.crossovers = {read(globals[4], 40.f, 1000.f, 200.f), read(globals[5], 500.f, 12000.f, 2500.f)};
        for (int b = 0; b < saturationBandCount; ++b) {
            s.types[size_t(b)] = juce::roundToInt(read(types[size_t(b)], 0.f, 3.f, float(SaturationSettings{}.types[size_t(b)])));
            s.driveDb[size_t(b)] = read(drives[size_t(b)], 0.f, 36.f, 12.f);
        }
        return s;
    }
    inline static constexpr std::array<const char*, 6> globalIDs{"satOn", "satMix", "satOutput", "satQuality", "satCrossover1", "satCrossover2"};
    std::array<std::atomic<float>*, 6> globals{};
    std::array<std::atomic<float>*, saturationBandCount> types{}, drives{};
    double rate{48000.};
    int activeQuality{0};
    SaturationCore live, studio;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::SmoothedValue<float> wet, wetStudio, output, fade;
};

} // namespace spectralforge
