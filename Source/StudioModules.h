#pragma once
#include <juce_dsp/juce_dsp.h>
#include "SpatialModules.h"

namespace spectralforge {
class DynamicsModule {
    double rate{48000};float power{},reductionDb{},memory{},meterGain{1};
    juce::SmoothedValue<float> mix,makeup;
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {rate=spec.sampleRate;mix.reset(rate,.015);makeup.reset(rate,.02);mix.setCurrentAndTargetValue(0);makeup.setCurrentAndTargetValue(1);reset();}
    void reset() {power=reductionDb=memory=0;meterGain=1;mix.setCurrentAndTargetValue(mix.getTargetValue());}
    float reduction() const {return -juce::Decibels::gainToDecibels(juce::jmax(1e-6f,meterGain));}
    void process(juce::AudioBuffer<float>& buffer,bool enabled,float threshold,float ratio,float attackMs,float releaseMs,float outputDb,int character=0) {
        mix.setTargetValue(enabled ? 1.f : 0.f);makeup.setTargetValue(juce::Decibels::decibelsToGain(outputDb));
        const float detector=float(std::exp(-1/(rate*(character==3 ? .00005 : character==4 ? .018 : character==1 ? .00015 : character==2 ? .025 : .01))));
        const float attack=float(std::exp(-1/(rate*.001*juce::jmax(.05f,character==3 ? attackMs*.035f : character==4 ? attackMs*1.8f : character==1 ? attackMs*.08f : attackMs))));
        const float releaseSeconds=.001f*juce::jmax(10.f,releaseMs);
        const float release=float(std::exp(-1/(rate*releaseSeconds)));
        const float memoryPole=float(std::exp(-1/(rate*.4)));
        for(int n=0;n<buffer.getNumSamples();++n) {
            float maximum=0;for(int c=0;c<buffer.getNumChannels();++c) maximum=juce::jmax(maximum,buffer.getSample(c,n)*buffer.getSample(c,n));
            power=detector*power+(1-detector)*maximum;
            const float over=10*std::log10(juce::jmax(power,1e-12f))-threshold;
            const float knee=character==4 ? 18.f : character==3 ? 3.f : character==1 ? 2.f : character==2 ? 12.f : 6.f;
            const float effectiveRatio=character==4 ? 1.f+(ratio-1.f)*(.25f+.75f*juce::jlimit(0.f,1.f,(over+9)/30)) : character==3 ? ratio*1.2f : character==2 ? ratio*.65f : ratio;
            const float soft=over<-knee*.5f ? 0 : over>knee*.5f ? over : (over+knee*.5f)*(over+knee*.5f)/(2*knee),target=soft*(1-1/juce::jmax(1.f,effectiveRatio));
            memory=memoryPole*memory+(1-memoryPole)*reductionDb;
            // Program-dependent recovery is evaluated per sample, not once per host block.
            const float recovery=character==4 ? float(std::exp(-1/(rate*releaseSeconds*(1.f+.25f*memory)))) : character==2 ? float(std::exp(-1/(rate*releaseSeconds*(1.f+.12f*reductionDb)))) : release;
            const float coefficient=target>reductionDb ? attack : recovery;reductionDb=coefficient*reductionDb+(1-coefficient)*target;
            const float compressed=juce::Decibels::decibelsToGain(-reductionDb),parallel=character==3 ? .12f+.88f*compressed : compressed;
            const float wet=mix.getNextValue(),gain=1+wet*(parallel*makeup.getNextValue()-1);
            // Meter applied compression including bypass/parallel blend, excluding makeup.
            meterGain=1+wet*(parallel-1);
            for(int c=0;c<buffer.getNumChannels();++c) buffer.setSample(c,n,buffer.getSample(c,n)*gain);
        }
    }
};
class EnvelopeModule {
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float> mix;
    double rate{48000};float envelope{};int clock{};
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {rate=spec.sampleRate;filter.prepare(spec);filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);mix.reset(rate,.015);mix.setCurrentAndTargetValue(0);reset();}
    void reset() {filter.reset();envelope=0;clock=0;}
    void process(juce::AudioBuffer<float>& b,bool on,float sensitivity,float resonance,float wet,int model=0) {
        filter.setType(model==1 || model>=3 ? juce::dsp::StateVariableTPTFilterType::bandpass : juce::dsp::StateVariableTPTFilterType::lowpass);
        mix.setTargetValue(on ? wet : 0.f);filter.setResonance(juce::jlimit(.5f,3.f,resonance));
        const float attack=float(std::exp(-1/(rate*(model==4 ? .002 : model==3 ? .012 : .008)))),release=float(std::exp(-1/(rate*(model==3 ? .18 : model==4 ? .07 : .110))));
        for(int n=0;n<b.getNumSamples();++n) {
            float peak=0;for(int c=0;c<b.getNumChannels();++c)peak=juce::jmax(peak,std::abs(b.getSample(c,n)));
            const float coefficient=peak>envelope ? attack : release;envelope=coefficient*envelope+(1-coefficient)*peak;
            if(clock++%8==0) {const float control=juce::jlimit(0.f,1.f,envelope*(2+38*sensitivity));const float cutoff=model==3 ? 100.f*std::pow(24.f,control) : model==4 ? 350.f*std::pow(6.f,std::sqrt(control)) : 250.f*std::pow(16.f,model==2 ? 1.f-control : control);filter.setCutoffFrequency(juce::jmin(float(rate*.4),cutoff));}clock%=8;
            const float amount=mix.getNextValue();for(int c=0;c<b.getNumChannels();++c) {const float x=b.getSample(c,n),filtered=filter.processSample(c,x),y=model==3 ? .5f*x+.7f*filtered : model==4 ? 1.3f*filtered : filtered;b.setSample(c,n,x+amount*(y-x));}
        }
    }
};
class BoostModule {
    using F=juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,juce::dsp::IIR::Coefficients<float>>;
    F low,high;juce::AudioBuffer<float> dry;juce::SmoothedValue<float> mix,gain;double rate{48000};
public:
    void prepare(const juce::dsp::ProcessSpec& s) {rate=s.sampleRate;dry.setSize((int)s.numChannels,(int)s.maximumBlockSize);low.prepare(s);high.prepare(s);mix.reset(rate,.015);gain.reset(rate,.02);mix.setCurrentAndTargetValue(0);gain.setCurrentAndTargetValue(1);}
    void reset() {low.reset();high.reset();}
    void process(juce::AudioBuffer<float>& b,bool on,float db,float bass,float treble,int model=0) {
        dry.makeCopyOf(b,true);using C=juce::dsp::IIR::ArrayCoefficients<float>;
        *low.state=model==1 ? C::makeHighPass(rate,650.f) : C::makeLowShelf(rate,model==3 ? 280.f : model==4 ? 90.f : model==2 ? 70.f : 120.f,.707f,juce::Decibels::decibelsToGain(bass+(model==3 ? 2.7f : model==4 ? 1.2f : 0.f)));*high.state=C::makeHighShelf(rate,juce::jmin(model==3 ? 2200.0 : model==4 ? 5000.0 : model==2 ? 7000.0 : 3500.0,rate*.4),.707f,juce::Decibels::decibelsToGain(treble+(model==3 ? -2.5f : model==4 ? 1.4f : model==1 ? 3.f : model==2 ? -.6f : 0.f)));
        juce::dsp::AudioBlock<float> block(b);juce::dsp::ProcessContextReplacing<float> context(block);low.process(context);high.process(context);
        mix.setTargetValue(on ? 1.f : 0.f);gain.setTargetValue(juce::Decibels::decibelsToGain(db));
        for(int n=0;n<b.getNumSamples();++n) {const float m=mix.getNextValue(),g=gain.getNextValue();for(int c=0;c<b.getNumChannels();++c)b.setSample(c,n,dry.getSample(c,n)*(1-m)+b.getSample(c,n)*g*m);}
    }
};
// Shared oversampled saturation infrastructure. Amp voices and these modules
// deliberately do not claim component/capture equivalence to branded hardware.
class ColourModule {
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> alignment{64};
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> mix,gain,level;
    std::array<juce::SmoothedValue<float>,5> tonePoles;
    std::array<std::array<float,2>,5> dc{},tone{};
    ModelMorph<5> morph;
    double rate{48000};bool toneReady{};
public:
    void prepare(const juce::dsp::ProcessSpec& s) {
        rate=s.sampleRate;oversampling=std::make_unique<juce::dsp::Oversampling<float>>(s.numChannels,2,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);oversampling->initProcessing(s.maximumBlockSize);
        alignment.prepare(s);alignment.setDelay(float(latency()));dry.setSize((int)s.numChannels,(int)s.maximumBlockSize);
        morph.prepare(rate*4);for(auto& pole:tonePoles)pole.reset(rate*4,.02);mix.reset(rate,.015);gain.reset(rate*4,.02);level.reset(rate,.02);mix.setCurrentAndTargetValue(0);gain.setCurrentAndTargetValue(1);level.setCurrentAndTargetValue(1);reset();
    }
    int latency() const {return oversampling ? juce::roundToInt(oversampling->getLatencyInSamples()) : 0;}
    void reset() {if(oversampling)oversampling->reset();alignment.reset();dc={};tone={};toneReady=false;}
    void process(juce::AudioBuffer<float>& b,bool on,float driveDb,float colour,float outputDb,bool fuzz=false,int model=0) {
        morph.select(juce::jlimit(0,fuzz ? 4 : 2,model));
        dry.makeCopyOf(b,true);juce::dsp::AudioBlock<float> d(dry);juce::dsp::ProcessContextReplacing<float> dcx(d);alignment.process(dcx);
        juce::dsp::AudioBlock<float> block(b);auto up=oversampling->processSamplesUp(block);
        gain.setTargetValue(juce::Decibels::decibelsToGain(driveDb));
        const float hp=float(1-std::exp(-juce::MathConstants<double>::twoPi*8/(rate*4)));
        for(size_t k=0;k<5;++k){const float cutoff=fuzz ? 900+colour*(k==0 ? 11000 : k==1 ? 6500 : k==3 ? 4200 : k==4 ? 17000 : 14500) : k==0 ? 900+colour*11000 : k==1 ? 11000+colour*7000 : 6500+colour*10000;const float pole=float(1-std::exp(-juce::MathConstants<double>::twoPi*cutoff/(rate*4)));if(toneReady)tonePoles[k].setTargetValue(pole);else tonePoles[k].setCurrentAndTargetValue(pole);}
        toneReady=true;
        // Advance one shared pole ramp per oversampled frame, never once per
        // channel or host block. Live tone automation must not step the filter.
        for(size_t n=0;n<up.getNumSamples();++n) {const float g=gain.getNextValue();const auto weights=morph.next();std::array<float,5> poles;for(size_t k=0;k<5;++k)poles[k]=tonePoles[k].getNextValue();for(size_t c=0;c<up.getNumChannels();++c) {
            const float x=up.getSample((int)c,(int)n)*g;float sum=0;
            for(size_t k=0;k<(fuzz ? 5u : 3u);++k){float y=0;
                if(fuzz){if(k==0)y=std::tanh(3.f*juce::jlimit(-.8f,1.f,x+.12f))-.345214f;
                    else if(k==1)y=.85f*(std::tanh(1.9f*x+.25f)-std::tanh(.25f));
                    else if(k==2) {const float gate=juce::jlimit(0.f,1.f,(std::abs(x)-.012f)*45.f);y=std::tanh(4.f*(x+.09f))*gate-std::tanh(.36f)*gate;}
                    else if(k==3) {const float gate=juce::jlimit(0.f,1.f,(std::abs(x)-.02f)*30.f);y=.9f*std::tanh(2.6f*x)*gate+.15f*x/(1+std::abs(x));}
                    else {const float gate=juce::jlimit(0.f,1.f,(std::abs(x)-.065f)*22.f);y=gate*(std::tanh(7*x+.45f)-std::tanh(.45f));}}
                else {if(k==0)y=std::tanh(x+.025f)-std::tanh(.025f);
                    else if(k==1)y=2.5f*std::tanh(x/2.5f);
                    else y=1.45f*(std::tanh(x/1.45f+.012f)-std::tanh(.012f));}
                dc[k][c]+=hp*(y-dc[k][c]);y-=dc[k][c];
                tone[k][c]+=poles[k]*(y-tone[k][c]);sum+=weights[k]*tone[k][c];
            }up.setSample((int)c,(int)n,sum);
        }}
        oversampling->processSamplesDown(block);mix.setTargetValue(on ? 1.f : 0.f);level.setTargetValue(juce::Decibels::decibelsToGain(outputDb));
        for(int n=0;n<b.getNumSamples();++n) {const float m=mix.getNextValue(),g=level.getNextValue();for(int c=0;c<b.getNumChannels();++c)b.setSample(c,n,dry.getSample(c,n)*(1-m)+b.getSample(c,n)*m*g);}
    }
};
class ConsoleEQ {
    using F=juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,juce::dsp::IIR::Coefficients<float>>;
    F low,mid,high;juce::AudioBuffer<float> dry;juce::SmoothedValue<float> mix;double rate{48000};
public:
    void prepare(const juce::dsp::ProcessSpec& s) {rate=s.sampleRate;dry.setSize((int)s.numChannels,(int)s.maximumBlockSize);low.prepare(s);mid.prepare(s);high.prepare(s);mix.reset(rate,.02);mix.setCurrentAndTargetValue(0);}
    void reset() {low.reset();mid.reset();high.reset();}
    void process(juce::AudioBuffer<float>& b,bool on,float lowDb,float midHz,float midDb,float q,float highDb,int model=0) {
        dry.makeCopyOf(b,true);using C=juce::dsp::IIR::ArrayCoefficients<float>;
        *low.state=C::makeLowShelf(rate,model==1 ? 110.f : model==2 ? 60.f : 80.f,.707f,juce::Decibels::decibelsToGain(lowDb));*mid.state=C::makePeakFilter(rate,juce::jmin(float(rate*.4),midHz),model==2 ? q*.55f : q,juce::Decibels::decibelsToGain(midDb));*high.state=C::makeHighShelf(rate,juce::jmin(model==1 ? 12000.0 : model==2 ? 10000.0 : 8000.0,rate*.4),.707f,juce::Decibels::decibelsToGain(highDb));
        juce::dsp::AudioBlock<float> block(b);juce::dsp::ProcessContextReplacing<float> context(block);low.process(context);mid.process(context);high.process(context);mix.setTargetValue(on ? 1.f : 0.f);
        for(int n=0;n<b.getNumSamples();++n) {const float m=mix.getNextValue();for(int c=0;c<b.getNumChannels();++c)b.setSample(c,n,dry.getSample(c,n)*(1-m)+b.getSample(c,n)*m);}
    }
};
}
