#pragma once
#include "PluginProcessor.h"
#include "NativeControlView.h"
#include "EffectSelectionCatalog.h"
#include "HardwareArtwork.h"
#include "EffectModelSync.h"

// The three time/modulation processors retain their released parameter IDs.
// A second set of attachments lets the rack overview and ALL panel stay in sync.
class RackEffectDetailPanel final : public juce::Component,private juce::Timer {
public:
    // `slotIndex` 1: the POST second slot (docs/POST_SECOND_SLOTS.md).
    RackEffectDetailPanel(ChimeraProcessor& owner,int familyIndex,int slotIndex=0):processor(owner),family(familyIndex),slot(slotIndex) {
        setComponentID("postEffectDetail"+juce::String(family)+spectralforge::postSlotSuffix(slot));
        for(auto* label:{&title,&reference}){addAndMakeVisible(*label);label->setColour(juce::Label::textColourId,juce::Colour(0xffe1dbce));}
        title.setFont(juce::FontOptions(13.f,juce::Font::bold));reference.setFont(juce::FontOptions(11.f));
        title.setText(juce::String(spectralforge::modelFamilies[(size_t)family].category)+spectralforge::postSlotNumber(slot),juce::dontSendNotification);
        addAndMakeVisible(model);model.setComponentID(id(spectralforge::modelFamilies[(size_t)family].parameter));
        spectralforge::buildExtendedEffectMenu(*model.getRootMenu(),family);
        modelSync=std::make_unique<spectralforge::ExtendedEffectModelSync>(processor.parameters(),family,model,slot);
        model.onChange=[this]{refreshModel();};
        addAndMakeVisible(enabled);enabled.setClickingTogglesState(true);enabled.setComponentID(id(family==10?"choruson":family==1?"delayon":"reverbon"));
        enabledAttachment=std::make_unique<juce::ParameterAttachment>(*processor.parameters().getParameter(enabled.getComponentID()),[this](float value){enabled.setToggleState(value>.5f,juce::dontSendNotification);enabled.setButtonText(value>.5f?"ON":"OFF");});enabledAttachment->sendInitialUpdate();
        enabled.onClick=[this]{enabledAttachment->setValueAsCompleteGesture(enabled.getToggleState()?1.f:0.f);};
        const std::array<const char*,3> ids=family==10?std::array<const char*,3>{"chorusrate","chorusdepth","chorusmix"}:family==1?std::array<const char*,3>{"delaytime","delayfeedback","delaymix"}:std::array<const char*,3>{"reverbsize","reverbdamping","reverbmix"};
        const std::array<const char*,3> labels=family==10?std::array<const char*,3>{"RATE","DEPTH","MIX"}:family==1?std::array<const char*,3>{"TIME","FEEDBACK","MIX"}:std::array<const char*,3>{"SIZE","DAMPING","MIX"};
        for(int i=0;i<3;++i){auto control=std::make_unique<NativeControlView>();const auto& range=processor.parameters().getParameter(id(ids[(size_t)i]))->getNormalisableRange();control->bind(processor.parameters(),id(ids[(size_t)i]),labels[(size_t)i],title.getText(),0,{},range.start,range.end,range.interval,3);addAndMakeVisible(*control);controls.push_back(std::move(control));}
        if(family==1){auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),id("delaysync"),"TEMPO SYNC","DELAY",2,{"OFF","ON"},0,1,1,3);addAndMakeVisible(*control);controls.push_back(std::move(control));controls.front()->slider.textFromValueFunction=[this](double value){const bool sync=processor.parameters().getRawParameterValue(id("delaysync"))->load()>.5f;return juce::String(sync?60000.0/processor.currentTempo():value,1)+" ms";};timerCallback();startTimerHz(15);}
        if(family==10)controls.front()->slider.setTextValueSuffix(" Hz");
        if(family==2) {
            // The reverb's second row (docs/POST_REVERB_CONTROLS.md): four knobs and the freeze switch.
            for(int i=0;i<spectralforge::postReverbControlsPerSlot;++i) {
                static constexpr std::array<const char*,spectralforge::postReverbControlsPerSlot> names{"PRE-DELAY","LOW CUT","TONE","WIDTH","FREEZE"};
                static constexpr std::array<const char*,spectralforge::postReverbControlsPerSlot> units{" ms"," Hz","","",""};
                static constexpr std::array<int,spectralforge::postReverbControlsPerSlot> decimals{0,0,2,2,0};
                const auto& spec=spectralforge::reverbControlSpecs[(size_t)i];const auto parameter=spectralforge::reverbControlID(slot,i);
                auto control=std::make_unique<NativeControlView>();
                control->bind(processor.parameters(),parameter,names[(size_t)i],title.getText(),spec.toggle?2:0,spec.toggle?juce::StringArray{"OFF","ON"}:juce::StringArray{},spec.minimum,spec.maximum,spec.toggle?1:0,3);
                if(!spec.toggle){control->slider.setTextValueSuffix(units[(size_t)i]);control->slider.setNumDecimalPlacesToDisplay(decimals[(size_t)i]);control->slider.setDoubleClickReturnValue(true,spec.initial);}
                addAndMakeVisible(*control);controls.push_back(std::move(control));
            }
        }
        refreshModel();setSize(740,family==2?434:270);
    }
    ~RackEffectDetailPanel() override {stopTimer();setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171b1b));spectralforge::art::rack(g,{0,0,(float)getWidth(),78},family,juce::jmax(0,model.getSelectedId()-1));
        g.setColour(juce::Colour(0xff0d1010).withAlpha(.9f));g.fillRoundedRectangle(8,6,(float)getWidth()-16,64,4);
    }
    void resized() override {
        title.setBounds(18,8,260,18);model.setBounds(18,29,330,25);reference.setBounds(18,54,getWidth()-36,18);enabled.setBounds(getWidth()-104,24,85,28);
        // The reverb has two rows: SIZE, DAMPING and MIX as before, its added controls under them.
        const size_t first=family==2?3:controls.size();
        const int cell=(getWidth()-32)/juce::jmax(1,(int)first);for(size_t i=0;i<first;++i)controls[i]->setBounds(16+(int)i*cell,92,cell-8,154);
        if(controls.size()>first){const int below=(getWidth()-32)/int(controls.size()-first);for(size_t i=first;i<controls.size();++i)controls[i]->setBounds(16+int(i-first)*below,256,below-8,154);}
    }
private:
    ChimeraProcessor& processor;int family{},slot{};
    juce::String id(const juce::String& x) const {return spectralforge::postSlotID(slot,x);}juce::Label title,reference;juce::ComboBox model;juce::TextButton enabled;
    std::unique_ptr<spectralforge::ExtendedEffectModelSync> modelSync;
    std::unique_ptr<juce::ParameterAttachment> enabledAttachment;std::vector<std::unique_ptr<NativeControlView>> controls;
    void timerCallback() override {if(family==1 && !controls.empty()){controls.front()->slider.setEnabled(processor.parameters().getRawParameterValue(id("delaysync"))->load()<.5f);controls.front()->slider.updateText();}}
    void refreshModel(){const int selected=juce::jmax(0,model.getSelectedId()-1);reference.setText(juce::String::fromUTF8(spectralforge::modelInfo(family,selected).character),juce::dontSendNotification);reference.setTooltip(reference.getText());for(auto& control:controls)control->slider.getProperties().set("knobStyle",spectralforge::art::rackStyle(family,selected).knobStyle);repaint();}
};
