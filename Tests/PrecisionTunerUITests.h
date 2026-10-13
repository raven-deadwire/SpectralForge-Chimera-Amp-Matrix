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
    const auto before=signalPathTests::values(*p);
    auto window=open(*editor,"tunerView","signalPathTuner");
    require(signalPathTests::values(*p)==before,"Opening tuner changed host values");
    auto* panel=dynamic_cast<PrecisionTunerPanel*>(window->getContentComponent());require(panel,"Precision tuner content missing");
    find<juce::TextButton>(*panel,"tunerEnable")->triggerClick();settle(40);
    require(p->parameters().getRawParameterValue("tuneron")->load()>.5f,"Start tuning did not enable detector");
    std::atomic<int> deliveredFrames{0};
    const auto inputStarted=std::chrono::steady_clock::now();
    std::jthread feeder([&](std::stop_token stop){
        juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;int block=0;
        auto next=std::chrono::steady_clock::now();
        const double hz=27.5*std::exp2(.05/1200);
        while(!stop.stop_requested()){
            for(int n=0;n<256;++n){const double phase=juce::MathConstants<double>::twoPi*hz*(block*256+n)/48000;const float x=float(.2*std::sin(phase)+.1*std::sin(2*phase));for(int c=0;c<2;++c)audio.setSample(c,n,x);}
            p->processBlock(audio,midi);deliveredFrames.fetch_add(256);++block;next+=std::chrono::microseconds(5333);std::this_thread::sleep_until(next);
        }
    });
    settle(600);
    const auto measured=p->tuningFrequency();
    std::cout<<"UI acquisition elapsed_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-inputStarted).count()
             <<" delivered_frames="<<deliveredFrames.load()<<" frequency_hz="<<measured<<" confidence="<<p->tuningConfidence()<<std::endl;
    require(std::abs(1200*std::log2(measured/(27.5*std::exp2(.05/1200))))<.1,"Production low A not acquired accurately");
    snapshot(*panel,directory,"strobe-low-A");snapshot(*editor,directory,"editor-tuner-entry");
    feeder.request_stop();feeder.join();
    set(*p,"tunerProfile",7);set(*p,"tunerDisplay",1);set(*p,"tunerString",8);set(*p,"tunerNote8",21);set(*p,"tunerOffset8",-7.3f);set(*p,"tunerref",442);settle(100);
    snapshot(*panel,directory,"custom-targets");
    for(auto* child:panel->getChildren())if(child->isVisible())inside(*panel,*child,"Tuner control outside window");
    juce::MemoryBlock state;p->getStateInformation(state);auto restored=std::make_unique<ChimeraProcessor>();restored->setStateInformation(state.getData(),int(state.getSize()));
    for(const auto* id:{"tunerProfile","tunerDisplay","tunerString","tunerNote8","tunerOffset8","tunerref"})require(std::abs(p->parameters().getRawParameterValue(id)->load()-restored->parameters().getRawParameterValue(id)->load())<.001,"Tuner state recall failed");
    // Preserve released ID/index prefix: append-only registration is checked in
    // Tools/check_tuner_parameter_compatibility.py as well as this old-state test.
    auto legacy=p->parameters().copyState();
    for(int i=legacy.getNumChildren();--i>=0;){const auto id=legacy.getChild(i).getProperty("id").toString();if(id.startsWith("tuner")&&id!="tunerref"&&id!="tuneron"&&id!="tunermute")legacy.removeChild(i,nullptr);}
    juce::MemoryBlock old;juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),old);p->setStateInformation(old.getData(),int(old.getSize()));
    require(p->parameters().getRawParameterValue("tunerProfile")->load()==0,"Legacy state inherited custom temperament");
    require(p->parameters().getRawParameterValue("tunerOffset8")->load()==0,"Legacy state inherited custom offset");
    const auto active=signalPathTests::values(*p);close(window);require(signalPathTests::values(*p)==active,"Closing tuner changed host values");
    p->releaseResources();require(p->backgroundResourcesReleased(),"Tuner teardown failed");
    std::cout<<"PASS native tuner UI, view-neutral host values, production A0, custom/project recall, legacy defaults, close and teardown\n";
}
}
