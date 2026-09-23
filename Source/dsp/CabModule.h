#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
namespace spectralforge {
class CabModule {
public:
 void prepare(const juce::dsp::ProcessSpec&);void reset();
 bool loadImpulseResponse(const juce::File& file);void setEnabled(bool e){enabled=e;}void setLowCut(float hz);void setHighCut(float hz);void process(juce::AudioBuffer<float>&);
private:double sampleRate{48000.0};bool enabled{true};float lowCutHz{70.f},highCutHz{9000.f};juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,juce::dsp::IIR::Coefficients<float>> hp,lp;juce::dsp::Convolution convolution;bool irLoaded{false};
};}
