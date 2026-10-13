#include "PluginEditor.h"
#include "PrecisionTunerPanel.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <atomic>
int main(){juce::ScopedJuceInitialiser_GUI init;std::cout<<std::unitbuf;
 auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,256);
 auto e=std::make_unique<ChimeraEditor>(*p);e->addToDesktop(juce::ComponentPeer::windowIsTemporary);e->setVisible(true);
 PrecisionTunerPanel panel(*p);panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);panel.setVisible(true);
 juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
 auto* on=p->parameters().getParameter("tuneron");on->setValueNotifyingHost(1);
 std::atomic<int> frames{0};std::atomic<double> pushMaximum{0};
 std::jthread feeder([&](std::stop_token stop){juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;int block=0;auto next=std::chrono::steady_clock::now();
 while(!stop.stop_requested()){
  for(int n=0;n<256;++n){double phase=juce::MathConstants<double>::twoPi*(27.5*std::exp2(.05/1200))*(block*256+n)/48000;for(int c=0;c<2;++c)audio.setSample(c,n,float(.2*std::sin(phase)+.1*std::sin(2*phase)));}
  auto start=std::chrono::steady_clock::now();p->processBlock(audio,midi);const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();pushMaximum.store(std::max(pushMaximum.load(),ms));++block;frames.store(block*256);next+=std::chrono::microseconds(5333);std::this_thread::sleep_until(next);
 }});
 auto start=std::chrono::steady_clock::now();for(int i=0;i<15;++i){juce::MessageManager::getInstance()->runDispatchLoopUntil(100);std::cout<<"elapsed_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" delivered_frames="<<frames.load()<<" signal_ms="<<frames.load()/48.<<" hz="<<p->tuningFrequency()<<" confidence="<<p->tuningConfidence()<<" process_max_ms="<<pushMaximum.load()<<'\n';}
 feeder.request_stop();feeder.join();p->releaseResources();return 0;
}
