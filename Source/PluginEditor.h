#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
class ChimeraAmpMatrixAudioProcessorEditor:public juce::AudioProcessorEditor{
public:explicit ChimeraAmpMatrixAudioProcessorEditor(ChimeraAmpMatrixAudioProcessor&);void paint(juce::Graphics&)override;void resized()override;
private:
 using ComboA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;using SliderA=juce::AudioProcessorValueTreeState::SliderAttachment;using ButtonA=juce::AudioProcessorValueTreeState::ButtonAttachment;
 struct LaneUI{juce::Label name;juce::ComboBox amp;juce::Slider drive,level,bass,mid,treble,presence,resonance,cabLow,cabHigh,delay;juce::ToggleButton mute{"M"},solo{"S"},polarity{"Ø"},cab{"Cab"};std::unique_ptr<ComboA>ampA;std::array<std::unique_ptr<SliderA>,10> sliders;std::array<std::unique_ptr<ButtonA>,4> buttons;};
 juce::Label title;juce::ComboBox mode;juce::Slider x1,x2;std::array<LaneUI,3> lanes;
 std::unique_ptr<ComboA>modeA;std::unique_ptr<SliderA>x1A,x2A;
};