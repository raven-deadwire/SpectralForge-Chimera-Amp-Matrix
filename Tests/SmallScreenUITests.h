#pragma once
#include "SignalPathUITests.h"
#include "WindowFit.h"
#include "AccessibleViewport.h"

namespace smallScreenTests {
using namespace nativeUITests;
inline void reachable(ChimeraEditor& editor,spectralforge::ui::AccessibleViewport& viewport,juce::Component& control) {
    viewport.reveal(control);
    const auto bounds=editor.getLocalArea(&control,control.getLocalBounds());
    const auto view=viewport.getLocalBounds().withSize(viewport.getMaximumVisibleWidth(),viewport.getMaximumVisibleHeight());
    require(view.contains(bounds),"Control cannot be completely revealed inside the editor viewport");
    auto* hit=editor.getComponentAt(bounds.getCentre());
    require(hit==&control || control.isParentOf(hit),"Scrolled control hit-test does not match its painted location");
}
inline void run(const juce::File& directory) {
    using namespace spectralforge::ui;
    auto processor=std::make_unique<ChimeraProcessor>();
    processor->loadFactoryPreset(0);
    auto editor=std::make_unique<ChimeraEditor>(*processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);editor->setTopLeftPosition(20,20);editor->toFront(true);settle(100);
    auto* viewport=find<AccessibleViewport>(*editor,"editorViewport");
    auto* surface=editorSurface(*editor);
    require(viewport && surface,"Scrollable editor components missing");
    auto* path=find<SignalPathView>(*editor,"signalPathStrip");
    // Locate the view by type too: its public identity is owned by the path UI.
    if(!path)for(auto* child:surface->getChildren())if(auto* view=dynamic_cast<SignalPathView*>(child))path=view;
    require(viewport && surface && path,"Scrollable editor components missing");
    require(viewport->getScrollOnDragMode()==juce::Viewport::ScrollOnDragMode::never,"Knob drag may pan the editor");
    juce::String csv="pixels,os_scale,mode,work_width,work_height,editor_width,editor_height,ui_scale,hscroll,vscroll,result\n";
    int cases=0,actions=0;
    for(auto pixels:{juce::Point<int>{1280,800},{1366,768},{1920,1080}})for(int percent:{100,125,150})for(int mode=0;mode<3;++mode) {
        set(*processor,"mode",float(mode));settle(50);
        // Explicit simulation: OS taskbar (40 physical px), then native frame
        // + Standalone notification allowance (16 x 64 logical px).
        const juce::Rectangle<int> area{0,0,pixels.x*100/percent,(pixels.y-40)*100/percent};
        const juce::Point<int> chrome{16,64};
        const auto fitted=fittedEditorSize(area,chrome);
        const auto before=signalPathTests::values(*processor);
        signalPathTests::Writes writes;processor->addListener(&writes);
        editor->setViewSize(fitted);settle(30);
        require(editor->getWidth()+chrome.x<=area.getWidth() && editor->getHeight()+chrome.y<=area.getHeight(),"Fitted editor exceeds the simulated work area");
        require(surface->getTransform().mat00>=minimumEditorScale && surface->getTransform().mat11>=minimumEditorScale,"Text shrank below 75 percent");
        if(pixels==juce::Point<int>{1366,768} && percent==125)
            require(viewport->getVerticalScrollBar().isVisible() && !viewport->getHorizontalScrollBar().isVisible(),"1366x768 / 125% gained avoidable horizontal scrolling");
        auto* input=signalPathTests::named<juce::Slider>(*editor,"Input gain");
        require(input && !input->isScrollWheelEnabled(),"Editor slider may change sound during wheel scrolling");
        viewport->setViewPosition(0,0);
        const auto centre=input->getLocalBounds().getCentre().toFloat();const auto now=juce::Time::getCurrentTime();
        const juce::MouseEvent wheelEvent(juce::Desktop::getInstance().getMainMouseSource(),centre,{},1,0,0,0,0,input,input,now,centre,now,0,false);
        juce::MouseWheelDetails wheel;wheel.deltaY=-1.f;
        static_cast<juce::Component&>(*input).mouseWheelMove(wheelEvent,wheel);
        if(viewport->getVerticalScrollBar().isVisible())require(viewport->getViewPositionY()>0,"Wheel on a control did not reach the editor scrollbar");
        for(const auto* stage:signalPathTests::compactStages) {
            auto& button=signalPathTests::pathButton(*path,stage);
            reachable(*editor,*viewport,button);
            signalPathTests::click(button);
            signalPathTests::compactDestination(*editor,*path,stage,"small screen");
            ++actions;
            // Right/bottom edges of the complete layout must be reachable.
            viewport->setViewPosition(100000,100000);
            require(viewport->getViewArea().getRight()>=viewport->getViewedComponent()->getWidth()
                    && viewport->getViewArea().getBottom()>=viewport->getViewedComponent()->getHeight(),"Scroll range cannot reach the last pixel");
            const auto name=juce::String(pixels.x)+"x"+juce::String(pixels.y)+"-"+juce::String(percent)+"-mode"+juce::String(mode)+"-"+stage;
            if(mode==0)signalPathTests::capture(*editor,directory,name);
        }
        juce::TextButton* settings=nullptr;
        for(auto* child:surface->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child);button && button->getButtonText()=="SETTINGS")settings=button;
        require(settings,"SETTINGS was removed");
        viewport->setViewPosition(0,0);settings->grabKeyboardFocus();settle(30);
        require(settings->hasKeyboardFocus(false),"SETTINGS cannot receive keyboard focus");
        require(viewport->getViewArea().contains(viewport->getViewedComponent()->getLocalArea(settings,settings->getLocalBounds())),"Keyboard focus did not reveal SETTINGS");
        reachable(*editor,*viewport,*settings);
        signalPathTests::click(*settings);settle(30);
        require(juce::PopupMenu::dismissAllActiveMenus(),"SETTINGS popup did not open");settle(30);++actions;
        // Explicit readable zoom may scroll; a fit/restore round trip retains
        // the exact dimensions, requested zoom, scroll offset and sound state.
        editor->setViewSize(fitted,1.f);viewport->setViewPosition(100000,100000);
        const auto scroll=editor->viewPosition();
        editor->setViewSize({1180,780});editor->setViewSize(fitted,1.f);editor->setViewPosition(scroll);
        require(editor->viewPosition()==scroll && editor->preferredViewScale()==1.f,"View restore lost zoom or scroll offset");
        require(surface->getTransform().mat00==1.f,"Explicit 100 percent zoom was silently reduced");
        require(signalPathTests::values(*processor)==before && writes.changes==0 && writes.gestures==0,"Layout/navigation changed parameters or emitted host automation");
        processor->removeListener(&writes);
        editor->setViewSize(fitted);
        csv+=juce::String(pixels.x)+"x"+juce::String(pixels.y)+","+juce::String(percent)+","+juce::String(mode)+","+juce::String(area.getWidth())+","+juce::String(area.getHeight())+","+juce::String(fitted.x)+","+juce::String(fitted.y)+","+juce::String(surface->getTransform().mat00,4)+","+juce::String(int(viewport->getHorizontalScrollBar().isVisible()))+","+juce::String(int(viewport->getVerticalScrollBar().isVisible()))+",PASS\n";
        ++cases;
    }
    // Dialog viewport ownership, readable content and both scroll endpoints.
    struct Dialog final:juce::DialogWindow {
        Dialog():DialogWindow("Small screen test",juce::Colours::black,true){}
        void closeButtonPressed() override {}
    };
    for(auto size:{juce::Point<int>{1200,650},{1040,792},{920,480},{760,720}}) {
        Dialog dialog;auto* content=new juce::Component();content->setSize(size.x,size.y);
        dialog.setContentOwned(content,true);dialog.setVisible(true);
        fitScrollableDialog(&dialog,{0,0,853,506});
        auto* scroll=dynamic_cast<AccessibleViewport*>(dialog.getContentComponent());
        require(scroll && scroll->getViewedComponent()==content && content->getWidth()==size.x && content->getHeight()==size.y,"Dialog lost its content or shrank its readable layout");
        require(dialog.getWidth()<=853 && dialog.getHeight()<=506,"Dialog does not fit simulated work area");
        scroll->setViewPosition(100000,100000);
        require(scroll->getViewArea().getRight()>=size.x && scroll->getViewArea().getBottom()>=size.y,"Dialog bottom/right controls are unreachable");
    }
    const auto saved=signalPathTests::values(*processor);editor.reset();
    editor=std::make_unique<ChimeraEditor>(*processor);
    require(signalPathTests::values(*processor)==saved,"Reopening the editor changed preset values");
    require(directory.getChildFile("small-screen-matrix.csv").replaceWithText(csv),"Cannot write screen matrix evidence");
    std::cout<<"PASS small screen: "<<cases<<" simulated resolution/DPI/routing combinations, "<<actions<<" navigation/settings actions; focus, hit-testing, readable zoom, restore, dialog ownership, zero parameter writes/gestures. Physical OS DPI/DAW acceptance remains separate.\n";
}
}
