#include "PostFxRack.h"
namespace spectralforge {
void PostFxRack::prepare(const juce::dsp::ProcessSpec&s){chorus.prepare(s);limiter.prepare(s);dry.setSize((int)s.numChannels,(int)s.maximumBlockSize);reset();}
void PostFxRack::reset(){chorus.reset();limiter.reset();}
void PostFxRack::setChorus(float rate,float depth,float mix,bool on){chorusOn=on;chorusMix=juce::jlimit(0.f,1.f,mix);chorus.setRate(rate);chorus.setDepth(depth);chorus.setCentreDelay(7.f);chorus.setFeedback(0.f);chorus.setMix(1.f);}
void PostFxRack::setLimiter(float db,bool on){limiterOn=on;limiter.setThreshold(db);limiter.setRelease(80.f);}
void PostFxRack::process(juce::AudioBuffer<float>&b){if(chorusOn){dry.makeCopyOf(b,true);juce::dsp::AudioBlock<float>bl(b);juce::dsp::ProcessContextReplacing<float>ctx(bl);chorus.process(ctx);for(int ch=0;ch<b.getNumChannels();++ch){b.applyGain(ch,0,b.getNumSamples(),chorusMix);b.addFrom(ch,0,dry,ch,0,b.getNumSamples(),1.f-chorusMix);}}if(limiterOn){juce::dsp::AudioBlock<float>bl(b);juce::dsp::ProcessContextReplacing<float>ctx(bl);limiter.process(ctx);}}
}
