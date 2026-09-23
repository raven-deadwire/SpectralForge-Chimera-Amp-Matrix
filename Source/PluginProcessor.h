#pragma once
#include <JuceHeader.h>
#include "dsp/ChimeraEngine.h"
class ChimeraAmpMatrixAudioProcessor:public juce::AudioProcessor{
public:
 ChimeraAmpMatrixAudioProcessor(); void prepareToPlay(double,int)override;void releaseResources()override{}
 bool isBusesLayoutSupported(const BusesLayout&)const override;void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
 juce::AudioProcessorEditor*createEditor()override;bool hasEditor()const override{return true;}
 const juce::String getName()const override{return "Chimera Amp Matrix";} bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 0;}int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return{};}void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&)override;void setStateInformation(const void*,int)override;
private:
 static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();juce::AudioProcessorValueTreeState state;spectralforge::ChimeraEngine engine;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChimeraAmpMatrixAudioProcessor)
};
