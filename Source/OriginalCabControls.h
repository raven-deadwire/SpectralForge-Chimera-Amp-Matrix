#pragma once
#include "PluginProcessor.h"

class OriginalCabControls : public juce::Component, private juce::Timer {
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    juce::Label title, tweeterLabel;
    juce::ComboBox design,rear;
    juce::Slider tweeter;
    struct Slot {
        juce::ToggleButton enabled;
        juce::ComboBox mic,unit;
        juce::Slider position,distance;
        juce::Label positionLabel,distanceLabel;
    };
    std::array<Slot,2> slots;
    std::vector<std::unique_ptr<CA>> choices;
    std::vector<std::unique_ptr<SA>> sliders;
    std::vector<std::unique_ptr<BA>> buttons;
    void timerCallback() override {
        for(auto& s:slots) {const bool on=s.enabled.getToggleState();s.mic.setEnabled(on);s.unit.setEnabled(on);s.position.setEnabled(on);s.distance.setEnabled(on);}
    }
public:
    ~OriginalCabControls() override {stopTimer();}
    OriginalCabControls(ChimeraProcessor& processor,int lane) {
        auto& state=processor.parameters();
        auto combo=[&](juce::ComboBox& box,const char* suffix,juce::StringArray items) {
            addAndMakeVisible(box);box.addItemList(items,1);const auto id=spectralforge::originalCabID(lane,suffix);box.setComponentID(id);
            choices.push_back(std::make_unique<CA>(state,id,box));
        };
        auto slider=[&](juce::Slider& control,const char* suffix) {
            addAndMakeVisible(control);control.setSliderStyle(juce::Slider::LinearHorizontal);control.setTextBoxStyle(juce::Slider::TextBoxRight,false,65,24);
            const auto id=spectralforge::originalCabID(lane,suffix);control.setComponentID(id);sliders.push_back(std::make_unique<SA>(state,id,control));
        };
        title.setText("ORIGINAL / MODELED v1  |  Independent acoustic approximation",juce::dontSendNotification);addAndMakeVisible(title);
        combo(design,"design",{"Guitar 4x12 / v1","Bass 4x10 / v1"});
        combo(rear,"rear",{"Closed rear","Open rear"});
        tweeterLabel.setText("Tweeter",juce::dontSendNotification);addAndMakeVisible(tweeterLabel);slider(tweeter,"tweeter");
        tweeter.setTooltip("Optional original central tweeter; modeled crossover and propagation. No measured hardware claim.");
        for(int i=0;i<2;++i) {
            auto& s=slots[i];const auto prefix=juce::String(i ? "B" : "A");
            addAndMakeVisible(s.enabled);s.enabled.setButtonText("Use modeled Mic "+prefix);const auto id=spectralforge::originalCabID(lane,(prefix+"on").toRawUTF8());
            s.enabled.setComponentID(id);buttons.push_back(std::make_unique<BA>(state,id,s.enabled));
            combo(s.mic,(prefix+"mic").toRawUTF8(),{"Attack dynamic / v1","Body ribbon / v1","Detail condenser / v1"});
            combo(s.unit,(prefix+"unit").toRawUTF8(),{"Unit 1 / upper left","Unit 2 / upper right","Unit 3 / lower left","Unit 4 / lower right"});
            slider(s.position,(prefix+"position").toRawUTF8());slider(s.distance,(prefix+"distance").toRawUTF8());
            s.positionLabel.setText("Position",juce::dontSendNotification);s.distanceLabel.setText("Distance (cm)",juce::dontSendNotification);
            addAndMakeVisible(s.positionLabel);addAndMakeVisible(s.distanceLabel);
            s.position.setTooltip("0 = cone centre; 1 = right cone edge. Position changes path lengths and interference from all four units.");
            s.distance.setTooltip("2-60 cm from the front cone plane. Natural level loss and arrival delay; no automatic alignment or room reverb.");
        }
        timerCallback();startTimerHz(10);
    }
    void resized() override {
        title.setBounds(0,0,getWidth(),28);design.setBounds(0,34,220,28);rear.setBounds(230,34,160,28);
        tweeterLabel.setBounds(405,34,70,28);tweeter.setBounds(475,34,getWidth()-475,28);
        const int w=(getWidth()-16)/2;
        for(int i=0;i<2;++i) {
            auto& s=slots[i];const int x=i*(w+16);
            s.enabled.setBounds(x,73,w,26);s.mic.setBounds(x,106,w,28);s.unit.setBounds(x,142,w,28);
            s.positionLabel.setBounds(x,180,100,25);s.position.setBounds(x+100,180,w-100,25);
            s.distanceLabel.setBounds(x,214,100,25);s.distance.setBounds(x+100,214,w-100,25);
        }
    }
};
