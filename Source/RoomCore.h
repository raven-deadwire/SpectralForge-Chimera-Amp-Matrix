#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
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
    const auto number = [&](const juce::String& id, const juce::String& name, float lo, float hi, float initial) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, roomParameterVersion}, name,
                                                               juce::NormalisableRange<float>{lo, hi, .01f}, initial));
    };
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"roomOn", roomParameterVersion}, "Room", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"roomSize", roomParameterVersion}, "Room size",
                                                            juce::StringArray{"Small", "Medium", "Large"}, 1));
    number("roomAbsorption", "Room absorption", 0.f, 100.f, 50.f);
    number("roomReturn", "Room return", -36.f, 6.f, 0.f);
    for (int lane = 0; lane < roomLaneCount; ++lane)
        number("roomSend" + juce::String(lane + 1), "Rig " + juce::String(lane + 1) + " room send", 0.f, 100.f, 50.f);
}

class RoomCore {
public:
    void bind(juce::AudioProcessorValueTreeState& state) {
        on = state.getRawParameterValue("roomOn");
        for (int lane = 0; lane < roomLaneCount; ++lane)
            send[size_t(lane)] = state.getRawParameterValue("roomSend" + juce::String(lane + 1));
    }
    void prepare(const juce::dsp::ProcessSpec& spec) {
        excitation.setSize(int(spec.numChannels), int(spec.maximumBlockSize));
        reset();
    }
    void reset() noexcept { excitation.clear(); processed = 0; inputPeak.store(0.f, std::memory_order_relaxed); }

    // New excitation is wanted only while the room is on; Room off stops the
    // send at once and lets an existing tail ring out (running()).
    bool feeding() const noexcept { return on != nullptr && on->load() > .5f; }
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
    // Adds the wet return to `output`. `input` is the (gated) send bus, or
    // nullptr while the room is off and only a tail remains.
    void process(const juce::AudioBuffer<float>* input, juce::AudioBuffer<float>& output) noexcept {
        if (!running()) return;
        processed += uint64_t(output.getNumSamples());
        float peak = 0.f;
        if (input != nullptr)
            for (int c = 0; c < input->getNumChannels(); ++c)
                peak = std::max(peak, input->getMagnitude(c, 0, input->getNumSamples()));
        inputPeak.store(peak, std::memory_order_relaxed);
        // Early reflections and late decay arrive with the week-2 room model;
        // until then the return is silent.
    }
    // Samples run through the room since prepare/reset: zero proves Room off
    // cost nothing.
    uint64_t processedSamples() const noexcept { return processed; }
    // Peak of the gated send bus in the last processed block (diagnostics,
    // tests): 0 while nothing excites the room.
    float lastInputPeak() const noexcept { return inputPeak.load(std::memory_order_relaxed); }

private:
    bool tailActive() const noexcept { return false; }

    std::atomic<float>* on{};
    std::array<std::atomic<float>*, roomLaneCount> send{};
    juce::AudioBuffer<float> excitation;
    uint64_t processed{};
    std::atomic<float> inputPeak{0.f};
};

} // namespace spectralforge
