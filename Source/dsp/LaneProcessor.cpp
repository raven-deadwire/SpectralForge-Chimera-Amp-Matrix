#include "LaneProcessor.h"
#include <cmath>
namespace spectralforge {
void LaneProcessor::process(juce::AudioBuffer<float>& b){
 if(muted){b.clear();return;} const float pg=1.f+drive*14.f;
 for(int n=0;n<b.getNumSamples();++n){ float g=level.getNextValue();
  for(int c=0;c<b.getNumChannels();++c){ auto* p=b.getWritePointer(c); float x=p[n];
   if(model==AmpModel::clean) x=std::tanh(x*(1.f+drive*1.5f));
   else if(model==AmpModel::tightDrive){x=std::tanh(x*pg);x=.88f*x+.12f*x*x*x;}
   else {float s=std::tanh(x*(1.f+drive*8.f));x=(1.f-drive*.28f)*x+(drive*.28f)*s;}
   p[n]=x*g;
  }
 }
 cab.process(b);
} }
