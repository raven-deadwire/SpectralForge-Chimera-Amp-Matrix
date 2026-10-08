#pragma once
#include "CabPanel.h"
#include "CabArtwork.h"
#include "HardwareArtwork.h"
#include <functional>

namespace spectralforge::cabRoom {
// This is scenery for navigating rigs. It has no audio/room-response parameter.
struct ArtworkBank {
    juce::Image room;
    ArtworkBank() {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        int size=0;
        if(const auto* bytes=ChimeraArtworkData::getNamedResource("roomstudio_png",size)) {
            room=juce::ImageFileFormat::loadFrom(bytes,static_cast<size_t>(size));
            if(room.isValid()) {
                const double scale=juce::jmin(1.0,1152.0/room.getWidth(),576.0/room.getHeight());
                if(scale<1.0)room=room.rescaled(juce::jmax(1,juce::roundToInt(room.getWidth()*scale)),
                    juce::jmax(1,juce::roundToInt(room.getHeight()*scale)),juce::Graphics::highResamplingQuality);
            }
        }
    }
    ~ArtworkBank() {room={};}
};
inline int mode(ChimeraProcessor& processor) {
    return juce::jlimit(0,2,juce::roundToInt(processor.parameters().getRawParameterValue("mode")->load()));
}
inline int rigCount(ChimeraProcessor& processor) {return mode(processor)+1;}
inline juce::String rigName(int currentMode,int lane) {
    return currentMode==2 ? (lane==0 ? "LOW" : lane==1 ? "MID" : "HIGH") : "RIG "+juce::String(lane+1);
}
}

// Physical rigs share a single room. Every child is a keyboard-accessible
// navigation button; opening a cabinet never selects an audio source.
class CabRoomOverview : public juce::Component, private juce::Timer {
    ChimeraProcessor& processor;
    juce::SharedResourcePointer<spectralforge::cabRoom::ArtworkBank> scenery;
    juce::SharedResourcePointer<spectralforge::art::RasterBank> heads;
    juce::SharedResourcePointer<spectralforge::cabArt::Bank> cabinets;
    struct RigState {
        int amp{-1},design{-1};
        std::array<int,2> original{{-1,-1}},source{{-1,-1}};
        std::array<std::array<uint64_t,4>,2> revision{};
        std::array<spectralforge::IRMetadata,2> capture;
        bool cabEnabled{true},ampEnabled{true},muted{},solo{},diOnly{};
        juce::String name,caption;
    };
    class Rig : public juce::Button {
        CabRoomOverview& room;
        int lane;
    public:
        Rig(CabRoomOverview& owner,int index):juce::Button("Open cabinet"),room(owner),lane(index) {
            setComponentID("cabRoomRig"+juce::String(lane+1));setMouseCursor(juce::MouseCursor::PointingHandCursor);
            setWantsKeyboardFocus(true);onClick=[this]{room.selectRig(lane);};
        }
        void paintButton(juce::Graphics& g,bool hovered,bool down) override {room.paintRig(g,lane,getLocalBounds().toFloat(),hovered || hasKeyboardFocus(true),down);}
    };
    std::array<RigState,3> state;
    std::array<std::unique_ptr<Rig>,3> rigs;
    int currentMode{-1};
    float read(const juce::String& id) const {return processor.parameters().getRawParameterValue(id)->load();}
    void timerCallback() override {if(isShowing())refreshState();}
    void paintRig(juce::Graphics& g,int lane,juce::Rectangle<float> area,bool hovered,bool down) {
        const auto& rig=state[static_cast<size_t>(lane)];
        const auto gold=juce::Colour(0xffc4a678),ink=juce::Colour(0xffe5dfd4);
        const float floor=area.getHeight()-57.f;
        const float side=juce::jmin(area.getWidth()*.86f,(floor-12.f)/1.20f);
        const float centre=area.getCentreX();
        const juce::Rectangle<float> cabinetBounds(centre-side*.5f,floor-side*(rig.design==1 ? .9410f : .9593f),side,side);
        const float cabinetTop=cabinetBounds.getY()+side*(rig.design==1 ? .0510f : .0399f);
        const juce::Rectangle<float> headBounds(centre-side*.40f,cabinetTop+2.f-side*.267f,side*.8f,side*.267f);
        // Ground shadows and a small pool of reflected amber light belong to
        // the scene, not to a card behind each piece of equipment.
        g.setColour(gold.withAlpha(hovered ? .13f : .045f));
        g.fillEllipse(centre-side*.56f,floor-16.f,side*1.12f,25.f);
        g.setColour(juce::Colours::black.withAlpha(.62f));g.fillEllipse(centre-side*.47f,floor-9.f,side*.94f,16.f);
        const bool original=rig.original[0]>0 || rig.original[1]>0;
        g.beginTransparencyLayer(rig.muted || !rig.cabEnabled || rig.diOnly ? .57f : 1.f);
        g.setColour(juce::Colours::white);
        if(original) {
            const auto asset=spectralforge::cabArt::cabinet(rig.design);
            const auto& image=cabinets->images[static_cast<size_t>(asset)];
            if(image.isValid())g.drawImage(image,cabinetBounds,juce::RectanglePlacement::centred);
            else spectralforge::cabArt::neutral(g,cabinetBounds,"CABINET");
        } else {
            const int slot=rig.source[0]!=0 ? 0 : 1;
            const auto face=cabinetBounds.reduced(side*.083f,side*.092f);
            if(rig.source[slot]!=0)spectralforge::art::cabinet(g,face,rig.capture[static_cast<size_t>(slot)]);
            else spectralforge::cabArt::neutral(g,face,"FILTERS");
        }
        g.endTransparencyLayer();
        g.beginTransparencyLayer(!rig.ampEnabled || rig.muted || rig.diOnly ? .57f : 1.f);
        spectralforge::art::head(g,headBounds,rig.amp);g.endTransparencyLayer();
        if(hovered) {
            g.setColour(gold.withAlpha(down ? .95f : .65f));
            const auto corners=cabinetBounds.reduced(side*.065f);
            for(const auto p:{corners.getTopLeft(),corners.getTopRight(),corners.getBottomLeft(),corners.getBottomRight()}) {
                const float dx=p.x<centre ? 13.f : -13.f;
                const float dy=p.y<corners.getCentreY() ? 13.f : -13.f;
                g.drawLine(p.x,p.y,p.x+dx,p.y,1.3f);g.drawLine(p.x,p.y,p.x,p.y+dy,1.3f);
            }
        }
        const auto labelArea=juce::Rectangle<float>(centre-juce::jmin(132.f,area.getWidth()*.47f),floor+8.f,
            juce::jmin(264.f,area.getWidth()*.94f),43.f);
        g.setColour(juce::Colour(0xff101312).withAlpha(.82f));g.fillRoundedRectangle(labelArea,3.f);
        g.setColour(hovered ? gold : ink);g.setFont(juce::FontOptions(13.f,juce::Font::bold));
        g.drawText(rig.name,labelArea.toNearestInt().withHeight(22),juce::Justification::centred);
        g.setColour(juce::Colour(0xffbab5a9));g.setFont(juce::FontOptions(10.5f));
        g.drawText(hovered ? "OPEN CABINET  >" : rig.caption,labelArea.toNearestInt().withTrimmedTop(22),juce::Justification::centred);
    }
public:
    std::function<void(int)> onCabinetSelected;
    explicit CabRoomOverview(ChimeraProcessor& p):processor(p) {
        setComponentID("cabRoomOverview");setTitle("Chimera cabinet room");
        setDescription("Choose a cabinet to open its speaker and microphone controls.");
        for(int i=0;i<3;++i) {rigs[static_cast<size_t>(i)]=std::make_unique<Rig>(*this,i);addAndMakeVisible(*rigs[static_cast<size_t>(i)]);}
        refreshState();startTimerHz(12);
    }
    ~CabRoomOverview() override {stopTimer();}
    int activeRigCount() const {return spectralforge::cabRoom::rigCount(processor);}
    juce::Rectangle<int> getRigBounds(int lane) const {
        return lane>=0 && lane<activeRigCount() ? rigs[static_cast<size_t>(lane)]->getBounds() : juce::Rectangle<int>{};
    }
    bool selectRig(int lane) {
        if(lane<0 || lane>=activeRigCount())return false;
        if(onCabinetSelected)onCabinetSelected(lane);
        return true;
    }
    void refreshState() {
        const int nextMode=spectralforge::cabRoom::mode(processor);
        if(currentMode!=nextMode) {currentMode=nextMode;resized();repaint();}
        for(int i=0;i<3;++i) {
            auto& rig=state[static_cast<size_t>(i)];
            auto& component=*rigs[static_cast<size_t>(i)];
            component.setVisible(i<activeRigCount());
            if(i>=activeRigCount())continue;
            const auto n=juce::String(i+1);
            const int amp=processor.selectedAmpModel(i),design=juce::roundToInt(read(spectralforge::originalCabID(i,"design")));
            const bool cabEnabled=read("cab"+n)>.5f,ampEnabled=read("ampon"+n)>.5f;
            const bool muted=read("mute"+n)>.5f,solo=read("solo"+n)>.5f;
            const bool diOnly=currentMode==2 && i==0 && read("lowampmix")<=.0001f;
            bool changed=rig.amp!=amp || rig.design!=design || rig.cabEnabled!=cabEnabled || rig.ampEnabled!=ampEnabled
                || rig.muted!=muted || rig.solo!=solo || rig.diOnly!=diOnly;
            rig.amp=amp;rig.design=design;rig.cabEnabled=cabEnabled;rig.ampEnabled=ampEnabled;rig.muted=muted;rig.solo=solo;rig.diOnly=diOnly;
            for(int slot=0;slot<2;++slot) {
                const auto s=static_cast<size_t>(slot);
                const int original=read(spectralforge::originalCabID(i,slot ? "Bon" : "Aon"))>.5f ? 1 : 0;
                const int source=juce::roundToInt(read((slot ? "cabBtype" : "cabtype")+n));
                const auto revision=processor.micDisplayRevision(i,slot);
                if(rig.source[s]!=source || rig.original[s]!=original || rig.revision[s]!=revision) {
                    rig.source[s]=source;rig.original[s]=original;rig.revision[s]=revision;
                    rig.capture[s]=processor.micCaptureMetadata(i,slot);changed=true;
                }
            }
            const auto name=spectralforge::cabRoom::rigName(currentMode,i)+(solo ? " / SOLO" : "");
            juce::String caption=muted ? "MUTED" : diOnly ? "CLEAN DI / AMP MIX 0%" : !cabEnabled ? "CABINET BYPASSED" :
                rig.original[0] && rig.original[1] ? (design==1 ? "ORIGINAL BASS 4x10" : "ORIGINAL GUITAR 4x12") :
                rig.original[0] || rig.original[1] ?
                    (rig.source[rig.original[0] ? 1 : 0] ? "ORIGINAL + CAPTURE / A-B" : "ORIGINAL + FILTERS / A-B") :
                rig.source[0] && rig.source[1] ? "CAPTURED IR / A-B" :
                rig.source[0] || rig.source[1] ? "CAPTURE + FILTERS / A-B" : "FILTERS ONLY";
            changed=changed || rig.name!=name || rig.caption!=caption;rig.name=name;rig.caption=caption;
            component.setButtonText(name+" / Open cabinet");
            component.setTooltip(name+" / "+caption+"\nOpen the cabinet view. IR Loader is available in the next screen.");
            component.getProperties().set("cabRoomAmpModel",amp);
            component.getProperties().set("cabRoomDesign",design);
            component.getProperties().set("cabRoomSource",caption);
            if(changed)component.repaint();
        }
    }
    void resized() override {
        const int count=activeRigCount();
        const int margin=juce::jmax(18,juce::roundToInt(getWidth()*.055f));
        const int gap=juce::jmax(12,juce::roundToInt(getWidth()*.03f));
        const int available=juce::jmax(1,getWidth()-margin*2-gap*(count-1));
        const int width=available/count;
        const int top=getHeight()>500 ? 83 : 43;
        for(int i=0;i<3;++i)rigs[static_cast<size_t>(i)]->setBounds(margin+i*(width+gap),top,width,juce::jmax(1,getHeight()-top-12));
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff151714));
        if(scenery->room.isValid()) {
            g.setColour(juce::Colours::white);
            g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
            g.drawImage(scenery->room,getLocalBounds().toFloat(),juce::RectanglePlacement::fillDestination|juce::RectanglePlacement::xMid|juce::RectanglePlacement::yBottom);
        }
        g.setGradientFill({juce::Colours::black.withAlpha(.42f),0.f,0.f,juce::Colours::black.withAlpha(0.f),0.f,90.f,false});
        g.fillRect(0,0,getWidth(),juce::jmin(90,getHeight()));
        g.setColour(juce::Colour(0xffe5dfd4));g.setFont(juce::FontOptions(14.f,juce::Font::bold));
        g.drawText(currentMode==2 ? "MATRIX / CABINET ROOM" : currentMode==1 ? "DUAL / CABINET ROOM" : "CABINET ROOM",24,13,getWidth()-48,22,juce::Justification::centredLeft);
        if(getHeight()>500) {
            g.setColour(juce::Colour(0xffc6bba8));g.setFont(juce::FontOptions(12.f));
            g.drawText("Select a cabinet to move closer and place its microphones.",24,38,getWidth()-48,22,juce::Justification::centredLeft);
        }
        g.setColour(juce::Colour(0xffb99b6c).withAlpha(.35f));g.drawRect(getLocalBounds(),1);
    }
};

// The same workspace serves the main-room click and the legacy PANEL button.
// Keeping this as presentation state preserves projects, A/B, IRs and routing.
class CabWorkspace : public juce::Component, private juce::Timer {
    spectralforge::art::DialogLook look;
    ChimeraProcessor& processor;
    CabRoomOverview room;
    std::unique_ptr<CabPanel> panel;
    juce::TextButton back{"< ROOM"};
    juce::Label breadcrumb;
    bool roomView{true};
    int focused{-1},lastMode{-1};
    void timerCallback() override {refreshState();}
    void updateNavigation() {
        room.setVisible(roomView);if(panel)panel->setVisible(!roomView);
        back.setVisible(!roomView && activeRigCount()>1);
        breadcrumb.setText(roomView ? "CHIMERA / CABINET ROOM" :
            "CHIMERA / "+spectralforge::cabRoom::rigName(lastMode,focused)+" / CABINET",juce::dontSendNotification);
        getProperties().set("cabRoomView",roomView);getProperties().set("cabFocusedRig",focused);
        resized();repaint();
    }
public:
    explicit CabWorkspace(ChimeraProcessor& p,int initialLane=0,bool focusRequested=false):processor(p),room(p) {
        setLookAndFeel(&look);setComponentID("cabWorkspace");
        addAndMakeVisible(room);addAndMakeVisible(back);addAndMakeVisible(breadcrumb);
        back.setComponentID("cabBackToRoom");back.setTooltip("Return to the complete room without changing the sound.");
        back.onClick=[this]{showRoom();};room.onCabinetSelected=[this](int lane){focusRig(lane);};
        breadcrumb.setFont(juce::FontOptions(12.f,juce::Font::bold));
        breadcrumb.setColour(juce::Label::textColourId,juce::Colour(0xffcfb991));
        setSize(1040,792);lastMode=spectralforge::cabRoom::mode(processor);
        if(activeRigCount()==1 || focusRequested)focusRig(juce::jlimit(0,activeRigCount()-1,initialLane));
        else updateNavigation();
        startTimerHz(12);
    }
    ~CabWorkspace() override {stopTimer();panel.reset();setLookAndFeel(nullptr);}
    int activeRigCount() const {return spectralforge::cabRoom::rigCount(processor);}
    bool isRoomView() const {return roomView;}
    int focusedRig() const {return focused;}
    void showRoom() {
        if(activeRigCount()==1) {focusRig(0);return;}
        if(panel)panel->finishInteraction();
        roomView=true;room.refreshState();updateNavigation();
    }
    bool focusRig(int lane) {
        if(lane<0 || lane>=activeRigCount())return false;
        if(panel)panel->finishInteraction();
        if(!panel || focused!=lane) {
            panel=std::make_unique<CabPanel>(processor,lane);
            panel->setComponentID("cabFocusedPanel");addAndMakeVisible(*panel);
        }
        focused=lane;roomView=false;panel->setView(CabPanel::View::cabinet);
        updateNavigation();return true;
    }
    void refreshState() {
        const int nextMode=spectralforge::cabRoom::mode(processor);
        if(nextMode!=lastMode) {
            lastMode=nextMode;
            if(activeRigCount()==1)focusRig(0);else showRoom();
        }
        if(roomView)room.refreshState();
    }
    void resized() override {
        room.setBounds(0,44,getWidth(),juce::jmax(1,getHeight()-44));
        if(panel)panel->setBounds(0,44,getWidth(),juce::jmax(1,getHeight()-44));
        back.setBounds(14,8,94,28);
        breadcrumb.setBounds(back.isVisible() ? 121 : 18,8,getWidth()-(back.isVisible() ? 139 : 36),28);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff101312));g.setColour(juce::Colour(0xff4b4437));g.drawHorizontalLine(43,0.f,float(getWidth()));
    }
};
