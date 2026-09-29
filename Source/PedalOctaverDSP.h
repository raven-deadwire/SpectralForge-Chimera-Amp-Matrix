#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace spectralforge {
// Original tracking divider, inspired by the /2 and /4 operating principle of
// analog monophonic octave pedals. This is NOT a circuit or component model.
// No transport/lookahead delay; attack/tracking/filter settling is not zero.
class PedalMonoOctaver {
    struct Channel {float tracker1{},tracker2{},envelope{},sub1{},sub2{},dc1{},dc2{};unsigned cycle{};bool armed{};};
    std::array<Channel,2> channels{};
    std::array<juce::SmoothedValue<float>,3> levels;
    float trackingPole{},subPole{},dcPole{},attack{},release{};
    bool controlsReady{};
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {
        const double r=spec.sampleRate;
        trackingPole=float(1-std::exp(-juce::MathConstants<double>::twoPi*650/r));
        subPole=float(1-std::exp(-juce::MathConstants<double>::twoPi*750/r));
        dcPole=float(1-std::exp(-juce::MathConstants<double>::twoPi*5/r));
        attack=float(std::exp(-1/(r*.002)));release=float(std::exp(-1/(r*.065)));
        for(auto& v:levels)v.reset(r,.015);reset();
    }
    void reset(){channels={};controlsReady=false;}
    int latency()const{return 0;}
    void process(juce::AudioBuffer<float>& b,float dry,float down1,float down2) {
        const std::array<float,3> gains{dry,down1,down2};
        for(size_t k=0;k<gains.size();++k) {
            const float g=juce::jlimit(0.f,1.f,gains[k]);
            if(controlsReady)levels[k].setTargetValue(g);else levels[k].setCurrentAndTargetValue(g);
        }
        controlsReady=true;
        for(int n=0;n<b.getNumSamples();++n) {
            const float gd=levels[0].getNextValue(),g1=levels[1].getNextValue(),g2=levels[2].getNextValue();
            for(int c=0;c<juce::jmin(2,b.getNumChannels());++c) {
                auto& s=channels[(size_t)c];const float x=b.getSample(c,n);
                const float magnitude=std::abs(x),pole=magnitude>s.envelope?attack:release;
                s.envelope=pole*s.envelope+(1-pole)*magnitude;
                s.tracker1+=trackingPole*(x-s.tracker1);s.tracker2+=trackingPole*(s.tracker1-s.tracker2);
                const float threshold=juce::jmax(1.0e-5f,s.envelope*.035f);
                if(s.tracker2 < -threshold)s.armed=true;
                if(s.armed && s.tracker2 > threshold){s.cycle=(s.cycle+1U)&3U;s.armed=false;}
                const float amplitude=s.envelope;
                const float first=(s.cycle&1U)?amplitude:-amplitude;
                const float second=(s.cycle&2U)?amplitude:-amplitude;
                s.sub1+=subPole*(first-s.sub1);s.sub2+=subPole*(second-s.sub2);
                s.dc1+=dcPole*(s.sub1-s.dc1);s.dc2+=dcPole*(s.sub2-s.dc2);
                b.setSample(c,n,gd*x+g1*(s.sub1-s.dc1)+g2*(s.sub2-s.dc2));
            }
        }
    }
    void process(juce::AudioBuffer<float>& b,const std::array<float,3>& p){process(b,p[0],p[1],p[2]);}
};

// Original STFT spectral octave engine, separate from the global transpose.
// Independent -12/+12 voices share analysis but retain separate synthesis phase.
// N-sample, explicitly reported transport latency and equally delayed dry path.
// N=2048 @ 44.1/48 kHz; N=4096 @ 96 kHz (42.7-46.4 ms).
// Close low-register chord tones can merge/detune, especially the down voice;
// this experimental algorithm does NOT claim Micro POG hardware equivalence.
// latency() describes frame timestamp delay. Wet transients are spectrally
// smeared; only the dry path promises a sample-exact delayed impulse.
class PedalPolyOctaver {
    struct Channel {
        std::vector<float> input,dry,downOutput,upOutput,analysis,downFFT,upFFT;
        std::vector<float> previousPhase,magnitude,frequency;
        std::array<std::vector<float>,2> phase,sumMagnitude,sumFrequency;
    };
    std::array<Channel,2> channels;
    std::array<juce::SmoothedValue<float>,3> levels;
    std::vector<float> window;
    std::unique_ptr<juce::dsp::FFT> fft;
    int size{2048},hop{512},inputPos{},outputPos{},hopClock{},preparedChannels{2};
    bool controlsReady{};
    static float wrap(float p){return p-juce::MathConstants<float>::twoPi*std::floor((p+juce::MathConstants<float>::pi)/juce::MathConstants<float>::twoPi);}
    void synthesize(Channel& s,int voice,float ratio,std::vector<float>& data,std::vector<float>& output) {
        auto& amp=s.sumMagnitude[(size_t)voice];auto& freq=s.sumFrequency[(size_t)voice];auto& phase=s.phase[(size_t)voice];
        std::fill(amp.begin(),amp.end(),0.f);std::fill(freq.begin(),freq.end(),0.f);std::fill(data.begin(),data.end(),0.f);
        const int bins=size/2;
        for(int k=1;k<bins;++k) {
            const int target=juce::roundToInt(float(k)*ratio);
            if(target<=0 || target>=bins)continue;
            amp[(size_t)target]+=s.magnitude[(size_t)k];
            if(s.magnitude[(size_t)k]>data[(size_t)target]){data[(size_t)target]=s.magnitude[(size_t)k];freq[(size_t)target]=s.frequency[(size_t)k]*ratio;}
        }
        std::fill(data.begin(),data.end(),0.f);
        for(int k=1;k<bins;++k) {
            const auto i=(size_t)k;
            const float omega=amp[i]>1.0e-12f?freq[i]:juce::MathConstants<float>::twoPi*float(k)/float(size);
            phase[i]=wrap(phase[i]+omega*float(hop));
            data[2*i]=amp[i]*std::cos(phase[i]);data[2*i+1]=amp[i]*std::sin(phase[i]);
        }
        fft->performRealOnlyInverseTransform(data.data());
        // Periodic Hann squared, 4x overlap: sum(window^2) = 3/2.
        for(int n=0;n<size;++n)output[(size_t)((outputPos+1+n)%size)]+=data[(size_t)n]*window[(size_t)n]*(2.f/3.f);
    }
    void frame(Channel& s) {
        std::fill(s.analysis.begin(),s.analysis.end(),0.f);
        for(int n=0;n<size;++n)s.analysis[(size_t)n]=s.input[(size_t)((inputPos+n)%size)]*window[(size_t)n];
        fft->performRealOnlyForwardTransform(s.analysis.data());
        for(int k=1;k<size/2;++k) {
            const auto i=(size_t)k;const float real=s.analysis[2*i],imaginary=s.analysis[2*i+1];
            const float p=std::atan2(imaginary,real),omega=juce::MathConstants<float>::twoPi*float(k)/float(size);
            const float delta=wrap(p-s.previousPhase[i]-omega*float(hop));
            s.previousPhase[i]=p;s.frequency[i]=omega+delta/float(hop);s.magnitude[i]=std::hypot(real,imaginary);
        }
        synthesize(s,0,.5f,s.downFFT,s.downOutput);synthesize(s,1,2.f,s.upFFT,s.upOutput);
    }
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {
        // Constant approximate time resolution across the supported rates.
        const int order=spec.sampleRate>64000?12:11;
        size=1<<order;hop=size/4;preparedChannels=juce::jlimit(1,2,int(spec.numChannels));fft=std::make_unique<juce::dsp::FFT>(order);
        window.resize((size_t)size);
        for(int n=0;n<size;++n)window[(size_t)n]=.5f-.5f*std::cos(juce::MathConstants<float>::twoPi*float(n)/float(size));
        for(auto& s:channels) {
            for(auto* v:{&s.input,&s.dry,&s.downOutput,&s.upOutput})v->resize((size_t)size);
            for(auto* v:{&s.analysis,&s.downFFT,&s.upFFT})v->resize((size_t)(2*size));
            for(auto* v:{&s.previousPhase,&s.magnitude,&s.frequency})v->resize((size_t)(size/2+1));
            for(auto* voices:{&s.phase,&s.sumMagnitude,&s.sumFrequency})for(auto& v:*voices)v.resize((size_t)(size/2+1));
        }
        for(auto& v:levels)v.reset(spec.sampleRate,.015);reset();
    }
    void reset() {
        inputPos=outputPos=hopClock=0;controlsReady=false;
        for(auto& s:channels) {
            for(auto* v:{&s.input,&s.dry,&s.downOutput,&s.upOutput,&s.analysis,&s.downFFT,&s.upFFT,&s.previousPhase,&s.magnitude,&s.frequency})std::fill(v->begin(),v->end(),0.f);
            for(auto* voices:{&s.phase,&s.sumMagnitude,&s.sumFrequency})for(auto& v:*voices)std::fill(v.begin(),v.end(),0.f);
        }
    }
    int latency()const{return size;}
    void process(juce::AudioBuffer<float>& b,float dry,float down,float up) {
        if(!fft)return;
        const std::array<float,3> gains{dry,down,up};
        for(size_t k=0;k<gains.size();++k) {
            const float g=juce::jlimit(0.f,1.f,gains[k]);
            if(controlsReady)levels[k].setTargetValue(g);else levels[k].setCurrentAndTargetValue(g);
        }
        controlsReady=true;
        const int count=juce::jmin(preparedChannels,b.getNumChannels());
        for(int n=0;n<b.getNumSamples();++n) {
            const float gd=levels[0].getNextValue(),g1=levels[1].getNextValue(),g2=levels[2].getNextValue();
            for(int c=0;c<count;++c) {
                auto& s=channels[(size_t)c];const float x=b.getSample(c,n),delayed=s.dry[(size_t)inputPos];
                s.input[(size_t)inputPos]=x;s.dry[(size_t)inputPos]=x;
                b.setSample(c,n,gd*delayed+g1*s.downOutput[(size_t)outputPos]+g2*s.upOutput[(size_t)outputPos]);
                s.downOutput[(size_t)outputPos]=s.upOutput[(size_t)outputPos]=0;
            }
            inputPos=(inputPos+1)%size;
            if(++hopClock==hop){hopClock=0;for(int c=0;c<count;++c)frame(channels[(size_t)c]);}
            outputPos=(outputPos+1)%size;
        }
    }
    void process(juce::AudioBuffer<float>& b,const std::array<float,3>& p){process(b,p[0],p[1],p[2]);}
};
} // namespace spectralforge
