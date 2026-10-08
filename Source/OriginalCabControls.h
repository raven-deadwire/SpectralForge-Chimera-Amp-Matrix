#pragma once
#include "PluginProcessor.h"
#include "CabArtwork.h"

class OriginalCabControls : public juce::Component, private juce::Timer {
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    juce::Label title, tweeterLabel, speakerLabel;
    juce::ComboBox design,rear;
    juce::Slider tweeter;
    spectralforge::cabArt::View cabinetImage,speakerImage;
    struct Slot {
        juce::ToggleButton enabled;
        juce::ComboBox mic,unit;
        juce::Slider position,distance;
        juce::Label positionLabel,distanceLabel;
        spectralforge::cabArt::View image;
    };
    std::array<Slot,2> slots;
    std::vector<std::unique_ptr<CA>> choices;
    std::vector<std::unique_ptr<SA>> sliders;
    std::vector<std::unique_ptr<BA>> buttons;
    void timerCallback() override {
        for(int i=0;i<2;++i) {
            auto& s=slots[i];const bool on=s.enabled.getToggleState();
            s.mic.setEnabled(on);s.unit.setEnabled(on);s.position.setEnabled(on);s.distance.setEnabled(on);
            s.image.setAsset(spectralforge::cabArt::originalMicrophone(s.mic.getSelectedId()-1),
                "Original Mic "+juce::String(i ? "B" : "A")+": "+s.mic.getText());
            s.image.setActive(on);
        }
        const bool any=slots[0].enabled.getToggleState() || slots[1].enabled.getToggleState();
        design.setEnabled(any);rear.setEnabled(any);tweeter.setEnabled(any);
        const int selected=design.getSelectedId()-1;
        cabinetImage.setAsset(spectralforge::cabArt::cabinet(selected),selected==1 ? "Chimera Bass 4x10 cabinet" : "Chimera Guitar 4x12 cabinet");
        speakerImage.setAsset(spectralforge::cabArt::speaker(selected),selected==1 ? "Original 10-inch bass speaker unit" : "Original 12-inch guitar speaker unit");
        speakerLabel.setText(selected==1 ? "10\" BASS UNIT" : "12\" GUITAR UNIT",juce::dontSendNotification);
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
        setComponentID("originalCabControls"+juce::String(lane+1));
        title.setText("ORIGINAL CABINET",juce::dontSendNotification);addAndMakeVisible(title);
        title.setFont(juce::FontOptions(13.f,juce::Font::bold));
        title.setColour(juce::Label::textColourId,juce::Colour(0xffc4a678));
        for(auto* image:{&cabinetImage,&speakerImage})addAndMakeVisible(*image);
        cabinetImage.setComponentID("ocab"+juce::String(lane+1)+"_cabinetImage");
        speakerImage.setComponentID("ocab"+juce::String(lane+1)+"_speakerImage");
        addAndMakeVisible(speakerLabel);speakerLabel.setFont(juce::FontOptions(10.5f));
        speakerLabel.setColour(juce::Label::textColourId,juce::Colour(0xff99aab5));
        speakerLabel.setJustificationType(juce::Justification::centred);
        combo(design,"design",{"Chimera Guitar 4x12","Chimera Bass 4x10"});
        combo(rear,"rear",{"Closed rear","Open rear"});
        tweeterLabel.setText("Tweeter",juce::dontSendNotification);addAndMakeVisible(tweeterLabel);slider(tweeter,"tweeter");
        tweeter.textFromValueFunction=[](double value){return juce::String(value*100.,0)+"%";};
        tweeter.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()*.01;};tweeter.updateText();
        tweeter.setTooltip("Adds the cabinet's central tweeter. Increase its level for more high-frequency detail.");
        for(int i=0;i<2;++i) {
            auto& s=slots[i];const auto prefix=juce::String(i ? "B" : "A");
            addAndMakeVisible(s.enabled);s.enabled.setButtonText("Original Mic "+prefix);const auto id=spectralforge::originalCabID(lane,(prefix+"on").toRawUTF8());
            s.enabled.setTooltip("Use an original microphone response for Mic "+prefix+". Turn off to return to its selected captured IR.");
            s.enabled.setComponentID(id);buttons.push_back(std::make_unique<BA>(state,id,s.enabled));
            combo(s.mic,(prefix+"mic").toRawUTF8(),{"Attack Dynamic","Body Ribbon","Detail Condenser"});
            s.mic.setTooltip("Select one of three Chimera original microphone responses.");
            combo(s.unit,(prefix+"unit").toRawUTF8(),{"Unit 1 / upper left","Unit 2 / upper right","Unit 3 / lower left","Unit 4 / lower right"});
            s.unit.setTooltip("Choose which of the cabinet's four identical speaker units this microphone faces.");
            slider(s.position,(prefix+"position").toRawUTF8());slider(s.distance,(prefix+"distance").toRawUTF8());
            s.positionLabel.setText("Position",juce::dontSendNotification);s.distanceLabel.setText("Distance (cm)",juce::dontSendNotification);
            addAndMakeVisible(s.positionLabel);addAndMakeVisible(s.distanceLabel);
            addAndMakeVisible(s.image);s.image.setComponentID("ocab"+juce::String(lane+1)+"_"+prefix+"image");
            s.position.textFromValueFunction=[](double value){return juce::String(value*100.,1)+"%";};
            s.position.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()*.01;};s.position.updateText();
            s.distance.textFromValueFunction=[](double value){return juce::String(value,1);};s.distance.updateText();
            s.position.setTooltip("0% = cone centre; 100% = right cone edge. Position changes path lengths and interference from all four units.");
            s.distance.setTooltip("2-60 cm from the front cone plane. Natural level loss and arrival delay; no automatic alignment or room reverb.");
            s.mic.onChange=[this]{timerCallback();};
            s.enabled.onClick=[this]{timerCallback();};
        }
        design.onChange=[this]{timerCallback();};
        timerCallback();startTimerHz(10);
    }
    void paint(juce::Graphics& g) override {
        const int cabinetWidth=juce::roundToInt(getWidth()*.30f);
        const int micWidth=(getWidth()-cabinetWidth-24)/2;
        g.setColour(juce::Colour(0xff1b252d));
        g.fillRoundedRectangle(0,0,float(cabinetWidth),float(getHeight()),8.f);
        for(int i=0;i<2;++i) {
            const float x=float(cabinetWidth+12+i*(micWidth+12));
            g.fillRoundedRectangle(x,0,float(micWidth),float(getHeight()),8.f);
        }
        g.setColour(juce::Colour(0xff31434e));
        g.drawRoundedRectangle(.5f,.5f,float(cabinetWidth-1),float(getHeight()-1),8.f,1.f);
        for(int i=0;i<2;++i) {
            const float x=float(cabinetWidth+12+i*(micWidth+12));
            g.setColour(juce::Colour(i ? 0xffa58961 : 0xff568fa4).withAlpha(.55f));
            g.drawRoundedRectangle(x+.5f,.5f,float(micWidth-1),float(getHeight()-1),8.f,1.f);
        }
    }
    void resized() override {
        const int cabinetWidth=juce::roundToInt(getWidth()*.30f);
        title.setBounds(12,6,cabinetWidth-24,24);
        const int heroWidth=juce::roundToInt(cabinetWidth*.55f);
        cabinetImage.setBounds(10,34,heroWidth,125);
        speakerImage.setBounds(heroWidth+12,43,cabinetWidth-heroWidth-24,94);
        speakerLabel.setBounds(heroWidth+12,137,cabinetWidth-heroWidth-24,18);
        design.setBounds(12,166,cabinetWidth-24,28);rear.setBounds(12,201,cabinetWidth-24,28);
        tweeterLabel.setBounds(12,242,62,25);tweeter.setBounds(76,242,cabinetWidth-88,25);
        const int w=(getWidth()-cabinetWidth-24)/2;
        for(int i=0;i<2;++i) {
            auto& s=slots[i];const int x=cabinetWidth+12+i*(w+12);
            const int imageWidth=juce::roundToInt(w*.27f),controlX=x+imageWidth+18;
            s.enabled.setBounds(x+10,6,w-20,26);s.image.setBounds(x+10,40,imageWidth,116);
            s.mic.setBounds(controlX,51,w-imageWidth-30,28);s.unit.setBounds(controlX,96,w-imageWidth-30,28);
            s.positionLabel.setBounds(x+12,174,92,26);s.position.setBounds(x+106,174,w-118,26);
            s.distanceLabel.setBounds(x+12,215,92,26);s.distance.setBounds(x+106,215,w-118,26);
        }
    }
};
