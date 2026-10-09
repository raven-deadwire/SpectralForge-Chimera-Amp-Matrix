#pragma once
#include "CabLayoutModel.h"
#include "CabSpeakerArt.h"
#include "CabArtwork.h"

namespace spectralforge::cabLayoutView {
// A fixed camera for each enclosure arrangement. Fitting the selected box on
// every change would cancel its diameter and make the microphone appear to grow.
struct Projection { juce::Rectangle<float> box; float pixelsPerMetre{},headWidth{}; };
inline Projection project(cabLayout::Settings selected,juce::Rectangle<float> area) {
    double maximumWidth=0,maximumStack=0,minimumWidth=100;
    for(int family=0;family<2;++family)for(int driver=0;driver<=int(cabExpansion::drivers.size());++driver) {
        auto reference=selected;reference.voice.base.cabinet=family;reference.voice.driver=driver;
        const auto box=cabLayout::geometry(reference).box;
        maximumWidth=std::max(maximumWidth,box.width);
        minimumWidth=std::min(minimumWidth,box.width);
        maximumStack=std::max(maximumStack,box.height+box.width*.44);
    }
    const auto model=cabLayout::geometry(selected);
    const float scale=juce::jmax(0.f,juce::jmin(area.getWidth()*.70f/float(maximumWidth),
        area.getHeight()*.84f/float(maximumStack)));
    const float width=float(model.box.width)*scale,height=float(model.box.height)*scale;
    const float bottom=area.getBottom()-area.getHeight()*.07f;
    return {{area.getCentreX()-width*.5f,bottom-height-width*.07f,width,height+width*.07f},scale,float(minimumWidth)*scale*.89f};
}
inline juce::Rectangle<float> baffle(juce::Rectangle<float> box) {return box.withTrimmedTop(box.getWidth()*.07f);}
inline juce::Point<float> point(const cabLayout::Geometry& model,juce::Rectangle<float> box,originalCab::Point p) {
    const auto face=baffle(box);
    return {face.getCentreX()+float(p.x/model.box.width)*face.getWidth(),face.getCentreY()-float(p.y/model.box.height)*face.getHeight()};
}
inline float radius(const cabLayout::Geometry& model,juce::Rectangle<float> box) {return float(model.radius/model.box.width)*box.getWidth();}
struct GroundSupport {std::array<juce::Rectangle<float>,2> feet;float y{};};
inline GroundSupport groundSupport(juce::Rectangle<float> box) {
    const float height=juce::jlimit(2.5f,7.5f,box.getWidth()*.022f),width=box.getWidth()*.085f;
    GroundSupport result;result.y=box.getBottom()+height;
    for(int i=0;i<2;++i)result.feet[size_t(i)]={box.getX()+box.getWidth()*(i ? .785f : .13f),
        box.getBottom()-1.f,width,height+1.f};
    return result;
}
inline void enclosure(juce::Graphics& g,juce::Rectangle<float> box) {
    const auto face=baffle(box);const float inset=box.getWidth()*.025f;
    // The same contact plane anchors both the rubber feet and scene shadows.
    for(const auto foot:groundSupport(box).feet) {
        g.setColour(juce::Colour(0xff070a0b));g.fillRoundedRectangle(foot,1.4f);
        g.setColour(juce::Colour(0xff494d46));
        g.drawLine(foot.getX()+1.f,foot.getY()+1.5f,foot.getRight()-1.f,foot.getY()+1.5f,.8f);
    }
    juce::Path roof;roof.startNewSubPath(box.getX()+inset,box.getY());roof.lineTo(box.getRight()-inset,box.getY());
    roof.lineTo(face.getRight(),face.getY());roof.lineTo(face.getX(),face.getY());roof.closeSubPath();
    g.setGradientFill({juce::Colour(0xff454640),box.getTopLeft(),juce::Colour(0xff202522),face.getTopLeft(),false});g.fillPath(roof);
    g.setColour(juce::Colour(0xff090c0d));g.fillRoundedRectangle(face,5.f);
    const auto grille=face.reduced(inset);g.setColour(juce::Colour(0xff242927));g.fillRect(grille);
    {
    const juce::Graphics::ScopedSaveState saved(g);g.reduceClipRegion(grille.toNearestInt());
    g.setColour(juce::Colour(0xff51574d).withAlpha(.28f));
    for(float y=grille.getY();y<grille.getBottom();y+=3.f)g.drawHorizontalLine(int(y),grille.getX(),grille.getRight());
    g.setColour(juce::Colours::black.withAlpha(.28f));
    for(float x=grille.getX();x<grille.getRight();x+=3.f)g.drawVerticalLine(int(x),grille.getY(),grille.getBottom());
    }
    g.setColour(juce::Colour(0xff777769));g.drawRoundedRectangle(face.reduced(inset*.6f),3.f,1.f);
    g.setFont(juce::FontOptions(juce::jlimit(7.f,10.f,box.getWidth()*.029f),juce::Font::bold));g.setColour(juce::Colour(0xffc1b68d));
    g.drawText("CHIMERA",face.withTrimmedTop(face.getHeight()-15).toNearestInt(),juce::Justification::centred);
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
}
