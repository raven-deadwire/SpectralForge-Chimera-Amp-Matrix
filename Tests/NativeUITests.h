#pragma once
#include "GateUITests.h"
#include "PluginEditor.h"
#include "RigViewTestHelpers.h"
#include "AmpNativeParameters.h"
#include "PostNativeCatalog.h"
#include "NativeMessageLoop.h"
#include <iostream>
#include <stdexcept>

namespace nativeUITests {
inline void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
inline void settle(int ms=55){require(chimeraTest::dispatchFor(ms),"Native UI loop exited");}
template<class T=juce::Component> T* find(juce::Component& root,const juce::String& id){
    if(root.getComponentID()==id)if(auto* typed=dynamic_cast<T*>(&root))return typed;
    for(auto* child:root.getChildren())if(auto* result=find<T>(*child,id))return result;return nullptr;
}
inline void set(ChimeraProcessor& processor,const juce::String& id,float value){auto* p=processor.parameters().getParameter(id);require(p!=nullptr,"Native UI parameter missing");p->setValueNotifyingHost(p->convertTo0to1(value));}
inline void tab(juce::Component& root,const char* name){for(auto* child:root.getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child);b && b->getButtonText()==name){b->triggerClick();settle();return;}throw std::runtime_error("Native UI page tab missing");}
inline void snapshot(juce::Component& root,const juce::File& directory,const char* name){auto stream=directory.getChildFile(juce::String(name)+".png").createOutputStream();juce::PNGImageFormat png;require(stream && png.writeImageToStream(root.createComponentSnapshot(root.getLocalBounds()),*stream),"Native UI screenshot failed");}
inline void views(juce::Component& root,std::vector<NativeControlView*>& result) {
    if(auto* view=dynamic_cast<NativeControlView*>(&root))result.push_back(view);
    for(auto* child:root.getChildren())views(*child,result);
}
inline juce::DialogWindow* dialog(const juce::String& contentID) {
    for(int i=0;i<juce::TopLevelWindow::getNumTopLevelWindows();++i)
        if(auto* window=dynamic_cast<juce::DialogWindow*>(juce::TopLevelWindow::getTopLevelWindow(i)))
            if(auto* content=window->getContentComponent();content && content->getComponentID()==contentID)return window;
    return nullptr;
}
inline juce::Component::SafePointer<juce::DialogWindow> open(juce::Component& root,const juce::String& buttonID,const juce::String& contentID) {
    auto* button=find<juce::TextButton>(root,buttonID);
    require(button && button->isVisible() && button->isEnabled(),"ALL button is inaccessible");
    button->triggerClick();settle(90);
    auto* result=dialog(contentID);require(result!=nullptr,"ALL button failed to open the full control panel");return result;
}
inline void close(juce::Component::SafePointer<juce::DialogWindow> window) {
    require(window!=nullptr,"Expanded panel disappeared before closing");window->exitModalState(0);
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+2000;
    while(window && juce::Time::getMillisecondCounterHiRes()<deadline)settle(10);
    require(window==nullptr,"Closing ALL left a live dialog or dangling panel");
}
inline void inside(juce::Component& root,juce::Component& control,const char* message) {
    require(root.getLocalBounds().contains(root.getLocalArea(&control,control.getLocalBounds())),message);
}
inline void postModelPresentation(PostNativePanel& panel,int section,int selected) {
    auto* model=find<juce::ComboBox>(panel,spectralforge::postNativeModelID(section));
    require(model && model->getRootMenu(),"POST model menu missing");
    std::array<int,3> found{};int items=0;
    for(juce::PopupMenu::MenuItemIterator it(*model->getRootMenu(),false);it.next();) {
        const auto& item=it.getItem();
        require(item.subMenu==nullptr && !item.isSectionHeader && !item.isSeparator,"Single-family POST menu has an unnecessary category step");
        require(item.itemID>=1 && item.itemID<=3 && item.isEnabled,"POST model menu contains an invalid or unavailable model");
        require(item.text==juce::String::fromUTF8(spectralforge::postNativeModel(section,item.itemID-1).name),"POST dropdown must contain only product aliases");
        ++found[(size_t)item.itemID-1];++items;
    }
    require(items==3 && found[0]==1 && found[1]==1 && found[2]==1,"Flat POST menu omits or duplicates a model");
    constexpr std::array<const char*,9> names{{"SSL G Series Compressor","Universal Audio 1176LN","Teletronix LA-2A","Neve 1073","Avalon V5","Focusrite ISA One","SSL E Series EQ","Neve 1073","Pultec EQP-1A"}};
    auto* reference=find<juce::Label>(panel,"postNativeReference"+juce::String(section));
    require(reference && reference->isVisible() && reference->getText()==names[(size_t)(section*3+selected)],"POST caption must show only the short original model name, without a reference prefix or description");
    inside(panel,*reference,"POST model caption is outside its rack panel");
    juce::GlyphArrangement glyphs;glyphs.addLineOfText(reference->getFont(),reference->getText(),0,0);
    require(glyphs.getBoundingBox(0,-1,true).getWidth()<=reference->getWidth()-10,"Short POST model caption still clips in the rack row");
    for(auto* child:panel.getChildren())if(child!=reference && child->isVisible())
        require(!reference->getBounds().intersects(child->getBounds()),"POST model caption overlaps another visible control");
}
inline void lowOverview(ChimeraProcessor& processor,ChimeraEditor& editor,juce::Component& canvas,const juce::File& directory) {
    set(processor,"mode",2);settle();for(int lane=0;lane<3;++lane)processor.setAmpModel(lane,lane==0?4:lane==1?15:22);settle();
    rigViewTests::showControls(canvas);
    for(int scale=0;scale<2;++scale) {
        editor.setSize(scale?885:1180,scale?585:780);settle();
        auto* panel=find<AmpNativePanel>(canvas,"ampNativePanel1");require(panel!=nullptr,"Matrix LOW panel missing");
        std::vector<NativeControlView*> controls;views(*panel,controls);std::vector<NativeControlView*> shown;
        for(auto* view:controls)if(view->isVisible())shown.push_back(view);
        const std::array<const char*,3> ids{"bandtone1","lowcomp","lowampmix"};
        require(shown.size()==ids.size(),"Matrix LOW overview must expose exactly BAND TONE, LOW DI COMP and DI / AMP MIX");
        for(size_t i=0;i<ids.size();++i) {
            require(shown[i]->getComponentID()==juce::String(ids[i])+"_control","Matrix LOW priority controls are in the wrong order");
            inside(*panel,*shown[i],"Matrix LOW priority control is clipped");inside(editor,*shown[i],"Matrix LOW priority control exceeds scaled editor");
            auto* slider=find<juce::Slider>(*shown[i],ids[i]);require(slider!=nullptr,"Matrix LOW priority knob missing");
            const double value=i==0?(scale?-3.5:4.5):(scale?.31:.64);slider->setValue(value,juce::sendNotificationSync);
            require(std::abs(processor.parameters().getRawParameterValue(ids[i])->load()-value)<.002,"Matrix LOW priority knob did not reach its host parameter");
        }
        require(!find<juce::ComboBox>(*panel,"ampChannel1")->isVisible() && !find<juce::ComboBox>(*panel,"ampInput1")->isVisible(),"Matrix LOW overview exposes amplifier selectors before ALL");
        snapshot(editor,directory,scale?"Native-Amp-Matrix-75pct":"Native-Amp-Matrix");
        auto window=open(*panel,"ampExpand1","ampNativePanel1");auto* full=dynamic_cast<AmpNativePanel*>(window->getContentComponent());
        require(full && &full->getLookAndFeel()==&editor.getLookAndFeel(),"Matrix LOW ALL lost the native amplifier panel/style");
        const int model=processor.selectedAmpModel(0),context=spectralforge::ampNativeContext(2,0);const auto& spec=spectralforge::ampNativePanel(model);
        auto* channel=find<juce::ComboBox>(*full,"ampChannel1");require(channel && channel->isVisible()==(spec.channels.size()>1),"Matrix LOW ALL did not restore the original channel selector");
        for(int ch=0;ch<(int)spec.channels.size();++ch) {
            channel->setSelectedId(ch+1,juce::sendNotificationSync);settle(40);
            require(processor.selectedAmpChannel(0)==ch,"Matrix LOW ALL channel callback is disconnected");
            for(int control=0;control<(int)spec.controls.size();++control) {
                auto* view=find<NativeControlView>(*full,spectralforge::ampNativeControlID(context,model,control,ch)+"_control");
                require(view && view->isVisible()==spectralforge::ampNativeControlVisible(model,control,ch),"Matrix LOW ALL omitted an original amplifier control");
            }
        }
        bool edited=false;
        for(int control=0;control<(int)spec.controls.size() && !edited;++control)if(spec.controls[(size_t)control].kind==spectralforge::AmpNativeControlKind::knob && spectralforge::ampNativeControlVisible(model,control,processor.selectedAmpChannel(0))) {
            const auto id=spectralforge::ampNativeControlID(context,model,control,processor.selectedAmpChannel(0));auto* slider=find<juce::Slider>(*full,id);
            const auto& range=processor.parameters().getParameter(id)->getNormalisableRange();slider->setValue(range.start+(range.end-range.start)*.57,juce::sendNotificationSync);
            require(std::abs(processor.parameters().getRawParameterValue(id)->load()-slider->getValue())<.002,"Matrix LOW ALL native knob did not reach the selected bank");edited=true;
        }
        require(edited,"Matrix LOW ALL provided no editable original knob");
        for(const auto* id:ids)require(find<NativeControlView>(*full,juce::String(id)+"_control")->isVisible(),"Matrix LOW shared controls missing from ALL");
        snapshot(*full,directory,scale?"Native-Amp-Matrix-ALL-75pct":"Native-Amp-Matrix-ALL");close(window);
    }
    editor.setSize(1180,780);settle();
}
inline int postPanels(ChimeraProcessor& processor,ChimeraEditor& editor,juce::Component& canvas,const juce::File& directory) {
    tab(canvas,"POST");int cases=0;
    for(int scale=0;scale<2;++scale) {
        editor.setSize(scale?885:1180,scale?585:780);settle();
        for(int section=0;section<6;++section) {
            require(find(canvas,"postModule"+juce::String(section))==nullptr,"Obsolete POST module rail remains in the rack overview");
            auto* expand=find<juce::TextButton>(canvas,"postExpand"+juce::String(section));require(expand && expand->isVisible(),"A POST rack row has no ALL button");inside(editor,*expand,"Rack ALL button exceeds scaled editor bounds");
        }
        snapshot(editor,directory,scale?"Native-Post-Rack-75pct":"Native-Post-Rack");
        for(int section=0;section<3;++section) {
            auto* compact=find<PostNativePanel>(canvas,"postNativePanel"+juce::String(section));require(compact && compact->isVisible(),"POST rack does not show all three native modules together");
            require(compact->getHeight()==64,"POST rack row no longer matches the compact hardware layout");inside(editor,*compact,"Compact POST module exceeds scaled editor bounds");
            auto* compactModel=find<juce::ComboBox>(*compact,spectralforge::postNativeModelID(section));require(compactModel!=nullptr,"Compact POST model selector missing");
            for(int model=0;model<3;++model) {
                compactModel->setSelectedId(model+1,juce::sendNotificationSync);settle();const auto& spec=spectralforge::postNativeModel(section,model);
                require(compactModel->getText()==spec.name,"POST model list contains a hardware reference");
                postModelPresentation(*compact,section,model);
                require((int)processor.parameters().getRawParameterValue(spectralforge::postNativeModelID(section))->load()==model,"POST model did not reach audio state");
                std::vector<NativeControlView*> overview;views(*compact,overview);int shown=0;
                for(auto* view:overview)if(view->isVisible()){++shown;inside(*compact,*view,"Compact POST control is clipped");inside(editor,*view,"Compact POST control exceeds scaled editor bounds");}
                require(shown>=4 && shown<=5,"POST overview must show a compact group of primary controls");
                auto window=open(*compact,"postExpand"+juce::String(section),compact->getComponentID());auto* panel=dynamic_cast<PostNativePanel*>(window->getContentComponent());
                require(panel && &panel->getLookAndFeel()==&editor.getLookAndFeel(),"Expanded POST lost its original control panel/style");
                auto* modelBox=find<juce::ComboBox>(*panel,spectralforge::postNativeModelID(section));require(modelBox && modelBox->getSelectedId()==model+1,"POST ALL opened the wrong model");
                postModelPresentation(*panel,section,model);
                std::vector<NativeControlView*> all;views(*panel,all);require((int)all.size()==spec.controlCount+2,"POST ALL omitted original controls or software trim/level");
                for(auto* view:all){require(view->isVisible(),"POST ALL contains a hidden original control");inside(*panel,*view,"POST ALL control exceeds the expanded panel");}
                for(int c=0;c<spec.controlCount;++c) {
                    const auto id=spectralforge::postNativeControlID(section,model,c);auto* view=find<NativeControlView>(*panel,id+"_control");
                    require(view && view->isVisible(),"POST original control missing from ALL");require(view->label.getText()==juce::String::fromUTF8(spec.controls[(size_t)c].label),"POST control label has invalid UTF-8 decoding");
                    require(view->isEnabled()==spec.controls[(size_t)c].connected,"Physical-only POST control is falsely interactive");const auto& control=spec.controls[(size_t)c];
                    if(control.kind==spectralforge::PostNativeControlKind::choice) {
                        const auto options=juce::StringArray::fromTokens(juce::String::fromUTF8(control.options),"|",{});require(view->choice.getNumItems()==options.size(),"POST choice option count mismatch");
                        for(int option=0;option<options.size();++option)require(view->choice.getItemText(option)==options[option],"POST choice option has invalid UTF-8 decoding");
                    }
                    if(control.connected) {
                        float target=0;
                        if(control.kind==spectralforge::PostNativeControlKind::knob){target=control.minimum+(control.maximum-control.minimum)*.61f;auto* slider=find<juce::Slider>(*view,id);require(slider!=nullptr,"POST knob widget missing");slider->setValue(target,juce::sendNotificationSync);target=(float)slider->getValue();}
                        else if(control.kind==spectralforge::PostNativeControlKind::choice){auto* choice=find<juce::ComboBox>(*view,id);require(choice!=nullptr,"POST choice widget missing");target=(float)(choice->getSelectedId()%choice->getNumItems());choice->setSelectedId((int)target+1,juce::sendNotificationSync);}
                        else {auto* button=find<juce::TextButton>(*view,id);require(button!=nullptr,"POST switch widget missing");target=button->getToggleState()?0.f:1.f;button->triggerClick();settle(15);}
                        require(std::abs(processor.parameters().getRawParameterValue(id)->load()-target)<.02f,"POST ALL original control callback did not reach audio state");
                    }
                    ++cases;
                }
                auto* bypass=find<juce::TextButton>(*panel,spectralforge::postNativeBypassID(section,model));require(bypass!=nullptr,"Native POST bypass missing");
                const float before=processor.parameters().getRawParameterValue(spectralforge::postNativeBypassID(section,model))->load();bypass->triggerClick();settle();
                require((processor.parameters().getRawParameterValue(spectralforge::postNativeBypassID(section,model))->load()>.5f)!=(before>.5f),"Native POST ALL bypass callback not connected");
                auto* compactBypass=find<juce::TextButton>(*compact,spectralforge::postNativeBypassID(section,model));require(compactBypass && compactBypass->getToggleState()==bypass->getToggleState(),"POST compact and ALL power states disagree");
                if(scale==0 && ((section==0 && model==1)||(section==1 && model==2)||(section==2 && model==0)))snapshot(*panel,directory,section==0?"Native-Post-Bus":section==1?"Native-Post-Preamp":"Native-Post-EQ");
                close(window);
            }
        }
        for(int section=3;section<6;++section) {
            const int family=section==3?10:section==4?1:2;
            if(family==1)set(processor,"delaysync",0);
            auto window=open(canvas,"postExpand"+juce::String(section),"postEffectDetail"+juce::String(family));auto& panel=*window->getContentComponent();
            require(&panel.getLookAndFeel()==&editor.getLookAndFeel(),"Time/modulation ALL lost the rack style");
            auto* model=find<juce::ComboBox>(panel,spectralforge::modelFamilies[(size_t)family].parameter);require(model!=nullptr,"Time/modulation ALL model selector missing");
            for(int choice=0;choice<spectralforge::modelFamilies[(size_t)family].count;++choice) {
                model->setSelectedId(choice+1,juce::sendNotificationSync);settle(35);
                require(model->getText()==spectralforge::effectFamilyMenuName(family,choice),"Time/modulation ALL model list exposes a hardware name");
                require((int)processor.parameters().getRawParameterValue(spectralforge::modelFamilies[(size_t)family].parameter)->load()==choice,"Time/modulation ALL model callback is disconnected");
            }
            const std::array<const char*,3> ids=family==10?std::array<const char*,3>{"chorusrate","chorusdepth","chorusmix"}:family==1?std::array<const char*,3>{"delaytime","delayfeedback","delaymix"}:std::array<const char*,3>{"reverbsize","reverbdamping","reverbmix"};
            std::vector<NativeControlView*> controls;views(panel,controls);require(controls.size()==(family==1?4u:3u),"Time/modulation ALL omitted a processor control");
            for(auto* view:controls){require(view->isVisible(),"Time/modulation ALL hides a control");inside(panel,*view,"Time/modulation ALL control exceeds panel bounds");}
            for(const auto* id:ids){auto* slider=find<juce::Slider>(panel,id);require(slider!=nullptr,"Time/modulation ALL knob missing");const auto& range=processor.parameters().getParameter(id)->getNormalisableRange();slider->setValue(range.start+(range.end-range.start)*.62,juce::sendNotificationSync);require(std::abs(processor.parameters().getRawParameterValue(id)->load()-slider->getValue())<.02,"Time/modulation ALL knob did not reach audio state");}
            const auto powerID=family==10?"choruson":family==1?"delayon":"reverbon";auto* power=find<juce::TextButton>(panel,powerID);require(power!=nullptr,"Time/modulation ALL power missing");
            const bool before=processor.parameters().getRawParameterValue(powerID)->load()>.5f;power->triggerClick();settle();require((processor.parameters().getRawParameterValue(powerID)->load()>.5f)!=before,"Time/modulation ALL power callback is disconnected");
            if(family==1){auto* sync=find<juce::TextButton>(panel,"delaysync");require(sync!=nullptr,"Delay ALL is missing tempo sync");const bool beforeSync=processor.parameters().getRawParameterValue("delaysync")->load()>.5f;sync->triggerClick();settle();require((processor.parameters().getRawParameterValue("delaysync")->load()>.5f)!=beforeSync,"Delay ALL tempo sync is disconnected");}
            if(scale==0)snapshot(panel,directory,section==3?"Native-Post-Modulation-ALL":section==4?"Native-Post-Delay-ALL":"Native-Post-Reverb-ALL");close(window);
        }
    }
    editor.setSize(1180,780);settle();return cases;
}
inline void dialogTeardown() {
    auto processor=std::make_unique<ChimeraProcessor>();auto editor=std::make_unique<ChimeraEditor>(*processor);auto* canvas=editor->findChildWithID("surface");tab(*canvas,"POST");
    std::vector<juce::Component::SafePointer<juce::DialogWindow>> windows;
    for(int section=0;section<6;++section){const int family=section==3?10:section==4?1:2;windows.push_back(open(*canvas,"postExpand"+juce::String(section),section<3?"postNativePanel"+juce::String(section):"postEffectDetail"+juce::String(family)));}
    editor.reset();settle();for(auto window:windows)require(window==nullptr,"Editor destruction left an ALL window or live parameter attachment");
}
inline void run(const juce::File& directory){
    auto storage=std::make_unique<ChimeraProcessor>();auto& processor=*storage;
    auto editorStorage=std::make_unique<ChimeraEditor>(processor);auto& editor=*editorStorage;
    auto* canvas=editor.findChildWithID("surface");require(canvas!=nullptr,"Native UI canvas missing");
    checkGateUI(processor,editor,directory);
    auto* gateLocation=find<juce::TextButton>(*canvas,"gateAfterRig");require(gateLocation!=nullptr,"POST GATE control missing");
    const bool originalGateLocation=processor.parameters().getRawParameterValue("gateAfterRig")->load()>.5f;
    gateLocation->triggerClick();settle();require((processor.parameters().getRawParameterValue("gateAfterRig")->load()>.5f)!=originalGateLocation,"POST GATE button does not toggle the audio parameter");
    gateLocation->triggerClick();settle();require((processor.parameters().getRawParameterValue("gateAfterRig")->load()>.5f)==originalGateLocation,"POST GATE button cannot restore the prior routing");
    for(const auto* id:{"legacyPreControls","boardEnabled"})require(find(*canvas,id)==nullptr,"Obsolete Legacy/engine-selection control remains in native editor");
    set(processor,"mode",0);tab(*canvas,"RIGS");
    auto* selector=find<AmpSelector>(*canvas,"ampSelect1");require(selector!=nullptr,"Native UI amp selector missing");
    int channelCases=0,controlCases=0;
    for(int scale=0;scale<2;++scale){editor.setSize(scale?885:1180,scale?585:780);settle();
        for(int model=0;model<spectralforge::ampModelCount;++model){
            if(spectralforge::ampIsActive(model))require(selector->selectMenuResult(model+1),"Native model selection failed");
            else processor.setAmpModel(0,model); // Legacy recall still displays its original panel.
            settle();
            auto* panel=find<AmpNativePanel>(*canvas,"ampNativePanel1");require(panel!=nullptr,"Native amplifier panel missing");auto* channel=find<juce::ComboBox>(*panel,"ampChannel1");
            const auto& spec=spectralforge::ampNativePanel(model);
            require(channel && channel->isVisible()==(spec.channels.size()>1),"Native UI single-channel selector not hidden");
            auto* reference=find<juce::Label>(*canvas,"ampreference1");require(reference && reference->isVisible() && reference->getText()==juce::String::fromUTF8(spectralforge::ampInfo(model).reference),"Native amp caption must show only the original model name outside the alias list");
            require(selector->getText()==juce::String::fromUTF8(spectralforge::ampInfo(model).name),"Native amp list exposes original reference name");
            require(canvas->getLocalBounds().contains(panel->getBounds()),"Native amp panel exceeds editor bounds");
            for(int ch=0;ch<(int)spec.channels.size();++ch){
                channel->setSelectedId(ch+1,juce::sendNotificationSync);settle(45);require(processor.selectedAmpChannel(0)==ch,"Original amplifier channel callback not connected");++channelCases;
                for(int control=0;control<(int)spec.controls.size();++control){
                    const auto id=spectralforge::ampNativeControlID(0,model,control,ch);auto* view=find<NativeControlView>(*panel,id+"_control");
                    require(view!=nullptr,"Original amplifier control was not constructed");
                    require(view->label.getText()==juce::String::fromUTF8(spec.controls[(size_t)control].label),"Native amplifier label has invalid UTF-8 decoding");
                    require(view->isVisible()==spectralforge::ampNativeControlVisible(model,control,ch),"Wrong channel-specific controls visible");
                    require(processor.parameters().getParameter(id)!=nullptr,"Original amplifier control has no host/audio parameter");++controlCases;
                }
            }
            // A visible native knob must both activate the compatibility bank
            // and write its actual host parameter, including at 75% scale.
            const int ch=processor.selectedAmpChannel(0);
            for(int c=0;c<(int)spec.controls.size();++c)if(spec.controls[(size_t)c].kind==spectralforge::AmpNativeControlKind::knob && spectralforge::ampNativeControlVisible(model,c,ch)){
                const auto id=spectralforge::ampNativeControlID(0,model,c,ch);auto* knob=find<juce::Slider>(*panel,id);require(knob!=nullptr,"Native knob missing");
                set(processor,spectralforge::ampNativeEnabledID(0),0);const double value=knob->getValue()>.6?.23:.77;knob->setValue(value,juce::sendNotificationSync);
                require(std::abs(processor.parameters().getRawParameterValue(id)->load()-value)<.002,"Native knob did not reach its host parameter");
                require(processor.parameters().getRawParameterValue(spectralforge::ampNativeEnabledID(0))->load()>.5f,"Native gesture did not activate the model bank");break;
            }
            if(model==spectralforge::firstOriginalAmpModel) {
                const auto hzID=spectralforge::ampNativeControlID(0,model,int(spectralforge::original::Control::midFrequency),ch);
                auto* frequency=find<juce::Slider>(*panel,hzID);require(frequency && frequency->getTextFromValue(850).contains("850"),"Original MID FREQ display is not in Hz");
                frequency->setValue(1100,juce::sendNotificationSync);require(std::abs(processor.parameters().getRawParameterValue(hzID)->load()-1100)<.01f,"Original MID FREQ edit is disconnected");
                frequency->setValue(850,juce::sendNotificationSync);set(processor,spectralforge::ampNativeControlID(0,model,0,ch),.5f);
                snapshot(editor,directory,scale?"Nastrond-75pct":"Nastrond-Classic");
                if(!scale){auto window=open(*panel,"ampExpand1","ampNativePanel1");
                    auto* reset=find<juce::TextButton>(*window->getContentComponent(),"ampResetChannel1");require(reset && reset->isVisible(),"Original full panel has no channel reset");
                    const auto gainID=spectralforge::ampNativeControlID(0,model,0,ch),responseID=spectralforge::originalResponseID(0,ch);
                    set(processor,gainID,.17f);set(processor,responseID,0);reset->triggerClick();settle();
                    require(std::abs(processor.parameters().getRawParameterValue(gainID)->load()-.5f)<.001f && processor.parameters().getRawParameterValue(responseID)->load()>.5f,"Channel reset did not restore its current factory voice");
                    snapshot(*window->getContentComponent(),directory,"Nastrond-Controls");close(window);}
            }
            if(model==spectralforge::niflheimrAmpModel) {
                using C=spectralforge::niflheimr::Control;
                const auto hzID=spectralforge::ampNativeControlID(0,model,int(C::midFrequency),ch);
                const auto blendID=spectralforge::ampNativeControlID(0,model,int(C::blend),ch);
                auto* frequency=find<juce::Slider>(*panel,hzID);auto* blend=find<juce::Slider>(*panel,blendID);
                require(frequency && frequency->getTextFromValue(650).contains("650") && frequency->getTextFromValue(650).contains("Hz"),"Niflheimr MID FREQ display is not in Hz");
                require(blend && blend->getTextFromValue(.65)=="65%","Niflheimr BLEND display is not a percentage");
                frequency->setValue(1120,juce::sendNotificationSync);blend->setValue(.82,juce::sendNotificationSync);
                require(std::abs(processor.parameters().getRawParameterValue(hzID)->load()-1120)<.01f && std::abs(processor.parameters().getRawParameterValue(blendID)->load()-.82f)<.001f,"Niflheimr frequency/blend edits are disconnected");
                snapshot(editor,directory,scale?"Niflheimr-75pct":"Niflheimr-Classic");
                if(!scale) {
                    auto window=open(*panel,"ampExpand1","ampNativePanel1");auto& full=*window->getContentComponent();
                    constexpr int order[]{0,1,2,4,3,5,6,8,9,10,11,12,13,7};
                    int topY=-1,bottomY=-1,previousX=-1;
                    for(int position=0;position<14;++position) {
                        auto* view=find<NativeControlView>(full,spectralforge::ampNativeControlID(0,model,order[position],ch)+"_control");
                        require(view && view->isVisible(),"Niflheimr full panel missing a visible control");
                        if(position==0)topY=view->getY();if(position==7){bottomY=view->getY();previousX=-1;}
                        require(view->getY()==(position<7?topY:bottomY) && view->getX()>previousX,"Niflheimr full panel does not follow the specified two-row control order");
                        previousX=view->getX();
                    }
                    require(bottomY>topY,"Niflheimr second knob row overlaps the first");
                    auto* reset=find<juce::TextButton>(full,"ampResetChannel1");require(reset && reset->isVisible(),"Niflheimr full panel has no channel reset");
                    const auto inactiveID=spectralforge::ampNativeControlID(0,model,int(C::blend),0);set(processor,inactiveID,.19f);
                    reset->triggerClick();settle();
                    for(size_t control=0;control<spectralforge::niflheimr::controlCount;++control)
                        require(std::abs(processor.parameters().getRawParameterValue(spectralforge::ampNativeControlID(0,model,int(control),ch))->load()-spectralforge::niflheimr::controls[control].initial)<.0011f,"Niflheimr reset did not restore all fourteen current-channel controls");
                    require(std::abs(processor.parameters().getRawParameterValue(inactiveID)->load()-.19f)<.001f,"Niflheimr channel reset changed an inactive channel");
                    snapshot(full,directory,"Niflheimr-Controls");close(window);
                }
            }
        }
    }
    editor.setSize(1180,780);processor.setAmpModel(0,15);settle();snapshot(editor,directory,"Native-Amp-Classic");
    processor.setAmpModel(0,4);settle();auto* expand=find<juce::TextButton>(*canvas,"ampExpand1");require(expand && expand->isVisible(),"Expanded amplifier panel is inaccessible");expand->triggerClick();settle(90);
    juce::DialogWindow* opened=nullptr;
    for(int i=0;i<juce::TopLevelWindow::getNumTopLevelWindows();++i)if(auto* window=dynamic_cast<juce::DialogWindow*>(juce::TopLevelWindow::getTopLevelWindow(i));window && dynamic_cast<AmpNativePanel*>(window->getContentComponent())){opened=window;break;}
    require(opened!=nullptr,"ALL button failed to open the original-control panel");require(&opened->getContentComponent()->getLookAndFeel()==&editor.getLookAndFeel(),"Expanded amplifier controls lost the editor style");snapshot(*opened->getContentComponent(),directory,"Native-Amp-Expanded");opened->exitModalState(0);settle();
    lowOverview(processor,editor,*canvas,directory);
    const int postControlCases=postPanels(processor,editor,*canvas,directory);
    dialogTeardown();
    std::cout<<"PASS native UI: all "<<spectralforge::ampModelCount<<" serialized amplifier panels at 100/75%, "<<channelCases<<" channel cases, "<<controlCases<<" channel/control visibility cases, Matrix LOW priority controls and ALL, 9 POST models / "<<postControlCases<<" controls at 100/75%, flat POST alias menus and short original model captions, six compact rack rows and ALL dialogs, editor teardown and real parameter callbacks\n";
}
}
inline void runNativeUITests(const juce::File& directory){nativeUITests::run(directory);}
