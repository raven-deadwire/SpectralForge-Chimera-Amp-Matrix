#pragma once
#include <juce_dsp/juce_dsp.h>
#include "PolyPitch.h"
#include <atomic>
#include <array>

namespace spectralforge {
class NoiseGate {
    double rate{48000};
    float detector{}, gain{1};
    int holdCounter{};
    bool open{true};
public:
    void prepare(double sr) { rate=sr; reset(); }
    void reset() { detector=0; gain=1; holdCounter=0; open=true; }
    float reduction() const { return gain; }
    void process(juce::AudioBuffer<float>& buffer,bool enabled,float thresholdDb,float releaseMs,float holdMs)
    {
        const float threshold=juce::Decibels::decibelsToGain(thresholdDb);
        const float closeThreshold=threshold*.501187f; // 6 dB hysteresis
        const float envelopeRelease=float(std::exp(-1.0/(rate*.020)));
        const float attack=float(std::exp(-1.0/(rate*.0005)));
        const float release=float(std::exp(-1.0/(rate*juce::jmax(5.f,releaseMs)*.001)));
        const int holdSamples=int(rate*holdMs*.001);
        for(int n=0;n<buffer.getNumSamples();++n)
        {
            float peak=0;
            for(int c=0;c<buffer.getNumChannels();++c) peak=juce::jmax(peak,std::abs(buffer.getSample(c,n)));
            detector=juce::jmax(peak,detector*envelopeRelease);
            if(detector>=threshold) { open=true; holdCounter=holdSamples; }
            else if(detector<closeThreshold)
            {
                if(holdCounter>0) --holdCounter;
                else open=false;
            }
            const float target=(!enabled || open) ? 1.f : 0.f;
            const float coefficient=target>gain ? attack : release;
            gain=target+(gain-target)*coefficient;
            for(int c=0;c<buffer.getNumChannels();++c) buffer.setSample(c,n,buffer.getSample(c,n)*gain);
        }
    }
};

class Transposer {
    PolyPitch stretch;
    juce::AudioBuffer<float> shifted, aligned;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> dryDelay;
    juce::SmoothedValue<float> wet,engage;
    bool firstBlock{true};
    int delaySamples{};
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        stretch.prepare(spec);
        delaySamples=stretch.latency();
        shifted.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        aligned.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        dryDelay.setMaximumDelayInSamples(delaySamples+1); dryDelay.prepare(spec); dryDelay.setDelay(float(delaySamples));
        wet.reset(spec.sampleRate,.015); wet.setCurrentAndTargetValue(0);
        engage.reset(spec.sampleRate,.015);engage.setCurrentAndTargetValue(0);firstBlock=true;
    }

    void reset() {stretch.reset();dryDelay.reset();wet.setCurrentAndTargetValue(0);firstBlock=true;}
    int latency() const { return delaySamples; }
    void process(juce::AudioBuffer<float>& buffer,bool enabled,int semitones)
    {
        // Keep the history warm, including while bypassed, so enabling pitch does
        // not expose an empty FFT window. The dry zero-semitone path is exact.
        if(firstBlock) {engage.setCurrentAndTargetValue(enabled ? 1.f : 0.f);firstBlock=false;}
        engage.setTargetValue(enabled ? 1.f : 0.f);
        const int n=buffer.getNumSamples();
        shifted.setSize(buffer.getNumChannels(),n,false,false,true);
        aligned.makeCopyOf(buffer,true);
        juce::dsp::AudioBlock<float> dryBlock(aligned);
        juce::dsp::ProcessContextReplacing<float> context(dryBlock); dryDelay.process(context);
        stretch.setSemitones(semitones);
        stretch.process(buffer,shifted);
        wet.setTargetValue(enabled && semitones!=0 ? 1.f : 0.f);
        for(int i=0;i<n;++i)
        {
            const float mix=wet.getNextValue(),blend=engage.getNextValue();
            for(int c=0;c<buffer.getNumChannels();++c)
                buffer.setSample(c,i,buffer.getSample(c,i)*(1.f-blend)+blend*(aligned.getSample(c,i)*(1.f-mix)+shifted.getSample(c,i)*mix));
        }
    }
};

// YIN on a decimated mono input. Analysis runs on a worker, never in processBlock.
class Tuner : private juce::Thread {
    static constexpr int frameSize=2048, queueSize=8192;
    juce::AbstractFifo fifo{queueSize};
    std::array<float,queueSize> queue{};
    std::array<float,frameSize> history{}, ordered{};
    int writeIndex{}, filled{}, decimation{4}, phase{};
    float low1{}, low2{}, coefficient{};
    double analysisRate{12000};
    std::atomic<bool> enabled{false};
    std::atomic<float> hz{0}, certainty{0};
public:
    Tuner() : Thread("Chimera tuner") {}
    ~Tuner() override { stop(); }
    void stop() { signalThreadShouldExit(); notify(); stopThread(-1); }
    void prepare(double rate)
    {
        stop(); fifo.reset(); history.fill(0); writeIndex=filled=phase=0; low1=low2=0;
        decimation=juce::jmax(1,juce::roundToInt(rate/12000.0)); analysisRate=rate/decimation;
        coefficient=float(1.0-std::exp(-juce::MathConstants<double>::twoPi*2000.0/rate));
        hz.store(0); certainty.store(0); startThread();
    }
    void push(const juce::AudioBuffer<float>& buffer,bool active)
    {
        enabled.store(active);
        if(!active) return;
        int channel=0;
        if(buffer.getNumChannels()>1 && buffer.getRMSLevel(1,0,buffer.getNumSamples())>buffer.getRMSLevel(0,0,buffer.getNumSamples())) channel=1;
        for(int n=0;n<buffer.getNumSamples();++n)
        {
            low1+=coefficient*(buffer.getSample(channel,n)-low1); low2+=coefficient*(low1-low2);
            if(++phase<decimation) continue;
            phase=0;
            const auto scope=fifo.write(1);
            scope.forEach([this](int i){queue[(size_t)i]=low2;});
        }
    }
    float frequency() const { return hz.load(); }
    float confidence() const { return certainty.load(); }
    static std::pair<float,float> estimate(const float* samples,int count,double rate)
    {
        const int window=count/2;
        const int minimum=juce::jmax(2,int(rate/1400.0)), maximum=juce::jmin(window-1,int(rate/25.0));
        double power=0;
        for(int i=0;i<count;++i) power+=double(samples[i])*samples[i];
        if(power/count<1e-9) return {0,0};
        std::array<double,frameSize/2> difference{}, rawDifference{};
        double sum=0;
        int selected=0;
        for(int lag=1;lag<=maximum;++lag)
        {
            double delta=0;
            for(int n=0;n<window;++n) { const double x=samples[n]-samples[n+lag]; delta+=x*x; }
            rawDifference[(size_t)lag]=delta; sum+=delta; difference[(size_t)lag]=sum>0 ? delta*lag/sum : 1;
            if(lag>minimum && difference[(size_t)lag-1]<.15 && difference[(size_t)lag]>difference[(size_t)lag-1])
            { selected=lag-1; break; }
        }
        if(selected==0) return {0,0};
        const double a=rawDifference[(size_t)selected-1], b=rawDifference[(size_t)selected], c=rawDifference[(size_t)selected+1];
        const double denominator=a-2*b+c;
        const double refinement=std::abs(denominator)>1e-12 ? .5*(a-c)/denominator : 0;
        // Refine the period against interpolated samples. Parabolic YIN alone
        // biases the upper guitar register where a period spans only 8-12 samples.
        auto cost=[&](double period) {
            const int base=(int)period; const double t=period-base;
            const double w0=-.5*t+t*t-.5*t*t*t, w1=1-2.5*t*t+1.5*t*t*t;
            const double w2=.5*t+2*t*t-1.5*t*t*t, w3=-.5*t*t+.5*t*t*t;
            double error=0;
            for(int n=1;n<window-2;++n) {
                const double shifted=w0*samples[n+base-1]+w1*samples[n+base]+w2*samples[n+base+1]+w3*samples[n+base+2];
                const double delta=samples[n]-shifted; error+=delta*delta;
            }
            return error;
        };
        double left=selected+refinement-.65,right=selected+refinement+.65;
        for(int iteration=0;iteration<18;++iteration) {
            const double a=left+(right-left)*.38196601125,b=left+(right-left)*.61803398875;
            if(cost(a)<cost(b)) right=b; else left=a;
        }
        return {float(rate/((left+right)*.5)),float(1.0-difference[(size_t)selected])};
    }
private:
    void run() override
    {
        while(!threadShouldExit())
        {
            bool received=false;
            { const auto scope=fifo.read(fifo.getNumReady());
              scope.forEach([&](int i){history[(size_t)writeIndex]=queue[(size_t)i]; writeIndex=(writeIndex+1)%frameSize; filled=juce::jmin(frameSize,filled+1); received=true;});
            }
            if(!enabled.load()) { hz.store(0); certainty.store(0); filled=0; }
            else if(received && filled==frameSize)
            {
                for(int i=0;i<frameSize;++i) ordered[(size_t)i]=history[(size_t)((writeIndex+i)%frameSize)];
                const auto pitch=estimate(ordered.data(),frameSize,analysisRate);
                hz.store(pitch.first); certainty.store(pitch.second);
            }
            else if(!received) { hz.store(0); certainty.store(0); }
            wait(40);
        }
    }
};
}
