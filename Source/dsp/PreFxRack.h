#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
namespace spectralforge {
class PreFxRack {
public:
 void prepare(const juce::dsp::ProcessSpec&);void reset();
 void setGate(float thresholdDb,bool enabled);void setBoost(float gainDb,bool enabled);void setEq(float lowDb,float midDb,float highDb,bool enabled);
 void process(juce::AudioBuffer<float>&);
private:
 double sampleRate{48000.0};float gateThreshold{0.001f},boostGain{1.f};bool gateOn{},boostOn{},eqOn{};
 juce::dsp::IIR::Filter<float> low,mid,high;
};
}
