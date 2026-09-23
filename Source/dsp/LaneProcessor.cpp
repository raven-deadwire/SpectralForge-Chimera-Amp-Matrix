#include "LaneProcessor.h"
#include <cmath>
namespace spectralforge {
void LaneProcessor::setTone(float b,float m,float t,float p,float r){bassDb=b;midDb=m;trebleDb=t;presenceDb=p;resonanceDb=r;*bassFilter.state=*juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate,90.f,.707f,juce::Decibels::decibelsToGain(b));*midFilter.state=*juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,800.f,.75f,juce::Decibels::decibelsToGain(m));*trebleFilter.state=*juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate,3200.f,.707f,juce::Decibels::decibelsToGain(t));*presenceFilter.state=*juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate,2000.f,.8f,juce::Decibels::decibelsToGain(p));*resonanceFilter.state=*juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,100.f,1.1f,juce::Decibels::decibelsToGain(r));}
void LaneProcessor::process(juce::AudioBuffer<float>& b){
 if(muted){b.clear();return;}
 juce::dsp::AudioBlock<float> base(b);
 auto up=oversampling->processSamplesUp(base);
 const float pg=1.f+drive*14.f;
 for(size_t n=0;n<up.getNumSamples();++n)for(size_t ch=0;ch<up.getNumChannels();++ch){float x=up.getSample((int)ch,(int)n);
  if(model==AmpModel::glass){const float y=std::tanh(x*(1.f+drive*1.15f));x=.86f*x+.14f*y;}
  else if(model==AmpModel::tight515){float a=std::tanh(x*pg);float b2=std::tanh((a-.035f*a*a)*(2.1f+drive*4.5f));x=.92f*b2+.08f*a*a*a;}
  else if(model==AmpModel::ironTube){const float y=std::tanh((x+.09f*x*x)*(1.f+drive*6.5f));x=.64f*x+.36f*y;}
  else {const float y=std::tanh(x*(1.f+drive*3.0f));x=.74f*y+.26f*x;}
  up.setSample((int)ch,(int)n,x);
 }
 oversampling->processSamplesDown(base);
 juce::dsp::ProcessContextReplacing<float>ctx(base);bassFilter.process(ctx);midFilter.process(ctx);trebleFilter.process(ctx);resonanceFilter.process(ctx);presenceFilter.process(ctx);
 for(int n=0;n<b.getNumSamples();++n){const float g=level.getNextValue();for(int ch=0;ch<b.getNumChannels();++ch)b.getWritePointer(ch)[n]*=g;}
 cab.process(b);
} }
