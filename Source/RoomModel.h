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
// Room Advanced 1.8 shaping (docs/ROOM_ADVANCED_1_8.md). The defaults are
// exactly the Room Core room: weights 1, decay 1x, diffusion 0.
struct RoomShape { float early{1.f}, late{1.f}, diffusion{0.f}; };

// Room Advanced 1.8 spatial stage (docs/ROOM_SPACE_1_8.md): surface materials
// with Sabine absorption coefficients at 125 / 250 / 500 / 1k / 2k / 4k Hz, as
// commonly tabulated in room-acoustics handbooks. Approximate published
// values, not measurements of particular surfaces.
struct RoomMaterial { const char* name; std::array<float, 6> absorption; };
inline constexpr std::array<RoomMaterial, 10> roomMaterials{{
    {"Concrete", {.01f, .01f, .02f, .02f, .02f, .02f}},
    {"Brick", {.03f, .03f, .03f, .04f, .05f, .07f}},
    {"Plaster", {.01f, .02f, .02f, .03f, .04f, .05f}},
    {"Gypsum board", {.29f, .10f, .05f, .04f, .07f, .09f}},
    {"Wood floor", {.15f, .11f, .10f, .07f, .06f, .07f}},
    {"Wood panel", {.28f, .22f, .17f, .09f, .10f, .11f}},
    {"Carpet", {.02f, .06f, .14f, .37f, .60f, .65f}},
    {"Heavy curtain", {.14f, .35f, .55f, .72f, .70f, .65f}},
    {"Acoustic tile", {.70f, .66f, .72f, .92f, .88f, .75f}},
    {"Glass", {.18f, .06f, .04f, .03f, .02f, .02f}}}};
// A declared rectangular room (metres): dimensions, surface materials, the
// cabinet's position and a spaced room-mic pair. Positions are measured from
// the back wall (length), the left wall (across, as a fraction of the width)
// and the floor; they are kept 0.1 m inside the walls.
struct RoomSpace {
    float length{7.f}, width{5.f}, height{3.f};
    int floor{6}, walls{5}, ceiling{3};
    float cabWall{2.1f}, cabAcross{.45f}, cabHeight{1.2f};
    float micDistance{2.45f}, micAcross{.55f}, micHeight{1.2f}, micSpacing{.2f};
    bool sameGeometry(const RoomSpace& o) const noexcept {
        return length == o.length && width == o.width && height == o.height && cabWall == o.cabWall && cabAcross == o.cabAcross
            && cabHeight == o.cabHeight && micDistance == o.micDistance && micAcross == o.micAcross && micHeight == o.micHeight
            && micSpacing == o.micSpacing;
    }
    bool sameSurfaces(const RoomSpace& o) const noexcept { return floor == o.floor && walls == o.walls && ceiling == o.ceiling; }
    static constexpr float margin = .1f;
    std::array<float, 3> cabinet() const noexcept {
        return {std::clamp(cabWall, margin, length - margin), std::clamp(cabAcross * width, margin, width - margin), std::clamp(cabHeight, margin, height - margin)};
    }
    // Ear 0 / 1 of the pair, spaced across the room around the mic position.
    std::array<float, 3> mic(int ear) const noexcept {
        const float x = std::clamp(cabinet()[0] + micDistance, margin, length - margin);
        const float y = std::clamp(micAcross * width + (ear == 0 ? -.5f : .5f) * micSpacing, margin, width - margin);
        return {x, y, std::clamp(micHeight, margin, height - margin)};
    }
    float volume() const noexcept { return length * width * height; }
    float surface() const noexcept { return 2.f * (length * width + length * height + width * height); }
    // Area-weighted mean absorption in one octave band (0-5: 125 Hz-4 kHz).
    float meanAbsorption(int band) const noexcept {
        const auto alpha = [band](int m) { return roomMaterials[size_t(std::clamp(m, 0, int(roomMaterials.size()) - 1))].absorption[size_t(band)]; };
        const float plane = length * width, sides = 2.f * (length * height + width * height);
        return (plane * (alpha(floor) + alpha(ceiling)) + sides * alpha(walls)) / surface();
    }
    // Eyring reverberation time with air absorption (4mV; about 30 dB/km at
    // 4 kHz, 20 C / 50 % RH; neglected at 500 Hz).
    double eyring(int band) const noexcept {
        const double alpha = std::min(.95, double(meanAbsorption(band)));
        const double air = band >= 5 ? 29.7 / (1000.0 * 4.343) : 0.0;
        return .161 * double(volume()) / (-double(surface()) * std::log(1.0 - alpha) + 4.0 * air * double(volume()));
    }
};

class RoomModel {
public:
    using Shape = RoomShape;
    using Space = RoomSpace;
    // size() of a model running a declared room.
    enum Size { small = 0, medium = 1, large = 2, declared = 3 };
    // Bounds of a declared room (metres).
    static constexpr std::array<float, 3> spaceMinimum{2.f, 2.f, 2.f}, spaceMaximum{30.f, 20.f, 10.f};
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
        // Sized for the largest declared room (30 x 20 x 10 m): second-order
        // reflections up to 200 ms, lines up to 2.23 x 3 mean free paths.
        earlyLength = int(std::ceil(rate * .2)) + 2;
        for (auto& ring : early) ring.assign(size_t(earlyLength), 0.f);
        const float largestPath = 4.f * spaceMaximum[0] * spaceMaximum[1] * spaceMaximum[2]
            / (2.f * (spaceMaximum[0] * spaceMaximum[1] + spaceMaximum[0] * spaceMaximum[2] + spaceMaximum[1] * spaceMaximum[2]));
        const double longest = 2.3 * 3.0 * std::max(double(meanFreePath(large)), double(largestPath)) / speedOfSound;
        lineCapacity = int(std::ceil(rate * longest)) + 2;
        for (auto& line : lines) line.assign(size_t(lineCapacity), 0.f);
        // Four Schroeder all-passes per side for the diffusion blend.
        static constexpr std::array<double, 4> diffuserSeconds{.0031, .0047, .0071, .0109};
        for (size_t k = 0; k < diffusers[0].size(); ++k)
            for (auto& side : diffusers) side[k].assign(size_t(std::max(2.0, std::round(rate * diffuserSeconds[k]))), 0.f);
        configure(medium, .5f);
        reset();
    }
    void reset() noexcept {
        for (auto& ring : early) std::fill(ring.begin(), ring.end(), 0.f);
        for (auto& line : lines) std::fill(line.begin(), line.end(), 0.f);
        lowpass.fill(0.f);
        earlyWrite = 0;
        lineWrite = 0;
        for (auto& side : diffusers) for (auto& d : side) std::fill(d.begin(), d.end(), 0.f);
        diffuserWrite = {};
    }
    // Room size, absorption (0-1) and the 1.8 decay scale (0.25-4, RT60
    // multiplier). Allocation-free; call per block.
    void configure(int size, float absorption, float decay = 1.f) noexcept {
        size = std::clamp(size, 0, 2);
        absorption = std::clamp(absorption, 0.f, 1.f);
        decay = std::clamp(decay, .25f, 4.f);
        if (decay != currentDecay && size == currentSize && absorption == currentAbsorption) currentAbsorption = -2.f;   // recompute, ramped
        currentDecay = decay;
        if (size != currentSize) {
            currentSize = size;
            earlyCalibration = earlyLevel[size_t(size)];
            lateCalibration = lateLevel[size_t(size)];
            placeReflections();
            const double base = 3.0 * meanFreePath(size) / speedOfSound;
            static constexpr std::array<double, 8> spread{1.0, 1.13, 1.27, 1.41, 1.58, 1.77, 1.99, 2.23};
            for (size_t i = 0; i < lines.size(); ++i)
                lineLength[i] = std::min(lineCapacity - 1, std::max(1, int(std::lround(rate * base * spread[i]))));
            currentAbsorption = -1.f;
        }
        if (absorption == currentAbsorption) return;
        // A new size (or a fresh model) starts at its gains; an absorption
        // or decay change ramps to them across the next processed block.
        const bool snap = currentAbsorption == -1.f;
        currentAbsorption = absorption;
        const float reflection = std::sqrt(1.f - wallAbsorption(absorption));
        for (int ear = 0; ear < 2; ++ear)
            for (int t = 0; t < tapCount[size_t(ear)]; ++t) {
                const auto& tap = taps[size_t(ear)][size_t(t)];
                tapTarget[size_t(ear)][size_t(t)] = tap.spreading * std::pow(reflection, float(tap.order));
            }
        // The damping low-pass also shortens the broadband decay: measured RT60
        // came out at 0.85-0.94 x the target, so line gains aim 1.12 x longer.
        const double rt60 = 1.12 * geometry(size).rt60AtHalf * rt60Scale(absorption) * double(currentDecay);
        for (size_t i = 0; i < lines.size(); ++i)
            lineTarget[i] = float(std::pow(10.0, -3.0 * double(lineLength[i]) / (rt60 * rate)));
        dampingTarget = 1.f - std::exp(-juce::MathConstants<float>::twoPi * dampingCutoff(absorption) / float(rate));
        if (snap) { tapGain = tapTarget; lineGain = lineTarget; damping = dampingTarget; }
        ramping = !snap;
    }
    // A declared room and the decay scale. A new geometry is placed and starts
    // at its gains (callers crossfade between models for that); new surface
    // materials or decay ramp across the next processed block.
    // Allocation-free; call per block.
    void configureSpace(Space space, float decay = 1.f) noexcept {
        space.length = std::clamp(space.length, spaceMinimum[0], spaceMaximum[0]);
        space.width = std::clamp(space.width, spaceMinimum[1], spaceMaximum[1]);
        space.height = std::clamp(space.height, spaceMinimum[2], spaceMaximum[2]);
        decay = std::clamp(decay, .25f, 4.f);
        const bool place = currentSize != declared || !space.sameGeometry(currentSpace);
        if (!place && space.sameSurfaces(currentSpace) && decay == currentDecay) return;
        currentDecay = decay;
        if (place) {
            currentSize = declared;
            currentSpace = space;
            placeDeclared();
            const double base = 3.0 * double(4.f * space.volume() / space.surface()) / speedOfSound;
            static constexpr std::array<double, 8> spread{1.0, 1.13, 1.27, 1.41, 1.58, 1.77, 1.99, 2.23};
            for (size_t i = 0; i < lines.size(); ++i)
                lineLength[i] = std::min(lineCapacity - 1, std::max(1, int(std::lround(rate * base * spread[i]))));
            // Levels are referenced to the cabinet heard at the Medium room's
            // mic distance (referenceDistance): reflections follow 1 / path, and
            // the diffuse tail energy follows T60 / V. The network's energy
            // already grows with T60 / (mean line length), and the mean line
            // length with V / S, so the late trim is the Medium trim x sqrt(S_medium / S).
            const auto m = geometry(medium);
            const float mediumSurface = 2.f * (m.length * m.width + m.length * m.height + m.width * m.height);
            earlyCalibration = earlyLevel[size_t(medium)];
            lateCalibration = lateLevel[size_t(medium)] * std::sqrt(mediumSurface / space.surface());
        }
        currentSpace.floor = space.floor;currentSpace.walls = space.walls;currentSpace.ceiling = space.ceiling;
        // Early reflections: each surface hit scales the pressure by
        // sqrt(1 - alpha), alpha the 500 Hz-1 kHz mean of its material.
        const auto reflection = [&](int material) {
            const auto& a = roomMaterials[size_t(std::clamp(material, 0, int(roomMaterials.size()) - 1))].absorption;
            return std::sqrt(1.f - .5f * (a[2] + a[3]));
        };
        const float wall = reflection(space.walls), floorReflection = reflection(space.floor), ceilingReflection = reflection(space.ceiling);
        for (int ear = 0; ear < 2; ++ear)
            for (int t = 0; t < tapCount[size_t(ear)]; ++t) {
                const auto& tap = taps[size_t(ear)][size_t(t)];
                tapTarget[size_t(ear)][size_t(t)] = tap.spreading * std::pow(wall, float(tap.wallHits))
                    * std::pow(floorReflection, float(tap.floorHits)) * std::pow(ceilingReflection, float(tap.ceilingHits));
            }
        // Late decay: Eyring T60 at 500 Hz sets the line gains, the 4 kHz T60
        // the damping low-pass. The low-pass's own loss at 500 Hz is divided
        // out of the line gains, so the 500 Hz decay is the Eyring time.
        const double low = currentSpace.eyring(2) * double(decay), high = currentSpace.eyring(5) * double(decay);
        double meanLength = 0.0;
        for (const int length : lineLength) meanLength += double(length) / double(lineLength.size());
        const double lowHz = 500.0, highHz = 4000.0;
        const auto response = [this](double a, double hz) {
            const double w = juce::MathConstants<double>::twoPi * hz / rate, b = 1.0 - a;
            return a / std::sqrt(1.0 - 2.0 * b * std::cos(w) + b * b);
        };
        const double wanted = std::pow(10.0, -3.0 * meanLength / rate * (1.0 / high - 1.0 / low));
        double coefficient = 1.0;
        if (wanted < 1.0) {
            double lo = 1.e-4, hi = 1.0;
            for (int i = 0; i < 48; ++i) {
                const double mid = .5 * (lo + hi);
                (response(mid, highHz) / response(mid, lowHz) < wanted ? lo : hi) = mid;
            }
            coefficient = .5 * (lo + hi);
        }
        const double atLow = response(coefficient, lowHz);
        for (size_t i = 0; i < lines.size(); ++i)
            lineTarget[i] = float(std::min(.99995, std::pow(10.0, -3.0 * double(lineLength[i]) / (low * rate)) / atLow));
        dampingTarget = float(coefficient);
        if (place) { tapGain = tapTarget; lineGain = lineTarget; damping = dampingTarget; }
        ramping = !place;
    }
    const Space& space() const noexcept { return currentSpace; }
    // Distance at which declared-room levels are referenced (m).
    static constexpr float referenceDistance = 2.5f;
    // Adds the wet room signal for `input` (1 or 2 channels) to `output`.
    // `inputGain` scales the excitation; `wetGain` the return.
    // Starts the shaping at its target with no ramp: for a fresh or newly
    // switched-in model, which has no earlier output to ramp from.
    void snapShape(Shape shape) noexcept { earlyWeight = shape.early; lateWeight = shape.late; diffusionMix = shape.diffusion; }
    void process(const float* const* input, float* const* output, int channels, int samples, float wetGain, Shape shape = {}) noexcept {
        // Early/late weights and the diffusion blend move linearly across the
        // block, like the absorption gains.
        const float share = samples > 0 ? 1.f / float(samples) : 0.f;
        const float earlyStep = (shape.early - earlyWeight) * share, lateStep = (shape.late - lateWeight) * share;
        const float diffusionStep = (shape.diffusion - diffusionMix) * share;
        const bool diffusing = diffusionMix > 0.f || shape.diffusion > 0.f;
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
            earlyWeight += earlyStep;lateWeight += lateStep;diffusionMix += diffusionStep;
            const float left = input[0][n], right = channels > 1 ? input[1][n] : left;
            // Diffusion blends the late network's excitation towards a copy
            // smeared by four all-passes (g 0.7); 0 % leaves it untouched.
            float lateLeftIn = left, lateRightIn = right;
            if (diffusing) {
                const auto diffuse = [this](int side, float x) {
                    for (size_t k = 0; k < diffusers[size_t(side)].size(); ++k) {
                        auto& line = diffusers[size_t(side)][k];auto& write = diffuserWrite[size_t(side)][k];
                        const float delayed = line[write], v = x + .7f * delayed;
                        line[write] = v;x = delayed - .7f * v;
                        if (++write == line.size()) write = 0;
                    }
                    return x;
                };
                lateLeftIn = left + diffusionMix * (diffuse(0, left) - left);
                lateRightIn = right + diffusionMix * (diffuse(1, right) - right);
            }
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
                const float excitation = (i & 1u) == 0 ? lateLeftIn : lateRightIn;
                lines[i][size_t(lineWrite)] = out[i] - householder + excitation * lateInput;
                lateLeft += out[i] * outputSign[0][i];
                lateRight += out[i] * outputSign[1][i];
            }
            if (++lineWrite == lineCapacity) lineWrite = 0;
            const float earlyGain = earlyCalibration * earlyWeight, lateGain = lateCalibration * lateWeight;
            output[0][n] += wetGain * (reflections[0] * earlyGain + lateLeft * lateGain);
            if (channels > 1) output[1][n] += wetGain * (reflections[1] * earlyGain + lateRight * lateGain);
        }
        if (ramping && samples > 0) { tapGain = tapTarget; lineGain = lineTarget; damping = dampingTarget; ramping = false; }
        if (samples > 0) { earlyWeight = shape.early; lateWeight = shape.late; diffusionMix = shape.diffusion; }
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
    struct Tap { int delay; float spreading; int order; int wallHits{}, floorHits{}, ceilingHits{}; };
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
    // The same image sources for a declared room. Delays are after the direct
    // sound at each ear (the room mic's direct sound is not part of the wet
    // return); gains are referenceDistance / path. Each tap counts its wall,
    // floor and ceiling bounces for the surface materials.
    void placeDeclared() noexcept {
        const auto& s = currentSpace;
        const std::array<float, 3> room{s.length, s.width, s.height};
        const auto source = s.cabinet();
        for (int ear = 0; ear < 2; ++ear) {
            const auto listener = s.mic(ear);
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
                            const int k = index[axis];
                            image[axis] = float(k) * room[axis] + (k % 2 == 0 ? source[axis] : room[axis] - source[axis]);
                        }
                        const float path = distance(image, listener);
                        const int delay = int(std::lround((path - direct) / speedOfSound * rate));
                        if (delay <= 0 || delay >= earlyLength - 1) continue;
                        // Along the height, odd positive images bounce off the
                        // ceiling, odd negative off the floor, +-2 off both.
                        const int floorHits = iz == -1 || std::abs(iz) == 2 ? 1 : 0, ceilingHits = iz == 1 || std::abs(iz) == 2 ? 1 : 0;
                        taps[size_t(ear)][size_t(count++)] = Tap{delay, referenceDistance / path, order, std::abs(ix) + std::abs(iy), floorHits, ceilingHits};
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
    float currentAbsorption{-1.f}, currentDecay{1.f};
    Space currentSpace;
    float earlyCalibration{}, lateCalibration{};
    float earlyWeight{1.f}, lateWeight{1.f}, diffusionMix{0.f};
    std::array<std::array<std::vector<float>, 4>, 2> diffusers;
    std::array<std::array<size_t, 4>, 2> diffuserWrite{};
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
