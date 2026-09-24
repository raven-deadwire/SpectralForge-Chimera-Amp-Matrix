#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class ChimeraLookAndFeel : public juce::LookAndFeel_V4 {
public:
    ChimeraLookAndFeel();
    void drawLinearSlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider::SliderStyle,juce::Slider&) override;
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
    juce::Font getTextButtonFont(juce::TextButton&,int) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
};
class ChimeraEditor : public juce::AudioProcessorEditor, private juce::Timer,
                       public juce::FileDragAndDropTarget {
public:
    explicit ChimeraEditor(ChimeraProcessor&);
    ~ChimeraEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
private:
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    void timerCallback() override;
    void updateModeUI();
    void updateBandLabels();
    void setupSlider(juce::Slider&,const juce::String&,const juce::String& suffix={});
    void loadIR(int);
    void showInfo();
    void referenceFile(bool save);
    void layoutControls();
    ChimeraProcessor& processor;
    ChimeraLookAndFeel look;
    juce::Component canvas;
    juce::TooltipWindow tooltips{this,600};
    juce::Label title,routingHelp,x1Label,x2Label,gateStatus,pitchStatus;
    juce::ComboBox mode,quality,scale;
    juce::Slider x1,x2;
    std::unique_ptr<CA> ma,qa;
    std::unique_ptr<SA> a1,a2;
    std::array<juce::Slider,7> globalSliders;
    std::array<std::unique_ptr<SA>,7> globalAttachments;
    juce::TextButton gateOn{"GATE"},pitchOn{"TRANSPOSE"},tunerOn{"TUNER"},tunerMute{"AUTO MUTE"},info{"INFO"};
    std::array<std::unique_ptr<BA>,4> globalButtons;
    juce::TextButton compareA{"A"},compareB{"B"},copyAB{"COPY"},rigsTab{"RIGS"},preTab{"PRE"},postTab{"POST"};
    int page{}; // 0: rigs, 1: pedalboard, 2: rack
    juce::Slider lowComp;
    juce::Label lowCompLabel,diVoice,diNote;
    std::unique_ptr<SA> lowCompAttachment;
    struct FXUI {
        juce::Label header,scope,description;
        juce::TextButton enabled{"ON"};
        std::array<juce::Slider,3> controls;
        std::array<juce::Label,3> labels;
        std::array<std::unique_ptr<SA>,3> attachments;
        std::unique_ptr<BA> button;
    };
    std::array<FXUI,5> effects; // drive, delay, reverb, gate, transpose
    struct LaneUI {
        juce::Label header,range,toneLabel,tonePivot,cabStatus;
        juce::ComboBox amp,cabType;
        juce::Slider drive,level,bass,lm,hm,treble,pres,res,bandTone,cabLow,cabHigh;
        std::array<juce::Label,8> knobLabels;
        juce::Label lowLabel,highLabel;
        juce::TextButton mute{"MUTE"},solo{"SOLO"},polarity{"INV"},cabOn{"CAB"},ampOn{"AMP"},load{"LOAD"};
        std::unique_ptr<CA> aa,ca;
        std::array<std::unique_ptr<SA>,8> sa;
        std::unique_ptr<SA> toneAttachment,lowAttachment,highAttachment;
        std::array<std::unique_ptr<BA>,5> buttons;
        std::array<juce::Slider*,8> controls() {return {&drive,&level,&bass,&lm,&hm,&treble,&pres,&res};}
    };
    std::array<LaneUI,3> lanes;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastMode{-1};
};
