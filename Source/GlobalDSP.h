#pragma once
#include <juce_dsp/juce_dsp.h>
#include "PolyPitch.h"
#include "DynamicsAnalysis.h"
#include "LifecycleTrace.h"
#include "PrecisionTuner.h"
#include "MacTunerWakeTimer.h"
#include <atomic>
#include <array>
#if CHIMERA_TUNER_TEST_HOOKS
#include "TunerTiming.h"
#endif

namespace spectralforge {
class NoiseGate {
    double rate{48000};
    float detector{}, gain{1};
    int holdCounter{};
    bool open{true};
    bool expand{};float expansionRatio{2},attackMs{.5f};
    DynamicsAnalysis* analysis{};
public:
    // 0..95.9 dB is a finite attenuation floor. 96 is the Full sentinel:
    // the original zero target, approached with the existing release curve.
    static constexpr float fullRangeDb=96.f;
    static float closedGain(float rangeDb) noexcept
    {
        if(!std::isfinite(rangeDb) || rangeDb>=fullRangeDb) return 0.f;
        return juce::Decibels::decibelsToGain(-juce::jmax(0.f,rangeDb));
    }
    void setAnalysis(DynamicsAnalysis* value) noexcept {analysis=value;}
    void prepare(double sr) { rate=sr; reset(); }
    void reset() { detector=0; gain=1; holdCounter=0; open=true; }
    float reduction() const { return gain; }
    void setMode(bool expander,float ratio,float attack) noexcept {expand=expander;expansionRatio=juce::jlimit(1.f,20.f,ratio);attackMs=juce::jlimit(.1f,20.f,attack);}
    void process(juce::AudioBuffer<float>& buffer,bool enabled,float thresholdDb,float releaseMs,float holdMs,float rangeDb=fullRangeDb)
    { DynamicsAnalysis::Scope capture(analysis,DynamicsStage::gate,buffer);processInternal(buffer,&buffer,nullptr,enabled,thresholdDb,releaseMs,holdMs,rangeDb); }
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
    // prepare() leaves a clean FIFO/history. The first active block belongs to
    // that clean generation; later active-state transitions invalidate it.
    bool firstActivation{true};
    double analysisRate{24000};
    std::atomic<bool> enabled{false};
    std::atomic<unsigned> generation{0}, pitchGeneration{0};
    std::atomic<float> hz{0}, certainty{0};
    std::atomic<double> pitchInputTime{0};
#if CHIMERA_TUNER_TEST_HOOKS
    tuning::TimingTrace testTiming;
    bool testNormalQoS{false}; // Set only while stopped, before prepare().
    bool testSuppressTimerNotifications{false};
    std::atomic<int> testWorkerPriority{int(juce::Thread::Priority::normal)};
    std::atomic<int> testPauseMilliseconds{0};
    std::atomic<bool> testWorkerPaused{false};
    std::atomic<bool> testWakeTimerActive{false};
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
        stop(); fifo.reset(); history.fill(0); writeIndex=filled=phase=0; low1=low2=0; previousChannel=-1;firstActivation=true;
        decimation=juce::jmax(1,juce::roundToInt(rate/24000.0)); analysisRate=rate/decimation;
        coefficient=float(1.0-std::exp(-juce::MathConstants<double>::twoPi*3000.0/rate));
        hz.store(0); certainty.store(0); pitchInputTime.store(0);
#if CHIMERA_TUNER_TEST_HOOKS
        testTiming.reset();testWorkerPriority.store(int(juce::Thread::Priority::normal));
#endif
        // This is a latency-sensitive worker, below the host's realtime audio
        // thread but not a background utility (which macOS may defer).
        startThread(juce::Thread::Priority::normal);
    }
    void push(const juce::AudioBuffer<float>& buffer,bool active)
    {
        const bool wasActive=enabled.exchange(active);
        if(wasActive!=active) {
            // No history can precede the first activation after prepare(); do
            // not create a generation race that discards its fresh FIFO. Every
            // subsequent disable/re-enable still invalidates queued/history data.
            if(!(firstActivation && active))generation.fetch_add(1);
            if(active)firstActivation=false;
            low1=low2=0;phase=0;previousChannel=-1;
        }
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
    // Caller must stop/join before reading: no locks or allocation on the worker.
    const tuning::TimingTrace& timingAfterStopForTest() const { jassert(stopped()); return testTiming; }
    void useNormalQoSForTest(bool normal) { jassert(stopped());testNormalQoS=normal; }
    bool wakeTimerActiveForTest() const { return testWakeTimerActive.load(); }
    void suppressTimerNotificationsForTest(bool suppress) { jassert(stopped());testSuppressTimerNotifications=suppress; }
    int workerPriorityForTest() const { return testWorkerPriority.load(); }
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
#if JUCE_MAC
        bool interactive=false;
        MacTunerWakeTimer wakeTimer(*this
#if CHIMERA_TUNER_TEST_HOOKS
            ,!testSuppressTimerNotifications
#endif
        );
#endif
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
#if JUCE_MAC
            // Active tuning continuously drives interactive pitch feedback. Default QoS can defer this
            // timed wait far beyond 10 ms on macOS. Change only on this worker,
            // never on the audio callback, and return to normal while disabled.
            bool requested=enabled.load();
#if CHIMERA_TUNER_TEST_HOOKS
            requested=requested && !testNormalQoS;
#endif
            if(requested!=interactive) {
                setPriority(requested?juce::Thread::Priority::highest:juce::Thread::Priority::normal);
                interactive=requested;
                if(requested)wakeTimer.start();else wakeTimer.stop();
#if CHIMERA_TUNER_TEST_HOOKS
                testWorkerPriority.store(int(getPriority()));
                testWakeTimerActive.store(wakeTimer.active());
#endif
            }
#endif
            const auto next=generation.load();
            const auto now=juce::Time::getMillisecondCounterHiRes();
            if(next!=seen || (lastReceived>0 && now-lastReceived>250)) {
                // A fresh block cannot certify an old or mixed queued window.
                // After an expired input gap, discard the pending snapshot and
                // reacquire a complete window from subsequently published input.
                // prepare() already resets the FIFO, so input queued before the
                // worker's first scheduled iteration is fresh and must not be
                // discarded merely because thread startup was delayed.
                seen=next;filled=0;lastReceived=0;
                hz.store(0);certainty.store(0);
                const auto discarded=fifo.read(fifo.getNumReady());
            }
#if CHIMERA_TUNER_TEST_HOOKS
            tuning::TimingObservation timing;timing.generation=seen;timing.wake=now;
#endif
            bool received=false;
            { const auto scope=fifo.read(fifo.getNumReady());int last=-1;
              scope.forEach([&](int i){history[(size_t)writeIndex]=queue[(size_t)i]; writeIndex=(writeIndex+1)%frameSize; filled=juce::jmin(frameSize,filled+1); received=true;last=i;});
              if(last>=0)lastReceived=queueInputTimes[size_t(last)];
            }
#if CHIMERA_TUNER_TEST_HOOKS
            timing.windowComplete=lastReceived;
            timing.service=juce::Time::getMillisecondCounterHiRes();
#endif
            if(!enabled.load() || juce::Time::getMillisecondCounterHiRes()-lastReceived>250) { hz.store(0); certainty.store(0); filled=0; }
            else if(received && filled==frameSize)
            {
                for(int i=0;i<frameSize;++i) ordered[(size_t)i]=history[(size_t)((writeIndex+i)%frameSize)];
#if CHIMERA_TUNER_TEST_HOOKS
                timing.estimateBegin=juce::Time::getMillisecondCounterHiRes();
                timing.cpuBegin=tuning::workerCpuMilliseconds();
#endif
                const auto pitch=estimate(ordered.data(),frameSize,analysisRate);
#if CHIMERA_TUNER_TEST_HOOKS
                timing.cpuEnd=tuning::workerCpuMilliseconds();
                timing.estimateEnd=juce::Time::getMillisecondCounterHiRes();
                timing.hz=pitch.first;timing.confidence=pitch.second;
#endif
                // Retain the input's arrival time, never the delayed consumption
                // or computation completion:
                // readers must also expire old observations if this worker stalls.
                if(generation.load()==seen && enabled.load()){
                    hz.store(pitch.first);certainty.store(pitch.second);pitchGeneration.store(seen);pitchInputTime.store(lastReceived);
#if CHIMERA_TUNER_TEST_HOOKS
                    timing.published=true;
#endif
                }
#if CHIMERA_TUNER_TEST_HOOKS
                timing.publish=juce::Time::getMillisecondCounterHiRes();
                testTiming.append(timing);
#endif
            }
#if CHIMERA_TUNER_TEST_HOOKS
            testCompletedIterations.fetch_add(1);
#endif
#if JUCE_MAC
            // Interruptible by stop(), test pauses and the strict timer. Keep
            // the legacy wait for disabled/normal-reference/failure fallback.
            // Dispatch delivery can itself be delayed: retain a bounded wait
            // so an active source never becomes the only route to progress.
            wait(10);
#else
            wait(10);
#endif
        }
#if JUCE_MAC
        wakeTimer.stop();
#if CHIMERA_TUNER_TEST_HOOKS
        testWakeTimerActive.store(false);
#endif
#endif
    }
};
}
