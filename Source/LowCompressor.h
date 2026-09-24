#pragma once
#include <juce_dsp/juce_dsp.h>

namespace spectralforge {
// Feed-forward RMS dynamics with a soft knee and linked stereo gain.
// A VCA-style control law, not a component model of a branded compressor.
class LowCompressor {
    juce::SmoothedValue<float> amount;
    float detector{},reductionDb{},rmsCoefficient{},attack{},release{};
public:
    void prepare(double rate) {
        rmsCoefficient=float(std::exp(-1.0/(rate*.030)));
        attack=float(std::exp(-1.0/(rate*.010)));
        release=float(std::exp(-1.0/(rate*.140)));
        amount.reset(rate,.020);amount.setCurrentAndTargetValue(0);reset();
    }
    void reset() {detector=0;reductionDb=0;amount.setCurrentAndTargetValue(amount.getTargetValue());}
    float reduction() const {return reductionDb;}
    void process(juce::AudioBuffer<float>& buffer,float control) {
        amount.setTargetValue(juce::jlimit(0.f,1.f,control));
        for(int n=0;n<buffer.getNumSamples();++n) {
            float power=0;
            for(int c=0;c<buffer.getNumChannels();++c) {const float x=buffer.getSample(c,n);power=juce::jmax(power,x*x);}
            detector=rmsCoefficient*detector+(1-rmsCoefficient)*power;
            const float knob=amount.getNextValue(),threshold=-12.f-36.f*knob,ratio=1.f+7.f*knob;
            const float over=10.f*std::log10(juce::jmax(detector,1.0e-12f))-threshold;
            constexpr float knee=6.f;
            const float soft=over<=-knee*.5f ? 0.f : over>=knee*.5f ? over : (over+knee*.5f)*(over+knee*.5f)/(2*knee);
            const float target=soft*(1.f-1.f/ratio),coefficient=target>reductionDb ? attack : release;
            reductionDb=coefficient*reductionDb+(1-coefficient)*target;
            // No automatic makeup: LEVEL is the explicit loudness-match control.
            const float gain=knob<1.e-6f ? 1.f : juce::Decibels::decibelsToGain(-reductionDb);
            for(int c=0;c<buffer.getNumChannels();++c) buffer.setSample(c,n,buffer.getSample(c,n)*gain);
        }
    }
};
}
