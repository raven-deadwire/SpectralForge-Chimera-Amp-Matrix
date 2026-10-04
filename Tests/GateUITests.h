#pragma once
#include "PluginEditor.h"
#include <stdexcept>

// Run from the native Windows suite; also available to the offline probe under Xvfb.
inline void checkGateUI(ChimeraProcessor& processor,ChimeraEditor& editor,const juce::File& directory)
{
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    auto* canvas=editor.findChildWithID("surface");require(canvas!=nullptr,"Missing editor canvas");
    auto* slider=dynamic_cast<juce::Slider*>(canvas->findChildWithID("gateRangeDb"));
    require(slider!=nullptr,"Range control missing");
    const float previous=processor.parameters().getRawParameterValue("gateRangeDb")->load();
    for(int width:{885,1180,1475}) {
        editor.setSize(width,width*780/1180);
        require(slider->isVisible() && editor.getLocalBounds().contains(editor.getLocalArea(slider,slider->getLocalBounds())),"Range outside editor at supported scale");
        slider->setValue(24,juce::sendNotificationSync);
        require(processor.parameters().getRawParameterValue("gateRangeDb")->load()==24,"UI Range edit did not reach DSP parameter");
        auto* parameter=processor.parameters().getParameter("gateRangeDb");parameter->setValueNotifyingHost(1.f);
        require(slider->getValue()==96 && slider->getTextFromValue(96)=="Full","Host Range automation did not update UI Full label");
        if(directory.isDirectory()) {
            auto output=directory.getChildFile("Gate-"+juce::String(width)+".png").createOutputStream();
            require(output!=nullptr,"Cannot write Gate screenshot");
            juce::PNGImageFormat png;require(png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),*output),"Cannot encode Gate screenshot");
        }
    }
    slider->setValue(previous,juce::sendNotificationSync);editor.setSize(1180,780);
}
