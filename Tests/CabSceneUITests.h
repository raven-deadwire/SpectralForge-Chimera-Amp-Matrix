#pragma once
#include "CabMicrophoneUITests.h"
#include "CabScene.h"
#include "CabRoom.h"
#include "CabLowBlendAudioTests.h"
#include "PluginEditor.h"
#include <atomic>
#include <iomanip>
#include <sstream>
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

// Native interactions require an initialized display and an actual peer.
// Fail before JUCE popup centering can dereference an empty display list.
struct Showing {
    juce::Component& component;
    explicit Showing(juce::Component& value):component(value) {
        const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        require(display!=nullptr && !display->userArea.isEmpty(),
            "CAB native interaction tests require a primary display; on Linux run with Xvfb and a window manager");
        component.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        const auto* peer=component.getPeer();
        require(peer!=nullptr && peer->getNativeHandle()!=nullptr,
            "CAB interaction fixture failed to create a native window handle");
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
    spectralforge::cabHead::View* head=nullptr;
    for(auto* child:scene.getChildren())if(auto* candidate=dynamic_cast<spectralforge::cabHead::View*>(child)) {
        require(head==nullptr,"Cabinet scene contains multiple amplifier head views");head=candidate;
    }
    require(head && head->isVisible(),"Cabinet scene is missing its perspective amplifier head");
    const auto headBounds=scene.getLocalArea(head,head->getLocalBounds().toFloat());
    const auto expectedHead=scene.amplifierBounds();
    require(scene.getLocalBounds().toFloat().expanded(1.f).contains(headBounds)
        && headBounds.getTopLeft().getDistanceFrom(expectedHead.getTopLeft())<1.5f
        && headBounds.getBottomRight().getDistanceFrom(expectedHead.getBottomRight())<1.5f,
        "Scene amplifier head is not placed on the cabinet's shared support plane");
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
        auto& node=component<spectralforge::cabArt::View>(scene,slot ? "ocab1_Bimage" : "ocab1_Aimage");
        if(!geometry.enabled) {
            require(!node.isVisible(),"Captured microphone retains a visible modeled body in the cabinet scene");
            continue;
        }
        const auto visual=scene.micVisualGeometry(slot);
        const auto artwork=scene.getLocalArea(&node,node.artworkBounds());
        require(std::isfinite(visual.scale) && visual.scale>0.f && !visual.bodyBounds.isEmpty()
            && visual.bodyBounds.getTopLeft().getDistanceFrom(artwork.getTopLeft())<2.f
            && visual.bodyBounds.getBottomRight().getDistanceFrom(artwork.getBottomRight())<2.f,
            "Rendered microphone sprite is detached from its physical body geometry");
        require(visual.capsule.getDistanceFrom(scene.microphoneAnchor(slot))<1.5f
            && visual.bodyBounds.expanded(2.f).contains(visual.capsule)
            && visual.bodyBounds.expanded(2.f).contains(visual.mount)
            && visual.bodyBounds.expanded(2.f).contains(visual.cable),
            "Microphone capsule, mount or cable exit is detached from its actual body");
    }
}

inline void headSupportGeometry() {
    const juce::SharedResourcePointer<spectralforge::cabHead::Bank> artwork;
    for(const auto& face:artwork->faces)
        require(face.front.isValid() && face.material.isValid() && std::isfinite(face.aspect) && face.aspect>0.f,
            "A selected amplifier head lost its real fascia or casing material during perspective extraction");
    int cases=0,overhangs=0;
    for(const float pixelsPerMetre:std::array<float,3>{{250.f,500.f,800.f}}) {
        // At a fixed camera scale the same amplifier must keep its size on both
        // a narrow 1x10 and a wide array; genuine overhang is allowed.
        const juce::Rectangle<float> cabinet{50.f,400.f,.38f*pixelsPerMetre,.50f*pixelsPerMetre};
        const auto wideCabinet=cabinet.withWidth(1.20f*pixelsPerMetre);
        const float supportY=cabinet.getY()+.025f*pixelsPerMetre;
        for(int model=0;model<spectralforge::ampModelCount;++model) {
            const auto physical=spectralforge::cabPhysical::head(model);
            const auto bounds=spectralforge::cabHead::physicalBoundsAboveCabinet(cabinet,pixelsPerMetre,model,supportY);
            const auto wideBounds=spectralforge::cabHead::physicalBoundsAboveCabinet(wideCabinet,pixelsPerMetre,model,supportY);
            require(std::abs(bounds.getWidth()-physical.width*pixelsPerMetre)<.01f
                && std::abs(bounds.getHeight()-(physical.height+physical.depth*.16f)*pixelsPerMetre)<.01f
                && std::abs(bounds.getCentreX()-cabinet.getCentreX())<.01f
                && bounds.getWidth()==wideBounds.getWidth()
                && bounds.getHeight()==wideBounds.getHeight(),
                "Amplifier dimensions follow the cabinet width or bitmap padding instead of physical scale");
            if(physical.width>.38f) {
                require(bounds.getX()<cabinet.getX() && bounds.getRight()>cabinet.getRight(),
                    "A full-size head was shrunk to prevent its physical overhang on a narrow cabinet");
                ++overhangs;
            }
            const auto geometry=spectralforge::cabHead::geometry(bounds,model);
            require(!geometry.front.isEmpty() && bounds.expanded(.01f).contains(geometry.front)
                && std::abs(geometry.front.getWidth()-physical.width*pixelsPerMetre)<.01f
                && std::isfinite(geometry.supportY) && std::abs(geometry.supportY-supportY)<.01f,
                "Amplifier front or foot plane does not preserve its physical envelope");
            const float footRise=geometry.supportY-geometry.front.getBottom();
            require(std::abs(geometry.front.getHeight()+footRise+geometry.handle.getHeight()
                    -physical.height*pixelsPerMetre)<.01f,
                "Amplifier body, feet and handle exceed or underfill its published height budget");
            for(const auto& point:geometry.roof)
                require(std::isfinite(point.x) && std::isfinite(point.y) && bounds.expanded(.01f).contains(point),
                    "Perspective amplifier roof escapes its finite head bounds");
            const auto& backLeft=geometry.roof[0];const auto& backRight=geometry.roof[1];
            const auto& frontRight=geometry.roof[2];const auto& frontLeft=geometry.roof[3];
            require(backLeft.x>frontLeft.x && backRight.x<frontRight.x && backRight.x>backLeft.x
                && backLeft.y<frontLeft.y && backRight.y<frontRight.y
                && std::abs(frontLeft.y-backLeft.y-physical.depth*.16f*pixelsPerMetre)<.01f,
                "Amplifier roof does not project its own physical depth at the common camera angle");
            for(const auto& foot:geometry.feet)
                require(!foot.isEmpty() && bounds.expanded(.01f).contains(foot)
                    && std::abs(foot.getBottom()-geometry.supportY)<.01f,
                    "Amplifier foot floats above or penetrates its common support plane");
            require(!geometry.feet[0].intersects(geometry.feet[1]),"Amplifier feet overlap");
            if(spectralforge::cabHead::hasTopHandle(model))
                require(!geometry.handle.isEmpty() && bounds.expanded(.01f).contains(geometry.handle),
                    "Amplifier carrying handle escapes its physical height envelope");
            else require(geometry.handle.isEmpty(),"A compact rack/desktop amplifier acquired an invented roof handle");
            ++cases;
        }
        const auto compact=spectralforge::cabHead::physicalBoundsAboveCabinet(cabinet,pixelsPerMetre,
            int(spectralforge::AmpModel::tastePunch),supportY);
        const auto tube=spectralforge::cabHead::physicalBoundsAboveCabinet(cabinet,pixelsPerMetre,
            int(spectralforge::AmpModel::ironTube),supportY);
        require(compact.getWidth()<tube.getWidth()*.5f && compact.getHeight()<tube.getHeight()*.3f,
            "Low-profile desktop bass head was enlarged to the tube-head display envelope");
    }
    require(overhangs>0,"Head physical-scale audit did not exercise cabinet overhang");
    std::cout<<"PASS amplifier physical proportions: "<<cases<<" model/camera cases, independent W/H/D, compact formats, "
        <<overhangs<<" genuine overhangs and shared foot support plane\n";
}

inline void mouseDrag(CabScene& scene,int slot,juce::Point<float> target,bool distanceOnly=false,
                      juce::Component* inputSurface=nullptr,bool bodyGesture=false) {
    auto& node=component<juce::Component>(scene,bodyGesture ? (slot ? "ocab1_Bimage" : "ocab1_Aimage")
        : (slot ? "ocab1_BmicHandle" : "ocab1_AmicHandle"));
    auto start=scene.microphoneHitPoint(slot);
    if(bodyGesture) {
        bool found=false;float closest=std::numeric_limits<float>::max();
        const auto capsule=scene.microphoneAnchor(slot);
        // Hit actual opaque artwork; transparent padding must not create a
        // broad invisible rectangle covering another physical microphone.
        for(int y=0;y<node.getHeight();++y)for(int x=0;x<node.getWidth();++x) {
            const auto candidate=scene.getLocalPoint(&node,juce::Point<float>(float(x),float(y)));
            if(node.hitTest(x,y) && scene.getComponentAt(candidate.roundToInt())==&node
                && candidate.getDistanceSquaredFrom(capsule)<closest) {
                start=candidate;closest=candidate.getDistanceSquaredFrom(capsule);found=true;
            }
        }
        require(found,"Visible microphone artwork has no directly draggable opaque body");
    }
    // The caller specifies the desired capsule destination. Preserve the
    // actual grab offset of the physical body or independently pickable handle.
    target+=start-scene.microphoneAnchor(slot);
    auto& surface=inputSurface ? *inputSurface : static_cast<juce::Component&>(scene);
    require(surface.getComponentAt(surface.getLocalPoint(&scene,start).roundToInt())==&node,
        "Visible microphone handle cannot receive a pointer gesture through its workspace transform");
    const auto source=juce::Desktop::getInstance().getMainMouseSource();
    const auto time=juce::Time::getCurrentTime();
    const juce::ModifierKeys modifiers(juce::ModifierKeys::leftButtonModifier
        | (distanceOnly ? juce::ModifierKeys::shiftModifier : 0));
    const auto local=[&](juce::Point<float> point){return node.getLocalPoint(&surface,surface.getLocalPoint(&scene,point));};
    const auto event=[&](juce::Point<float> point,bool dragged) {
        return juce::MouseEvent(source,local(point),modifiers,1.f,0.f,0.f,0.f,0.f,
            &node,&node,juce::Time::getCurrentTime(),local(start),time,1,dragged);
    };
    node.mouseDown(event(start,false));
    require(scene.isDragging(),"Mic node mouseDown did not begin the real host gesture");
    node.mouseDrag(event(target,true));
    node.mouseUp(event(target,true));
    require(!scene.isDragging(),"Mic node mouseUp left a drag active");
}

inline void workspaceControlBounds(CabWorkspace& workspace,int lane) {
    auto& panel=component<CabPanel>(workspace,"cabFocusedPanel");
    require(!workspace.isRoomView() && panel.isShowing(),"CAB bounds fixture has no visible focused panel");
    const auto available=workspace.getLocalBounds().toFloat().withTrimmedTop(44.f);
    const auto displayed=workspace.getLocalArea(&panel,panel.getLocalBounds().toFloat());
    require(!displayed.isEmpty() && available.expanded(.51f).contains(displayed),
        "Focused CAB panel is clipped by the native workspace or its navigation header");
    require(std::abs(displayed.getWidth()/float(panel.getWidth())
        -displayed.getHeight()/float(panel.getHeight()))<.0001f,
        "Constrained native CAB popup distorts the focused panel aspect ratio");
    const auto check=[&](juce::Component& control) {
        const auto bounds=workspace.getLocalArea(&control,control.getLocalBounds().toFloat());
        if(bounds.isEmpty() || !available.expanded(.51f).contains(bounds)
            || !displayed.expanded(.51f).contains(bounds))
            throw std::runtime_error("Constrained CAB popup clips essential control: "+control.getComponentID().toStdString());
    };
    std::function<void(juce::Component&)> visit=[&](juce::Component& parent) {
        for(auto* child:parent.getChildren())if(child->isVisible()) {
            if(dynamic_cast<juce::Slider*>(child) || dynamic_cast<juce::ComboBox*>(child)
                || dynamic_cast<juce::Button*>(child) || dynamic_cast<juce::Label*>(child))check(*child);
            visit(*child);
        }
    };
    visit(panel);
    if(auto* low=findComponent(workspace,"cabFocusedLowBlend");low && low->isShowing()) {
        const auto header=workspace.getLocalBounds().toFloat().withHeight(44.f);
        const auto lowBounds=workspace.getLocalArea(low,low->getLocalBounds().toFloat());
        require(header.contains(lowBounds),"Constrained CAB popup clips the LOW blend header");
        const auto& back=component<juce::Button>(workspace,"cabBackToRoom");
        require(!back.isShowing() || !lowBounds.intersects(back.getBounds().toFloat()),
            "Constrained CAB popup overlaps LOW blend with the ROOM navigation button");
    }
    auto& blend=component<juce::Slider>(panel,("cabblend"+juce::String(lane+1)).toRawUTF8());
    check(blend);
    const auto blendBounds=workspace.getLocalArea(&blend,blend.getLocalBounds().toFloat());
    if(panel.getView()==CabPanel::View::cabinet)for(const auto* suffix:{"design","rear","tweeter"}) {
        auto* control=findComponent(panel,spectralforge::originalCabID(lane,suffix));
        require(control!=nullptr && control->isShowing(),"Constrained CAB popup lost an enclosure control");
        check(*control);
        require(!blendBounds.intersects(workspace.getLocalArea(control,control->getLocalBounds().toFloat())),
            "Constrained CAB popup overlaps enclosure controls with the mic blend");
    }
}

inline void constrainedWorkspace(const juce::File& screenshots) {
    auto processor=std::make_unique<ChimeraProcessor>();
    automate(*processor,"mode",2);automate(*processor,"ocab1_Aon",1);automate(*processor,"ocab1_Bon",1);
    automate(*processor,"ocab1_Bunit",3);
    CabWorkspace workspace(*processor,0,true);workspace.setSize(1000,657);Showing showing(workspace);
    auto& panel=component<CabPanel>(workspace,"cabFocusedPanel");
    auto& scene=component<CabScene>(panel,"cabScene1");
    workspaceControlBounds(workspace,0);
    {
        const auto values=parameterValues(*processor);HostEvents events(*processor);
        const auto projection=scene.microphoneAnchor(0)-scene.coneTarget(0);
        mouseDrag(scene,0,scene.speakerCentre(1).translated(scene.speakerRadius()*.583f,0.f)
            +projection,false,&workspace);
        require(raw(*processor,"ocab1_Aunit")==1 && std::abs(raw(*processor,"ocab1_Aposition")-.583f)<.00051f,
            "Scaled workspace pointer mapping changed the intended mic unit or cone position");
        unchangedExcept(*processor,values,{"ocab1_Aunit","ocab1_Aposition"});
        events.expect({"ocab1_Aunit","ocab1_Aposition"});
    }
    {
        const auto values=parameterValues(*processor);HostEvents events(*processor);
        mouseDrag(scene,0,scene.microphoneAnchor(0).translated(0.f,1200.f),true,&workspace);
        require(raw(*processor,"ocab1_Adistance")==60,"Scaled workspace Shift-drag did not preserve the distance boundary");
        unchangedExcept(*processor,values,{"ocab1_Adistance"});events.expect({"ocab1_Adistance"});
    }
    snapshot(workspace,screenshots,"cab-visual-workspace-constrained.png");
    navigationUnchanged(*processor,[&] {
        click(component<juce::Button>(panel,"cabViewIRLoader"),[&]{return panel.getView()==CabPanel::View::irLoader;});
        workspaceControlBounds(workspace,0);
        snapshot(workspace,screenshots,"cab-visual-workspace-constrained-ir-loader.png");
        click(component<juce::Button>(panel,"cabViewCabinet"),[&]{return panel.getView()==CabPanel::View::cabinet;});
        workspaceControlBounds(workspace,0);
        click(component<juce::Button>(workspace,"cabBackToRoom"),[&]{return workspace.isRoomView();});
        auto& room=component<CabRoomOverview>(workspace,"cabRoomOverview");
        click(component<juce::Button>(room,"cabRoomRig1"),[&]{return !workspace.isRoomView() && workspace.focusedRig()==0;});
        workspaceControlBounds(workspace,0);
    });
    std::cout<<"PASS constrained CAB workspace: 1000x657 control containment, uniform artwork, enclosure/blend separation, transformed mic/Shift gestures and view-only IR/back navigation\n";
}

inline void lowBlendControls(const juce::File& screenshots) {
    auto processor=std::make_unique<ChimeraProcessor>();
    automate(*processor,"mode",2);automate(*processor,"ampon1",1);automate(*processor,"cab1",1);
    for(int lane=0;lane<3;++lane) {
        processor->setAmpModel(lane,std::array<int,3>{{25,24,15}}[size_t(lane)]);
        automate(*processor,spectralforge::originalCabID(lane,"design"),lane==0 ? 1.f : 0.f);
        automate(*processor,spectralforge::originalCabID(lane,"Aon"),1);
        automate(*processor,spectralforge::originalCabID(lane,"Bon"),1);
        automate(*processor,spectralforge::originalCabID(lane,"Bunit"),3);
    }
    automate(*processor,"ocab1_Amic",0);automate(*processor,"ocab1_Bmic",1);
    CabWorkspace workspace(*processor);Showing showing(workspace);
    auto& room=component<CabRoomOverview>(workspace,"cabRoomOverview");
    auto& rig=component<juce::Button>(room,"cabRoomRig1");
    auto& roomSlider=component<juce::Slider>(room,"cabRoomLowAmpMix");
    auto& roomControl=component<juce::Component>(room,"cabRoomLowBlend");
    const auto contribution=[&](const char* id){return float(rig.getProperties()[id]);};
    const std::array<float,5> mixes{{0.f,.25f,.5f,.75f,1.f}};
    for(const float mix:mixes) {
        automate(*processor,"lowampmix",mix);
        require(dispatchUntil([&] {
            return std::abs(contribution("cabRoomLowAmpMix")-mix)<1e-6f
                && std::abs(float(roomSlider.getValue())-mix)<1e-6f;
        }),"Host LOW blend automation did not reach the cabinet room");
        require(roomSlider.isShowing() && roomSlider.isEnabled() && rig.isEnabled(),
            "Matrix LOW blend endpoint disabled the slider or cabinet selection");
        require(std::abs(contribution("cabRoomEffectiveAmpMix")-mix)<1e-6f
            && std::abs(contribution("cabRoomCabContribution")-mix)<1e-6f
            && std::abs(contribution("cabRoomDiContribution")-(1.f-mix))<1e-6f,
            "Matrix LOW room does not represent both sides of the DI / AMP + CAB blend");
        const int percent=juce::roundToInt(mix*100.f);
        const auto caption="DI "+juce::String(100-percent)+"% / AMP + CAB "+juce::String(percent)+"%";
        require(rig.getProperties()["cabRoomSource"].toString()==caption
            && component<juce::Label>(room,"cabRoomLowDIPercent").getText()=="DI "+juce::String(100-percent)+"%"
            && component<juce::Label>(room,"cabRoomLowAmpPercent").getText()=="AMP + CAB "+juce::String(percent)+"%",
            "Matrix LOW caption or endpoint percentages misrepresent the audible blend");
        const auto filename="cab-visual-room-matrix-low-"+juce::String(percent)+".png";
        snapshot(workspace,screenshots,filename.toRawUTF8());
        navigationUnchanged(*processor,[&] {
            click(rig,[&]{return !workspace.isRoomView() && workspace.focusedRig()==0;});
            auto& slider=component<juce::Slider>(workspace,"cabFocusedLowAmpMix");
            require(slider.isShowing() && slider.isEnabled() && std::abs(float(slider.getValue())-mix)<1e-6f,
                "Focused Matrix LOW did not preserve its existing host blend");
            auto& panel=component<CabPanel>(workspace,"cabFocusedPanel");
            auto& scene=component<CabScene>(panel,"cabScene1");
            require(component<juce::Component>(scene,"ocab1_Aimage").isShowing(),
                "Matrix LOW DI endpoint made the configured cabinet microphone uneditable");
            if(percent==50)snapshot(workspace,screenshots,"cab-visual-matrix-low-focused-50.png");
            click(component<juce::Button>(panel,"cabViewIRLoader"),[&]{return panel.getView()==CabPanel::View::irLoader;});
            require(slider.isShowing() && std::abs(float(slider.getValue())-mix)<1e-6f,
                "IR LOADER lost Matrix LOW's DI / AMP + CAB blend control");
            if(percent==50)snapshot(workspace,screenshots,"cab-visual-matrix-low-ir-loader-50.png");
            click(component<juce::Button>(workspace,"cabBackToRoom"),[&]{return workspace.isRoomView();});
        });
    }
    // Exercise the attachments through the visible UI controls. These writes
    // must target the existing LOW parameter, never cabblend1 or a new wet mix.
    const auto setHostBlend=[&](juce::Slider& slider,double requested,const char* failure) {
        auto* parameter=processor->parameters().getParameter("lowampmix");
        require(parameter!=nullptr,"Missing Matrix LOW host blend parameter");
        const auto& range=parameter->getNormalisableRange();
        const float legal=range.snapToLegalValue(float(requested));
        const float expected=parameter->convertFrom0to1(parameter->convertTo0to1(legal));
        {
            // setValue alone sends no drag lifecycle. Use JUCE's public
            // programmatic UI gesture so the real attachment must pair it.
            juce::Slider::ScopedDragNotification gesture(slider);
            slider.setValue(requested,juce::sendNotificationSync);
        }
        const float actual=raw(*processor,"lowampmix");
        std::ostringstream diagnostic;
        diagnostic<<std::setprecision(10)<<"LOW blend "<<slider.getComponentID().toStdString()
            <<" requested="<<requested<<" literal="<<float(requested)<<" interval="<<range.interval
            <<" legal="<<legal<<" expected="<<expected<<" actual="<<actual<<" slider="<<slider.getValue();
        std::cout<<diagnostic.str()<<'\n';
        // Compare the host's exact legal value, not a decimal float literal:
        // with the registered .01f interval, snap(.36f) differs by one ULP.
        if(actual!=expected || slider.getValue()!=double(legal))
            throw std::runtime_error(std::string(failure)+": "+diagnostic.str());
    };
    {
        const auto values=parameterValues(*processor);HostEvents events(*processor);
        setHostBlend(roomSlider,.36,"Room LOW slider is not bound to the existing host blend");
        unchangedExcept(*processor,values,{"lowampmix"});events.expect({"lowampmix"});
    }
    require(workspace.focusRig(0),"Cannot focus Matrix LOW blend interaction");
    {
        auto& slider=component<juce::Slider>(workspace,"cabFocusedLowAmpMix");
        const auto values=parameterValues(*processor);HostEvents events(*processor);
        setHostBlend(slider,.68,"Focused LOW slider is not bound to the room's host parameter");
        unchangedExcept(*processor,values,{"lowampmix"});events.expect({"lowampmix"});
    }
    workspace.showRoom();
    automate(*processor,"lowampmix",.5f);automate(*processor,"ampon1",0);
    require(dispatchUntil([&]{return contribution("cabRoomEffectiveAmpMix")==0.f;}),
        "AMP bypass does not update the Matrix LOW room's effective DI share");
    require(raw(*processor,"lowampmix")==.5f && contribution("cabRoomLowAmpMix")==.5f
        && contribution("cabRoomDiContribution")==1.f && contribution("cabRoomCabContribution")==0.f
        && rig.getProperties()["cabRoomSource"].toString()=="DI 100% / AMP OFF (50% SET)",
        "AMP bypass discarded the stored LOW blend or showed a false cabinet contribution");
    automate(*processor,"ampon1",1);automate(*processor,"cab1",0);
    require(dispatchUntil([&]{return contribution("cabRoomEffectiveAmpMix")==.5f
        && contribution("cabRoomCabContribution")==0.f;}),"CAB bypass retained a false cabinet contribution");
    require(raw(*processor,"lowampmix")==.5f && contribution("cabRoomDiContribution")==.5f
        && rig.getProperties()["cabRoomSource"].toString()=="DI 50% / AMP 50% / CAB OFF",
        "CAB bypass incorrectly removed the amplifier share from Matrix LOW");
    automate(*processor,"cab1",1);workspace.refreshState();
    require(workspace.focusRig(1),"Cannot focus Matrix MID for LOW blend visibility check");
    require(!component<juce::Slider>(workspace,"cabFocusedLowAmpMix").isShowing(),
        "Matrix MID exposes the unrelated LOW blend control");
    automate(*processor,"mode",1);workspace.refreshState();
    require(!roomControl.isShowing() && !component<juce::Slider>(workspace,"cabFocusedLowAmpMix").isShowing(),
        "Dual mode exposes the Matrix-only LOW blend");
    std::cout<<"PASS Matrix LOW cabinet UI: 0/25/50/75/100 contributions, retained bypass values, room/focused host bindings, IR Loader continuity and view-only navigation\n";
}

inline void micVisualBoundaries(const juce::File& screenshots) {
    auto processor=std::make_unique<ChimeraProcessor>();
    automate(*processor,"mode",2);automate(*processor,"ocab1_Aon",1);automate(*processor,"ocab1_Bon",1);
    CabWorkspace workspace(*processor,0,true);workspace.setSize(1000,657);Showing showing(workspace);
    auto& panel=component<CabPanel>(workspace,"cabFocusedPanel");
    auto& scene=component<CabScene>(panel,"cabScene1");
    const auto verify=[&] {
        physicalGeometry(scene);
        const auto available=scene.getLocalBounds().toFloat().expanded(1.f);
        std::array<juce::Rectangle<float>,2> handles;
        for(int slot=0;slot<2;++slot) {
            const auto visual=scene.micVisualGeometry(slot);
            require(available.contains(visual.bodyBounds),
                "A legal mic type/unit/position/distance clips the microphone body outside the scene");
            auto& handle=component<juce::Component>(scene,slot ? "ocab1_BmicHandle" : "ocab1_AmicHandle");
            handles[size_t(slot)]=scene.getLocalArea(&handle,handle.getLocalBounds().toFloat());
            require(handle.isShowing() && handle.getWidth()>=24 && handle.getHeight()>=24
                && available.contains(handles[size_t(slot)]),
                "A legal mic boundary loses the minimum independently pickable A/B handle");
            require(workspace.getComponentAt(workspace.getLocalPoint(&scene,scene.microphoneHitPoint(slot)).roundToInt())==&handle,
                "Coincident A/B microphone placement prevents selecting one microphone through the scaled workspace");
        }
        require(!handles[0].intersects(handles[1]),"Coincident microphone A/B handles overlap");
    };
    int cases=0;
    // Both mics deliberately share one target. Every legal endpoint must
    // retain its true capsule/body geometry and independent pointer access.
    for(int design=0;design<2;++design)for(int model=0;model<3;++model)for(int unit=0;unit<4;++unit)
        for(float position:{0.f,1.f})for(float distance:{2.f,60.f}) {
            automate(*processor,"ocab1_design",float(design));
            for(const auto* slot:{"A","B"}) {
                const auto prefix="ocab1_"+juce::String(slot);
                automate(*processor,prefix+"mic",float(model));automate(*processor,prefix+"unit",float(unit));
                automate(*processor,prefix+"position",position);automate(*processor,prefix+"distance",distance);
            }
            scene.refresh();verify();++cases;
        }
    for(const auto* slot:{"A","B"}) {
        const auto prefix="ocab1_"+juce::String(slot);
        automate(*processor,prefix+"unit",0);automate(*processor,prefix+"distance",10);
    }
    for(int slot:{0,1,0}) {
        automate(*processor,"ocab1_Aposition",.5f);automate(*processor,"ocab1_Bposition",.5f);scene.refresh();verify();
        const auto before=parameterValues(*processor);HostEvents events(*processor);
        const auto projection=scene.microphoneAnchor(slot)-scene.coneTarget(slot);
        mouseDrag(scene,slot,scene.speakerCentre(0).translated(scene.speakerRadius()*.57f,0.f)+projection,false,&workspace);
        const auto prefix="ocab1_"+juce::String(slot ? "B" : "A");
        require(std::abs(raw(*processor,prefix+"position")-.57f)<.00051f,
            "Overlapping microphone handle drag did not change the intended capsule position");
        unchangedExcept(*processor,before,{prefix+"unit",prefix+"position"});
        events.expect({prefix+"unit",prefix+"position"});verify();
    }
    snapshot(workspace,screenshots,"cab-visual-microphones-same-speaker.png");
    std::cout<<"PASS microphone visual geometry: "<<cases<<" paired endpoint configurations, body/capsule/mount/cable alignment, independent coincident handles and transformed host gestures\n";
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
    {
        const auto before=parameterValues(*processor);
        const auto projection=scene.microphoneAnchor(0)-scene.coneTarget(0);
        mouseDrag(scene,0,scene.speakerCentre(0).translated(scene.speakerRadius()*.36f,0.f)+projection,false,nullptr,true);
        require(std::abs(raw(*processor,"ocab1_Aposition")-.36f)<.00051f,
            "Direct opaque microphone body drag did not move its capsule to the requested cone position");
        unchangedExcept(*processor,before,{"ocab1_Aunit","ocab1_Aposition"});events.expect({"ocab1_Aunit","ocab1_Aposition"});
    }
    {
        events.reset();const auto before=parameterValues(*processor);
        auto& handle=component<juce::Component>(scene,"ocab1_AmicHandle");
        require(handle.keyPressed(juce::KeyPress(juce::KeyPress::rightKey)),"Mic handle rejects keyboard cone-position adjustment");
        require(std::abs(raw(*processor,"ocab1_Aposition")-.37f)<.00051f,"Mic keyboard step does not advance cone position by one percent");
        unchangedExcept(*processor,before,{"ocab1_Aposition"});events.expect({"ocab1_Aposition"});
    }
    // The four destinations are an independent physical quadrant contract,
    // including 0.001 radial resolution and preserving where the node was held.
    for(int unit=0;unit<4;++unit) {
        const auto before=parameterValues(*processor);events.reset();
        const auto projection=scene.microphoneAnchor(0)-scene.coneTarget(0);
        const auto target=scene.speakerCentre(unit).translated(scene.speakerRadius()*.637f,0.f)+projection;
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
        const auto target=scene.speakerCentre(2).translated(scene.speakerRadius()*.31f,0.f)+projection;
        mouseDrag(scene,1,target);
        require(raw(*processor,"ocab1_Bunit")==2 && std::abs(raw(*processor,"ocab1_Bposition")-.31f)<.00051f,
            "Dragging Mic B did not use its own physical geometry");
        unchangedExcept(*processor,before,{"ocab1_Bunit","ocab1_Bposition"});
        events.expect({"ocab1_Bunit","ocab1_Bposition"});
    }
    for(const auto movement:std::array<std::pair<float,float>,2>{{{1200.f,60.f},{-1200.f,2.f}}}) {
        const auto before=parameterValues(*processor);events.reset();
        mouseDrag(scene,0,scene.microphoneAnchor(0).translated(0.f,movement.first),true);
        require(std::abs(raw(*processor,"ocab1_Adistance")-movement.second)<.051f,"Shift-drag escaped the released 2-60 cm distance range");
        unchangedExcept(*processor,before,{"ocab1_Adistance"});events.expect({"ocab1_Adistance"});
    }
    // Values outside a cone clamp to its centre/edge without selecting a
    // fictitious fifth unit or altering the actual mic distance.
    for(const auto edge:std::array<std::pair<float,float>,2>{{{-1000.f,0.f},{1000.f,1.f}}}) {
        const auto before=parameterValues(*processor);events.reset();
        const int unit=edge.first<0 ? 0 : 1;
        const auto projection=scene.microphoneAnchor(0)-scene.coneTarget(0);
        mouseDrag(scene,0,scene.speakerCentre(unit).translated(edge.first,0.f)+projection);
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
    require(!component<juce::Component>(scene,"ocab1_AmicHandle").keyPressed(juce::KeyPress(juce::KeyPress::rightKey)),
        "IR LOADER permits hidden microphone keyboard editing");
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

struct NativePopupGeometry {
    bool showing{},displayAvailable{},clientContained{},contentContained{},frameAvailable{},frameContained{};
    juce::String description;
};

inline NativePopupGeometry nativePopupGeometry(juce::DialogWindow& window,CabWorkspace& workspace,
                                               juce::Component& editor) {
    NativePopupGeometry result;
    const auto client=window.getScreenBounds();
    const auto content=window.getLocalArea(&workspace,workspace.getLocalBounds());
    const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(client);
    result.showing=window.isShowing() && workspace.isShowing();
    result.displayAvailable=display!=nullptr;
    result.clientContained=display && display->userArea.contains(client);
    result.contentContained=window.getLocalBounds().contains(content);
    result.description="editor="+editor.getScreenBounds().toString()
        +" window-local="+window.getLocalBounds().toString()+" window-parent="+window.getBounds().toString()
        +" window-screen="+client.toString()+" workspace-parent="+workspace.getBounds().toString()
        +" workspace-in-window="+content.toString()+" showing="+juce::String(int(result.showing))
        +" desktop-scale="+juce::String(static_cast<juce::Component&>(window).getDesktopScaleFactor());
    if(display)result.description+=" display-user="+display->userArea.toString()
        +" display-total="+display->totalArea.toString()+" display-scale="+juce::String(display->scale);
    if(auto* peer=window.getPeer()) {
        result.description+=" peer="+peer->getBounds().toString()
            +" peer-scale="+juce::String(peer->getPlatformScaleFactor());
        if(const auto frame=peer->getFrameSizeIfPresent()) {
            result.frameAvailable=true;
            const auto outer=peer->localToGlobal(frame->addedTo(peer->getBounds().withZeroOrigin()).toFloat())
                /juce::Desktop::getInstance().getGlobalScaleFactor();
            result.frameContained=display && display->userArea.toFloat().contains(outer);
            result.description+=" frame="+juce::String(frame->getTop())+","+juce::String(frame->getLeft())
                +","+juce::String(frame->getBottom())+","+juce::String(frame->getRight())+" outer="+outer.toString();
        } else result.description+=" frame=PENDING";
    } else result.description+=" peer=MISSING";
    result.description+=" client-contained="+juce::String(int(result.clientContained))
        +" content-contained="+juce::String(int(result.contentContained))
        +" frame-contained="+juce::String(int(result.frameContained));
    return result;
}

inline void nativePopupReady(juce::DialogWindow& window,CabWorkspace& workspace,juce::Component& editor,
                             const juce::File& screenshots,int rig) {
    auto geometry=nativePopupGeometry(window,workspace,editor);
    std::cout<<"NATIVE CAB initial rig="<<rig<<": "<<geometry.description<<'\n';
    // Dialog creation can precede native title/frame placement. Observe the
    // real event queue until strict containment holds; never move the fixture,
    // remove an assertion, or use a fixed delay to hide persistent overflow.
    const bool ready=dispatchUntil([&] {
        const auto next=nativePopupGeometry(window,workspace,editor);
        if(next.description!=geometry.description)
            std::cout<<"NATIVE CAB geometry rig="<<rig<<": "<<next.description<<'\n';
        geometry=next;
        return geometry.showing && geometry.displayAvailable && geometry.clientContained
            && geometry.contentContained && geometry.frameAvailable && geometry.frameContained;
    });
    std::cout<<"NATIVE CAB final rig="<<rig<<": "<<geometry.description<<'\n';
    if(!ready) {
        const auto name="cab-visual-native-popup-failed-rig"+juce::String(rig)+".png";
        snapshot(window,screenshots,name.toRawUTF8());
    }
    require(geometry.showing && geometry.displayAvailable,"Native CAB popup never became visible on an available monitor");
    require(geometry.clientContained,"Native CAB popup client bounds extend beyond the available monitor area");
    require(geometry.contentContained,"Native CAB popup content extends beyond its window area");
    require(geometry.frameAvailable,"Native CAB popup never reported its actual native frame dimensions");
    require(geometry.frameContained,"Native CAB popup outer title/frame extends beyond the available monitor area");
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
            nativePopupReady(*window,*workspace,*editor,screenshots,mode+1);
            workspaceControlBounds(*workspace,mode);
            std::cout<<"NATIVE CAB popup: workspace="<<workspace->getWidth()<<"x"<<workspace->getHeight()<<" rig="<<mode+1<<"\n";
            auto& focused=component<CabPanel>(*workspace,"cabFocusedPanel");
            focused.setView(CabPanel::View::irLoader);workspaceControlBounds(*workspace,mode);
            if(mode==2)snapshot(*workspace,screenshots,"cab-visual-editor-ir-loader-rig3.png");
            focused.setView(CabPanel::View::cabinet);workspaceControlBounds(*workspace,mode);
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
    headSupportGeometry();
    cabLowBlendAudioTests::run();
    sceneInteractions(folder,screenshots);roomNavigation(screenshots);
    constrainedWorkspace(screenshots);lowBlendControls(screenshots);
    micVisualBoundaries(screenshots);editorNavigation(screenshots);
}
}
