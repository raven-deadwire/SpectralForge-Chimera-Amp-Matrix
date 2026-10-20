#pragma once
#include "CallbackRecorder.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdint>
#include <vector>

namespace chimera_perf {
// Non-owning, single callback thread. Construct after prepareToPlay; do not
// mutate processor/state/input or export evidence until the callback has stopped.
template<class Processor>
class FullGraphAdapter {
    std::vector<Processor*> processors;
    std::vector<juce::AudioBuffer<float>> tracks;
    juce::AudioBuffer<float> mix;
    juce::MidiBuffer midi;
    const juce::AudioBuffer<float>& input;
    int position = 0, frames;
public:
    std::vector<double> energy;
    std::vector<std::uint64_t> calls;
    bool finite = true;
    FullGraphAdapter(std::vector<Processor*> nodes,
                     const juce::AudioBuffer<float>& stimulus, int block)
        : processors(std::move(nodes)), mix(2, block), input(stimulus), frames(block),
          energy(processors.size()), calls(processors.size()) {
        tracks.reserve(processors.size());
        for (std::size_t i = 0; i < processors.size(); ++i) tracks.emplace_back(2, block);
        midi.ensureSize(2048);
    }
    // Includes fan-out, every entire production processor and fan-in. Analysis
    // (energy/finite checks) is deliberately outside the recorded scope.
    void process(bool emptyGraph = false) {
        mix.clear();
        for (std::size_t i = 0; i < processors.size(); ++i) {
            auto& track = tracks[i];
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < frames; ++n)
                    track.setSample(c, n, input.getSample(c, (position+n) % input.getNumSamples()));
            midi.clear();
            if (!emptyGraph) processors[i]->processBlock(track, midi);
            for (int c = 0; c < 2; ++c) mix.addFrom(c, 0, track, c, 0, frames, 1.f / float(processors.size()));
        }
        position = (position + frames) % input.getNumSamples();
    }
    void observe() noexcept {
        for (std::size_t i = 0; i < tracks.size(); ++i) {
            ++calls[i];
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < frames; ++n) {
                    const double x = tracks[i].getSample(c, n);
                    finite = finite && std::isfinite(x);
                    energy[i] += x*x;
                }
        }
    }
};
}
