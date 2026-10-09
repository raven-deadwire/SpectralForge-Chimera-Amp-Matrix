#pragma once
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <cmath>

namespace spectralforge::cabEnclosureArt {
// The original cabinet illustrations remain the source of the shell material.
// Only inspected, speaker-free rectangles are used. The four baked cones
// are never rendered; the bass horn plate is a separate acoustic-positioned crop.
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
    std::array<juce::Image,2> feet;
    juce::Image grille,roof,grilleOverlay,header,horn;
    float sourcePixelsPerMetre{};
    bool bass{};
    bool isValid() const noexcept {
        if(!grille.isValid() || !roof.isValid() || !grilleOverlay.isValid() || !header.isValid() || sourcePixelsPerMetre<=0.f)return false;
        for(const auto& image:perimeter)if(!image.isValid())return false;
        return true;
    }
    const char* key() const noexcept {return bass ? "cab-bass-410-shell" : "cab-guitar-412-shell";}
};

// Retain the original woven/metal material as a transmissive layer in front
// of the drivers. Only the inspected empty grille patch is sampled; no baked
// cones, horn, cabinet outline or alpha-subsection backing bitmap can enter it.
inline juce::Image makeGrilleOverlay(const juce::Image& tile,bool bass) {
    if(!tile.isValid())return {};
    juce::Image result(juce::Image::ARGB,tile.getWidth(),tile.getHeight(),true);
    const juce::Image::BitmapData source(tile,juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData target(result,juce::Image::BitmapData::writeOnly);
    const auto luminance=[](juce::Colour c) {return .2126f*c.getFloatRed()+.7152f*c.getFloatGreen()+.0722f*c.getFloatBlue();};
    float low=1.f,high=0.f;
    for(int y=0;y<source.height;++y)for(int x=0;x<source.width;++x) {
        const float value=luminance(source.getPixelColour(x,y));
        low=juce::jmin(low,value);high=juce::jmax(high,value);
    }
    if(high-low<.01f)return result;
    for(int y=0;y<source.height;++y)for(int x=0;x<source.width;++x) {
        const auto colour=source.getPixelColour(x,y);
        const float wire=juce::jlimit(0.f,1.f,(luminance(colour)-low)/(high-low));
        const float opacity=std::pow(wire,bass ? .75f : 1.1f)*(bass ? .88f : .67f);
        target.setPixelColour(x,y,colour.withAlpha(opacity));
    }
    return result;
}

// Called once while CabArtwork::Bank is constructed on the message thread.
// Perimeter slices share the decoded source storage. Repeating materials own
// their cropped pixels: JUCE 8.0.8's Direct2D bitmap brush wraps the underlying
// bitmap, ignoring a subsection's area/offset. Sharing that bitmap would repeat
// the entire cabinet (including baked cones) across the new baffle/roof.
// The small repeating copies are made once, never during paint or processing.
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
    for(size_t i=0;i<skin.perimeter.size();++i) {
        auto area=regions.perimeter[i];
        // A long rail is a material, not a picture to squash into a different
        // cabinet height. Keep a short speaker-free grain period at its centre.
        if(i%2) {
            if(i==topRail || i==bottomRail)area=area.withSizeKeepingCentre(64,area.getHeight());
            else area=area.withSizeKeepingCentre(area.getWidth(),64);
        }
        skin.perimeter[i]=clip(area);
        if(i%2)skin.perimeter[i]=skin.perimeter[i].createCopy();
    }
    skin.feet[0]=clip(bass ? juce::Rectangle<int>(113,1147,65,36) : juce::Rectangle<int>(102,1172,66,26));
    skin.feet[1]=clip(bass ? juce::Rectangle<int>(1087,1147,63,36) : juce::Rectangle<int>(1090,1172,67,26));
    skin.grille=clip(regions.grille).createCopy();
    skin.roof=clip(regions.roof).createCopy();
    // Preserve the complete photographed roof, rolled front edge and metal
    // cap returns. A tiny grain tile cannot reproduce their original form.
    // These strips end before any of the baked cones or bass horn begins.
    skin.header=clip(bass ? juce::Rectangle<int>(63,61,1127,87)
                          : juce::Rectangle<int>(44,48,1166,110));
    if(bass)skin.horn=clip({507,536,236,156});
    skin.grilleOverlay=makeGrilleOverlay(skin.grille,bass);
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

inline float upperInset(juce::Rectangle<float> face,float pixelsPerMetre,bool slant) {
    return slant ? juce::jmin(pixelsPerMetre*.026f,face.getWidth()*.06f) : 0.f;
}
inline juce::Path frontOutline(juce::Rectangle<float> face,float inset,float radius) {
    juce::Path p;
    if(inset<=0.f) {p.addRoundedRectangle(face,radius);return p;}
    const float x=face.getX(),r=face.getRight(),y=face.getY(),b=face.getBottom(),bend=y+face.getHeight()*.46f;
    p.startNewSubPath(x+inset+radius,y);p.lineTo(r-inset-radius,y);
    p.quadraticTo(r-inset,y,r-inset,y+radius);
    p.lineTo(r-inset*.08f,bend-radius);p.quadraticTo(r,bend,r,bend+radius);
    p.lineTo(r,b-radius);p.quadraticTo(r,b,r-radius,b);
    p.lineTo(x+radius,b);p.quadraticTo(x,b,x,b-radius);
    p.lineTo(x,bend+radius);p.quadraticTo(x,bend,x+inset*.08f,bend-radius);
    p.lineTo(x+inset,y+radius);p.quadraticTo(x+inset,y,x+inset+radius,y);
    p.closeSubPath();return p;
}

inline void paintFrame(juce::Graphics& g,const Skin& skin,juce::Rectangle<float> face,float pixelsPerMetre,bool slant=false) {
    if(!skin.isValid() || face.isEmpty() || pixelsPerMetre<=0.f)return;
    const juce::Graphics::ScopedSaveState saved(g);
    const float corner=juce::jmin(pixelsPerMetre*(skin.bass ? .048f : .044f),
        juce::jmin(face.getWidth(),face.getHeight())*.18f);
    const float rail=juce::jmin(pixelsPerMetre*(skin.bass ? .035f : .038f),corner);
    const float x=face.getX(),y=face.getY(),r=face.getRight(),b=face.getBottom();
    const float inset=upperInset(face,pixelsPerMetre,slant);
    const std::array<juce::Rectangle<float>,pieceCount> destinations{{
        {x+inset,y,corner,corner}, {x+inset+corner,y,face.getWidth()-(corner+inset)*2.f,rail},
        {r-inset-corner,y,corner,corner}, {r-rail,y+corner,rail,face.getHeight()-corner*2.f},
        {r-corner,b-corner,corner,corner}, {x+corner,b-rail,face.getWidth()-corner*2.f,rail},
        {x,b-corner,corner,corner}, {x,y+corner,rail,face.getHeight()-corner*2.f}
    }};
    const float grainScale=pixelsPerMetre/skin.sourcePixelsPerMetre;
    // Tile leather at one physical material scale along each rail. Preserve
    // the original edge bevel across its width and the original metal caps.
    for(size_t i=1;i<destinations.size();i+=2) {
        const auto area=destinations[i];const auto& image=skin.perimeter[i];
        const bool horizontal=i==topRail || i==bottomRail;
        if(slant && !horizontal) {
            const float bend=y+face.getHeight()*.46f,top=y+corner;
            const float start=i==leftRail ? x+inset : r-rail-inset;
            const float end=i==leftRail ? x : r-rail;
            {
                const juce::Graphics::ScopedSaveState upper(g);
                g.addTransform(juce::AffineTransform(1.f,(end-start)/(bend-top),start,0.f,1.f,top));
                g.setFillType(juce::FillType(image,juce::AffineTransform::scale(rail/float(image.getWidth()),grainScale)));
                g.fillRect(0.f,0.f,rail,bend-top);
            }
            g.setFillType(juce::FillType(image,juce::AffineTransform::scale(rail/float(image.getWidth()),grainScale).translated(end,bend)));
            g.fillRect(end,bend,rail,b-corner-bend);
            continue;
        }
        const auto transform=juce::AffineTransform::scale(
            horizontal ? grainScale : area.getWidth()/float(image.getWidth()),
            horizontal ? area.getHeight()/float(image.getHeight()) : grainScale)
            .translated(area.getX(),area.getY());
        g.setFillType(juce::FillType(image,transform));g.fillRect(area);
    }
    g.setColour(juce::Colours::white);
    for(size_t i=0;i<destinations.size();i+=2)
        g.drawImage(skin.perimeter[i],destinations[i],juce::RectanglePlacement::stretchToFit);
    // The source rails already contain the rounded leather return and piping.
    // Do not replace them with an extra synthetic box outline.
}

inline void paintGrille(juce::Graphics& g,const Skin& skin,juce::Rectangle<float> face,float pixelsPerMetre,bool slant=false) {
    if(!skin.isValid() || face.isEmpty() || pixelsPerMetre<=0.f)return;
    const float corner=juce::jmin(pixelsPerMetre*(skin.bass ? .048f : .044f),
        juce::jmin(face.getWidth(),face.getHeight())*.18f);
    const float rail=juce::jmin(pixelsPerMetre*(skin.bass ? .035f : .038f),corner);
    {
        const juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(frontOutline(face,upperInset(face,pixelsPerMetre,slant),corner*.28f));
        fillMaterial(g,skin.grilleOverlay,face.reduced(rail*.82f),pixelsPerMetre/skin.sourcePixelsPerMetre);
    }
    // The grille and frame are in front of the mounted units. Repaint the
    // flange here so large valid drivers cannot erase the cabinet's hardware.
    paintFrame(g,skin,face,pixelsPerMetre,slant);
}

// The caller owns all camera geometry. Decorative widths also use its common
// pixels-per-metre scale, so a smaller cabinet does not acquire larger corners
// or a differently magnified texture. Speaker count and diameter are irrelevant.
inline void paint(juce::Graphics& g,const Skin& skin,juce::Rectangle<float> box,
                  juce::Rectangle<float> face,float pixelsPerMetre,bool slant=false) {
    if(box.isEmpty() || face.isEmpty() || pixelsPerMetre<=0.f)return;
    const juce::Graphics::ScopedSaveState saved(g);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    const float corner=juce::jmin(pixelsPerMetre*(skin.bass ? .048f : .044f),
        juce::jmin(face.getWidth(),face.getHeight())*.18f);
    const float rail=juce::jmin(pixelsPerMetre*(skin.bass ? .035f : .038f),corner);
    const float depth=juce::jmax(0.f,face.getY()-box.getY());
    const float inset=upperInset(face,pixelsPerMetre,slant);
    const auto outline=frontOutline(face,inset,corner*.28f);
    g.setColour(juce::Colour(0xff0b0d0e));g.fillPath(outline);
    const auto grille=face.reduced(rail*.82f);
    if(skin.isValid()) {
        const juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(outline);
        fillMaterial(g,skin.grille,grille,pixelsPerMetre/skin.sourcePixelsPerMetre);
        if(slant) {
            // A soft change in the upper baffle's room-light angle makes the
            // slant legible without inventing a seam across continuous cloth.
            const auto upper=face.withHeight(face.getHeight()*.46f);
            g.setGradientFill({juce::Colours::white.withAlpha(.055f),upper.getTopLeft(),
                juce::Colours::black.withAlpha(.12f),upper.getBottomLeft(),false});g.fillRect(upper);
        }
    } else {
        g.setColour(juce::Colour(skin.bass ? 0xff242a2c : 0xff242320));g.fillRect(grille);
    }
    // Subtle recess shading belongs to the shell, not to a baked speaker face.
    g.setGradientFill({juce::Colours::black.withAlpha(.35f),grille.getTopLeft(),
        juce::Colours::transparentBlack,grille.getTopLeft().translated(0.f,rail),false});
    g.fillRect(grille.withHeight(rail));

    if(skin.header.isValid()) {
        g.setColour(juce::Colours::white);
        g.drawImage(skin.header,{box.getX()+inset,box.getY(),box.getWidth()-inset*2.f,depth+rail},
            juce::RectanglePlacement::stretchToFit);
    }
    paintFrame(g,skin,face,pixelsPerMetre,slant);
}
}
