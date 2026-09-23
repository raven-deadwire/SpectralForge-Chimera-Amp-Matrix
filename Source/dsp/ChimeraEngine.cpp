#include "ChimeraEngine.h"
namespace spectralforge {
void ChimeraEngine::prepare(const juce::dsp::ProcessSpec&s){crossover.prepare(s);for(auto&l:lanes)l.prepare(s);for(auto&b:work)b.setSize((int)s.numChannels,(int)s.maximumBlockSize);}
void ChimeraEngine::reset(){crossover.reset();for(auto&l:lanes)l.reset();}
void ChimeraEngine::process(juce::AudioBuffer<float>&b,RoutingMode mode,float x1,float x2,const std::array<float,3>&lev,const std::array<int,3>&models,const std::array<bool,3>&mute,const std::array<bool,3>&solo){
 int ns=b.getNumSamples(),nc=b.getNumChannels();bool any=solo[0]||solo[1]||solo[2];
 for(int i=0;i<3;++i){work[i].setSize(nc,ns,false,false,true);lanes[i].setModel((AmpModel)juce::jlimit(0,2,models[i]));lanes[i].setLevelDb(lev[i]);lanes[i].setMuted(mute[i]||(any&&!solo[i]));}
 if(mode==RoutingMode::classic){lanes[0].process(b);return;}
 if(mode==RoutingMode::dual){work[0].makeCopyOf(b,true);work[1].makeCopyOf(b,true);lanes[0].process(work[0]);lanes[1].process(work[1]);b.clear();for(int i=0;i<2;++i)for(int c=0;c<nc;++c)b.addFrom(c,0,work[i],c,0,ns,.5f);return;}
 crossover.setFrequencies(x1,x2);crossover.split(b,work[0],work[1],work[2]);b.clear();
 for(int i=0;i<3;++i){lanes[i].process(work[i]);for(int c=0;c<nc;++c)b.addFrom(c,0,work[i],c,0,ns);}
} }
