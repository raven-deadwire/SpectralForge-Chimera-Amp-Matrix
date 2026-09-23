#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "ThreeBandCrossover.h"
#include "LaneProcessor.h"
namespace spectralforge {
enum class RoutingMode:int{classic=0,dual=1,matrix=2};
struct LaneSettings {
 float levelDb{0.f}; int model{0}; bool mute{false}; bool solo{false};
 bool polarityInvert{false}; float fineDelayMs{0.f};
};
class ChimeraEngine {
public:
 void prepare(const juce::dsp::ProcessSpec&); void reset();
 void process(juce::AudioBuffer<float>&,RoutingMode,float,float,const std::array<LaneSettings,3>&);
private:
 void applyAlignment(juce::AudioBuffer<float>&,int lane,const LaneSettings&);
 ThreeBandCrossover crossover; std::array<LaneProcessor,3> lanes; std::array<juce::AudioBuffer<float>,3> work;
 using Delay = juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear>;
 std::array<Delay,3> delays { Delay(512), Delay(512), Delay(512) };
 double sampleRate{48000.0};
};}
