#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace spectralforge::ui {
// Scrollbars and wheel/trackpad scrolling never turn a drag on a knob into a
// pan. Keyboard focus reveals the destination without changing its value.
class AccessibleViewport : public juce::Viewport, private juce::FocusChangeListener {
public:
    AccessibleViewport() {
        setScrollBarsShown(true,true);
        setScrollBarThickness(16);
        setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
        juce::Desktop::getInstance().addFocusChangeListener(this);
    }
    ~AccessibleViewport() override {juce::Desktop::getInstance().removeFocusChangeListener(this);}
    void reveal(juce::Component& target) {
        auto* content=getViewedComponent();
        if(content==nullptr || !content->isParentOf(&target) || !target.isShowing())return;
        const auto bounds=content->getLocalArea(&target,target.getLocalBounds());
        const auto visible=getViewArea();
        const auto offset=[](int start,int end,int position,int extent) {
            if(start<position)return start;
            if(end>position+extent)return juce::jmax(start,end-extent);
            return position;
        };
        setViewPosition(offset(bounds.getX(),bounds.getRight(),visible.getX(),visible.getWidth()),
                        offset(bounds.getY(),bounds.getBottom(),visible.getY(),visible.getHeight()));
    }
private:
    void globalFocusChanged(juce::Component* component) override {if(component)reveal(*component);}
};
inline juce::Component* dialogContent(juce::DialogWindow& window) {
    auto* content=window.getContentComponent();
    if(auto* viewport=dynamic_cast<AccessibleViewport*>(content))return viewport->getViewedComponent();
    return content;
}

// Preserve the content's readable layout; only the surrounding viewport gets
// smaller. Ownership moves from the window to this viewport exactly once.
inline void fitScrollableDialog(juce::DialogWindow* window,juce::Rectangle<int> area) {
    if(!window || !window->getContentComponent())return;
    auto* content=window->getContentComponent();
    int frameWidth=0,frameHeight=0;
    if(auto* peer=window->getPeer())if(const auto frame=peer->getFrameSizeIfPresent()) {
        frameWidth=frame->getLeftAndRight();frameHeight=frame->getTopAndBottom();
    }
    // Reserve native decorations until the window manager supplies them.
    if(window->isUsingNativeTitleBar() && frameHeight==0){frameWidth=16;frameHeight=48;}
    const int width=juce::jmax(1,area.getWidth()-frameWidth-8-(window->getWidth()-content->getWidth()));
    const int height=juce::jmax(1,area.getHeight()-frameHeight-8-(window->getHeight()-content->getHeight()));
    if(content->getWidth()>width || content->getHeight()>height) {
        if(auto* viewport=dynamic_cast<AccessibleViewport*>(content))viewport->setSize(width,height);
        else {
            const auto original=content->getBounds();
            window->setContentNonOwned(content,true);
            window->clearContentComponent();
            content->setTopLeftPosition(0,0);
            auto* wrapper=new AccessibleViewport();
            wrapper->setComponentID("dialogViewport");
            wrapper->setViewedComponent(content,true);
            wrapper->setSize(juce::jmin(original.getWidth()+16,width),juce::jmin(original.getHeight()+16,height));
            window->setContentOwned(wrapper,true);
        }
    }
    window->setTopLeftPosition(area.getX()+juce::jmax(4,(area.getWidth()-window->getWidth())/2),
                              area.getY()+juce::jmax(frameHeight, (area.getHeight()-window->getHeight())/2));
}
inline void fitScrollableDialog(juce::DialogWindow* window) {
    if(window)if(const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(window->getScreenBounds()))
        fitScrollableDialog(window,display->userArea);
}
}
