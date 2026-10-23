#pragma once
#include "SignalPathUITests.h"
#include "WindowFit.h"
#include "AccessibleViewport.h"

namespace smallScreenTests {
using namespace nativeUITests;
inline void reachable(ChimeraEditor& editor,spectralforge::ui::AccessibleViewport& viewport,juce::Component& control) {
    // Reveal inner panel viewports before the transformed editor viewport.
    // Moving only the editor canvas cannot unclip an AMP/POST control inside it.
    for(auto* parent=control.getParentComponent();parent && parent!=&viewport;parent=parent->getParentComponent())
        if(auto* inner=dynamic_cast<juce::Viewport*>(parent)) {
            auto* content=inner->getViewedComponent();
            require(content && content->isParentOf(&control),"Control has no owning viewport content");
            const auto target=content->getLocalArea(&control,control.getLocalBounds());
            const auto visible=inner->getViewArea();
            const auto offset=[](int start,int end,int position,int extent) {
                return start<position?start:end>position+extent?juce::jmax(start,end-extent):position;
            };
            inner->setViewPosition(offset(target.getX(),target.getRight(),visible.getX(),visible.getWidth()),
                                   offset(target.getY(),target.getBottom(),visible.getY(),visible.getHeight()));
            require(inner->getViewArea().contains(target),"Inner viewport cannot reveal the complete control");
        }
    viewport.reveal(control);
    // A hover tooltip from the previous action is transient and may cover
    // this target on small screens; it is not part of the painted layout.
    for(auto* child:editor.getChildren())
        if(auto* tip=dynamic_cast<juce::TooltipWindow*>(child))tip->hideTip();
    const auto bounds=editor.getLocalArea(&control,control.getLocalBounds());
    const auto view=viewport.getLocalBounds().withSize(viewport.getMaximumVisibleWidth(),viewport.getMaximumVisibleHeight());
    require(view.contains(bounds),"Control cannot be completely revealed inside the editor viewport");
    auto* hit=editor.getComponentAt(bounds.getCentre());
    if(hit!=&control && !control.isParentOf(hit)) {
        std::cerr<<"SCROLL HIT target="<<control.getComponentID()<<" name="<<control.getName()
                 <<" bounds="<<bounds.toString()<<" view="<<viewport.getViewArea().toString()
                 <<" hit="<<(hit?hit->getComponentID():juce::String("null"))
                 <<" hit-name="<<(hit?hit->getName():juce::String())<<'\n';
        for(auto* parent=control.getParentComponent();parent;parent=parent->getParentComponent())
            std::cerr<<"SCROLL ancestor="<<parent->getComponentID()<<" bounds="<<parent->getBounds().toString()<<'\n';
    }
    require(hit==&control || control.isParentOf(hit),"Scrolled control hit-test does not match its painted location");
}

inline void exerciseControl(ChimeraProcessor& processor,ChimeraEditor& editor,
                            spectralforge::ui::AccessibleViewport& viewport,
                            juce::Component& root,const juce::String& id,const juce::String& activationID={}) {
    auto* control=find<juce::Slider>(root,id);
    auto* parameter=processor.parameters().getParameter(id);
    require(control && parameter && control->isShowing() && control->isEnabled(),"Integrated compressor control is unavailable");
    reachable(editor,viewport,*control);
    const auto before=signalPathTests::values(processor);
    struct Automation final:juce::AudioProcessorListener {
        ChimeraProcessor& processor;int index,activation,changes{},begins{},ends{},foreign{};
        Automation(ChimeraProcessor& p,int i,int a):processor(p),index(i),activation(a){processor.addListener(this);}
        ~Automation() override {processor.removeListener(this);}
        void audioProcessorParameterChanged(juce::AudioProcessor*,int i,float) override {if(i==index)++changes;else if(i!=activation)++foreign;}
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
        void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int i) override {if(i==index)++begins;else if(i!=activation)++foreign;}
        void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int i) override {if(i==index)++ends;else if(i!=activation)++foreign;}
    };
    const auto* activation=processor.parameters().getParameter(activationID);
    // Existing POST edits emit source-selection gestures even when already
    // active. Preserve that contract while requiring its value to stay intact.
    Automation automation(processor,parameter->getParameterIndex(),activation?activation->getParameterIndex():-1);
    const auto start=juce::Point<float>{control->getWidth()*.5f,control->getHeight()*.35f};
    const auto finish=start+juce::Point<float>{0,-24};
    juce::Desktop::setMousePosition(control->localPointToGlobal(start).roundToInt());
    const auto now=juce::Time::getCurrentTime();
    const auto event=[&](juce::Point<float> position,bool down) {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),position,
            juce::ModifierKeys(down?juce::ModifierKeys::leftButtonModifier:0),1,0,0,0,0,
            control,control,now,start,now,1,position!=start);
    };
    static_cast<juce::Component&>(*control).mouseDown(event(start,true));
    static_cast<juce::Component&>(*control).mouseDrag(event(finish,true));
    static_cast<juce::Component&>(*control).mouseUp(event(finish,false));settle(20);
    require(automation.changes>0 && automation.begins>0 && automation.begins==automation.ends && automation.foreign==0,
            "Scrolled compressor drag lost balanced host gestures or edited another parameter");
    require(parameter->getValue()!=before[size_t(parameter->getParameterIndex())],"Scrolled compressor drag did not change its parameter");
    const auto edited=signalPathTests::values(processor);
    for(size_t i=0;i<edited.size();++i)if(int(i)!=parameter->getParameterIndex())
        require(edited[i]==before[i],"Compressor drag changed another audio parameter");
    parameter->setValueNotifyingHost(parameter->getValue()>.5f?.23f:.77f);settle(20);
    require(std::abs(parameter->convertTo0to1(float(control->getValue()))-parameter->getValue())<.002f,
            "Host automation did not update the scrolled compressor control");
    parameter->setValueNotifyingHost(before[size_t(parameter->getParameterIndex())]);settle(20);
    require(signalPathTests::values(processor)==before,"Compressor edit/automation did not restore the complete parameter state");
}
inline void integratedWorkspaces(ChimeraProcessor& processor,ChimeraEditor& editor,
                                 spectralforge::ui::AccessibleViewport& viewport,int mode) {
    const auto before=signalPathTests::values(processor);
    {
        signalPathTests::Writes writes;processor.addListener(&writes);
        for(const auto* id:{"commonDynamics.open","eq.bank.tone","eq.bank.final"}) {
            auto* button=find<juce::TextButton>(editor,id);require(button,"Global workspace entry missing");
            reachable(editor,viewport,*button);signalPathTests::click(*button);
            require(button->getToggleState(),"Global workspace entry did not select its workspace");
            auto* panel=find(editor,juce::String(id)=="commonDynamics.open"?"commonDynamics.panel":
                juce::String(id)=="eq.bank.tone"?"toneEQ_panel":"finalEQ_panel");
            require(panel && panel->isShowing() && editorBoundsReachable(editor,*panel),"Workspace cannot be reached after top-bar navigation");
        }
        processor.removeListener(&writes);
        require(signalPathTests::values(processor)==before && writes.changes==0 && writes.gestures==0,
                "Global workspace navigation emitted audio changes or automation");
    }
    auto* canvas=editorSurface(editor);require(canvas,"Integrated canvas missing");
    tab(*canvas,"PRE");
    auto* board=find<PedalBoardPanel>(editor,"universalPedalBoard");require(board,"PRE board missing");
    board->navigateOwner(-1);settle(30);
    exerciseControl(processor,editor,viewport,*board,spectralforge::pedalControlID(0,6,0));
    // Factory presets may reorder cards: stable owner 0 is not display slot 0.
    const auto order=processor.pedalBoardState().order;
    const auto position=std::find(order.begin(),order.end(),0);
    require(position!=order.end(),"PRE compressor owner is missing from the board order");
    auto* detail=find<juce::TextButton>(*board,"boardDetailAt"+juce::String(int(position-order.begin())));
    require(detail && detail->isShowing() && detail->isEnabled(),"PRE DETAIL missing");
    reachable(editor,viewport,*detail);signalPathTests::click(*detail);
    require(find(*board,"boardCompressorSpectrum") && find(*board,"boardCompressorSpectrum")->isShowing(),"PRE DETAIL lost its spectrum");
    require(processor.dynamicsAnalysis().preOwner.load()==0,"PRE DETAIL selected another compressor owner");
    board->navigateOwner(-1);
    tab(*canvas,"POST");
    auto* post=find<PostNativePanel>(editor,"postNativePanel0");require(post,"POST compressor missing");
    // Pick an actually visible front-panel control by its stable parameter ID.
    std::vector<NativeControlView*> controls;views(*post,controls);
    juce::Slider* front=nullptr;
    for(auto* c:controls)if(c->slider.isShowing() && c->slider.isEnabled()){front=&c->slider;break;}
    require(front,"POST front-panel compressor knobs missing");
    exerciseControl(processor,editor,viewport,*post,front->getComponentID(),spectralforge::postNativeModeID(0));
    auto window=open(*post,"postExpand0","postNativePanel0");
    require(find(*window,"postCompressorSpectrum") && find(*window,"postCompressorSpectrum")->isShowing(),"POST ALL lost its spectrum");
    close(window);
    if(mode==2) {
        tab(*canvas,"RIGS");
        auto* head=find<AmpNativePanel>(editor,"ampNativePanel1");require(head,"Matrix LOW head missing");
        exerciseControl(processor,editor,viewport,*head,"lowcomp");
    }
    require(signalPathTests::values(processor)==before,"Integrated navigation/editing changed unrelated preset values");
    juce::MemoryBlock state;processor.getStateInformation(state);
    processor.loadFactoryPreset(1);settle(30);
    processor.setStateInformation(state.getData(),int(state.getSize()));settle(60);
    require(signalPathTests::values(processor)==before,"Preset replacement and project recall lost parameter values");
    require(int(processor.parameters().getRawParameterValue("mode")->load())==mode,"Project recall lost routing mode");
    tab(*canvas,"PRE");
    auto* restored=find<juce::Slider>(editor,spectralforge::pedalControlID(0,6,0));
    require(restored && restored->isShowing() && std::abs(float(restored->getValue())-
        processor.parameters().getRawParameterValue(spectralforge::pedalControlID(0,6,0))->load())<.002f,
        "Preset recall left a stale PRE control attachment");
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
        std::cout<<"RUN small screen: "<<pixels.x<<"x"<<pixels.y<<", "<<percent<<"% OS scale, mode "<<mode<<"\n";
        processor->loadFactoryPreset(0);
        processor->setPedalModel(0,6);processor->setPedalBoardEnabled(true);
        set(*processor,spectralforge::postNativeModeID(0),1);set(*processor,spectralforge::postNativeModelID(0),0);
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
        juce::MouseWheelDetails wheel{};wheel.deltaY=-1.f;
        static_cast<juce::Component&>(*input).mouseWheelMove(wheelEvent,wheel);
        if(viewport->getVerticalScrollBar().isVisible())require(viewport->getViewPositionY()>0,"Wheel on a control did not reach the editor scrollbar");
        for(const auto* stage:signalPathTests::compactStages) {
            auto& button=signalPathTests::pathButton(*path,stage);
            reachable(*editor,*viewport,button);
            signalPathTests::click(button);
            signalPathTests::compactDestination(*editor,*path,stage,"small screen");
            ++actions;
            for(const auto* id:{"commonDynamics.open","eq.bank.tone","eq.bank.final"}) {
                auto* entry=find<juce::TextButton>(*editor,id);
                require(entry && entry->isShowing() && entry->isEnabled(),"Top-bar entry is unavailable from a signal-path page");
                reachable(*editor,*viewport,*entry);
            }
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
        // Observe opening separately from dismissal, then finish teardown before
        // the next case. Keep the pointer on the newly revealed popup target.
        auto* modals=juce::ModalComponentManager::getInstance();
        waitForNativeSpectrum([&]{return modals->getNumModalComponents()==0;},"Previous dialog/menu did not finish closing");
        juce::Desktop::setMousePosition(settings->localPointToGlobal(settings->getLocalBounds().getCentre()));
        settle(30);
        struct Receipt final:juce::Button::Listener {
            juce::Button& button;int clicks{};
            explicit Receipt(juce::Button& b):button(b){button.addListener(this);}
            ~Receipt() override {button.removeListener(this);}
            void buttonClicked(juce::Button*) override {++clicks;}
        } receipt(*settings);
        // A CI runner's test process may never be the foreground app; JUCE then
        // dismisses a popup on its next timer tick unless the pointer is over it.
        // The menu opens synchronously inside the click, so observe it there.
        bool opened=false,menu=false;
        const auto original=settings->onClick;
        settings->onClick=[&]{original();opened=modals->getNumModalComponents()>0;menu=juce::PopupMenu::dismissAllActiveMenus();};
        settings->triggerClick();
        waitForNativeSpectrum([&]{return receipt.clicks>0;},"SETTINGS command was not delivered");
        settings->onClick=original;
        require(receipt.clicks==1,"SETTINGS command was delivered more than once");
        require(opened,"SETTINGS popup did not open");
        require(menu,"SETTINGS modal component was not a popup menu");
        waitForNativeSpectrum([&]{return modals->getNumModalComponents()==0;},"SETTINGS popup did not finish closing");
        ++actions;
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
        integratedWorkspaces(*processor,*editor,*viewport,mode);
        editor->setViewSize(fitted);
        if(viewport->getViewedComponent()->getWidth()<=editor->getWidth()
           && viewport->getViewedComponent()->getHeight()<=editor->getHeight())
            require(!viewport->getHorizontalScrollBar().isVisible() && !viewport->getVerticalScrollBar().isVisible(),"Fit screen retained stale scrollbars after readable zoom");
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
    std::cout<<"PASS small screen: "<<cases<<" simulated resolution/DPI/routing combinations, "<<actions<<" navigation/settings actions; Dynamics/Tone EQ/Final EQ entries, PRE/POST/LOW drag+host automation, preset/project recall; focus, hit-testing, readable zoom, restore, dialog ownership, zero navigation parameter writes/gestures. Physical OS DPI/DAW acceptance remains separate.\n";
}
}
