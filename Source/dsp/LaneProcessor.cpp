#include "LaneProcessor.h"
#include <cmath>
namespace spectralforge {
void LaneProcessor::setTone(float b,float m,float t,float p,float r){bassDb=b;midDb=m;trebleDb=t;presenceDb=p;resonanceDb=r;*bassFilter.state=*juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate,90.f,.707f,juce::Decibels::decibelsToGain(b));*midFilter.state=*juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,800.f,.75f,juce::Decibels::decibelsToGain(m));*trebleFilter.state=*juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate,3200.f,.707f,juce::Decibels::decibelsToGain(t));*presenceFilter.state=*juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate,2000.f,.8f,juce::Decibels::decibelsToGain(p));*resonanceFilter.state=*juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,100.f,1.1f,juce::Decibels::decibelsToGain(r));}
void LaneProcessor::process(juce::AudioBuffer<float>& b){
 if(muted){b.clear();return;} const float pg=1.f+drive*14.f;
 for(int n=0;n<b.getNumSamples();++n){ float g=level.getNextValue();
  for(int c=0;c<b.getNumChannels();++c){ auto* p=b.getWritePointer(c); float x=p[n];
   if(model==AmpModel::glass){const float y=std::tanh(x*(1.f+drive*1.2f));x=.82f*x+.18f*y;}
   else if(model==AmpModel::tight515){x=std::tanh(x*pg);x=.9f*x+.1f*x*x*x;}
   else if(model==AmpModel::ironTube){const float y=std::tanh((x+.08f*x*x)*(1.f+drive*7.f));x=.68f*x+.32f*y;}
   else {const float y=std::tanh(x*(1.f+drive*3.5f));x=.78f*y+.22f*x;}
   p[n]=x*g;
  }
 }
 juce::dsp::AudioBlock<float> bl(b);juce::dsp::ProcessContextReplacing<float>ctx(bl);bassFilter.process(ctx);midFilter.process(ctx);trebleFilter.process(ctx);resonanceFilter.process(ctx);presenceFilter.process(ctx);cab.process(b);
} }
