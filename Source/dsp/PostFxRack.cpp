#include "PostFxRack.h"
namespace spectralforge {
void PostFxRack::prepare(const juce::dsp::ProcessSpec&s){sampleRate=s.sampleRate;chorus.prepare(s);delay.prepare(s);reverb.prepare(s);limiter.prepare(s);dry.setSize((int)s.numChannels,(int)s.maximumBlockSize);reset();}
void PostFxRack::reset(){chorus.reset();delay.reset();reverb.reset();limiter.reset();}
void PostFxRack::setChorus(float rate,float depth,float mix,bool on){chorusOn=on;chorusMix=juce::jlimit(0.f,1.f,mix);chorus.setRate(rate);chorus.setDepth(depth);chorus.setCentreDelay(7.f);chorus.setFeedback(0.f);chorus.setMix(1.f);}
void PostFxRack::setDelay(float ms,float feedback,float mix,bool on){delayOn=on;delayFeedback=juce::jlimit(0.f,.92f,feedback);delayMix=juce::jlimit(0.f,1.f,mix);delay.setDelay((float)(sampleRate*juce::jlimit(1.f,1000.f,ms)/1000.0));}
void PostFxRack::setReverb(float size,float damping,float mix,bool on){reverbOn=on;juce::dsp::Reverb::Parameters p;p.roomSize=juce::jlimit(0.f,1.f,size);p.damping=juce::jlimit(0.f,1.f,damping);p.wetLevel=juce::jlimit(0.f,1.f,mix);p.dryLevel=1.f-p.wetLevel;p.width=1.f;p.freezeMode=0.f;reverb.setParameters(p);}
void PostFxRack::setLimiter(float db,bool on){limiterOn=on;limiter.setThreshold(db);limiter.setRelease(80.f);}
void PostFxRack::process(juce::AudioBuffer<float>&b){if(chorusOn){for(int ch=0;ch<b.getNumChannels();++ch)dry.copyFrom(ch,0,b,ch,0,b.getNumSamples());juce::dsp::AudioBlock<float>bl(b);juce::dsp::ProcessContextReplacing<float>ctx(bl);chorus.process(ctx);for(int ch=0;ch<b.getNumChannels();++ch){b.applyGain(ch,0,b.getNumSamples(),chorusMix);b.addFrom(ch,0,dry,ch,0,b.getNumSamples(),1.f-chorusMix);}}
 if(delayOn)for(int n=0;n<b.getNumSamples();++n)for(int ch=0;ch<b.getNumChannels();++ch){const float in=b.getSample(ch,n);const float wet=delay.popSample(ch);delay.pushSample(ch,in+wet*delayFeedback);b.setSample(ch,n,in*(1.f-delayMix)+wet*delayMix);}
 if(reverbOn){juce::dsp::AudioBlock<float>bl(b);juce::dsp::ProcessContextReplacing<float>ctx(bl);reverb.process(ctx);}if(limiterOn){juce::dsp::AudioBlock<float>bl(b);juce::dsp::ProcessContextReplacing<float>ctx(bl);limiter.process(ctx);}}
}
