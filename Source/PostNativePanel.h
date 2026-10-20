#pragma once
#include "PluginProcessor.h"
#include "PostNativeCatalog.h"
#include "NativeControlView.h"
#include "AmpSelector.h"
#include "HardwareArtwork.h"
#include "UIRefresh.h"
#include "DynamicsSpectrumPanel.h"

class PostNativePanel final : public juce::Component {
public:
    PostNativePanel(ChimeraProcessor& owner,int sectionIndex,bool expanded=false):processor(owner),section(sectionIndex),detailed(expanded) {
        setComponentID("postNativePanel"+juce::String(section));
        for(auto* label:{&title,&reference,&meterLabel,&sourceStatus}) {addAndMakeVisible(*label);label->setColour(juce::Label::textColourId,juce::Colour(0xffe1dbce));}
        sourceStatus.setComponentID("postCompressorStatus");sourceStatus.setFont(juce::FontOptions(10.5f));sourceStatus.setVisible(detailed && section==0);
        title.setFont(juce::FontOptions(detailed?11.f:15.f,juce::Font::bold));reference.setFont(juce::FontOptions(10.5f));meterLabel.setFont(juce::FontOptions(detailed?11.f:9.f));
        reference.setComponentID("postNativeReference"+juce::String(section));reference.setMinimumHorizontalScale(1.f);
        title.setText(section==0?"BUS COMPRESSOR":section==1?"PREAMPLIFIER":"EQUALIZER",juce::dontSendNotification);
        addAndMakeVisible(model);addAndMakeVisible(bypass);addAndMakeVisible(viewport);addAndMakeVisible(expand);
        expand.setButtonText(section==0?"SPECTRUM / ALL CONTROLS":"ALL");expand.setComponentID("postExpand"+juce::String(section));expand.setTooltip(section==0?"Edit this compressor while viewing its live input/output spectrum":"Open all processor controls");expand.setVisible(!detailed);
        expand.onClick=[this]{openDetails();};
        model.setComponentID(spectralforge::postNativeModelID(section));model.setName(title.getText()+" model");
        for(int m=0;m<3;++m)model.addItem(juce::String::fromUTF8(spectralforge::postNativeModel(section,m).name),m+1);
        bypass.setClickingTogglesState(true);
        if(!detailed){bypass.getProperties().set("pedalPower",true);bypass.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff41654c));bypass.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff353a39));bypass.setColour(juce::TextButton::textColourOffId,juce::Colour(0xffe1dbce));bypass.setColour(juce::TextButton::textColourOnId,juce::Colour(0xffb1b5b3));}
        model.onChange=[this]{model.acceptSelection();processor.activateNativePost(section);if(auto* parameter=processor.parameters().getParameter(spectralforge::postNativeModelID(section))){parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1((float)(model.getSelectedId()-1)));parameter->endChangeGesture();}refresh();};
        bypass.onClick=[this]{processor.activateNativePost(section);if(bypassAttachment)bypassAttachment->setValueAsCompleteGesture(bypass.getToggleState()?1.f:0.f);updateBypass();};
        viewport.setComponentID("postNativeViewport"+juce::String(section));content.setComponentID("postNativeControls"+juce::String(section));
        viewport.setViewedComponent(&content,false);viewport.setScrollBarsShown(true,false);viewport.setScrollBarThickness(9);
        if(detailed && section==0) {
            spectrum=std::make_unique<spectralforge::DynamicsSpectrumPanel>(processor.dynamicsAnalysis(),processor.eqCaptureBoundary());
            spectrum->setComponentID("postCompressorSpectrum");spectrum->setStage(spectralforge::DynamicsStage::postCompressor);
            spectrum->setPostNativeSource(1);
            addAndMakeVisible(*spectrum);
        }
        if(!detailed && section==0)viewport.setVisible(false);
    }
    ~PostNativePanel() override {if(dialog)delete dialog.getComponent();setLookAndFeel(nullptr);}
    int selectedModel() const noexcept {return currentModel<0?0:currentModel;}
    void openDetails() {
        if(detailed){toFront(true);return;}
        if(dialog){dialog->toFront(true);return;}
        // Navigation only. Attachments initialise without enabling native POST
        // or sending parameter gestures; actual control edits retain activation.
        refresh();auto* full=new PostNativePanel(processor,section,true);
        full->setLookAndFeel(&getLookAndFeel());full->setSize(section==0?1000:920,section==0?670:480);full->refresh();
        juce::DialogWindow::LaunchOptions options;options.content.setOwned(full);
        options.dialogTitle=title.getText()+" / "+juce::String::fromUTF8(spectralforge::postNativeModel(section,selectedModel()).name);
        options.dialogBackgroundColour=juce::Colour(0xff171b1b);options.useNativeTitleBar=true;
        options.escapeKeyTriggersCloseButton=true;options.resizable=false;options.componentToCentreAround=this;
        dialog=options.launchAsync();
    }
    void refreshIfNeeded() {
        if(dialog)if(auto* full=dynamic_cast<PostNativePanel*>(dialog->getContentComponent()))full->refreshIfNeeded();
        if(!spectralforge::ui::visible(*this)){refreshKey.dirty=true;return;}
        const int next=(int)processor.parameters().getRawParameterValue(spectralforge::postNativeModelID(section))->load();
        if(refreshKey.update(next))refresh();
        else refreshMeter();
    }
    uint64_t stateRefreshCount() const noexcept {return refreshCount;}
    uint64_t meterRefreshCount() const noexcept {return meterCount;}
    void refresh() {
        ++refreshCount;
        const auto* selected=processor.parameters().getRawParameterValue(spectralforge::postNativeModelID(section));
        const int nextModel=selected?juce::jlimit(0,2,(int)selected->load()):0;
        if(currentModel!=nextModel) {
            currentModel=nextModel;const auto& spec=spectralforge::postNativeModel(section,currentModel);model.resetSyncExplicit(currentModel+1);
            // Provenance stays in the catalog; the public caption explains the processor.
            reference.setText(spectralforge::postNativeDescription(section,currentModel),juce::dontSendNotification);reference.setTooltip(reference.getText());
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
                if(c.kind==spectralforge::PostNativeControlKind::knob) {
                    const juce::String key(c.id);
                    if(section==0&&currentModel==1&&(key=="attack"||key=="release"))control->showHardwarePosition(1,7);
                    else if(section==2&&currentModel==2)control->showHardwarePosition();
                    else if(key.contains("frequency"))control->slider.setTextValueSuffix(" Hz");
                    else if(key.contains("gain")||key=="input"||key=="output"||key=="makeup"||key=="trim")control->slider.setTextValueSuffix(" dB");
                }
                control->setEnabled(c.connected);const auto note=juce::String::fromUTF8(c.note).replace("Digital implementation range; hardware taper uncalibrated.","").trim();const auto tooltip=juce::String::fromUTF8(c.label)+(note.isEmpty()?"":"\n"+note);control->label.setTooltip(tooltip);control->slider.setTooltip(tooltip);control->choice.setTooltip(tooltip);control->toggle.setTooltip(tooltip);
                control->activate=[this]{processor.activateNativePost(section);};content.addAndMakeVisible(*control);controls.push_back(std::move(control));
            }
            for(int i=0;i<2;++i) {
                auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),i?spectralforge::postNativeLevelID(section,currentModel):spectralforge::postNativeTrimID(section,currentModel),i?"OUTPUT LEVEL":"INPUT TRIM","CHIMERA",0,{},-24,24,.01,3);
                control->slider.setTextValueSuffix(" dB");control->activate=[this]{processor.activateNativePost(section);};content.addAndMakeVisible(*control);controls.push_back(std::move(control));
            }
            const auto front=frontControls();
            const auto hardware=spectralforge::art::rackStyle(section+7,currentModel);
            const auto labelInk=hardware.brightFace?juce::Colour(0xff26312c):juce::Colour(0xffe1dbce);
            for(size_t i=0;i<controls.size();++i) {
                auto& control=*controls[i];control.setCompact(!detailed);
                control.setVisible(detailed || (section!=0 && std::find(front.begin(),front.end(),(int)i)!=front.end()));
                if(!detailed) {
                    control.label.setColour(juce::Label::textColourId,labelInk);control.label.setColour(juce::Label::backgroundColourId,hardware.brightFace?juce::Colour(0xffeae5d9).withAlpha(.9f):juce::Colour(0xff0d1010).withAlpha(.88f));
                    control.slider.getProperties().set("brightFace",hardware.brightFace);
                    if(section==2 && currentModel==2 && i<9){const auto id=juce::String(spec.controls[i].id);if(id.startsWith("lf_") || id.startsWith("hf_"))control.label.setText((id.startsWith("lf_")?"LOW ":"HIGH ")+juce::String::fromUTF8(spec.controls[i].label),juce::dontSendNotification);}
                }
            }
            if(spectrum)spectrum->sourceChanged();
            viewport.setViewPosition(0,0);resized();repaint();
        }
        model.syncSelectedId(nextModel+1);
        refreshMeter();
    }
    void refreshMeter() {
        ++meterCount;
        const bool nativeActive=processor.parameters().getRawParameterValue(spectralforge::postNativeModeID(section))->load()>.5f;
        if(spectrum) {
            if(sourceNativeKey.update(nativeActive?1:0)) {
                spectrum->sourceChanged();sourceStatus.setText(nativeActive?"NATIVE COMPRESSOR SOURCE ACTIVE":"NATIVE COMPRESSOR INACTIVE / stored controls",juce::dontSendNotification);
            }
        }
        if(section==0 && !nativeActive) {
            if(meterText.update("SOURCE INACTIVE"))meterLabel.setText("SOURCE INACTIVE",juce::dontSendNotification);
            if(meterUnit.update("inactive"))meterLabel.setTooltip("Stored native compressor controls; the native source is inactive.");
            return;
        }
        juce::String label="OUT ",unit=" dBFS";bool meterOff=false;
        const auto meterChoice=[this](int control){if(const auto* p=processor.parameters().getRawParameterValue(spectralforge::postNativeControlID(section,currentModel,control)))return(int)p->load();return 0;};
        if(section==0){label="GR ";unit=" dB";
            if(currentModel==1){const int mode=meterChoice(6);meterOff=mode==3;if(mode==1 || mode==2){label=mode==1?"+4 ":"+8 ";unit=" VU";}}
            if(currentModel==2){const int mode=meterChoice(3);if(mode!=1){label=mode==0?"+4 ":"+10 ";unit=" VU";}}
        }else if(section==1 && currentModel==2)label=meterChoice(11)?"POST ":"INPUT ";
        const auto caption=meterOff?juce::String("METER OFF"):label+juce::String(processor.postNativeMeter(section),1)+unit;
        if(meterText.update(caption))meterLabel.setText(caption,juce::dontSendNotification);
        if(meterUnit.update(unit))meterLabel.setTooltip(unit==" VU"?"Digital VU reference: +4 dBu = -18 dBFS. Hardware meter calibration is not reproduced.":"Live processor meter");
    }
    void paint(juce::Graphics& g) override {
        if(!detailed) {
            g.setColour(juce::Colour(0xff171b1b).withAlpha(.9f));g.fillRoundedRectangle(getLocalBounds().toFloat(),4);
            spectralforge::art::rack(g,{276,0,(float)getWidth()-276,64},section+7,selectedModel());
            g.setColour(juce::Colour(0xffc4a678));g.fillRect(8,8,2,46);
            g.setColour(bypass.getToggleState()?juce::Colour(0xff3a3a34):juce::Colour(0xffa5c1a0));g.fillEllipse(16,26,5,5);
            g.setColour(juce::Colour(0xff0d1010).withAlpha(.88f));g.fillRoundedRectangle(298,16,112,32,3);return;
        }
        g.setColour(juce::Colour(0xff171b1b));g.fillRoundedRectangle(getLocalBounds().toFloat(),5);
        spectralforge::art::rack(g,{0,0,(float)getWidth(),79},section+7,selectedModel());
        g.setColour(juce::Colour(0xff0d1010).withAlpha(.88f));g.fillRoundedRectangle(8,6,(float)getWidth()-16,64,4);
        g.setColour(juce::Colour(0xff3a3a34));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),5,1);
    }
    void resized() override {
        if(!detailed) {
            title.setBounds(28,2,186,19);model.setBounds(28,23,182,24);reference.setBounds(28,47,182,15);
            bypass.setBounds(221,13,43,25);expand.setBounds(221,42,43,19);meterLabel.setBounds(300,20,108,24);
            if(section==0) {
                expand.setBounds(438,8,juce::jmax(180,getWidth()-452),48);
                viewport.setVisible(false);return;
            }
            viewport.setBounds(438,1,getWidth()-448,62);viewport.setScrollBarsShown(false,false);
            const auto front=frontControls();const int cell=viewport.getWidth()/juce::jmax(1,(int)front.size());
            for(size_t i=0;i<front.size();++i)if(juce::isPositiveAndBelow(front[i],(int)controls.size()))controls[(size_t)front[i]]->setBounds((int)i*cell,0,cell-6,62);
            content.setSize(viewport.getWidth(),62);return;
        }
        title.setBounds(18,8,220,16);model.setBounds(18,26,juce::jmin(310,getWidth()-210),26);
        sourceStatus.setBounds(350,8,430,16);
        reference.setBounds(18,54,getWidth()-36,18);bypass.setBounds(getWidth()-105,22,87,28);meterLabel.setBounds(getWidth()-305,25,190,24);
        // Native windows can be constrained by the display at 125/150% UI
        // scale. Reserve a complete control row before sizing the spectrum.
        // Keep at least 120 px of plot area (91 px are analyzer chrome).
        const int spectrumHeight=juce::jlimit(211,252,getHeight()-84-10-100-8);
        const int controlsTop=section==0?84+spectrumHeight+10:84;
        if(spectrum)spectrum->setBounds(8,84,getWidth()-16,spectrumHeight);
        viewport.setBounds(8,controlsTop,getWidth()-16,getHeight()-controlsTop-8);
        const int available=juce::jmax(200,viewport.getWidth()-11),columns=juce::jmax(3,available/130),cell=available/columns;
        for(size_t i=0;i<controls.size();++i)controls[i]->setBounds(((int)i%columns)*cell,((int)i/columns)*103,cell-4,100);
        content.setSize(available,juce::jmax(viewport.getHeight(),(((int)controls.size()+columns-1)/columns)*103));
    }
private:
    spectralforge::ui::Changed<int> refreshKey;
    spectralforge::ui::Changed<int> sourceNativeKey;
    spectralforge::ui::Changed<juce::String> meterText,meterUnit;
    uint64_t refreshCount{},meterCount{};
    ChimeraProcessor& processor;int section{},currentModel{-1};bool detailed{};StableAmpComboBox model;juce::TextButton bypass,expand;
    juce::Component::SafePointer<juce::DialogWindow> dialog;
    std::unique_ptr<spectralforge::DynamicsSpectrumPanel> spectrum;
    juce::Label title,reference,meterLabel,sourceStatus;juce::Viewport viewport;juce::Component content;
    std::unique_ptr<juce::ParameterAttachment> bypassAttachment;std::vector<std::unique_ptr<NativeControlView>> controls;
    std::vector<int> frontControls() const {
        if(section==0){if(selectedModel()==0)return {0,2,3,4,1};if(selectedModel()==1)return {0,1,2,3,5};return {1,0,2,3};}
        if(section==1){if(selectedModel()==0)return {0,1,2,3};if(selectedModel()==1)return {0,1,2,3,8};return {0,1,3,9,13};}
        if(selectedModel()==0)return {9,6,3,0,15};if(selectedModel()==1)return {3,1,0,5,8};return {0,1,2,4,6};
    }
    void updateBypass(){bypass.setButtonText(detailed?(bypass.getToggleState()?"BYPASSED":"ACTIVE"):(bypass.getToggleState()?"OFF":"ON"));repaint();}
};
