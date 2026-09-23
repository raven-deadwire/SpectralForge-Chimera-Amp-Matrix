#include "ThreeBandCrossover.h"
namespace spectralforge {
void ThreeBandCrossover::prepare(const juce::dsp::ProcessSpec&s){split1.prepare(s);split2.prepare(s);lowPhase.prepare(s);lowPhase.setType(juce::dsp::LinkwitzRileyFilterType::allpass);setFrequencies(x1,x2);}
void ThreeBandCrossover::reset(){split1.reset();split2.reset();lowPhase.reset();}
void ThreeBandCrossover::setFrequencies(float a,float b){x1=juce::jlimit(60.f,800.f,a);x2=juce::jlimit(x1+100.f,6000.f,b);split1.setCutoffFrequency(x1);split2.setCutoffFrequency(x2);lowPhase.setCutoffFrequency(x2);}
void ThreeBandCrossover::split(const juce::AudioBuffer<float>&in,juce::AudioBuffer<float>&lo,juce::AudioBuffer<float>&mi,juce::AudioBuffer<float>&hi){
 const int nc=in.getNumChannels(),ns=in.getNumSamples();lo.setSize(nc,ns,false,false,true);mi.setSize(nc,ns,false,false,true);hi.setSize(nc,ns,false,false,true);
 for(int ch=0;ch<nc;++ch){auto*lp=lo.getWritePointer(ch);auto*mp=mi.getWritePointer(ch);auto*hp=hi.getWritePointer(ch);auto*src=in.getReadPointer(ch);
  for(int n=0;n<ns;++n){float low{},upper{},mid{},high{};split1.processSample(ch,src[n],low,upper);split2.processSample(ch,upper,mid,high);lp[n]=lowPhase.processSample(ch,low);mp[n]=mid;hp[n]=high;}
 }
} }
