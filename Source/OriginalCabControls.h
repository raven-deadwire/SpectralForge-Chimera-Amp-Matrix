#pragma once
#include "PluginProcessor.h"
#include "CabScene.h"

class OriginalCabControls : public juce::Component,private juce::Timer {
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    juce::Label tweeterLabel,speakerLabel,sceneHint;
    juce::ComboBox design,rear;
    juce::Slider tweeter;
    CabScene scene;
    struct Slot {
        juce::ToggleButton enabled;
        juce::ComboBox mic,unit;
        juce::Slider position,distance;
        juce::Label positionLabel,distanceLabel,mode;
    };
    std::array<Slot,2> slots;
    std::vector<std::unique_ptr<CA>> choices;
    std::vector<std::unique_ptr<SA>> sliders;
    std::vector<std::unique_ptr<BA>> buttons;
    void timerCallback() override {
        bool any=false;
        for(auto& s:slots) {
            const bool on=s.enabled.getToggleState();any=any || on;
            s.mic.setEnabled(on);s.unit.setEnabled(on);s.position.setEnabled(on);s.distance.setEnabled(on);
            s.mode.setText(on ? "LIVE CABINET GEOMETRY" : "CAPTURED IR ACTIVE",juce::dontSendNotification);
            s.mode.setColour(juce::Label::textColourId,juce::Colour(on ? 0xffa9b9b5 : 0xffb99e79));
        }
        design.setEnabled(any);rear.setEnabled(any);tweeter.setEnabled(any);
        speakerLabel.setText(design.getSelectedId()==2 ? "4 x 10\" BASS UNITS" : "4 x 12\" GUITAR UNITS",juce::dontSendNotification);
        sceneHint.setText(any ? "Drag a microphone to move it  |  Shift-drag for distance" :
            "Cabinet preview  |  Enable Original Mic A or B to use this cabinet",juce::dontSendNotification);
        scene.refresh();
    }
public:
    static int sideWidth(int width) noexcept {return juce::jlimit(164,188,juce::roundToInt(float(width)*.181f));}
    CabScene& getScene() noexcept {return scene;}
    const CabScene& getScene() const noexcept {return scene;}
    ~OriginalCabControls() override {stopTimer();scene.endMicDrag();}
    OriginalCabControls(ChimeraProcessor& processor,int lane):scene(processor,lane) {
        setInterceptsMouseClicks(false,true);
        auto& state=processor.parameters();
        auto combo=[&](juce::ComboBox& box,const char* suffix,juce::StringArray items) {
            addAndMakeVisible(box);box.addItemList(items,1);
            const auto id=spectralforge::originalCabID(lane,suffix);box.setComponentID(id);
            choices.push_back(std::make_unique<CA>(state,id,box));
        };
        auto slider=[&](juce::Slider& control,const char* suffix,bool rotary=true) {
            addAndMakeVisible(control);
            control.setSliderStyle(rotary ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearHorizontal);
            control.setTextBoxStyle(rotary ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight,false,rotary ? 72 : 54,21);
            const auto id=spectralforge::originalCabID(lane,suffix);control.setComponentID(id);
            sliders.push_back(std::make_unique<SA>(state,id,control));
        };
        setComponentID("originalCabControls"+juce::String(lane+1));addAndMakeVisible(scene);
        combo(design,"design",{"Chimera Guitar 4x12","Chimera Bass 4x10"});
        combo(rear,"rear",{"Closed rear","Open rear"});
        tweeterLabel.setText("TWEETER",juce::dontSendNotification);addAndMakeVisible(tweeterLabel);
        tweeterLabel.setFont(juce::FontOptions(10.f,juce::Font::bold));
        slider(tweeter,"tweeter",false);
        tweeter.textFromValueFunction=[](double value){return juce::String(value*100.,0)+"%";};
        tweeter.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()*.01;};tweeter.updateText();
        tweeter.setTooltip("Adds the cabinet's central tweeter. Increase its level for more high-frequency detail.");
        for(auto* label:{&speakerLabel,&sceneHint}) {
            addAndMakeVisible(*label);label->setFont(juce::FontOptions(10.5f));
            label->setColour(juce::Label::textColourId,juce::Colour(0xff8e9697));
            label->setJustificationType(juce::Justification::centred);
        }
        for(int i=0;i<2;++i) {
            auto& s=slots[size_t(i)];const auto prefix=juce::String(i ? "B" : "A");
            addAndMakeVisible(s.enabled);s.enabled.setButtonText("Original Mic "+prefix);
            const auto id=spectralforge::originalCabID(lane,(prefix+"on").toRawUTF8());s.enabled.setComponentID(id);
            s.enabled.setTooltip("Use the original cabinet and microphone response for Mic "+prefix+". Turning this off restores its selected captured IR.");
            buttons.push_back(std::make_unique<BA>(state,id,s.enabled));
            combo(s.mic,(prefix+"mic").toRawUTF8(),{"Attack Dynamic","Body Ribbon","Detail Condenser"});
            s.mic.setTooltip("Three Chimera original microphone responses. Captured microphone choices are in IR LOADER.");
            combo(s.unit,(prefix+"unit").toRawUTF8(),{"Unit 1 / upper left","Unit 2 / upper right","Unit 3 / lower left","Unit 4 / lower right"});
            s.unit.setTooltip("Choose which of the cabinet's four identical speaker units this microphone faces.");
            slider(s.position,(prefix+"position").toRawUTF8());slider(s.distance,(prefix+"distance").toRawUTF8());
            s.positionLabel.setText("POSITION",juce::dontSendNotification);s.distanceLabel.setText("DISTANCE / cm",juce::dontSendNotification);
            for(auto* label:{&s.positionLabel,&s.distanceLabel,&s.mode}) {
                addAndMakeVisible(*label);label->setFont(juce::FontOptions(10.f));
                label->setColour(juce::Label::textColourId,juce::Colour(0xffa8acad));
                label->setJustificationType(juce::Justification::centred);
            }
            s.mode.setComponentID("ocab"+juce::String(lane+1)+"_"+prefix+"mode");
            s.position.textFromValueFunction=[](double value){return juce::String(value*100.,1)+"%";};
            s.position.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()*.01;};s.position.updateText();
            s.distance.textFromValueFunction=[](double value){return juce::String(value,1);};s.distance.updateText();
            s.position.setTooltip("0% = cone centre; 100% = right cone edge. Drag the microphone across a speaker to change this position.");
            s.distance.setTooltip("2-60 cm from the cone plane. Shift-drag the microphone vertically to change distance; arrival time and natural level change together.");
            s.mic.onChange=[this]{timerCallback();};s.unit.onChange=[this]{timerCallback();};
            s.position.onValueChange=[this]{timerCallback();};s.distance.onValueChange=[this]{timerCallback();};
            s.enabled.onClick=[this]{timerCallback();};
        }
        design.onChange=[this]{timerCallback();};timerCallback();startTimerHz(20);
    }
    void resized() override {
        const int side=sideWidth(getWidth());
        const int centreX=side+14,centreWidth=getWidth()-2*centreX;
        scene.setBounds(centreX,6,centreWidth,548);
        const int designWidth=juce::roundToInt(float(centreWidth)*.40f);
        const int rearWidth=juce::roundToInt(float(centreWidth)*.23f);
        design.setBounds(centreX,579,designWidth,28);
        rear.setBounds(centreX+designWidth+8,579,rearWidth,28);
        const int tweeterX=centreX+designWidth+rearWidth+24;
        tweeterLabel.setBounds(tweeterX,579,55,28);
        tweeter.setBounds(tweeterX+57,579,centreX+centreWidth-tweeterX-57,28);
        speakerLabel.setBounds(centreX,554,centreWidth,19);
        sceneHint.setBounds(centreX,607,centreWidth,15);
        for(int i=0;i<2;++i) {
            auto& s=slots[size_t(i)];const int x=i ? getWidth()-side : 20;
            const int width=side-20,knob=(width-8)/2;
            s.enabled.setBounds(x,40,width,25);s.mode.setBounds(x,66,width,22);
            s.mic.setBounds(x,99,width,28);s.unit.setBounds(x,139,width,28);
            s.position.setBounds(x-1,182,knob,91);s.distance.setBounds(x+knob+8,182,knob,91);
            s.positionLabel.setBounds(x-1,275,knob,19);s.distanceLabel.setBounds(x+knob+8,275,knob,19);
        }
    }
};
