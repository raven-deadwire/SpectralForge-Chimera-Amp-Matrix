#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include "MatrixTone.h"
#include "Amplifier.h"
#include "Cabinet.h"
#include "LowCompressor.h"
namespace spectralforge {
enum class RoutingMode:int{classic,dual,matrix};
struct LaneState{int amp{};float drive{.35f},levelDb{},bass{},lowMid{},highMid{},treble{},presence{},resonance{};bool mute{},solo{},polarity{},cab{true},ampEnabled{true};float lowComp{},bandTone{},fineDelayMs{},cabLow{70.f},cabHigh{9000.f};};
class Crossover {
    using LR=juce::dsp::LinkwitzRileyFilter<float>;
    LR lowSplit,highSplit,lowPhase;
    juce::SmoothedValue<float,juce::ValueSmoothingTypes::Multiplicative> low,high;
    double sampleRate{48000};
    int coefficientPhase{};
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate=spec.sampleRate; lowSplit.prepare(spec); highSplit.prepare(spec); lowPhase.prepare(spec);
        lowPhase.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
        low.reset(sampleRate,.050); high.reset(sampleRate,.050);
        low.setCurrentAndTargetValue(150); high.setCurrentAndTargetValue(1200); reset();
    }
    void reset() {
        lowSplit.reset(); highSplit.reset(); lowPhase.reset(); coefficientPhase=0;
        low.setCurrentAndTargetValue(low.getTargetValue()); high.setCurrentAndTargetValue(high.getTargetValue());
    }
    void set(float x1,float x2) {
        low.setTargetValue(juce::jlimit(30.f,float(sampleRate*.2),x1));
        high.setTargetValue(juce::jlimit(low.getTargetValue()+10.f,float(sampleRate*.45),x2));
    }
    void split(const juce::AudioBuffer<float>& input,std::array<juce::AudioBuffer<float>,3>& bands) {
        for(auto& buffer:bands) buffer.setSize(input.getNumChannels(),input.getNumSamples(),false,false,true);
        for(int n=0;n<input.getNumSamples();++n) {
            const auto a=low.getNextValue(),b=high.getNextValue();
            // Identical coefficients for the high split and low allpass, including
            // during automation. The 16-sample clock persists across host blocks.
            if(coefficientPhase++%16==0) {lowSplit.setCutoffFrequency(a);highSplit.setCutoffFrequency(b);lowPhase.setCutoffFrequency(b);}
            coefficientPhase%=16;
            for(int c=0;c<input.getNumChannels();++c) {
                float lo{},upper{},mid{},hi{};
                lowSplit.processSample(c,input.getSample(c,n),lo,upper);
                highSplit.processSample(c,upper,mid,hi);
                bands[0].setSample(c,n,lowPhase.processSample(c,lo));bands[1].setSample(c,n,mid);bands[2].setSample(c,n,hi);
            }
        }
    }
};
class DualCrossover {
    juce::dsp::LinkwitzRileyFilter<float> filter;
    juce::SmoothedValue<float,juce::ValueSmoothingTypes::Multiplicative> frequency;
    double rate{48000};int clock{};
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {rate=spec.sampleRate;filter.prepare(spec);frequency.reset(rate,.05);frequency.setCurrentAndTargetValue(350);reset();}
    void reset() {filter.reset();frequency.setCurrentAndTargetValue(frequency.getTargetValue());clock=0;}
    void split(const juce::AudioBuffer<float>& input,std::array<juce::AudioBuffer<float>,3>& bands,float hz) {
        frequency.setTargetValue(juce::jlimit(30.f,float(rate*.45),hz));
        for(int i=0;i<2;++i)bands[i].setSize(input.getNumChannels(),input.getNumSamples(),false,false,true);
        for(int n=0;n<input.getNumSamples();++n) {
            const float cutoff=frequency.getNextValue();if(clock++%16==0)filter.setCutoffFrequency(cutoff);clock%=16;
            for(int c=0;c<input.getNumChannels();++c) {float low{},high{};filter.processSample(c,input.getSample(c,n),low,high);bands[0].setSample(c,n,low);bands[1].setSample(c,n,high);}
        }
    }
};
class Engine {
    std::array<Amp,3> amps;
    std::array<Cab,3> cabs;
    std::array<MatrixTone,3> bandTones;
    std::array<juce::SmoothedValue<float>,3> levels;
    Crossover xo,diXO;
    DualCrossover dualXO;
    bool previousDualCross{};
    juce::SmoothedValue<float> dualMix;
    std::vector<float> blendCurve;
    LowCompressor lowCompressor;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> diAlignment{128};
    std::array<juce::AudioBuffer<float>,3> diBands;
    std::array<juce::AudioBuffer<float>,3> work;
    double sampleRate{48000.0};
    RoutingMode previousMode{static_cast<RoutingMode>(-1)};
public:
    Cab& cabinet(int lane) { return cabs[(size_t)lane]; }
    float lowReduction() const {return lowCompressor.reduction();}
    int latency() const { return amps[0].latency(); }
    void setOversampling(int choice) { for (auto& amp : amps) amp.setOversampling(choice); }
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;previousMode=static_cast<RoutingMode>(-1);
        xo.prepare(spec);diXO.prepare(spec);dualXO.prepare(spec);
        dualMix.reset(spec.sampleRate,.02);dualMix.setCurrentAndTargetValue(.5f);blendCurve.resize(spec.maximumBlockSize);lowCompressor.prepare(spec.sampleRate);
        for (auto& amp : amps) amp.prepare(spec);
        diAlignment.prepare(spec);diAlignment.setDelay(float(latency()));
        for(auto& buffer:diBands) buffer.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        for (auto& cab : cabs) cab.prepare(spec);
        for (auto& tone : bandTones) tone.prepare(spec);
        for (auto& gain : levels) { gain.reset(spec.sampleRate,.010); gain.setCurrentAndTargetValue(1); }
        for (auto& buffer : work)
            buffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));
    }
    void reset()
    {
        xo.reset();diXO.reset();dualXO.reset();lowCompressor.reset();diAlignment.reset();
        for (auto& amp : amps) amp.reset();
        for (auto& cab : cabs) cab.reset();
        for (auto& tone : bandTones) tone.reset();
    }
    void process(juce::AudioBuffer<float>& buffer, RoutingMode mode, float x1, float x2,
                 const std::array<LaneState,3>& states, const juce::AudioBuffer<float>* cleanInput=nullptr, bool dualCross=false, float blend=.5f)
    {
        if (mode != previousMode || dualCross!=previousDualCross) { reset(); previousMode = mode; previousDualCross=dualCross; }
        const bool matrix = mode == RoutingMode::matrix;
        const bool split=matrix || (mode==RoutingMode::dual && dualCross);
        dualMix.setTargetValue(juce::jlimit(0.f,1.f,blend));for(int n=0;n<buffer.getNumSamples();++n)blendCurve[(size_t)n]=dualMix.getNextValue();
        const int count = mode == RoutingMode::classic ? 1 : mode == RoutingMode::dual ? 2 : 3;
        bool anySolo = false;
        for (int i = 0; i < count; ++i) anySolo = anySolo || states[i].solo;
        if (matrix) {
            xo.set(x1, x2); xo.split(buffer, work);
            if(cleanInput) {diXO.set(x1,x2);diXO.split(*cleanInput,diBands);work[0].makeCopyOf(diBands[0],true);}
        }
        else if(mode==RoutingMode::dual && dualCross) dualXO.split(buffer,work,x1);
        else for (int i = 0; i < count; ++i) work[i].makeCopyOf(buffer, true);
        buffer.clear();
        for (int i = 0; i < count; ++i)
        {
            const auto& state = states[i];
            const bool muted = state.mute || (anySolo && !state.solo);
            amps[i].set(static_cast<AmpModel>(juce::jlimit(0,7,state.amp)), state.drive);
            if (split)
            {
                bandTones[i].set(matrix ? matrixTonePivot(i,x1,x2,sampleRate) : matrixTonePivot(i==0 ? 0 : 2,x1,x1,sampleRate),state.bandTone);
                bandTones[i].process(work[i]);
            }
            else
                amps[i].tone(state.bass, state.lowMid, state.highMid, state.treble,
                             state.presence, state.resonance);
            // Full-range EQ/presence/resonance are bypassed in Matrix, including
            // values recalled from previous Classic/Dual sessions. Keep their state.
            if(matrix && i==0) {
                lowCompressor.process(work[i],state.lowComp);
                juce::dsp::AudioBlock<float> block(work[i]);juce::dsp::ProcessContextReplacing<float> context(block);
                diAlignment.process(context);
            } else {
                amps[i].process(work[i], !split,state.ampEnabled);
                cabs[i].enable(state.cab);
                cabs[i].setCuts(state.cabLow, state.cabHigh);
                cabs[i].process(work[i]);
            }
            levels[i].setTargetValue(muted ? 0.f : juce::Decibels::decibelsToGain(state.levelDb) * (state.polarity ? -1.f : 1.f));
            for(int n=0;n<buffer.getNumSamples();++n) {
                const float weight=mode==RoutingMode::dual && !dualCross ? (i==0 ? 1-blendCurve[(size_t)n] : blendCurve[(size_t)n]) : 1.f;
                const float gain=levels[i].getNextValue()*weight;
                for(int c=0;c<buffer.getNumChannels();++c) work[i].setSample(c,n,work[i].getSample(c,n)*gain);
            }
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                buffer.addFrom(c, 0, work[i], c, 0, buffer.getNumSamples(),
                               1.0f);
        }
    }
};
}
