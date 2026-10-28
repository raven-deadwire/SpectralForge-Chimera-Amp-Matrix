#include <juce_cryptography/juce_cryptography.h>
#include "NativeStateTests.h"
#include "CabPanel.h"
#include <iostream>
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    juce::File dir{juce::String(argv[1])};dir.createDirectory();
    nativeStateTests::run(dir);
    auto p=std::make_unique<ChimeraProcessor>();
    auto* toggle=p->parameters().getParameter("cabAcutsOff1");toggle->setValueNotifyingHost(1);
    CabPanel panel(*p,0,{},dir.getChildFile("prefs.json"));
    for(auto view:{CabPanel::View::cabinet,CabPanel::View::irLoader}){
        panel.setView(view);panel.setSize(1040,748);
        auto* button=dynamic_cast<juce::TextButton*>(panel.findChildWithID("cabAcutsOff1"));
        if(!button || !button->getToggleState() || !panel.getLocalBounds().contains(button->getBounds()))throw std::runtime_error("cut bypass UI attachment/bounds");
        juce::PNGImageFormat png;auto out=dir.getChildFile(view==CabPanel::View::cabinet?"cabinet.png":"ir-loader.png").createOutputStream();
        if(!png.writeImageToStream(panel.createComponentSnapshot(panel.getLocalBounds()),*out))throw std::runtime_error("snapshot");
    }
    std::cout<<"PASS native state/ordinals and CAB cut-bypass UI attachments/snapshots\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
