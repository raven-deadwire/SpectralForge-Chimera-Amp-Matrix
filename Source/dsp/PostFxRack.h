#pragma once
#include <juce_dsp/juce_dsp.h>
namespace spectralforge {
class PostFxRack {
public:void prepare(const juce::dsp::ProcessSpec&);void reset();void setChorus(float,float,float,bool);void setDelay(float,float,float,bool);void setReverb(float,float,float,bool);void setLimiter(float,bool);void process(juce::AudioBuffer<float>&);
private:bool chorusOn{},delayOn{},reverbOn{},limiterOn{};float chorusMix{.25f},delayMix{.2f},delayFeedback{.25f};double sampleRate{48000.0};juce::AudioBuffer<float> dry;juce::dsp::Chorus<float> chorus;juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear> delay{96000};juce::dsp::Reverb reverb;juce::dsp::Limiter<float> limiter;
};}
