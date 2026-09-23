#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
namespace spectralforge {
class ThreeBandCrossover {
 using F=juce::dsp::LinkwitzRileyFilter<float>;
public:
 void prepare(const juce::dsp::ProcessSpec&); void reset(); void setFrequencies(float,float);
 void split(const juce::AudioBuffer<float>&,juce::AudioBuffer<float>&,juce::AudioBuffer<float>&,juce::AudioBuffer<float>&);
private: F split1,split2,lowPhase; float x1{150.f},x2{1200.f};
}; }
