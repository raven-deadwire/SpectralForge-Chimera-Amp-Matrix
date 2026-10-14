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
    bool expand{};float expansionRatio{2},attackMs{.5f};
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
    void setMode(bool expander,float ratio,float attack) noexcept {expand=expander;expansionRatio=juce::jlimit(1.f,20.f,ratio);attackMs=juce::jlimit(.1f,20.f,attack);}
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
        const float attack=float(std::exp(-1.0/(rate*(attackMs==.5f?.0005:double(attackMs)*.001))));
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
            float target=(!enabled || open) ? 1.f : floor;
            if(enabled && !open && expand){
                const float below=juce::jmax(0.f,thresholdDb-juce::Decibels::gainToDecibels(detector,-120.f));
                target=juce::jmax(floor,juce::Decibels::decibelsToGain(-below*(expansionRatio-1.f),-120.f));
            }
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
    // Only published block ends are stamped. A FIFO snapshot always ends at
    // one of those boundaries; read its stamp before releasing the slots.
    std::array<double,queueSize> queueInputTimes{};
    std::array<float,frameSize> history{}, ordered{};
    int writeIndex{}, filled{}, decimation{2}, phase{};
    float low1{}, low2{}, coefficient{};
    int previousChannel{-1};
    double analysisRate{24000};
    std::atomic<bool> enabled{false};
    std::atomic<unsigned> generation{0}, pitchGeneration{0};
    std::atomic<float> hz{0}, certainty{0};
    std::atomic<double> pitchInputTime{0};
#if CHIMERA_TUNER_TEST_HOOKS
    std::atomic<int> testPauseMilliseconds{0};
    std::atomic<bool> testWorkerPaused{false};
    std::atomic<unsigned> testCompletedIterations{0};
#endif
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
        hz.store(0); certainty.store(0); pitchInputTime.store(0);
        // This is a latency-sensitive worker, below the host's realtime audio
        // thread but not a background utility (which macOS may defer).
        startThread(juce::Thread::Priority::normal);
    }
    void push(const juce::AudioBuffer<float>& buffer,bool active)
    {
        if(enabled.exchange(active)!=active){generation.fetch_add(1);low1=low2=0;phase=0;previousChannel=-1;}
        if(!active || buffer.getNumChannels()==0) return;
        int channel=0;
        if(buffer.getNumChannels()>1){
            const std::array<float,2> rms{buffer.getRMSLevel(0,0,buffer.getNumSamples()),buffer.getRMSLevel(1,0,buffer.getNumSamples())};
            channel=rms[1]>rms[0]?1:0;
            // Tiny stereo level changes must not reset the analysis every block.
            // Switch only when the new input is at least 3 dB stronger; a real
            // switch still invalidates the old channel's estimate immediately.
            if(previousChannel>=0 && rms[size_t(channel)]<=rms[size_t(previousChannel)]*1.41421356f)channel=previousChannel;
        }
        if(previousChannel>=0 && channel!=previousChannel){generation.fetch_add(1);low1=low2=0;phase=0;}
        previousChannel=channel;
        const int produced=(phase+buffer.getNumSamples())/decimation;
        int first{},firstCount{},second{},secondCount{},written=0;
        fifo.prepareToWrite(produced,first,firstCount,second,secondCount);
        const int available=firstCount+secondCount;
        for(int n=0;n<buffer.getNumSamples();++n)
        {
            const auto sample=buffer.getSample(channel,n);
            if(!std::isfinite(sample)){low1=low2=0;phase=0;generation.fetch_add(1);continue;}
            low1+=coefficient*(sample-low1); low2+=coefficient*(low1-low2);
            if(++phase<decimation) continue;
            phase=0;
            if(written<available){
                const int index=written<firstCount?first+written:second+written-firstCount;
                queue[size_t(index)]=low2;++written;
            }
        }
        // One SPSC publication per audio block, with bounded overflow recovery.
        if(written>0) {
            const int last=written-1;
            const int index=last<firstCount?first+last:second+last-firstCount;
            queueInputTimes[size_t(index)]=juce::Time::getMillisecondCounterHiRes();
        }
        fifo.finishedWrite(written);
        if(available<produced)generation.fetch_add(1);
    }
    float frequency() const { return readingIsFresh()?hz.load():0; }
    float confidence() const { return readingIsFresh()?certainty.load():0; }
#if CHIMERA_TUNER_TEST_HOOKS
    void pauseWorkerForTest(int milliseconds) { testPauseMilliseconds.store(milliseconds); notify(); }
    bool workerPausedForTest() const { return testWorkerPaused.load(); }
    unsigned completedWorkerIterationsForTest() const { return testCompletedIterations.load(); }
#endif
    static std::pair<float,float> estimate(const float* samples,int count,double rate)
    {
        return tuning::estimate(samples,count,rate);
    }
private:
    bool readingIsFresh() const
    {
        const auto inputTime=pitchInputTime.load();
        return enabled.load() && pitchGeneration.load()==generation.load()
            && inputTime>0 && juce::Time::getMillisecondCounterHiRes()-inputTime<=250;
    }
    void run() override
    {
        const lifecycle::Scope trace("tuner.worker", this);
        unsigned seen=generation.load();double lastReceived=0;
        while(!threadShouldExit())
        {
#if CHIMERA_TUNER_TEST_HOOKS
            if(const auto pause=testPauseMilliseconds.exchange(0);pause>0) {
                testWorkerPaused.store(true);
                const auto until=juce::Time::getMillisecondCounterHiRes()+pause;
                while(!threadShouldExit() && juce::Time::getMillisecondCounterHiRes()<until)wait(5);
                testWorkerPaused.store(false);
            }
#endif
            const auto next=generation.load();
            if(next!=seen){seen=next;filled=0;hz.store(0);certainty.store(0);const auto discarded=fifo.read(fifo.getNumReady());}
            bool received=false;
            { const auto scope=fifo.read(fifo.getNumReady());int last=-1;
              scope.forEach([&](int i){history[(size_t)writeIndex]=queue[(size_t)i]; writeIndex=(writeIndex+1)%frameSize; filled=juce::jmin(frameSize,filled+1); received=true;last=i;});
              if(last>=0)lastReceived=queueInputTimes[size_t(last)];
            }
            if(!enabled.load() || juce::Time::getMillisecondCounterHiRes()-lastReceived>250) { hz.store(0); certainty.store(0); filled=0; }
            else if(received && filled==frameSize)
            {
                for(int i=0;i<frameSize;++i) ordered[(size_t)i]=history[(size_t)((writeIndex+i)%frameSize)];
                const auto pitch=estimate(ordered.data(),frameSize,analysisRate);
                // Retain the input's arrival time, never the delayed consumption
                // or computation completion:
                // readers must also expire old observations if this worker stalls.
                if(generation.load()==seen && enabled.load()){hz.store(pitch.first);certainty.store(pitch.second);pitchGeneration.store(seen);pitchInputTime.store(lastReceived);}
            }
#if CHIMERA_TUNER_TEST_HOOKS
            testCompletedIterations.fetch_add(1);
#endif
            wait(10);
        }
    }
};
}
