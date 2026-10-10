#pragma once
#include "CabEnclosureArtwork.h"

// Exercise the production tiling operation on both renderers. A software-only
// component snapshot cannot detect Direct2D wrapping a subsection's parent.
namespace cabEnclosureMaterialTests {
inline void transmission() {
    using namespace spectralforge::cabEnclosureArt;
    for(bool bass:{false,true})for(bool native:{false,true}) {
        const auto type=native ? std::unique_ptr<juce::ImageType>(new juce::NativeImageType)
                               : std::unique_ptr<juce::ImageType>(new juce::SoftwareImageType);
        juce::Image source(juce::Image::ARGB,16,12,true,*type);
        {
            juce::Graphics g(source);g.fillAll(juce::Colours::black);
            g.setColour(juce::Colour(0xff888888));g.fillRect(0,0,8,12);
        }
        const auto overlay=makeGrilleOverlay(source,bass);
        const auto wire=overlay.getPixelAt(3,6);
        check(wire.getAlpha()>100 && wire.getAlpha()<255 && overlay.getPixelAt(12,6).getAlpha()==0,
            "foreground grille lost transmissive apertures or wire texture");
        {juce::Graphics g(source);g.fillAll(juce::Colours::red);}
        for(float scale:{.25f,1.f,1.75f}) {
            juce::Image target(juce::Image::ARGB,112,76,true,*type);
            {juce::Graphics g(target);fillMaterial(g,overlay,{4.f,4.f,104.f,68.f},scale);}
            int minimum=255,maximum=0;
            for(int y=8;y<68;++y)for(int x=8;x<104;++x) {
                const auto pixel=target.getPixelAt(x,y);
                minimum=juce::jmin(minimum,int(pixel.getAlpha()));maximum=juce::jmax(maximum,int(pixel.getAlpha()));
                check(std::abs(int(pixel.getRed())-int(pixel.getGreen()))<=2,
                    "foreground grille tiling leaked coloured parent pixels");
            }
            check(minimum<10 && maximum>100 && maximum<255 && target.getPixelAt(0,0).getAlpha()==0,
                "foreground grille is opaque, missing or outside its baffle");
        }
    }
}
inline void run() {
    using namespace spectralforge::cabEnclosureArt;
    transmission();
    for(bool bass:{false,true})for(bool native:{false,true}) {
        const auto type=native ? std::unique_ptr<juce::ImageType>(new juce::NativeImageType)
                               : std::unique_ptr<juce::ImageType>(new juce::SoftwareImageType);
        juce::Image source(juce::Image::ARGB,1254,1254,true,*type);
        const auto regions=sourceRegions(bass);
        {
            juce::Graphics g(source);
            g.fillAll(juce::Colours::red); // Forbidden parent pixels, outside the crops.
            g.setColour(juce::Colours::blue);g.fillRect(regions.grille);
            g.setColour(juce::Colours::green);g.fillRect(regions.roof);
            for(size_t i=1;i<regions.perimeter.size();i+=2) {
                g.setColour(juce::Colour(0xffd0a080));g.fillRect(regions.perimeter[i]);
            }
        }
        const auto skin=makeSkin(source,bass);
        check(skin.isValid(),"material fixture extraction failed");
        check(skin.grille.getBounds()==juce::Rectangle<int>(36, bass ? 66 : 72)
            && skin.roof.getBounds()==juce::Rectangle<int>(128,20),"material crop dimensions changed");
        for(size_t i=1;i<skin.perimeter.size();i+=2) {
            const auto& tile=skin.perimeter[i];
            check((i==topRail || i==bottomRail ? tile.getWidth() : tile.getHeight())==64,
                "rail material retained a whole stretched cabinet edge");
            for(float scale:{.25f,1.f,1.75f}) {
                juce::Image target(juce::Image::ARGB,192,128,true,*type);
                {juce::Graphics g(target);fillMaterial(g,tile,{4.f,4.f,184.f,120.f},scale);}
                for(int y=8;y<120;++y)for(int x=8;x<184;++x) {
                    const auto pixel=target.getPixelAt(x,y);
                    const bool matches=std::abs(int(pixel.getRed())-208)<=2
                        && std::abs(int(pixel.getGreen())-160)<=2
                        && std::abs(int(pixel.getBlue())-128)<=2 && pixel.getAlpha()>=253;
                    if(!matches)std::cerr<<"rail tile mismatch bass="<<int(bass)<<" native="<<int(native)
                        <<" piece="<<i<<" scale="<<scale<<" xy="<<x<<','<<y
                        <<" rgba="<<int(pixel.getRed())<<','<<int(pixel.getGreen())<<','
                        <<int(pixel.getBlue())<<','<<int(pixel.getAlpha())<<'\n';
                    check(matches,"rail tiling leaked the original cabinet parent bitmap");
                }
            }
        }
        for(bool roof:{false,true}) {
            const auto& tile=roof ? skin.roof : skin.grille;
            const auto expected=roof ? juce::Colours::green : juce::Colours::blue;
            check(tile.getPixelAt(tile.getWidth()/2,tile.getHeight()/2)==expected,
                "repeating material still aliases the parent cabinet artwork");
            for(float scale:{.25f,1.f,1.75f}) {
                juce::Image target(juce::Image::ARGB,192,128,true,*type);
                {
                    juce::Graphics g(target);
                    fillMaterial(g,tile,{4.f,4.f,184.f,120.f},scale);
                }
                // Inset from clipping/filter edges; several complete periods
                // ensure a full parent bitmap cannot masquerade as a tile.
                for(int y=8;y<120;++y)for(int x=8;x<184;++x) {
                    const auto pixel=target.getPixelAt(x,y);
                    check(std::abs(int(pixel.getRed())-int(expected.getRed()))<=2
                        && std::abs(int(pixel.getGreen())-int(expected.getGreen()))<=2
                        && std::abs(int(pixel.getBlue())-int(expected.getBlue()))<=2
                        && pixel.getAlpha()>=253,
                        "native/software material tiling leaked the parent cabinet or lost coverage");
                }
            }
        }
        // Copy ownership is also observable independently of a backend: edits
        // to the source must not inject artwork into either repeating tile.
        {
            juce::Graphics g(source);g.fillAll(juce::Colours::red);
        }
        check(skin.grille.getPixelAt(18,20)==juce::Colours::blue
            && skin.roof.getPixelAt(64,10)==juce::Colours::green,
            "repeating material still aliases the parent cabinet artwork");
        for(size_t i=1;i<skin.perimeter.size();i+=2)
            check(skin.perimeter[i].getPixelAt(10,10)==juce::Colour(0xffd0a080),
                "rail material still aliases the parent cabinet artwork");
    }
    std::cout<<"PASS enclosure materials: 72 native/software tiled renders, guitar/bass grille/roof/rails, "
        <<"three scales, no parent pixels and independent cropped storage\n";
}
}
