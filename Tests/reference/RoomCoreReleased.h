#pragma once
// Room Core as released on claude/room-core (de75919), for parity tests only.

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace spectralforge::room_core_released {

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

#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <array>
#include <atomic>

namespace spectralforge::room_core_released {

// Room Core 1.3.3 (docs/ROOM_CORE_1_3_3.md): one shared room fed by a send
// from each rig lane's audible AMP/CAB output and returned after the post-rig
// gates, before Tone EQ. Parameters append after the extra gates (hint 14);
// projects without them restore with the room off.
inline constexpr int roomParameterVersion = 15;
inline constexpr int roomLaneCount = 3;
// roomOn, roomSize, roomAbsorption, roomReturn and one send per rig lane.
inline constexpr int roomParameterCount = 4 + roomLaneCount;

inline void appendRoomParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    const auto number = [&](const juce::String& id, const juce::String& name, float lo, float hi, float initial, bool decibels) {
        auto attributes = juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [decibels](float value, int) { return decibels ? juce::String(value, 1) + " dB" : juce::String(juce::roundToInt(value)) + " %"; });
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, roomParameterVersion}, name,
                                                               juce::NormalisableRange<float>{lo, hi, .01f}, initial, attributes));
    };
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"roomOn", roomParameterVersion}, "Room", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"roomSize", roomParameterVersion}, "Room size",
                                                            juce::StringArray{"Small", "Medium", "Large"}, 1));
    number("roomAbsorption", "Room absorption", 0.f, 100.f, 50.f, false);
    number("roomReturn", "Room return", -36.f, 6.f, 0.f, true);
    for (int lane = 0; lane < roomLaneCount; ++lane)
        number("roomSend" + juce::String(lane + 1), "Rig " + juce::String(lane + 1) + " room send", 0.f, 100.f, 50.f, false);
}

class RoomCore {
public:
    void bind(juce::AudioProcessorValueTreeState& state) {
        on = state.getRawParameterValue("roomOn");
        size = state.getRawParameterValue("roomSize");
        absorption = state.getRawParameterValue("roomAbsorption");
        returnDb = state.getRawParameterValue("roomReturn");
        for (int lane = 0; lane < roomLaneCount; ++lane)
            send[size_t(lane)] = state.getRawParameterValue("roomSend" + juce::String(lane + 1));
    }
    void prepare(const juce::dsp::ProcessSpec& spec) {
        rate = spec.sampleRate;
        excitation.setSize(int(spec.numChannels), int(spec.maximumBlockSize));
        silence.setSize(int(spec.numChannels), int(spec.maximumBlockSize));
        for (auto& w : wet) w.setSize(int(spec.numChannels), int(spec.maximumBlockSize));
        for (auto& f : feedIn) f.setSize(int(spec.numChannels), int(spec.maximumBlockSize));
        for (auto& model : models) model.prepare(rate);
        prepared = true;
        returnGain.reset(rate, .02);
        absorptionSmooth.reset(rate, .02);
        sizeFade.reset(rate, .05);
        oldOut.reset(rate, .25);
        feedGain.reset(rate, .02);
        reset();
    }
    void reset() noexcept {
        excitation.clear();
        silence.clear();
        for (auto& model : models) model.reset();
        processed = 0;
        quietSamples = 0;
        tail = false;
        fading = false;
        active = 0;
        returnGain.setCurrentAndTargetValue(0.f);
        feedGain.setCurrentAndTargetValue(0.f);
        absorptionSmooth.setCurrentAndTargetValue(readAbsorption());
        sizeFade.setCurrentAndTargetValue(1.f);
        oldOut.setCurrentAndTargetValue(0.f);
        if (prepared) models[0].configure(readSize(), readAbsorption());
        inputPeak.store(0.f, std::memory_order_relaxed);
    }

    // New excitation is wanted only while the room is on; Room off fades the
    // send out over 20 ms and lets an existing tail ring out (running()).
    bool feeding() const noexcept { return on != nullptr && on->load() > .5f; }
    // The engine fills the send bus while the room is on or its send is
    // still fading out.
    bool wantsInput() const noexcept { return feeding() || feedGain.getCurrentValue() > 0.f; }
    bool running() const noexcept { return feeding() || tailActive(); }

    // Per-lane send as a linear factor (0-1).
    std::array<float, roomLaneCount> sends() const noexcept {
        std::array<float, roomLaneCount> out{};
        for (size_t lane = 0; lane < out.size(); ++lane)
            out[lane] = send[lane] != nullptr ? juce::jlimit(0.f, 1.f, send[lane]->load() * .01f) : 0.f;
        return out;
    }
    // The send bus for this block; the engine overwrites it.
    juce::AudioBuffer<float>& bus(int channels, int samples) noexcept {
        excitation.setSize(channels, samples, false, false, true);
        return excitation;
    }
    // Adds the wet return to `output`. `input` is the (gated) send bus, faded
    // here in place, or nullptr once the room is off and only a tail remains.
    void process(juce::AudioBuffer<float>* input, juce::AudioBuffer<float>& output) noexcept {
        if (!running()) return;
        const int channels = output.getNumChannels(), samples = output.getNumSamples();
        processed += uint64_t(samples);
        // Room on/off ramps the excitation, not the return: a hard send edge
        // would replay at every reflection tap.
        feedGain.setTargetValue(feeding() ? 1.f : 0.f);
        float peak = 0.f;
        if (input != nullptr) {
            if (feedGain.isSmoothing())
                for (int n = 0; n < input->getNumSamples(); ++n) {
                    const float gain = feedGain.getNextValue();
                    for (int c = 0; c < input->getNumChannels(); ++c) input->setSample(c, n, input->getSample(c, n) * gain);
                }
            else if (feedGain.getTargetValue() <= 0.f) input->clear();
            for (int c = 0; c < input->getNumChannels(); ++c)
                peak = std::max(peak, input->getMagnitude(c, 0, input->getNumSamples()));
        }
        else feedGain.setCurrentAndTargetValue(0.f);
        inputPeak.store(peak, std::memory_order_relaxed);
        const float returnTarget = juce::Decibels::decibelsToGain(returnDb != nullptr ? returnDb->load() : 0.f);
        // An empty room starts at its return level; the faded send builds it up.
        if (!tail && feeding()) { tail = true; returnGain.setCurrentAndTargetValue(returnTarget); }
        returnGain.setTargetValue(returnTarget);
        // A size change hands the send from the old room to the new one over
        // 50 ms and fades the old room's output over 250 ms. Switching the
        // inputs or crossfading only the outputs would replay a hard edge at
        // every reflection tap.
        const int wanted = readSize();
        if (!fading && wanted != models[size_t(active)].size()) {
            const int next = 1 - active;
            models[size_t(next)].reset();
            models[size_t(next)].configure(wanted, absorptionSmooth.getCurrentValue());
            active = next;
            fading = true;
            sizeFade.setCurrentAndTargetValue(0.f);
            sizeFade.setTargetValue(1.f);
            oldOut.setCurrentAndTargetValue(1.f);
            oldOut.setTargetValue(0.f);
        }
        absorptionSmooth.setTargetValue(readAbsorption());
        absorptionSmooth.skip(samples);
        const float absorbed = absorptionSmooth.getCurrentValue();
        models[size_t(active)].configure(models[size_t(active)].size(), absorbed);
        if (fading) models[size_t(1 - active)].configure(models[size_t(1 - active)].size(), absorbed);
        silence.setSize(channels, samples, false, false, true);
        const auto& source = input != nullptr ? *input : silence;
        for (auto& w : wet) { w.setSize(channels, samples, false, false, true); w.clear(); }
        if (fading) {
            for (auto& f : feedIn) f.setSize(channels, samples, false, false, true);
            for (int n = 0; n < samples; ++n) {
                const float mix = sizeFade.getNextValue();
                for (int c = 0; c < channels; ++c) {
                    const float x = c < source.getNumChannels() ? source.getSample(c, n) : 0.f;
                    feedIn[0].setSample(c, n, x * mix);
                    feedIn[1].setSample(c, n, x * (1.f - mix));
                }
            }
            models[size_t(active)].process(feedIn[0].getArrayOfReadPointers(), wet[0].getArrayOfWritePointers(), channels, samples, 1.f);
            models[size_t(1 - active)].process(feedIn[1].getArrayOfReadPointers(), wet[1].getArrayOfWritePointers(), channels, samples, 1.f);
        }
        else models[size_t(active)].process(source.getArrayOfReadPointers(), wet[0].getArrayOfWritePointers(), channels, samples, 1.f);
        float wetPeak = 0.f;
        for (int n = 0; n < samples; ++n) {
            const float gain = returnGain.getNextValue();
            const float old = fading ? oldOut.getNextValue() : 0.f;
            for (int c = 0; c < channels; ++c) {
                const float value = gain * (wet[0].getSample(c, n) + old * wet[1].getSample(c, n));
                output.addSample(c, n, value);
                wetPeak = std::max(wetPeak, std::abs(value));
            }
        }
        if (fading && !oldOut.isSmoothing()) { fading = false; models[size_t(1 - active)].reset(); }
        // Room off: the tail rings out, then the room goes idle and costs nothing.
        if (feeding() || wetPeak > tailFloor) quietSamples = 0;
        else if ((quietSamples += samples) >= int(rate * .05)) { tail = false; for (auto& model : models) model.reset(); }
    }
    bool tailRinging() const noexcept { return tail && !feeding(); }
    // Host tail while the room is on: from full scale to the -120 dBFS idle
    // floor takes two RT60s (measured at most 1.04 x target; 10 % margin),
    // plus the early-reflection window and the 50 ms idle hold. Read from the
    // parameters only, so any thread may ask; zero with the room off.
    double tailSeconds() const noexcept {
        if (!feeding()) return 0.0;
        return 2.0 * 1.1 * double(RoomModel::geometry(readSize()).rt60AtHalf * RoomModel::rt60Scale(readAbsorption())) + .1 + .05;
    }
    // Samples run through the room since prepare/reset: zero proves Room off
    // cost nothing.
    uint64_t processedSamples() const noexcept { return processed; }
    // Peak of the gated send bus in the last processed block (diagnostics,
    // tests): 0 while nothing excites the room.
    float lastInputPeak() const noexcept { return inputPeak.load(std::memory_order_relaxed); }

private:
    bool tailActive() const noexcept { return tail; }
    int readSize() const noexcept { return size != nullptr ? juce::jlimit(0, 2, juce::roundToInt(size->load())) : 1; }
    float readAbsorption() const noexcept { return absorption != nullptr ? juce::jlimit(0.f, 1.f, absorption->load() * .01f) : .5f; }
    static constexpr float tailFloor = 1.e-6f;   // -120 dBFS

    std::atomic<float>* size{};
    std::atomic<float>* absorption{};
    std::atomic<float>* returnDb{};
    std::array<RoomModel, 2> models;
    std::array<juce::AudioBuffer<float>, 2> wet;
    juce::AudioBuffer<float> silence;
    std::array<juce::AudioBuffer<float>, 2> feedIn;
    juce::SmoothedValue<float> returnGain, absorptionSmooth, sizeFade, oldOut, feedGain;
    double rate{48000.0};
    int active{}, quietSamples{};
    bool tail{}, fading{}, prepared{};

    std::atomic<float>* on{};
    std::array<std::atomic<float>*, roomLaneCount> send{};
    juce::AudioBuffer<float> excitation;
    uint64_t processed{};
    std::atomic<float> inputPeak{0.f};
};

} // namespace spectralforge
