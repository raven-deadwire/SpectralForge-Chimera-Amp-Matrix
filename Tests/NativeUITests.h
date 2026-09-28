#pragma once
#include "PluginEditor.h"
#include "AmpNativeParameters.h"
#include "PostNativeCatalog.h"
#include <iostream>
#include <stdexcept>

namespace nativeUITests {
inline void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
inline void settle(int ms=55){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
template<class T=juce::Component> T* find(juce::Component& root,const juce::String& id){
    if(root.getComponentID()==id)if(auto* typed=dynamic_cast<T*>(&root))return typed;
    for(auto* child:root.getChildren())if(auto* result=find<T>(*child,id))return result;return nullptr;
}
inline void set(ChimeraProcessor& processor,const juce::String& id,float value){auto* p=processor.parameters().getParameter(id);require(p!=nullptr,"Native UI parameter missing");p->setValueNotifyingHost(p->convertTo0to1(value));}
inline void tab(juce::Component& root,const char* name){for(auto* child:root.getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child);b && b->getButtonText()==name){b->triggerClick();settle();return;}throw std::runtime_error("Native UI page tab missing");}
inline void snapshot(juce::Component& root,const juce::File& directory,const char* name){auto stream=directory.getChildFile(juce::String(name)+".png").createOutputStream();juce::PNGImageFormat png;require(stream && png.writeImageToStream(root.createComponentSnapshot(root.getLocalBounds()),*stream),"Native UI screenshot failed");}
inline void run(const juce::File& directory){
    auto storage=std::make_unique<ChimeraProcessor>();auto& processor=*storage;ChimeraEditor editor(processor);auto* canvas=editor.findChildWithID("surface");require(canvas!=nullptr,"Native UI canvas missing");
    for(const auto* id:{"legacyPreControls","boardEnabled"})require(find(*canvas,id)==nullptr,"Obsolete Legacy/engine-selection control remains in native editor");
    set(processor,"mode",0);tab(*canvas,"RIGS");
    auto* selector=find<AmpSelector>(*canvas,"ampSelect1");require(selector!=nullptr,"Native UI amp selector missing");
    int channelCases=0,controlCases=0;
    for(int scale=0;scale<2;++scale){editor.setSize(scale?885:1180,scale?585:780);settle();
        for(int model=0;model<spectralforge::ampModelCount;++model){
            require(selector->selectMenuResult(model+1),"Native model selection failed");settle();
            auto* panel=find<AmpNativePanel>(*canvas,"ampNativePanel1");require(panel!=nullptr,"Native amplifier panel missing");auto* channel=find<juce::ComboBox>(*panel,"ampChannel1");
            const auto& spec=spectralforge::ampNativePanel(model);
            require(channel && channel->isVisible()==(spec.channels.size()>1),"Native UI single-channel selector not hidden");
            auto* reference=find<juce::Label>(*canvas,"ampreference1");require(reference && reference->isVisible() && reference->getText()==juce::String("REFERENCE: ")+spectralforge::ampInfo(model).reference,"Native original reference missing outside alias list");
            require(selector->getText()==spectralforge::ampInfo(model).name,"Native amp list exposes original reference name");
            require(canvas->getLocalBounds().contains(panel->getBounds()),"Native amp panel exceeds editor bounds");
            for(int ch=0;ch<(int)spec.channels.size();++ch){
                channel->setSelectedId(ch+1,juce::sendNotificationSync);settle(45);require(processor.selectedAmpChannel(0)==ch,"Original amplifier channel callback not connected");++channelCases;
                for(int control=0;control<(int)spec.controls.size();++control){
                    const auto id=spectralforge::ampNativeControlID(0,model,control);auto* view=find<NativeControlView>(*panel,id+"_control");
                    require(view!=nullptr,"Original amplifier control was not constructed");
                    require(view->isVisible()==spectralforge::ampNativeControlVisible(model,control,ch),"Wrong channel-specific controls visible");
                    require(processor.parameters().getParameter(id)!=nullptr,"Original amplifier control has no host/audio parameter");++controlCases;
                }
            }
            // A visible native knob must both activate the compatibility bank
            // and write its actual host parameter, including at 75% scale.
            const int ch=processor.selectedAmpChannel(0);
            for(int c=0;c<(int)spec.controls.size();++c)if(spec.controls[(size_t)c].kind==spectralforge::AmpNativeControlKind::knob && spectralforge::ampNativeControlVisible(model,c,ch)){
                const auto id=spectralforge::ampNativeControlID(0,model,c);auto* knob=find<juce::Slider>(*panel,id);require(knob!=nullptr,"Native knob missing");
                set(processor,spectralforge::ampNativeEnabledID(0),0);const double value=knob->getValue()>.6?.23:.77;knob->setValue(value,juce::sendNotificationSync);
                require(std::abs(processor.parameters().getRawParameterValue(id)->load()-value)<.002,"Native knob did not reach its host parameter");
                require(processor.parameters().getRawParameterValue(spectralforge::ampNativeEnabledID(0))->load()>.5f,"Native gesture did not activate the model bank");break;
            }
        }
    }
    editor.setSize(1180,780);processor.setAmpModel(0,15);settle();snapshot(editor,directory,"Native-Amp-Classic");
    processor.setAmpModel(0,4);settle();auto* expand=find<juce::TextButton>(*canvas,"ampExpand1");require(expand && expand->isVisible(),"Expanded amplifier panel is inaccessible");expand->triggerClick();settle(90);
    juce::DialogWindow* opened=nullptr;
    for(int i=0;i<juce::TopLevelWindow::getNumTopLevelWindows();++i)if(auto* window=dynamic_cast<juce::DialogWindow*>(juce::TopLevelWindow::getTopLevelWindow(i));window && dynamic_cast<AmpNativePanel*>(window->getContentComponent())){opened=window;break;}
    require(opened!=nullptr,"ALL button failed to open the original-control panel");require(&opened->getContentComponent()->getLookAndFeel()==&editor.getLookAndFeel(),"Expanded amplifier controls lost the editor style");snapshot(*opened->getContentComponent(),directory,"Native-Amp-Expanded");opened->exitModalState(0);settle();
    set(processor,"mode",2);settle();for(int lane=0;lane<3;++lane)processor.setAmpModel(lane,lane==0?18:lane==1?15:22);settle();snapshot(editor,directory,"Native-Amp-Matrix");
    tab(*canvas,"POST");int postControlCases=0;
    for(int section=0;section<3;++section){auto* rail=find<juce::TextButton>(*canvas,"postModule"+juce::String(section));require(rail!=nullptr,"Native POST section rail missing");rail->triggerClick();settle();
        auto* panel=find<PostNativePanel>(*canvas,"postNativePanel"+juce::String(section));require(panel && panel->isVisible(),"Native POST panel not visible");
        auto* modelBox=find<juce::ComboBox>(*panel,spectralforge::postNativeModelID(section));require(modelBox!=nullptr,"Native POST model selector missing");
        for(int model=0;model<3;++model){modelBox->setSelectedId(model+1,juce::sendNotificationSync);settle();const auto& spec=spectralforge::postNativeModel(section,model);
            require(modelBox->getText()==spec.name,"POST model list contains a hardware reference");
            require((int)processor.parameters().getRawParameterValue(spectralforge::postNativeModelID(section))->load()==model,"POST model did not reach audio state");
            for(int c=0;c<spec.controlCount;++c){const auto id=spectralforge::postNativeControlID(section,model,c);auto* view=find<NativeControlView>(*panel,id+"_control");require(view && view->isVisible(),"POST original control missing");require(view->isEnabled()==spec.controls[(size_t)c].connected,"Physical-only POST control is falsely interactive");
                const auto& control=spec.controls[(size_t)c];if(control.connected){float target=0;
                    if(control.kind==spectralforge::PostNativeControlKind::knob){target=control.minimum+(control.maximum-control.minimum)*.61f;auto* slider=find<juce::Slider>(*view,id);require(slider!=nullptr,"POST knob widget missing");slider->setValue(target,juce::sendNotificationSync);target=(float)slider->getValue();}
                    else if(control.kind==spectralforge::PostNativeControlKind::choice){auto* choice=find<juce::ComboBox>(*view,id);require(choice!=nullptr,"POST choice widget missing");target=(float)((choice->getSelectedId())%choice->getNumItems());choice->setSelectedId((int)target+1,juce::sendNotificationSync);}
                    else {auto* button=find<juce::TextButton>(*view,id);require(button!=nullptr,"POST switch widget missing");target=button->getToggleState()?0.f:1.f;button->triggerClick();settle(15);}
                    require(std::abs(processor.parameters().getRawParameterValue(id)->load()-target)<.02f,"POST original control callback did not reach audio state");
                }++postControlCases;}
            auto* bypass=find<juce::TextButton>(*panel,spectralforge::postNativeBypassID(section,model));require(bypass!=nullptr,"Native POST bypass missing");const float before=processor.parameters().getRawParameterValue(spectralforge::postNativeBypassID(section,model))->load();bypass->triggerClick();settle();require((processor.parameters().getRawParameterValue(spectralforge::postNativeBypassID(section,model))->load()>.5f)!=(before>.5f),"Native POST bypass callback not connected");
            if((section==0 && model==1)||(section==1 && model==2)||(section==2 && model==0))snapshot(editor,directory,section==0?"Native-Post-Bus":section==1?"Native-Post-Preamp":"Native-Post-EQ");
        }
    }
    std::cout<<"PASS native UI: all 23 amplifier panels at 100/75%, "<<channelCases<<" channel cases, "<<controlCases<<" channel/control visibility cases, 9 POST panels / "<<postControlCases<<" controls, expanded panel and real parameter callbacks\n";
}
}
inline void runNativeUITests(const juce::File& directory){nativeUITests::run(directory);}
