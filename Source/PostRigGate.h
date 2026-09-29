#pragma once
#include "GlobalDSP.h"

namespace spectralforge {
// Optional clean-input detector / post-rig attenuation. Call detect() before
// PRE and apply() after the rig, before Delay/Reverb. Keeping detection running
// while the option is disabled avoids a cold envelope when it is enabled.
class PostRigGate {
    NoiseGate detector;
    juce::AudioBuffer<float> envelope;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> alignment;
    int maximumDelay{},detectedSamples{};
    float lastGain{1};
public:
    void prepare(const juce::dsp::ProcessSpec& spec,int maximumLatencySamples=0)
    {
        maximumDelay=maximumLatencySamples>0 ? maximumLatencySamples : int(std::ceil(spec.sampleRate*.25))+512;
        detector.prepare(spec.sampleRate);
        envelope.setSize(1,(int)spec.maximumBlockSize);
        alignment.setMaximumDelayInSamples(maximumDelay);
        auto mono=spec;mono.numChannels=1;alignment.prepare(mono);
        reset();
    }
    void reset() {detector.reset();alignment.reset();envelope.clear();detectedSamples=0;lastGain=1;}
    void detect(const juce::AudioBuffer<float>& cleanInput,bool enabled,float thresholdDb,float releaseMs,float holdMs,int delaySamples)
    {
        jassert(cleanInput.getNumSamples()<=envelope.getNumSamples());
        jassert(delaySamples>=0 && delaySamples<=maximumDelay);
        detectedSamples=juce::jmin(cleanInput.getNumSamples(),envelope.getNumSamples());
        // The processor chunks host buffers to its prepared maximum size.
        if(detectedSamples!=cleanInput.getNumSamples()) {detectedSamples=0;return;}
        detector.detect(cleanInput,envelope.getWritePointer(0),enabled,thresholdDb,releaseMs,holdMs);
        alignment.setDelay(float(juce::jlimit(0,maximumDelay,delaySamples)));
        for(int n=0;n<detectedSamples;++n) {
            alignment.pushSample(0,envelope.getSample(0,n));
            envelope.setSample(0,n,alignment.popSample(0));
        }
    }
    void apply(juce::AudioBuffer<float>& output)
    {
        jassert(output.getNumSamples()==detectedSamples);
        if(output.getNumSamples()!=detectedSamples) return;
        for(int n=0;n<detectedSamples;++n) {
            lastGain=envelope.getSample(0,n);
            for(int c=0;c<output.getNumChannels();++c)output.setSample(c,n,output.getSample(c,n)*lastGain);
        }
    }
    // Linear gain, matching the existing NoiseGate meter convention.
    float reduction() const {return lastGain;}
};
}
