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
