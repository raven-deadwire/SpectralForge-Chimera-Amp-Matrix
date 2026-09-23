#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class ChimeraEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ChimeraEditor(ChimeraProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    void timerCallback() override;
    void updateModeUI();
    void updateBandLabels();
    void setupKnob(juce::Slider&, const juce::String&, const juce::String& suffix = {});
    ChimeraProcessor& processor;
    juce::TooltipWindow tooltips{this, 600};
    juce::Label title, routingHelp, x1Label, x2Label;
    juce::ComboBox mode;
    juce::Slider x1, x2;
    std::unique_ptr<CA> ma;
    std::unique_ptr<SA> a1, a2;
    struct LaneUI {
        juce::Label header, range, toneLabel, tonePivot, toneHelp;
        juce::ComboBox amp;
        juce::Slider drive, level, bass, lm, hm, treble, pres, res, bandTone;
        std::array<juce::Label, 8> knobLabels;
        juce::TextButton mute{"MUTE"}, solo{"SOLO"};
        std::unique_ptr<CA> aa;
        std::array<std::unique_ptr<SA>, 8> sa;
        std::unique_ptr<SA> toneAttachment;
        std::unique_ptr<BA> muteAttachment, soloAttachment;
        std::array<juce::Slider*, 8> fullRangeControls()
        { return {&drive, &level, &bass, &lm, &hm, &treble, &pres, &res}; }
    };
    std::array<LaneUI, 3> lanes;
    int lastMode{-1};
};
