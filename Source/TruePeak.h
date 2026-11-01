#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <vector>

namespace spectralforge {

// The true-peak interpolator shared by the loudness meter and the Studio
// limiter (docs/TRUE_PEAK_LIMITER_1_5.md), so the limiter holds exactly the
// peaks the meter reads: 4x below 160 kHz, 2x above, a 64-tap (32 at 2x)
// Hann-windowed sinc, 16 taps per phase. Phase 0 is the sample itself, and
// every phase lies within one sample of the input `truePeakDelay` samples ago.
inline constexpr int truePeakTapsPerPhase = 16;
inline constexpr int truePeakDelay = truePeakTapsPerPhase / 2;
using TruePeakPhases = std::array<std::array<float, truePeakTapsPerPhase>, 4>;

inline int truePeakFactor(double sampleRate) noexcept { return sampleRate < 160000. ? 4 : 2; }

inline TruePeakPhases truePeakPhases(int factor) {
    TruePeakPhases phases{};
    const int length = truePeakTapsPerPhase * factor;
    // An integer centre puts the phases at 0, 1/factor, ... of a sample,
    // so phase 0 is the sample itself.
    const double centre = length / 2;
    std::vector<double> h(static_cast<size_t>(length));
    for (int k = 0; k < length; ++k) {
        const double t = (k - centre) / factor, x = juce::MathConstants<double>::pi * t;
        const double sinc = std::abs(t) < 1e-12 ? 1. : std::sin(x) / x;
        const double w = .5 * (1. - std::cos(juce::MathConstants<double>::twoPi * k / length));   // Hann, symmetric about the centre
        h[size_t(k)] = sinc * w;
    }
    for (int p = 0; p < factor; ++p) {
        double sum = 0.;
        for (int t = 0; t < truePeakTapsPerPhase; ++t) sum += h[size_t(t * factor + p)];
        for (int t = 0; t < truePeakTapsPerPhase; ++t) phases[size_t(p)][size_t(t)] = float(h[size_t(t * factor + p)] / sum);
    }
    return phases;
}

// One channel's interpolator history. push() stores a sample and returns the
// largest magnitude among the `factor` interpolated points around the
// sample `truePeakDelay` steps back (phase 0 is that sample).
struct TruePeakHistory {
    std::array<float, truePeakTapsPerPhase> line{};
    int position{};
    void clear() noexcept { line.fill(0.f); position = 0; }
    float push(float x, const TruePeakPhases& phases, int factor) noexcept {
        line[size_t(position)] = x;
        const int start = position;
        position = (start + 1) % truePeakTapsPerPhase;
        float peak = 0.f;
        for (int p = 0; p < factor; ++p) {
            float y = 0.f;
            for (int t = 0; t < truePeakTapsPerPhase; ++t) y += phases[size_t(p)][size_t(t)] * line[size_t((start - t + truePeakTapsPerPhase) % truePeakTapsPerPhase)];
            peak = juce::jmax(peak, std::abs(y));
        }
        return peak;
    }
};

} // namespace spectralforge
