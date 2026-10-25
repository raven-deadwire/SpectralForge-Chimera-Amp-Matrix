#pragma once
#include "NativeUITests.h"
#include "SignalPathUITests.h"
#include "EasyMode.h"
#include "FactoryPresets.h"
#include "PresetOrder.h"

namespace easyModeUITests {
using namespace nativeUITests;
inline float raw(ChimeraProcessor& p,const juce::String& id){return p.parameters().getRawParameterValue(id)->load();}
inline void preference() {
    struct Temporary {
        juce::File folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Chimera-easy-preference",{},false);
        ~Temporary(){folder.deleteRecursively();}
    } temporary;
    using spectralforge::easy::Preference;
    const auto file=temporary.folder.getChildFile("interface.json");
    require(Preference::startsEasy(file),"First start must open Easy view");
    require(Preference::remember(false,file) && !Preference::startsEasy(file),"Full editor choice was not remembered");
    require(Preference::remember(true,file) && Preference::startsEasy(file),"Easy view choice was not remembered");
    require(file.replaceWithText("{broken") && Preference::startsEasy(file),"Unreadable preference must fall back to Easy view");
}
inline void mapping() {
    // Every head exposes at least GAIN, BASS and TREBLE on one channel; the
    // printed table documents which front-panel control each knob edits.
    using spectralforge::easy::Role;
    for(int model=0;model<spectralforge::ampModelCount;++model) {
        const auto& panel=spectralforge::ampNativePanel(model);
        std::array<bool,spectralforge::easy::roleCount> found{};
        for(int channel=0;channel<(int)panel.channels.size();++channel) {
            std::cout<<"EASY MAP "<<spectralforge::ampInfo(model).name<<" / "<<panel.channels[(size_t)channel];
            for(int role=0;role<spectralforge::easy::roleCount;++role) {
                const int control=spectralforge::easy::ampControl(model,channel,Role(role));
                if(control>=0) {
                    require(spectralforge::ampNativeControlVisible(model,control,channel)
                            && panel.controls[(size_t)control].kind==spectralforge::AmpNativeControlKind::knob,"Easy knob maps to a hidden or non-knob control");
                    found[(size_t)role]=true;
                }
                std::cout<<" | "<<spectralforge::easy::roleNames[(size_t)role]<<"="<<(control>=0?panel.controls[(size_t)control].label:"-");
            }
            std::cout<<'\n';
        }
        require(found[0] && found[1] && found[3],"A head has no Easy GAIN, BASS or TREBLE control");
    }
}
inline void editorView(const juce::File& directory) {
    auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,256);
    auto editor=std::make_unique<ChimeraEditor>(*p);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);
    require(!editor->easyModeActive() && find(*editor,"easyView")==nullptr,"A directly constructed editor must keep the full editor");
    auto* open=find<juce::TextButton>(*editor,"easyOpen");require(open && open->isShowing(),"EASY VIEW dock entry missing");
    const auto before=signalPathTests::values(*p);
    editor->setEasyMode(true);settle(150);
    auto* view=find<EasyModeView>(*editor,"easyView");
    require(view && view->isShowing() && !find(*editor,"surface")->isVisible(),"Easy view did not replace the full editor");
    require(signalPathTests::values(*p)==before,"Opening Easy view changed the sound");
    auto* room=find(*view,"cabRoomOverview");require(room && room->isShowing(),"Easy view does not show the cabinet room");
    for(auto* child:view->getChildren())if(child->isVisible())inside(*view,*child,"Easy control outside its view");

    // Amp knobs edit the selected head's own parameters and follow amp changes.
    const auto gainID=view->boundParameter(0);require(gainID.isNotEmpty(),"Easy GAIN is not bound to the default head");
    auto* gain=find<juce::Slider>(*view,"easy.knob.gain");require(gain && gain->isEnabled(),"Easy GAIN knob missing");
    gain->setValue(gain->proportionOfLengthToValue(.8),juce::sendNotificationSync);
    auto* parameter=p->parameters().getParameter(gainID);
    require(std::abs(parameter->getValue()-.8f)<.01f,"Easy GAIN did not edit the head's gain");
    p->setAmpModel(0,spectralforge::niflheimrAmpModel);settle(250);
    require(view->boundParameter(0)!=gainID && view->boundParameter(0).contains("niflheimr"),"Easy knobs did not follow the selected amp");

    // REVERB is 0 while off; turning it up enables the existing reverb.
    set(*p,"reverbon",0);set(*p,"reverbmix",.15f);settle(100);
    auto* reverb=find<juce::Slider>(*view,"easy.knob.reverb");require(reverb && reverb->getValue()==0,"Reverb off must show REVERB at 0");
    require(raw(*p,"reverbon")<.5f,"Showing REVERB at 0 switched the reverb on");
    reverb->setValue(.3,juce::sendNotificationSync);
    require(raw(*p,"reverbon")>.5f && std::abs(raw(*p,"reverbmix")-.3f)<.002f,"Easy REVERB did not enable and set the reverb");
    reverb->setValue(0,juce::sendNotificationSync);
    require(raw(*p,"reverbon")<.5f && std::abs(raw(*p,"reverbmix")-.3f)<.002f,"Easy REVERB at 0 must switch off and keep the mix");
    set(*p,"reverbon",1);settle(50);
    require(std::abs(reverb->getValue()-.3)<.002,"Turning reverb on elsewhere did not update Easy REVERB");
    set(*p,"reverbon",0);settle(50);
    auto* volume=find<juce::Slider>(*view,"easy.knob.volume");volume->setValue(-12,juce::sendNotificationSync);
    require(std::abs(raw(*p,"output")+12)<.01f,"Easy VOLUME did not edit output gain");

    // Presets load through the full editor, so CUSTOM detection stays shared.
    auto* presets=find<juce::ComboBox>(*view,"easy.preset");require(presets,"Easy preset menu missing");
    presets->setSelectedId(spectralforge::presetDisplayOrder()[0]+1,juce::sendNotificationSync);settle(150);
    auto* description=find<juce::Label>(*view,"easy.presetDescription");
    require(description && description->getText().isNotEmpty() && !description->getText().startsWith("Your own"),"Easy preset did not load");
    signalPathTests::capture(*editor,directory,"easy-view");

    auto* tuner=find<juce::TextButton>(*view,"easy.tuner");signalPathTests::click(*tuner);settle(150);
    require(raw(*p,"tuneron")>.5f && find(*view,"precisionTuner")!=nullptr && !room->isVisible(),"Easy TUNER did not replace the room with the tuner");
    signalPathTests::capture(*editor,directory,"easy-view-tuner");
    signalPathTests::click(*tuner);settle(150);
    require(raw(*p,"tuneron")<.5f && find(*view,"precisionTuner")==nullptr && room->isShowing(),"Easy TUNER did not close the tuner");

    const auto edited=signalPathTests::values(*p);
    editor->setEasyMode(false);settle(150);
    require(!view->isVisible() && find(*editor,"surface")->isVisible() && signalPathTests::values(*p)==edited,"Leaving Easy view changed the sound");
    for(int scale:{75,150}){editor->setEasyMode(true);editor->setSize(1180*scale/100,780*scale/100);settle(60);signalPathTests::capture(*editor,directory,"easy-view-"+juce::String(scale));}
    editor.reset();p->releaseResources();
}
inline void run(const juce::File& directory) {
    preference();mapping();editorView(directory);
    std::cout<<"PASS Easy view: preference, head knob mapping, shared parameters, reverb switching, presets, tuner and sound-neutral switching\n";
}
}
