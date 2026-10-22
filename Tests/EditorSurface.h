#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
inline juce::Component* editorSurface(juce::Component& root) {
    if(root.getComponentID()=="surface")return &root;
    for(auto* child:root.getChildren())if(auto* surface=editorSurface(*child))return surface;
    return nullptr;
}
