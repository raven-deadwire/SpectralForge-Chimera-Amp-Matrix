#pragma once
#include "PluginProcessor.h"
#include "CabArtwork.h"
#include "CabHeadView.h"
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
    };
    struct MicVisualGeometry {
        juce::Rectangle<float> bodyBounds;
        juce::Point<float> capsule,mount,cable,badge;
        float scale{};
    };
private:
    ChimeraProcessor& processor;
    int lane;
    int design{-1},ampModel{-1};
    std::array<MicGeometry,2> geometry;
    std::array<MicVisualGeometry,2> visualGeometry;
    std::array<uint64_t,2> displayedKeys{{~uint64_t{},~uint64_t{}}};
    spectralforge::cabArt::View cabinetImage;
    std::array<spectralforge::cabArt::View,4> speakerImages;
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
    juce::RangedAudioParameter* parameter(int slot,const char* suffix) {
        return processor.parameters().getParameter(spectralforge::originalCabID(lane,(juce::String(slot ? "B" : "A")+suffix).toRawUTF8()));
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
        const float width=cabinetBounds().getWidth();
        // Both slots share the same camera/depth axis, parallel to the rigid
        // end-address barrel. Slot identity never changes the projection.
        return {depth*width*.060f,depth*width*.028f};
    }
    void updatePlacement() {
        if(getWidth()==0 || getHeight()==0)return;
        // Reserve room for a real microphone body beyond the last cone and for
        // stands below it, including the farthest supported 60 cm position.
        const float cabSize=juce::jmin(float(getWidth())*.76f,float(getHeight())*.71f);
        const float headHeight=cabSize/3.f;
        const float stackHeight=cabSize+headHeight-9.f;
        const float top=juce::jmax(0.f,(float(getHeight())-stackHeight)*.45f);
        const float x=(float(getWidth())-cabSize)*.5f;
        cabinetImage.setBounds(juce::Rectangle<float>(x,top+headHeight-9.f,cabSize,cabSize).toNearestInt());
        const auto cab=cabinetBounds();
        headImage.setBounds(spectralforge::cabHead::boundsAboveCabinet(cab).toNearestInt());
        const float diameter=cab.getWidth()*(design ? .3798f : .4014f);
        for(int i=0;i<4;++i)
            speakerImages[size_t(i)].setBounds(juce::Rectangle<float>(diameter,diameter).withCentre(speakerCentre(i)).toNearestInt());
        foreground.setBounds(getLocalBounds());
        for(int i=0;i<2;++i) {
            auto& node=*microphones[size_t(i)];
            const float depth=juce::jlimit(0.f,1.f,(geometry[size_t(i)].distanceCm-2.f)/58.f);
            const auto presentation=spectralforge::cabArt::microphonePresentation(geometry[size_t(i)].model);
            const float longest=speakerRadius()*2.f*presentation.longestSpeakerRatio
                *(design ? 1.2f : 1.f)*(1.f+depth*.12f);
            const float aspect=node.artworkAspectRatio();
            const float width=aspect>=1.f ? longest : longest*aspect;
            const float height=aspect>=1.f ? longest/aspect : longest;
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
            visual.cable=point(presentation.cable);visual.scale=longest;
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
        for(int i=0;i<2;++i)if(geometry[size_t(i)].enabled) {
            const auto& visual=visualGeometry[size_t(i)];
            const auto anchor=visual.capsule,target=coneTarget(i);
            const auto colour=juce::Colour(i ? 0xffc6a16c : 0xff77aeba);
            const auto cab=cabinetBounds();
            const float scale=juce::jlimit(.65f,1.25f,cab.getWidth()/410.f);
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
        for(int i=0;i<4;++i) {
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
        const int nextDesign=int(raw("design"));
        const int nextAmp=processor.selectedAmpModel(lane);
        bool changed=design!=nextDesign || ampModel!=nextAmp;
        if(draggingSlot>=0 && design!=nextDesign)endMicDrag();
        design=nextDesign;ampModel=nextAmp;headImage.setModel(ampModel);
        cabinetImage.setAsset(spectralforge::cabArt::cabinet(design),design ? "Chimera Bass 4x10 cabinet" : "Chimera Guitar 4x12 cabinet");
        for(auto& image:speakerImages)image.setAsset(spectralforge::cabArt::speaker(design),design ? "Original 10-inch bass speaker unit" : "Original 12-inch guitar speaker unit");
        bool any=false;
        for(int i=0;i<2;++i) {
            const auto prefix=juce::String(i ? "B" : "A");
            const auto value=[&](const char* suffix){return raw((prefix+suffix).toRawUTF8());};
            auto& geo=geometry[size_t(i)];
            const MicGeometry next{value("on")>.5f,int(value("mic")),int(value("unit")),value("position"),value("distance")};
            if(draggingSlot==i && (!next.enabled || geo.model!=next.model))endMicDrag();
            geo=next;
            const spectralforge::originalCab::Settings settings{geo.enabled,design,0,geo.model,geo.unit,0,geo.position,geo.distanceCm};
            const auto key=spectralforge::originalCab::key(settings);
            changed=changed || displayedKeys[size_t(i)]!=key;displayedKeys[size_t(i)]=key;
            auto& image=*microphones[size_t(i)];
            image.setAsset(spectralforge::cabArt::originalMicrophone(geo.model),"Original Mic "+prefix);
            image.setActive(geo.enabled);image.setVisible(geo.enabled);any=any || geo.enabled;
            handles[size_t(i)]->setVisible(geo.enabled);
            if(!geo.enabled && hoveredSlot==i)setHoveredMic(-1);
        }
        if(draggingSlot>=0 && !geometry[size_t(draggingSlot)].enabled)endMicDrag();
        cabinetImage.setAlpha(any ? 1.f : .62f);headImage.setAlpha(any ? 1.f : .62f);
        for(auto& speaker:speakerImages)speaker.setAlpha(any ? 1.f : .62f);
        if(changed)updatePlacement();
    }
    juce::Rectangle<float> cabinetBounds() const {
        return cabinetImage.artworkBounds().translated(float(cabinetImage.getX()),float(cabinetImage.getY()));
    }
    juce::Point<float> speakerCentre(int unit) const {
        const auto cab=cabinetBounds();const int u=juce::jlimit(0,3,unit);
        // Artwork anchors are measured within the alpha-trimmed cabinet. Their
        // ordering is identical to originalCab::centres: upper L/R, lower L/R.
        static constexpr std::array<juce::Point<float>,4> guitar{{{.2770f,.3027f},{.7187f,.3036f},{.2770f,.7138f},{.7196f,.7138f}}};
        static constexpr std::array<juce::Point<float>,4> bass{{{.2724f,.2814f},{.7249f,.2814f},{.2715f,.7016f},{.7249f,.7025f}}};
        const auto centre=(design ? bass : guitar)[size_t(u)];
        const float x=centre.x,y=centre.y;
        return {cab.getX()+cab.getWidth()*x,cab.getY()+cab.getHeight()*y};
    }
    float speakerRadius() const {return cabinetBounds().getWidth()*(design ? .1686f : .1861f);}
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
            || int(raw("design"))!=design
            || int(raw(slot ? "Bmic" : "Amic"))!=geometry[size_t(slot)].model) {
            endMicDrag();refresh();return;
        }
        if(distanceDrag) {
            const float travel=juce::jmax(72.f,cabinetBounds().getHeight()*.25f);
            setParameter(parameter(slot,"distance"),startDistance+(point.y-dragStart.y)*58.f/travel);
        } else {
            const auto target=point-dragOffset-distanceProjection(slot,geometry[size_t(slot)].distanceCm);
            int unit=geometry[size_t(slot)].unit;float closest=std::numeric_limits<float>::max();
            for(int i=0;i<4;++i) {
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
        g.setColour(juce::Colours::black.withAlpha(.45f));
        g.fillEllipse(cab.getX()-32.f,h-18.f,cab.getWidth()+64.f,25.f);
        g.setColour(juce::Colour(0xff5a5c57).withAlpha(.15f));g.drawLine(6.f,h-5.f,w-6.f,h-5.f,1.f);
    }
};
