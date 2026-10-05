#pragma once
#include "OriginalAmpDSP.h"
#include <juce_dsp/juce_dsp.h>
#include <stdexcept>

namespace spectralforge::original {
// Development integration adapter. Allocations and factor changes occur only
// in prepare() while audio is stopped, never in process()/set().
class OriginalAmpProcessor {
    OriginalAmpDSP core;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::size_t capacity=1;
public:
    void prepare(double rate,std::size_t maximumBlock,unsigned factor) {
        if(factor!=1 && factor!=2 && factor!=4 && factor!=8)throw std::invalid_argument("Original amp oversampling must be 1, 2, 4 or 8");
        const auto stages=factor==8?3:factor==4?2:factor==2?1:0;
        capacity=std::max(std::size_t{1},maximumBlock);
        oversampler=std::make_unique<juce::dsp::Oversampling<float>>(2,std::size_t(stages),juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);
        oversampler->initProcessing(capacity);core.prepare(rate*factor);
    }
    void set(const State& state) noexcept { core.set(state); }
    void reset() noexcept { if(oversampler)oversampler->reset();core.reset(); }
    float latency() const noexcept { return oversampler?oversampler->getLatencyInSamples():0; }
    void process(juce::AudioBuffer<float>& buffer) noexcept {
        if(!oversampler || buffer.getNumChannels()<1 || buffer.getNumChannels()>2){buffer.clear();return;}
        juce::ScopedNoDenormals denormals;
        juce::dsp::AudioBlock<float> base(buffer);
        for(std::size_t offset=0;offset<base.getNumSamples();offset+=capacity) {
            auto part=base.getSubBlock(offset,std::min(capacity,base.getNumSamples()-offset));
            auto up=oversampler->processSamplesUp(part);
            for(std::size_t n=0;n<up.getNumSamples();++n)for(std::size_t c=0;c<up.getNumChannels();++c)
                up.setSample(int(c),int(n),core.tick(up.getSample(int(c),int(n)),int(c)));
            oversampler->processSamplesDown(part);
        }
    }
};
}
