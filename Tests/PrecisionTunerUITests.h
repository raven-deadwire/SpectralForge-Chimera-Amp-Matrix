#pragma once
#include "NativeUITests.h"
#include "SignalPathUITests.h"
#include <thread>
#include <chrono>
namespace precisionTunerUITests {
using namespace nativeUITests;
inline void run(const juce::File& directory){
    auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,256);
    auto editor=std::make_unique<ChimeraEditor>(*p);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);
    auto* open=find<juce::TextButton>(*editor,"tunerOpen");require(open&&open->isShowing(),"Tuner dock entry missing");
    const auto before=signalPathTests::values(*p);open->triggerClick();settle(80);
    auto* panel=find<PrecisionTunerPanel>(*editor,"precisionTuner");require(panel&&panel->isShowing(),"Tuner workspace did not open");
    require(signalPathTests::values(*p)==before,"Opening tuner changed audio state");
    require(!find(*editor,"cabRoomOverview")->isShowing(),"Underlying rig remains accessible under tuner");
    find<juce::TextButton>(*editor,"tunerEnable")->triggerClick();settle(40);
    require(p->parameters().getRawParameterValue("tuneron")->load()>.5f,"Start tuning did not activate detector");
    juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
    const double hz=spectralforge::tuning::frequency(21,440,.05); // A0, 27.5 Hz + 0.05 cent
    // Audio delivery must not depend on a native UI event pump's scheduling.
    std::atomic<int> deliveredFrames{0};
    const auto inputStarted=std::chrono::steady_clock::now();
    std::jthread feeder([&](std::stop_token stop){
        int block=0;auto next=std::chrono::steady_clock::now();
        while(!stop.stop_requested()){
            for(int n=0;n<256;++n){const double phase=juce::MathConstants<double>::twoPi*hz*(block*256+n)/48000;const float x=float(.2*std::sin(phase)+.1*std::sin(2*phase));for(int c=0;c<2;++c)audio.setSample(c,n,x);}
            p->processBlock(audio,midi);deliveredFrames.fetch_add(256);++block;next+=std::chrono::microseconds(5333);std::this_thread::sleep_until(next);
        }
    });
    settle(600);
    std::cout<<"UI acquisition elapsed_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-inputStarted).count()<<" delivered_frames="<<deliveredFrames.load()<<std::endl;
    const auto measured=p->tuningFrequency();require(measured>0&&p->tuningConfidence()>.9f,"Production tuner did not acquire low A");
    const double cents=1200*std::log2(measured/hz);require(std::abs(cents)<.1,"Production decimator/worker missed synthetic 0.1 cent target");
    std::cout<<"MEASURE production tuner low A: "<<measured<<" Hz / error "<<cents<<" ct\n";
    for(int scale:{75,100,125,150}){editor->setSize(1180*scale/100,780*scale/100);settle(40);for(auto* child:panel->getChildren())if(child->isVisible())inside(*panel,*child,"Tuner control outside workspace");signalPathTests::capture(*editor,directory,"precision-strobe-"+juce::String(scale));}
    feeder.request_stop();feeder.join();
    set(*p,"tunerProfile",7);set(*p,"tunerDisplay",1);set(*p,"tunerString",8);set(*p,"tunerNote8",21);set(*p,"tunerOffset8",-7.3f);set(*p,"tunerref",442);settle(100);
    for(auto* child:panel->getChildren())if(child->isVisible())inside(*panel,*child,"Custom tuner control outside workspace");
    signalPathTests::capture(*editor,directory,"precision-custom");
    juce::MemoryBlock state;p->getStateInformation(state);auto restored=std::make_unique<ChimeraProcessor>();restored->setStateInformation(state.getData(),int(state.getSize()));
    for(const auto* id:{"tunerProfile","tunerDisplay","tunerString","tunerNote8","tunerOffset8","tunerref"})require(std::abs(p->parameters().getRawParameterValue(id)->load()-restored->parameters().getRawParameterValue(id)->load())<.001,"Tuner settings did not survive project recall");
    const auto active=signalPathTests::values(*p);find<juce::TextButton>(*editor,"path.pre")->triggerClick();settle(80);
    require(!panel->isShowing()&&find(*editor,"universalPedalBoard")->isShowing(),"Signal stage did not leave tuner workspace");require(signalPathTests::values(*p)==active,"Leaving tuner workspace changed audio/mute state");
    // Legacy projects must acquire defaults, not inherit the active custom profile.
    auto legacy=p->parameters().copyState();for(int i=legacy.getNumChildren();--i>=0;)if(legacy.getChild(i).getProperty("id").toString().startsWith("tuner")&&legacy.getChild(i).getProperty("id").toString()!="tunerref"&&legacy.getChild(i).getProperty("id").toString()!="tuneron"&&legacy.getChild(i).getProperty("id").toString()!="tunermute")legacy.removeChild(i,nullptr);
    juce::MemoryBlock old;juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),old);p->setStateInformation(old.getData(),int(old.getSize()));require(p->parameters().getRawParameterValue("tunerProfile")->load()==0,"Legacy project inherited custom tuning");
    require(p->parameters().getRawParameterValue("tunerOffset8")->load()==0,"Legacy project inherited custom offset");
    p->releaseResources();require(p->backgroundResourcesReleased(),"Tuner worker did not stop");
    std::cout<<"PASS tuner workspace, audio-neutral navigation, production low A, four scales, custom recall, legacy defaults and worker teardown\n";
}
}
