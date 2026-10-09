#pragma once
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <cmath>

namespace spectralforge::cabEnclosureArt {
// The original cabinet illustrations remain the source of the shell material.
// Only these inspected, speaker-free rectangles are used. The source centre
// (including its four cones and, for bass, its fixed horn) is never rendered.
// Coordinates refer to the unchanged 1254 x 1254 PNGs in Assets/Artwork/Cab.
enum Piece { topLeft, topRail, topRight, rightRail, bottomRight, bottomRail,
             bottomLeft, leftRail, pieceCount };
struct SourceRegions {
    std::array<juce::Rectangle<int>,pieceCount> perimeter;
    juce::Rectangle<int> grille,roof;
    float referenceWidthMetres;
    int referenceFrontWidth;
};
inline SourceRegions sourceRegions(bool bass) {
    if(bass)return {{{
        {63,98,85,84}, {148,98,960,50}, {1108,98,82,84},
        {1127,182,63,876}, {1108,1058,82,90}, {148,1085,960,63},
        {63,1058,85,90}, {63,182,63,876}
    }},{610,290,36,66},{704,74,128,20},.62f,1127};
    return {{{
        {44,96,70,64}, {114,98,1026,60}, {1140,96,70,64},
        {1141,160,69,952}, {1140,1112,70,66}, {114,1122,1026,56},
        {44,1112,70,66}, {46,160,68,952}
    }},{607,250,36,72},{768,70,128,20},.74f,1166};
}

struct Skin {
    std::array<juce::Image,pieceCount> perimeter;
    juce::Image grille,roof;
    float sourcePixelsPerMetre{};
    bool bass{};
    bool isValid() const noexcept {
        if(!grille.isValid() || !roof.isValid() || sourcePixelsPerMetre<=0.f)return false;
        for(const auto& image:perimeter)if(!image.isValid())return false;
        return true;
    }
    const char* key() const noexcept {return bass ? "cab-bass-410-shell" : "cab-guitar-412-shell";}
};

// Called once while CabArtwork::Bank is constructed on the message thread.
// Perimeter slices share the decoded source storage. Repeating materials own
// their cropped pixels: JUCE 8.0.8's Direct2D bitmap brush wraps the underlying
// bitmap, ignoring a subsection's area/offset. Sharing that bitmap would repeat
// the entire cabinet (including baked cones) across the new baffle/roof.
// These two small copies are made once, never during paint or audio processing.
inline Skin makeSkin(const juce::Image& source,bool bass) {
    Skin skin;skin.bass=bass;
    if(!source.isValid())return skin;
    const auto regions=sourceRegions(bass);
    const auto clip=[&](juce::Rectangle<int> area) {
        const double sx=double(source.getWidth())/1254.0,sy=double(source.getHeight())/1254.0;
        const int left=int(std::ceil(area.getX()*sx)),top=int(std::ceil(area.getY()*sy));
        const int right=int(std::floor(area.getRight()*sx)),bottom=int(std::floor(area.getBottom()*sy));
        return source.getClippedImage(juce::Rectangle<int>(left,top,right-left,bottom-top));
    };
    for(size_t i=0;i<skin.perimeter.size();++i)skin.perimeter[i]=clip(regions.perimeter[i]);
    skin.grille=clip(regions.grille).createCopy();
    skin.roof=clip(regions.roof).createCopy();
    skin.sourcePixelsPerMetre=float(regions.referenceFrontWidth)*float(source.getWidth())
        /(1254.f*regions.referenceWidthMetres);
    return skin;
}

inline void fillMaterial(juce::Graphics& g,const juce::Image& image,
                         juce::Rectangle<float> area,float sourceScale) {
    const juce::Graphics::ScopedSaveState saved(g);
    const juce::FillType texture(image,juce::AffineTransform::scale(sourceScale)
        .translated(area.getX(),area.getY()));
    g.setFillType(texture);g.fillRect(area);
}

// The caller owns all camera geometry. Decorative widths also use its common
// pixels-per-metre scale, so a smaller cabinet does not acquire larger corners
// or a differently magnified texture. Speaker count and diameter are irrelevant.
inline void paint(juce::Graphics& g,const Skin& skin,juce::Rectangle<float> box,
                  juce::Rectangle<float> face,float pixelsPerMetre) {
    if(box.isEmpty() || face.isEmpty() || pixelsPerMetre<=0.f)return;
    const juce::Graphics::ScopedSaveState saved(g);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    const float corner=juce::jmin(pixelsPerMetre*(skin.bass ? .048f : .044f),
        juce::jmin(face.getWidth(),face.getHeight())*.18f);
    const float rail=juce::jmin(pixelsPerMetre*(skin.bass ? .035f : .038f),corner);
    const float depth=juce::jmax(0.f,face.getY()-box.getY());
    const float backInset=juce::jmin(pixelsPerMetre*.018f,depth*.5f);
    juce::Path roof;roof.startNewSubPath(box.getX()+backInset,box.getY());
    roof.lineTo(box.getRight()-backInset,box.getY());
    roof.lineTo(face.getRight(),face.getY()+rail*.35f);
    roof.lineTo(face.getX(),face.getY()+rail*.35f);roof.closeSubPath();
    g.setColour(juce::Colour(0xff242627));g.fillPath(roof);
    if(skin.isValid()) {
        const juce::Graphics::ScopedSaveState clipped(g);g.reduceClipRegion(roof);
        fillMaterial(g,skin.roof,box.withHeight(depth+rail),pixelsPerMetre/skin.sourcePixelsPerMetre);
    }
    g.setGradientFill({juce::Colours::white.withAlpha(.10f),box.getTopLeft(),
        juce::Colours::black.withAlpha(.16f),face.getTopLeft(),false});g.fillPath(roof);
    g.setColour(juce::Colour(0xff858788).withAlpha(.42f));
    g.drawLine(box.getX()+backInset,box.getY(),box.getRight()-backInset,box.getY(),.7f);

    g.setColour(juce::Colour(0xff0b0d0e));g.fillRoundedRectangle(face,corner*.28f);
    const auto grille=face.reduced(rail*.82f);
    if(skin.isValid())
        fillMaterial(g,skin.grille,grille,pixelsPerMetre/skin.sourcePixelsPerMetre);
    else {
        g.setColour(juce::Colour(skin.bass ? 0xff242a2c : 0xff242320));g.fillRect(grille);
    }
    // Subtle recess shading belongs to the shell, not to a baked speaker face.
    g.setGradientFill({juce::Colours::black.withAlpha(.35f),grille.getTopLeft(),
        juce::Colours::transparentBlack,grille.getTopLeft().translated(0.f,rail),false});
    g.fillRect(grille.withHeight(rail));

    if(skin.isValid()) {
        const float x=face.getX(),y=face.getY(),r=face.getRight(),b=face.getBottom();
        const std::array<juce::Rectangle<float>,pieceCount> destinations{{
            {x,y,corner,corner}, {x+corner,y,face.getWidth()-corner*2.f,rail},
            {r-corner,y,corner,corner}, {r-rail,y+corner,rail,face.getHeight()-corner*2.f},
            {r-corner,b-corner,corner,corner}, {x+corner,b-rail,face.getWidth()-corner*2.f,rail},
            {x,b-corner,corner,corner}, {x,y+corner,rail,face.getHeight()-corner*2.f}
        }};
        g.setColour(juce::Colours::white);
        // Perimeter pieces have their own dimensions. The fixed 4-speaker
        // centre is absent; stretching the cabinet illustration is impossible.
        for(size_t i=0;i<destinations.size();++i)
            g.drawImage(skin.perimeter[i],destinations[i],juce::RectanglePlacement::stretchToFit);
    }
    // Restore a continuous recessed piping line where adjoining original
    // slices meet. The two material families retain their own metal finish.
    g.setColour(juce::Colour(skin.bass ? 0xff878b8c : 0xffab9671).withAlpha(.74f));
    g.drawRoundedRectangle(face.reduced(rail*.93f),rail*.13f,juce::jmax(.55f,pixelsPerMetre*.0012f));
    const float plateWidth=juce::jmin(face.getWidth()*.25f,pixelsPerMetre*.105f);
    const float plateHeight=juce::jmin(rail*.72f,pixelsPerMetre*.019f);
    const juce::Rectangle<float> plate(face.getCentreX()-plateWidth*.5f,
        face.getBottom()-rail*.5f-plateHeight*.5f,plateWidth,plateHeight);
    if(plate.getWidth()>12.f && plate.getHeight()>3.f) {
        g.setColour(juce::Colour(0xff101212));g.fillRoundedRectangle(plate,plateHeight*.12f);
        g.setColour(juce::Colour(skin.bass ? 0xffb3b7b4 : 0xffc1ae81));
        g.setFont(juce::FontOptions(juce::jmax(3.f,plateHeight*.79f),juce::Font::bold));
        g.drawText("CHIMERA",plate,juce::Justification::centred);
    }
}
}
