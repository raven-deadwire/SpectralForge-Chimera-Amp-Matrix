#pragma once
#include "PluginProcessor.h"
#include "PostNativeCatalog.h"
#include "NativeControlView.h"
#include "AmpSelector.h"
#include "HardwareArtwork.h"

class PostNativePanel final : public juce::Component {
public:
    PostNativePanel(ChimeraProcessor& owner,int sectionIndex):processor(owner),section(sectionIndex) {
        setComponentID("postNativePanel"+juce::String(section));
        for(auto* label:{&title,&reference,&meterLabel}) {addAndMakeVisible(*label);label->setColour(juce::Label::textColourId,juce::Colour(0xffe1dbce));}
        title.setFont(juce::FontOptions(11.f,juce::Font::bold));reference.setFont(juce::FontOptions(10.5f));meterLabel.setFont(juce::FontOptions(11.f));
        title.setText(section==0?"BUS COMPRESSOR":section==1?"PREAMPLIFIER":"EQUALIZER",juce::dontSendNotification);
        addAndMakeVisible(model);addAndMakeVisible(bypass);addAndMakeVisible(viewport);
        model.setComponentID(spectralforge::postNativeModelID(section));model.setName(title.getText()+" model");
        juce::PopupMenu group;for(int m=0;m<3;++m)group.addItem(m+1,juce::String::fromUTF8(spectralforge::postNativeModel(section,m).name));
        model.getRootMenu()->addSubMenu(title.getText(),group);
        bypass.setClickingTogglesState(true);
        model.onChange=[this]{model.acceptSelection();processor.activateNativePost(section);if(auto* parameter=processor.parameters().getParameter(spectralforge::postNativeModelID(section))){parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1((float)(model.getSelectedId()-1)));parameter->endChangeGesture();}refresh();};
        bypass.onClick=[this]{processor.activateNativePost(section);if(bypassAttachment)bypassAttachment->setValueAsCompleteGesture(bypass.getToggleState()?1.f:0.f);updateBypass();};
        viewport.setViewedComponent(&content,false);viewport.setScrollBarsShown(true,false);viewport.setScrollBarThickness(9);
    }
    int selectedModel() const noexcept {return currentModel<0?0:currentModel;}
    void refresh() {
        const auto* selected=processor.parameters().getRawParameterValue(spectralforge::postNativeModelID(section));
        const int nextModel=selected?juce::jlimit(0,2,(int)selected->load()):0;
        if(currentModel!=nextModel) {
            currentModel=nextModel;const auto& spec=spectralforge::postNativeModel(section,currentModel);model.resetSyncExplicit(currentModel+1);
            reference.setText(juce::String("REFERENCE: ")+juce::String::fromUTF8(spec.reference),juce::dontSendNotification);reference.setTooltip(reference.getText());
            bypassAttachment.reset();bypass.setComponentID(spectralforge::postNativeBypassID(section,currentModel));
            if(auto* parameter=processor.parameters().getParameter(spectralforge::postNativeBypassID(section,currentModel))) {
                bypassAttachment=std::make_unique<juce::ParameterAttachment>(*parameter,[this](float v){bypass.setToggleState(v>.5f,juce::dontSendNotification);updateBypass();});bypassAttachment->sendInitialUpdate();
            }
            controls.clear();
            for(int i=0;i<spec.controlCount;++i) {
                const auto& c=spec.controls[(size_t)i];auto control=std::make_unique<NativeControlView>();
                control->bind(processor.parameters(),spectralforge::postNativeControlID(section,currentModel,i),juce::String::fromUTF8(c.label),c.connected?"PANEL":"HARDWARE ONLY",
                    c.kind==spectralforge::PostNativeControlKind::knob?0:c.kind==spectralforge::PostNativeControlKind::choice?1:2,
                    juce::StringArray::fromTokens(juce::String::fromUTF8(c.options),"|",{}),c.minimum,c.maximum,c.interval,spectralforge::art::rackStyle(section+7,currentModel).knobStyle);
                control->setEnabled(c.connected);const auto note=juce::String::fromUTF8(c.note).replace("Digital implementation range; hardware taper uncalibrated.","").trim();const auto tooltip=juce::String::fromUTF8(c.label)+(note.isEmpty()?"":"\n"+note);control->label.setTooltip(tooltip);control->slider.setTooltip(tooltip);control->choice.setTooltip(tooltip);control->toggle.setTooltip(tooltip);
                control->activate=[this]{processor.activateNativePost(section);};content.addAndMakeVisible(*control);controls.push_back(std::move(control));
            }
            for(int i=0;i<2;++i) {
                auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),i?spectralforge::postNativeLevelID(section,currentModel):spectralforge::postNativeTrimID(section,currentModel),i?"OUTPUT LEVEL":"INPUT TRIM","CHIMERA",0,{},-24,24,.01,3);
                control->slider.setTextValueSuffix(" dB");control->activate=[this]{processor.activateNativePost(section);};content.addAndMakeVisible(*control);controls.push_back(std::move(control));
            }
            viewport.setViewPosition(0,0);resized();repaint();
        }
        model.syncSelectedId(nextModel+1);
        juce::String label="OUT ",unit=" dBFS";bool meterOff=false;
        const auto meterChoice=[this](int control){if(const auto* p=processor.parameters().getRawParameterValue(spectralforge::postNativeControlID(section,currentModel,control)))return(int)p->load();return 0;};
        if(section==0){label="GR ";unit=" dB";
            if(currentModel==1){const int mode=meterChoice(6);meterOff=mode==3;if(mode==1 || mode==2){label=mode==1?"+4 ":"+8 ";unit=" VU";}}
            if(currentModel==2){const int mode=meterChoice(3);if(mode!=1){label=mode==0?"+4 ":"+10 ";unit=" VU";}}
        }else if(section==1 && currentModel==2)label=meterChoice(11)?"POST ":"INPUT ";
        meterLabel.setText(meterOff?"METER OFF":label+juce::String(processor.postNativeMeter(section),1)+unit,juce::dontSendNotification);
        meterLabel.setTooltip(unit==" VU"?"Digital VU reference: +4 dBu = -18 dBFS. Hardware meter calibration is not reproduced.":"Live processor meter");
    }
    void paint(juce::Graphics& g) override {
        g.setColour(juce::Colour(0xff171b1b));g.fillRoundedRectangle(getLocalBounds().toFloat(),5);
        spectralforge::art::rack(g,{0,0,(float)getWidth(),79},section+7,selectedModel());
        g.setColour(juce::Colour(0xff0d1010).withAlpha(.88f));g.fillRoundedRectangle(8,6,(float)getWidth()-16,64,4);
        g.setColour(juce::Colour(0xff3a3a34));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),5,1);
    }
    void resized() override {
        title.setBounds(18,8,220,16);model.setBounds(18,26,juce::jmin(310,getWidth()-210),26);
        reference.setBounds(18,54,getWidth()-36,18);bypass.setBounds(getWidth()-105,22,87,28);meterLabel.setBounds(getWidth()-305,25,190,24);
        viewport.setBounds(8,84,getWidth()-16,getHeight()-92);
        const int available=juce::jmax(200,viewport.getWidth()-11),columns=juce::jmax(3,available/130),cell=available/columns;
        for(size_t i=0;i<controls.size();++i)controls[i]->setBounds(((int)i%columns)*cell,((int)i/columns)*103,cell-4,100);
        content.setSize(available,juce::jmax(viewport.getHeight(),(((int)controls.size()+columns-1)/columns)*103));
    }
private:
    ChimeraProcessor& processor;int section{},currentModel{-1};StableAmpComboBox model;juce::TextButton bypass;
    juce::Label title,reference,meterLabel;juce::Viewport viewport;juce::Component content;
    std::unique_ptr<juce::ParameterAttachment> bypassAttachment;std::vector<std::unique_ptr<NativeControlView>> controls;
    void updateBypass(){bypass.setButtonText(bypass.getToggleState()?"BYPASSED":"ACTIVE");}
};
