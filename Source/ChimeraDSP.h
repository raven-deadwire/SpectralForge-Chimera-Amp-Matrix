#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include "MatrixTone.h"
namespace spectralforge {
enum class RoutingMode:int{classic,dual,matrix};
enum class AmpModel:int{glass,britEdge,tight515,wideRect,liquidLead,ironTube,solidPunch,modernBass};
struct LaneState{int amp{};float drive{.35f},levelDb{},bass{},lowMid{},highMid{},treble{},presence{},resonance{};bool mute{},solo{},polarity{},cab{true};float bandTone{},fineDelayMs{},cabLow{70.f},cabHigh{9000.f};};
class Crossover {
 using LR=juce::dsp::LinkwitzRileyFilter<float>; LR a,b,phase;
public:void prepare(const juce::dsp::ProcessSpec&s){a.prepare(s);b.prepare(s);phase.prepare(s);phase.setType(juce::dsp::LinkwitzRileyFilterType::allpass);set(150,1200);}
 void reset(){a.reset();b.reset();phase.reset();} void set(float x1,float x2){a.setCutoffFrequency(x1);b.setCutoffFrequency(x2);phase.setCutoffFrequency(x2);}
 void split(const juce::AudioBuffer<float>&in,std::array<juce::AudioBuffer<float>,3>&o){for(auto&x:o)x.setSize(in.getNumChannels(),in.getNumSamples(),false,false,true);for(int c=0;c<in.getNumChannels();++c)for(int n=0;n<in.getNumSamples();++n){float lo{},up{},mi{},hi{};a.processSample(c,in.getSample(c,n),lo,up);b.processSample(c,up,mi,hi);o[0].setSample(c,n,phase.processSample(c,lo));o[1].setSample(c,n,mi);o[2].setSample(c,n,hi);}}
};
class Amp {
 double sr{48000}; AmpModel model{AmpModel::glass}; float drive{.35f}; using IIR=juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,juce::dsp::IIR::Coefficients<float>>; IIR lo,lm,hm,hi,pres,res; std::unique_ptr<juce::dsp::Oversampling<float>> os;
public:void prepare(const juce::dsp::ProcessSpec&s){sr=s.sampleRate;tone(0,0,0,0,0,0);lo.prepare(s);lm.prepare(s);hm.prepare(s);hi.prepare(s);pres.prepare(s);res.prepare(s);os=std::make_unique<juce::dsp::Oversampling<float>>(s.numChannels,2,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);os->initProcessing(s.maximumBlockSize);tone(0,0,0,0,0,0);}
 void reset(){lo.reset();lm.reset();hm.reset();hi.reset();pres.reset();res.reset();if(os)os->reset();} void set(AmpModel m,float d){model=m;drive=juce::jlimit(0.f,1.f,d);}
 float latency()const{return os?os->getLatencyInSamples():0.f;}
 void tone(float bass,float lowmid,float highmid,float treble,float presence,float resonance){float bf=80,lmf=450,hmf=1500,hf=4000,pf=2500,rf=90;if(model==AmpModel::ironTube){bf=40;lmf=220;hmf=800;hf=4000;rf=70;}else if(model==AmpModel::solidPunch){bf=60;lmf=250;hmf=1000;hf=5000;pf=4500;}else if(model==AmpModel::tight515){bf=110;lmf=500;hmf=1200;hf=3800;pf=2000;rf=95;}*lo.state=juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf(sr,bf,.707f,juce::Decibels::decibelsToGain(bass));*lm.state=juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(sr,lmf,.8f,juce::Decibels::decibelsToGain(lowmid));*hm.state=juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(sr,hmf,.8f,juce::Decibels::decibelsToGain(highmid));*hi.state=juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(sr,hf,.707f,juce::Decibels::decibelsToGain(treble));*pres.state=juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(sr,pf,.8f,juce::Decibels::decibelsToGain(presence));*res.state=juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(sr,rf,1.1f,juce::Decibels::decibelsToGain(resonance));}
 void process(juce::AudioBuffer<float>&b,bool useFullRangeTone=true){juce::dsp::AudioBlock<float>base(b);auto up=os->processSamplesUp(base);for(size_t c=0;c<up.getNumChannels();++c)for(size_t n=0;n<up.getNumSamples();++n){float x=up.getSample((int)c,(int)n),g=1.f+drive*18.f;switch(model){case AmpModel::glass:x=.9f*x+.1f*std::tanh(x*(1.f+drive));break;case AmpModel::ironTube:x=.58f*x+.42f*std::tanh((x+.08f*x*x)*(1.f+drive*7.f));break;case AmpModel::solidPunch:x=.25f*x+.75f*std::tanh(x*(1.f+drive*4.f));break;case AmpModel::modernBass:x=.5f*x+.5f*std::tanh(x*(1.f+drive*10.f));break;default:{float y=std::tanh(x*g);x=std::tanh((y-.03f*y*y)*(1.8f+drive*4.f));break;}}up.setSample((int)c,(int)n,x);}os->processSamplesDown(base);juce::dsp::ProcessContextReplacing<float>ctx(base);if(useFullRangeTone){lo.process(ctx);lm.process(ctx);hm.process(ctx);hi.process(ctx);res.process(ctx);pres.process(ctx);}}
};
class Cab{using IIR=juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,juce::dsp::IIR::Coefficients<float>>;IIR hp,lp;juce::dsp::Convolution conv;double sr{48000};bool on{true};
public:void prepare(const juce::dsp::ProcessSpec&s){sr=s.sampleRate;setCuts(70,9000);hp.prepare(s);lp.prepare(s);conv.prepare(s);setCuts(70,9000);}void reset(){hp.reset();lp.reset();conv.reset();}void enable(bool x){on=x;}void setCuts(float a,float b){*hp.state=juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sr,a);*lp.state=juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(sr,b);}bool load(const juce::File&f){if(!f.existsAsFile())return false;conv.loadImpulseResponse(f,juce::dsp::Convolution::Stereo::yes,juce::dsp::Convolution::Trim::yes,0,juce::dsp::Convolution::Normalise::yes);return true;}void process(juce::AudioBuffer<float>&b){if(!on)return;juce::dsp::AudioBlock<float>bl(b);juce::dsp::ProcessContextReplacing<float>ctx(bl);hp.process(ctx);conv.process(ctx);lp.process(ctx);}};
class Engine {
    std::array<Amp,3> amps;
    std::array<Cab,3> cabs;
    std::array<MatrixTone,3> bandTones;
    Crossover xo;
    std::array<juce::AudioBuffer<float>,3> work;
    double sampleRate{48000.0};
    RoutingMode previousMode{RoutingMode::classic};
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        xo.prepare(spec);
        for (auto& amp : amps) amp.prepare(spec);
        for (auto& cab : cabs) cab.prepare(spec);
        for (auto& tone : bandTones) tone.prepare(spec);
        for (auto& buffer : work)
            buffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));
    }
    void reset()
    {
        xo.reset();
        for (auto& amp : amps) amp.reset();
        for (auto& cab : cabs) cab.reset();
        for (auto& tone : bandTones) tone.reset();
    }
    void process(juce::AudioBuffer<float>& buffer, RoutingMode mode, float x1, float x2,
                 const std::array<LaneState,3>& states)
    {
        if (mode != previousMode) { reset(); previousMode = mode; }
        const bool matrix = mode == RoutingMode::matrix;
        const int count = mode == RoutingMode::classic ? 1 : mode == RoutingMode::dual ? 2 : 3;
        bool anySolo = false;
        for (int i = 0; i < count; ++i) anySolo = anySolo || states[i].solo;
        if (matrix) { xo.set(x1, x2); xo.split(buffer, work); }
        else for (int i = 0; i < count; ++i) work[i].makeCopyOf(buffer, true);
        buffer.clear();
        for (int i = 0; i < count; ++i)
        {
            const auto& state = states[i];
            if (state.mute || (anySolo && !state.solo)) continue;
            amps[i].set(static_cast<AmpModel>(juce::jlimit(0,7,state.amp)), state.drive);
            if (matrix)
            {
                bandTones[i].set(matrixTonePivot(i, x1, x2, sampleRate), state.bandTone);
                bandTones[i].process(work[i]);
            }
            else
                amps[i].tone(state.bass, state.lowMid, state.highMid, state.treble,
                             state.presence, state.resonance);
            // Full-range EQ/presence/resonance are bypassed in Matrix, including
            // values recalled from previous Classic/Dual sessions. Keep their state.
            amps[i].process(work[i], !matrix);
            cabs[i].enable(state.cab);
            cabs[i].setCuts(state.cabLow, state.cabHigh);
            cabs[i].process(work[i]);
            if (state.polarity) work[i].applyGain(-1.0f);
            work[i].applyGain(juce::Decibels::decibelsToGain(state.levelDb));
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                buffer.addFrom(c, 0, work[i], c, 0, buffer.getNumSamples(),
                               mode == RoutingMode::dual ? 0.5f : 1.0f);
        }
    }
};
}
