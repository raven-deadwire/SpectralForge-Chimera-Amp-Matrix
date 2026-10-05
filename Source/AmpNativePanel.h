#pragma once
#include "PluginProcessor.h"
#include "AmpNativeParameters.h"
#include "NativeControlView.h"
#include "HardwareArtwork.h"
#include "AmpSelector.h"
#include "UIRefresh.h"

class AmpNativePanel final : public juce::Component {
public:
    AmpNativePanel(ChimeraProcessor& owner,int laneIndex,bool expanded=false):processor(owner),lane(laneIndex),detailed(expanded) {
        setComponentID("ampNativePanel"+juce::String(lane+1));
        addAndMakeVisible(channel);addAndMakeVisible(input);addAndMakeVisible(channelLabel);addAndMakeVisible(inputLabel);addAndMakeVisible(viewport);
        channel.setComponentID("ampChannel"+juce::String(lane+1));input.setComponentID("ampInput"+juce::String(lane+1));
        channel.setName("Amp channel");input.setName("Amp input route");
        for(auto* label:{&channelLabel,&inputLabel}) {label->setFont(juce::FontOptions(9.f));label->setColour(juce::Label::textColourId,juce::Colour(0xffa4a79f));}
        channelLabel.setText("CHANNEL",juce::dontSendNotification);inputLabel.setText("INPUT",juce::dontSendNotification);
        viewport.setViewedComponent(&content,false);viewport.setScrollBarsShown(true,false);viewport.setScrollBarThickness(9);
        addAndMakeVisible(expand);expand.setButtonText("ALL");expand.setComponentID("ampExpand"+juce::String(lane+1));expand.setTooltip("Open the full amplifier panel");expand.setVisible(!detailed);
        expand.onClick=[this]{if(dialog){dialog->toFront(true);return;}auto* full=new AmpNativePanel(processor,lane,true);full->setLookAndFeel(&getLookAndFeel());full->setSize(920,480);full->refresh();juce::DialogWindow::LaunchOptions options;options.content.setOwned(full);options.dialogTitle=juce::String::fromUTF8(spectralforge::ampInfo(model).name)+" / "+juce::String::fromUTF8(spectralforge::ampInfo(model).reference);options.dialogBackgroundColour=juce::Colour(0xff171b1b);options.useNativeTitleBar=true;options.escapeKeyTriggersCloseButton=true;options.resizable=false;options.componentToCentreAround=this;dialog=options.launchAsync();};
        channel.onChange=[this]{channel.acceptSelection();processor.setAmpChannel(lane,channel.getSelectedId()-1);refresh();};
        input.onChange=[this]{input.acceptSelection();processor.setAmpNativeRoute(lane,input.getSelectedId()-1);};
        addAndMakeVisible(resetChannel);resetChannel.setButtonText("RESET CHANNEL");resetChannel.setComponentID("ampResetChannel"+juce::String(lane+1));
        resetChannel.setTooltip("Restore this channel's factory voice and knobs. Other channels, PRE, cabinets and POST are kept.");
        resetChannel.onClick=[this]{
            if(model!=spectralforge::firstOriginalAmpModel)return;
            const auto set=[this](const juce::String& id,float value){if(auto* p=processor.parameters().getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(value));p->endChangeGesture();}};
            const auto voice=spectralforge::original::channelState(currentChannel);
            for(size_t c=0;c<spectralforge::original::controlCount;++c)set(spectralforge::ampNativeControlID(context,model,int(c),currentChannel),voice.values[c]);
            set(spectralforge::originalResponseID(context,currentChannel),1);processor.activateNativeAmp(lane);
        };
    }
    ~AmpNativePanel() override {if(dialog)delete dialog.getComponent();setLookAndFeel(nullptr);}
    // The ALL window has independent visibility from its owning tab.
    void refreshIfNeeded() {
        if(dialog)if(auto* full=dynamic_cast<AmpNativePanel*>(dialog->getContentComponent()))full->refreshIfNeeded();
        if(!spectralforge::ui::visible(*this)){refreshKey.dirty=true;return;}
        const std::array<int,5> key{{(int)processor.parameters().getRawParameterValue("mode")->load(),
            (int)processor.parameters().getRawParameterValue("dualtype")->load(),processor.selectedAmpModel(lane),
            processor.selectedAmpChannel(lane),processor.selectedAmpNativeRoute(lane)}};
        if(refreshKey.update(key))refresh();
    }
    uint64_t stateRefreshCount() const noexcept {return refreshCount;}
    void refresh() {
        ++refreshCount;
        const int mode=(int)processor.parameters().getRawParameterValue("mode")->load();
        const bool split=mode==2 || (mode==1 && processor.parameters().getRawParameterValue("dualtype")->load()>.5f);
        const int nextContext=spectralforge::ampNativeContext(mode,lane),nextModel=processor.selectedAmpModel(lane);
        const auto& panel=spectralforge::ampNativePanel(nextModel);
        channel.setTooltip(nextModel==spectralforge::firstOriginalAmpModel ? "Each channel remembers its own knobs. First selection uses defaults; switching back restores your edits. RESET CHANNEL is explicit." : "Select the amplifier channel.");
        resetChannel.setVisible(detailed && nextModel==spectralforge::firstOriginalAmpModel);
        if(nextContext!=context || nextModel!=model || split!=currentSplit || (nextModel==spectralforge::firstOriginalAmpModel && currentChannel!=processor.selectedAmpChannel(lane))) {
            context=nextContext;model=nextModel;currentSplit=split;lowOverview=!detailed && mode==2 && lane==0;currentChannel=-1;controls.clear();
            channel.clear(juce::dontSendNotification);input.clear(juce::dontSendNotification);
            for(size_t i=0;i<panel.channels.size();++i)channel.addItem(juce::String::fromUTF8(panel.channels[i]),(int)i+1);
            for(size_t i=0;i<panel.routes.size();++i)input.addItem(juce::String::fromUTF8(panel.routes[i]),(int)i+1);
            channel.resetSyncExplicit(processor.selectedAmpChannel(lane)+1);input.resetSyncExplicit(processor.selectedAmpNativeRoute(lane)+1);
            channel.setVisible(!lowOverview && panel.channels.size()>1);channelLabel.setVisible(!lowOverview && panel.channels.size()>1);
            input.setVisible(!lowOverview && panel.routes.size()>1);inputLabel.setVisible(!lowOverview && panel.routes.size()>1);
            for(size_t i=0;i<panel.controls.size();++i) {
                auto control=std::make_unique<NativeControlView>();const auto& spec=panel.controls[i];juce::StringArray options;
                for(const auto* option:spec.options)options.add(juce::String::fromUTF8(option));
                control->bind(processor.parameters(),spectralforge::ampNativeControlID(context,model,(int)i,processor.selectedAmpChannel(lane)),juce::String::fromUTF8(spec.label),juce::String::fromUTF8(spec.group),
                              spec.kind==spectralforge::AmpNativeControlKind::knob?0:spec.kind==spectralforge::AmpNativeControlKind::choice?1:2,
                              options,spec.minimum,spec.maximum,spec.kind==spectralforge::AmpNativeControlKind::knob?.001:1.,spectralforge::art::headStyle(model).knobStyle);
                if(spec.kind==spectralforge::AmpNativeControlKind::knob) {
                    if(model==spectralforge::firstOriginalAmpModel && i==size_t(spectralforge::original::Control::midFrequency)){control->slider.setTextValueSuffix(" Hz");control->slider.setNumDecimalPlacesToDisplay(0);control->slider.setSkewFactorFromMidPoint(850);}
                    else control->showHardwarePosition();
                }
                control->activate=[this]{processor.activateNativeAmp(lane);};content.addAndMakeVisible(*control);controls.push_back(std::move(control));
            }
            for(int i=0;i<2;++i) {
                auto control=std::make_unique<NativeControlView>();
                control->bind(processor.parameters(),i?spectralforge::ampNativeOutputLevelID(context):spectralforge::ampNativeInputTrimID(context),i?"OUTPUT LEVEL":"INPUT TRIM","CHIMERA",0,{},-24,24,.01,3);
                control->slider.setTextValueSuffix(" dB");control->activate=[this]{processor.activateNativeAmp(lane);};content.addAndMakeVisible(*control);controls.push_back(std::move(control));
            }
            if(model==3 || model==15) {
                auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),spectralforge::ampNativeSoloID(context),"SOLO FOOTSWITCH","CHIMERA",2,{"OFF","ON"},0,1,1,3);
                control->activate=[this]{processor.activateNativeAmp(lane);};content.addAndMakeVisible(*control);controls.push_back(std::move(control));
            }
            if(split) {
                auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),"bandtone"+juce::String(lane+1),"BAND TONE","CHIMERA",0,{},-12,12,.01,3);
                control->slider.setTextValueSuffix(" dB");content.addAndMakeVisible(*control);controls.push_back(std::move(control));
            }
            if(mode==2 && lane==0)for(int i=0;i<2;++i) {
                auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),i?"lowampmix":"lowcomp",i?"DI / AMP MIX":"LOW DI COMP","CHIMERA",0,{},0,1,.001,3);
                if(i){control->slider.textFromValueFunction=[](double v){return juce::String(juce::roundToInt(v*100))+"% AMP";};control->slider.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()/100.;};control->slider.updateText();}
                content.addAndMakeVisible(*control);controls.push_back(std::move(control));
            }
            viewport.setViewPosition(0,0);
        }
        channel.syncSelectedId(processor.selectedAmpChannel(lane)+1);input.syncSelectedId(processor.selectedAmpNativeRoute(lane)+1);
        const int nextChannel=processor.selectedAmpChannel(lane);
        if(currentChannel!=nextChannel) {
            currentChannel=nextChannel;for(size_t i=0;i<controls.size();++i) {
                const auto id=controls[i]->getComponentID();
                const bool frontControl=id=="bandtone1_control" || id=="lowcomp_control" || id=="lowampmix_control";
                controls[i]->setVisible(lowOverview?frontControl:i>=panel.controls.size() || spectralforge::ampNativeControlVisible(model,(int)i,currentChannel));
            }
            viewport.setViewPosition(0,0);resized();
        }
    }
    void resized() override {
        const int shown=(channel.isVisible()?1:0)+(input.isVisible()?1:0);
        const int half=(getWidth()-(resetChannel.isVisible()?132:detailed?0:42))/juce::jmax(1,shown);
        resetChannel.setBounds(getWidth()-129,14,126,24);
        channelLabel.setBounds(3,0,half-7,13);inputLabel.setBounds((channel.isVisible()?half:0)+3,0,half-7,13);
        channel.setBounds(3,14,half-8,24);input.setBounds((channel.isVisible()?half:0)+3,14,half-8,24);
        expand.setBounds(getWidth()-41,shown?14:0,38,24);
        const int header=shown?43:detailed?0:28;
        viewport.setBounds(0,header,getWidth(),juce::jmax(0,getHeight()-header));
        const int available=juce::jmax(100,getWidth()-11),columns=juce::jmax(2,available/110),cell=available/columns;
        int visible=0;for(auto& control:controls)if(control->isVisible()) {control->setBounds((visible%columns)*cell,(visible/columns)*103,cell-3,100);++visible;}
        content.setSize(available,juce::jmax(viewport.getHeight(),((visible+columns-1)/columns)*103));
    }
private:
    spectralforge::ui::Changed<std::array<int,5>> refreshKey;
    uint64_t refreshCount{};
    ChimeraProcessor& processor;int lane{},context{-1},model{-1},currentChannel{-1};bool detailed{},currentSplit{},lowOverview{};
    juce::TextButton expand,resetChannel;juce::Component::SafePointer<juce::DialogWindow> dialog;
    juce::Label channelLabel,inputLabel;StableAmpComboBox channel,input;
    juce::Viewport viewport;juce::Component content;std::vector<std::unique_ptr<NativeControlView>> controls;
};
