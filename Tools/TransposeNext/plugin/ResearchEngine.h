#pragma once
// Research only. The two engines the T6 DAW check compares, configured exactly
// as TransposeProbe does: the released PolyPitch ("baseline") and the
// corr40-split-v2 candidate.
#include "PolyPitch.h"
#include "TransientSplitPitch.h"

namespace transposeResearchPlugin {
enum class Engine {baseline=0, v2=1};

struct ResearchEngine {
    spectralforge::PolyPitch baseline;
    transposeNext::TransientSplitPitch v2;
    void prepare(double rate,int maxBlock,int channels) {
        const juce::dsp::ProcessSpec spec{rate,juce::uint32(std::max(1,maxBlock)),juce::uint32(channels)};
        baseline.prepare(spec);
        v2.prepare(spec,true,true,true,true,true);
    }
    void reset() {baseline.reset();v2.reset();}
    void setSemitones(int n) {baseline.setSemitones(n);v2.setSemitones(n);}
    // Baseline advertises its FFT window as PDC. The candidate has a variable
    // 2-40 ms read delay and no fixed PDC contract yet (T6 open item): 0.
    int latency(Engine e) const {return e==Engine::baseline?baseline.latency():0;}
    void process(Engine e,const juce::AudioBuffer<float>& in,juce::AudioBuffer<float>& out) {
        if(e==Engine::baseline)baseline.process(in,out);else v2.process(in,out);
    }
};
}
