#include <juce_dsp/juce_dsp.h>
#include "../Source/dsp/ThreeBandCrossover.h"
#include <iostream>
#include <cmath>
int main(){
 for(double sr:{48000.0,96000.0}){
  spectralforge::ThreeBandCrossover x;juce::dsp::ProcessSpec spec{sr,512,2};x.prepare(spec);x.setFrequencies(150.f,1200.f);
  juce::AudioBuffer<float> in(2,512),lo,mi,hi;in.clear();in.setSample(0,0,1.f);in.setSample(1,0,1.f);x.split(in,lo,mi,hi);
  for(int ch=0;ch<2;++ch)for(int n=0;n<512;++n){const float y=lo.getSample(ch,n)+mi.getSample(ch,n)+hi.getSample(ch,n);if(!std::isfinite(y)){std::cerr<<"non-finite";return 2;}}
 }
 return 0;
}
