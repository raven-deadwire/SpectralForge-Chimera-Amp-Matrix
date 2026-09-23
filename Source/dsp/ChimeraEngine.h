#pragma once
#include <JuceHeader.h>
#include "ThreeBandCrossover.h"
#include "LaneProcessor.h"
namespace spectralforge {
enum class RoutingMode:int{classic=0,dual=1,matrix=2};
class ChimeraEngine {
public:
 void prepare(const juce::dsp::ProcessSpec&); void reset();
 void process(juce::AudioBuffer<float>&,RoutingMode,float,float,const std::array<float,3>&,const std::array<int,3>&,const std::array<bool,3>&,const std::array<bool,3>&);
private: ThreeBandCrossover crossover; std::array<LaneProcessor,3> lanes; std::array<juce::AudioBuffer<float>,3> work;
};}
