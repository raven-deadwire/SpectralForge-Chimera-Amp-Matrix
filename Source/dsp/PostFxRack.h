#pragma once
#include <juce_dsp/juce_dsp.h>
namespace spectralforge {
class PostFxRack {
public:void prepare(const juce::dsp::ProcessSpec&);void reset();void setChorus(float rate,float depth,float mix,bool on);void setLimiter(float thresholdDb,bool on);void process(juce::AudioBuffer<float>&);
private:bool chorusOn{},limiterOn{};float chorusMix{.25f};juce::AudioBuffer<float> dry;juce::dsp::Chorus<float> chorus;juce::dsp::Limiter<float> limiter;
};}
