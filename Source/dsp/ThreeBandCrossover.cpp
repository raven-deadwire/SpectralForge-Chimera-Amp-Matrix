#include "ThreeBandCrossover.h"
namespace spectralforge {
void ThreeBandCrossover::prepare(const juce::dsp::ProcessSpec&s){for(auto*f:{&lowLP,&midHP,&midLP,&highHP})f->prepare(s);setFrequencies(x1,x2);}
void ThreeBandCrossover::reset(){for(auto*f:{&lowLP,&midHP,&midLP,&highHP})f->reset();}
void ThreeBandCrossover::setFrequencies(float a,float b){x1=juce::jlimit(60.f,800.f,a);x2=juce::jlimit(x1+100.f,6000.f,b);
 lowLP.setType(F::Type::lowpass);midHP.setType(F::Type::highpass);midLP.setType(F::Type::lowpass);highHP.setType(F::Type::highpass);
 lowLP.setCutoffFrequency(x1);midHP.setCutoffFrequency(x1);midLP.setCutoffFrequency(x2);highHP.setCutoffFrequency(x2);}
void ThreeBandCrossover::split(const juce::AudioBuffer<float>&in,juce::AudioBuffer<float>&lo,juce::AudioBuffer<float>&mi,juce::AudioBuffer<float>&hi){
 lo.makeCopyOf(in,true);mi.makeCopyOf(in,true);hi.makeCopyOf(in,true);
 juce::dsp::AudioBlock<float> lb(lo),mb(mi),hb(hi);
 lowLP.process(juce::dsp::ProcessContextReplacing<float>(lb));
 midHP.process(juce::dsp::ProcessContextReplacing<float>(mb));midLP.process(juce::dsp::ProcessContextReplacing<float>(mb));
 highHP.process(juce::dsp::ProcessContextReplacing<float>(hb));
} }
