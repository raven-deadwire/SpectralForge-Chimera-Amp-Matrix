#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
namespace spectralforge {
class PreFxRack {
public:
 void prepare(const juce::dsp::ProcessSpec&);void reset();
 void setGate(float thresholdDb,bool enabled);void setCompressor(float thresholdDb,float ratio,float attackMs,float releaseMs,bool enabled);void setBoost(float gainDb,bool enabled);void setOverdrive(float drive,float tone,float levelDb,bool enabled);void setEq(float lowDb,float midDb,float highDb,bool enabled);
 void process(juce::AudioBuffer<float>&);
private:
 double sampleRate{48000.0};float gateThreshold{0.001f},boostGain{1.f},odDrive{0.f},odTone{0.5f},odLevel{1.f};bool gateOn{},compOn{},boostOn{},odOn{},eqOn{};juce::dsp::Compressor<float> compressor;juce::dsp::IIR::Filter<float> odToneFilter;
 juce::dsp::IIR::Filter<float> low,mid,high;
};
}
