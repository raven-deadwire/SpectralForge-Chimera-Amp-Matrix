#pragma once
#include "PluginEditor.h"
#include "KnobInteraction.h"
#include <stdexcept>
#include <vector>

// Issue #113: interface-size discovery, letterboxed free-aspect windows and the
// shared knob mouse policy. Offline editor checks; Windows title-bar maximise,
// F11 full screen and real trackpad feel still need the installed Standalone.
namespace windowScaleUITests {
inline void collectSliders(juce::Component& root,std::vector<juce::Slider*>& sliders)
{
    if(auto* slider=dynamic_cast<juce::Slider*>(&root))sliders.push_back(slider);
    for(auto* child:root.getChildren())collectSliders(*child,sliders);
}

inline void run(const juce::File& directory)
{
    const auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    const auto settle=[]{juce::MessageManager::getInstance()->runDispatchLoopUntil(60);};
    auto processor=std::make_unique<ChimeraProcessor>();
    auto editor=std::make_unique<ChimeraEditor>(*processor);
    editor->setSize(1180,780);settle();
    auto* canvas=editor->findChildWithID("surface");require(canvas!=nullptr,"Missing editor canvas");

    juce::ComboBox* size=nullptr;
    for(auto* child:canvas->getChildren())if(auto* box=dynamic_cast<juce::ComboBox*>(child);box && box->getName()=="Interface size")size=box;
    require(size!=nullptr,"Interface size selector missing");
    require(size->getNumItems()==5 && size->getItemId(4)==20 && size->getItemText(4)=="Fit to screen",
            "Interface size must offer 75-150% and Fit to screen");
    require(size->indexOfItemId(21)<0,"Full screen is a Standalone-only command");

    const auto translation=[&]{const auto t=canvas->getTransform();return juce::Point<float>(t.mat02,t.mat12);};
    const auto scale=[&]{return canvas->getTransform().mat00;};
    // Fixed-ratio sizes keep the existing geometry exactly.
    for(const auto& [width,height,factor]:std::vector<std::tuple<int,int,float>>{{885,585,.75f},{1180,780,1.f},{1770,1170,1.5f}}) {
        editor->setSize(width,height);settle();
        require(std::abs(scale()-factor)<1.e-4f && translation().getDistanceFromOrigin()<.5f,"Fixed-ratio scale changed");
    }
    // A maximised Standalone window may have any aspect ratio: letterbox, never stretch or crop.
    editor->setSize(1400,780);settle();
    require(std::abs(scale()-1.f)<1.e-4f && std::abs(translation().x-110.f)<.6f && std::abs(translation().y)<.5f,"Wide window is not centred");
    require(size->getText()=="100%","Size selector must report the letterboxed scale");
    if(directory.isDirectory())if(auto output=directory.getChildFile("Window-letterbox-1400x780.png").createOutputStream())
        juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),*output);
    editor->setSize(1180,900);settle();
    require(std::abs(scale()-1.f)<1.e-4f && std::abs(translation().y-60.f)<.6f,"Tall window is not centred");
    editor->setSize(2560,1080);settle();
    require(std::abs(scale()-1080.f/780.f)<1.e-3f,"Large maximised window must use the limiting dimension");
    auto* input=dynamic_cast<juce::Slider*>(canvas->findChildWithID("gateRangeDb"));
    require(input!=nullptr,"Range control missing");
    const auto screen=editor->getLocalArea(input,input->getLocalBounds());
    require(editor->getLocalBounds().contains(screen),"Control outside the letterboxed editor");

    // Fit to screen chooses a supported size inside the display.
    size->setSelectedId(20,juce::sendNotificationSync);settle();
    const float fitted=editor->getWidth()/1180.f;
    require(fitted>=.75f-1.e-3f && fitted<=2.f+1.e-3f && std::abs(editor->getHeight()-780.f*fitted)<1.5f,"Fit to screen produced an unsupported size");
    if(const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();display && fitted>.751f)
        require(editor->getWidth()<=display->userArea.getWidth() && editor->getHeight()<=display->userArea.getHeight(),"Fit to screen exceeds the display");
    require(size->getText()!="Fit to screen","Size selector kept the command text");

    // Every slider follows the same physical-pixel, Shift-fine, no-jump, no-scroll policy.
    for(const auto& [width,height]:std::vector<std::pair<int,int>>{{885,585},{1180,780},{1770,1170}}) {
        editor->setSize(width,height);settle();spectralforge::ui::KnobInteraction::apply(*editor);
        std::vector<juce::Slider*> sliders;collectSliders(*editor,sliders);
        require(sliders.size()>20,"Editor sliders not found");
        const int expected=juce::roundToInt(250.0/(width/1180.0));
        for(auto* slider:sliders) {
            const auto& properties=slider->getProperties();
            require((bool)properties["knobPolicy"],"Slider missed the shared knob policy");
            require(!slider->getSliderSnapsToMousePosition(),"Click on a slider track still jumps the value");
            require(slider->getVelocityModeIsSwappable(),"Shift fine adjustment unavailable");
            if(!(bool)properties["wheelAllowed"])require(!slider->isScrollWheelEnabled(),"Scrolling past a control can change its value");
            if(slider->getParentComponent()==canvas)require(slider->getMouseDragSensitivity()==expected,"Drag travel is not scale independent");
        }
    }
    editor->setSize(1180,780);settle();
    std::cout<<"PASS window scale: Fit to screen, letterboxed free-aspect sizes and shared knob policy\n";
}
}
