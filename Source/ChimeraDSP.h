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
struct LaneState{int amp{};float drive{.35f},levelDb{},bass{},lowMid{},highMid{},treble{},presence{},resonance{};bool mute{},solo{},polarity{},cab{true},ampEnabled{true};float lowComp{},lowAmpMix{},bandTone{},fineDelayMs{},cabLow{70.f},cabHigh{9000.f};int ampChannel{};AmpNativeState native{};};
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
    std::array<Cab,3> cabs;std::array<bool,3> laneHeard{};
    std::array<MatrixTone,3> bandTones;
    std::array<juce::SmoothedValue<float>,3> levels;
    Crossover xo,diXO;
    DualCrossover dualXO;
    bool previousDualCross{};
    juce::SmoothedValue<float> dualMix,lowMix;
    juce::AudioBuffer<float> lowDry;
    // Room Core: the LOW lane's AMP/CAB share (mix x AMP/CAB, before the DI
    // blend) is the only part of that lane that may excite the room.
    juce::AudioBuffer<float> lowRoom;
    std::array<juce::SmoothedValue<float>,3> roomSendGains;
    bool roomFedLastBlock{};
    std::vector<float> blendCurve;
    LowCompressor lowCompressor;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> diAlignment{128};
    std::array<juce::AudioBuffer<float>,3> diBands;
    std::array<juce::AudioBuffer<float>,3> work;
    double sampleRate{48000.0};
    RoutingMode previousMode{static_cast<RoutingMode>(-1)};
    // A routing change jumps between two different signal paths, and a reset
    // path restarts with a hard onset that rings for ~5 ms. So the change
    // fades the old routing out (10 ms), resets and switches, keeps the
    // output muted while the new path settles (20 ms), then fades it in.
    enum class Switch { idle, out, settle, in };
    Switch switching{Switch::idle};
    juce::SmoothedValue<float> switchGain;
    int settleRemaining{}, settleLength{};
public:
    Cab& cabinet(int lane) { return cabs[(size_t)lane]; }
    // Audio thread: every cabinet the last block could be heard through is
    // settled. Lanes outside the routing mode, muted lanes and lanes with the
    // cabinet off do not count.
    bool audibleCabinetsSettled() const noexcept {
        for(size_t i=0;i<cabs.size();++i)if(laneHeard[i] && !cabs[i].settled())return false;
        return true;
    }
    // Audio thread: see Cab::setInstantSwap.
    void setInstantCabinetSwap(bool value) noexcept { for(auto& cab:cabs) cab.setInstantSwap(value); }
    float lowReduction() const {return lowCompressor.reduction();}
    void setDynamicsAnalysis(DynamicsAnalysis* value) noexcept {lowCompressor.setAnalysis(value);}
    int latency() const { return amps[0].latency(); }
    void setOversampling(int choice) { for (auto& amp : amps) amp.setOversampling(choice); }
    // Seed the engine choice before Amp::prepare initializes its native/legacy
    // mix. A restored native rig must start identically on fresh and reused
    // processors; setNative still crossfades edits made during processing.
    void prepare(const juce::dsp::ProcessSpec& spec,const std::array<AmpNativeState,3>& initialNativeStates)
    {
        for(size_t i=0;i<amps.size();++i)amps[i].setNative(initialNativeStates[i]);
        prepare(spec);
    }
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;previousMode=static_cast<RoutingMode>(-1);
        switchGain.reset(spec.sampleRate,.01);switchGain.setCurrentAndTargetValue(1);switching=Switch::idle;settleLength=int(spec.sampleRate*.02);
        xo.prepare(spec);diXO.prepare(spec);dualXO.prepare(spec);
        lowMix.reset(spec.sampleRate,.02);lowMix.setCurrentAndTargetValue(0);lowDry.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        lowRoom.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        for(auto& send:roomSendGains)send.reset(spec.sampleRate,.02);roomFedLastBlock=false;
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
        xo.reset();diXO.reset();dualXO.reset();lowCompressor.reset();diAlignment.reset();lowMix.setCurrentAndTargetValue(lowMix.getTargetValue());
        for (auto& amp : amps) amp.reset();
        for (auto& cab : cabs) cab.reset();
        for (auto& tone : bandTones) tone.reset();
    }
    // roomBus (optional, Room Core): receives each lane's audible AMP/CAB
    // contribution after lane level, polarity, mute/solo and Dual blend,
    // times roomSends[lane]. Matrix LOW sends only its AMP/CAB share. Null
    // (room off) leaves the processing exactly as without a room.
    void process(juce::AudioBuffer<float>& buffer, RoutingMode mode, float x1, float x2,
                 const std::array<LaneState,3>& states, const juce::AudioBuffer<float>* cleanInput=nullptr, bool dualCross=false, float blend=.5f,
                 juce::AudioBuffer<float>* roomBus=nullptr, const std::array<float,3>* roomSends=nullptr)
    {
        const bool feedRoom=roomBus!=nullptr && roomSends!=nullptr;
        if(feedRoom) {
            roomBus->clear();
            for(size_t i=0;i<roomSendGains.size();++i) {
                if(roomFedLastBlock)roomSendGains[i].setTargetValue((*roomSends)[i]);
                else roomSendGains[i].setCurrentAndTargetValue((*roomSends)[i]);
            }
            lowRoom.setSize(buffer.getNumChannels(),buffer.getNumSamples(),false,false,true);
        }
        roomFedLastBlock=feedRoom;
        const auto adopt=[&]{
            if(mode==RoutingMode::matrix && previousMode!=RoutingMode::matrix) amps[0].setDriveImmediately(0);
            reset(); previousMode=mode; previousDualCross=dualCross;
        };
        const bool change=mode!=previousMode || dualCross!=previousDualCross;
        if(previousMode==static_cast<RoutingMode>(-1)) adopt();   // after prepare: at once
        else if(change) {
            if(switching==Switch::idle || switching==Switch::in) {switching=Switch::out;switchGain.setTargetValue(0);}
            // Switch once silent; a further change while settling switches again.
            if((switching==Switch::out && !switchGain.isSmoothing()) || switching==Switch::settle) {
                adopt();switching=Switch::settle;settleRemaining=settleLength;
            }
        }
        else if(switching==Switch::out) {switching=Switch::in;switchGain.setTargetValue(1);}   // change undone
        // Until the switch, the old routing keeps playing.
        mode=previousMode; dualCross=previousDualCross;
        const bool matrix = mode == RoutingMode::matrix;
        const bool split=matrix || (mode==RoutingMode::dual && dualCross);
        dualMix.setTargetValue(juce::jlimit(0.f,1.f,blend));for(int n=0;n<buffer.getNumSamples();++n)blendCurve[(size_t)n]=dualMix.getNextValue();
        const int count = mode == RoutingMode::classic ? 1 : mode == RoutingMode::dual ? 2 : 3;
        for(int i=count;i<3;++i)laneHeard[(size_t)i]=false;
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
            laneHeard[(size_t)i]=!muted && state.cab;
            amps[i].setNative(state.native);
            amps[i].set(static_cast<AmpModel>(juce::jlimit(0,ampModelCount-1,state.amp)), matrix && i==0 ? 0.f : state.drive,state.ampChannel);
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
                lowDry.makeCopyOf(work[i],true);
                juce::dsp::AudioBlock<float> block(lowDry);juce::dsp::ProcessContextReplacing<float> context(block);
                diAlignment.process(context);
                // The compressed low band feeds both branches. The DI has the same
                // algorithmic delay as the head at every oversampling setting.
                // IR capture delay/phase is intentional and is not silently trimmed.
                amps[i].process(work[i],false,true);
                cabs[i].enable(state.cab);cabs[i].setCuts(state.cabLow,state.cabHigh);cabs[i].process(work[i]);
                lowMix.setTargetValue(state.ampEnabled ? juce::jlimit(0.f,1.f,state.lowAmpMix) : 0.f);
                for(int n=0;n<buffer.getNumSamples();++n) {
                    const float mix=lowMix.getNextValue();
                    for(int c=0;c<buffer.getNumChannels();++c) {
                        if(feedRoom)lowRoom.setSample(c,n,work[i].getSample(c,n)*mix);
                        work[i].setSample(c,n,lowDry.getSample(c,n)*(1-mix)+work[i].getSample(c,n)*mix);
                    }
                }
            } else {
                amps[i].process(work[i], !split,state.ampEnabled);
                cabs[i].enable(state.cab);
                cabs[i].setCuts(state.cabLow, state.cabHigh);
                cabs[i].process(work[i]);
            }
            levels[i].setTargetValue(muted ? 0.f : juce::Decibels::decibelsToGain(state.levelDb) * (state.polarity ? -1.f : 1.f));
            const bool lowShare=matrix && i==0;
            for(int n=0;n<buffer.getNumSamples();++n) {
                const float weight=mode==RoutingMode::dual && !dualCross ? (i==0 ? 1-blendCurve[(size_t)n] : blendCurve[(size_t)n]) : 1.f;
                const float gain=levels[i].getNextValue()*weight;
                if(feedRoom) {
                    const float send=roomSendGains[(size_t)i].getNextValue();
                    for(int c=0;c<buffer.getNumChannels();++c) {
                        const float direct=work[i].getSample(c,n)*gain;
                        work[i].setSample(c,n,direct);
                        roomBus->addSample(c,n,(lowShare ? lowRoom.getSample(c,n)*gain : direct)*send);
                    }
                }
                else for(int c=0;c<buffer.getNumChannels();++c) work[i].setSample(c,n,work[i].getSample(c,n)*gain);
            }
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                buffer.addFrom(c, 0, work[i], c, 0, buffer.getNumSamples(),
                               1.0f);
        }
        if(switching!=Switch::idle) {
            for(int n=0;n<buffer.getNumSamples();++n) {
                const float gain=switching==Switch::settle ? 0.f : switchGain.getNextValue();
                for(int c=0;c<buffer.getNumChannels();++c) {
                    buffer.setSample(c,n,buffer.getSample(c,n)*gain);
                    if(feedRoom)roomBus->setSample(c,n,roomBus->getSample(c,n)*gain);
                }
            }
            if(switching==Switch::settle && (settleRemaining-=buffer.getNumSamples())<=0) {switching=Switch::in;switchGain.setTargetValue(1);}
            else if(switching==Switch::in && !switchGain.isSmoothing()) switching=Switch::idle;
        }
    }
};
}
