#pragma once
#include "PluginProcessor.h"
#include "CabScene.h"

class OriginalCabControls : public juce::Component,private juce::Timer {
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    ChimeraProcessor& processor;
    int lane;
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
    static void renameItem(juce::ComboBox& box,int id,const juce::String& text) {
        const int selected=int(box.getSelectedIdAsValue().getValue());
        box.changeItemText(id,text);box.setSelectedId(selected,juce::dontSendNotification);
    }
    spectralforge::cabLayout::Settings requestedSettings() const {
        auto& state=processor.parameters();spectralforge::cabLayout::Settings result{};
        result.layout=juce::roundToInt(state.getRawParameterValue(spectralforge::cabLayoutID(lane,"layout"))->load());
        result.voice.driver=juce::roundToInt(state.getRawParameterValue(spectralforge::cabExpansionID(lane,"driver"))->load());
        result.voice.base.cabinet=juce::roundToInt(state.getRawParameterValue(spectralforge::originalCabID(lane,"design"))->load());
        return result;
    }
    void choose(const juce::String& id,int selected) {
        auto* parameter=processor.parameters().getParameter(id);
        if(juce::roundToInt(parameter->convertFrom0to1(parameter->getValue()))==selected)return;
        parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(float(selected)));parameter->endChangeGesture();
    }
    void selectLayout() {
        if(layout.getSelectedId()<2) {timerCallback();return;}
        auto requested=requestedSettings();requested.layout=juce::jmax(0,layout.getSelectedId()-1);
        const auto compatible=spectralforge::cabLayout::effectiveSettings(requested);
        choose(spectralforge::cabLayoutID(lane,"layout"),compatible.layout);
        choose(spectralforge::cabExpansionID(lane,"driver"),compatible.voice.driver);
        timerCallback();
    }
    void selectDriver() {
        auto requested=requestedSettings();const auto current=spectralforge::cabLayout::effectiveSettings(requested);
        requested.voice.driver=juce::jmax(0,driver.getSelectedId()-1);
        if(spectralforge::cabLayout::isDriverCompatible(requested)
            && spectralforge::cabExpansion::isBass(requested.voice)==spectralforge::cabLayout::isBass(current)
            && spectralforge::cabExpansion::diameter(requested.voice)==spectralforge::cabLayout::nominalInches(current))
            choose(spectralforge::cabExpansionID(lane,"driver"),requested.voice.driver);
        timerCallback();
    }
    void selectFamily() {
        if(requestedSettings().layout==0) {
            choose(spectralforge::originalCabID(lane,"design"),juce::jmax(0,design.getSelectedId()-1));
            choose(spectralforge::cabExpansionID(lane,"driver"),0);
        }
        timerCallback();
    }
    void timerCallback() override {
        const auto requested=requestedSettings();
        const auto effective=spectralforge::cabLayout::effectiveSettings(requested);
        const int selectedLayout=effective.layout;
        // Saved layout zero is the original four-unit cabinet. Present it as
        // the matching visible type without rewriting its DSP/state identity.
        const int visibleLayout=selectedLayout ? selectedLayout : spectralforge::cabLayout::isBass(effective) ? 6 : 3;
        layout.setSelectedId(visibleLayout+1,juce::dontSendNotification);
        driver.setSelectedId(effective.voice.driver+1,juce::dontSendNotification);
        design.setSelectedId(spectralforge::cabLayout::isBass(effective) ? 2 : 1,juce::dontSendNotification);
        const int unitCount=spectralforge::cabLayout::count(selectedLayout);
        bool any=false;
        for(auto& s:slots) {
            const bool on=s.enabled.getToggleState();any=any || on;
            s.unit.setVisible(selectedLayout==0);s.arrayUnit.setVisible(selectedLayout>0);s.arrayUnit.setEnabled(on);
            for(int n=1;n<=8;++n) {
                s.arrayUnit.setItemEnabled(n,n<=unitCount);
                auto target=effective;target.unit=target.voice.base.unit=n-1;
                renameItem(s.arrayUnit,n,"Unit "+juce::String(n)+(n>unitCount ? " (uses "
                    +juce::String(spectralforge::cabLayout::effectiveUnit(target)+1)+")" : ""));
            }
            s.response.setEnabled(on);s.mic.setEnabled(on && s.response.getSelectedId()==1);s.unit.setEnabled(on);s.position.setEnabled(on);s.distance.setEnabled(on);
            s.mode.setText(on ? "CABINET MIC ACTIVE" : "CAPTURED IR ACTIVE",juce::dontSendNotification);
            s.mode.setColour(juce::Label::textColourId,juce::Colour(on ? 0xffa9b9b5 : 0xffb99e79));
            const int selected=s.response.getSelectedId()-2;
            if(selected>=0 && selected<int(spectralforge::cabExpansion::microphones.size())) {
                const auto& model=spectralforge::cabExpansion::microphones[size_t(selected)];
                const auto* reference=spectralforge::micCatalog::byId(model.catalogId);
                s.responseLabel.setText(reference && !reference->original ? reference->reference : "Chimera original",juce::dontSendNotification);
                s.responseLabel.setTooltip("Research reference only. The selected sound is a separately authored Chimera response, not a measured clone.");
            } else {
                s.responseLabel.setText("Chimera original",juce::dontSendNotification);
            }
            renameItem(s.response,1,s.mic.getText());
        }
        renameItem(driver,1,requested.voice.base.cabinet==1 ? "Chimera Bass 10" : "Chimera Guitar 12");
        for(int item=0;item<=int(spectralforge::cabExpansion::drivers.size());++item) {
            auto candidate=requested;candidate.voice.driver=item;
            driver.setItemEnabled(item+1,spectralforge::cabLayout::isDriverCompatible(candidate)
                && spectralforge::cabExpansion::isBass(candidate.voice)==spectralforge::cabLayout::isBass(effective)
                && spectralforge::cabExpansion::diameter(candidate.voice)==spectralforge::cabLayout::nominalInches(effective));
        }
        layout.setEnabled(any);design.setEnabled(any && selectedLayout==0);driver.setEnabled(any);tweeterDesign.setEnabled(any);rear.setEnabled(any);tweeter.setEnabled(any);
        const int selectedDriver=effective.voice.driver-1;
        speakerLabel.setText(selectedDriver>=0 && selectedDriver<int(spectralforge::cabExpansion::drivers.size())
            ? juce::String(unitCount)+" x "+juce::String(spectralforge::cabExpansion::drivers[size_t(selectedDriver)].inches)+"\" / "
                +(selectedDriver==5 ? "Deep low mids, firm attack" : selectedDriver==6 ? "Broad response, tight lows"
                    : spectralforge::cabExpansion::drivers[size_t(selectedDriver)].reference)
            : juce::String(unitCount)+(spectralforge::cabLayout::isBass(effective) ? " x 10\" BASS UNITS" : " x 12\" GUITAR UNITS"),juce::dontSendNotification);
        sceneHint.setText(any ? "Drag a microphone to move it  |  Shift-drag for distance" :
            "Cabinet preview  |  Enable Mic A or B to use this cabinet",juce::dontSendNotification);
        scene.refresh();
    }
public:
    static int sideWidth(int width) noexcept {return juce::jlimit(164,188,juce::roundToInt(float(width)*.181f));}
    CabScene& getScene() noexcept {return scene;}
    const CabScene& getScene() const noexcept {return scene;}
    void refreshState() {timerCallback();}
    ~OriginalCabControls() override {stopTimer();scene.endMicDrag();}
    OriginalCabControls(ChimeraProcessor& processor,int lane):processor(processor),lane(lane),scene(processor,lane) {
        setInterceptsMouseClicks(false,true);
        auto& state=processor.parameters();
        auto combo=[&](juce::ComboBox& box,const char* suffix,juce::StringArray items) {
            addAndMakeVisible(box);box.addItemList(items,1);
            const auto id=spectralforge::originalCabID(lane,suffix);box.setComponentID(id);
            if(juce::String(suffix)=="design")box.onChange=[this]{selectFamily();};
            else choices.push_back(std::make_unique<CA>(state,id,box));
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
            if(juce::String(suffix)=="driver")box.onChange=[this]{selectDriver();};
            else {
                choices.push_back(std::make_unique<CA>(state,id,box));
                box.onChange=[this]{timerCallback();};
            }
        };
        const auto layoutCombo=[&](juce::ComboBox& box,const char* suffix,const juce::StringArray& names) {
            addAndMakeVisible(box);const bool cabinetLayout=juce::String(suffix)=="layout";
            // Keep the host's ten stored ordinals; expose only the nine unique
            // current cabinet types. The UI IDs still equal stored value + 1.
            for(int i=cabinetLayout ? 1 : 0;i<names.size();++i)box.addItem(names[i],i+1);
            const auto id=spectralforge::cabLayoutID(lane,suffix);
            box.setComponentID(id);
            if(cabinetLayout)box.onChange=[this]{selectLayout();};
            else {choices.push_back(std::make_unique<CA>(state,id,box));box.onChange=[this]{timerCallback();};}
        };
        layoutCombo(layout,"layout",spectralforge::cabLayoutNames());
        layout.setTooltip("Choose a cabinet by speaker count and diameter. A compatible speaker loads with the cabinet.");
        setComponentID("originalCabControls"+juce::String(lane+1));addAndMakeVisible(scene);
        combo(design,"design",{"Guitar","Bass"});
        design.setTooltip("Choose Guitar or Bass for the four-speaker cabinet. Other cabinet layouts set this category automatically.");
        combo(rear,"rear",{"Closed rear","Open rear"});
        extra(driver,"driver",spectralforge::expandedDriverNames());
        driver.setTooltip("Choose a speaker that fits the cabinet's type and diameter. All units in the cabinet use this speaker.");
        extra(tweeterDesign,"tweeter",{"Chimera HF","Silk HF","Metal HF","Air HF"});
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
            addAndMakeVisible(s.enabled);s.enabled.setButtonText("Mic "+prefix);
            const auto id=spectralforge::originalCabID(lane,(prefix+"on").toRawUTF8());s.enabled.setComponentID(id);
            s.enabled.setTooltip("Use the cabinet and microphone model for Mic "+prefix+". Turning this off restores its selected captured IR.");
            buttons.push_back(std::make_unique<BA>(state,id,s.enabled));
            combo(s.mic,(prefix+"mic").toRawUTF8(),{"Attack Dynamic","Body Ribbon","Detail Condenser"});
            s.mic.setTooltip("Choose Attack Dynamic, Body Ribbon or Detail Condenser when the Chimera microphone is selected above.");
            extra(s.response,(prefix+"mic").toRawUTF8(),spectralforge::expandedMicNames());
            s.response.setTooltip("Chimera original models: 9 dynamics, 3 ribbons and 8 condensers. Each has its own acoustic response; these are not measured hardware clones.");
            addAndMakeVisible(s.responseLabel);s.responseLabel.setFont(juce::FontOptions(10.f));
            s.responseLabel.setJustificationType(juce::Justification::centred);
            combo(s.unit,(prefix+"unit").toRawUTF8(),{"Unit 1 / upper left","Unit 2 / upper right","Unit 3 / lower left","Unit 4 / lower right"});
            s.unit.setTooltip("Choose the speaker that this microphone picks up.");
            layoutCombo(s.arrayUnit,(prefix+"unit").toRawUTF8(),{"Unit 1","Unit 2","Unit 3","Unit 4","Unit 5","Unit 6","Unit 7","Unit 8"});
            s.arrayUnit.setTooltip("Choose a speaker independently for Mic A or B. Numbering runs left to right, top to bottom.");
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
        timerCallback();startTimerHz(20);
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
