#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TruePeak.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace spectralforge {

// Studio true-peak mode of the output limiter (docs/TRUE_PEAK_LIMITER_1_5.md),
// appended after every other bank at its own version. Live (the default)
// leaves the existing zero-latency sample-peak limiter, and sessions,
// unchanged.
inline constexpr int truePeakLimiterParameterVersion = 32;
inline constexpr int truePeakLimiterParameterCount = 1;
inline juce::StringArray limiterModeNames() { return {"Live", "Studio true peak"}; }
inline void appendTruePeakLimiterParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"limiterMode", truePeakLimiterParameterVersion}, "Output limiter mode", limiterModeNames(), 0));
}

// Lookahead true-peak limiter, stereo linked.
//
// Detector: the interpolator the loudness meter uses (TruePeak.h) gives, per
// input sample, the largest true-peak point within one sample of the input
// `truePeakDelay` samples back. The gain that point needs is ceiling / peak.
// Gain: a running minimum over `window + 2` of those needs, a release that
// only lets the gain rise, then a `window`-long moving average. Every value
// the average takes in is a minimum that covers the same three detector
// points, so the averaged gain never exceeds what any of them needs: the
// ramp down is finished when the peak arrives and has no step.
// The audio is delayed by `window + truePeakDelay`; that is the latency.
// With no reduction the gain is exactly 1 and the output is the input,
// delayed, bit for bit.
class TruePeakLimiter {
public:
    static constexpr double lookaheadSeconds = .001;
    static constexpr double margin = 0.998849369;   // -0.01 dB

    void prepare(double sampleRate) {
        factor = truePeakFactor(sampleRate);
        phases = truePeakPhases(factor);
        window = std::max(1, juce::roundToInt(sampleRate * lookaheadSeconds));
        delayLength = window + truePeakDelay;
        for (auto& d : delay) d.assign(size_t(delayLength), 0.f);
        minimumSpan = window + 2;
        queueIndex.assign(size_t(minimumSpan + 1), 0);
        queueValue.assign(size_t(minimumSpan + 1), 1.);
        average.assign(size_t(window), 1.);
        reset();
    }
    void reset() noexcept {
        for (auto& d : delay) std::fill(d.begin(), d.end(), 0.f);
        for (auto& h : history) h.clear();
        std::fill(average.begin(), average.end(), 1.);
        sum = double(window); averagePosition = 0; delayPosition = 0;
        head = 0; count = 0; counter = 0; released = 1.;
    }
    int latency() const noexcept { return delayLength; }
    int oversampling() const noexcept { return factor; }

    // One frame (1 or 2 channels) in place; returns the gain applied.
    double process(float* frame, int channels, double ceiling, double release) noexcept {
        float peak = 0.f;
        for (int c = 0; c < channels; ++c)
            peak = std::max(peak, history[size_t(c)].push(frame[c], phases, factor));
        // Aim 0.01 dB under the ceiling: the gain moves between the samples an
        // inter-sample peak is built from, which can add a few thousandths of
        // a dB (docs/TRUE_PEAK_LIMITER_1_5.md).
        const double target = ceiling * margin;
        const double need = double(peak) > target ? target / double(peak) : 1.;
        // Running minimum over the last `minimumSpan` needs (monotonic queue).
        const int slots = int(queueValue.size());
        while (count > 0 && queueValue[size_t((head + count - 1) % slots)] >= need) --count;
        const int slot = (head + count) % slots;
        queueIndex[size_t(slot)] = counter; queueValue[size_t(slot)] = need; ++count;
        while (counter - queueIndex[size_t(head)] >= minimumSpan) { head = (head + 1) % slots; --count; }
        const double minimum = queueValue[size_t(head)];
        ++counter;
        released = std::min(minimum, 1. + (released - 1.) * release);
        if (released > 1. - 1e-9) released = 1.;
        sum += released - average[size_t(averagePosition)];
        average[size_t(averagePosition)] = released;
        if (++averagePosition == window) {
            averagePosition = 0;
            double exact = 0.; for (double a : average) exact += a;   // no drift
            sum = exact;
        }
        const double gain = std::min(1., sum / double(window));
        for (int c = 0; c < channels; ++c) {
            auto& d = delay[size_t(c)];
            const float delayed = d[size_t(delayPosition)];
            d[size_t(delayPosition)] = frame[c];
            frame[c] = gain == 1. ? delayed : float(double(delayed) * gain);
        }
        if (++delayPosition == delayLength) delayPosition = 0;
        return gain;
    }

private:
    // The queue is a ring of minimumSpan + 1 slots from `head`.
    int factor{4}, window{48}, delayLength{56}, minimumSpan{50};
    TruePeakPhases phases{};
    std::array<TruePeakHistory, 2> history{};
    std::array<std::vector<float>, 2> delay{};
    std::vector<long long> queueIndex;
    std::vector<double> queueValue, average;
    double sum{48.}, released{1.};
    int averagePosition{}, delayPosition{}, head{}, count{};
    long long counter{};
};

} // namespace spectralforge
