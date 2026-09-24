#pragma once
#include "GlobalDSP.h"
#include "StudioModules.h"

namespace spectralforge {
struct FXState {
    bool driveOn{},delayOn{},reverbOn{},preCompOn{},filterOn{},boostOn{},fuzzOn{},busCompOn{},preampOn{},eqOn{},chorusOn{},delaySync{};
    float preComp{.35f},preAttack{15},preLevel{},filterSense{.4f},filterQ{1.2f},filterMix{1},boostGain{6},boostBass{},boostTreble{},fuzzDrive{18},fuzzTone{.45f},fuzzLevel{-12};
    float busThreshold{-18},busRatio{4},busAttack{30},busRelease{100},busMakeup{},preampDrive{6},preampColour{.65f},preampLevel{-6};
    float eqLow{},eqMidHz{1000},eqMid{},eqQ{.707f},eqHigh{},chorusRate{.7f},chorusDepth{.3f},chorusMix{.25f};
    float drive{.3f},tone{4000},driveLevel{},delayMs{250},feedback{.25f},delayMix{.2f},room{.35f},damping{.55f},reverbMix{.15f};
};
class DriveModule {
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> alignment{64};
    using Filter=juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,juce::dsp::IIR::Coefficients<float>>;
    Filter highPass,lowPass;
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> amount,mix,level;
    double rate{48000};
public:

    void prepare(const juce::dsp::ProcessSpec& spec) {
        rate=spec.sampleRate;
        // The oversampler is re-created only during prepare, never on audio.
        oversampling=std::make_unique<juce::dsp::Oversampling<float>>(spec.numChannels,2,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true); oversampling->initProcessing(spec.maximumBlockSize);
        alignment.prepare(spec); alignment.setDelay(float(latency()));
        dry.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        *highPass.state=juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(rate,80);
        *lowPass.state=juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(rate,juce::jmin(4000.0,rate*.45));
        highPass.prepare(spec);lowPass.prepare(spec);
        amount.reset(rate*4,.02);amount.setCurrentAndTargetValue(.3f);
        mix.reset(rate,.015);mix.setCurrentAndTargetValue(0);level.reset(rate,.020);level.setCurrentAndTargetValue(1);
    }
    void reset() {if(oversampling) oversampling->reset();alignment.reset();highPass.reset();lowPass.reset();mix.setCurrentAndTargetValue(0);}
    int latency() const {return juce::roundToInt(oversampling ? oversampling->getLatencyInSamples() : 0.f);}
    const juce::AudioBuffer<float>& cleanOutput() const {return dry;}
    void process(juce::AudioBuffer<float>& buffer,const FXState& state) {
        dry.makeCopyOf(buffer,true);juce::dsp::AudioBlock<float> dryBlock(dry);juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);alignment.process(dryContext);
        juce::dsp::AudioBlock<float> block(buffer);juce::dsp::ProcessContextReplacing<float> context(block);highPass.process(context);
        auto up=oversampling->processSamplesUp(block); amount.setTargetValue(state.drive);
        for(size_t n=0;n<up.getNumSamples();++n) {const float gain=1.f+amount.getNextValue()*15.f;for(size_t c=0;c<up.getNumChannels();++c) up.setSample((int)c,(int)n,std::tanh(up.getSample((int)c,(int)n)*gain)*.7f);}
        oversampling->processSamplesDown(block);
        *lowPass.state=juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(rate,juce::jmin(double(state.tone),rate*.45));lowPass.process(context);
        mix.setTargetValue(state.driveOn ? 1.f : 0.f);level.setTargetValue(juce::Decibels::decibelsToGain(state.driveLevel));
        for(int n=0;n<buffer.getNumSamples();++n) {const float wet=mix.getNextValue(),gain=level.getNextValue();for(int c=0;c<buffer.getNumChannels();++c) buffer.setSample(c,n,dry.getSample(c,n)*(1-wet)+buffer.getSample(c,n)*wet*gain);}
    }
};
// Gate and pitch are shared. The drive exposes its latency-aligned clean tap
// for Matrix LOW DI; other active rigs receive its wet/bypassed output.
class PreFXChain {
public:
    NoiseGate gate;
    Transposer transpose;
    DriveModule drive;
    DynamicsModule compressor;
    EnvelopeModule envelope;
    ColourModule fuzz;
    BoostModule boost;
    juce::AudioBuffer<float> clean;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> cleanAlignment{128};
    void prepare(const juce::dsp::ProcessSpec& spec) {gate.prepare(spec.sampleRate);transpose.prepare(spec);compressor.prepare(spec);envelope.prepare(spec);fuzz.prepare(spec);boost.prepare(spec);drive.prepare(spec);clean.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);cleanAlignment.prepare(spec);cleanAlignment.setDelay(float(fuzz.latency()+drive.latency()));}
    void reset() {gate.reset();transpose.reset();compressor.reset();envelope.reset();fuzz.reset();boost.reset();drive.reset();cleanAlignment.reset();}
    const juce::AudioBuffer<float>& cleanOutput() const {return clean;}
    int latency(bool pitchEnabled) const {return fuzz.latency()+drive.latency()+(pitchEnabled ? transpose.latency() : 0);}
    void process(juce::AudioBuffer<float>& buffer,bool gateOn,float threshold,float release,float hold,bool pitchOn,int semitones,const FXState& state) {
        gate.process(buffer,gateOn,threshold,release,hold);transpose.process(buffer,pitchOn,semitones);
        compressor.process(buffer,state.preCompOn,-12-30*state.preComp,1+5*state.preComp,state.preAttack,140,state.preLevel);
        envelope.process(buffer,state.filterOn,state.filterSense,state.filterQ,state.filterMix);
        clean.makeCopyOf(buffer,true);juce::dsp::AudioBlock<float> block(clean);juce::dsp::ProcessContextReplacing<float> context(block);cleanAlignment.process(context);
        fuzz.process(buffer,state.fuzzOn,state.fuzzDrive,state.fuzzTone,state.fuzzLevel,true);
        boost.process(buffer,state.boostOn,state.boostGain,state.boostBass,state.boostTreble);
        drive.process(buffer,state);
    }
};
// Global post modules receive the merged signal exactly once in every mode.
// Echo/reverb are intentional effect delays, not hidden lane latency.
class PostFXChain {
    DynamicsModule compressor;ColourModule preamp;ConsoleEQ eq;juce::dsp::Chorus<float> chorus;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear> delay;
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> wetBuffer;
    juce::SmoothedValue<float> time,feedback,delayMix,reverbMix;
    double rate{48000};
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {
        compressor.prepare(spec);preamp.prepare(spec);eq.prepare(spec);chorus.setMix(0);chorus.prepare(spec);chorus.setCentreDelay(8);
        rate=spec.sampleRate;delay.setMaximumDelayInSamples(int(rate*2.1));delay.prepare(spec);reverb.prepare(spec);
        wetBuffer.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        for(auto* value:{&time,&feedback,&delayMix,&reverbMix}) value->reset(rate,.030);
        time.setCurrentAndTargetValue(float(rate*.25));feedback.setCurrentAndTargetValue(.25f);delayMix.setCurrentAndTargetValue(0);reverbMix.setCurrentAndTargetValue(0);
    }
    int latency() const {return preamp.latency();}
    void reset() {compressor.reset();preamp.reset();eq.reset();chorus.setMix(0);chorus.reset();delay.reset();reverb.reset();delayMix.setCurrentAndTargetValue(0);reverbMix.setCurrentAndTargetValue(0);}
    void process(juce::AudioBuffer<float>& buffer,const FXState& state) {
        compressor.process(buffer,state.busCompOn,state.busThreshold,state.busRatio,state.busAttack,state.busRelease,state.busMakeup);
        preamp.process(buffer,state.preampOn,state.preampDrive,state.preampColour,state.preampLevel);
        eq.process(buffer,state.eqOn,state.eqLow,state.eqMidHz,state.eqMid,state.eqQ,state.eqHigh);
        chorus.setRate(state.chorusRate);chorus.setDepth(state.chorusDepth);chorus.setMix(state.chorusOn ? state.chorusMix : 0.f);
        {juce::dsp::AudioBlock<float> block(buffer);juce::dsp::ProcessContextReplacing<float> context(block);chorus.process(context);}
        time.setTargetValue(float(rate)*state.delayMs*.001f);feedback.setTargetValue(juce::jlimit(0.f,.85f,state.feedback));delayMix.setTargetValue(state.delayOn ? state.delayMix : 0.f);
        for(int n=0;n<buffer.getNumSamples();++n) {const float samples=time.getNextValue(),fb=feedback.getNextValue(),mix=delayMix.getNextValue();for(int c=0;c<buffer.getNumChannels();++c) {
            const float dry=buffer.getSample(c,n),echo=delay.popSample(c,samples);delay.pushSample(c,(state.delayOn ? dry : 0.f)+echo*fb);buffer.setSample(c,n,dry*(1-mix)+echo*mix);
        }}
        juce::Reverb::Parameters parameters;parameters.roomSize=state.room;parameters.damping=state.damping;parameters.wetLevel=1;parameters.dryLevel=0;parameters.width=1;parameters.freezeMode=0;reverb.setParameters(parameters);
        wetBuffer.makeCopyOf(buffer,true);if(!state.reverbOn) wetBuffer.clear();
        juce::dsp::AudioBlock<float> block(wetBuffer);juce::dsp::ProcessContextReplacing<float> context(block);reverb.process(context);
        reverbMix.setTargetValue(state.reverbOn ? state.reverbMix : 0.f);
        for(int n=0;n<buffer.getNumSamples();++n) {const float mix=reverbMix.getNextValue();for(int c=0;c<buffer.getNumChannels();++c) buffer.setSample(c,n,buffer.getSample(c,n)*(1-mix)+wetBuffer.getSample(c,n)*mix);}
    }
};
}
