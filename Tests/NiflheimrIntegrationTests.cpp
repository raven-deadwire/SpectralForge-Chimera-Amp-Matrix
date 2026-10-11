#include "Amplifier.h"
#include "AmpNativeParameters.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

namespace { std::atomic<bool> allocationWatch{false}; std::atomic<std::size_t> allocations{0}; }
void* operator new(std::size_t n) {
    if (allocationWatch.load(std::memory_order_relaxed)) ++allocations;
    if (auto* p = std::malloc(std::max(n, std::size_t{1}))) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
using namespace spectralforge;
constexpr double pi = 3.14159265358979323846;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
float source(int n, double rate, int stereoChannel) {
    const auto t = n / rate;
    const auto fundamental = stereoChannel == 0 ? 30.867706 : 27.5;
    return float(.08 * std::exp(-8 * std::fmod(t, .073))
                 * (std::sin(2 * pi * fundamental * t) + .4 * std::sin(2 * pi * 191 * t)
                    + .23 * std::sin(2 * pi * 1703 * t)));
}
AmpNativeState settings(int channel) {
    auto state = defaultAmpNativeState(niflheimrAmpModel);
    state.channel = channel;
    const auto values = niflheimr::channelState(channel);
    for (std::size_t i = 0; i < niflheimr::controlCount; ++i) state.values[i] = values.values[i];
    return state;
}
struct Render { std::array<std::vector<float>, 2> audio; int latency = 0; };
Render render(const AmpNativeState& state, double rate, int osIndex, int blockSize,
              int preparedBlock = 257, bool silentRight = false, bool insertEmpty = false,
              bool conflictingLegacy = false) {
    constexpr int count = 4096;
    Amp amp;
    amp.prepare({rate, juce::uint32(preparedBlock), 2});
    amp.setNative(state);
    amp.setOversampling(osIndex);
    amp.reset();
    Render output;
    output.latency = amp.latency();
    require(output.latency >= 0 && output.latency < 64, "Production latency outside prepared compensation capacity");
    for (auto& channel : output.audio) channel.resize(count);
    juce::AudioBuffer<float> block(2, blockSize), empty(2, 0);
    for (int offset = 0; offset < count; offset += blockSize) {
        const auto size = std::min(blockSize, count - offset);
        block.setSize(2, size, false, false, true);
        for (int n = 0; n < size; ++n) {
            block.setSample(0, n, source(offset + n, rate, 0));
            block.setSample(1, n, silentRight ? 0.f : source(offset + n, rate, 1));
        }
        allocationWatch = true;
        amp.setNative(state);
        if (conflictingLegacy) {
            amp.set(static_cast<AmpModel>(niflheimrAmpModel), 0.f);
            amp.tone(12, -12, 12, -12, 12, -12);
        }
        if (insertEmpty) amp.process(empty);
        amp.process(block);
        allocationWatch = false;
        for (int channel = 0; channel < 2; ++channel)
            for (int n = 0; n < size; ++n) {
                const auto sample = block.getSample(channel, n);
                require(std::isfinite(sample), "Production native route produced nonfinite output");
                require(std::abs(sample) < 64, "Production native route produced an unsafe peak");
                output.audio[std::size_t(channel)][std::size_t(offset + n)] = sample;
            }
    }
    return output;
}
double difference(const Render& a, const Render& b) {
    double error = 0;
    require(a.latency == b.latency, "Block size changed reported latency");
    for (std::size_t c = 0; c < a.audio.size(); ++c)
        for (std::size_t n = 0; n < a.audio[c].size(); ++n)
            error = std::max(error, std::abs(double(a.audio[c][n]) - b.audio[c][n]));
    return error;
}
void productionDispatch() {
    double maxError = 0;
    for (int channel = 0; channel < niflheimr::channelCount; ++channel) {
        const auto state = settings(channel);
        const auto audio = render(state, 48000, 0, 127);
        niflheimr::NiflheimrDSP core;
        core.prepare(48000);
        core.set(niflheimr::channelState(channel));
        core.reset();
        for (int n = 0; n < int(audio.audio[0].size()) - audio.latency; ++n)
            for (int c = 0; c < 2; ++c) {
                const auto expected = core.tick(source(n, 48000, c), c);
                maxError = std::max(maxError, std::abs(double(expected) - audio.audio[std::size_t(c)][std::size_t(n + audio.latency)]));
            }
        for (int n = 0; n < audio.latency; ++n)
            require(std::abs(audio.audio[0][std::size_t(n)]) < 1.e-8f, "1x production route has unreported pre-latency output");
        const auto legacy = render(state, 48000, 0, 127, 257, false, false, true);
        require(difference(audio, legacy) < 1.e-7, "Legacy drive=0 or legacy Tone incorrectly altered Niflheimr native state");
    }
    require(maxError < 2.e-6, "Production model dispatch differs from Niflheimr core at 1x");
    std::cout << "PASS five production channel dispatches; 1x core residual=" << maxError << '\n';
}
void productionBlocks() {
    double residual = 0, oversizedResidual = 0;
    int routes = 0;
    for (double rate : {44100., 48000., 96000., 192000., 384000.})
        for (int os = 0; os < 4; ++os)
            for (int channel = 0; channel < niflheimr::channelCount; ++channel) {
                const auto state = settings(channel);
                const auto a = render(state, rate, os, 64);
                const auto b = render(state, rate, os, 257);
                residual = std::max(residual, difference(a, b));
                double energy = 0;
                for (auto sample : a.audio[0]) energy += double(sample) * sample;
                require(energy > 1.e-8, "Production native route is silent");
                ++routes;
            }
    for (int os = 0; os < 4; ++os) {
        const auto state = settings(os);
        const auto reference = render(state, 48000, os, 128, 128, true);
        const auto oversized = render(state, 48000, os, 733, 128, true, true);
        oversizedResidual = std::max(oversizedResidual, difference(reference, oversized));
        for (auto sample : oversized.audio[1]) require(std::abs(sample) < 1.e-8f, "Native left signal leaked into silent right channel");
    }
    require(residual < 2.e-6 && oversizedResidual < 2.e-6, "Native output depends on host block partition or zero-size callbacks");
    require(allocations.load() == 0, "Production set/process allocated on the audio path");
    std::cout << "PASS " << routes << " channel/rate/OS routes; partition residual=" << residual
              << " oversized/empty residual=" << oversizedResidual << " allocations=" << allocations << '\n';
}
// The full processor tests independently cover released host index ordering.
// This host exercises the production native cache and all stored channel banks.
struct Host : juce::AudioProcessor {
    using juce::AudioProcessor::processBlock;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
        juce::AudioProcessorValueTreeState::ParameterLayout result;
        addAmpNativeParameters(result);
        appendNewAmpNativeParameters(result);
        appendNewAmpNativeParameters(result, firstOriginalAmpModel, firstOriginalAmpModel + 1);
        appendOriginalChannelParameters(result);
        appendNiflheimrParameters(result);
        return result;
    }
    juce::AudioProcessorValueTreeState state{*this, nullptr, "NIFLHEIMR_TEST", layout()};
    const juce::String getName() const override { return "Niflheimr native cache test"; }
    void prepareToPlay(double, int) override {} void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return false; } bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; } bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; } int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {} const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& bytes) override {
        juce::MemoryOutputStream stream(bytes, false); state.copyState().writeToStream(stream);
    }
    void setStateInformation(const void* bytes, int size) override {
        const auto tree = juce::ValueTree::readFromData(bytes, std::size_t(size));
        if (tree.hasType("NIFLHEIMR_TEST")) state.replaceState(tree);
    }
};
void setRaw(Host& host, const juce::String& id, float value) {
    auto* parameter = host.state.getParameter(id);
    require(parameter != nullptr, "Missing Niflheimr parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void cacheAndState() {
    Host original, restored;
    using ChannelValues = std::array<float, niflheimr::controlCount>;
    std::array<std::array<ChannelValues, niflheimr::channelCount>, ampNativeContextCount> expected{};
    for (int context = 0; context < ampNativeContextCount; ++context) {
        setRaw(original, ampNativeModelID(context), float(niflheimrAmpModel));
        setRaw(original, ampNativeChannelID(context, niflheimrAmpModel), float(context % niflheimr::channelCount));
        for (int channel = 0; channel < niflheimr::channelCount; ++channel)
            for (std::size_t control = 0; control < niflheimr::controlCount; ++control) {
                const auto id = ampNativeControlID(context, niflheimrAmpModel, int(control), channel);
                auto* parameter = original.state.getParameter(id);
                require(parameter != nullptr, "Missing independent Niflheimr channel control");
                const float normal = .05f + .9f * float((context * 71 + channel * 19 + int(control) * 7) % 97) / 96.f;
                parameter->setValueNotifyingHost(normal);
                expected[std::size_t(context)][std::size_t(channel)][control] = original.state.getRawParameterValue(id)->load();
            }
    }
    // Cache pointers bind before restore, matching the lifetime of the real processor.
    AmpNativeParameterCache cache;
    cache.bind(restored.state);
    juce::MemoryBlock bytes;
    original.getStateInformation(bytes);
    restored.setStateInformation(bytes.getData(), int(bytes.getSize()));
    for (int context = 0; context < ampNativeContextCount; ++context) {
        require(cache.read(context).channel == context % niflheimr::channelCount, "Selected Niflheimr channel was not restored");
        for (int channel = 0; channel < niflheimr::channelCount; ++channel) {
            setRaw(restored, ampNativeChannelID(context, niflheimrAmpModel), float(channel));
            const auto state = cache.read(context);
            require(state.model == niflheimrAmpModel && state.channel == channel, "Cache selected the wrong model or channel");
            for (std::size_t control = 0; control < niflheimr::controlCount; ++control)
                require(std::abs(state.values[control] - expected[std::size_t(context)][std::size_t(channel)][control]) < .001f,
                        "Cache failed to read restored per-channel controls");
        }
    }
    // Switching every bank must not write new defaults over any inactive bank.
    for (int context = 0; context < ampNativeContextCount; ++context)
        for (int channel = 0; channel < niflheimr::channelCount; ++channel)
            for (std::size_t control = 0; control < niflheimr::controlCount; ++control) {
                const auto id = ampNativeControlID(context, niflheimrAmpModel, int(control), channel);
                require(std::abs(restored.state.getRawParameterValue(id)->load() - expected[std::size_t(context)][std::size_t(channel)][control]) < .001f,
                        "Selecting a channel changed an inactive bank");
            }
    std::cout << "PASS 6 contexts x 5 channels x 14 controls; binary restore and live native cache\n";
}
} // namespace

int main() {
    try {
        productionDispatch();
        productionBlocks();
        cacheAndState();
        std::cout << "PASS Niflheimr production integration; musical/reference acceptance remains pending\n";
        return 0;
    } catch (const std::exception& error) {
        allocationWatch = false;
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
