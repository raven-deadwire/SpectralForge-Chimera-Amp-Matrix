#pragma once
#include <juce_dsp/juce_dsp.h>

namespace spectralforge {
template<size_t N> struct ModelMorph {
    std::array<juce::SmoothedValue<float>,N> weights;
    void prepare(double rate) {for(size_t i=0;i<N;++i){weights[i].reset(rate,.035);weights[i].setCurrentAndTargetValue(i==0 ? 1.f : 0.f);}}
    void select(int choice) {choice=juce::jlimit(0,int(N)-1,choice);for(size_t i=0;i<N;++i)weights[i].setTargetValue(int(i)==choice ? 1.f : 0.f);}
    std::array<float,N> next() {std::array<float,N> result;for(size_t i=0;i<N;++i)result[i]=weights[i].getNextValue();return result;}
};
class ModulationModule {
    using Delay=juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear>;
    std::array<Delay,4> delays;
    std::array<std::array<float,4>,2> phaseMemory{};
    std::array<float,2> flangeFeedback{};
    ModelMorph<6> morph;juce::SmoothedValue<float> mix,speed,depth;
    double rate{48000},phase{};
public:
    void prepare(const juce::dsp::ProcessSpec& s) {rate=s.sampleRate;for(auto& d:delays){d.setMaximumDelayInSamples(int(rate*.08));d.prepare(s);}morph.prepare(rate);for(auto* v:{&mix,&speed,&depth})v->reset(rate,.03);mix.setCurrentAndTargetValue(0);speed.setCurrentAndTargetValue(.7f);depth.setCurrentAndTargetValue(.3f);reset();}
    void reset() {for(auto& d:delays)d.reset();phaseMemory={};flangeFeedback={};phase=0;mix.setCurrentAndTargetValue(0);}
    void process(juce::AudioBuffer<float>& b,bool on,int model,float hz,float amount,float wet) {
        morph.select(model);mix.setTargetValue(on ? wet : 0);speed.setTargetValue(hz);depth.setTargetValue(amount);
        for(int n=0;n<b.getNumSamples();++n) {
            const auto weights=morph.next();const float m=mix.getNextValue(),d=depth.getNextValue();phase+=speed.getNextValue()/rate;phase-=std::floor(phase);
            for(int c=0;c<b.getNumChannels();++c) {
                const float x=b.getSample(c,n),lfo=std::sin(float(juce::MathConstants<double>::twoPi*(phase+c*.25))),opposed=std::sin(float(juce::MathConstants<double>::twoPi*(phase+c*.5)));
                std::array<float,6> voices{};
                voices[0]=delays[0].popSample(c,float(rate)*(.009f+.004f*d*lfo));delays[0].pushSample(c,x);
                const float a=delays[1].popSample(c,float(rate)*(.018f+.005f*d*opposed));delays[1].pushSample(c,x);
                const float v=delays[2].popSample(c,float(rate)*(.006f+.004f*d*lfo));delays[2].pushSample(c,x);
                voices[1]=.6f*a+.4f*v;
                const float frequency=juce::jlimit(50.f,float(rate*.35),350.f*std::pow(10.f,d*(.5f+.5f*lfo))),t=std::tan(float(juce::MathConstants<double>::pi)*frequency/float(rate)),coefficient=(1-t)/(1+t);
                float filtered=x;
                for(auto& z:phaseMemory[(size_t)c]){const float y=z-coefficient*filtered;z=filtered+coefficient*y;filtered=y;}
                voices[2]=filtered;
                voices[3]=delays[3].popSample(c,float(rate)*(.002f+.0018f*d*lfo));delays[3].pushSample(c,x+.55f*d*flangeFeedback[(size_t)c]);flangeFeedback[(size_t)c]=voices[3];
                voices[4]=v;voices[5]=x*(1-d*(.5f+.5f*opposed));
                float result=0;for(size_t i=0;i<6;++i)result+=voices[i]*weights[i];
                // A vibrato reaches full-wet at the top of the shared MIX range.
                const float effective=juce::jlimit(0.f,1.f,m*(1.f+weights[4]*(1.f/.7f-1.f)));
                b.setSample(c,n,x+effective*(result-x));
            }
        }
    }
};
class EchoModule {
    using Delay=juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear>;
    std::array<Delay,3> delays;std::array<std::array<float,2>,3> dark{},dc{};
    ModelMorph<3> morph;juce::SmoothedValue<float> mix,time,feedback;
    double rate{48000},phase{};
public:
    void prepare(const juce::dsp::ProcessSpec& s) {rate=s.sampleRate;for(auto& d:delays){d.setMaximumDelayInSamples(int(rate*2.2));d.prepare(s);}morph.prepare(rate);for(auto* v:{&mix,&time,&feedback})v->reset(rate,.035);mix.setCurrentAndTargetValue(0);time.setCurrentAndTargetValue(float(rate*.25));feedback.setCurrentAndTargetValue(.25f);reset();}
    void reset() {for(auto& d:delays)d.reset();dark={};dc={};phase=0;mix.setCurrentAndTargetValue(0);}
    void process(juce::AudioBuffer<float>& b,bool on,int model,float ms,float fb,float wet) {
        morph.select(model);mix.setTargetValue(on ? wet : 0);time.setTargetValue(float(rate)*juce::jlimit(20.f,2000.f,ms)*.001f);feedback.setTargetValue(juce::jlimit(0.f,.85f,fb));
        const std::array<float,3> cutoff{18000,4200,1800};std::array<float,3> lp{};for(size_t k=0;k<3;++k)lp[k]=float(1-std::exp(-juce::MathConstants<double>::twoPi*juce::jmin(cutoff[k],float(rate*.4))/rate));
        for(int n=0;n<b.getNumSamples();++n) {
            const auto weights=morph.next();const float m=mix.getNextValue(),samples=time.getNextValue(),gain=feedback.getNextValue();phase+=.53/rate;phase-=std::floor(phase);
            for(int c=0;c<b.getNumChannels();++c){const float x=b.getSample(c,n);float sum=0;
                for(size_t k=0;k<3;++k) {const float wow=k==1 ? float(rate)*.0008f*std::sin(float(juce::MathConstants<double>::twoPi*(phase+c*.03))) : k==2 ? float(rate)*.00012f*std::sin(float(juce::MathConstants<double>::twoPi*phase*5)) : 0;
                    const float delayed=delays[k].popSample(c,samples+wow);dark[k][(size_t)c]+=lp[k]*(delayed-dark[k][(size_t)c]);
                    const float echo=k==0 ? delayed : dark[k][(size_t)c];
                    // Passive loss in the recirculation path keeps tails bounded.
                    delays[k].pushSample(c,(on ? x : 0)+echo*gain);sum+=weights[k]*echo;
                }
                b.setSample(c,n,x*(1-m)+sum*m);
            }
        }
    }
};
class SpaceModule {
    using Delay=juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear>;
    std::array<juce::dsp::Reverb,2> rooms;std::array<juce::AudioBuffer<float>,2> wet;
    Delay hallPre,spring;std::array<std::array<float,6>,2> dispersion{};std::array<float,2> springLow{};
    ModelMorph<3> morph;juce::SmoothedValue<float> mix;double rate{48000},phase{};
public:
    void prepare(const juce::dsp::ProcessSpec& s) {rate=s.sampleRate;for(size_t i=0;i<2;++i){rooms[i].prepare(s);wet[i].setSize((int)s.numChannels,(int)s.maximumBlockSize);}hallPre.setMaximumDelayInSamples(int(rate*.1));spring.setMaximumDelayInSamples(int(rate*.2));hallPre.prepare(s);spring.prepare(s);morph.prepare(rate);mix.reset(rate,.03);mix.setCurrentAndTargetValue(0);reset();}
    void reset() {for(auto& r:rooms)r.reset();hallPre.reset();spring.reset();dispersion={};springLow={};phase=0;mix.setCurrentAndTargetValue(0);}
    void process(juce::AudioBuffer<float>& b,bool on,int model,float size,float damping,float amount) {
        morph.select(model);mix.setTargetValue(on ? amount : 0);
        for(size_t i=0;i<2;++i){juce::Reverb::Parameters p;p.roomSize=i==0 ? .25f+.45f*size : .6f+.36f*size;p.damping=damping;p.wetLevel=1;p.dryLevel=0;p.width=i==0 ? .7f : 1;p.freezeMode=0;rooms[i].setParameters(p);wet[i].makeCopyOf(b,true);if(!on)wet[i].clear();}
        for(int n=0;n<b.getNumSamples();++n){phase+=.17/rate;phase-=std::floor(phase);for(int c=0;c<b.getNumChannels();++c){const float x=wet[1].getSample(c,n),delay=float(rate)*(.022f+.014f*size+.001f*std::sin(float(juce::MathConstants<double>::twoPi*(phase+c*.3))));wet[1].setSample(c,n,hallPre.popSample(c,delay));hallPre.pushSample(c,x);}}
        for(size_t i=0;i<2;++i){juce::dsp::AudioBlock<float> block(wet[i]);juce::dsp::ProcessContextReplacing<float> context(block);rooms[i].process(context);}
        const float lp=float(1-std::exp(-juce::MathConstants<double>::twoPi*(1800+5000*(1-damping))/rate));
        for(int n=0;n<b.getNumSamples();++n){const auto weights=morph.next();const float m=mix.getNextValue();for(int c=0;c<b.getNumChannels();++c){const float x=b.getSample(c,n);float s=spring.popSample(c,float(rate)*(.031f+.039f*size+c*.0061f));
            for(size_t k=0;k<6;++k){auto& z=dispersion[(size_t)c][k];const float a=.32f+.07f*float(k),y=z-a*s;z=s+a*y;s=y;}
            springLow[(size_t)c]+=lp*(s-springLow[(size_t)c]);s=springLow[(size_t)c];spring.pushSample(c,(on ? x : 0)+s*(.35f+.5f*size));
            const float sum=weights[0]*wet[0].getSample(c,n)+weights[1]*wet[1].getSample(c,n)+weights[2]*s;
            b.setSample(c,n,x*(1-m)+sum*m);
        }}
    }
};
}
