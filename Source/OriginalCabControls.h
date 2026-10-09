#pragma once
#include "PluginProcessor.h"
#include "CabScene.h"

class OriginalCabControls : public juce::Component,private juce::Timer {
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    juce::Label tweeterLabel,speakerLabel,sceneHint;
    juce::ComboBox design,rear,driver,tweeterDesign,layout;
    juce::Slider tweeter;
    CabScene scene;
    struct Slot {
        juce::ToggleButton enabled;
        juce::ComboBox mic,unit,response,arrayUnit;
        juce::Label responseLabel;
        juce::Slider position,distance;
        juce::Label positionLabel,distanceLabel,mode;
    };
    std::array<Slot,2> slots;
    std::vector<std::unique_ptr<CA>> choices;
    std::vector<std::unique_ptr<SA>> sliders;
    std::vector<std::unique_ptr<BA>> buttons;
    void timerCallback() override {
        const int selectedLayout=juce::jmax(0,layout.getSelectedId()-1);
        const int unitCount=spectralforge::cabLayout::count(selectedLayout);
        bool any=false;
        for(auto& s:slots) {
            const bool on=s.enabled.getToggleState();any=any || on;
            s.unit.setVisible(selectedLayout==0);s.arrayUnit.setVisible(selectedLayout>0);s.arrayUnit.setEnabled(on);
            for(int n=1;n<=8;++n) {
                s.arrayUnit.setItemEnabled(n,n<=unitCount);
                s.arrayUnit.changeItemText(n,"Unit "+juce::String(n)+(n>unitCount ? " (uses "+juce::String(unitCount)+")" : ""));
            }
            s.response.setEnabled(on);s.mic.setEnabled(on && s.response.getSelectedId()==1);s.unit.setEnabled(on);s.position.setEnabled(on);s.distance.setEnabled(on);
            s.mode.setText(on ? "LIVE CABINET GEOMETRY" : "CAPTURED IR ACTIVE",juce::dontSendNotification);
            s.mode.setColour(juce::Label::textColourId,juce::Colour(on ? 0xffa9b9b5 : 0xffb99e79));
            const int selected=s.response.getSelectedId()-2;
            if(selected>=0 && selected<int(spectralforge::cabExpansion::microphones.size())) {
                const auto& model=spectralforge::cabExpansion::microphones[size_t(selected)];
                const auto* reference=spectralforge::micCatalog::byId(model.catalogId);
                s.responseLabel.setText(reference && !reference->original ? reference->reference : "Chimera original",juce::dontSendNotification);
                s.responseLabel.setTooltip("Research reference only. The selected sound is a separately authored Chimera response, not a measured clone.");
            } else {
                s.responseLabel.setText("Original response v1",juce::dontSendNotification);
            }
        }
        design.changeItemText(1,selectedLayout ? "Legacy guitar driver" : "Chimera Guitar 4x12");
        design.changeItemText(2,selectedLayout ? "Legacy bass driver" : "Chimera Bass 4x10");
        layout.setEnabled(any);design.setEnabled(any && driver.getSelectedId()==1);driver.setEnabled(any);tweeterDesign.setEnabled(any);rear.setEnabled(any);tweeter.setEnabled(any);
        const int selectedDriver=driver.getSelectedId()-2;
        speakerLabel.setText(selectedDriver>=0 && selectedDriver<int(spectralforge::cabExpansion::drivers.size())
            ? juce::String(unitCount)+" x "+juce::String(spectralforge::cabExpansion::drivers[size_t(selectedDriver)].inches)+"\" / "+spectralforge::cabExpansion::drivers[size_t(selectedDriver)].reference
            : juce::String(unitCount)+(design.getSelectedId()==2 ? " x 10\" BASS UNITS" : " x 12\" GUITAR UNITS"),juce::dontSendNotification);
        const int inches=selectedDriver>=0 ? spectralforge::cabExpansion::drivers[size_t(selectedDriver)].inches : design.getSelectedId()==2 ? 10 : 12;
        for(size_t i=0;i<spectralforge::cabLayout::layouts.size();++i) {
            const auto& l=spectralforge::cabLayout::layouts[i];
            layout.changeItemText(int(i)+2,juce::String(l.bass ? "Bass " : "Guitar ")+juce::String(l.columns*l.rows)+"x"+juce::String(inches)
                +(inches!=l.inches ? " / scaled "+juce::String(l.inches)+"-inch box" : ""));
        }
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
        const auto extra=[&](juce::ComboBox& box,const char* suffix,const juce::StringArray& items) {
            addAndMakeVisible(box);
            for(int i=0;i<items.size();++i) {
                const juce::String id(suffix);
                if(id.endsWith("mic")) {
                    if(i==1)box.addSectionHeading("Dynamic");
                    if(i==10)box.addSectionHeading("Ribbon");
                    if(i==13)box.addSectionHeading("Condenser");
                } else if(id=="driver") {
                    if(i==1)box.addSectionHeading("Guitar");
                    if(i==9)box.addSectionHeading("Bass");
                }
                box.addItem(items[i],i+1);
            }
            const auto id=spectralforge::cabExpansionID(lane,suffix);box.setComponentID(id);
            choices.push_back(std::make_unique<CA>(state,id,box));
            box.onChange=[this]{timerCallback();};
        };
        const auto layoutCombo=[&](juce::ComboBox& box,const char* suffix,const juce::StringArray& names) {
            addAndMakeVisible(box);box.addItemList(names,1);const auto id=spectralforge::cabLayoutID(lane,suffix);
            box.setComponentID(id);choices.push_back(std::make_unique<CA>(state,id,box));box.onChange=[this]{timerCallback();};
        };
        layoutCombo(layout,"layout",spectralforge::cabLayoutNames());
        layout.setTooltip("Choose enclosure volume and array. Alternative speaker diameters scale the whole box; the displayed count and diameter describe the actual model. Legacy preserves the previous four-unit response.");
        setComponentID("originalCabControls"+juce::String(lane+1));addAndMakeVisible(scene);
        combo(design,"design",{"Chimera Guitar 4x12","Chimera Bass 4x10"});
        combo(rear,"rear",{"Closed rear","Open rear"});
        extra(driver,"driver",spectralforge::expandedDriverNames());
        driver.setTooltip("Select a Chimera speaker design. The selected cabinet layout scales to its diameter. All units in the box use this design.");
        extra(tweeterDesign,"tweeter",{"Legacy tweeter","Silk HF","Metal HF","Air HF"});
        tweeterDesign.setTooltip("Independent tweeter design. The TWEETER level controls its contribution; zero is off.");
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
            s.mic.setTooltip("The preserved v1 microphone, active when Legacy microphone is selected above.");
            extra(s.response,(prefix+"mic").toRawUTF8(),spectralforge::expandedMicNames());
            s.response.setTooltip("Chimera original models: 9 dynamics, 3 ribbons and 8 condensers. Each has its own acoustic response; these are not measured hardware clones.");
            addAndMakeVisible(s.responseLabel);s.responseLabel.setFont(juce::FontOptions(10.f));
            s.responseLabel.setJustificationType(juce::Justification::centred);
            combo(s.unit,(prefix+"unit").toRawUTF8(),{"Unit 1 / upper left","Unit 2 / upper right","Unit 3 / lower left","Unit 4 / lower right"});
            s.unit.setTooltip("Choose a unit in the preserved four-unit legacy cabinet.");
            layoutCombo(s.arrayUnit,(prefix+"unit").toRawUTF8(),{"Unit 1","Unit 2","Unit 3","Unit 4","Unit 5","Unit 6","Unit 7","Unit 8"});
            s.arrayUnit.setTooltip("Choose any unit independently for Mic A or B. Numbering runs left to right, top to bottom. A saved or automated index beyond this layout uses its last unit and remains stored for larger layouts.");
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
        scene.setBounds(centreX,6,centreWidth,482);
        layout.setBounds(centreX,515,centreWidth,28);
        const int designWidth=juce::roundToInt(float(centreWidth)*.40f);
        const int rearWidth=juce::roundToInt(float(centreWidth)*.23f);
        design.setBounds(centreX,579,designWidth,28);
        rear.setBounds(centreX+designWidth+8,579,rearWidth,28);
        const int tweeterX=centreX+designWidth+rearWidth+24;
        tweeterLabel.setBounds(tweeterX,579,55,28);
        tweeter.setBounds(tweeterX+57,579,centreX+centreWidth-tweeterX-57,28);
        speakerLabel.setBounds(centreX,490,centreWidth,19);
        const int driverWidth=juce::roundToInt(float(centreWidth)*.64f);
        driver.setBounds(centreX,547,driverWidth,28);
        tweeterDesign.setBounds(centreX+driverWidth+8,547,centreWidth-driverWidth-8,28);
        sceneHint.setBounds(centreX,607,centreWidth,15);
        for(int i=0;i<2;++i) {
            auto& s=slots[size_t(i)];const int x=i ? getWidth()-side : 20;
            const int width=side-20,knob=(width-8)/2;
            s.enabled.setBounds(x,40,width,25);s.mode.setBounds(x,66,width,22);
            s.response.setBounds(x,99,width,28);s.mic.setBounds(x,132,width,25);s.unit.setBounds(x,169,width,28);s.arrayUnit.setBounds(x,169,width,28);
            s.position.setBounds(x-1,211,knob,91);s.distance.setBounds(x+knob+8,211,knob,91);
            s.positionLabel.setBounds(x-1,305,knob,19);s.distanceLabel.setBounds(x+knob+8,305,knob,19);
            s.responseLabel.setBounds(x,326,width,12);
        }
    }
};
