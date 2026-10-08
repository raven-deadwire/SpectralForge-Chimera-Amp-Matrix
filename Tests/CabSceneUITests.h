#pragma once
#include "CabMicrophoneUITests.h"
#include "CabScene.h"
#include "CabRoom.h"
#include "PluginEditor.h"
#include <atomic>
#include <thread>

namespace cabSceneUITests {
using namespace cabMicrophoneUITests;

inline std::vector<float> parameterValues(const ChimeraProcessor& processor) {
    std::vector<float> result;
    for(auto* parameter:processor.getParameters())result.push_back(parameter->getValue());
    return result;
}

inline int parameterIndex(ChimeraProcessor& processor,const juce::String& id) {
    auto* parameter=processor.parameters().getParameter(id);
    require(parameter!=nullptr,"CAB gesture referenced an unknown host parameter");
    return parameter->getParameterIndex();
}

inline void unchangedExcept(ChimeraProcessor& processor,const std::vector<float>& before,
                            std::initializer_list<juce::String> allowed) {
    const auto after=parameterValues(processor);
    require(before.size()==after.size(),"CAB interaction changed the host parameter registry");
    std::vector<int> indices;
    for(const auto& id:allowed)indices.push_back(parameterIndex(processor,id));
    for(size_t i=0;i<before.size();++i)if(std::find(indices.begin(),indices.end(),int(i))==indices.end())
        if(before[i]!=after[i])throw std::runtime_error("CAB interaction changed unrelated host parameter "+std::to_string(i));
}

// Observe the real host notification interface, including gestures that leave
// the final value unchanged. Value snapshots alone cannot detect those writes.
class HostEvents : private juce::AudioProcessorParameter::Listener {
    struct Event {
        std::atomic<int> values{},starts{},ends{},open{};
    };
    ChimeraProcessor& processor;
    std::unique_ptr<Event[]> events;
    int count;
    std::atomic<bool> invalid{};
    void parameterValueChanged(int index,float) override {
        if(index>=0 && index<count)++events[size_t(index)].values;
        else invalid=true;
    }
    void parameterGestureChanged(int index,bool starting) override {
        if(index<0 || index>=count){invalid=true;return;}
        auto& event=events[size_t(index)];
        if(starting) {
            ++event.starts;if(event.open.fetch_add(1)!=0)invalid=true;
        } else {
            ++event.ends;if(event.open.fetch_sub(1)!=1)invalid=true;
        }
    }
public:
    explicit HostEvents(ChimeraProcessor& value):processor(value),
        events(std::make_unique<Event[]>(size_t(value.getParameters().size()))),count(value.getParameters().size()) {
        for(auto* parameter:processor.getParameters())parameter->addListener(this);
    }
    ~HostEvents() override {for(auto* parameter:processor.getParameters())parameter->removeListener(this);}
    void reset() {
        for(int i=0;i<count;++i) {
            auto& event=events[size_t(i)];
            require(event.open.load()==0,"Resetting CAB host audit with an unclosed gesture");
            event.values=0;event.starts=0;event.ends=0;
        }
        require(!invalid.load(),"CAB emitted an invalid or nested host gesture");
    }
    void expect(std::initializer_list<juce::String> allowed={},int gestures=1,bool expectValues=true,
                std::initializer_list<juce::String> hostAutomation={}) const {
        require(!invalid.load(),"CAB emitted an invalid or nested host gesture");
        std::vector<int> indices;
        for(const auto& id:allowed)indices.push_back(parameterIndex(processor,id));
        std::vector<int> automated;
        for(const auto& id:hostAutomation)automated.push_back(parameterIndex(processor,id));
        for(int i=0;i<count;++i) {
            const auto& event=events[size_t(i)];
            require(event.open.load()==0,"CAB left a host automation gesture open");
            if(std::find(indices.begin(),indices.end(),i)!=indices.end()) {
                require(event.starts.load()==gestures && event.ends.load()==gestures,
                    "CAB drag must pair one begin/end gesture for each affected parameter");
                if(expectValues)require(event.values.load()>0,"CAB drag did not notify the host of its parameter value");
                else require(event.values.load()==0,"Cancelled/hidden CAB drag still sent a parameter value to the host");
            } else if(std::find(automated.begin(),automated.end(),i)!=automated.end()) {
                require(event.values.load()>0 && event.starts.load()==0 && event.ends.load()==0,
                    "Host-driven source change acquired a spurious UI automation gesture");
            } else if(event.values.load()!=0 || event.starts.load()!=0 || event.ends.load()!=0)
                throw std::runtime_error("CAB view/drag notified unrelated host parameter "+std::to_string(i));
        }
    }
};

template<class Action> void navigationUnchanged(ChimeraProcessor& processor,Action action) {
    const auto values=parameterValues(processor);
    juce::MemoryBlock before,after;
    require(processor.tryGetStateInformation(before),"Cannot save CAB navigation baseline");
    const int comparison=processor.comparisonSlot();
    HostEvents events(processor);
    action();
    require(processor.tryGetStateInformation(after),"Cannot save CAB navigation result");
    require(before==after,"CAB navigation changed serialized audio parameters, comparison state or retained IR assets");
    require(processor.comparisonSlot()==comparison,"CAB navigation selected another comparison state");
    unchangedExcept(processor,values,{});events.expect();
}

inline void automate(ChimeraProcessor& processor,const juce::String& id,float value) {
    auto* parameter=processor.parameters().getParameter(id);
    require(parameter!=nullptr,"Missing automated CAB parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

inline float raw(ChimeraProcessor& processor,const juce::String& id) {
    return processor.parameters().getRawParameterValue(id)->load();
}

// A native peer makes isShowing() and keyboard focus exercise the same paths
// as a real popup. Linux CI already runs this existing CAB suite under Xvfb.
struct Showing {
    juce::Component& component;
    explicit Showing(juce::Component& value):component(value) {
        component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        component.setVisible(true);
        require(component.isShowing(),"CAB interaction fixture has no visible native peer");
    }
    ~Showing(){component.setVisible(false);component.removeFromDesktop();}
};

inline void click(juce::Button& button,const std::function<bool()>& ready) {
    require(button.isVisible() && button.isEnabled(),"CAB navigation button is unavailable");
    button.triggerClick();
    require(dispatchUntil(ready),"CAB navigation click did not reach its view");
}

inline void tabs(CabPanel& panel,ChimeraProcessor& processor) {
    require(panel.getView()==CabPanel::View::cabinet,"CAB must open on the central cabinet scene");
    auto& scene=component<CabScene>(panel,"cabScene1");
    auto& cabinet=component<juce::TextButton>(panel,"cabViewCabinet");
    auto& irLoader=component<juce::TextButton>(panel,"cabViewIRLoader");
    auto& captured=component<juce::ComboBox>(panel,"cabAmic1");
    navigationUnchanged(processor,[&] {
        click(irLoader,[&]{return panel.getView()==CabPanel::View::irLoader;});
        require(captured.isVisible() && !scene.isShowing(),"IR LOADER did not reveal capture cards and hide the scene");
        require(!scene.beginMicDrag(0,scene.microphoneAnchor(0)),"Hidden cabinet scene accepted a microphone drag");
        click(cabinet,[&]{return panel.getView()==CabPanel::View::cabinet;});
        require(!captured.isVisible() && scene.isShowing(),"CABINET did not restore the scene independently of captures");
        panel.setView(CabPanel::View::irLoader);
        panel.setView(CabPanel::View::cabinet);
    });
}

inline void physicalGeometry(CabScene& scene) {
    const auto cabinet=scene.cabinetBounds();
    require(!cabinet.isEmpty() && scene.getLocalBounds().toFloat().contains(cabinet),
        "Central cabinet artwork is outside its scene");
    const auto upperLeft=scene.speakerCentre(0),upperRight=scene.speakerCentre(1);
    const auto lowerLeft=scene.speakerCentre(2),lowerRight=scene.speakerCentre(3);
    require(upperLeft.x<upperRight.x && lowerLeft.x<lowerRight.x
        && upperLeft.y<lowerLeft.y && upperRight.y<lowerRight.y,
        "Physical unit IDs must remain upper left/right, then lower left/right");
    // Product cutouts have measured subpixel offsets rather than an invented
    // perfectly rectangular grille. They must still align within one pixel.
    require(std::abs(upperLeft.y-upperRight.y)<1.f && std::abs(lowerLeft.y-lowerRight.y)<1.f
        && std::abs(upperLeft.x-lowerLeft.x)<1.f && std::abs(upperRight.x-lowerRight.x)<1.f,
        "Four-unit cabinet geometry lost its two rows and two columns");
    const float radius=scene.speakerRadius();
    require(radius>20.f && radius*2.f<upperRight.x-upperLeft.x,"Speaker cone radius overlaps another physical unit");
    for(int unit=0;unit<4;++unit) {
        const auto centre=scene.speakerCentre(unit);
        require(cabinet.contains(juce::Rectangle<float>(radius*2.f,radius*2.f).withCentre(centre)),
            "A speaker cone no longer lies on the cabinet face");
        const auto id=unit==0 ? juce::String("ocab1_speakerImage") : "ocab1_speakerImage"+juce::String(unit+1);
        auto& image=component<spectralforge::cabArt::View>(scene,id.toRawUTF8());
        require(image.isVisible() && image.hasImage() && image.getBounds().getCentre().toFloat().getDistanceFrom(centre)<1.5f,
            "Speaker unit artwork does not correspond to its physical interaction centre");
    }
    for(int slot=0;slot<2;++slot) {
        const auto geometry=scene.micGeometry(slot);
        const auto expected=scene.speakerCentre(geometry.unit).translated(geometry.position*radius,0.f);
        require(scene.coneTarget(slot).getDistanceFrom(expected)<.01f,
            "Mic cone target does not reflect the stored physical unit and radial position");
        auto& node=component<juce::Component>(scene,slot ? "ocab1_Bimage" : "ocab1_Aimage");
        const auto capsule=scene.getLocalPoint(&node,juce::Point<float>(32.f,float(node.getHeight())*.16f));
        require(capsule.getDistanceFrom(scene.microphoneAnchor(slot))<1.5f,
            "Rotated microphone capsule is detached from its physical scene anchor");
    }
}

inline void mouseDrag(CabScene& scene,int slot,juce::Point<float> target,bool distanceOnly=false) {
    auto& node=component<juce::Component>(scene,slot ? "ocab1_Bimage" : "ocab1_Aimage");
    const auto start=scene.microphoneAnchor(slot).translated(7.f,12.f);
    require(scene.getComponentAt(start.roundToInt())==&node,"Visible microphone capsule cannot receive a pointer gesture");
    const auto source=juce::Desktop::getInstance().getMainMouseSource();
    const auto time=juce::Time::getCurrentTime();
    const juce::ModifierKeys modifiers(juce::ModifierKeys::leftButtonModifier
        | (distanceOnly ? juce::ModifierKeys::shiftModifier : 0));
    const auto event=[&](juce::Point<float> point,bool dragged) {
        return juce::MouseEvent(source,node.getLocalPoint(&scene,point),modifiers,1.f,0.f,0.f,0.f,0.f,
            &node,&node,juce::Time::getCurrentTime(),node.getLocalPoint(&scene,start),time,1,dragged);
    };
    node.mouseDown(event(start,false));
    require(scene.isDragging(),"Mic node mouseDown did not begin the real host gesture");
    node.mouseDrag(event(target,true));
    node.mouseUp(event(target,true));
    require(!scene.isDragging(),"Mic node mouseUp left a drag active");
}

inline void sceneInteractions(const juce::File& folder,const juce::File& screenshots) {
    using Asset=spectralforge::cabArt::Asset;
    using Image=spectralforge::cabArt::View;
    const auto capture=folder.getChildFile("Scene_Strike.wav");
    writeFixture(capture,"Chimera Strike","Synthetic scene cabinet",spectralforge::IRMetadata::Instrument::guitar,"12",9);
    auto processor=std::make_unique<ChimeraProcessor>();
    require(processor->loadMicIR(0,0,capture).wasOk() && processor->loadMicIR(0,1,capture).wasOk(),"Cannot load retained scene captures");
    automate(*processor,"ocab1_Aon",1);automate(*processor,"ocab1_Bon",1);
    automate(*processor,"ocab1_Amic",0);automate(*processor,"ocab1_Bmic",1);
    automate(*processor,"ocab1_Aunit",0);automate(*processor,"ocab1_Bunit",3);
    automate(*processor,"ocab1_Aposition",.25f);automate(*processor,"ocab1_Bposition",.1f);
    automate(*processor,"ocab1_Adistance",10);automate(*processor,"ocab1_Bdistance",23);
    CabPanel panel(*processor,0,{folder},folder.getChildFile("library.json"));
    Showing showing(panel);
    auto& scene=component<CabScene>(panel,"cabScene1");
    tabs(panel,*processor);physicalGeometry(scene);
    snapshot(panel,screenshots,"cab-visual-scene-guitar.png");
    HostEvents events(*processor);
    // The four destinations are an independent physical quadrant contract,
    // including 0.001 radial resolution and preserving where the node was held.
    for(int unit=0;unit<4;++unit) {
        const auto before=parameterValues(*processor);events.reset();
        const auto projection=scene.microphoneAnchor(0)-scene.coneTarget(0);
        const auto target=scene.speakerCentre(unit).translated(scene.speakerRadius()*.637f,0.f)+projection+juce::Point<float>(7.f,12.f);
        mouseDrag(scene,0,target);
        require(raw(*processor,"ocab1_Aunit")==float(unit) && std::abs(raw(*processor,"ocab1_Aposition")-.637f)<.00051f,
            "Dragging Mic A did not reach the intended physical unit and quantized cone position");
        unchangedExcept(*processor,before,{"ocab1_Aunit","ocab1_Aposition"});
        events.expect({"ocab1_Aunit","ocab1_Aposition"});physicalGeometry(scene);
    }
    // A separate B event must not change A, the microphone model, source mode,
    // shared enclosure, filters, blend, or any of the other 4,824 parameters.
    automate(*processor,"ocab1_Bunit",1);
    require(dispatchUntil([&]{return scene.micGeometry(1).unit==1;}),"Cannot separate the two mic nodes before the independent B pointer test");
    {
        const auto before=parameterValues(*processor);events.reset();
        const auto projection=scene.microphoneAnchor(1)-scene.coneTarget(1);
        const auto target=scene.speakerCentre(2).translated(scene.speakerRadius()*.31f,0.f)+projection+juce::Point<float>(7.f,12.f);
        mouseDrag(scene,1,target);
        require(raw(*processor,"ocab1_Bunit")==2 && std::abs(raw(*processor,"ocab1_Bposition")-.31f)<.00051f,
            "Dragging Mic B did not use its own physical geometry");
        unchangedExcept(*processor,before,{"ocab1_Bunit","ocab1_Bposition"});
        events.expect({"ocab1_Bunit","ocab1_Bposition"});
    }
    for(const auto movement:std::array<std::pair<float,float>,2>{{{1200.f,60.f},{-1200.f,2.f}}}) {
        const auto before=parameterValues(*processor);events.reset();
        mouseDrag(scene,0,scene.microphoneAnchor(0).translated(7.f,12.f+movement.first),true);
        require(std::abs(raw(*processor,"ocab1_Adistance")-movement.second)<.051f,"Shift-drag escaped the released 2-60 cm distance range");
        unchangedExcept(*processor,before,{"ocab1_Adistance"});events.expect({"ocab1_Adistance"});
    }
    // Values outside a cone clamp to its centre/edge without selecting a
    // fictitious fifth unit or altering the actual mic distance.
    for(const auto edge:std::array<std::pair<float,float>,2>{{{-1000.f,0.f},{1000.f,1.f}}}) {
        const auto before=parameterValues(*processor);events.reset();
        const int unit=edge.first<0 ? 0 : 1;
        const auto projection=scene.microphoneAnchor(0)-scene.coneTarget(0);
        mouseDrag(scene,0,scene.speakerCentre(unit).translated(edge.first,0.f)+projection+juce::Point<float>(7.f,12.f));
        require(raw(*processor,"ocab1_Aunit")==float(unit) && raw(*processor,"ocab1_Aposition")==edge.second,
            "Off-cone drag did not clamp to the nearest physical speaker centre/edge");
        unchangedExcept(*processor,before,{"ocab1_Aunit","ocab1_Aposition"});events.expect({"ocab1_Aunit","ocab1_Aposition"});
    }
    // A tab can interrupt a drag before mouseUp. It must close that existing
    // gesture, then reject later hidden movement without any parameter write.
    events.reset();const auto beforeHide=parameterValues(*processor);
    require(scene.beginMicDrag(0,scene.microphoneAnchor(0)),"Cannot begin tab-interrupted microphone drag");
    panel.setView(CabPanel::View::irLoader);
    require(!scene.isDragging(),"Leaving CABINET left an automation gesture open");
    scene.dragMicTo({-10000.f,10000.f});scene.endMicDrag();
    require(!scene.beginMicDrag(0,{0.f,0.f}),"IR LOADER permits hidden modeled-microphone editing");
    unchangedExcept(*processor,beforeHide,{});events.expect({"ocab1_Aunit","ocab1_Aposition"},1,false);
    panel.setView(CabPanel::View::cabinet);

    const auto oldAnchor=scene.microphoneAnchor(0);
    automate(*processor,"ocab1_Amic",2);automate(*processor,"ocab1_Aunit",2);
    automate(*processor,"ocab1_Aposition",.214f);automate(*processor,"ocab1_Adistance",37.6f);
    require(dispatchUntil([&] {
        const auto value=scene.micGeometry(0);
        return value.model==2 && value.unit==2 && std::abs(value.position-.214f)<.00051f
            && std::abs(value.distanceCm-37.6f)<.051f;
    }),"Host automation did not reposition and replace the visible microphone node");
    require(scene.microphoneAnchor(0).getDistanceFrom(oldAnchor)>10.f
        && component<Image>(scene,"ocab1_Aimage").asset()==Asset::detailCondenser
        && component<Image>(scene,"ocab1_Bimage").asset()==Asset::bodyRibbon,
        "Host automation retained a stale node or changed the other microphone image");
    physicalGeometry(scene);
    automate(*processor,"ocab1_design",1);
    require(dispatchUntil([&]{return component<Image>(scene,"ocab1_cabinetImage").asset()==Asset::bassCabinet;}),
        "Host cabinet automation did not replace the central 4x10 artwork");
    physicalGeometry(scene);snapshot(panel,screenshots,"cab-visual-scene-bass.png");

    // Captured IRs retain their catalog cards and waveform but expose no
    // movable position that could be mistaken for new acoustic processing.
    events.reset();const auto sourceSwitch=parameterValues(*processor);
    require(scene.beginMicDrag(0,scene.microphoneAnchor(0)),"Cannot begin host-interrupted mic drag");
    // Automation from a host/audio thread queues the UI attachment update.
    // Do not pump the message queue until the intervening pointer event.
    std::thread host([&]{automate(*processor,"ocab1_Aon",0);});host.join();
    require(raw(*processor,"ocab1_Aon")==0 && scene.micGeometry(0).enabled,
        "Host source-race fixture did not retain a pending UI update");
    scene.dragMicTo({-10000.f,10000.f});
    require(!scene.isDragging(),"Host capture selection did not terminate the current model drag");
    unchangedExcept(*processor,sourceSwitch,{"ocab1_Aon"});
    events.expect({"ocab1_Aunit","ocab1_Aposition"},1,false,{"ocab1_Aon"});
    require(dispatchUntil([&]{return !scene.micGeometry(0).enabled;}),"Captured source left an active scene microphone");
    events.reset();const auto beforeCaptured=parameterValues(*processor);
    require(!component<Image>(scene,"ocab1_Aimage").isVisible() && component<Image>(scene,"ocab1_Bimage").isVisible(),
        "Mixed capture/model scene does not identify which microphone can move");
    require(!scene.beginMicDrag(0,scene.microphoneAnchor(0)),"Captured microphone accepted modeled geometry editing");
    scene.dragMicTo({0.f,0.f});scene.endMicDrag();
    unchangedExcept(*processor,beforeCaptured,{});events.expect();
    snapshot(panel,screenshots,"cab-visual-scene-mixed.png");
    tabs(panel,*processor);
    panel.setView(CabPanel::View::irLoader);
    require(component<Image>(panel,"cabAmicImage1").asset()==Asset::strike
        && component<juce::ComboBox>(panel,"cabAmic1").getText().contains("Chimera Strike"),
        "Spatial editing lost the retained independent capture identity");
    snapshot(panel,screenshots,"cab-visual-ir-loader.png");
    panel.setView(CabPanel::View::cabinet);
    for(const auto size:std::array<juce::Point<int>,2>{{{900,748},{1180,820}}}) {
        const auto values=parameterValues(*processor);
        panel.setSize(size.x,size.y);physicalGeometry(scene);
        unchangedExcept(*processor,values,{});
    }
    panel.setSize(1040,748);
    juce::MemoryBlock saved;require(processor->tryGetStateInformation(saved),"Cannot save microphone drag result");
    auto restored=std::make_unique<ChimeraProcessor>();restored->setStateInformation(saved.getData(),int(saved.getSize()));
    require(parameterValues(*restored)==parameterValues(*processor),"Mic scene drag values changed during full project recall");
    CabPanel reopened(*restored,0,{folder},folder.getChildFile("library.json"));
    require(reopened.getView()==CabPanel::View::cabinet,"Saved view navigation changed the default CAB opening behavior");
    auto& recalledScene=component<CabScene>(reopened,"cabScene1");
    physicalGeometry(recalledScene);
    for(int slot=0;slot<2;++slot) {
        const auto a=scene.micGeometry(slot),b=recalledScene.micGeometry(slot);
        require(a.enabled==b.enabled && a.model==b.model && a.unit==b.unit && a.position==b.position && a.distanceCm==b.distanceCm,
            "Reopened scene no longer reflects its exact saved mic source and geometry");
    }
    std::cout<<"PASS CAB scene: view-only tabs, four physical units, real pointer/Shift gestures, host automation, boundaries, captured/hidden rejection and full project recall\n";
}

inline void roomNavigation(const juce::File& screenshots) {
    auto processor=std::make_unique<ChimeraProcessor>();
    for(int lane=0;lane<3;++lane) {
        automate(*processor,spectralforge::originalCabID(lane,"Aon"),1);
        automate(*processor,spectralforge::originalCabID(lane,"Bon"),1);
        automate(*processor,spectralforge::originalCabID(lane,"Bunit"),3);
        automate(*processor,spectralforge::originalCabID(lane,"design"),lane==1 ? 1.f : 0.f);
    }
    CabWorkspace workspace(*processor);Showing showing(workspace);
    require(workspace.activeRigCount()==1 && !workspace.isRoomView() && workspace.focusedRig()==0,
        "Single-rig mode must open directly on its active cabinet");
    navigationUnchanged(*processor,[&] {
        workspace.showRoom();
        require(!workspace.isRoomView() && !workspace.focusRig(-1) && !workspace.focusRig(1),
            "Single-rig navigation exposed an inactive cabinet");
    });
    for(int mode=1;mode<=2;++mode) {
        automate(*processor,"mode",float(mode));workspace.refreshState();
        const int count=mode+1;
        require(workspace.isRoomView() && workspace.activeRigCount()==count,"Routing mode did not reveal its active cabinet room");
        auto& room=component<CabRoomOverview>(workspace,"cabRoomOverview");
        require(room.activeRigCount()==count,"Room overview disagrees with the production active rig count");
        for(int lane=0;lane<3;++lane) {
            auto& card=component<juce::Button>(room,("cabRoomRig"+juce::String(lane+1)).toRawUTF8());
            require(card.isVisible()==(lane<count),"Room exposes an inactive rig or hides an active one");
            if(lane<count) {
                const auto bounds=room.getRigBounds(lane);
                require(!bounds.isEmpty() && room.getLocalBounds().contains(bounds),"Active room cabinet has no usable click target");
                require(int(card.getProperties()["cabRoomAmpModel"])==processor->selectedAmpModel(lane)
                    && int(card.getProperties()["cabRoomDesign"])==(lane==1 ? 1 : 0),
                    "Room rig artwork does not represent its current amplifier/cabinet selection");
                for(int earlier=0;earlier<lane;++earlier)
                    require(!bounds.intersects(room.getRigBounds(earlier)),"Active room cabinet click targets overlap");
            }
        }
        snapshot(workspace,screenshots,mode==1 ? "cab-visual-room-dual.png" : "cab-visual-room-matrix.png");
        navigationUnchanged(*processor,[&] {
            require(!workspace.focusRig(-1) && !workspace.focusRig(count) && workspace.isRoomView(),
                "Invalid focus request changed the active room view");
            require(!room.selectRig(-1) && !room.selectRig(count),"Room selection accepts an inactive rig index");
            for(int lane=0;lane<count;++lane) {
                auto& card=component<juce::Button>(room,("cabRoomRig"+juce::String(lane+1)).toRawUTF8());
                click(card,[&]{return !workspace.isRoomView() && workspace.focusedRig()==lane;});
                auto& panel=component<CabPanel>(workspace,"cabFocusedPanel");
                require(findComponent(panel,"cabScene"+juce::String(lane+1))!=nullptr,"Focused cabinet controls bind a different rig");
                panel.setView(CabPanel::View::irLoader);panel.setView(CabPanel::View::cabinet);
                if(mode==1 && lane==1)snapshot(workspace,screenshots,"cab-visual-room-focused-rig2.png");
                auto& back=component<juce::Button>(workspace,"cabBackToRoom");
                click(back,[&]{return workspace.isRoomView();});
            }
        });
        if(mode==1) {
            require(workspace.focusRig(0),"Cannot focus the room drag-lifecycle fixture");
            auto& panel=component<CabPanel>(workspace,"cabFocusedPanel");
            auto& scene=component<CabScene>(panel,"cabScene1");
            const auto values=parameterValues(*processor);HostEvents events(*processor);
            require(scene.beginMicDrag(0,scene.microphoneAnchor(0)),"Cannot begin the room-interrupted microphone drag");
            workspace.showRoom();
            require(!scene.isDragging(),"Returning to the room left an automation gesture open");
            scene.dragMicTo({-10000.f,10000.f});scene.endMicDrag();
            unchangedExcept(*processor,values,{});events.expect({"ocab1_Aunit","ocab1_Aposition"},1,false);
        }
    }
    require(workspace.focusRig(2),"Cannot focus the third Matrix cabinet");
    automate(*processor,"mode",1);workspace.refreshState();
    require(workspace.activeRigCount()==2 && workspace.isRoomView() && !workspace.focusRig(2),
        "Shrinking Matrix to Dual retained an actionable third rig");
    automate(*processor,"mode",0);workspace.refreshState();
    require(workspace.activeRigCount()==1 && !workspace.isRoomView() && workspace.focusedRig()==0,
        "Returning to single-rig mode did not focus the only active cabinet");
    automate(*processor,"mode",2);
    navigationUnchanged(*processor,[&] {
        CabWorkspace requested(*processor,1,true);
        require(!requested.isRoomView() && requested.focusedRig()==1,"Explicit active-rig detail request did not focus that cabinet");
        require(!requested.focusRig(3),"Explicit detail workspace accepted an out-of-range rig");
    });
    std::cout<<"PASS CAB room: production 1/2/3-rig mapping, real cabinet/back clicks, explicit focus, mode transitions and zero audio/state writes from navigation\n";
}

inline juce::TextButton& pageButton(juce::Component& canvas,const char* title) {
    for(auto* child:canvas.getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))
        if(button->getButtonText()==title)return *button;
    throw std::runtime_error("Missing editor page tab: "+std::string(title));
}

inline juce::DialogWindow* cabinetDialog() {
    for(int i=0;i<juce::TopLevelWindow::getNumTopLevelWindows();++i)
        if(auto* window=dynamic_cast<juce::DialogWindow*>(juce::TopLevelWindow::getTopLevelWindow(i)))
            if(dynamic_cast<CabWorkspace*>(window->getContentComponent()))return window;
    return nullptr;
}

inline void editorNavigation(const juce::File& screenshots) {
    auto processor=std::make_unique<ChimeraProcessor>();
    automate(*processor,"mode",1);
    for(int lane=0;lane<3;++lane) {
        processor->setAmpModel(lane,std::array<int,3>{{15,18,22}}[size_t(lane)]);
        automate(*processor,spectralforge::originalCabID(lane,"Aon"),1);
        automate(*processor,spectralforge::originalCabID(lane,"Bon"),1);
        automate(*processor,spectralforge::originalCabID(lane,"Bunit"),3);
        automate(*processor,spectralforge::originalCabID(lane,"design"),lane==1 ? 1.f : 0.f);
    }
    auto editor=std::make_unique<ChimeraEditor>(*processor);
    auto& canvas=component<juce::Component>(*editor,"surface");
    require(findComponent(canvas,"cabRoomOverview")==nullptr
        && !juce::SharedResourcePointer<spectralforge::cabArt::Bank>::getSharedObjectWithoutCreating(),
        "Opening the PRE page eagerly constructs the hidden cabinet room and equipment bank");
    Showing showing(*editor);
    for(int mode=1;mode<=2;++mode) {
        automate(*processor,"mode",float(mode));
        navigationUnchanged(*processor,[&] {
            auto& rigs=pageButton(canvas,"RIGS");
            click(rigs,[&] {
                auto* room=findComponent(canvas,"cabRoomOverview");
                return room && room->isVisible() && rigs.getToggleState();
            });
            auto& room=component<CabRoomOverview>(canvas,"cabRoomOverview");
            auto& toggle=component<juce::TextButton>(canvas,"cabRigControls");
            require(room.activeRigCount()==mode+1 && toggle.isVisible() && toggle.getButtonText()=="RIG CONTROLS",
                "Main RIGS page does not default to its active Dual/Matrix room");
            for(int lane=0;lane<3;++lane)
                require(!component<AmpNativePanel>(canvas,("ampNativePanel"+juce::String(lane+1)).toRawUTF8()).isVisible(),
                    "Main cabinet room is covered by amplifier knob panels");
            require(editor->getLocalBounds().contains(editor->getLocalArea(&room,room.getLocalBounds())),
                "Main cabinet room extends outside the editor");
            snapshot(*editor,screenshots,mode==1 ? "cab-visual-editor-dual.png" : "cab-visual-editor-matrix.png");
            auto& card=component<juce::Button>(room,("cabRoomRig"+juce::String(mode+1)).toRawUTF8());
            click(card,[]{return cabinetDialog()!=nullptr;});
            juce::Component::SafePointer<juce::DialogWindow> window(cabinetDialog());
            auto* workspace=dynamic_cast<CabWorkspace*>(window->getContentComponent());
            require(workspace && !workspace->isRoomView() && workspace->focusedRig()==mode,
                "Main room cabinet click opened another rig or stopped at the room overview");
            auto& focused=component<CabPanel>(*workspace,"cabFocusedPanel");
            focused.setView(CabPanel::View::irLoader);focused.setView(CabPanel::View::cabinet);
            if(mode==2)snapshot(*workspace,screenshots,"cab-visual-editor-focused-rig3.png");
            auto& back=component<juce::Button>(*workspace,"cabBackToRoom");
            click(back,[&]{return workspace->isRoomView();});
            window->exitModalState(0);
            require(dispatchUntil([&]{return window==nullptr;}),"Closing CAB left a live workspace or parameter attachments");
            click(toggle,[&]{return component<AmpNativePanel>(canvas,"ampNativePanel1").isVisible();});
            require(!room.isVisible() && toggle.getButtonText()=="CABINET ROOM","RIG CONTROLS did not replace the room");
            for(int lane=0;lane<3;++lane)
                require(component<AmpNativePanel>(canvas,("ampNativePanel"+juce::String(lane+1)).toRawUTF8()).isVisible()==(lane<=mode),
                    "RIG CONTROLS lost an active amplifier panel or exposed an inactive lane");
            click(toggle,[&]{return room.isVisible();});
            editor->setSize(885,585);
            require(editor->getLocalBounds().contains(editor->getLocalArea(&room,room.getLocalBounds())),
                "Cabinet room is clipped at the supported 75 percent editor size");
            if(mode==2)snapshot(*editor,screenshots,"cab-visual-editor-matrix-75pct.png");
            editor->setSize(1180,780);
            auto& pre=pageButton(canvas,"PRE");
            click(pre,[&]{return pre.getToggleState() && !room.isVisible();});
        });
    }
    std::cout<<"PASS main editor CAB: lazy PRE startup, Dual/Matrix room defaults, cabinet-to-focused-dialog/back, rig controls toggle, 75 percent layout and no host/audio-state writes\n";
}

inline void run(const juce::File& folder,const juce::File& screenshots) {
    sceneInteractions(folder,screenshots);roomNavigation(screenshots);editorNavigation(screenshots);
}
}
