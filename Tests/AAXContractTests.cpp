#include "PluginProcessor.h"
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void set(ChimeraProcessor& p, const char* id, float value) {
    auto* parameter = p.parameters().getParameter(id);
    require(parameter != nullptr, "Missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float rawInput(int channel, int sample) {
    return sample < 0 ? 0.f : static_cast<float>((sample * 37 + channel * 19) % 101 - 50) / 1024.f;
}
void bypassSamples(ChimeraProcessor& p, int channels) {
    int elapsed = 0;
    juce::MidiBuffer midi;
    // Enter bypass before any wet audio, then cross wet/bypass repeatedly. Vary
    // the callback length past prepareToPlay's 128-sample hint without reprepare.
    for (int block = 0; block < 72; ++block) {
        const int count = std::array<int, 3>{64, 128, 257}[block % 3];
        const bool bypass = block < 36 || block >= 48;
        juce::AudioBuffer<float> audio(channels, count);
        for (int c = 0; c < channels; ++c) for (int n = 0; n < count; ++n)
            audio.setSample(c, n, rawInput(c, elapsed + n));
        if (bypass) {
            const int delay = p.getLatencySamples();
            p.processBlockBypassed(audio, midi);
            require(p.getLatencySamples() == delay, "Host bypass changed reported latency");
            for (int c = 0; c < channels; ++c) for (int n = 0; n < count; ++n)
                require(audio.getSample(c, n) == rawInput(c, elapsed + n - delay),
                        "Master bypass audio does not match host-reported PDC delay");
        } else {
            p.processBlock(audio, midi);
        }
        elapsed += count;
    }
}
void latencyAndRecall() {
    for (int channels : {1, 2}) for (int rate : {44100, 48000, 96000}) {
        auto p = std::make_unique<ChimeraProcessor>();
        auto layout = p->getBusesLayout();
        layout.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        layout.outputBuses = layout.inputBuses;
        require(p->setBusesLayout(layout), "Latency bus rejected");
        set(*p, "mode", rate == 44100 ? 0.f : rate == 48000 ? 1.f : 2.f);
        set(*p, "boardEnabled", channels == 1 ? 0.f : 1.f);
        set(*p, "transpose", -2); set(*p, "transposeon", 0);
        p->setRateAndBufferSizeDetails(rate, 128); p->prepareToPlay(rate, 128);
        const int base = p->getLatencySamples();
        const int pitch = p->pitchLatency();
        juce::AudioBuffer<float> audio(channels, 128); juce::MidiBuffer midi;
        auto process = [&] { audio.clear(); p->processBlock(audio, midi); };
        set(*p, "transposeon", 1); process();
        require(p->getLatencySamples() == base + pitch, "Active pitch delay differs from common DSP");
        set(*p, "transpose", 0); process();
        require(p->getLatencySamples() == base, "Zero semitones retained pitch delay");
        set(*p, "transpose", -2); set(*p, "transposeon", 0); process();
        require(p->getLatencySamples() == base, "Disabled pitch retained delay");
        set(*p, "transposeon", 1); process();
        juce::MemoryBlock state; p->getStateInformation(state);
        auto restored = std::make_unique<ChimeraProcessor>();
        require(restored->setBusesLayout(layout), "Restored bus rejected");
        restored->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        restored->setRateAndBufferSizeDetails(rate, 128); restored->prepareToPlay(rate, 128);
        require(restored->getLatencySamples() == base + pitch, "Restored pitch delay differs before first callback");
        bypassSamples(*restored, channels);
        // A second prepare must clear history and support a restored zero-shift
        // state as well as a shifted one, with no extra bypass parameter.
        set(*restored, "transpose", 0);
        restored->releaseResources(); restored->prepareToPlay(rate, 128);
        require(restored->getLatencySamples() == base, "Reprepare kept stale pitch delay");
        bypassSamples(*restored, channels);
        restored->releaseResources(); p->releaseResources();
        std::cout << "PASS AAX common processor rate=" << rate << " channels=" << channels
                  << " dry_latency=" << base << " pitch_latency=" << pitch
                  << " exact master-bypass samples and state/reprepare; Pro Tools PDC NOT_TESTED\n";
    }
}
void check() {
    auto p = std::make_unique<ChimeraProcessor>();
    // JUCE 8.0.8's AAX wrapper keys its map with a 31-based 32-bit hash.
    // Test the real complete parameter bank, including its wrapper-only bypass.
    std::map<std::uint32_t, juce::String> hashes;
    auto add = [&](const juce::String& id) {
        std::uint32_t hash = 0;
        for (auto c : id.toStdString()) hash = 31u * hash + static_cast<unsigned char>(c);
        require(hashes.emplace(hash, id).second, "AAX parameter hash collision");
    };
    add("MasterBypassID");
    for (auto* parameter : p->getParameters()) {
        auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        require(identified != nullptr, "Unstable AAX parameter ID");
        add(identified->paramID);
    }
    for (int channels : {1, 2}) {
        auto layout = p->getBusesLayout();
        layout.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        layout.outputBuses = layout.inputBuses;
        require(p->setBusesLayout(layout), "Required AAX bus rejected");
        for (int mode = 0; mode < 3; ++mode) {
            set(*p, "mode", static_cast<float>(mode));
            const auto id = p->getAAXClientExtensions().getPluginIDForMainBusConfig(
                layout.getMainInputChannelSet(), layout.getMainOutputChannelSet(), false);
            require(id == (channels == 1 ? 0x43684d31 : 0x43685331), "Persistent AAX ID changed");
        }
        layout.outputBuses.set(0, channels == 1 ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::mono());
        require(!p->isBusesLayoutSupported(layout), "Unqualified asymmetric bus accepted");
    }
    set(*p, "boardEnabled", 0); set(*p, "transpose", -2); set(*p, "transposeon", 0);
    p->setRateAndBufferSizeDetails(48000, 128); p->prepareToPlay(48000, 128);
    const auto dryLatency = p->getLatencySamples();
    juce::AudioBuffer<float> buffer(2, 128); juce::MidiBuffer midi; buffer.clear();
    set(*p, "transposeon", 1); p->processBlock(buffer, midi);
    require(p->getLatencySamples() > dryLatency, "Transpose latency not reported");
    set(*p, "transposeon", 0); buffer.clear(); p->processBlock(buffer, midi);
    require(p->getLatencySamples() == dryLatency, "Bypass latency not restored");
    p->releaseResources();
    std::cout << "PASS AAX stable layout IDs, " << hashes.size()
              << " collision-free host IDs, shared dynamic latency; no Pro Tools host acceptance claimed\n";
}
}
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    try { check(); latencyAndRecall(); return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
