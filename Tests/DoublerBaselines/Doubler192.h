// Frozen source: b46dab01548b8c86a9801c4c2ff643877bd742d6 Source/PerformanceUtilities.h
// Only the namespace differs; test-only baseline.
#pragma once
#include <juce_dsp/juce_dsp.h>
namespace Doubler192 {
class PerformanceUtilities {
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear> doubler;
    juce::SmoothedValue<float> mix,time;
    // The delayed copy carries only the band above this crossover, so Drop A
    // fundamentals and palm-mute body stay dry and centred, without comb notches.
    static constexpr double doublerCrossoverHz=200;
    struct Biquad {double b0{1},b1{},b2{},a1{},a2{};};Biquad highPass;std::array<std::array<double,2>,2> hpState{};
    double rate{48000},modPhase{},beatPhase{},clickPhase{};
    float clickEnvelope{},clickFrequency{1000};int beat{};bool wasMetronome{};
public:
    void prepare(const juce::dsp::ProcessSpec& s) {rate=s.sampleRate;
        {   // 2nd-order Butterworth high-pass (RBJ).
            const double w=juce::MathConstants<double>::twoPi*doublerCrossoverHz/rate,alpha=std::sin(w)*juce::MathConstants<double>::sqrt2/2,c=std::cos(w),a0=1+alpha;
            highPass={(1+c)/2/a0,-(1+c)/a0,(1+c)/2/a0,-2*c/a0,(1-alpha)/a0};
        }doubler.setMaximumDelayInSamples(int(rate*.040));doubler.prepare(s);mix.reset(rate,.02);time.reset(rate,.05);mix.setCurrentAndTargetValue(0);time.setCurrentAndTargetValue(6);reset();}
    void reset() {doubler.reset();hpState={};modPhase=beatPhase=clickPhase=0;clickEnvelope=0;beat=0;wasMetronome=false;}
    void process(juce::AudioBuffer<float>& b,float tempo,bool on,float spread,bool metronome,bool restart) {
        // A stereo decorrelation effect: intentional wet delay, no dry-path latency.
        // Above the crossover each channel is (1-m) dry + m delayed, scaled to
        // keep its power (g); below it the signal passes unchanged. With
        // h = high band of x: out = x + h*(g*(1-m)-1) + g*m*delayed(h). Off
        // (m = 0, g = 1) leaves x bit-identical.
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
                auto& z=hpState[(size_t)juce::jmin(c,1)];
                const double hd=highPass.b0*x+z[0];z[0]=highPass.b1*x-highPass.a1*hd+z[1];z[1]=highPass.b2*x-highPass.a2*hd;
                const float h=float(hd),g=1.f/std::sqrt((1-m)*(1-m)+m*m);
                const float shifted=doubler.popSample(c,float(rate*.001)*juce::jmax(.2f,delayMs));doubler.pushSample(c,h);
                b.setSample(c,n,x+h*(g*(1-m)-1)+g*m*shifted+click);
            }
            modPhase+=juce::MathConstants<double>::twoPi*.13/rate;if(modPhase>juce::MathConstants<double>::twoPi)modPhase-=juce::MathConstants<double>::twoPi;
        }
    }
};
}
