#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
class ChimeraAmpMatrixAudioProcessorEditor:public juce::AudioProcessorEditor{
public:explicit ChimeraAmpMatrixAudioProcessorEditor(ChimeraAmpMatrixAudioProcessor&);void paint(juce::Graphics&)override;void resized()override;
private:juce::Label title;
};
