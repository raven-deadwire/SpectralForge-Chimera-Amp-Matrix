#pragma once
#include "FXChain.h"
#include "PedalBoardCatalog.h"

namespace spectralforge {
// These are original, experimental control curves. They do not transform the
// old raw state into hardware positions, nor certify the referenced circuits.
// The untouched PreFXChain is still used whenever boardEnabled is false.
class PedalNewDrive {
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    std::array<std::array<float,2>,2> low{},tone{},dc{};
    std::array<juce::SmoothedValue<float>,8> controls;
    std::array<float,3> highPassPoles{};std::array<float,2> tonePoles{};
    double rate{48000};float dcPole{};int controlClock{};
    float circuit(float x,int circuitIndex,int channel,float gain,float tonePosition,float volume,int shape) {
        const auto k=(size_t)circuitIndex,c=(size_t)channel;
        juce::ignoreUnused(tonePosition);
        const float hp=highPassPoles[(size_t)shape];
        low[k][c]+=hp*(x-low[k][c]);x-=low[k][c]*(shape==1?.35f:.85f);
        const float driven=x*(1+gain*(shape==2?160.f:shape==1?45.f:85.f));
        float y=shape==2 ? juce::jlimit(-.42f,.42f,driven) : shape==1 ? .8f*std::tanh(driven/.8f) : .7f*(std::tanh(driven+.25f)-std::tanh(.25f));
        tone[k][c]+=tonePoles[k]*(y-tone[k][c]);y=tone[k][c];
        dc[k][c]+=dcPole*(y-dc[k][c]);
        return (y-dc[k][c])*(4*volume*volume);
    }
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {
        rate=spec.sampleRate;
        for(size_t k=0;k<highPassPoles.size();++k)highPassPoles[k]=float(1-std::exp(-juce::MathConstants<double>::twoPi*(k==0?280:k==1?70:110)/(rate*4)));
        dcPole=float(1-std::exp(-juce::MathConstants<double>::twoPi*8/(rate*4)));
        oversampling=std::make_unique<juce::dsp::Oversampling<float>>(spec.numChannels,2,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);
        oversampling->initProcessing(spec.maximumBlockSize);
        for(auto& c:controls) {c.reset(rate*4,.02);c.setCurrentAndTargetValue(.5f);}reset();
    }
    void reset(){if(oversampling)oversampling->reset();low={};tone={};dc={};controlClock=0;}
    int latency()const {return oversampling?juce::roundToInt(oversampling->getLatencyInSamples()):0;}
    void process(juce::AudioBuffer<float>& buffer,const PedalInstanceState& state) {
        for(size_t i=0;i<controls.size();++i)controls[i].setTargetValue(state.controls[i]);
        juce::dsp::AudioBlock<float> block(buffer);auto up=oversampling->processSamplesUp(block);
        const int mode=juce::jlimit(0,5,juce::roundToInt(state.controls[6]));
        for(size_t n=0;n<up.getNumSamples();++n) {
            std::array<float,8> p;for(size_t i=0;i<p.size();++i)p[i]=controls[i].getNextValue();
            if(controlClock++%32==0) {
                const float firstTone=state.model==27?juce::jlimit(0.f,1.f,p[2]+(state.controls[3]<.5f?.1f:-.1f)):p[1];
                tonePoles[0]=float(1-std::exp(-juce::MathConstants<double>::twoPi*(state.model==28?7200.f:700.f*std::pow(18.f,firstTone))/(rate*4)));
                tonePoles[1]=float(1-std::exp(-juce::MathConstants<double>::twoPi*700.f*std::pow(18.f,p[4])/(rate*4)));
            }
            for(size_t c=0;c<up.getNumChannels();++c) {
                const float x=up.getSample((int)c,(int)n);float y=x;
                if(state.model==26)y=circuit(x,0,(int)c,p[0],p[1],p[2],0);
                else if(state.model==27) {
                    const bool highPeak=state.controls[3]<.5f;
                    y=circuit(x,0,(int)c,p[1]*(highPeak?1.25f:.8f),juce::jlimit(0.f,1.f,p[2]+(highPeak?.1f:-.1f)),p[0],1);
                } else if(state.model==28)y=circuit(x,0,(int)c,p[0],.5f,p[1],2);
                else {
                    const auto boss=[&](float v){return circuit(v,0,(int)c,p[0],p[1],p[2],0);};
                    const auto jhs=[&](float v){return circuit(v,1,(int)c,p[3],p[4],p[5],1);};
                    if(mode==0)y=jhs(x);else if(mode==1)y=boss(x);
                    else if(mode==2)y=state.controls[7]>.5f?boss(x):jhs(x);
                    else if(mode==3)y=boss(jhs(x));else if(mode==4)y=jhs(boss(x));
                    else y=.5f*(boss(x)+jhs(x));
                }
                up.setSample((int)c,(int)n,y);
            }
        }
        controlClock%=32;
        oversampling->processSamplesDown(block);
    }
};

class PedalEffectInstance {
    using Delay=juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None>;
    using F=juce::dsp::IIR::Filter<float>;
    using C=juce::dsp::IIR::ArrayCoefficients<float>;
    DriveModule drive;ColourModule fuzz;DynamicsModule compressor;BoostModule boost;ModulationModule modulation;
    PedalNewDrive newDrive;
    std::array<std::array<F,2>,10> eq;
    juce::dsp::StateVariableTPTFilter<float> envelopeFilter;
    juce::AudioBuffer<float> dry,source;
    Delay dryDelay{128},padding{128};
    juce::SmoothedValue<float> wet,outputGain,wahPosition;
    double rate{48000};float envelope{};int lastModel{},fixedLatency{},filterClock{};
    float appliedGR{};
    void equalize(juce::AudioBuffer<float>& b,int index,const std::array<float,6>& coefficients) {
        auto& filters=eq[(size_t)index];for(auto& f:filters)*f.coefficients=coefficients;
        for(int c=0;c<b.getNumChannels();++c)for(int n=0;n<b.getNumSamples();++n)b.setSample(c,n,filters[(size_t)c].processSample(b.getSample(c,n)));
    }
    static float gain(float position) {return 4*position*position;}
    void volume(juce::AudioBuffer<float>& b,float target) {
        outputGain.setTargetValue(target);
        for(int n=0;n<b.getNumSamples();++n) {const float value=outputGain.getNextValue();for(int c=0;c<b.getNumChannels();++c)b.setSample(c,n,b.getSample(c,n)*value);}
    }
    void processCompressor(juce::AudioBuffer<float>& b,const PedalInstanceState& s) {
        const auto& p=s.controls;float threshold=-24,ratio=4,attack=5,release=140,input=1,output=1,dryLevel=0;
        const int character=s.model-6;
        if(s.model==6) {const std::array<float,4> ratios{4,8,12,20};ratio=ratios[(size_t)juce::jlimit(0,3,juce::roundToInt(p[3]))];attack=.02f+.78f*p[1];release=40*std::pow(20.f,p[0]);input=juce::Decibels::decibelsToGain(-12+36*p[4]);output=gain(p[2]);}
        else if(s.model==7) {threshold=-6-42*p[1];ratio=4;attack=5;release=180;output=gain(p[0]);}
        else if(s.model==8) {threshold=-8-36*p[0];ratio=2+6*p[0];attack=12;release=200;output=gain(p[2]);}
        else if(s.model==9) {input=juce::Decibels::decibelsToGain(-12+36*p[0]);output=gain(p[1]);dryLevel=p[2];ratio=4+16*p[3];attack=.1f+29.9f*p[4];release=40*std::pow(20.f,p[5]);}
        else {const std::array<float,5> recovery{8000,4000,600,400,200};input=juce::Decibels::decibelsToGain(-12+36*p[0]);threshold=-6-36*p[2];attack=5+65*p[3];release=recovery[(size_t)juce::jlimit(0,4,juce::roundToInt(p[4]))];output=gain(p[5]);ratio=p[6]>.5f?12.f:3.f;}
        b.applyGain(input);compressor.process(b,true,threshold,ratio,attack,release,0,character,false);appliedGR=compressor.reduction();
        if(dryLevel>0)for(int c=0;c<b.getNumChannels();++c)b.addFrom(c,0,source,c,0,b.getNumSamples(),dryLevel);
        if(s.model==8) {
            const float tilt=p[3]>.5f?(p[1]-.5f)*12:0;
            equalize(b,0,C::makeLowShelf(rate,900,.707f,juce::Decibels::decibelsToGain(-tilt)));
            equalize(b,1,C::makeHighShelf(rate,900,.707f,juce::Decibels::decibelsToGain(tilt)));
            equalize(b,2,C::makeLowPass(rate,juce::jmin(float(rate*.45),p[4]>.5f?4800.f:float(rate*.45))));
        }
        volume(b,output);
    }
    void processEnvelope(juce::AudioBuffer<float>& b,const PedalInstanceState& s) {
        const auto& p=s.controls;int mode=0;bool reverse=false;float low=160,span=24,sensitivity=.5f,q=1.5f,release=.11f,dryLevel=0,fxLevel=1;
        if(s.model==11) {mode=juce::roundToInt(p[0]);reverse=p[1]>.5f;low=p[2]>.5f?120.f:400.f;q=.5f+5.5f*p[3];sensitivity=p[4];fxLevel=p[5]>.5f?2.f:1.f;}
        else if(s.model==12 || s.model==13) {sensitivity=p[0];q=.5f+5.5f*p[1];mode=juce::roundToInt(p[2]);low=p[3]>.5f?400.f:120.f;reverse=p[4]>.5f;}
        else if(s.model==14) {mode=1;dryLevel=2*p[0];fxLevel=2*p[1];low=76.f*std::pow(1300.f/76.f,p[2]);span=juce::jmax(1.1f,4200.f/low);q=.5f+5.5f*p[3];sensitivity=p[4];}
        else {mode=1;release=.025f*std::pow(24.f,p[0]);low=200.f*std::pow(6.f,p[1]);span=6;sensitivity=p[2];}
        envelopeFilter.setType(mode==1?juce::dsp::StateVariableTPTFilterType::bandpass:mode==2?juce::dsp::StateVariableTPTFilterType::highpass:juce::dsp::StateVariableTPTFilterType::lowpass);
        envelopeFilter.setResonance(q);
        const float attack=float(std::exp(-1/(rate*.006))),decay=float(std::exp(-1/(rate*release)));
        for(int n=0;n<b.getNumSamples();++n) {
            float peak=0;for(int c=0;c<b.getNumChannels();++c)peak=juce::jmax(peak,std::abs(b.getSample(c,n)));
            const float pole=peak>envelope?attack:decay;envelope=pole*envelope+(1-pole)*peak;
            if(filterClock++%8==0) {float amount=juce::jlimit(0.f,1.f,envelope*(1+50*sensitivity));if(reverse)amount=1-amount;envelopeFilter.setCutoffFrequency(juce::jmin(float(rate*.4),low*std::pow(span,amount)));}
            for(int c=0;c<b.getNumChannels();++c) {const float x=b.getSample(c,n),f=envelopeFilter.processSample(c,x);b.setSample(c,n,mode==3?.5f*x+.5f*f:dryLevel*x+fxLevel*f);}
        }
        filterClock%=8;
    }
    int processModel(juce::AudioBuffer<float>& b,const PedalInstanceState& s) {
        const auto& p=s.controls;
        if(s.model>=1 && s.model<=5) {
            FXState state;state.driveOn=true;state.models[0]=s.model-1;state.drive=p[0];state.tone=700*std::pow(18.f,s.model==3?1-p[1]:p[1]);state.driveLevel=0;
            float level=p[2];
            if(s.model==4) {state.drive=p[6];state.tone=12000;level=p[0];}
            if(s.model==5) {state.drive=p[3];level=p[2];}
            if(s.model==5)equalize(b,0,C::makeLowShelf(rate,100,.707f,p[4]>.5f?2.f:1.f));
            drive.process(b,state);
            if(s.model==4) {
                equalize(b,0,C::makeLowShelf(rate,p[8]>.5f?80.f:40.f,.707f,juce::Decibels::decibelsToGain((p[4]-.5f)*24)));
                equalize(b,1,C::makePeakFilter(rate,p[7]>.5f?1000.f:500.f,.8f,juce::Decibels::decibelsToGain((p[3]-.5f)*24)));
                equalize(b,2,C::makeHighShelf(rate,3500,.707f,juce::Decibels::decibelsToGain((p[2]-.5f)*24)));
                equalize(b,3,C::makeHighShelf(rate,6000,.707f,juce::Decibels::decibelsToGain((p[5]-.5f)*12)));
                for(int c=0;c<b.getNumChannels();++c)for(int n=0;n<b.getNumSamples();++n)b.setSample(c,n,b.getSample(c,n)*p[1]+dry.getSample(c,n)*(1-p[1]));
            } else if(s.model==5) {
                equalize(b,1,C::makePeakFilter(rate,1000,.7f,p[5]>.5f?2.f:1.f));
                volume(b,gain(level));
                for(int c=0;c<b.getNumChannels();++c)for(int n=0;n<b.getNumSamples();++n)b.setSample(c,n,b.getSample(c,n)*p[0]+dry.getSample(c,n)*(1-p[0]));
                return drive.latency();
            }
            volume(b,gain(level));return drive.latency();
        }
        if(s.model>=6 && s.model<=10) {processCompressor(b,s);return 0;}
        if(s.model>=11 && s.model<=15) {processEnvelope(b,s);return 0;}
        if(s.model>=16 && s.model<=20) {
            float amount=s.model==16?p[2]:s.model<=18?p[1]:p[3],colour=s.model==16 || s.model==19?p[1]:.5f;
            // GATE/PINCH alter envelope-dependent gating; COMP/STAB are explicit
            // original shaping experiments, not a Fuzz Factory circuit/oscillator.
            if(s.model==19 || s.model==20)for(int c=0;c<b.getNumChannels();++c)for(int n=0;n<b.getNumSamples();++n) {
                const float x=b.getSample(c,n),threshold=(s.model==19?p[2]:p[1])*.08f;
                float y=x*juce::jlimit(0.f,1.f,(std::abs(x)-threshold)*80.f);
                if(s.model==19)y=y>=0?y*(1-.8f*p[2]):y;
                else {const float bias=(1-p[4])*.3f;y=(std::tanh(y*(1+8*p[2])+bias)-std::tanh(bias))*(.3f+.7f*p[4]);}
                b.setSample(c,n,y);
            }
            fuzz.process(b,true,amount*36,colour,0,true,s.model-16);volume(b,gain(p[0]));return fuzz.latency();
        }
        if(s.model>=21 && s.model<=25) {
            const int model=s.model-21;float db=24*p[0],bass=0,treble=0;
            if(s.model==21){bass=(p[3]-.5f)*24;treble=(p[2]-.5f)*24;}
            if(s.model==24){db+=p[1]>.5f?3.f:0.f;treble=p[2]>.5f?2.5f:0.f;}
            boost.process(b,true,db,bass,treble,model);if(s.model==21)volume(b,gain(p[1]));return 0;
        }
        if(s.model>=26 && s.model<=29) {newDrive.process(b,s);return newDrive.latency();}
        if(s.model==30) {
            envelopeFilter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);envelopeFilter.setResonance(.7f+7.3f*p[2]);wahPosition.setTargetValue(p[0]);
            for(int n=0;n<b.getNumSamples();++n){const float position=wahPosition.getNextValue();if(filterClock++%8==0)envelopeFilter.setCutoffFrequency(juce::jmin(float(rate*.4),(170+450*p[1])*std::pow(7.f,position)));for(int c=0;c<b.getNumChannels();++c)b.setSample(c,n,envelopeFilter.processSample(c,b.getSample(c,n)));}filterClock%=8;return 0;
        }
        if(s.model==31) {
            b.applyGain(juce::Decibels::decibelsToGain(p[10]));
            for(int band=0;band<10;++band)equalize(b,band,C::makePeakFilter(rate,juce::jmin(float(rate*.45),31.25f*std::pow(2.f,float(band))),1.4f,juce::Decibels::decibelsToGain(p[(size_t)band])));
            volume(b,juce::Decibels::decibelsToGain(p[11]));return 0;
        }
        if(s.model>=32 && s.model<=37) {modulation.process(b,true,s.model-32,p[0],p[1],p[2]);return 0;}
        return 0;
    }
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {
        rate=spec.sampleRate;drive.prepare(spec);fuzz.prepare(spec);compressor.prepare(spec);boost.prepare(spec);modulation.prepare(spec);newDrive.prepare(spec);
        fixedLatency=juce::jmax(drive.latency(),fuzz.latency(),newDrive.latency());
        dry.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);source.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        dryDelay.prepare(spec);dryDelay.setDelay(float(fixedLatency));padding.prepare(spec);
        auto mono=spec;mono.numChannels=1;for(auto& bank:eq)for(auto& f:bank){*f.coefficients=C::makePeakFilter(rate,1000,.707f,1);f.prepare(mono);}
        envelopeFilter.prepare(spec);wet.reset(rate,.015);outputGain.reset(rate,.02);wahPosition.reset(rate,.02);wet.setCurrentAndTargetValue(0);outputGain.setCurrentAndTargetValue(1);wahPosition.setCurrentAndTargetValue(.5f);reset();
    }
    int latency()const{return fixedLatency;}
    float reduction()const{return appliedGR;}
    void reset() {drive.reset();fuzz.reset();compressor.reset();boost.reset();modulation.reset();newDrive.reset();for(auto& bank:eq)for(auto& f:bank)f.reset();envelopeFilter.reset();dryDelay.reset();padding.reset();envelope=0;lastModel=0;filterClock=0;appliedGR=0;wet.setCurrentAndTargetValue(0);}
    void process(juce::AudioBuffer<float>& b,const PedalInstanceState& state) {
        source.makeCopyOf(b,true);dry.makeCopyOf(b,true);juce::dsp::AudioBlock<float> dryBlock(dry);juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);dryDelay.process(dryContext);
        const bool active=state.model>0 && state.model<pedalModelCount && pedalModel(state.model).implemented && !state.bypass;
        if(lastModel!=state.model) {drive.reset();fuzz.reset();compressor.reset();boost.reset();modulation.reset();newDrive.reset();envelopeFilter.reset();padding.reset();for(auto& bank:eq)for(auto& f:bank)f.reset();envelope=0;filterClock=0;wet.setCurrentAndTargetValue(0);lastModel=state.model;}
        wet.setTargetValue(active?1.f:0.f);appliedGR=0;
        if(!active && !wet.isSmoothing()) {b.makeCopyOf(dry,true);return;} // Empty/bypassed: no effect DSP.
        const int native=processModel(b,state);padding.setDelay(float(juce::jmax(0,fixedLatency-native)));
        if(native<fixedLatency) {juce::dsp::AudioBlock<float> block(b);juce::dsp::ProcessContextReplacing<float> context(block);padding.process(context);}
        for(int n=0;n<b.getNumSamples();++n) {const float mix=wet.getNextValue();for(int c=0;c<b.getNumChannels();++c)b.setSample(c,n,dry.getSample(c,n)*(1-mix)+b.getSample(c,n)*mix);}
    }
};

class PedalBoardDSP {
    std::array<PedalEffectInstance,pedalBoardCapacity> instances;
    juce::AudioBuffer<float> clean;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> cleanAlignment{1024};
    int slotDelay{};float gr{};
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {for(auto& instance:instances)instance.prepare(spec);slotDelay=instances[0].latency();clean.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);cleanAlignment.prepare(spec);reset();}
    void reset(){for(auto& instance:instances)instance.reset();cleanAlignment.reset();gr=0;}
    int latency()const{return pedalBoardCapacity*slotDelay;}
    float compressorReduction()const{return gr;}
    const juce::AudioBuffer<float>& cleanOutput()const{return clean;}
    void process(juce::AudioBuffer<float>& b,PedalBoardState state) {
        sanitisePedalBoard(state);gr=0;
        if(state.lowTap==0)clean.makeCopyOf(b,true);
        for(int position=0;position<pedalBoardCapacity;++position) {
            const auto owner=(size_t)state.order[(size_t)position];instances[owner].process(b,state.instances[owner]);gr=juce::jmax(gr,instances[owner].reduction());
            if(position+1==state.lowTap)clean.makeCopyOf(b,true);
        }
        const int remainder=(pedalBoardCapacity-state.lowTap)*slotDelay;
        cleanAlignment.setDelay(float(remainder));
        // Keep the alignment history advancing even at the final tap.
        juce::dsp::AudioBlock<float> block(clean);juce::dsp::ProcessContextReplacing<float> context(block);cleanAlignment.process(context);
    }
};
} // namespace spectralforge
