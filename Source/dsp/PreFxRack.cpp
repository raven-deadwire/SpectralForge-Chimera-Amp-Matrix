#include "PreFxRack.h"
namespace spectralforge {
void PreFxRack::prepare(const juce::dsp::ProcessSpec&s){sampleRate=s.sampleRate;low.prepare(s);mid.prepare(s);high.prepare(s);reset();setEq(0,0,0,false);}
void PreFxRack::reset(){low.reset();mid.reset();high.reset();}
void PreFxRack::setGate(float db,bool e){gateThreshold=juce::Decibels::decibelsToGain(db);gateOn=e;}
void PreFxRack::setBoost(float db,bool e){boostGain=juce::Decibels::decibelsToGain(db);boostOn=e;}
void PreFxRack::setEq(float l,float m,float h,bool e){eqOn=e;*low.state=*juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate,120.f,.707f,juce::Decibels::decibelsToGain(l));*mid.state=*juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,850.f,.8f,juce::Decibels::decibelsToGain(m));*high.state=*juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate,3500.f,.707f,juce::Decibels::decibelsToGain(h));}
void PreFxRack::process(juce::AudioBuffer<float>&b){if(gateOn)for(int ch=0;ch<b.getNumChannels();++ch){auto*p=b.getWritePointer(ch);for(int n=0;n<b.getNumSamples();++n)if(std::abs(p[n])<gateThreshold)p[n]=0.f;}if(boostOn)b.applyGain(boostGain);if(eqOn){juce::dsp::AudioBlock<float> bl(b);juce::dsp::ProcessContextReplacing<float>ctx(bl);low.process(ctx);mid.process(ctx);high.process(ctx);}}
}
