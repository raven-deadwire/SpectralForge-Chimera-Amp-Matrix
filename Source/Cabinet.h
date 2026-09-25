#pragma once
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace spectralforge {
// The worker builds a complete convolution engine. Audio swaps raw ownership;
// all allocation, FFT planning and destruction stay off the audio callback.
class Cab {
public:
    struct Kernel {
        juce::dsp::Convolution convolution;
        int source{};
        unsigned generation{};
        bool hasIR{true};
        Kernel(juce::AudioBuffer<float> samples, double rate, const juce::dsp::ProcessSpec& spec,
               int type, unsigned revision) : source(type), generation(revision)
        {
            convolution.loadImpulseResponse(std::move(samples), rate, juce::dsp::Convolution::Stereo::yes,
                juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
            convolution.prepare(spec); // Wait for this IR before it reaches audio.
        }
        void process(juce::AudioBuffer<float>& buffer)
        {
            if (!hasIR) return;
            juce::dsp::AudioBlock<float> block(buffer);
            juce::dsp::ProcessContextReplacing<float> context(block);
            convolution.process(context);
        }
    };
    std::atomic<int> requestedSource{1}, activeSource{-1};
    std::atomic<unsigned> activeGeneration{0};
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
    int fadeRemaining{}, fadeLength{1};
public:
    ~Cab() { clear(); }
    void clear() // Only while processing and the worker are stopped.
    {
        delete pending.exchange(nullptr); delete retired.exchange(nullptr);
        delete active; active=nullptr; delete fading; fading=nullptr;
        activeSource.store(-1); activeGeneration.store(0); fadeRemaining=0;
    }
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        clear(); sr=spec.sampleRate;
        setCuts(70,9000); hp.prepare(spec); lp.prepare(spec);
        alternate.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        dry.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        enabled.reset(sr,.020); enabled.setCurrentAndTargetValue(on ? 1.f : 0.f);
        fadeLength=juce::jmax(1,int(sr*.050));
    }
    void publish(std::unique_ptr<Kernel> kernel) { delete pending.exchange(kernel.release()); }
    void collect() { delete retired.exchange(nullptr); }
    void install(std::unique_ptr<Kernel> kernel) // prepareToPlay only
    {
        delete active; active=kernel.release();
        activeSource.store(active->source); activeGeneration.store(active->generation);
    }
    void reset()
    {
        hp.reset(); lp.reset();
        if(active) active->convolution.reset();
        if(fading) fading->convolution.reset();
    }
    void enable(bool value) { on=value; enabled.setTargetValue(on ? 1.f : 0.f); }
    void setCuts(float low, float high)
    {
        *hp.state=juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sr,juce::jlimit(10.f,float(sr*.44),low));
        *lp.state=juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(sr,juce::jlimit(100.f,float(sr*.45),high));
    }
    void process(juce::AudioBuffer<float>& buffer)
    {
        if(fadeRemaining==0 && retired.load()==nullptr)
            if(auto* next=pending.exchange(nullptr))
            {
                {
                    fading=active; active=next;
                    activeSource.store(next->source); activeGeneration.store(next->generation);
                    fadeRemaining=fadeLength;
                }
            }
        dry.makeCopyOf(buffer,true);
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        hp.process(context);
        if(fadeRemaining>0) alternate.makeCopyOf(buffer,true);
        if(active) active->process(buffer);
        if(fadeRemaining>0)
        {
            if(fading) fading->process(alternate);
            for(int n=0;n<buffer.getNumSamples() && fadeRemaining>0;++n,--fadeRemaining)
            {
                const float wet=1.f-float(fadeRemaining)/float(fadeLength);
                for(int c=0;c<buffer.getNumChannels();++c)
                    buffer.setSample(c,n,wet*buffer.getSample(c,n)+(1.f-wet)*alternate.getSample(c,n));
            }
            if(fadeRemaining==0) { retired.store(fading); fading=nullptr; }
        }
        lp.process(context);
        for(int n=0;n<buffer.getNumSamples();++n)
        {
            const float wet=enabled.getNextValue();
            for(int c=0;c<buffer.getNumChannels();++c)
                buffer.setSample(c,n,wet*buffer.getSample(c,n)+(1.f-wet)*dry.getSample(c,n));
        }
    }
};
}
