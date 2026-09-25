#pragma once
#include "GlobalDSP.h"
#include "StudioModules.h"

namespace spectralforge {
struct FXState {
    bool envelopeFirst{true};
    std::array<int,11> models{}; // drive, delay, reverb, comp, filter, fuzz, boost, bus, preamp, EQ, modulation
    bool driveOn{},delayOn{},reverbOn{},preCompOn{},filterOn{},boostOn{},fuzzOn{},busCompOn{},preampOn{},eqOn{},chorusOn{},delaySync{};
    float preComp{.35f},preAttack{15},preLevel{},filterSense{.4f},filterQ{1.2f},filterMix{1},boostGain{6},boostBass{},boostTreble{},fuzzDrive{18},fuzzTone{.45f},fuzzLevel{-12};
    float busThreshold{-18},busRatio{4},busAttack{30},busRelease{100},busMakeup{},preampDrive{6},preampColour{.65f},preampLevel{-6};
    float eqLow{},eqMidHz{1000},eqMid{},eqQ{.707f},eqHigh{},chorusRate{.7f},chorusDepth{.3f},chorusMix{.25f};
    float drive{.3f},tone{4000},driveLevel{},delayMs{250},feedback{.25f},delayMix{.2f},room{.35f},damping{.55f},reverbMix{.15f};
};
// Original 4x-oversampled voicings, informed by the named references rather
// than circuit or capture replicas. Control filters are smoothed on a fixed
// sample clock so model changes do not jump at host-block boundaries.
class DriveModule {
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> alignment{64};
    using Filter=juce::dsp::IIR::Filter<float>;
    using C=juce::dsp::IIR::ArrayCoefficients<float>;
    std::array<Filter,2> highPass,lowPass,contour;
    std::array<float,2> bassMemory{},dcMemory{};
    ModelMorph<5> voices;
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> amount,mix,level,contourDb;
    juce::SmoothedValue<float,juce::ValueSmoothingTypes::Multiplicative> inputCutoff,toneCutoff,contourHz;
    double rate{48000};int controlClock{};
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {
        rate=spec.sampleRate;
        oversampling=std::make_unique<juce::dsp::Oversampling<float>>(spec.numChannels,2,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);oversampling->initProcessing(spec.maximumBlockSize);
        alignment.prepare(spec);alignment.setDelay(float(latency()));
        dry.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        auto mono=spec;mono.numChannels=1;
        for(size_t c=0;c<2;++c) {
            *highPass[c].coefficients=C::makeHighPass(rate,180);
            *lowPass[c].coefficients=C::makeLowPass(rate,juce::jmin(4000.0,rate*.45));
            *contour[c].coefficients=C::makePeakFilter(rate,1800,.65f,1.f);
            highPass[c].prepare(mono);lowPass[c].prepare(mono);contour[c].prepare(mono);
        }
        voices.prepare(rate*4);bassMemory={};dcMemory={};controlClock=0;
        amount.reset(rate*4,.02);amount.setCurrentAndTargetValue(.3f);
        mix.reset(rate,.015);mix.setCurrentAndTargetValue(0);level.reset(rate,.020);level.setCurrentAndTargetValue(1);
        inputCutoff.reset(rate,.035);inputCutoff.setCurrentAndTargetValue(180);
        toneCutoff.reset(rate,.02);toneCutoff.setCurrentAndTargetValue(juce::jmin(4000.0,rate*.45));
        contourHz.reset(rate,.035);contourHz.setCurrentAndTargetValue(1800);
        contourDb.reset(rate,.035);contourDb.setCurrentAndTargetValue(0);
    }
    void reset() {
        if(oversampling)oversampling->reset();alignment.reset();
        for(size_t c=0;c<2;++c){highPass[c].reset();lowPass[c].reset();contour[c].reset();}
        bassMemory={};dcMemory={};controlClock=0;mix.setCurrentAndTargetValue(0);
    }
    int latency() const {return juce::roundToInt(oversampling ? oversampling->getLatencyInSamples() : 0.f);}
    const juce::AudioBuffer<float>& cleanOutput() const {return dry;}
    void process(juce::AudioBuffer<float>& buffer,const FXState& state) {
        dry.makeCopyOf(buffer,true);juce::dsp::AudioBlock<float> dryBlock(dry);juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);alignment.process(dryContext);
        const int model=juce::jlimit(0,4,state.models[0]);voices.select(model);
        inputCutoff.setTargetValue(model==3 ? 18.f : model==4 ? 28.f : model==0 ? 180.f : model==1 ? 55.f : 90.f);
        contourHz.setTargetValue(model==3 ? 700.f : 1800.f);contourDb.setTargetValue(model==3 ? -4.5f : model==4 ? 2.f : 0.f);
        toneCutoff.setTargetValue(juce::jlimit(20.f,float(rate*.45),state.tone));
        for(int n=0;n<buffer.getNumSamples();++n) {
            const float hz=inputCutoff.getNextValue();
            if((controlClock+n)%16==0) {const auto coefficients=C::makeHighPass(rate,hz);for(auto& f:highPass)*f.coefficients=coefficients;}
            for(int c=0;c<buffer.getNumChannels();++c)buffer.setSample(c,n,highPass[(size_t)c].processSample(buffer.getSample(c,n)));
        }
        juce::dsp::AudioBlock<float> block(buffer);auto up=oversampling->processSamplesUp(block);amount.setTargetValue(state.drive);
        const float bassPole=float(1-std::exp(-juce::MathConstants<double>::twoPi*220/(rate*4)));
        const float dcPole=float(1-std::exp(-juce::MathConstants<double>::twoPi*8/(rate*4)));
        for(size_t n=0;n<up.getNumSamples();++n) {
            const float gain=1.f+amount.getNextValue()*15.f;const auto weights=voices.next();
            for(size_t c=0;c<up.getNumChannels();++c) {
                const float x=up.getSample((int)c,(int)n);bassMemory[c]+=bassPole*(x-bassMemory[c]);const float low=bassMemory[c],high=x-low;
                float y=0;
                for(size_t k=0;k<weights.size();++k)if(weights[k]>0) {
                    float voice=0;
                    switch(k) {
                        case 0:voice=std::tanh(x*gain)*.7f;break;
                        case 1:voice=.45f*x+.55f*(std::tanh(x*gain+.12f)-std::tanh(.12f));break;
                        case 2:voice=juce::jlimit(-.62f,.62f,x*gain*1.5f);break;
                        case 3:voice=.4f*x+.65f*(std::tanh((x-.35f*low)*gain*.8f+.08f)-std::tanh(.08f));break;
                        case 4:voice=.8f*low+.2f*x+.65f*(std::tanh(high*gain*1.6f+.04f)-std::tanh(.04f));break;
                    }
                    y+=weights[k]*voice;
                }
                // Asymmetric clipping creates signal-dependent DC even when f(0)=0.
                dcMemory[c]+=dcPole*(y-dcMemory[c]);up.setSample((int)c,(int)n,y-dcMemory[c]);
            }
        }
        oversampling->processSamplesDown(block);
        mix.setTargetValue(state.driveOn ? 1.f : 0.f);level.setTargetValue(juce::Decibels::decibelsToGain(state.driveLevel));
        for(int n=0;n<buffer.getNumSamples();++n) {
            const float hz=contourHz.getNextValue(),db=contourDb.getNextValue(),tone=toneCutoff.getNextValue();
            if((controlClock+n)%16==0) {
                const auto peak=C::makePeakFilter(rate,hz,.65f,juce::Decibels::decibelsToGain(db)),low=C::makeLowPass(rate,tone);
                for(size_t c=0;c<2;++c){*contour[c].coefficients=peak;*lowPass[c].coefficients=low;}
            }
            const float wet=mix.getNextValue(),gain=level.getNextValue();
            for(int c=0;c<buffer.getNumChannels();++c) {
                const float shaped=lowPass[(size_t)c].processSample(contour[(size_t)c].processSample(buffer.getSample(c,n)));
                buffer.setSample(c,n,dry.getSample(c,n)*(1-wet)+shaped*wet*gain);
            }
        }
        controlClock=(controlClock+buffer.getNumSamples())%16;
        for(size_t c=0;c<2;++c){highPass[c].snapToZero();lowPass[c].snapToZero();contour[c].snapToZero();}
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
        const auto compress=[&]{compressor.process(buffer,state.preCompOn,-12-30*state.preComp,1+5*state.preComp,state.preAttack,140,state.preLevel,state.models[3]);};
        const auto filter=[&]{envelope.process(buffer,state.filterOn,state.filterSense,state.filterQ,state.filterMix,state.models[4]);};
        if(state.envelopeFirst) {filter();compress();} else {compress();filter();}
        clean.makeCopyOf(buffer,true);juce::dsp::AudioBlock<float> block(clean);juce::dsp::ProcessContextReplacing<float> context(block);cleanAlignment.process(context);
        fuzz.process(buffer,state.fuzzOn,state.fuzzDrive,state.fuzzTone,state.fuzzLevel,true,state.models[5]);
        boost.process(buffer,state.boostOn,state.boostGain,state.boostBass,state.boostTreble,state.models[6]);
        drive.process(buffer,state);
    }
};
// Global post modules receive the merged signal exactly once in every mode.
// Echo/reverb are intentional effect delays, not hidden lane latency.
class PostFXChain {
    DynamicsModule compressor;ColourModule preamp;ConsoleEQ eq;
    ModulationModule modulation;EchoModule echo;SpaceModule space;
public:
    std::array<float,6> stagePeaks{};
    float compressorReduction() const {return compressor.reduction();}
    void prepare(const juce::dsp::ProcessSpec& spec) {compressor.prepare(spec);preamp.prepare(spec);eq.prepare(spec);modulation.prepare(spec);echo.prepare(spec);space.prepare(spec);stagePeaks={};}
    int latency() const {return preamp.latency();}
    void reset() {compressor.reset();preamp.reset();eq.reset();modulation.reset();echo.reset();space.reset();stagePeaks={};}
    void process(juce::AudioBuffer<float>& buffer,const FXState& state) {
        compressor.process(buffer,state.busCompOn,state.busThreshold,state.busRatio,state.busAttack,state.busRelease,state.busMakeup,state.models[7]);
        stagePeaks[0]=buffer.getMagnitude(0,buffer.getNumSamples());
        preamp.process(buffer,state.preampOn,state.preampDrive,state.preampColour,state.preampLevel,false,state.models[8]);
        stagePeaks[1]=buffer.getMagnitude(0,buffer.getNumSamples());
        eq.process(buffer,state.eqOn,state.eqLow,state.eqMidHz,state.eqMid,state.eqQ,state.eqHigh,state.models[9]);
        stagePeaks[2]=buffer.getMagnitude(0,buffer.getNumSamples());
        modulation.process(buffer,state.chorusOn,state.models[10],state.chorusRate,state.chorusDepth,state.chorusMix);
        stagePeaks[3]=buffer.getMagnitude(0,buffer.getNumSamples());
        echo.process(buffer,state.delayOn,state.models[1],state.delayMs,state.feedback,state.delayMix);
        stagePeaks[4]=buffer.getMagnitude(0,buffer.getNumSamples());
        space.process(buffer,state.reverbOn,state.models[2],state.room,state.damping,state.reverbMix);
        stagePeaks[5]=buffer.getMagnitude(0,buffer.getNumSamples());
    }
};
}
