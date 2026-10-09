#pragma once
#include "CabLayoutModel.h"
#include "CabSpeakerArt.h"
#include "CabArtwork.h"
#include "CabHeadDimensions.h"
#include "CabCameraFraming.h"
#include "CabHornArtwork.h"

namespace spectralforge::cabLayoutView {
// Each view frames complete rigs in metres. Room rigs share a single camera
// scale; focused views can fit their selected stack without unused equipment.
inline constexpr float depthProjection=.16f;
struct Projection { juce::Rectangle<float> box; float pixelsPerMetre{}; };
inline cabCamera::Envelope stackEnvelope(cabLayout::Settings selected,int amplifier) {
    const auto box=cabLayout::geometry(selected).box;
    const auto head=cabPhysical::head(amplifier);
    return cabCamera::stack(float(box.width),float(box.height+box.depth*depthProjection),
        head.width,head.height+head.depth*depthProjection);
}
inline float cameraScale(juce::Rectangle<float> area,cabLayout::Settings selected,int amplifier) {
    return cabCamera::focusScale(area.getWidth(),area.getHeight(),stackEnvelope(selected,amplifier));
}
inline Projection projectAtScale(cabLayout::Settings selected,juce::Rectangle<float> area,float scale) {
    const auto model=cabLayout::geometry(selected);
    const float width=float(model.box.width)*scale;
    const float height=float(model.box.height+model.box.depth*depthProjection)*scale;
    const float bottom=area.getBottom()-area.getHeight()*.07f;
    return {{area.getCentreX()-width*.5f,bottom-height,width,height},scale};
}
inline Projection project(cabLayout::Settings selected,juce::Rectangle<float> area,int amplifier) {
    return projectAtScale(selected,area,cameraScale(area,selected,amplifier));
}
inline juce::Rectangle<float> baffle(const cabLayout::Geometry& model,juce::Rectangle<float> box) {
    return box.withTrimmedTop(float(model.box.depth)*depthProjection*box.getWidth()/float(model.box.width));
}
inline juce::Point<float> point(const cabLayout::Geometry& model,juce::Rectangle<float> box,originalCab::Point p) {
    const auto face=baffle(model,box);
    return {face.getCentreX()+float(p.x/model.box.width)*face.getWidth(),face.getCentreY()-float(p.y/model.box.height)*face.getHeight()};
}
inline float radius(const cabLayout::Geometry& model,juce::Rectangle<float> box) {return float(model.radius/model.box.width)*box.getWidth();}
// The authored guitar 4x12 uses the familiar slanted upper baffle. Smaller
// guitar boxes retain rounded leather returns; bass frames stay straight.
// This enclosure presentation does not alter the authored acoustic geometry.
inline bool slantedFront(const cabLayout::Geometry& model,bool bass) {return !bass && model.count==4;}
struct GroundSupport {std::array<juce::Rectangle<float>,2> feet;float y{};};
inline GroundSupport groundSupport(juce::Rectangle<float> box) {
    const float height=juce::jlimit(2.5f,7.5f,box.getWidth()*.022f),width=box.getWidth()*.057f;
    GroundSupport result;result.y=box.getBottom()+height;
    for(int i=0;i<2;++i)result.feet[size_t(i)]={box.getX()+box.getWidth()*(i ? .893f : .05f),
        box.getBottom()-1.f,width,height+1.f};
    return result;
}
inline void enclosure(juce::Graphics& g,const cabLayout::Geometry& model,juce::Rectangle<float> box,bool bass) {
    const juce::SharedResourcePointer<cabArt::Bank> bank;
    const auto& skin=bank->enclosureSkins[size_t(bass)];
    // The same contact plane anchors both the rubber feet and scene shadows.
    int footIndex=0;
    for(const auto foot:groundSupport(box).feet) {
        g.setColour(juce::Colour(0xff070a0b));g.fillRoundedRectangle(foot,1.4f);
        g.setColour(juce::Colour(0xff494d46));
        g.drawLine(foot.getX()+1.f,foot.getY()+1.5f,foot.getRight()-1.f,foot.getY()+1.5f,.8f);
        if(skin.feet[size_t(footIndex)].isValid()) {
            g.setColour(juce::Colours::white);
            g.drawImage(skin.feet[size_t(footIndex)],foot,juce::RectanglePlacement::stretchToFit);
        }
        ++footIndex;
    }
    cabEnclosureArt::paint(g,skin,box,baffle(model,box),
        box.getWidth()/float(model.box.width),slantedFront(model,bass));
}
inline void speakers(juce::Graphics& g,const cabLayout::Geometry& model,juce::Rectangle<float> box,int driver,int legacyDesign=0) {
    const juce::SharedResourcePointer<cabArt::Bank> bank;
    const float inches=float(cabExpansion::diameter({{false,legacyDesign},driver,0,0}));
    const float outerRadius=inches*.0254f*.5f;
    const float diameter=outerRadius*2.f*box.getWidth()/float(model.box.width);
    const auto& image=bank->frontSpeakers[size_t(driver>0 ? juce::jlimit(1,14,driver)+1 : int(legacyDesign==1))];
    g.setColour(juce::Colours::white);g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    for(int n=0;n<model.count;++n)g.drawImage(image,
        juce::Rectangle<float>(diameter,diameter).withCentre(point(model,box,model.centres[size_t(n)])),juce::RectanglePlacement::stretchToFit);
}
inline void grille(juce::Graphics& g,const cabLayout::Geometry& model,juce::Rectangle<float> box,bool bass) {
    const juce::SharedResourcePointer<cabArt::Bank> bank;
    cabEnclosureArt::paintGrille(g,bank->enclosureSkins[size_t(bass)],baffle(model,box),
        box.getWidth()/float(model.box.width),slantedFront(model,bass));
}
inline void tweeter(juce::Graphics& g,const cabLayout::Geometry& model,juce::Rectangle<float> box,
                    bool bass,float level,int kind) {
    if(level<=0.f)return;
    const auto centre=point(model,box,model.horn);
    const float scale=box.getWidth()/float(model.box.width);
    const juce::SharedResourcePointer<cabArt::Bank> bank;
    const auto& image=bank->enclosureSkins[1].horn;
    if(bass && kind==0 && image.isValid()) {
        const juce::Graphics::ScopedSaveState clip(g);
        // Use the selected acoustic horn position. Never bake a fixed centre
        // horn into a different array, or show hardware when HF is disabled.
        g.reduceClipRegion(baffle(model,box).reduced(.035f*scale).toNearestInt());
        const auto plate=juce::Rectangle<float>(.126f*scale,.087f*scale).withCentre(centre);
        g.setColour(juce::Colours::white.withAlpha(.8f));
        g.drawImage(image,plate,
            juce::RectanglePlacement::stretchToFit);
        g.setColour(juce::Colours::white);
        g.drawImage(bank->hornMouth,plate,juce::RectanglePlacement::stretchToFit);
    } else {
        const float radius=box.getWidth()*.024f;
        g.setColour(juce::Colour(0xffbcc0b9));g.drawEllipse(centre.x-radius,centre.y-radius,radius*2.f,radius*2.f,1.5f);
    }
}
// One shared depth order for the room, focus view and evidence renders.
// Both cone assemblies and HF hardware are behind the same continuous grille.
inline void frontHardware(juce::Graphics& g,const cabLayout::Geometry& model,juce::Rectangle<float> box,
                          bool bass,float level,int kind) {
    tweeter(g,model,box,bass,level,kind);
    grille(g,model,box,bass);
}
}
