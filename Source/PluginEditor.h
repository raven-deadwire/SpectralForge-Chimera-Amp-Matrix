#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class ChimeraEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ChimeraEditor(ChimeraProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    void timerCallback() override;
    void updateModeUI();
    void setupKnob(juce::Slider&, const juce::String&, const juce::String& suffix={});
    ChimeraProcessor& processor;
    juce::Label title, routingHelp, x1Label, x2Label;
    juce::ComboBox mode;
    juce::Slider x1,x2;
    std::unique_ptr<CA> ma;
    std::unique_ptr<SA> a1,a2;
    struct L {
        juce::Label header, range;
        juce::ComboBox amp;
        juce::Slider drive,level,bass,lm,hm,treble,pres,res;
        std::unique_ptr<CA> aa;
        std::array<std::unique_ptr<SA>,8> sa;
    };
    std::array<L,3> lanes;
    int lastMode{-1};
};