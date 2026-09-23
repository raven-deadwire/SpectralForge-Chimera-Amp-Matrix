#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
class ChimeraEditor:public juce::AudioProcessorEditor{
public:explicit ChimeraEditor(ChimeraProcessor&);void paint(juce::Graphics&)override;void resized()override;
private:using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;using SA=juce::AudioProcessorValueTreeState::SliderAttachment;juce::Label title;juce::ComboBox mode;juce::Slider x1,x2;std::unique_ptr<CA>ma;std::unique_ptr<SA>a1,a2;struct L{juce::ComboBox amp;juce::Slider drive,level,bass,lm,hm,treble,pres,res;std::unique_ptr<CA>aa;std::array<std::unique_ptr<SA>,8>sa;};std::array<L,3>lanes;};
