#pragma once
// Research only. The engines the T6 DAW check compares, configured exactly as
// TransposeProbe does: the released PolyPitch ("baseline"), the
// corr40-split-v2 candidate and corr40-split-v3 (block-invariant splice hold).
#include "PolyPitch.h"
#include "TransientSplitPitch.h"

namespace transposeResearchPlugin {
enum class Engine {baseline=0, v2=1, v3=2};

struct ResearchEngine {
    spectralforge::PolyPitch baseline;
    transposeNext::TransientSplitPitch v2,v3;
    void prepare(double rate,int maxBlock,int channels) {
        const juce::dsp::ProcessSpec spec{rate,juce::uint32(std::max(1,maxBlock)),juce::uint32(channels)};
        baseline.prepare(spec);
        v2.prepare(spec,true,true,true,true,true);
        v3.prepare(spec,true,true,true,true,true,true);
    }
    void reset() {baseline.reset();v2.reset();v3.reset();}
    void setSemitones(int n) {baseline.setSemitones(n);v2.setSemitones(n);v3.setSemitones(n);}
    // Baseline advertises its FFT window as PDC. The candidates' read delay
    // varies (about 2-40 ms). Live mode reports 0 (monitoring); fixed-PDC mode
    // reports candidatePdcMs, chosen in docs/TRANSPOSE_T6_PDC_CONTRACT.md so the
    // attack residual after host compensation is smallest. The low band is not
    // compensated by it (reported separately).
    static constexpr double candidatePdcMs=2.5;  // minimax attack residual, all rates (PDC study)
    int latency(Engine e,bool fixedPdc,double rate) const {
        if(e==Engine::baseline)return baseline.latency();
        return fixedPdc?int(std::lround(candidatePdcMs*rate/1000.)):0;
    }
    void process(Engine e,const juce::AudioBuffer<float>& in,juce::AudioBuffer<float>& out) {
        if(e==Engine::baseline)baseline.process(in,out);else if(e==Engine::v2)v2.process(in,out);else v3.process(in,out);
    }
};
}
