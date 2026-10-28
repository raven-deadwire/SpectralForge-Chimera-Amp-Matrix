#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "RoomModel.h"
#include <algorithm>
#include <array>
#include <atomic>

namespace spectralforge {

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

// Room Advanced 1.8, first part (docs/ROOM_ADVANCED_1_8.md): early/late
// balance, decay, diffusion, wet low/high cut and width. Appended after every
// earlier bank at version hint 24; every default is the Room Core room, exactly.
inline constexpr int roomAdvancedParameterVersion = 24;
inline constexpr int roomAdvancedParameterCount = 6;
inline void appendRoomAdvancedParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    const auto number = [&](const juce::String& id, const juce::String& name, float lo, float hi, float initial, float centre, const juce::String& unit) {
        juce::NormalisableRange<float> range{lo, hi, .01f};
        if (centre > lo && centre < hi) range.setSkewForCentre(centre);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, roomAdvancedParameterVersion}, name, range, initial,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([unit](float v, int) { return juce::String(juce::roundToInt(v)) + unit; })));
    };
    number("roomEarlyLate", "Room early / late balance", -100.f, 100.f, 0.f, 0.f, " %");
    number("roomDecay", "Room decay", 25.f, 400.f, 100.f, 100.f, " %");
    number("roomDiffusion", "Room diffusion", 0.f, 100.f, 0.f, 0.f, " %");
    number("roomLowCut", "Room low cut", 20.f, 1000.f, 20.f, 150.f, " Hz");
    number("roomHighCut", "Room high cut", 1000.f, 20000.f, 20000.f, 5000.f, " Hz");
    number("roomWidth", "Room width", 0.f, 200.f, 100.f, 0.f, " %");
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
        earlyLate = state.getRawParameterValue("roomEarlyLate");
        decay = state.getRawParameterValue("roomDecay");
        diffusion = state.getRawParameterValue("roomDiffusion");
        lowCut = state.getRawParameterValue("roomLowCut");
        highCut = state.getRawParameterValue("roomHighCut");
        width = state.getRawParameterValue("roomWidth");
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
        for (auto* v : {&lowMix, &highMix, &widthGain}) v->reset(rate, .02);
        for (auto* v : {&lowHz, &highHz}) v->reset(rate, .02);
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
        const auto a = readAdvanced();
        lowMix.setCurrentAndTargetValue(a.lowOn ? 1.f : 0.f);highMix.setCurrentAndTargetValue(a.highOn ? 1.f : 0.f);
        lowHz.setCurrentAndTargetValue(a.lowHz);highHz.setCurrentAndTargetValue(a.highHz);widthGain.setCurrentAndTargetValue(a.width);
        lowFilter = {};highFilter = {};
        if (prepared) { models[0].configure(readSize(), readAbsorption(), a.decay); models[0].snapShape(a.shape); }
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
            const auto shaping = readAdvanced();
            models[size_t(next)].reset();
            models[size_t(next)].configure(wanted, absorptionSmooth.getCurrentValue(), shaping.decay);
            models[size_t(next)].snapShape(shaping.shape);
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
        const auto advanced = readAdvanced();
        models[size_t(active)].configure(models[size_t(active)].size(), absorbed, advanced.decay);
        if (fading) models[size_t(1 - active)].configure(models[size_t(1 - active)].size(), absorbed, advanced.decay);
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
            models[size_t(active)].process(feedIn[0].getArrayOfReadPointers(), wet[0].getArrayOfWritePointers(), channels, samples, 1.f, advanced.shape);
            models[size_t(1 - active)].process(feedIn[1].getArrayOfReadPointers(), wet[1].getArrayOfWritePointers(), channels, samples, 1.f, advanced.shape);
        }
        else models[size_t(active)].process(source.getArrayOfReadPointers(), wet[0].getArrayOfWritePointers(), channels, samples, 1.f, advanced.shape);
        // Wet low/high cut (12 dB/oct) and width (mid/side). A cut at its end
        // stop, or 100 % width, is switched out through a 20 ms mix, so the
        // Room Core return stays sample-exact at the defaults.
        lowMix.setTargetValue(advanced.lowOn ? 1.f : 0.f);highMix.setTargetValue(advanced.highOn ? 1.f : 0.f);
        // A cut switched in from off starts at its own frequency (no sweep up
        // from the end stop) on a cleared filter, faded in by its mix.
        if (lowMix.getCurrentValue() <= 0.f && !lowMix.isSmoothing()) { lowHz.setCurrentAndTargetValue(advanced.lowHz); lowFilter = {}; }
        if (highMix.getCurrentValue() <= 0.f && !highMix.isSmoothing()) { highHz.setCurrentAndTargetValue(advanced.highHz); highFilter = {}; }
        lowHz.setTargetValue(advanced.lowHz);highHz.setTargetValue(advanced.highHz);widthGain.setTargetValue(advanced.width);
        const bool shaping = lowMix.isSmoothing() || highMix.isSmoothing() || widthGain.isSmoothing() || lowMix.getCurrentValue() > 0.f
            || highMix.getCurrentValue() > 0.f || widthGain.getCurrentValue() != 1.f;
        float wetPeak = 0.f;
        for (int n = 0; n < samples; ++n) {
            const float gain = returnGain.getNextValue();
            const float old = fading ? oldOut.getNextValue() : 0.f;
            if (!shaping) {
                for (int c = 0; c < channels; ++c) {
                    const float value = gain * (wet[0].getSample(c, n) + old * wet[1].getSample(c, n));
                    output.addSample(c, n, value);
                    wetPeak = std::max(wetPeak, std::abs(value));
                }
                continue;
            }
            // Smoothstep on the linear mix ramps: the fade has no slope corner
            // at either end.
            const auto ease = [](float m) { return m * m * (3.f - 2.f * m); };
            const float lm = ease(lowMix.getNextValue()), hm = ease(highMix.getNextValue()), wg = widthGain.getNextValue();
            const double gl = std::tan(juce::MathConstants<double>::pi * double(lowHz.getNextValue()) / rate);
            const double gh = std::tan(juce::MathConstants<double>::pi * std::min(double(highHz.getNextValue()), rate * .45) / rate);
            std::array<float, 2> v{};
            for (int c = 0; c < channels && c < 2; ++c) {
                float x = wet[0].getSample(c, n) + old * wet[1].getSample(c, n);
                // A cut switched fully out drops its state, so switching it in starts clean.
                if (lm > 0.f) x += lm * (float(lowFilter[size_t(c)].tick(x, gl)[2]) - x); else lowFilter[size_t(c)] = {};
                if (hm > 0.f) x += hm * (float(highFilter[size_t(c)].tick(x, gh)[0]) - x); else highFilter[size_t(c)] = {};
                v[size_t(c)] = x;
            }
            if (channels > 1) { const float mid = .5f * (v[0] + v[1]), side = .5f * (v[0] - v[1]) * wg; v[0] = mid + side; v[1] = mid - side; }
            for (int c = 0; c < channels && c < 2; ++c) {
                const float value = gain * v[size_t(c)];
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
        return 2.0 * 1.1 * double(RoomModel::geometry(readSize()).rt60AtHalf * RoomModel::rt60Scale(readAbsorption())) * double(readAdvanced().decay) + .1 + .05;
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
    struct Advanced { RoomModel::Shape shape; float decay{1.f}; bool lowOn{}, highOn{}; float lowHz{20.f}, highHz{20000.f}, width{1.f}; };
    Advanced readAdvanced() const noexcept {
        const auto read = [](const std::atomic<float>* p, float lo, float hi, float fallback) {
            const float v = p != nullptr ? p->load() : fallback;
            return std::isfinite(v) ? juce::jlimit(lo, hi, v) : fallback;
        };
        Advanced a;
        // -100 % keeps only the early reflections, +100 % only the late tail.
        const float balance = read(earlyLate, -100.f, 100.f, 0.f) * .01f;
        a.shape.early = juce::jlimit(0.f, 1.f, 1.f - balance);a.shape.late = juce::jlimit(0.f, 1.f, 1.f + balance);
        a.shape.diffusion = read(diffusion, 0.f, 100.f, 0.f) * .01f;
        // The skewed range returns 100 % as 99.99999; within half a step of
        // 100 % the decay is exactly 1x, so the default room stays exact.
        const float decayPercent = read(decay, 25.f, 400.f, 100.f);
        a.decay = std::abs(decayPercent - 100.f) < .005f ? 1.f : decayPercent * .01f;
        a.lowHz = read(lowCut, 20.f, 1000.f, 20.f);a.highHz = read(highCut, 1000.f, 20000.f, 20000.f);
        a.lowOn = a.lowHz > 20.5f;a.highOn = a.highHz < 19950.f;
        const float widthPercent = read(width, 0.f, 200.f, 100.f);
        a.width = std::abs(widthPercent - 100.f) < .005f ? 1.f : widthPercent * .01f;
        return a;
    }
    // Zero-delay-feedback state-variable filter (Q 0.707); returns {low, band, high}.
    struct SVF {
        double ic1{}, ic2{};
        std::array<double, 3> tick(double x, double g) noexcept {
            const double k = 1.41421356, a1 = 1. / (1. + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const double v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2. * v1 - ic1;ic2 = 2. * v2 - ic2;
            return {v2, v1, x - k * v1 - v2};
        }
    };
    std::atomic<float>* earlyLate{};
    std::atomic<float>* decay{};
    std::atomic<float>* diffusion{};
    std::atomic<float>* lowCut{};
    std::atomic<float>* highCut{};
    std::atomic<float>* width{};
    juce::SmoothedValue<float> lowMix, highMix, widthGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> lowHz{20.f}, highHz{20000.f};
    std::array<SVF, 2> lowFilter{}, highFilter{};

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
