#pragma once
#include "GateUITests.h"
#include "PluginEditor.h"
#include "RigViewTestHelpers.h"
#include "AmpNativeParameters.h"
#include "PostNativeCatalog.h"
#include "NativeMessageLoop.h"
#include "PublicNameTests.h"
#include <iostream>
#include <stdexcept>

namespace nativeUITests {
inline void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
inline void settle(int ms=55){require(chimeraTest::dispatchFor(ms),"Native UI loop exited");}
template<class Ready> void waitForNativeSpectrum(Ready&& ready,const char* failure) {
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+2000.;
    while(!ready()) {
        require(juce::Time::getMillisecondCounterHiRes()<deadline,failure);
        settle(10);
    }
}
// A native event dispatch can return before a JUCE timer runs (notably while
// AppKit is creating a peer). Synchronize the actual callback/UI handshake;
// do not assume a fixed sleep armed capture before an offline audio burst.
inline void prepareDynamicsCapture(ChimeraProcessor& processor) {
    auto& analysis=processor.dynamicsAnalysis();
    auto* view=static_cast<spectralforge::DynamicsSpectrumPanel*>(analysis.uiConsumer);
    if(view==nullptr)return;
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+2000.;
    juce::AudioBuffer<float> silence(2,256);juce::MidiBuffer midi;
    for(;;) {
        require(analysis.uiConsumer==view,"Dynamics capture changed owner while awaiting its callback boundary");
        if(!view->isShowing() || view->isSnapshotFrozen()
           || view->selectedStage()==spectralforge::DynamicsStage::none)return;
        if(!view->isWaitingForFreshCapture() && analysis.fifo.enabled.load())return;
        if(juce::Time::getMillisecondCounterHiRes()>=deadline) {
            std::cerr<<"Dynamics capture timeout stage="<<int(view->selectedStage())
                     <<" drain="<<view->isWaitingForFreshCapture()<<" enabled="<<analysis.fifo.enabled.load()
                     <<" pending="<<analysis.fifo.pendingSamples()<<" frames="<<view->fftFrames()<<'\n';
            require(false,"Native dynamics capture did not acknowledge its real audio callback");
        }
        silence.clear();processor.processBlock(silence,midi);settle(10);
    }
}
inline void consumeSpectrumAudio(spectralforge::EQAnalyzerFIFO& fifo) {
    if(!fifo.enabled.load())return;
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+2000.;
    while(fifo.pendingSamples()>0) {
        if(juce::Time::getMillisecondCounterHiRes()>=deadline) {
            std::cerr<<"Spectrum consumption timeout enabled="<<fifo.enabled.load()<<" pending="<<fifo.pendingSamples()<<'\n';
            require(false,"Native spectrum timer did not consume the supplied production audio");
        }
        settle(10);
    }
}
template<class T=juce::Component> T* find(juce::Component& root,const juce::String& id){
    if(root.getComponentID()==id)if(auto* typed=dynamic_cast<T*>(&root))return typed;
    for(auto* child:root.getChildren())if(auto* result=find<T>(*child,id))return result;return nullptr;
}
inline void set(ChimeraProcessor& processor,const juce::String& id,float value){auto* p=processor.parameters().getParameter(id);require(p!=nullptr,"Native UI parameter missing");p->setValueNotifyingHost(p->convertTo0to1(value));}
inline void tab(juce::Component& root,const char* name){
    const juce::String label(name);const auto id=label=="RIGS"?"head":label=="PRE"?"pre":label=="POST"?"post":"tone.eq";
    auto* button=find<juce::TextButton>(root,"path."+juce::String(id));require(button && button->isVisible(),"Signal navigation stage missing");
    struct Receipt final:juce::Button::Listener {
        int clicks{};
        void buttonClicked(juce::Button*) override {++clicks;}
    } receipt;
    button->addListener(&receipt);button->triggerClick();settle();button->removeListener(&receipt);
    if(receipt.clicks!=1)
        std::cerr<<"NATIVE NAVIGATION pending command id="<<button->getComponentID()
                 <<" clicks="<<receipt.clicks<<" selected="<<button->getToggleState()<<'\n';
}

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
    auto* reference=find<juce::Label>(panel,"postNativeReference"+juce::String(section));
    require(reference && reference->isVisible() && reference->getText()==spectralforge::postNativeDescription(section,selected),"POST caption must describe the selected sound");
    publicNameTests::componentText(panel,"POST panel");
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
        const std::array<const char*,2> ids{"bandtone1","lowampmix"};
        require(shown.size()>ids.size(),"Matrix LOW overview must retain amplifier controls below the shared knobs");
        for(size_t i=0;i<ids.size();++i) {
            auto* view=find<NativeControlView>(*panel,juce::String(ids[i])+"_control");
            require(view && view->isVisible(),"Matrix LOW shared control missing");
            inside(*panel,*view,"Matrix LOW priority control is clipped");inside(editor,*view,"Matrix LOW priority control exceeds scaled editor");
            auto* slider=find<juce::Slider>(*view,ids[i]);require(slider!=nullptr,"Matrix LOW priority knob missing");
            const double value=i==0?(scale?-3.5:4.5):(scale?.31:.64);slider->setValue(value,juce::sendNotificationSync);
            require(std::abs(processor.parameters().getRawParameterValue(ids[i])->load()-value)<.002,"Matrix LOW priority knob did not reach its host parameter");
        }
        auto* compEntry=find<juce::TextButton>(*panel,"matrixLowCompressor.open");
        require(compEntry && compEntry->isVisible() && find(*panel,"lowcomp_control")==nullptr,
                "Matrix LOW compression must open a spectrum editor");
        inside(*panel,*compEntry,"LOW spectrum editor button is clipped");inside(editor,*compEntry,"LOW spectrum entry exceeds scaled editor");
        const auto toneBounds=panel->getLocalArea(find(*panel,"bandtone1_control"),find(*panel,"bandtone1_control")->getLocalBounds());
        const auto blendBounds=panel->getLocalArea(find(*panel,"lowampmix_control"),find(*panel,"lowampmix_control")->getLocalBounds());
        const auto entryBounds=panel->getLocalArea(compEntry,compEntry->getLocalBounds());
        require(entryBounds.getX()>=toneBounds.getRight() && entryBounds.getRight()<=blendBounds.getX()
                && entryBounds.getY()>=toneBounds.getY() && entryBounds.getBottom()<=toneBounds.getBottom(),
                "LOW shared row must retain tone / compressor spectrum / blend order");
        const float compBefore=processor.parameters().getRawParameterValue("lowcomp")->load();
        auto compWindow=open(*panel,"matrixLowCompressor.open","matrixLowCompressor.panel");auto& compFull=*compWindow->getContentComponent();
        auto* compSpectrum=find<spectralforge::DynamicsSpectrumPanel>(compFull,"matrixLowCompressor.spectrum");
        require(compSpectrum && compSpectrum->isShowing() && processor.parameters().getRawParameterValue("lowcomp")->load()==compBefore,
                "Opening LOW spectrum changed compression or omitted its analyzer");
        auto* compKnob=find<juce::Slider>(compFull,"lowcomp");require(compKnob && compKnob->isShowing(),"LOW spectrum editor has no real compression parameter");
        compKnob->setValue(scale?.31:.64,juce::sendNotificationSync);
        require(std::abs(processor.parameters().getRawParameterValue("lowcomp")->load()-compKnob->getValue())<.002,"LOW spectrum compressor callback is disconnected");
        close(compWindow);
        const auto& overviewSpec=spectralforge::ampNativePanel(processor.selectedAmpModel(0));
        auto* overviewChannel=find<juce::ComboBox>(*panel,"ampChannel1");
        require(overviewChannel && overviewChannel->isVisible()==(overviewSpec.channels.size()>1),"Matrix LOW overview lost its amplifier channel selector");
        auto* overviewInput=find<juce::ComboBox>(*panel,"ampInput1");
        require(overviewInput && overviewInput->isVisible()==(overviewSpec.routes.size()>1),"Matrix LOW overview lost its amplifier input selector");
        for(size_t i=0;i<ids.size();++i) {
            auto* view=find<NativeControlView>(*panel,juce::String(ids[i])+"_control");
            require(view->getY()==0 && (i==0 || view->getX()>find<NativeControlView>(*panel,juce::String(ids[i-1])+"_control")->getX()),"LOW shared knobs must occupy the first row in tone/comp/blend order");
        }
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
        require(find<juce::TextButton>(*full,"matrixLowCompressor.open") && find<juce::TextButton>(*full,"matrixLowCompressor.open")->isVisible()
                && find(*full,"lowcomp_control")==nullptr,"Expanded LOW panel exposes compression without its spectrum");
        compWindow=open(*full,"matrixLowCompressor.open","matrixLowCompressor.panel");
        compSpectrum=find<spectralforge::DynamicsSpectrumPanel>(*compWindow->getContentComponent(),"matrixLowCompressor.spectrum");
        require(compSpectrum && compSpectrum->isShowing(),"Expanded LOW panel cannot open its compressor spectrum");close(compWindow);
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
                require(section==0?shown==0:(shown>=4 && shown<=5),
                        "POST compressor adjustments must open the spectrum editor; other rows retain primary controls");
                auto window=open(*compact,"postExpand"+juce::String(section),compact->getComponentID());auto* panel=dynamic_cast<PostNativePanel*>(window->getContentComponent());
                require(panel && &panel->getLookAndFeel()==&editor.getLookAndFeel(),"Expanded POST lost its original control panel/style");
                if(section==0) {
                    auto* spectrum=find<spectralforge::DynamicsSpectrumPanel>(*panel,"postCompressorSpectrum");
                    require(spectrum && spectrum->isShowing() && spectrum->selectedStage()==spectralforge::DynamicsStage::postCompressor,
                            "POST compressor controls opened without their own spectrum");
                    require(spectrum->graphBounds().getHeight()>=140.f,"POST compressor spectrum plot is too small");
                    inside(*panel,*spectrum,"POST compressor spectrum is clipped");
                }
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
                publicNameTests::componentText(panel,"time/modulation detail panel");
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
inline void dynamicsEditors(const juce::File& directory,bool queueStress=false) {
    using namespace spectralforge;
    auto storage=std::make_unique<ChimeraProcessor>();auto& processor=*storage;
    constexpr std::array<int,5> models{6,7,8,9,10};
    for(int owner=0;owner<5;++owner)processor.setPedalModel(owner,models[(size_t)owner]);
    processor.movePedal(1,-1); // Display position zero now belongs to stable owner one.
    set(processor,"mode",0);set(processor,"oversampling",0);set(processor,"ampon1",0);
    processor.prepareToPlay(48000,256);
    auto editor=std::make_unique<ChimeraEditor>(processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(90);
    auto* canvas=editor->findChildWithID("surface");require(canvas,"Native dynamics canvas missing");
    auto* board=find<PedalBoardPanel>(*canvas,"universalPedalBoard");require(board,"Native dynamics PRE board missing");
    const auto values=[&]{std::vector<float> result;for(auto* p:processor.getParameters())result.push_back(p->getValue());return result;};
    struct Writes final:juce::AudioProcessorListener {
        ChimeraProcessor& processor;int changes{},gestures{};
        explicit Writes(ChimeraProcessor& p):processor(p){processor.addListener(this);}
        ~Writes() override {processor.removeListener(this);}
        void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {++changes;}
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
        void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int) override {++gestures;}
        void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int) override {++gestures;}
    };
    const auto navigate=[&](auto&& action) {
        const auto before=values();Writes writes(processor);action();settle(60);
        require(values()==before && writes.changes==0 && writes.gestures==0,
                "Native dynamics navigation changed an audio parameter or sent a host gesture");
    };
    int64_t samplePosition=0;
    const auto audio=[&] {
        juce::AudioBuffer<float> block(2,256);juce::MidiBuffer midi;
        for(int chunk=0;chunk<32;++chunk) {
            if(chunk%8==0)prepareDynamicsCapture(processor);
            for(int i=0;i<256;++i,++samplePosition) {
                const double phase=juce::MathConstants<double>::twoPi*double(samplePosition)/48000.;
                const float value=.16f*float(std::sin(375.*phase))+.04f*float(std::sin(6000.*phase));
                block.setSample(0,i,value);block.setSample(1,i,-value*.8f);
            }
            processor.processBlock(block,midi);
            if((chunk+1)%8==0)settle(45);
        }
        consumeSpectrumAudio(processor.dynamicsAnalysis().fifo);
    };
    const auto spectrumIn=[&](juce::Component& panel,const char* id,DynamicsStage stage) {
        auto* spectrum=find<DynamicsSpectrumPanel>(panel,id);
        require(spectrum && spectrum->isShowing() && spectrum->selectedStage()==stage,
                "Native dynamics adjustment lacks the selected module's visible spectrum");
        require(spectrum->getLocalBounds().toFloat().contains(spectrum->graphBounds()) && spectrum->graphBounds().getHeight()>=120.f,
                "Native dynamics plot is clipped or too small");
        inside(panel,*spectrum,"Native dynamics spectrum exceeds its editor");
        for(const char* key:{"live","input","output","freeze","resolution","smoothing"}) {
            auto* control=find(*spectrum,"commonDynamics.spectrum."+juce::String(key));
            require(control && control->isShowing(),"Native dynamics spectrum display control is hidden");
            inside(*spectrum,*control,"Native dynamics spectrum toolbar is clipped");
        }
        return spectrum;
    };
    int preCases=0,postCases=0;
    for(int scale:{75,100,125,150}) {
        processor.setPedalBoardEnabled(false);settle();
        navigate([&]{
            editor->setSize(1180*scale/100,780*scale/100);
            if(queueStress && scale==75) {
                // A bounded native message backlog, without replacing the event
                // pump or calling a product timer/refresh/navigation callback.
                for(int i=0;i<32;++i)
                    require(juce::MessageManager::callAsync([]{juce::Thread::sleep(10);}),"Could not enqueue navigation backlog");
            }
            tab(*canvas,"PRE");board->navigateOwner(-1);
        });
        auto* entry=find<juce::TextButton>(*board,"boardCompressorSpectrumAt0");
        if(!entry || !entry->isShowing() || !entry->isEnabled()) {
            auto* selected=find<juce::TextButton>(*canvas,"path.pre");
            const auto state=processor.pedalBoardState();
            std::cerr<<"PRE ENTRY state scale="<<scale<<" stress="<<queueStress
                     <<" preSelected="<<(selected && selected->getToggleState())
                     <<" boardVisible="<<board->isVisible()<<" boardShowing="<<board->isShowing()
                     <<" entryVisible="<<(entry && entry->isVisible())
                     <<" entryShowing="<<(entry && entry->isShowing())
                     <<" entryEnabled="<<(entry && entry->isEnabled())
                     <<" ownerAt0="<<state.order[0]<<" modelAt0="<<state.instances[(size_t)state.order[0]].model<<'\n';
        }
        require(entry && entry->isShowing() && entry->isEnabled(),"Reordered PRE compressor has no visible spectrum entry");
        navigate([&]{entry->triggerClick();});
        require(processor.dynamicsAnalysis().preOwner.load()==1 && !processor.pedalBoardState().enabled,
                "PRE spectrum entry selected a display position as owner or activated the disabled board");
        auto* inactivePre=spectrumIn(*board,"boardCompressorSpectrum",DynamicsStage::preCompressor);audio();
        auto* preStatus=find<juce::Label>(*board,"boardCompressorStatus");
        require(!inactivePre->analyzerHasData() && preStatus && preStatus->isShowing() && preStatus->getText().contains("INACTIVE"),
                "Inactive native PRE compressor displayed the legacy path's audio or omitted its inactive state");
        navigate([&]{require(!board->openCompressorDetails(5),"Invalid PRE compressor owner was accepted");});
        require(processor.dynamicsAnalysis().preOwner.load()==1,"Invalid owner navigation changed the displayed compressor");
        for(int owner=0;owner<5;++owner) {
            navigate([&]{require(board->openCompressorDetails(owner),"Existing PRE compressor editor is inaccessible");});
            auto* spectrum=spectrumIn(*board,"boardCompressorSpectrum",DynamicsStage::preCompressor);
            require(spectrum->preOwner()==owner && processor.dynamicsAnalysis().preOwner.load()==owner && processor.dynamicsAnalysis().uiConsumer==spectrum,
                    "PRE editor observes a different stable owner or shares its FIFO consumer");
            require(!spectrum->isSnapshotFrozen() && !spectrum->analyzerHasData(),"PRE owner switch retained another compressor's snapshot");
            auto* title=find<juce::Label>(*board,"boardDetailTitle");
            require(title && title->getText()==pedalMenuName(models[(size_t)owner]),"PRE owner editor shows the wrong model caption");
            auto* viewport=find<juce::Viewport>(*board,"boardDetailViewport");
            auto* content=find(*board,"boardDetailControls");require(viewport && content,"PRE spectrum editor lost its control viewport");
            require(viewport->getY()>=spectrum->getBottom(),"PRE controls overlap the spectrum");
            inside(*editor,*spectrum,"PRE spectrum exceeds the scaled editor");
            const auto& model=pedalModel(models[(size_t)owner]);
            for(int c=0;c<model.controlCount;++c) {
                const auto id=pedalControlID(owner,models[(size_t)owner],c);auto* knob=find<juce::Slider>(*content,id);
                require(knob && knob->isShowing() && knob->isEnabled()==model.controls[(size_t)c].connected,
                        "PRE spectrum editor lost an original control or enabled a hardware-only control");
                navigate([&]{viewport->setViewPosition(0,knob->getY());});
                require(viewport->getViewArea().contains(knob->getBounds()),"PRE spectrum editor cannot scroll an entire control into view");
                inside(*editor,*knob,"PRE compressor control exceeds the scaled editor");
                require(spectrum->isShowing(),"Scrolling PRE controls hid the spectrum");
                if(model.controls[(size_t)c].connected) {
                    const auto* parameter=processor.parameters().getRawParameterValue(id);const float before=parameter->load();
                    const int key=before<model.controls[(size_t)c].maximum?juce::KeyPress::upKey:juce::KeyPress::downKey;
                    require(knob->keyPressed(juce::KeyPress(key)),"Visible PRE compressor keyboard edit was not accepted");
                    require(parameter->load()!=before && std::abs(parameter->load()-knob->getValue())<.002f,
                            "PRE spectrum editor control did not update its exact model/owner bank");
                    require(processor.pedalBoardState().enabled,"A deliberate PRE compressor edit failed to activate its board");
                }
                ++preCases;
            }
            navigate([&]{viewport->setViewPosition(0,0);});audio();
            if(!spectrum->analyzerHasData() || spectrum->analysisSampleRate()!=48000.)
                std::cerr<<"PRE spectrum owner="<<owner<<" scale="<<scale<<" data="<<spectrum->analyzerHasData()
                         <<" frames="<<spectrum->fftFrames()<<" rate="<<spectrum->analysisSampleRate()
                         <<" drain="<<spectrum->isWaitingForFreshCapture()<<" enabled="<<processor.dynamicsAnalysis().fifo.enabled.load()
                         <<" pending="<<processor.dynamicsAnalysis().fifo.pendingSamples()<<'\n';
            require(spectrum->analyzerHasData() && spectrum->fftFrames()>0 && spectrum->analysisSampleRate()==48000.,
                    "PRE spectrum editor did not receive real module input/output audio");
            snapshot(*editor,directory,("Native-PRE-Spectrum-owner"+juce::String(owner)+"-"+juce::String(scale)).toRawUTF8());
            navigate([&]{spectrum->setSnapshotFrozen(true);});require(spectrum->isSnapshotFrozen(),"Measured PRE frame could not freeze");
        }
        processor.setPedalBoardEnabled(false);settle(160);audio();
        require(!inactivePre->isSnapshotFrozen() && !inactivePre->analyzerHasData() && preStatus->getText().contains("INACTIVE"),
                "Disabling the native PRE board retained a frozen frame or captured the legacy compressor");
        navigate([&]{board->navigateOwner(-1);});
        require(processor.dynamicsAnalysis().uiConsumer==nullptr && !processor.dynamicsAnalysis().fifo.enabled.load(),
                "Leaving PRE details left an active spectrum consumer");
        for(int model=0;model<3;++model) {
            set(processor,postNativeModelID(0),float(model));set(processor,postNativeModeID(0),0);settle();
            if(scale==75) {
                // Reproduce the logical content size of a scaled dialog
                // constrained by a small display, on every platform.
                PostNativePanel compact(processor,0,true);compact.setSize(780,420);compact.refresh();
                auto* viewport=find<juce::Viewport>(compact,"postNativeViewport0");
                auto* spectrum=find<DynamicsSpectrumPanel>(compact,"postCompressorSpectrum");
                require(viewport && spectrum && spectrum->graphBounds().getHeight()>=120.f
                        && viewport->getY()>=spectrum->getBottom(),"Compact POST layout lost its full spectrum");
                std::vector<NativeControlView*> controls;views(compact,controls);
                require(controls.size()==size_t(postNativeModel(0,model).controlCount+2),"Compact POST layout omitted controls");
                for(auto* control:controls) {
                    viewport->setViewPosition(0,control->getY());
                    require(viewport->getViewArea().contains(control->getBounds()),"Display-constrained POST viewport clips a full control");
                }
            }
            juce::Component::SafePointer<juce::DialogWindow> window;
            navigate([&]{tab(*canvas,"POST");window=open(*canvas,"postExpand0","postNativePanel0");});
            auto* full=dynamic_cast<PostNativePanel*>(window->getContentComponent());require(full,"POST spectrum editor content missing");
            auto* spectrum=spectrumIn(*full,"postCompressorSpectrum",DynamicsStage::postCompressor);
            require(processor.parameters().getRawParameterValue(postNativeModeID(0))->load()==0,
                    "Opening POST compressor details activated a compatibility bank");
            require(spectrum->postNativeSource()==1 && processor.dynamicsAnalysis().postNativeSource.load()==1,
                    "Native POST editor did not select the native source identity");
            audio();auto* postStatus=find<juce::Label>(*full,"postCompressorStatus");
            require(!spectrum->analyzerHasData() && postStatus && postStatus->isShowing() && postStatus->getText().contains("INACTIVE"),
                    "Inactive native POST editor displayed legacy compressor audio or omitted its inactive state");
            auto* viewport=find<juce::Viewport>(*full,"postNativeViewport0");require(viewport && viewport->getY()>=spectrum->getBottom(),"POST controls overlap the spectrum");
            const auto& spec=postNativeModel(0,model);
            for(int c=0;c<spec.controlCount;++c) {
                const auto id=postNativeControlID(0,model,c);auto* view=find<NativeControlView>(*full,id+"_control");
                require(view && view->isShowing() && view->isEnabled()==spec.controls[(size_t)c].connected,
                        "POST spectrum editor omitted or incorrectly enabled an original control");
                navigate([&]{viewport->setViewPosition(0,view->getY());});
                if(!viewport->getViewArea().contains(view->getBounds())) {
                    std::cerr<<"POST control clipped model="<<model<<" scale="<<scale<<" id="<<id
                             <<" panel="<<full->getLocalBounds().toString()<<" viewport="<<viewport->getViewArea().toString()
                             <<" control="<<view->getBounds().toString()<<'\n';
                    snapshot(*full,directory,"Native-POST-clipped");
                }
                require(viewport->getViewArea().contains(view->getBounds()),"POST spectrum editor cannot expose an entire control");
                inside(*full,*view,"POST compressor control exceeds its focused editor");
                if(spec.controls[(size_t)c].connected) {
                    const auto& control=spec.controls[(size_t)c];float target=0;
                    if(control.kind==PostNativeControlKind::knob){view->slider.setValue(control.minimum+(control.maximum-control.minimum)*.61f,juce::sendNotificationSync);target=(float)view->slider.getValue();}
                    else if(control.kind==PostNativeControlKind::choice){target=float(view->choice.getSelectedId()%view->choice.getNumItems());view->choice.setSelectedId((int)target+1,juce::sendNotificationSync);}
                    else {target=view->toggle.getToggleState()?0.f:1.f;view->toggle.triggerClick();settle(20);}
                    require(std::abs(processor.parameters().getRawParameterValue(id)->load()-target)<.02f,
                            "POST spectrum control edit did not reach its selected model bank");
                    require(spectrum->isShowing(),"POST control adjustment hid its spectrum");
                }
                ++postCases;
            }
            // Software input/output controls are also real dynamics adjustments.
            for(const auto& id:{postNativeTrimID(0,model),postNativeLevelID(0,model)}) {
                auto* view=find<NativeControlView>(*full,id+"_control");require(view && view->isShowing(),"POST spectrum editor omitted software trim/level");
                navigate([&]{viewport->setViewPosition(0,view->getY());});
                require(viewport->getViewArea().contains(view->getBounds()),"POST software trim/level is clipped");
                view->slider.setValue(scale==75?1.5:-1.5,juce::sendNotificationSync);
                require(std::abs(processor.parameters().getRawParameterValue(id)->load()-view->slider.getValue())<.01f,
                        "POST software trim/level callback is disconnected");
            }
            require(processor.parameters().getRawParameterValue(postNativeModeID(0))->load()>.5f,
                    "A deliberate POST spectrum control edit failed to activate the native bank");
            navigate([&]{viewport->setViewPosition(0,0);});audio();
            require(spectrum->analyzerHasData() && spectrum->fftFrames()>0 && processor.dynamicsAnalysis().uiConsumer==spectrum,
                    "POST spectrum editor did not capture real input/output from its module");
            snapshot(*full,directory,("Native-POST-Spectrum-model"+juce::String(model)+"-"+juce::String(scale)).toRawUTF8());
            navigate([&]{spectrum->setSnapshotFrozen(true);});require(spectrum->isSnapshotFrozen(),"POST measured frame could not freeze");
            set(processor,postNativeModeID(0),0);settle(160);audio();
            require(!spectrum->isSnapshotFrozen() && !spectrum->analyzerHasData() && postStatus->getText().contains("INACTIVE"),
                    "Disabling native POST retained its frozen frame or captured the legacy source");
            navigate([&]{close(window);});
            require(processor.dynamicsAnalysis().uiConsumer==nullptr && !processor.dynamicsAnalysis().fifo.enabled.load(),
                    "Closing POST compressor details left an active spectrum consumer");
        }
        set(processor,"mode",2);settle();
        navigate([&]{tab(*canvas,"RIGS");rigViewTests::showControls(*canvas);});
        auto* lowHead=find<::AmpNativePanel>(*canvas,"ampNativePanel1");require(lowHead,"Matrix LOW head panel missing");
        auto* lowEntry=find<juce::TextButton>(*lowHead,"matrixLowCompressor.open");
        require(lowEntry && lowEntry->isShowing() && find(*lowHead,"lowcomp_control")==nullptr,
                "Matrix LOW overview exposes compression without its spectrum");
        inside(*editor,*lowEntry,"Matrix LOW spectrum entry exceeds scaled editor");
        juce::Component::SafePointer<juce::DialogWindow> lowWindow;
        navigate([&]{lowWindow=open(*lowHead,"matrixLowCompressor.open","matrixLowCompressor.panel");});
        auto& lowPanel=*lowWindow->getContentComponent();auto* lowSpectrum=spectrumIn(lowPanel,"matrixLowCompressor.spectrum",DynamicsStage::lowCompressor);
        for(const auto* id:{"lowcomp","lowampmix"}) {
            auto* control=find<juce::Slider>(lowPanel,id);require(control && control->isShowing(),"LOW spectrum editor lost an existing host control");
            inside(lowPanel,*control,"LOW spectrum control is clipped");
            require(control->getY()>=lowSpectrum->getBottom(),"LOW control overlaps its spectrum");
            control->setValue(juce::String(id)=="lowcomp"?.67:.37,juce::sendNotificationSync);
            require(std::abs(processor.parameters().getRawParameterValue(id)->load()-control->getValue())<.002f,
                    "LOW spectrum editor did not update the released parameter ID");
        }
        audio();require(lowSpectrum->analyzerHasData() && lowSpectrum->fftFrames()>0,"Matrix LOW spectrum did not receive actual compressor input/output");
        snapshot(lowPanel,directory,("Native-LOW-Spectrum-"+juce::String(scale)).toRawUTF8());
        navigate([&]{lowSpectrum->setSnapshotFrozen(true);});require(lowSpectrum->isSnapshotFrozen(),"LOW measured frame could not freeze");
        set(processor,"mode",0);settle(100);audio();
        require(lowSpectrum->selectedStage()==DynamicsStage::none && !lowSpectrum->isSnapshotFrozen() && !lowSpectrum->analyzerHasData(),
                "Inactive Matrix routing retained a LOW compressor spectrum");
        navigate([&]{close(lowWindow);});
        require(processor.dynamicsAnalysis().uiConsumer==nullptr && !processor.dynamicsAnalysis().fifo.enabled.load(),
                "Closing LOW compressor editor left analyzer capture active");
    }
    for(int owner=0;owner<5;++owner)processor.setPedalModel(owner,0);settle();
    navigate([&]{tab(*canvas,"PRE");require(!board->openCompressorDetails(),"An absent PRE compressor opened a fabricated editor");});
    require(find(*board,"boardCompressorSpectrum") && !find(*board,"boardCompressorSpectrum")->isShowing(),
            "Empty PRE board retained a visible compressor spectrum");
    editor.reset();processor.releaseResources();
    require(processor.dynamicsAnalysis().uiConsumer==nullptr && !processor.dynamicsAnalysis().fifo.enabled.load(),
            "Native dynamics editor destruction retained analyzer capture");
    std::cout<<"PASS native dynamics: 5 PRE compressor models with stable reordered owners / "<<preCases
             <<" control cases, 3 POST models / "<<postCases<<" original control cases plus trim/level, Matrix LOW compression/blend, real IN/OUT spectra, inactive native/legacy isolation, scrollable controls, frozen source invalidation, navigation without parameter writes or gestures, and 75/100/125/150 percent layouts\n";
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
    rigViewTests::showControls(*canvas);
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
            auto* reference=find<juce::Label>(*canvas,"ampreference1");require(reference && reference->isVisible() && reference->getText()==juce::String::fromUTF8(spectralforge::ampInfo(model).character),"Native amp caption must describe the selected sound");
            publicNameTests::safeText(reference->getText(),"selected AMP caption");
            require(selector->getText()==juce::String::fromUTF8(spectralforge::ampInfo(model).name),"Native amp list exposes original reference name");
            require(canvas->getLocalBounds().contains(panel->getBounds()),"Native amp panel exceeds editor bounds");
            for(int ch=0;ch<(int)spec.channels.size();++ch){
                channel->setSelectedId(ch+1,juce::sendNotificationSync);settle(45);require(processor.selectedAmpChannel(0)==ch,"Original amplifier channel callback not connected");++channelCases;
                publicNameTests::componentText(*panel,"selected AMP channel");
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
    dynamicsEditors(directory);
    // Preserve the legacy machine-readable receipt required by Validation/ci-check-map.json.
    // Its historical caption label does not change the sound-description and safeText assertions above.
    std::cout<<"PASS public captions: selected amplifier captions use sound descriptions and satisfy the public-name policy\n";
    std::cout<<"PASS native UI: all "<<spectralforge::ampModelCount<<" serialized amplifier panels at 100/75%, "<<channelCases<<" channel cases, "<<controlCases<<" channel/control visibility cases, Matrix LOW priority controls and ALL, 9 POST models / "<<postControlCases<<" controls at 100/75%, flat POST alias menus and short original model captions, six compact rack rows and ALL dialogs, editor teardown and real parameter callbacks\n";
}
}
inline void runNativeUITests(const juce::File& directory){nativeUITests::run(directory);}

