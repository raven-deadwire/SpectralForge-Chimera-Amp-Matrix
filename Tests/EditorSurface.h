#pragma once
#include "AccessibleViewport.h"
#include <juce_gui_basics/juce_gui_basics.h>
inline juce::Component* editorSurface(juce::Component& root) {
    if(root.getComponentID()=="surface")return &root;
    for(auto* child:root.getChildren())if(auto* surface=editorSurface(*child))return surface;
    return nullptr;
}
// A viewport may intentionally show only part of a readable canvas. Require
// both ends of the target to be reachable, including after a window manager
// bounds an oversized test window. Ordinary non-editor bounds stay strict.
inline bool editorBoundsReachable(juce::Component& root,juce::Component& control) {
    for(auto* parent=control.getParentComponent();parent && parent!=&root;parent=parent->getParentComponent()) {
        auto* viewport=dynamic_cast<spectralforge::ui::AccessibleViewport*>(parent);
        if(!viewport || viewport->getComponentID()!="editorViewport")continue;
        auto* content=viewport->getViewedComponent();
        if(!content || !root.isParentOf(viewport))return false;
        const auto bounds=content->getLocalArea(&control,control.getLocalBounds());
        if(bounds.isEmpty() || !content->getLocalBounds().contains(bounds))return false;
        const auto previous=viewport->getViewPosition();
        viewport->setViewPosition(bounds.getPosition());
        const bool first=viewport->getViewArea().contains(bounds.getPosition());
        viewport->setViewPosition(bounds.getRight()-viewport->getMaximumVisibleWidth(),
                                  bounds.getBottom()-viewport->getMaximumVisibleHeight());
        const bool last=viewport->getViewArea().contains({bounds.getRight()-1,bounds.getBottom()-1});
        viewport->setViewPosition(previous);
        return first && last;
    }
    return root.getLocalBounds().contains(root.getLocalArea(&control,control.getLocalBounds()));
}
