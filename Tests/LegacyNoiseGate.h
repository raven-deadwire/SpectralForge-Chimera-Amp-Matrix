// Frozen NoiseGate from main fe3656521046be79f99a62857976021c07ec398b.
// Independent oracle for exact full-close compatibility; do not modernise.
#pragma once
#include <juce_dsp/juce_dsp.h>
namespace gateBaseline {
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
    { processInternal(buffer,&buffer,nullptr,enabled,thresholdDb,releaseMs,holdMs); }
    // The same input detector can control a later gain stage without altering
    // the clean signal or deriving its envelope from distortion-generated noise.
    void detect(const juce::AudioBuffer<float>& buffer,float* envelope,bool enabled,float thresholdDb,float releaseMs,float holdMs)
    { processInternal(buffer,nullptr,envelope,enabled,thresholdDb,releaseMs,holdMs); }
private:
    void processInternal(const juce::AudioBuffer<float>& buffer,juce::AudioBuffer<float>* output,float* envelope,bool enabled,float thresholdDb,float releaseMs,float holdMs)
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
            if(envelope) envelope[n]=gain;
            if(output)for(int c=0;c<buffer.getNumChannels();++c) output->setSample(c,n,buffer.getSample(c,n)*gain);
        }
    }
};

}
