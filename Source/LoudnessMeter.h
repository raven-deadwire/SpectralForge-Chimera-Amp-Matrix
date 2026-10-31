#pragma once

#include <juce_core/juce_core.h>
#include "TruePeak.h"
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace spectralforge {

// Studio loudness and true-peak meter (docs/LOUDNESS_METER_1_5.md) after
// ITU-R BS.1770-4 and EBU Tech 3341: K-weighting (the two BS.1770 filter
// stages, derived for any sample rate from their analogue prototypes),
// momentary (400 ms), short-term (3 s) and gated integrated loudness
// (absolute -70 LUFS, relative -10 LU gate over 400 ms blocks every 100 ms),
// loudness range (EBU Tech 3342, short-term values every 100 ms),
// and the maximum true peak from 4x (2x from 176.4 kHz) polyphase
// interpolation. Integrated loudness keeps a fixed-size histogram, so memory
// does not grow with the measuring time. A meter only: it never changes the
// audio. Values update every 100 ms; process() runs on the audio thread, the
// getters and reset() on any thread.
class LoudnessMeter {
public:
    static constexpr float silent = -200.f;   // no energy / nothing measured yet
    static constexpr float absoluteGate = -70.f, relativeGate = -10.f;

    void prepare(double sampleRate) {
        rate = sampleRate;
        hop = juce::jmax(1, juce::roundToInt(rate * .1));
        const auto k = kWeighting(rate);
        shelf = {k[0], k[1], k[2], k[3], k[4]};
        highPass = {k[5], k[6], k[7], k[8], k[9]};
        factor = truePeakFactor(rate);
        designInterpolator();
        histogram.assign(size_t(histogramBins), {});
        rangeHistogram.assign(size_t(histogramBins), 0);
        clearFilters();
        reset();
        resetRequested.store(false);
    }
    void requestReset() noexcept { resetRequested.store(true); }
    // K-weighting at a sample rate: stage 1 (high shelf, head effect) b0 b1
    // b2 a1 a2, then stage 2 (RLB high-pass) b0 b1 b2 a1 a2; a0 = 1. From
    // the analogue prototypes behind the BS.1770-4 48 kHz coefficients.
    static std::array<double, 10> kWeighting(double sampleRate) noexcept {
        std::array<double, 10> c{};
        {
            const double f0 = 1681.974450955533, gain = 3.999843853973347, q = .7071752369554196;
            const double k = std::tan(juce::MathConstants<double>::pi * f0 / sampleRate), vh = std::pow(10., gain / 20.), vb = std::pow(vh, .4996667741545416);
            const double a0 = 1. + k / q + k * k;
            c = {(vh + vb * k / q + k * k) / a0, 2. * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0, 2. * (k * k - 1.) / a0, (1. - k / q + k * k) / a0};
        }
        const double f0 = 38.13547087602444, q = .5003270373238773;
        const double k = std::tan(juce::MathConstants<double>::pi * f0 / sampleRate), a0 = 1. + k / q + k * k;
        c[5] = 1.; c[6] = -2.; c[7] = 1.; c[8] = 2. * (k * k - 1.) / a0; c[9] = (1. - k / q + k * k) / a0;
        return c;
    }

    void process(const float* const* data, int channels, int samples) noexcept {
        if (histogram.empty()) return;   // not prepared
        if (resetRequested.exchange(false)) reset();
        channels = juce::jmin(channels, 2);
        for (int n = 0; n < samples; ++n) {
            for (int c = 0; c < channels; ++c) {
                const float x = data[c][n];
                auto& s = state[size_t(c)];
                const double y = tick(highPass, s.second, tick(shelf, s.first, x));
                segmentEnergy += y * y;
                peakTick(c, x);
            }
            if (++segmentFill == hop) closeSegment();
        }
    }

    float momentary() const noexcept { return momentaryLufs.load(); }
    float shortTerm() const noexcept { return shortTermLufs.load(); }
    float integrated() const noexcept { return integratedLufs.load(); }
    float truePeak() const noexcept { return truePeakDb.load(); }
    // Loudness range in LU (EBU Tech 3342); 0 until short-term values exist.
    float loudnessRange() const noexcept { return rangeLu.load(); }
    int oversampling() const noexcept { return factor; }

private:
    struct Coefficients { double b0{1}, b1{}, b2{}, a1{}, a2{}; };
    struct State { double z1{}, z2{}; };
    struct Bin { long long count{}; double energy{}; };
    struct ChannelState { State first, second; };
    static constexpr int segmentsShort = 30, segmentsMomentary = 4;
    static constexpr float histogramLow = absoluteGate, histogramStep = .01f;
    static constexpr int histogramBins = int((10.f - histogramLow) / histogramStep) + 1;   // -70 to +10 LUFS

    static double tick(const Coefficients& c, State& s, double x) noexcept {
        const double y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }
    static float lufs(double meanSquare) noexcept {
        return meanSquare > 0. ? float(-.691 + 10. * std::log10(meanSquare)) : silent;
    }
    // Filter and interpolator state belong to the signal, not the reading:
    // a reset mid-programme keeps them, so it adds no step to the audio seen.
    void clearFilters() noexcept {
        for (auto& s : state) s = {};
        for (auto& h : history) h.clear();
    }
    void reset() noexcept {
        segments.fill(0.); segmentCount = 0; segmentFill = 0; segmentEnergy = 0.; next = 0;
        for (auto& b : histogram) b = {};
        gatedBlocks = 0; gatedEnergy = 0.; peak = 0.f;
        for (auto& b : rangeHistogram) b = 0;
        rangeValues = 0; rangeEnergy = 0.; rangeLu.store(0.f);
        momentaryLufs.store(silent); shortTermLufs.store(silent); integratedLufs.store(silent); truePeakDb.store(silent);
    }
    // 100 ms segment done: update momentary and short-term, add one 400 ms
    // gating block to the integrated histogram, refresh the gated result.
    void closeSegment() noexcept {
        segments[size_t(next)] = segmentEnergy / hop;
        next = (next + 1) % segmentsShort; segmentCount = juce::jmin(segmentCount + 1, segmentsShort);
        segmentEnergy = 0.; segmentFill = 0;
        const auto mean = [&](int count) {
            double sum = 0.;
            for (int i = 1; i <= count; ++i) sum += segments[size_t((next - i + segmentsShort) % segmentsShort)];
            return sum / count;
        };
        if (segmentCount >= segmentsMomentary) {
            const double block = mean(segmentsMomentary);
            momentaryLufs.store(lufs(block));
            const float level = lufs(block);
            if (level > absoluteGate) {
                const int bin = juce::jlimit(0, histogramBins - 1, int((level - histogramLow) / histogramStep));
                histogram[size_t(bin)].count++; histogram[size_t(bin)].energy += block;
                gatedBlocks++; gatedEnergy += block;
                updateIntegrated();
            }
        }
        if (segmentCount >= segmentsShort) {
            const double block = mean(segmentsShort);
            const float level = lufs(block);
            shortTermLufs.store(level);
            if (level > absoluteGate) {
                rangeHistogram[size_t(juce::jlimit(0, histogramBins - 1, int((level - histogramLow) / histogramStep)))]++;
                rangeValues++; rangeEnergy += block;
                updateRange();
            }
        }
        truePeakDb.store(peak > 0.f ? float(20. * std::log10(double(peak))) : silent);
    }
    void updateIntegrated() noexcept {
        // Relative gate: 10 LU below the mean of all blocks above -70 LUFS.
        const float threshold = lufs(gatedEnergy / double(gatedBlocks)) + relativeGate;
        long long count = 0; double energy = 0.;
        const int first = juce::jlimit(0, histogramBins, int(std::ceil((threshold - histogramLow) / histogramStep)));
        for (int b = first; b < histogramBins; ++b) { count += histogram[size_t(b)].count; energy += histogram[size_t(b)].energy; }
        integratedLufs.store(count > 0 ? lufs(energy / double(count)) : silent);
    }
    // EBU Tech 3342: short-term values above -70 LUFS, then those within
    // 20 LU of their power mean; LRA = 95th minus 10th percentile.
    void updateRange() noexcept {
        const float threshold = lufs(rangeEnergy / double(rangeValues)) - 20.f;
        const int first = juce::jlimit(0, histogramBins, int(std::ceil((threshold - histogramLow) / histogramStep)));
        long long total = 0;
        for (int b = first; b < histogramBins; ++b) total += rangeHistogram[size_t(b)];
        if (total == 0) { rangeLu.store(0.f); return; }
        const auto percentile = [&](double fraction) {
            const long long rank = juce::jlimit(1LL, total, (long long)std::ceil(fraction * double(total)));
            long long seen = 0;
            for (int b = first; b < histogramBins; ++b) if ((seen += rangeHistogram[size_t(b)]) >= rank) return histogramLow + (b + .5f) * histogramStep;
            return histogramLow + (histogramBins - .5f) * histogramStep;
        };
        rangeLu.store(juce::jmax(0.f, percentile(.95) - percentile(.10)));
    }
    // The interpolator shared with the Studio limiter (TruePeak.h).
    void designInterpolator() { phases = truePeakPhases(factor); }
    void peakTick(int channel, float x) noexcept {
        peak = juce::jmax(peak, std::abs(x));   // never below the sample peak
        if (factor == 1) return;
        peak = juce::jmax(peak, history[size_t(channel)].push(x, phases, factor));
    }

    double rate{48000.};
    int hop{4800}, factor{4}, segmentFill{}, segmentCount{}, next{};
    Coefficients shelf{}, highPass{};
    std::array<ChannelState, 2> state{};
    std::array<double, segmentsShort> segments{};
    double segmentEnergy{}, gatedEnergy{};
    long long gatedBlocks{};
    std::vector<Bin> histogram;
    std::vector<long long> rangeHistogram;
    long long rangeValues{};
    double rangeEnergy{};
    TruePeakPhases phases{};
    std::array<TruePeakHistory, 2> history{};
    float peak{};
    std::atomic<bool> resetRequested{false};
    std::atomic<float> momentaryLufs{silent}, shortTermLufs{silent}, integratedLufs{silent}, truePeakDb{silent}, rangeLu{0.f};
};

} // namespace spectralforge
