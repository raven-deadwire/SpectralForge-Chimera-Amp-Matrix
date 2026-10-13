#pragma once
#include "SignalPathUITests.h"
namespace commonDynamicsUITests {
using namespace nativeUITests;
inline void run(const juce::File& directory){
    auto p=std::make_unique<ChimeraProcessor>();auto editor=std::make_unique<ChimeraEditor>(*p);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);
    auto* tone=find<juce::TextButton>(*editor,"path.tone.eq");require(tone,"Tone EQ navigation missing");signalPathTests::click(*tone);settle(60);
    auto* open=find<juce::TextButton>(*editor,"commonDynamics.open");require(open && open->isShowing(),"Dynamics entry missing");
    const auto before=signalPathTests::values(*p);signalPathTests::click(*open);settle(60);
    auto* panel=find<spectralforge::CommonDynamicsPanel>(*editor,"commonDynamics.panel");require(panel && panel->isShowing(),"Dynamics workspace did not open");
    require(signalPathTests::values(*p)==before,"Opening dynamics changed sound state");
    for(int scale:{75,100,125,150}){editor->setSize(1180*scale/100,780*scale/100);settle(40);
        for(const auto* id:{"cleanCompOn","cleanCompThreshold","cleanCompMix","gateMode","gateRangeDb","liveLimiterOn","liveLimiterCeiling","dynamicEQ4_target","dynamicEQ4_ratio"}){
            auto* c=find(*panel,id);require(c && c->isShowing() && editor->getLocalBounds().contains(editor->getLocalArea(c,c->getLocalBounds())),"Dynamics control clipped or absent");
        }
        signalPathTests::capture(*editor,directory,"common-dynamics-"+juce::String(scale));
    }
    editor->setSize(1180,780);settle(60);signalPathTests::click(*find<juce::TextButton>(*panel,"cleanCompOn"));settle(40);
    require(p->parameters().getRawParameterValue("cleanCompOn")->load()>.5f,"Compressor switch not connected");
    for(int s=0;s<4;++s){auto* target=find<juce::ComboBox>(*panel,spectralforge::dynamicSlotID(s,"target"));target->setSelectedId(s+2,juce::sendNotificationSync);}
    settle(60);auto* second=find<juce::ComboBox>(*panel,"dynamicEQ2_target");second->setSelectedId(2,juce::sendNotificationSync);settle(40);
    require(p->parameters().getRawParameterValue("dynamicEQ1_target")->load()==1 && p->parameters().getRawParameterValue("dynamicEQ2_target")->load()==2,"Duplicate assignment displaced an existing node");
    signalPathTests::capture(*editor,directory,"common-dynamics-four-slots");
    signalPathTests::click(*find<juce::TextButton>(*editor,"tunerOpen"));settle(60);require(!panel->isShowing(),"Dynamics remains active under tuner workspace");
    signalPathTests::click(*tone);settle(60);require(find(*editor,"toneEQ_panel")->isShowing()&&!panel->isShowing(),"Tone signal did not leave dynamics/tuner");
    std::cout<<"PASS native dynamics navigation, audio-neutral opening, four scales, compressor control and non-destructive four-slot assignment\n";
}
}
