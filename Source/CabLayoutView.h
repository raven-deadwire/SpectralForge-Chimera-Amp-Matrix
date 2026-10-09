#pragma once
#include "CabLayoutModel.h"
#include "CabArtwork.h"
namespace spectralforge::cabLayoutView {
inline juce::Rectangle<float> baffle(juce::Rectangle<float> box) {return box.withTrimmedTop(box.getWidth()*.07f);}
inline juce::Point<float> point(const cabLayout::Geometry& model,juce::Rectangle<float> box,originalCab::Point p) {
    const auto face=baffle(box);
    return {face.getCentreX()+float(p.x/model.box.width)*face.getWidth(),face.getCentreY()-float(p.y/model.box.height)*face.getHeight()};
}
inline float radius(const cabLayout::Geometry& model,juce::Rectangle<float> box) {return float(model.radius/model.box.width)*box.getWidth();}
inline void enclosure(juce::Graphics& g,juce::Rectangle<float> box) {
    const auto face=baffle(box);const float inset=box.getWidth()*.025f;
    juce::Path roof;roof.startNewSubPath(box.getX()+inset,box.getY());roof.lineTo(box.getRight()-inset,box.getY());
    roof.lineTo(face.getRight(),face.getY());roof.lineTo(face.getX(),face.getY());roof.closeSubPath();
    g.setColour(juce::Colour(0xff3b3c38));g.fillPath(roof);
    g.setColour(juce::Colour(0xff0b0d0e));g.fillRoundedRectangle(face,5.f);
    const auto grille=face.reduced(inset);g.setColour(juce::Colour(0xff252927));g.fillRect(grille);
    g.setColour(juce::Colour(0xff353a36));
    for(float y=grille.getY();y<grille.getBottom();y+=4.f)g.drawHorizontalLine(int(y),grille.getX(),grille.getRight());
    g.setColour(juce::Colour(0xff66665a));g.drawRoundedRectangle(face.reduced(1.f),4.f,1.5f);
    g.setFont(juce::FontOptions(9.f,juce::Font::bold));g.setColour(juce::Colour(0xffc1b68d));
    g.drawText("CHIMERA",face.withTrimmedTop(face.getHeight()-16).toNearestInt(),juce::Justification::centred);
}
inline void speakers(juce::Graphics& g,const cabLayout::Geometry& model,juce::Rectangle<float> box,bool bass,const cabArt::Bank& bank) {
    const auto index=size_t(cabArt::speaker(int(bass)));const auto& image=bank.images[index];
    if(!image.isValid())return;const auto trimmed=image.getClippedImage(bank.contentBounds[index]);
    const float diameter=radius(model,box)*2.f/.90f;g.setColour(juce::Colours::white);
    for(int n=0;n<model.count;++n)g.drawImage(trimmed,juce::Rectangle<float>(diameter,diameter).withCentre(point(model,box,model.centres[size_t(n)])),juce::RectanglePlacement::centred);
}
}
