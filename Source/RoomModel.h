#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace spectralforge {

// Room Core 1.3.3 room model (docs/ROOM_CORE_1_3_3.md section 4): early
// reflections from image sources of a declared rectangular room plus an
// 8-line feedback delay network for the late decay. Stereo in, stereo wet
// out (added to the output). All memory is allocated in prepare().
class RoomModel {
public:
    enum Size { small = 0, medium = 1, large = 2 };
    struct Geometry { float length, width, height, rt60AtHalf; };
    // Declared rooms (metres) and the late RT60 at 50 % absorption.
    static Geometry geometry(int size) noexcept {
        static constexpr std::array<Geometry, 3> rooms{{{3.5f, 3.0f, 2.5f, .35f}, {7.f, 5.f, 3.f, .7f}, {14.f, 10.f, 6.f, 1.4f}}};
        return rooms[size_t(std::clamp(size, 0, 2))];
    }
    // RT60 scale from absorption (0-1): 1.6x at 0, 1x at 0.5, 0.4x at 1.
    static float rt60Scale(float absorption) noexcept {
        const float a = std::clamp(absorption, 0.f, 1.f);
        return a <= .5f ? std::pow(1.6f, 1.f - 2.f * a) : std::pow(.4f, 2.f * a - 1.f);
    }
    // Damping low-pass cutoff in each delay line: 12 kHz at 0 down to 2.5 kHz at 1.
    static float dampingCutoff(float absorption) noexcept {
        return 12000.f * std::pow(2500.f / 12000.f, std::clamp(absorption, 0.f, 1.f));
    }
    // Mean free path 4V/S (m), which scales the late network's line lengths.
    static float meanFreePath(int size) noexcept {
        const auto g = geometry(size);
        const float volume = g.length * g.width * g.height;
        const float surface = 2.f * (g.length * g.width + g.length * g.height + g.width * g.height);
        return 4.f * volume / surface;
    }

    void prepare(double sampleRate) {
        rate = sampleRate;
        // Sized for the Large room: 80 ms of reflections, lines up to 2.23 x 3 mean free paths.
        earlyLength = int(std::ceil(rate * .095)) + 2;
        for (auto& ring : early) ring.assign(size_t(earlyLength), 0.f);
        const double longest = 2.3 * 3.0 * meanFreePath(large) / speedOfSound;
        lineCapacity = int(std::ceil(rate * longest)) + 2;
        for (auto& line : lines) line.assign(size_t(lineCapacity), 0.f);
        configure(medium, .5f);
        reset();
    }
    void reset() noexcept {
        for (auto& ring : early) std::fill(ring.begin(), ring.end(), 0.f);
        for (auto& line : lines) std::fill(line.begin(), line.end(), 0.f);
        lowpass.fill(0.f);
        earlyWrite = 0;
        lineWrite = 0;
    }
    // Room size and absorption (0-1). Allocation-free; call per block.
    void configure(int size, float absorption) noexcept {
        size = std::clamp(size, 0, 2);
        absorption = std::clamp(absorption, 0.f, 1.f);
        if (size != currentSize) {
            currentSize = size;
            placeReflections();
            const double base = 3.0 * meanFreePath(size) / speedOfSound;
            static constexpr std::array<double, 8> spread{1.0, 1.13, 1.27, 1.41, 1.58, 1.77, 1.99, 2.23};
            for (size_t i = 0; i < lines.size(); ++i)
                lineLength[i] = std::min(lineCapacity - 1, std::max(1, int(std::lround(rate * base * spread[i]))));
            currentAbsorption = -1.f;
        }
        if (absorption == currentAbsorption) return;
        // A new size (or a fresh model) starts at its gains; an absorption
        // change ramps to them across the next processed block.
        const bool snap = currentAbsorption < 0.f;
        currentAbsorption = absorption;
        const float reflection = std::sqrt(1.f - wallAbsorption(absorption));
        for (int ear = 0; ear < 2; ++ear)
            for (int t = 0; t < tapCount[size_t(ear)]; ++t) {
                const auto& tap = taps[size_t(ear)][size_t(t)];
                tapTarget[size_t(ear)][size_t(t)] = tap.spreading * std::pow(reflection, float(tap.order));
            }
        // The damping low-pass also shortens the broadband decay: measured RT60
        // came out at 0.85-0.94 x the target, so line gains aim 1.12 x longer.
        const double rt60 = 1.12 * geometry(size).rt60AtHalf * rt60Scale(absorption);
        for (size_t i = 0; i < lines.size(); ++i)
            lineTarget[i] = float(std::pow(10.0, -3.0 * double(lineLength[i]) / (rt60 * rate)));
        dampingTarget = 1.f - std::exp(-juce::MathConstants<float>::twoPi * dampingCutoff(absorption) / float(rate));
        if (snap) { tapGain = tapTarget; lineGain = lineTarget; damping = dampingTarget; }
        ramping = !snap;
    }
    // Adds the wet room signal for `input` (1 or 2 channels) to `output`.
    // `inputGain` scales the excitation; `wetGain` the return.
    void process(const float* const* input, float* const* output, int channels, int samples, float wetGain) noexcept {
        // Per-block absorption updates would step every gain at the block
        // edge, an audible zipper; they move linearly across the block instead.
        std::array<std::array<float, maxTaps>, 2> tapStep{};
        std::array<float, 8> lineStep{};
        float dampingStep = 0.f;
        if (ramping && samples > 0) {
            const float share = 1.f / float(samples);
            for (int ear = 0; ear < 2; ++ear)
                for (int t = 0; t < tapCount[size_t(ear)]; ++t)
                    tapStep[size_t(ear)][size_t(t)] = (tapTarget[size_t(ear)][size_t(t)] - tapGain[size_t(ear)][size_t(t)]) * share;
            for (size_t i = 0; i < lines.size(); ++i) lineStep[i] = (lineTarget[i] - lineGain[i]) * share;
            dampingStep = (dampingTarget - damping) * share;
        }
        for (int n = 0; n < samples; ++n) {
            if (ramping) {
                for (int ear = 0; ear < 2; ++ear)
                    for (int t = 0; t < tapCount[size_t(ear)]; ++t) tapGain[size_t(ear)][size_t(t)] += tapStep[size_t(ear)][size_t(t)];
                for (size_t i = 0; i < lines.size(); ++i) lineGain[i] += lineStep[i];
                damping += dampingStep;
            }
            const float left = input[0][n], right = channels > 1 ? input[1][n] : left;
            early[0][size_t(earlyWrite)] = left;
            early[1][size_t(earlyWrite)] = right;
            std::array<float, 2> reflections{};
            for (int ear = 0; ear < 2; ++ear) {
                float sum = 0.f;
                for (int t = 0; t < tapCount[size_t(ear)]; ++t) {
                    int index = earlyWrite - taps[size_t(ear)][size_t(t)].delay;
                    if (index < 0) index += earlyLength;
                    sum += tapGain[size_t(ear)][size_t(t)] * early[size_t(ear)][size_t(index)];
                }
                reflections[size_t(ear)] = sum;
            }
            if (++earlyWrite == earlyLength) earlyWrite = 0;
            // Late network: read, damp, mix (Householder), feed back with input.
            std::array<float, 8> out{};
            float total = 0.f;
            for (size_t i = 0; i < lines.size(); ++i) {
                int index = lineWrite - lineLength[i];
                if (index < 0) index += lineCapacity;
                lowpass[i] += damping * (lines[i][size_t(index)] - lowpass[i]);
                out[i] = lowpass[i] * lineGain[i];
                total += out[i];
            }
            const float householder = total * (2.f / float(lines.size()));
            float lateLeft = 0.f, lateRight = 0.f;
            for (size_t i = 0; i < lines.size(); ++i) {
                const float excitation = (i & 1u) == 0 ? left : right;
                lines[i][size_t(lineWrite)] = out[i] - householder + excitation * lateInput;
                lateLeft += out[i] * outputSign[0][i];
                lateRight += out[i] * outputSign[1][i];
            }
            if (++lineWrite == lineCapacity) lineWrite = 0;
            const float earlyGain = earlyLevel[size_t(currentSize)], lateGain = lateLevel[size_t(currentSize)];
            output[0][n] += wetGain * (reflections[0] * earlyGain + lateLeft * lateGain);
            if (channels > 1) output[1][n] += wetGain * (reflections[1] * earlyGain + lateRight * lateGain);
        }
        if (ramping && samples > 0) { tapGain = tapTarget; lineGain = lineTarget; damping = dampingTarget; ramping = false; }
    }

    int size() const noexcept { return currentSize; }
    int reflectionCount(int ear) const noexcept { return tapCount[size_t(std::clamp(ear, 0, 1))]; }
    // Latest early reflection, in samples after the direct sound.
    int lastReflectionDelay() const noexcept {
        int last = 0;
        for (int ear = 0; ear < 2; ++ear)
            for (int t = 0; t < tapCount[size_t(ear)]; ++t) last = std::max(last, taps[size_t(ear)][size_t(t)].delay);
        return last;
    }

private:
    struct Tap { int delay; float spreading; int order; };
    static constexpr double speedOfSound = 343.0;
    static constexpr int maxTaps = 32;
    // Absorption control (0-1) to a wall energy absorption coefficient.
    static float wallAbsorption(float absorption) noexcept { return .05f + .85f * absorption; }

    // First- and second-order image sources (no two-wall corners beyond order
    // 2) for a source and two ears at fixed positions in the declared room.
    void placeReflections() noexcept {
        const auto g = geometry(currentSize);
        const std::array<float, 3> room{g.length, g.width, g.height};
        const std::array<float, 3> source{.3f * g.length, .45f * g.width, 1.2f};
        for (int ear = 0; ear < 2; ++ear) {
            const std::array<float, 3> listener{.65f * g.length, .55f * g.width + (ear == 0 ? -.1f : .1f), 1.2f};
            const float direct = distance(source, listener);
            int count = 0;
            for (int ix = -2; ix <= 2; ++ix)
                for (int iy = -2; iy <= 2; ++iy)
                    for (int iz = -2; iz <= 2; ++iz) {
                        const int order = std::abs(ix) + std::abs(iy) + std::abs(iz);
                        if (order == 0 || order > 2 || count >= maxTaps) continue;
                        const std::array<int, 3> index{ix, iy, iz};
                        std::array<float, 3> image{};
                        for (size_t axis = 0; axis < 3; ++axis) {
                            // Image k along an axis of length L: k L + x (k even) or k L + L - x (k odd);
                            // k = 1 is 2L - x, k = -1 is -x, k = +-2 is x +- 2L.
                            const int k = index[axis];
                            image[axis] = float(k) * room[axis] + (k % 2 == 0 ? source[axis] : room[axis] - source[axis]);
                        }
                        const float path = distance(image, listener);
                        const int delay = int(std::lround((path - direct) / speedOfSound * rate));
                        if (delay <= 0 || delay >= earlyLength - 1) continue;
                        taps[size_t(ear)][size_t(count++)] = Tap{delay, direct / path, order};
                    }
            tapCount[size_t(ear)] = count;
        }
    }
    static float distance(const std::array<float, 3>& a, const std::array<float, 3>& b) noexcept {
        float sum = 0.f;
        for (size_t axis = 0; axis < 3; ++axis) sum += (a[axis] - b[axis]) * (a[axis] - b[axis]);
        return std::sqrt(sum);
    }

    double rate{48000.0};
    int currentSize{-1};
    float currentAbsorption{-1.f};
    std::array<std::vector<float>, 2> early;
    int earlyLength{}, earlyWrite{};
    std::array<std::array<Tap, maxTaps>, 2> taps{};
    std::array<std::array<float, maxTaps>, 2> tapGain{}, tapTarget{};
    std::array<int, 2> tapCount{};
    std::array<std::vector<float>, 8> lines;
    std::array<int, 8> lineLength{};
    std::array<float, 8> lineGain{}, lineTarget{}, lowpass{};
    int lineCapacity{}, lineWrite{};
    float damping{1.f}, dampingTarget{1.f};
    bool ramping{};
    // Level calibration (white-noise energy, docs/ROOM_CORE_1_3_3.md section 2):
    // at 50 % absorption the early block and the late tail each carry -12 dB of
    // the excitation's energy. Measured per size from the impulse responses
    // (early 0.955 / 1.392 / 1.480, late 0.0222 / 0.0302 / 0.0302 at a late
    // output of 0.35); absorption keeps its own effect on both.
    static constexpr std::array<float, 3> earlyLevel{.25710f, .21294f, .20646f};
    static constexpr std::array<float, 3> lateLevel{.59006f, .50590f, .50590f};
    float lateInput{.35f};
    static constexpr std::array<std::array<float, 8>, 2> outputSign{{{1, -1, 1, 1, -1, 1, -1, -1}, {1, 1, -1, 1, 1, -1, -1, 1}}};
};

} // namespace spectralforge
