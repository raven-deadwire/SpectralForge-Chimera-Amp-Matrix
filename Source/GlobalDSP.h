#pragma once
#include <juce_dsp/juce_dsp.h>
#include "PolyPitch.h"
#include "LifecycleTrace.h"
#include "PrecisionTuner.h"
#include <atomic>
#include <array>

namespace spectralforge {
class NoiseGate {
    double rate{48000};
    float detector{}, gain{1};
    int holdCounter{};
    bool open{true};
public:
    // 0..95.9 dB is a finite attenuation floor. 96 is the Full sentinel:
    // the original zero target, approached with the existing release curve.
    static constexpr float fullRangeDb=96.f;
    static float closedGain(float rangeDb) noexcept
    {
        if(!std::isfinite(rangeDb) || rangeDb>=fullRangeDb) return 0.f;
        return juce::Decibels::decibelsToGain(-juce::jmax(0.f,rangeDb));
    }
    void prepare(double sr) { rate=sr; reset(); }
    void reset() { detector=0; gain=1; holdCounter=0; open=true; }
    float reduction() const { return gain; }
    void process(juce::AudioBuffer<float>& buffer,bool enabled,float thresholdDb,float releaseMs,float holdMs,float rangeDb=fullRangeDb)
    { processInternal(buffer,&buffer,nullptr,enabled,thresholdDb,releaseMs,holdMs,rangeDb); }
    // The same input detector can control a later gain stage without altering
    // the clean signal or deriving its envelope from distortion-generated noise.
    void detect(const juce::AudioBuffer<float>& buffer,float* envelope,bool enabled,float thresholdDb,float releaseMs,float holdMs,float rangeDb=fullRangeDb)
    { processInternal(buffer,nullptr,envelope,enabled,thresholdDb,releaseMs,holdMs,rangeDb); }
private:
    void processInternal(const juce::AudioBuffer<float>& buffer,juce::AudioBuffer<float>* output,float* envelope,bool enabled,float thresholdDb,float releaseMs,float holdMs,float rangeDb)
    {
        const float threshold=juce::Decibels::decibelsToGain(thresholdDb);
        const float closeThreshold=threshold*.501187f; // 6 dB hysteresis
        const float envelopeRelease=float(std::exp(-1.0/(rate*.020)));
        const float attack=float(std::exp(-1.0/(rate*.0005)));
        const float release=float(std::exp(-1.0/(rate*juce::jmax(5.f,releaseMs)*.001)));
        const int holdSamples=int(rate*holdMs*.001);
        const float floor=closedGain(rangeDb);
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
            const float target=(!enabled || open) ? 1.f : floor;
            const float coefficient=target>gain ? attack : release;
            gain=target+(gain-target)*coefficient;
            if(envelope) envelope[n]=gain;
            if(output)for(int c=0;c<buffer.getNumChannels();++c) output->setSample(c,n,buffer.getSample(c,n)*gain);
        }
    }
};

class Transposer {
    PolyPitch stretch;
    juce::AudioBuffer<float> shifted, aligned;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> dryDelay;
    juce::SmoothedValue<float> wet,engage;
    bool firstBlock{true};
    int delaySamples{},warmupSamples{};
    bool wasRendering{};
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        stretch.prepare(spec);
        delaySamples=stretch.latency();
        shifted.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        aligned.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        dryDelay.setMaximumDelayInSamples(delaySamples+1); dryDelay.prepare(spec); dryDelay.setDelay(float(delaySamples));
        wet.reset(spec.sampleRate,.015); wet.setCurrentAndTargetValue(0);
        engage.reset(spec.sampleRate,.015);engage.setCurrentAndTargetValue(0);firstBlock=true;warmupSamples=0;wasRendering=false;
    }

    void reset() {stretch.reset();dryDelay.reset();wet.setCurrentAndTargetValue(0);engage.setCurrentAndTargetValue(0);firstBlock=true;warmupSamples=0;wasRendering=false;}
    int latency() const { return delaySamples; }
    uint64_t processedPitchFrames() const { return stretch.processedFrames(); }
    void process(juce::AudioBuffer<float>& buffer,bool enabled,int semitones)
    {
        // Bypass/zero semitones keeps only the delay/input history warm. A newly
        // enabled shifter fills its overlap-add window before the wet fade, so
        // enabling does not fade into an empty synthesis window.
        if(firstBlock) {engage.setCurrentAndTargetValue(enabled ? 1.f : 0.f);firstBlock=false;}
        engage.setTargetValue(enabled ? 1.f : 0.f);
        const int n=buffer.getNumSamples();
        shifted.setSize(buffer.getNumChannels(),n,false,false,true);
        aligned.makeCopyOf(buffer,true);
        juce::dsp::AudioBlock<float> dryBlock(aligned);
        juce::dsp::ProcessContextReplacing<float> context(dryBlock); dryDelay.process(context);
        const bool needsShift=enabled && semitones!=0;
        // Keep the outgoing ratio until its wet fade finishes. Bypass/zero
        // must not inject unity/new-ratio frames into the old OLA tail.
        if(needsShift) stretch.setSemitones(semitones);
        if(!needsShift) wet.setTargetValue(0.f);
        const bool render=needsShift || wet.isSmoothing() || wet.getCurrentValue()>0.f;
        if(render && !wasRendering) warmupSamples=delaySamples;
        stretch.process(buffer,shifted,render);
        wasRendering=render;
        for(int i=0;i<n;++i)
        {
            if(needsShift && warmupSamples>0) --warmupSamples;
            else if(needsShift) wet.setTargetValue(1.f);
            const float mix=wet.getNextValue(),blend=engage.getNextValue();
            for(int c=0;c<buffer.getNumChannels();++c)
                buffer.setSample(c,i,buffer.getSample(c,i)*(1.f-blend)+blend*(aligned.getSample(c,i)*(1.f-mix)+shifted.getSample(c,i)*mix));
        }
    }
};

// YIN on a decimated mono input. Analysis runs on a worker, never in processBlock.
class Tuner : private juce::Thread {
    static constexpr int frameSize=4096, queueSize=16384;
    juce::AbstractFifo fifo{queueSize};
    std::array<float,queueSize> queue{};
    std::array<float,frameSize> history{}, ordered{};
    int writeIndex{}, filled{}, decimation{2}, phase{};
    float low1{}, low2{}, coefficient{};
    int previousChannel{-1};
    double analysisRate{24000};
    std::atomic<bool> enabled{false};
    std::atomic<unsigned> generation{0}, pitchGeneration{0};
    std::atomic<float> hz{0}, certainty{0};
public:
    Tuner() : Thread("Chimera tuner") {}
    ~Tuner() override { stop(); }
    void requestStop() { lifecycle::write("tuner.stop.requested", this); signalThreadShouldExit(); notify(); }
    void stop() { const lifecycle::Scope trace("tuner.stop", this); requestStop(); stopThread(-1); enabled.store(false);hz.store(0);certainty.store(0); }
    bool stopped() const noexcept { return !isThreadRunning(); }
    void prepare(double rate)
    {
        const lifecycle::Scope trace("tuner.prepare", this);
        stop(); fifo.reset(); history.fill(0); writeIndex=filled=phase=0; low1=low2=0; previousChannel=-1;
        decimation=juce::jmax(1,juce::roundToInt(rate/24000.0)); analysisRate=rate/decimation;
        coefficient=float(1.0-std::exp(-juce::MathConstants<double>::twoPi*3000.0/rate));
        hz.store(0); certainty.store(0); startThread();
    }
    void push(const juce::AudioBuffer<float>& buffer,bool active)
    {
        if(enabled.exchange(active)!=active){generation.fetch_add(1);low1=low2=0;phase=0;previousChannel=-1;}
        if(!active || buffer.getNumChannels()==0) return;
        int channel=0;
        if(buffer.getNumChannels()>1 && buffer.getRMSLevel(1,0,buffer.getNumSamples())>buffer.getRMSLevel(0,0,buffer.getNumSamples())) channel=1;
        if(previousChannel>=0 && channel!=previousChannel){generation.fetch_add(1);low1=low2=0;phase=0;}
        previousChannel=channel;
        bool overflow=false;
        for(int n=0;n<buffer.getNumSamples();++n)
        {
            const auto sample=buffer.getSample(channel,n);
            if(!std::isfinite(sample)){low1=low2=0;generation.fetch_add(1);continue;}
            low1+=coefficient*(sample-low1); low2+=coefficient*(low1-low2);
            if(++phase<decimation) continue;
            phase=0;
            if(fifo.getFreeSpace()==0){overflow=true;continue;}
            const auto scope=fifo.write(1);
            scope.forEach([this](int i){queue[(size_t)i]=low2;});
        }
        if(overflow)generation.fetch_add(1);
    }
    float frequency() const { return enabled.load() && pitchGeneration.load()==generation.load()?hz.load():0; }
    float confidence() const { return enabled.load() && pitchGeneration.load()==generation.load()?certainty.load():0; }
    static std::pair<float,float> estimate(const float* samples,int count,double rate)
    {
        return tuning::estimate(samples,count,rate);
    }
private:
    void run() override
    {
        const lifecycle::Scope trace("tuner.worker", this);
        unsigned seen=generation.load();double lastReceived=0;
        while(!threadShouldExit())
        {
            const auto next=generation.load();
            if(next!=seen){seen=next;filled=0;hz.store(0);certainty.store(0);const auto discarded=fifo.read(fifo.getNumReady());}
            bool received=false;
            { const auto scope=fifo.read(fifo.getNumReady());
              scope.forEach([&](int i){history[(size_t)writeIndex]=queue[(size_t)i]; writeIndex=(writeIndex+1)%frameSize; filled=juce::jmin(frameSize,filled+1); received=true;});
            }
            if(received)lastReceived=juce::Time::getMillisecondCounterHiRes();
            if(!enabled.load()) { hz.store(0); certainty.store(0); filled=0; }
            else if(received && filled==frameSize)
            {
                for(int i=0;i<frameSize;++i) ordered[(size_t)i]=history[(size_t)((writeIndex+i)%frameSize)];
                const auto pitch=estimate(ordered.data(),frameSize,analysisRate);
                if(generation.load()==seen && enabled.load()){hz.store(pitch.first);certainty.store(pitch.second);pitchGeneration.store(seen);}
            }
            else if(!received && juce::Time::getMillisecondCounterHiRes()-lastReceived>250) { hz.store(0); certainty.store(0); filled=0; }
            wait(10);
        }
    }
};
}
