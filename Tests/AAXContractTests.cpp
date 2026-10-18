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
    try { check(); return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
