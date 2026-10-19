#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace spectralforge {

inline constexpr int multibandBandCount = 4;

// Independent implementation. The four fixed, contiguous bands are an audio
// processor; they are not the amplifier Matrix routes. No lookahead, FIR delay,
// external sidechain, or proprietary "Dynamic Phase" behaviour is claimed.
struct MultibandBandSettings {
    bool bypass{false};
    // 0: downward compression, 1: upward compression,
    // 2: downward expansion,   3: upward expansion.
    int mode{0};
    float threshold{-24.f};
    float range{6.f};
    float ratio{2.f};
    float attack{10.f};
    float release{120.f};
    float knee{6.f};
    float makeup{0.f};
    bool solo{false};
};

struct MultibandSettings {
    bool on{false};
    float mix{1.f};
    int detector{1}; // 0: linked peak, 1: max of the two channel RMS detectors.
    std::array<float, 3> crossovers{120.f, 700.f, 3500.f};
    std::array<MultibandBandSettings, multibandBandCount> bands{};
};

namespace multiband_detail {

inline constexpr double pi = 3.1415926535897932384626433832795;
inline constexpr double butterworthK = 1.4142135623730950488;

inline double finiteClamp(double x, double lo, double hi, double fallback) noexcept {
    return std::isfinite(x) ? std::clamp(x, lo, hi) : fallback;
}

inline double validRate(double sr) noexcept {
    return finiteClamp(sr, 8000., 384000., 48000.);
}

inline double gainFromDB(double db) noexcept { return std::pow(10., db * .05); }
inline double levelDB(double level) noexcept { return 20. * std::log10(std::max(1.e-12, level)); }

inline double coefficient(double rate, double milliseconds) noexcept {
    return std::exp(-1. / (rate * milliseconds * .001));
}

// The knee is a continuous, differentiable transition between the two slopes.
inline double softHinge(double above, double knee) noexcept {
    if (knee <= 0.) return std::max(0., above);
    if (above <= -.5 * knee) return 0.;
    if (above >= .5 * knee) return above;
    const double position = above + .5 * knee;
    return position * position / (2. * knee);
}

// Signed applied gain in dB: negative means attenuation, positive means boost.
inline double dynamicsGainDB(double level, const MultibandBandSettings& s) noexcept {
    const double ratio = finiteClamp(s.ratio, 1., 20., 2.);
    const double threshold = finiteClamp(s.threshold, -72., 0., -24.);
    const double knee = finiteClamp(s.knee, 0., 24., 6.);
    const double range = finiteClamp(s.range, 0., 36., 6.);
    const double below = softHinge(threshold - level, knee);
    const double above = softHinge(level - threshold, knee);
    double change = 0.;
    switch (std::clamp(s.mode, 0, 3)) {
        case 0: change = -(1. - 1. / ratio) * above; break;
        case 1: change =  (1. - 1. / ratio) * below; break;
        case 2: change = -(ratio - 1.) * below; break;
        case 3: change =  (ratio - 1.) * above; break;
    }
    return std::clamp(change, -range, range);
}

// A finite linear ramp with exact endpoints, including exact steady bypass.
class Ramp {
public:
    void reset(double value) noexcept { current = target = value; step = 0.; remaining = 0; }
    void setTarget(double value, int samples) noexcept {
        if (value == target) return;
        target = value;
        remaining = std::max(1, samples);
        step = (target - current) / remaining;
    }
    double next() noexcept {
        if (remaining > 0) {
            current += step;
            if (--remaining == 0) current = target;
        }
        return current;
    }
    double value() const noexcept { return current; }
    bool moving() const noexcept { return remaining > 0; }
private:
    double current{}, target{}, step{};
    int remaining{};
};

struct SVFCoefficients {
    double a1{1.}, a2{}, a3{};
    void setG(double g) noexcept {
        a1 = 1. / (1. + g * (g + butterworthK));
        a2 = g * a1;
        a3 = g * a2;
    }
};

// Topology-preserving state-variable sections retain stable poles while their
// positive frequency parameter is smoothed. Coefficients are shared across
// every section at a given crossover, including its phase compensators.
class CrossoverFrequency {
public:
    void prepare(double sr, double hz) noexcept {
        rate = sr;
        smoothing = coefficient(rate, 20.);
        g = target = std::tan(pi * hz / rate);
        coefficients.setG(g);
    }
    void setTarget(double hz) noexcept { target = std::tan(pi * hz / rate); }
    const SVFCoefficients& next() noexcept {
        if (g != target) {
            g = target + smoothing * (g - target);
            if (std::abs(g - target) <= 1.e-10 * std::max(1., target)) g = target;
            coefficients.setG(g);
        }
        return coefficients;
    }
    float frequency() const noexcept { return float(rate * std::atan(g) / pi); }
private:
    double rate{48000.}, g{}, target{}, smoothing{};
    SVFCoefficients coefficients{};
};

struct SVFOutput { double low{}, band{}, high{}; };

class SVF {
public:
    void reset() noexcept { z1 = z2 = 0.; }
    SVFOutput tick(double x, const SVFCoefficients& c) noexcept {
        const double v3 = x - z2;
        const double v1 = c.a1 * z1 + c.a2 * v3;
        const double v2 = z2 + c.a2 * z1 + c.a3 * v3;
        z1 = 2. * v1 - z1;
        z2 = 2. * v2 - z2;
        if (std::abs(z1) < 1.e-30) z1 = 0.;
        if (std::abs(z2) < 1.e-30) z2 = 0.;
        return {v2, v1, x - butterworthK * v1 - v2};
    }
    double allpass(double x, const SVFCoefficients& c) noexcept {
        return x - 2. * butterworthK * tick(x, c).band;
    }
private:
    double z1{}, z2{};
};

class LinkwitzRileySplit {
public:
    void reset() noexcept { first.reset(); low.reset(); high.reset(); }
    std::array<double, 2> tick(double x, const SVFCoefficients& c) noexcept {
        const auto a = first.tick(x, c);
        return {low.tick(a.low, c).low, high.tick(a.high, c).high};
    }
private:
    SVF first, low, high;
};

// L_i + H_i = A_i, where A_i is a second-order allpass. The compensated
// cascade is [A3 A2 L1, A3 L2 H1, L3 H2 H1, H3 H2 H1]. Its unity-gain sum
// is A3 A2 A1, not the misaligned sum from a naive three-way LR cascade.
// Hence settled crossovers reconstruct with flat MAGNITUDE. They do rotate
// phase: zero added sample delay does not mean linear/zero phase. Automation
// is smoothed, but the LTI allpass identity is not claimed during motion.
class FourBandSplit {
public:
    void reset() noexcept {
        for (auto& s : split) s.reset();
        for (auto& a : compensation) a.reset();
    }
    std::array<double, 4> tick(double input,
                               const std::array<const SVFCoefficients*, 3>& c) noexcept {
        const auto first = split[0].tick(input, *c[0]);
        const auto second = split[1].tick(first[1], *c[1]);
        const auto third = split[2].tick(second[1], *c[2]);
        const double low = compensation[1].allpass(
            compensation[0].allpass(first[0], *c[1]), *c[2]);
        const double lowMid = compensation[2].allpass(second[0], *c[2]);
        return {low, lowMid, third[0], third[1]};
    }
private:
    std::array<LinkwitzRileySplit, 3> split;
    std::array<SVF, 3> compensation;
};

struct Detector {
    std::array<double, 2> energy{};
    double peak{};
    void reset() noexcept { energy = {}; peak = 0.; }
    double tick(const std::array<double, 2>& sample, int channels, int mode,
                double rmsCoefficient, double peakCoefficient) noexcept {
        double peakSample = 0., maximumEnergy = 0.;
        for (int c = 0; c < channels; ++c) {
            const auto index = size_t(c);
            const double x = sample[index];
            peakSample = std::max(peakSample, std::abs(x));
            energy[index] = rmsCoefficient * energy[index]
                          + (1. - rmsCoefficient) * x * x;
            if (energy[index] < 1.e-30) energy[index] = 0.;
            maximumEnergy = std::max(maximumEnergy, energy[index]);
        }
        if (channels == 1) energy[1] = 0.;
        peak = std::max(peakSample, peak * peakCoefficient);
        if (peak < 1.e-30) peak = 0.;
        return mode == 0 ? peak : std::sqrt(maximumEnergy);
    }
};

inline float safeOutput(double value) noexcept {
    if (!std::isfinite(value)) return 0.f;
    constexpr double bound = std::numeric_limits<float>::max();
    return float(std::clamp(value, -bound, bound));
}

inline MultibandBandSettings sanitiseBand(MultibandBandSettings s) noexcept {
    s.mode = std::clamp(s.mode, 0, 3);
    s.threshold = float(finiteClamp(s.threshold, -72., 0., -24.));
    s.range = float(finiteClamp(s.range, 0., 36., 6.));
    s.ratio = float(finiteClamp(s.ratio, 1., 20., 2.));
    s.attack = float(finiteClamp(s.attack, .1, 200., 10.));
    s.release = float(finiteClamp(s.release, 5., 2000., 120.));
    s.knee = float(finiteClamp(s.knee, 0., 24., 6.));
    s.makeup = float(finiteClamp(s.makeup, -24., 24., 0.));
    return s;
}

} // namespace multiband_detail

class MultibandDynamicsCore {
public:
    // No delay line/lookahead. IIR frequency-dependent group delay remains.
    static constexpr int latencySamples() noexcept { return 0; }

    MultibandDynamicsCore() noexcept { prepare(48000.); }

    static std::array<float, 3> validatedCrossovers(
        std::array<float, 3> input, double sampleRate) noexcept {
        using namespace multiband_detail;
        const double maximum = std::min(16000., .4 * validRate(sampleRate));
        constexpr double spacing = 1.4;
        const double f0 = finiteClamp(input[0], 40., maximum / (spacing * spacing), 120.);
        const double f1 = std::clamp(finiteClamp(input[1], 40., maximum, 700.),
                                   f0 * spacing, maximum / spacing);
        const double f2 = std::clamp(finiteClamp(input[2], 40., maximum, 3500.),
                                   f1 * spacing, maximum);
        return {float(f0), float(f1), float(f2)};
    }

    void setSettings(const MultibandSettings& next) noexcept {
        using namespace multiband_detail;
        settings = next;
        settings.mix = float(finiteClamp(settings.mix, 0., 1., 1.));
        settings.detector = std::clamp(settings.detector, 0, 1);
        settings.crossovers = validatedCrossovers(settings.crossovers, rate);
        for (auto& b : settings.bands) b = sanitiseBand(b);
    }

    const MultibandSettings& getSettings() const noexcept { return settings; }

    void prepare(double sampleRate) noexcept {
        using namespace multiband_detail;
        rate = validRate(sampleRate);
        setSettings(settings);
        rampSamples = std::max(1, int(std::ceil(rate * .01)));
        rmsCoefficient = coefficient(rate, 5.);
        peakCoefficient = coefficient(rate, 30.);
        reset();
    }

    void reset() noexcept {
        for (auto& s : splitter) s.reset();
        for (size_t i = 0; i < frequencies.size(); ++i)
            frequencies[i].prepare(rate, settings.crossovers[i]);
        wet.reset(settings.on ? 1. : 0.);
        mix.reset(settings.mix);
        const bool solo = anySolo();
        for (size_t i = 0; i < bandState.size(); ++i) {
            auto& s = bandState[i];
            s.detector.reset();
            s.gainDB = 0.;
            s.makeup.reset(settings.bands[i].makeup);
            s.wet.reset(settings.bands[i].bypass ? 0. : 1.);
            s.solo.reset(!solo || settings.bands[i].solo ? 1. : 0.);
        }
        meters = {};
    }

    // In-place mono/stereo; extra channels are untouched. Size is independent
    // of a prepared maximum: zero, ragged, and oversized blocks are bounded.
    // All storage is fixed; no allocation, lock, logging or parameter lookup.
    void process(float* const* audio, int channelCount, int sampleCount) noexcept {
        using namespace multiband_detail;
        const int channels = std::clamp(channelCount, 0, 2);
        if (channels == 0 || sampleCount <= 0 || audio == nullptr) return;
        for (int c = 0; c < channels; ++c) if (audio[c] == nullptr) return;

        if (settings.on && wet.value() == 0. && !wet.moving()) {
            for (auto& split : splitter) split.reset();
            for (auto& state : bandState) { state.detector.reset(); state.gainDB = 0.; }
        }
        wet.setTarget(settings.on ? 1. : 0., rampSamples);
        if (!settings.on && !wet.moving()) { meters = {}; return; }

        mix.setTarget(settings.mix, rampSamples);
        for (size_t i = 0; i < frequencies.size(); ++i)
            frequencies[i].setTarget(settings.crossovers[i]);
        const bool solo = anySolo();
        for (size_t i = 0; i < bandState.size(); ++i) {
            auto& s = bandState[i];
            const auto& v = settings.bands[i];
            s.attack = coefficient(rate, v.attack);
            s.release = coefficient(rate, v.release);
            s.makeup.setTarget(v.makeup, rampSamples);
            s.wet.setTarget(v.bypass ? 0. : 1., rampSamples);
            s.solo.setTarget(!solo || v.solo ? 1. : 0., rampSamples);
        }

        for (int n = 0; n < sampleCount; ++n) {
            std::array<const SVFCoefficients*, 3> coefficients{};
            for (size_t i = 0; i < frequencies.size(); ++i)
                coefficients[i] = &frequencies[i].next();
            std::array<double, 2> dry{};
            std::array<std::array<double, 4>, 2> samples{};
            for (int c = 0; c < channels; ++c) {
                const auto index = size_t(c);
                dry[index] = std::isfinite(audio[c][n]) ? double(audio[c][n]) : 0.;
                samples[index] = splitter[index].tick(dry[index], coefficients);
            }
            std::array<double, 2> sum{};
            const double parallel = mix.next();
            for (size_t i = 0; i < bandState.size(); ++i) {
                auto& state = bandState[i];
                const auto& v = settings.bands[i];
                const std::array<double, 2> detectorInput{samples[0][i], samples[1][i]};
                const double level = state.detector.tick(detectorInput, channels,
                    settings.detector, rmsCoefficient, peakCoefficient);
                const double desired = v.bypass ? 0. : dynamicsGainDB(levelDB(level), v);
                const bool increasing = std::abs(desired) > std::abs(state.gainDB)
                                     || desired * state.gainDB < 0.;
                const double smoothing = increasing ? state.attack : state.release;
                state.gainDB = desired + smoothing * (state.gainDB - desired);
                if (std::abs(state.gainDB) < 1.e-10) state.gainDB = 0.;
                const double bandWet = state.wet.next();
                const double makeup = state.makeup.next();
                const double dynamic = gainFromDB(state.gainDB);
                const double branch = gainFromDB(state.gainDB + makeup);
                // The dry component is each identically phase-compensated band,
                // not unfiltered input. Parallel mix therefore cannot comb.
                const double gain = (1. + parallel * bandWet * (branch - 1.))
                                  * state.solo.next();
                for (int c = 0; c < channels; ++c)
                    sum[size_t(c)] += samples[size_t(c)][i] * gain;
                meters[i] = float(-levelDB(1. + parallel * bandWet * (dynamic - 1.)));
            }
            const double active = wet.next();
            for (int c = 0; c < channels; ++c) {
                const auto index = size_t(c);
                // A short global bypass crossfade can have phase coloration;
                // the fully disabled endpoint is the original sample exactly.
                if (active >= 1.) audio[c][n] = safeOutput(sum[index]);
                else if (active > 0.) audio[c][n] = safeOutput(dry[index] + active * (sum[index] - dry[index]));
                // active == 0 intentionally does not rewrite the input sample.
            }
        }
        if (!settings.on && !wet.moving()) meters = {};
    }

    float reduction(int band) const noexcept {
        return meters[size_t(std::clamp(band, 0, multibandBandCount - 1))];
    }
    std::array<float, 3> crossovers() const noexcept {
        return {frequencies[0].frequency(), frequencies[1].frequency(), frequencies[2].frequency()};
    }
    double sampleRate() const noexcept { return rate; }

private:
    struct BandState {
        multiband_detail::Detector detector;
        multiband_detail::Ramp makeup, wet, solo;
        double gainDB{}, attack{}, release{};
    };
    bool anySolo() const noexcept {
        for (const auto& b : settings.bands) if (b.solo) return true;
        return false;
    }

    MultibandSettings settings{};
    double rate{48000.}, rmsCoefficient{}, peakCoefficient{};
    int rampSamples{480};
    std::array<multiband_detail::CrossoverFrequency, 3> frequencies;
    std::array<multiband_detail::FourBandSplit, 2> splitter;
    std::array<BandState, multibandBandCount> bandState;
    multiband_detail::Ramp wet, mix;
    std::array<float, multibandBandCount> meters{};
};

struct DeEsserSettings {
    bool on{false};
    float frequency{6000.f};
    float threshold{-24.f};
    float range{6.f};
    float attack{2.f};
    float release{80.f};
    float ratio{4.f};
    float knee{6.f};
    bool listen{false};
};

// Split-band de-esser: only the high branch is attenuated. This is not a
// broadband compressor whose detector merely happens to be high-passed.
class DeEsserCore {
public:
    static constexpr int latencySamples() noexcept { return 0; }
    DeEsserCore() noexcept { prepare(48000.); }

    void setSettings(DeEsserSettings next) noexcept {
        using namespace multiband_detail;
        settings = next;
        settings.frequency = float(finiteClamp(settings.frequency, 1500.,
            std::min(16000., rate * .4), std::min(6000., rate * .4)));
        settings.threshold = float(finiteClamp(settings.threshold, -72., 0., -24.));
        settings.range = float(finiteClamp(settings.range, 0., 24., 6.));
        settings.attack = float(finiteClamp(settings.attack, .1, 100., 2.));
        settings.release = float(finiteClamp(settings.release, 5., 1000., 80.));
        settings.ratio = float(finiteClamp(settings.ratio, 1., 20., 4.));
        settings.knee = float(finiteClamp(settings.knee, 0., 24., 6.));
    }
    const DeEsserSettings& getSettings() const noexcept { return settings; }

    void prepare(double sampleRate) noexcept {
        using namespace multiband_detail;
        rate = validRate(sampleRate);
        setSettings(settings);
        rampSamples = std::max(1, int(std::ceil(rate * .01)));
        rmsCoefficient = coefficient(rate, 2.);
        peakCoefficient = coefficient(rate, 5.);
        reset();
    }
    void reset() noexcept {
        for (auto& s : splitter) s.reset();
        detector.reset();
        frequencySmoother.prepare(rate, settings.frequency);
        wet.reset(settings.on ? 1. : 0.);
        listen.reset(settings.listen ? 1. : 0.);
        gainDB = 0.;
        meter = 0.f;
    }

    void process(float* const* audio, int channelCount, int sampleCount) noexcept {
        using namespace multiband_detail;
        const int channels = std::clamp(channelCount, 0, 2);
        if (channels == 0 || sampleCount <= 0 || audio == nullptr) return;
        for (int c = 0; c < channels; ++c) if (audio[c] == nullptr) return;
        if (settings.on && wet.value() == 0. && !wet.moving()) {
            for (auto& s : splitter) s.reset();
            detector.reset();
            gainDB = 0.;
        }
        wet.setTarget(settings.on ? 1. : 0., rampSamples);
        if (!settings.on && !wet.moving()) { meter = 0.f; return; }
        listen.setTarget(settings.listen ? 1. : 0., rampSamples);
        frequencySmoother.setTarget(settings.frequency);
        const double attack = coefficient(rate, settings.attack);
        const double release = coefficient(rate, settings.release);
        MultibandBandSettings dynamics;
        dynamics.threshold = settings.threshold;
        dynamics.range = settings.range;
        dynamics.ratio = settings.ratio;
        dynamics.knee = settings.knee;

        for (int n = 0; n < sampleCount; ++n) {
            const auto& coefficients = frequencySmoother.next();
            std::array<double, 2> dry{}, low{}, high{};
            for (int c = 0; c < channels; ++c) {
                const auto i = size_t(c);
                dry[i] = std::isfinite(audio[c][n]) ? double(audio[c][n]) : 0.;
                const auto split = splitter[i].tick(dry[i], coefficients);
                low[i] = split[0];
                high[i] = split[1];
            }
            const double level = detector.tick(high, channels, 0, rmsCoefficient, peakCoefficient);
            const double desired = dynamicsGainDB(levelDB(level), dynamics);
            const double smoothing = desired < gainDB ? attack : release;
            gainDB = desired + smoothing * (gainDB - desired);
            if (std::abs(gainDB) < 1.e-10) gainDB = 0.;
            const double gain = gainFromDB(gainDB);
            const double audition = listen.next(), active = wet.next();
            for (int c = 0; c < channels; ++c) {
                const auto i = size_t(c);
                const double processed = low[i] + gain * high[i];
                // Listen auditions the unattenuated high-band detector signal.
                const double output = processed + audition * (high[i] - processed);
                if (active >= 1.) audio[c][n] = safeOutput(output);
                else if (active > 0.) audio[c][n] = safeOutput(dry[i] + active * (output - dry[i]));
            }
        }
        meter = !settings.on && !wet.moving() ? 0.f : float(-gainDB);
    }

    float reduction() const noexcept { return meter; }
    float frequency() const noexcept { return frequencySmoother.frequency(); }
    double sampleRate() const noexcept { return rate; }
private:
    DeEsserSettings settings{};
    double rate{48000.}, rmsCoefficient{}, peakCoefficient{}, gainDB{};
    int rampSamples{480};
    std::array<multiband_detail::LinkwitzRileySplit, 2> splitter;
    multiband_detail::CrossoverFrequency frequencySmoother;
    multiband_detail::Detector detector;
    multiband_detail::Ramp wet, listen;
    float meter{};
};

} // namespace spectralforge
