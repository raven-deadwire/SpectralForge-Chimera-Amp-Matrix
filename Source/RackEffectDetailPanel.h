#pragma once
#include "PluginProcessor.h"
#include "NativeControlView.h"
#include "EffectSelectionCatalog.h"
#include "HardwareArtwork.h"

// The three time/modulation processors retain their released parameter IDs.
// A second set of attachments lets the rack overview and ALL panel stay in sync.
class RackEffectDetailPanel final : public juce::Component,private juce::Timer {
public:
    RackEffectDetailPanel(ChimeraProcessor& owner,int familyIndex):processor(owner),family(familyIndex) {
        setComponentID("postEffectDetail"+juce::String(family));
        for(auto* label:{&title,&reference}){addAndMakeVisible(*label);label->setColour(juce::Label::textColourId,juce::Colour(0xffe1dbce));}
        title.setFont(juce::FontOptions(13.f,juce::Font::bold));reference.setFont(juce::FontOptions(11.f));
        title.setText(spectralforge::modelFamilies[(size_t)family].category,juce::dontSendNotification);
        addAndMakeVisible(model);model.setComponentID(spectralforge::modelFamilies[(size_t)family].parameter);
        const auto names=spectralforge::effectFamilyMenuNames(family);
        if(family==10){const std::array<const char*,5> groups{"Chorus","Phaser","Flanger","Vibrato","Tremolo"};for(int group=0;group<5;++group){juce::PopupMenu items;for(int m=0;m<names.size();++m)if((m<2?0:m-1)==group)items.addItem(m+1,names[m]);model.getRootMenu()->addSubMenu(groups[(size_t)group],items);}}
        else {const std::array<const char*,3> groups=family==1?std::array<const char*,3>{"Digital","Tape","Analog"}:std::array<const char*,3>{"Plate","Hall","Spring"};for(int m=0;m<names.size();++m){juce::PopupMenu items;items.addItem(m+1,names[m]);model.getRootMenu()->addSubMenu(groups[(size_t)m],items);}}
        modelAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters(),model.getComponentID(),model);
        model.onChange=[this]{refreshModel();};
        addAndMakeVisible(enabled);enabled.setClickingTogglesState(true);enabled.setComponentID(family==10?"choruson":family==1?"delayon":"reverbon");
        enabledAttachment=std::make_unique<juce::ParameterAttachment>(*processor.parameters().getParameter(enabled.getComponentID()),[this](float value){enabled.setToggleState(value>.5f,juce::dontSendNotification);enabled.setButtonText(value>.5f?"ON":"OFF");});enabledAttachment->sendInitialUpdate();
        enabled.onClick=[this]{enabledAttachment->setValueAsCompleteGesture(enabled.getToggleState()?1.f:0.f);};
        const std::array<const char*,3> ids=family==10?std::array<const char*,3>{"chorusrate","chorusdepth","chorusmix"}:family==1?std::array<const char*,3>{"delaytime","delayfeedback","delaymix"}:std::array<const char*,3>{"reverbsize","reverbdamping","reverbmix"};
        const std::array<const char*,3> labels=family==10?std::array<const char*,3>{"RATE","DEPTH","MIX"}:family==1?std::array<const char*,3>{"TIME","FEEDBACK","MIX"}:std::array<const char*,3>{"SIZE","DAMPING","MIX"};
        for(int i=0;i<3;++i){auto control=std::make_unique<NativeControlView>();const auto& range=processor.parameters().getParameter(ids[(size_t)i])->getNormalisableRange();control->bind(processor.parameters(),ids[(size_t)i],labels[(size_t)i],title.getText(),0,{},range.start,range.end,range.interval,3);addAndMakeVisible(*control);controls.push_back(std::move(control));}
        if(family==1){auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),"delaysync","TEMPO SYNC","DELAY",2,{"OFF","ON"},0,1,1,3);addAndMakeVisible(*control);controls.push_back(std::move(control));controls.front()->slider.textFromValueFunction=[this](double value){const bool sync=processor.parameters().getRawParameterValue("delaysync")->load()>.5f;return juce::String(sync?60000.0/processor.currentTempo():value,1)+" ms";};timerCallback();startTimerHz(15);}
        if(family==10)controls.front()->slider.setTextValueSuffix(" Hz");
        refreshModel();setSize(740,270);
    }
    ~RackEffectDetailPanel() override {stopTimer();setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171b1b));spectralforge::art::rack(g,{0,0,(float)getWidth(),78},family,juce::jmax(0,model.getSelectedId()-1));
        g.setColour(juce::Colour(0xff0d1010).withAlpha(.9f));g.fillRoundedRectangle(8,6,(float)getWidth()-16,64,4);
    }
    void resized() override {
        title.setBounds(18,8,260,18);model.setBounds(18,29,330,25);reference.setBounds(18,54,getWidth()-36,18);enabled.setBounds(getWidth()-104,24,85,28);
        const int cell=(getWidth()-32)/juce::jmax(1,(int)controls.size());for(size_t i=0;i<controls.size();++i)controls[i]->setBounds(16+(int)i*cell,92,cell-8,154);
    }
private:
    ChimeraProcessor& processor;int family{};juce::Label title,reference;juce::ComboBox model;juce::TextButton enabled;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment;
    std::unique_ptr<juce::ParameterAttachment> enabledAttachment;std::vector<std::unique_ptr<NativeControlView>> controls;
    void timerCallback() override {if(family==1 && !controls.empty()){controls.front()->slider.setEnabled(processor.parameters().getRawParameterValue("delaysync")->load()<.5f);controls.front()->slider.updateText();}}
    void refreshModel(){const int selected=juce::jmax(0,model.getSelectedId()-1);reference.setText(juce::String("REFERENCE: ")+juce::String::fromUTF8(spectralforge::modelInfo(family,selected).reference),juce::dontSendNotification);reference.setTooltip(reference.getText());for(auto& control:controls)control->slider.getProperties().set("knobStyle",spectralforge::art::rackStyle(family,selected).knobStyle);repaint();}
};
