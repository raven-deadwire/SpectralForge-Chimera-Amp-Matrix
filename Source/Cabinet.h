#pragma once
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <limits>
#include "LifecycleTrace.h"
#include "CabRealtimeProfile.h"
#include "ModeledCabConvolution.h"

namespace spectralforge {
// The worker builds a complete convolution engine. Audio swaps raw ownership;
// all allocation, FFT planning and destruction stay off the audio callback.
class Cab {
public:
    struct Kernel {
        std::unique_ptr<juce::dsp::Convolution> convolution;
        std::unique_ptr<ModeledCabConvolution> modeledConvolution;
        int source{};
        unsigned generation{};
        uint64_t modelKey{};
        bool hasIR{true};
        static bool supportsPreparedModel(const juce::dsp::ProcessSpec& spec) noexcept {
            return spec.numChannels>=1 && spec.numChannels<=2 && spec.maximumBlockSize>0 && spec.maximumBlockSize<=512;
        }
        Kernel(juce::AudioBuffer<float> samples, double rate, const juce::dsp::ProcessSpec& spec,
               int type, unsigned revision, uint64_t model=0,
               std::shared_ptr<const ModeledCabConvolution::Prepared> prepared={})
            : source(type), generation(revision), modelKey(model)
        {
            SF_CAB_PROFILE_SCOPE(KernelPrepare);
            // These authored responses are already at the processing rate.
            // Mono retains the native real FFT and only its non-negative bins;
            // packed independent stereo streams share one complex transform.
            if(model && supportsPreparedModel(spec)
                && samples.getNumChannels()==1 && rate==spec.sampleRate) {
                if(!prepared)prepared=std::make_shared<const ModeledCabConvolution::Prepared>(samples,int(spec.maximumBlockSize),spec.numChannels==1);
                modeledConvolution=std::make_unique<ModeledCabConvolution>(std::move(prepared));
                return;
            }
            convolution=std::make_unique<juce::dsp::Convolution>();
            convolution->loadImpulseResponse(std::move(samples), rate, juce::dsp::Convolution::Stereo::yes,
                juce::dsp::Convolution::Trim::no, model ? juce::dsp::Convolution::Normalise::no : juce::dsp::Convolution::Normalise::yes);
            convolution->prepare(spec); // Wait for this IR before it reaches audio.
        }
        int getLatency() const noexcept {return modeledConvolution ? modeledConvolution->getLatency() : convolution->getLatency();}
        void reset() noexcept
        {
            if(modeledConvolution)modeledConvolution->reset();else convolution->reset();
        }
        void process(juce::AudioBuffer<float>& buffer)
        {
            if (!hasIR) return;
            if(modeledConvolution) {modeledConvolution->process(buffer);return;}
            juce::dsp::AudioBlock<float> block(buffer);
            juce::dsp::ProcessContextReplacing<float> context(block);
            convolution->process(context);
        }
    };
    std::atomic<int> requestedSource{1}, activeSource{-1};
    std::atomic<unsigned> activeGeneration{0};
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    std::atomic<uint64_t> requestedModel{0}, activeModel{0};
    std::atomic<unsigned> rejectedModels{0};
private:
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    Filter hp, lp;
    std::atomic<Kernel*> pending{nullptr}, retired{nullptr};
    Kernel* active{};
    Kernel* fading{};
    juce::AudioBuffer<float> alternate, dry;
    juce::SmoothedValue<float> enabled;
    double sr{48000};
    bool on{true};
    std::unique_ptr<Cab> micB;
    juce::AudioBuffer<float> second, delayBuffer;
    int delayWrite{};
    bool bWasRunning{};
    bool preferSecondSwap{};
    juce::SmoothedValue<float> micGain, micBlend, micDelay;
    double cutRate{};
    float lastLowCut{}, lastHighCut{};
    float lastGainDb{std::numeric_limits<float>::quiet_NaN()};
    bool lastInvert{};
    int fadeRemaining{}, fadeLength{1};
    static void destroyKernel(Kernel* kernel)
    {
        if (kernel == nullptr) return;
        const lifecycle::Scope trace("cab.kernel.destroy", kernel);
        delete kernel; // JUCE fallback kernels also join their private loader.
    }
public:
    explicit Cab(bool child=false) { if(!child) micB=std::make_unique<Cab>(true); else requestedSource.store(0); }
    Cab* secondMic() const noexcept { return micB.get(); }
    float blend{}, gainDb{}, delayMs{};
    bool invert{};
    ~Cab() { clear(); }
    void clear() // Only while processing and the worker are stopped.
    {
        const lifecycle::Scope trace("cab.clear", this);
        destroyKernel(pending.exchange(nullptr)); destroyKernel(retired.exchange(nullptr));
        destroyKernel(active); active=nullptr; destroyKernel(fading); fading=nullptr;
        if(micB) micB->clear();
        activeSource.store(-1); activeGeneration.store(0); activeModel.store(0); fadeRemaining=0;
    }
    // Diagnostic query: only after host processing and the IR worker stop.
    bool hasResources() const noexcept
    {
        return (micB && micB->hasResources()) || pending.load() != nullptr || retired.load() != nullptr || active != nullptr || fading != nullptr;
    }
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        clear(); sr=spec.sampleRate;
        if(micB) micB->prepare(spec);
        bWasRunning=false; preferSecondSwap=false;
        second.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        delayBuffer.setSize((int)spec.numChannels,int(sr*.020)+2); delayBuffer.clear(); delayWrite=0;
        // A prepared/restored state owns its first sample. Ramping from unity
        // would briefly bypass saved attenuation or start an inverted mic with
        // positive polarity. Runtime changes retain their 20 ms smoothing.
        micGain.reset(sr,.020);
        micGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(gainDb)*(invert ? -1.f : 1.f));
        lastGainDb=gainDb;lastInvert=invert;
        micBlend.reset(sr,.020); micBlend.setCurrentAndTargetValue(blend);
        micDelay.reset(sr,.020);
        micDelay.setCurrentAndTargetValue(float(sr*.001)*juce::jlimit(0.f,20.f,delayMs));
        setCuts(70,9000); hp.prepare(spec); lp.prepare(spec);
        alternate.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        dry.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        enabled.reset(sr,.020); enabled.setCurrentAndTargetValue(on ? 1.f : 0.f);
        fadeLength=juce::jmax(1,int(sr*.050));
    }
    void publish(std::unique_ptr<Kernel> kernel) { SF_CAB_PROFILE_SCOPE(WorkerPublish); destroyKernel(pending.exchange(kernel.release())); }
    void collect() { SF_CAB_PROFILE_SCOPE(WorkerCollect); destroyKernel(retired.exchange(nullptr)); }
    void install(std::unique_ptr<Kernel> kernel) // prepareToPlay only
    {
        destroyKernel(active); active=kernel.release();
        activeSource.store(active->source); activeGeneration.store(active->generation); activeModel.store(active->modelKey);
    }
    void reset()
    {
        hp.reset(); lp.reset();
        if(micB) micB->reset();
        delayBuffer.clear(); delayWrite=0; bWasRunning=false;
        if(active) active->reset();
        if(fading) fading->reset();
    }
    void enable(bool value) { on=value; enabled.setTargetValue(on ? 1.f : 0.f); }
    void setCuts(float low, float high)
    {
        SF_CAB_PROFILE_SCOPE(Parameters);
        low=juce::jlimit(10.f,float(sr*.44),low);
        high=juce::jlimit(100.f,float(sr*.45),high);
        if(cutRate!=sr || lastLowCut!=low)
            *hp.state=juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sr,low);
        if(cutRate!=sr || lastHighCut!=high)
            *lp.state=juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(sr,high);
        cutRate=sr;lastLowCut=low;lastHighCut=high;
    }
    // Audio-thread diagnostic; never queried by the worker.
    int transitioningMicCount() const noexcept {return (fadeRemaining>0 ? 1 : 0)+(micB ? micB->transitioningMicCount() : 0);}
    void process(juce::AudioBuffer<float>& buffer,bool allowKernelSwap=true)
    {
        SF_CAB_PROFILE_BEGIN(KernelSwap,swapProfile);
        // Model responses are longer than the legacy captures. Serialize A/B
        // crossfades within each rig: six steady paths plus at most three fading
        // paths. Keep every sample of the authored response and the 50 ms fade.
        // No cross-rig owner can get stuck when routing disables another lane.
        const auto hasModel=[](const Cab& c){return c.requestedModel.load()!=0 || c.activeModel.load()!=0 || (c.fading && c.fading->modelKey!=0);};
        const bool serialiseModels=micB && (hasModel(*this) || hasModel(*micB));
        const bool pairAllowsSwap=!serialiseModels || (micB->fadeRemaining==0 && (!preferSecondSwap || micB->pending.load()==nullptr));
        if(allowKernelSwap && pairAllowsSwap && fadeRemaining==0 && retired.load()==nullptr)
            if(auto* next=pending.exchange(nullptr))
            {
                // A superseded model may have reached pending after the worker's
                // final check. Retire it here without freeing or blocking audio.
                if(next->modelKey!=requestedModel.load()) { retired.store(next); ++rejectedModels; }
                else {
                    fading=active; active=next;
                    activeSource.store(next->source); activeGeneration.store(next->generation); activeModel.store(next->modelKey);
                    fadeRemaining=fadeLength;
                    if(serialiseModels)preferSecondSwap=true;
                }
            }
        SF_CAB_PROFILE_END(swapProfile);
        // The settled enabled path is the overwhelmingly common case. Avoid
        // copying and blending a dry buffer that cannot contribute; bypass
        // automation still takes the unchanged smoothed path below.
        const bool blendDry=enabled.isSmoothing() || enabled.getCurrentValue()!=1.f
            || enabled.getTargetValue()!=1.f;
        if(blendDry)dry.makeCopyOf(buffer,true);
        // Service a muted slot's pending swap so its UI can become ready. Once
        // settled, skip its convolution; clear frozen history before waking it.
        const bool renderB=micB && (blend>0.f || micBlend.getCurrentValue()>0.f || micB->pending.load()!=nullptr || micB->fadeRemaining>0);
        if(renderB) {
            if(!bWasRunning)micB->reset();second.makeCopyOf(buffer,true);
            const auto* before=micB->active;
            micB->process(second,!serialiseModels || fadeRemaining==0);
            if(serialiseModels && before!=micB->active)preferSecondSwap=false;
        }
        bWasRunning=renderB;
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        { SF_CAB_PROFILE_SCOPE(Filters); hp.process(context); }
        if(fadeRemaining>0) alternate.makeCopyOf(buffer,true);
        if(active) { SF_CAB_PROFILE_SCOPE(ActiveConvolution); active->process(buffer); }
        if(fadeRemaining>0)
        {
            if(fading) { SF_CAB_PROFILE_SCOPE(FadingConvolution); fading->process(alternate); }
            for(int n=0;n<buffer.getNumSamples() && fadeRemaining>0;++n,--fadeRemaining)
            {
                const float wet=1.f-float(fadeRemaining)/float(fadeLength);
                for(int c=0;c<buffer.getNumChannels();++c)
                    buffer.setSample(c,n,wet*buffer.getSample(c,n)+(1.f-wet)*alternate.getSample(c,n));
            }
            if(fadeRemaining==0) { retired.store(fading); fading=nullptr; }
        }
        { SF_CAB_PROFILE_SCOPE(Filters); lp.process(context); }
        SF_CAB_PROFILE_SCOPE(MicPost);
        if(lastGainDb!=gainDb || lastInvert!=invert) {
            micGain.setTargetValue(juce::Decibels::decibelsToGain(gainDb)*(invert ? -1.f : 1.f));
            lastGainDb=gainDb;lastInvert=invert;
        }
        micBlend.setTargetValue(blend);
        micDelay.setTargetValue(float(sr*.001)*juce::jlimit(0.f,20.f,delayMs));
        const bool directMic=!micGain.isSmoothing() && !micDelay.isSmoothing()
            && micDelay.getCurrentValue()==0.f;
        if(directMic) {
            // Keep the circular history current even at zero delay, so later
            // delay automation starts from real preceding audio rather than
            // silence. Copy contiguous spans before mixing, rather than doing
            // an integer modulo and channel switch for every sample. More than
            // one wrap is possible with large host blocks at low sample rates.
            const int samples=buffer.getNumSamples(), size=delayBuffer.getNumSamples();
            for(int offset=0;offset<samples;) {
                const int count=juce::jmin(samples-offset,size-delayWrite);
                for(int c=0;c<buffer.getNumChannels();++c) {
                    juce::FloatVectorOperations::copy(delayBuffer.getWritePointer(c,delayWrite),
                        buffer.getReadPointer(c,offset),count);
                }
                offset+=count;delayWrite+=count;
                if(delayWrite==size)delayWrite=0;
            }
            // History holds the pre-gain mic, exactly as in the fractional
            // delay path. A settled non-unity gain needs only one vector pass.
            const float gain=micGain.getCurrentValue();
            if(gain!=1.f)buffer.applyGain(gain);
            if(micBlend.isSmoothing()) {
                // Preserve the existing per-sample ramp, including the exact
                // smoother progression when the second mic is not rendering.
                for(int n=0;n<samples;++n) {
                    const float mix=micBlend.getNextValue();
                    if(renderB)for(int c=0;c<buffer.getNumChannels();++c)
                        buffer.setSample(c,n,(1.f-mix)*buffer.getSample(c,n)+mix*second.getSample(c,n));
                }
            } else if(renderB) {
                const float mix=micBlend.getCurrentValue();
                for(int c=0;c<buffer.getNumChannels();++c) {
                    auto* output=buffer.getWritePointer(c);
                    juce::FloatVectorOperations::multiply(output,1.f-mix,samples);
                    juce::FloatVectorOperations::addWithMultiply(output,second.getReadPointer(c),mix,samples);
                }
            }
        } else {
            for(int n=0;n<buffer.getNumSamples();++n) {
                const float gain=micGain.getNextValue(), mix=micBlend.getNextValue(), delay=micDelay.getNextValue();
                const int size=delayBuffer.getNumSamples();
                const int whole=int(delay); const float fraction=delay-whole;
                int read=delayWrite-whole;
                if(read<0)read+=size;
                const int previous=read==0 ? size-1 : read-1;
                for(int c=0;c<buffer.getNumChannels();++c) {
                    delayBuffer.setSample(c,delayWrite,buffer.getSample(c,n));
                    const float a=gain*((1.f-fraction)*delayBuffer.getSample(c,read)+fraction*delayBuffer.getSample(c,previous));
                    buffer.setSample(c,n,renderB ? (1.f-mix)*a+mix*second.getSample(c,n) : a);
                }
                if(++delayWrite==size)delayWrite=0;
            }
        }
        if(blendDry)for(int n=0;n<buffer.getNumSamples();++n)
        {
            const float wet=enabled.getNextValue();
            for(int c=0;c<buffer.getNumChannels();++c)
                buffer.setSample(c,n,wet*buffer.getSample(c,n)+(1.f-wet)*dry.getSample(c,n));
        }
    }
};
}
