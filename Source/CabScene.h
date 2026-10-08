#pragma once
#include "PluginProcessor.h"
#include "CabArtwork.h"
#include "HardwareArtwork.h"
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
private:
    ChimeraProcessor& processor;
    int lane;
    int design{-1},ampModel{-1};
    std::array<MicGeometry,2> geometry;
    std::array<uint64_t,2> displayedKeys{{~uint64_t{},~uint64_t{}}};
    spectralforge::cabArt::View cabinetImage;
    std::array<spectralforge::cabArt::View,4> speakerImages;
    class HeadImage : public juce::Component {
        juce::SharedResourcePointer<spectralforge::art::RasterBank> bank;
        int model{};
    public:
        HeadImage() {setInterceptsMouseClicks(false,false);}
        void setModel(int value) {
            if(model!=value){model=value;repaint();}
            setTitle("Current amplifier: "+juce::String(spectralforge::ampCatalog[size_t(juce::jlimit(0,spectralforge::ampModelCount-1,model))].name));
        }
        void paint(juce::Graphics& g) override {
            spectralforge::art::head(g,getLocalBounds().toFloat(),model);
        }
    } headImage;
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
            setTooltip("Drag across a speaker to change unit and cone position. Shift-drag vertically to change distance. Arrow keys move position; Shift + Up/Down changes distance.");
        }
        void mouseDown(const juce::MouseEvent& event) override {
            grabKeyboardFocus();toFront(false);
            owner.beginMicDrag(slot,event.getEventRelativeTo(&owner).position,event.mods.isShiftDown());
        }
        void mouseDrag(const juce::MouseEvent& event) override {owner.dragMicTo(event.getEventRelativeTo(&owner).position);}
        void mouseUp(const juce::MouseEvent&) override {owner.endMicDrag();}
        bool keyPressed(const juce::KeyPress& key) override {return owner.nudgeMicrophone(slot,key);}
    } microphoneA{*this,0},microphoneB{*this,1};
    std::array<MicNode*,2> microphones{{&microphoneA,&microphoneB}};
    int draggingSlot{-1},gestureCount{};
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
    juce::Point<float> distanceProjection(int slot,float cm) const {
        const float depth=juce::jlimit(0.f,1.f,(cm-2.f)/58.f);
        const float scale=cabinetBounds().getWidth()/420.f;
        return {(slot ? 1.f : -1.f)*depth*36.f*scale,depth*40.f*scale};
    }
    void updatePlacement() {
        if(getWidth()==0 || getHeight()==0)return;
        const float cabSize=juce::jmin(float(getWidth())*.82f,float(getHeight())*.747f);
        const float headHeight=cabSize/3.f;
        const float stackHeight=cabSize+headHeight-9.f;
        const float top=juce::jmax(0.f,(float(getHeight())-stackHeight)*.45f);
        const float x=(float(getWidth())-cabSize)*.5f;
        headImage.setBounds(juce::Rectangle<float>(x-4.f,top,cabSize+8.f,headHeight).toNearestInt());
        cabinetImage.setBounds(juce::Rectangle<float>(x,top+headHeight-9.f,cabSize,cabSize).toNearestInt());
        const auto cab=cabinetBounds();
        const float diameter=cab.getWidth()*(design ? .3798f : .4014f);
        for(int i=0;i<4;++i)
            speakerImages[size_t(i)].setBounds(juce::Rectangle<float>(diameter,diameter).withCentre(speakerCentre(i)).toNearestInt());
        foreground.setBounds(getLocalBounds());
        for(int i=0;i<2;++i) {
            auto& node=*microphones[size_t(i)];
            const float depth=juce::jlimit(0.f,1.f,(geometry[size_t(i)].distanceCm-2.f)/58.f);
            const float height=juce::jlimit(68.f,100.f,cab.getWidth()*.194f)*(1.f+depth*.14f);
            const auto anchor=microphoneAnchor(i);
            node.setTransform({});
            node.setBounds(juce::Rectangle<float>(anchor.x-32.f,anchor.y-height*.16f,64.f,height).toNearestInt());
            // The capsule is the physical anchor. A gentle angle separates the
            // two microphones when the user deliberately gives them equal values.
            node.setTransform(juce::AffineTransform::rotation(i ? .15f : -.28f,anchor.x,anchor.y));
        }
        foreground.repaint();repaint();
    }
    bool nudgeMicrophone(int slot,const juce::KeyPress& key) {
        if(!geometry[size_t(slot)].enabled)return false;
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
            const auto anchor=microphoneAnchor(i),target=coneTarget(i);
            const auto colour=juce::Colour(i ? 0xffc6a16c : 0xff77aeba);
            const auto bottom=anchor.translated(i ? -6.f : 17.f,68.f);
            const float floor=float(getHeight())-6.f;
            juce::Path cable;cable.startNewSubPath(bottom);
            cable.cubicTo(bottom.x+float(i ? 19 : -27),bottom.y+43.f,
                float(i ? getWidth()-70 : 70),floor+9.f,float(i ? getWidth()-25 : 25),floor);
            g.setColour(juce::Colours::black.withAlpha(.7f));g.strokePath(cable,juce::PathStrokeType(4.f));
            g.setColour(juce::Colour(0xff43464a).withAlpha(.7f));g.strokePath(cable,juce::PathStrokeType(1.2f));
            g.setColour(colour.withAlpha(.70f));g.drawLine(juce::Line<float>(target,anchor),1.f);
            g.drawEllipse(target.x-3.f,target.y-3.f,6.f,6.f,1.f);
            if(draggingSlot==i) {
                const float radius=speakerRadius();const auto centre=speakerCentre(geometry[size_t(i)].unit);
                g.setColour(colour.withAlpha(.30f));g.drawEllipse(centre.x-radius,centre.y-radius,radius*2.f,radius*2.f,1.f);
                g.drawLine(centre.x,centre.y,centre.x+radius,centre.y,1.f);
            }
            const auto badge=anchor.translated(i ? 24.f : -30.f,29.f);
            g.setColour(juce::Colour(0xff111416).withAlpha(.9f));g.fillEllipse(badge.x-9.f,badge.y-9.f,18.f,18.f);
            g.setColour(colour);g.setFont(juce::FontOptions(10.f,juce::Font::bold));
            g.drawText(i ? "B" : "A",juce::Rectangle<float>(badge.x-9.f,badge.y-9.f,18.f,18.f).toNearestInt(),juce::Justification::centred);
        }
    }
public:
    CabScene(ChimeraProcessor& value,int rig):processor(value),lane(rig) {
        setComponentID("cabScene"+juce::String(lane+1));
        setInterceptsMouseClicks(false,true);
        addAndMakeVisible(headImage);addAndMakeVisible(cabinetImage);
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
        refresh();
    }
    ~CabScene() override {endMicDrag();}
    void refresh() {
        const int nextDesign=int(raw("design"));
        const int nextAmp=processor.selectedAmpModel(lane);
        bool changed=design!=nextDesign || ampModel!=nextAmp;
        design=nextDesign;ampModel=nextAmp;headImage.setModel(ampModel);
        cabinetImage.setAsset(spectralforge::cabArt::cabinet(design),design ? "Chimera Bass 4x10 cabinet" : "Chimera Guitar 4x12 cabinet");
        for(auto& image:speakerImages)image.setAsset(spectralforge::cabArt::speaker(design),design ? "Original 10-inch bass speaker unit" : "Original 12-inch guitar speaker unit");
        bool any=false;
        for(int i=0;i<2;++i) {
            const auto prefix=juce::String(i ? "B" : "A");
            const auto value=[&](const char* suffix){return raw((prefix+suffix).toRawUTF8());};
            auto& geo=geometry[size_t(i)];
            geo={value("on")>.5f,int(value("mic")),int(value("unit")),value("position"),value("distance")};
            const spectralforge::originalCab::Settings settings{geo.enabled,design,0,geo.model,geo.unit,0,geo.position,geo.distanceCm};
            const auto key=spectralforge::originalCab::key(settings);
            changed=changed || displayedKeys[size_t(i)]!=key;displayedKeys[size_t(i)]=key;
            auto& image=*microphones[size_t(i)];
            image.setAsset(spectralforge::cabArt::originalMicrophone(geo.model),"Original Mic "+prefix);
            image.setActive(geo.enabled);image.setVisible(geo.enabled);any=any || geo.enabled;
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
        foreground.repaint();return true;
    }
    void dragMicTo(juce::Point<float> point) {
        if(draggingSlot<0)return;
        const int slot=draggingSlot;
        // A host/source change can precede the next UI timer tick. Stop before
        // writing even one geometry value after a slot returns to captured IR.
        if(!isShowing() || raw(slot ? "Bon" : "Aon")<=.5f) {
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
        draggingSlot=-1;
        for(int i=0;i<gestureCount;++i)if(gestures[size_t(i)])gestures[size_t(i)]->endChangeGesture();
        gestures.fill(nullptr);gestureCount=0;foreground.repaint();
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
