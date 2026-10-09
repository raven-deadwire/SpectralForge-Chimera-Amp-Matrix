#pragma once
#include "CabEnclosureArtwork.h"

// Exercise the production tiling operation on both renderers. A software-only
// component snapshot cannot detect Direct2D wrapping a subsection's parent.
namespace cabEnclosureMaterialTests {
inline void run() {
    using namespace spectralforge::cabEnclosureArt;
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
        }
        const auto skin=makeSkin(source,bass);
        check(skin.isValid(),"material fixture extraction failed");
        check(skin.grille.getBounds()==juce::Rectangle<int>(36, bass ? 66 : 72)
            && skin.roof.getBounds()==juce::Rectangle<int>(128,20),"material crop dimensions changed");
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
    }
    std::cout<<"PASS enclosure materials: 24 native/software tiled renders, guitar/bass grille/roof, "
        <<"three scales, no parent pixels and independent cropped storage\n";
}
}
