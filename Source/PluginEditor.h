#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
class ChimeraAmpMatrixAudioProcessorEditor:public juce::AudioProcessorEditor{
public:explicit ChimeraAmpMatrixAudioProcessorEditor(ChimeraAmpMatrixAudioProcessor&);void paint(juce::Graphics&)override;void resized()override;
private:juce::Label title;
};
