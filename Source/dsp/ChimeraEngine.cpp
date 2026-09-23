#include "ChimeraEngine.h"
namespace spectralforge {
void ChimeraEngine::prepare(const juce::dsp::ProcessSpec&s){sampleRate=s.sampleRate;crossover.prepare(s);for(auto&l:lanes)l.prepare(s);for(auto&d:delays)d.prepare(s);for(auto&b:work)b.setSize((int)s.numChannels,(int)s.maximumBlockSize);}
void ChimeraEngine::reset(){crossover.reset();for(auto&l:lanes)l.reset();for(auto&d:delays)d.reset();}
void ChimeraEngine::applyAlignment(juce::AudioBuffer<float>&b,int lane,const LaneSettings&s){
 const float maxMs=5.f, ms=juce::jlimit(0.f,maxMs,s.fineDelayMs);delays[(size_t)lane].setDelay((float)(sampleRate*ms/1000.0));
 juce::dsp::AudioBlock<float> block(b);juce::dsp::ProcessContextReplacing<float> ctx(block);delays[(size_t)lane].process(ctx);
 if(s.polarityInvert)b.applyGain(-1.f);
}
void ChimeraEngine::process(juce::AudioBuffer<float>&b,RoutingMode mode,float x1,float x2,const std::array<LaneSettings,3>&s){
 int ns=b.getNumSamples(),nc=b.getNumChannels();bool any=s[0].solo||s[1].solo||s[2].solo;
 for(int i=0;i<3;++i){work[i].setSize(nc,ns,false,false,true);lanes[i].setModel((AmpModel)juce::jlimit(0,2,s[i].model));lanes[i].setLevelDb(s[i].levelDb);lanes[i].setMuted(s[i].mute||(any&&!s[i].solo));}
 if(mode==RoutingMode::classic){lanes[0].process(b);applyAlignment(b,0,s[0]);return;}
 if(mode==RoutingMode::dual){work[0].makeCopyOf(b,true);work[1].makeCopyOf(b,true);b.clear();for(int i=0;i<2;++i){lanes[i].process(work[i]);applyAlignment(work[i],i,s[i]);for(int c=0;c<nc;++c)b.addFrom(c,0,work[i],c,0,ns,.5f);}return;}
 crossover.setFrequencies(x1,x2);crossover.split(b,work[0],work[1],work[2]);b.clear();
 for(int i=0;i<3;++i){lanes[i].process(work[i]);applyAlignment(work[i],i,s[i]);for(int c=0;c<nc;++c)b.addFrom(c,0,work[i],c,0,ns);}
} }
