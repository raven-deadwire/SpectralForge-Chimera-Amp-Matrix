#pragma once
#include "PluginProcessor.h"
#include "CabArtwork.h"
#include "CabHeadView.h"
#include "CabLayoutView.h"
#include <limits>

// A view of the existing original-cabinet geometry. Moving a microphone writes
// only the released unit / radial-position / distance parameters for that slot.
// Captured IRs have no inferred geometry and never acquire a draggable node.
class CabScene : public juce::Component {
public:
    struct MicGeometry {
        bool enabled{};
        int model{},unit{};
        float position{.25f},distanceCm{10.f};
        int expansion{};
    };
    struct MicVisualGeometry {
        juce::Rectangle<float> bodyBounds;
        juce::Point<float> capsule,mount,cable,badge;
        float scale{};
    };
private:
    ChimeraProcessor& processor;
    int lane;
    int design{-1},ampModel{-1},driverModel{-1},layoutModel{};
    juce::Rectangle<float> arrayBounds;
    float cameraScale{};
    std::array<MicGeometry,2> geometry;
    std::array<MicVisualGeometry,2> visualGeometry;
    std::array<uint64_t,2> displayedKeys{{~uint64_t{},~uint64_t{}}};
    spectralforge::cabArt::View cabinetImage;
    std::array<spectralforge::cabArt::View,8> speakerImages;
    spectralforge::cabHead::View headImage;
    class Foreground : public juce::Component {
        CabScene& owner;
    public:
        explicit Foreground(CabScene& value):owner(value) {setInterceptsMouseClicks(false,false);}
        void paint(juce::Graphics& g) override {owner.paintRigging(g);}
    } foreground{*this};
    class MicNode : public spectralforge::cabArt::View,public juce::SettableTooltipClient {
        CabScene& owner;
        int slot;
    public:
        MicNode(CabScene& value,int index):owner(value),slot(index) {
            setTrimArtwork(true);setInterceptsMouseClicks(true,false);setWantsKeyboardFocus(true);
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            setTooltip("Drag the microphone or its A/B handle across a speaker: centre to edge. Shift-drag down to move away, up to move closer. Left/Right changes position; Shift + Up/Down changes distance.");
        }
        bool hitTest(int x,int y) override {return isArtworkPoint({float(x),float(y)});}
        void mouseDown(const juce::MouseEvent& event) override {
            grabKeyboardFocus();
            owner.beginMicDrag(slot,event.getEventRelativeTo(&owner).position,event.mods.isShiftDown());
        }
        void mouseDrag(const juce::MouseEvent& event) override {owner.dragMicTo(event.getEventRelativeTo(&owner).position);}
        void mouseUp(const juce::MouseEvent&) override {owner.endMicDrag();}
        void mouseEnter(const juce::MouseEvent&) override {owner.setHoveredMic(slot);}
        void mouseExit(const juce::MouseEvent&) override {owner.setHoveredMic(-1);}
        void focusGained(FocusChangeType) override {owner.setHoveredMic(slot);}
        void focusLost(FocusChangeType) override {owner.setHoveredMic(-1);}
        bool keyPressed(const juce::KeyPress& key) override {return owner.nudgeMicrophone(slot,key);}
    } microphoneA{*this,0},microphoneB{*this,1};
    std::array<MicNode*,2> microphones{{&microphoneA,&microphoneB}};
    // Handles are distinct siblings above both sprites. Coincident capsules
    // remain physically coincident and each slot remains selectable.
    class MicHandle : public juce::Component,public juce::SettableTooltipClient {
        CabScene& owner;
        int slot;
    public:
        MicHandle(CabScene& value,int index):owner(value),slot(index) {
            setInterceptsMouseClicks(true,false);setWantsKeyboardFocus(true);
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            setTooltip("Drag Mic "+juce::String(slot ? "B" : "A")+" to choose speaker and centre-to-edge position. Shift-drag vertically changes distance only.");
            setTitle("Mic "+juce::String(slot ? "B" : "A")+" position handle");
        }
        void mouseDown(const juce::MouseEvent& event) override {
            grabKeyboardFocus();
            owner.beginMicDrag(slot,event.getEventRelativeTo(&owner).position,event.mods.isShiftDown());
        }
        void mouseDrag(const juce::MouseEvent& event) override {owner.dragMicTo(event.getEventRelativeTo(&owner).position);}
        void mouseUp(const juce::MouseEvent&) override {owner.endMicDrag();}
        void mouseEnter(const juce::MouseEvent&) override {owner.setHoveredMic(slot);}
        void mouseExit(const juce::MouseEvent&) override {owner.setHoveredMic(-1);}
        void focusGained(FocusChangeType) override {owner.setHoveredMic(slot);}
        void focusLost(FocusChangeType) override {owner.setHoveredMic(-1);}
        bool keyPressed(const juce::KeyPress& key) override {return owner.nudgeMicrophone(slot,key);}
        void paint(juce::Graphics& g) override {
            const auto area=getLocalBounds().toFloat().reduced(1.5f);
            const auto colour=juce::Colour(slot ? 0xffc6a16c : 0xff77aeba);
            const bool active=owner.draggingSlot==slot || owner.hoveredSlot==slot;
            g.setColour(juce::Colour(0xff0c1012).withAlpha(.96f));g.fillEllipse(area);
            g.setColour(colour.withAlpha(active ? 1.f : .78f));g.drawEllipse(area,active ? 1.8f : 1.f);
            g.setColour(colour);g.setFont(juce::FontOptions(11.f,juce::Font::bold));
            g.drawText(slot ? "B" : "A",area.toNearestInt(),juce::Justification::centred);
        }
    } handleA{*this,0},handleB{*this,1};
    std::array<MicHandle*,2> handles{{&handleA,&handleB}};
    int draggingSlot{-1},hoveredSlot{-1},gestureCount{};
    bool distanceDrag{};
    juce::Point<float> dragStart,dragOffset;
    float startDistance{};
    std::array<juce::RangedAudioParameter*,3> gestures{};

    float raw(const char* suffix) const {
        return processor.parameters().getRawParameterValue(spectralforge::originalCabID(lane,suffix))->load();
    }
    int expanded(const char* suffix) const {
        return int(processor.parameters().getRawParameterValue(spectralforge::cabExpansionID(lane,suffix))->load());
    }
    int layoutValue(const char* suffix) const {
        return int(processor.parameters().getRawParameterValue(spectralforge::cabLayoutID(lane,suffix))->load());
    }
    spectralforge::cabLayout::Settings currentSettings() const {
        spectralforge::originalCab::Settings base{};base.cabinet=int(raw("design"));
        return spectralforge::cabLayout::effectiveSettings({{base,expanded("driver"),0,0},layoutValue("layout"),0});
    }
    int effectiveDesign() const {return int(spectralforge::cabLayout::isBass(currentSettings()));}
    juce::RangedAudioParameter* parameter(int slot,const char* suffix) {
        const auto id=(juce::String(slot ? "B" : "A")+suffix);
        return processor.parameters().getParameter(layoutValue("layout")>0 && juce::String(suffix)=="unit"
            ? spectralforge::cabLayoutID(lane,id.toRawUTF8()) : spectralforge::originalCabID(lane,id.toRawUTF8()));
    }
    static void setParameter(juce::RangedAudioParameter* p,float value) {
        p->setValueNotifyingHost(p->convertTo0to1(p->getNormalisableRange().snapToLegalValue(value)));
    }
    void setHoveredMic(int slot) {
        if(hoveredSlot==slot)return;
        hoveredSlot=slot;foreground.repaint();
        for(auto* handle:handles)handle->repaint();
    }
    juce::Point<float> distanceProjection(int,float cm) const {
        const float depth=juce::jlimit(0.f,1.f,(cm-2.f)/58.f);
        // Both slots share the same camera/depth axis, parallel to the rigid
        // end-address barrel. Slot identity never changes the projection.
        return {depth*cameraScale*.058f,depth*cameraScale*.027f};
    }
    void updatePlacement() {
        if(getWidth()==0 || getHeight()==0)return;
        const auto projection=spectralforge::cabLayoutView::project(arraySettings(),getLocalBounds().toFloat());
        arrayBounds=projection.box;cameraScale=projection.pixelsPerMetre;
        const auto cab=cabinetBounds();
        // Keep the legacy artwork identity for accessibility/catalog consumers;
        // the modeled baffle and all cones are drawn independently, front-on.
        cabinetImage.setBounds(cab.toNearestInt());
        headImage.setBounds(amplifierBounds().toNearestInt());
        const float diameter=speakerOuterDiameter();
        for(int i=0;i<8;++i) {
            auto& image=speakerImages[size_t(i)];image.setVisible(i<speakerCount());
            if(i<speakerCount()) {
                const auto bounds=juce::Rectangle<float>(diameter,diameter).withCentre(speakerCentre(i));
                image.setBounds(bounds.getSmallestIntegerContainer());
                image.setArtworkArea(bounds.translated(-float(image.getX()),-float(image.getY())));
            }
        }
        foreground.setBounds(getLocalBounds());
        for(int i=0;i<2;++i) {
            auto& node=*microphones[size_t(i)];
            const auto& micGeometry=geometry[size_t(i)];
            auto presentation=micGeometry.expansion>0
                ? spectralforge::cabArt::expandedMicrophonePresentation(micGeometry.expansion-1)
                : spectralforge::cabArt::microphonePresentation(micGeometry.model);
            const auto dimensions=micGeometry.expansion>0
                ? spectralforge::cabPhysical::microphone(micGeometry.expansion-1)
                : spectralforge::cabPhysical::legacyMicrophone(micGeometry.model);
            node.setPhysicalDimensions(dimensions.width,dimensions.height);
            const float rotation=micGeometry.expansion>0 && !presentation.sideAddress ? juce::degreesToRadians(-65.f) : 0.f;
            node.setArtworkRotation(rotation);
            presentation.capsule=node.orientedAnchor(presentation.capsule);
            presentation.mount=node.orientedAnchor(presentation.mount);
            presentation.cable=node.orientedAnchor(presentation.cable);
            // The body retains its real envelope at every speaker and distance.
            const auto physical=juce::Rectangle<float>(dimensions.width,dimensions.height)
                .transformedBy(juce::AffineTransform::rotation(rotation));
            const float width=physical.getWidth()*cameraScale,height=physical.getHeight()*cameraScale;
            const auto anchor=microphoneAnchor(i);
            const juce::Rectangle<float> body{anchor.x-width*presentation.capsule.x,
                anchor.y-height*presentation.capsule.y,width,height};
            node.setTransform({});
            node.setBounds(body.expanded(3.f).getSmallestIntegerContainer());
            node.setArtworkArea(body.translated(-float(node.getX()),-float(node.getY())));
            auto& visual=visualGeometry[size_t(i)];
            visual.bodyBounds=node.artworkBounds().translated(float(node.getX()),float(node.getY()));
            const auto point=[&](juce::Point<float> normalised) {
                return juce::Point<float>{visual.bodyBounds.getX()+normalised.x*visual.bodyBounds.getWidth(),
                    visual.bodyBounds.getY()+normalised.y*visual.bodyBounds.getHeight()};
            };
            visual.capsule=point(presentation.capsule);visual.mount=point(presentation.mount);
            visual.cable=point(presentation.cable);visual.scale=std::max(width,height);
            visual.badge=anchor.translated(i ? 30.f : -30.f,-18.f);
            visual.badge.x=juce::jlimit(14.f,float(getWidth())-14.f,visual.badge.x);
            visual.badge.y=juce::jlimit(14.f,float(getHeight())-14.f,visual.badge.y);
        }
        // Two different mic positions can also bring their handles together.
        // Offset UI handles only; never offset a pickup point or audio value.
        auto& badgeA=visualGeometry[0].badge;auto& badgeB=visualGeometry[1].badge;
        if(std::abs(badgeA.x-badgeB.x)<28.f && std::abs(badgeA.y-badgeB.y)<28.f) {
            const float centre=juce::jlimit(30.f,float(getWidth())-30.f,(badgeA.x+badgeB.x)*.5f);
            badgeA.x=centre-15.f;badgeB.x=centre+15.f;
        }
        for(int i=0;i<2;++i) {
            auto& handle=*handles[size_t(i)];
            handle.setBounds(juce::Rectangle<float>(24.f,24.f).withCentre(visualGeometry[size_t(i)].badge).toNearestInt());
            visualGeometry[size_t(i)].badge=handle.getBounds().getCentre().toFloat();
            handle.repaint();
        }
        foreground.repaint();repaint();
    }
    bool nudgeMicrophone(int slot,const juce::KeyPress& key) {
        if(!isShowing() || !geometry[size_t(slot)].enabled || raw(slot ? "Bon" : "Aon")<=.5f)return false;
        const bool shift=key.getModifiers().isShiftDown();
        auto* p=parameter(slot,shift ? "distance" : "position");
        float delta=0;
        if(shift && key.getKeyCode()==juce::KeyPress::upKey)delta=-1.f;
        else if(shift && key.getKeyCode()==juce::KeyPress::downKey)delta=1.f;
        else if(!shift && key.getKeyCode()==juce::KeyPress::leftKey)delta=-.01f;
        else if(!shift && key.getKeyCode()==juce::KeyPress::rightKey)delta=.01f;
        else return false;
        const auto& current=geometry[size_t(slot)];
        p->beginChangeGesture();setParameter(p,(shift ? current.distanceCm : current.position)+delta);p->endChangeGesture();
        refresh();return true;
    }
    void paintRigging(juce::Graphics& g) {
        if(layoutModel>0) {
            g.setFont(juce::FontOptions(9.f));g.setColour(juce::Colour(0xff899492));
            for(int n=0;n<speakerCount();++n) {
                const auto c=speakerCentre(n);
                g.drawText(juce::String(n+1),juce::Rectangle<float>(c.x-speakerRadius()-10,c.y-6,10,12),juce::Justification::centred);
            }
        }
        if(raw("tweeter")>0) {
                const auto model=arrayGeometry();const auto face=baffleBounds();
                const auto horn=juce::Point<float>{face.getCentreX()+float(model.horn.x/model.box.width)*face.getWidth(),face.getCentreY()-float(model.horn.y/model.box.height)*face.getHeight()};
                const float r=face.getWidth()*.024f;g.setColour(juce::Colour(0xffbcc0b9));g.drawEllipse(horn.x-r,horn.y-r,r*2,r*2,1.5f);
        }
        for(int i=0;i<2;++i)if(geometry[size_t(i)].enabled) {
            const auto& visual=visualGeometry[size_t(i)];
            const auto anchor=visual.capsule,target=coneTarget(i);
            const auto colour=juce::Colour(i ? 0xffc6a16c : 0xff77aeba);
            const auto cab=cabinetBounds();
            const float scale=juce::jlimit(.65f,1.25f,cameraScale*.74f/410.f);
            const float floor=juce::jmin(float(getHeight())-12.f,cab.getBottom()+15.f*scale);
            const float stemX=i ? juce::jmin(float(getWidth())-28.f,cab.getRight()+30.f*scale)
                : juce::jmax(28.f,cab.getX()-30.f*scale);
            const float jointY=juce::jmin(floor-18.f,visual.mount.y+24.f*scale);
            const juce::Point<float> joint{stemX,jointY},base{stemX,floor};
            const auto rod=[&](juce::Point<float> a,juce::Point<float> b,float width) {
                g.setColour(juce::Colour(0xff080a0b));g.drawLine(juce::Line<float>(a,b),width*scale+1.2f);
                g.setColour(juce::Colour(0xff484b4d));
                g.drawLine(juce::Line<float>(a.translated(-.5f,-.5f),b.translated(-.5f,-.5f)),width*scale*.36f);
            };
            // The boom terminates at the sprite's actual mounting collar. The
            // cable starts at its XLR exit, not a guessed point below a capsule.
            rod(base,joint,4.f);rod(joint,visual.mount,3.5f);
            rod(base,base.translated(-22.f*scale,6.f*scale),3.f);
            rod(base,base.translated(22.f*scale,6.f*scale),3.f);
            rod(base,base.translated(6.f*scale,-10.f*scale),2.5f);
            g.setColour(juce::Colour(0xff15191b));g.fillEllipse(joint.x-5.f*scale,joint.y-5.f*scale,10.f*scale,10.f*scale);
            g.setColour(juce::Colour(0xff6f7476));g.drawEllipse(joint.x-4.f*scale,joint.y-4.f*scale,8.f*scale,8.f*scale,.75f);
            juce::Path cable;cable.startNewSubPath(visual.cable);
            cable.cubicTo(visual.cable.x+7.f*scale,visual.cable.y+19.f*scale,
                stemX,juce::jmin(floor,visual.cable.y+37.f*scale),stemX+2.f*scale,floor);
            cable.cubicTo(stemX+float(i ? 12 : -12)*scale,floor+5.f*scale,
                float(i ? getWidth()-18 : 18),floor+7.f*scale,float(i ? getWidth()-8 : 8),floor+4.f*scale);
            g.setColour(juce::Colours::black.withAlpha(.95f));g.strokePath(cable,juce::PathStrokeType(2.8f*scale));
            g.setColour(juce::Colour(0xff53585c).withAlpha(.65f));g.strokePath(cable,juce::PathStrokeType(.8f*scale));
            const bool active=draggingSlot==i || hoveredSlot==i;
            g.setColour(colour.withAlpha(active ? .85f : .42f));
            g.drawLine(juce::Line<float>(target,anchor),active ? 1.3f : .8f);
            g.drawEllipse(target.x-3.f,target.y-3.f,6.f,6.f,1.f);
            g.setColour(colour.withAlpha(active ? .70f : .26f));
            g.drawLine(juce::Line<float>(anchor,visual.badge),.8f);
            if(active) {
                const float radius=speakerRadius();const auto centre=speakerCentre(geometry[size_t(i)].unit);
                g.setColour(colour.withAlpha(.30f));g.drawEllipse(centre.x-radius,centre.y-radius,radius*2.f,radius*2.f,1.f);
                g.setColour(colour.withAlpha(.60f));g.drawLine(centre.x,centre.y,centre.x+radius,centre.y,1.2f);
                g.drawLine(centre.x,centre.y-4.f,centre.x,centre.y+4.f,1.f);
                g.drawLine(centre.x+radius,centre.y-4.f,centre.x+radius,centre.y+4.f,1.f);
            }
        }
    }
public:
    CabScene(ChimeraProcessor& value,int rig):processor(value),lane(rig) {
        setComponentID("cabScene"+juce::String(lane+1));
        setInterceptsMouseClicks(false,true);
        // The head's feet and contact shadow lie on the cabinet roof. Paint
        // the cabinet first so its opaque top cannot cover those supports.
        addAndMakeVisible(cabinetImage);addAndMakeVisible(headImage);
        cabinetImage.setTrimArtwork(true);cabinetImage.setComponentID("ocab"+juce::String(lane+1)+"_cabinetImage");
        for(int i=0;i<8;++i) {
            auto& speaker=speakerImages[size_t(i)];addAndMakeVisible(speaker);speaker.setTrimArtwork(true);
            speaker.setComponentID("ocab"+juce::String(lane+1)+(i==0 ? "_speakerImage" : "_speakerImage"+juce::String(i+1)));
        }
        addAndMakeVisible(foreground);
        for(int i=0;i<2;++i) {
            auto& mic=*microphones[size_t(i)];addAndMakeVisible(mic);
            mic.setComponentID("ocab"+juce::String(lane+1)+(i ? "_Bimage" : "_Aimage"));
        }
        for(int i=0;i<2;++i) {
            auto& handle=*handles[size_t(i)];addAndMakeVisible(handle);
            handle.setComponentID("ocab"+juce::String(lane+1)+(i ? "_BmicHandle" : "_AmicHandle"));
        }
        refresh();
    }
    ~CabScene() override {endMicDrag();}
    void refresh() {
        const int nextDesign=effectiveDesign();
        const int nextAmp=processor.selectedAmpModel(lane);
        const int nextDriver=currentSettings().voice.driver;
        const int nextLayout=layoutValue("layout");
        bool changed=design!=nextDesign || ampModel!=nextAmp || driverModel!=nextDriver || layoutModel!=nextLayout;
        if(draggingSlot>=0 && (design!=nextDesign || driverModel!=nextDriver || layoutModel!=nextLayout))endMicDrag();
        layoutModel=nextLayout;cabinetImage.setVisible(false);
        design=nextDesign;ampModel=nextAmp;driverModel=nextDriver;headImage.setModel(ampModel);
        const auto* selected=driverModel>0 ? &spectralforge::cabExpansion::drivers[size_t(driverModel-1)] : nullptr;
        cabinetImage.setAsset(spectralforge::cabArt::cabinet(design),selected
            ? juce::String("Chimera ")+juce::String(speakerCount())+"x"+juce::String(selected->inches)+" / "+selected->name
            : design ? "Chimera Bass 4x10 cabinet" : "Chimera Guitar 4x12 cabinet");
        for(auto& image:speakerImages) {
            image.setAsset(spectralforge::cabArt::speaker(design),selected
                ? juce::String(selected->name)+" / "+juce::String(selected->inches)+"-inch speaker unit"
                : design ? "Chimera 10-inch bass speaker unit" : "Chimera 12-inch guitar speaker unit");
            image.setSpeakerDesign(driverModel);
        }
        bool any=false;
        for(int i=0;i<2;++i) {
            const auto prefix=juce::String(i ? "B" : "A");
            const auto value=[&](const char* suffix){return raw((prefix+suffix).toRawUTF8());};
            auto& geo=geometry[size_t(i)];
            const int unit=layoutModel>0 ? juce::jlimit(0,speakerCount()-1,layoutValue((prefix+"unit").toRawUTF8())) : int(value("unit"));
            const MicGeometry next{value("on")>.5f,int(value("mic")),unit,value("position"),value("distance"),expanded((prefix+"mic").toRawUTF8())};
            if(draggingSlot==i && (!next.enabled || geo.model!=next.model || geo.expansion!=next.expansion))endMicDrag();
            geo=next;
            const spectralforge::originalCab::Settings settings{geo.enabled,design,int(raw("rear")),geo.model,geo.unit,raw("tweeter"),geo.position,geo.distanceCm};
            const auto key=spectralforge::cabLayout::key({{settings,expanded("driver"),geo.expansion,expanded("tweeter")},layoutModel,geo.unit});
            changed=changed || displayedKeys[size_t(i)]!=key;displayedKeys[size_t(i)]=key;
            auto& image=*microphones[size_t(i)];
            image.setAsset(geo.expansion>0 ? spectralforge::cabArt::capturedMicrophone(spectralforge::micCatalog::byId(
                spectralforge::cabExpansion::microphones[size_t(geo.expansion-1)].catalogId)) : spectralforge::cabArt::originalMicrophone(geo.model),geo.expansion>0
                ? spectralforge::cabExpansion::microphones[size_t(geo.expansion-1)].name : juce::String(geo.model==0 ? "Attack Dynamic" : geo.model==1 ? "Body Ribbon" : "Detail Condenser"));
            image.setActive(geo.enabled);image.setVisible(geo.enabled);any=any || geo.enabled;
            handles[size_t(i)]->setVisible(geo.enabled);
            if(!geo.enabled && hoveredSlot==i)setHoveredMic(-1);
        }
        if(draggingSlot>=0 && !geometry[size_t(draggingSlot)].enabled)endMicDrag();
        cabinetImage.setAlpha(any ? 1.f : .62f);headImage.setAlpha(any ? 1.f : .62f);
        for(auto& speaker:speakerImages)speaker.setAlpha(any ? 1.f : .62f);
        if(changed)updatePlacement();
    }
    int layoutSelection() const noexcept {return layoutModel;}
    int speakerCount() const {return spectralforge::cabLayout::count(layoutModel);}
    spectralforge::cabLayout::Settings arraySettings() const {return currentSettings();}
    spectralforge::cabLayout::Geometry arrayGeometry() const {return spectralforge::cabLayout::geometry(arraySettings());}
    juce::Rectangle<float> baffleBounds() const {return spectralforge::cabLayoutView::baffle(arrayGeometry(),arrayBounds);}
    juce::Rectangle<float> cabinetBounds() const {return arrayBounds;}
    juce::Rectangle<float> amplifierBounds() const {return spectralforge::cabHead::physicalBoundsAboveCabinet(arrayBounds,cameraScale,ampModel,
        arrayBounds.getY()+(baffleBounds().getY()-arrayBounds.getY())*.72f);}
    juce::Point<float> speakerCentre(int unit) const {
        const auto model=arrayGeometry();
        return spectralforge::cabLayoutView::point(model,arrayBounds,model.centres[size_t(juce::jlimit(0,model.count-1,unit))]);
    }
    float speakerRadius() const {return float(arrayGeometry().radius)*cameraScale;}
    float speakerOuterDiameter() const {return float(spectralforge::cabExpansion::diameter(arraySettings().voice))*.0254f*cameraScale;}
    float pixelsPerMetre() const noexcept {return cameraScale;}
    juce::Point<float> coneTarget(int slot) const {
        const auto& geo=geometry[size_t(juce::jlimit(0,1,slot))];
        return speakerCentre(geo.unit).translated(geo.position*speakerRadius(),0.f);
    }
    juce::Point<float> microphoneAnchor(int slot) const {
        const int index=juce::jlimit(0,1,slot);
        return coneTarget(index)+distanceProjection(index,geometry[size_t(index)].distanceCm);
    }
    MicGeometry micGeometry(int slot) const {return geometry[size_t(juce::jlimit(0,1,slot))];}
    MicVisualGeometry micVisualGeometry(int slot) const {return visualGeometry[size_t(juce::jlimit(0,1,slot))];}
    juce::Point<float> microphoneHitPoint(int slot) const {return micVisualGeometry(slot).badge;}
    bool isDragging() const noexcept {return draggingSlot>=0;}
    bool beginMicDrag(int slot,juce::Point<float> point,bool distanceOnly=false) {
        endMicDrag();refresh();
        if(slot<0 || slot>1 || !geometry[size_t(slot)].enabled || !isShowing())return false;
        draggingSlot=slot;distanceDrag=distanceOnly;dragStart=point;
        dragOffset=point-microphoneAnchor(slot);startDistance=geometry[size_t(slot)].distanceCm;
        gestures[0]=parameter(slot,distanceOnly ? "distance" : "unit");
        gestureCount=distanceOnly ? 1 : 2;
        if(!distanceOnly)gestures[1]=parameter(slot,"position");
        for(int i=0;i<gestureCount;++i)gestures[size_t(i)]->beginChangeGesture();
        foreground.repaint();handles[size_t(slot)]->repaint();return true;
    }
    void dragMicTo(juce::Point<float> point) {
        if(draggingSlot<0)return;
        const int slot=draggingSlot;
        // A host/source change can precede the next UI timer tick. Stop before
        // writing even one geometry value after a slot returns to captured IR.
        if(!isShowing() || raw(slot ? "Bon" : "Aon")<=.5f
            || effectiveDesign()!=design
            || layoutValue("layout")!=layoutModel
            || currentSettings().voice.driver!=driverModel
            || int(raw(slot ? "Bmic" : "Amic"))!=geometry[size_t(slot)].model
            || expanded(slot ? "Bmic" : "Amic")!=geometry[size_t(slot)].expansion) {
            endMicDrag();refresh();return;
        }
        if(distanceDrag) {
            const float travel=juce::jmax(72.f,cabinetBounds().getHeight()*.25f);
            setParameter(parameter(slot,"distance"),startDistance+(point.y-dragStart.y)*58.f/travel);
        } else {
            const auto target=point-dragOffset-distanceProjection(slot,geometry[size_t(slot)].distanceCm);
            int unit=geometry[size_t(slot)].unit;float closest=std::numeric_limits<float>::max();
            for(int i=0;i<speakerCount();++i) {
                const auto centre=speakerCentre(i);
                const juce::Point<float> candidate{juce::jlimit(centre.x,centre.x+speakerRadius(),target.x),centre.y};
                const float distance=candidate.getDistanceSquaredFrom(target);
                if(distance<closest){closest=distance;unit=i;}
            }
            setParameter(parameter(slot,"unit"),float(unit));
            setParameter(parameter(slot,"position"),(target.x-speakerCentre(unit).x)/speakerRadius());
        }
        refresh();
    }
    void endMicDrag() {
        if(draggingSlot<0)return;
        const int slot=draggingSlot;
        draggingSlot=-1;
        for(int i=0;i<gestureCount;++i)if(gestures[size_t(i)])gestures[size_t(i)]->endChangeGesture();
        gestures.fill(nullptr);gestureCount=0;foreground.repaint();handles[size_t(slot)]->repaint();
    }
    void resized() override {updatePlacement();}
    void visibilityChanged() override {if(!isShowing())endMicDrag();}
    void paint(juce::Graphics& g) override {
        const float w=float(getWidth()),h=float(getHeight());
        g.setGradientFill({juce::Colour(0xff30302b).withAlpha(.45f),w*.5f,h*.45f,
            juce::Colour(0xff151718).withAlpha(0.f),w*.5f,0.f,true});g.fillAll();
        const auto cab=cabinetBounds();
        const float supportY=spectralforge::cabLayoutView::groundSupport(cab).y;
        g.setColour(juce::Colours::black.withAlpha(.28f));
        g.fillEllipse(cab.getX()-18.f,supportY-5.f,cab.getWidth()+36.f,12.f);
        g.setColour(juce::Colours::black.withAlpha(.48f));
        g.fillEllipse(cab.getX()+cab.getWidth()*.08f,supportY-2.f,cab.getWidth()*.84f,5.f);
        g.setColour(juce::Colour(0xff5a5c57).withAlpha(.15f));g.drawLine(6.f,h-5.f,w-6.f,h-5.f,1.f);
        g.beginTransparencyLayer(geometry[0].enabled || geometry[1].enabled ? 1.f : .62f);
        spectralforge::cabLayoutView::enclosure(g,arrayGeometry(),cab);g.endTransparencyLayer();
    }
};
