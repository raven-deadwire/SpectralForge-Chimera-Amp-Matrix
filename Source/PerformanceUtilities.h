#pragma once
#include <juce_dsp/juce_dsp.h>
namespace spectralforge {
class PerformanceUtilities {
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear> doubler;
    juce::SmoothedValue<float> mix,time;
    double rate{48000},modPhase{},beatPhase{},clickPhase{};
    float clickEnvelope{},clickFrequency{1000};int beat{};bool wasMetronome{};
public:
    void prepare(const juce::dsp::ProcessSpec& s) {rate=s.sampleRate;doubler.setMaximumDelayInSamples(int(rate*.040));doubler.prepare(s);mix.reset(rate,.02);time.reset(rate,.05);mix.setCurrentAndTargetValue(0);time.setCurrentAndTargetValue(6);reset();}
    void reset() {doubler.reset();modPhase=beatPhase=clickPhase=0;clickEnvelope=0;beat=0;wasMetronome=false;}
    void process(juce::AudioBuffer<float>& b,float tempo,bool on,float spread,bool metronome,bool restart) {
        // A stereo decorrelation effect: intentional wet delay, no dry-path latency.
        mix.setTargetValue(on && b.getNumChannels()==2 ? .35f : 0.f);time.setTargetValue(spread);
        if(restart || (metronome && !wasMetronome)) {beatPhase=1;beat=0;}wasMetronome=metronome;
        for(int n=0;n<b.getNumSamples();++n) {
            const float m=mix.getNextValue(),ms=time.getNextValue();
            if(metronome) {
                beatPhase+=juce::jlimit(40.f,240.f,tempo)/(60*rate);
                if(beatPhase>=1) {beatPhase-=1;clickEnvelope=.04f;clickFrequency=beat++%4==0 ? 1400.f : 900.f;clickPhase=0;}
            }
            const float click=metronome ? clickEnvelope*float(std::sin(clickPhase)) : 0.f;
            clickEnvelope*=float(std::exp(-1/(rate*.004)));clickPhase+=juce::MathConstants<double>::twoPi*clickFrequency/rate;
            for(int c=0;c<b.getNumChannels();++c) {
                const float x=b.getSample(c,n),delayMs=ms*(c==0 ? .55f : 1.f)+.25f*float(std::sin(modPhase+c*1.7));
                const float shifted=doubler.popSample(c,float(rate*.001)*juce::jmax(.2f,delayMs));doubler.pushSample(c,x);
                b.setSample(c,n,x*(1-m)+shifted*m+click);
            }
            modPhase+=juce::MathConstants<double>::twoPi*.13/rate;if(modPhase>juce::MathConstants<double>::twoPi)modPhase-=juce::MathConstants<double>::twoPi;
        }
    }
};
}
