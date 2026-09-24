#pragma once
#include "Cabinet.h"
#include "IRMetadata.h"
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <memory>
#include <mutex>

namespace spectralforge {
class IRLibrary : private juce::Thread {
public:
    struct Asset {
        juce::AudioBuffer<float> samples;
        juce::MemoryBlock encoded;
        juce::String name;
        IRMetadata metadata;
        double rate{};
    };
    explicit IRLibrary(std::array<Cab*,3> cabinets);
    ~IRLibrary() override;
    void prepare(const juce::dsp::ProcessSpec&, const std::array<int,3>& sources);
    void stop();
    juce::Result importFile(int lane, const juce::File&);
    juce::String status(int lane) const;
    IRMetadata metadata(int lane, int source) const;
    void setMetadata(int lane, const IRMetadata&);
    juce::ValueTree save() const;
    void restore(const juce::ValueTree&);
    static std::shared_ptr<Asset> decode(const juce::MemoryBlock&, const juce::String&, juce::String& error);
private:
    void run() override;
    std::unique_ptr<Cab::Kernel> build(int lane, int source, unsigned generation);
    std::array<Cab*,3> cabs;
    std::array<std::shared_ptr<Asset>,3> users;
    std::array<std::shared_ptr<Asset>,2> factory;
    std::array<unsigned,3> generations{1,1,1};
    std::array<juce::String,3> errors;
    mutable std::mutex mutex;
    juce::dsp::ProcessSpec spec{48000,512,2};
};
}
