#pragma once
#include <juce_graphics/juce_graphics.h>
#include "ChimeraArtworkData.h"
#include <array>

namespace spectralforge::art {
enum class Surface { workbench, pedalOxide, pedalAlloy, pedalCarbon, pedalBrass, ampBritish, ampModern, ampSilver, rackStudio, rackAlloy, cabEight, cabFour, cabTwo, emblem, count };
struct RasterBank {
    std::array<juce::Image,(size_t)Surface::count> images;
    RasterBank() {
        constexpr std::array<const char*,(size_t)Surface::count> names{"ravenworkbench_jpg","pedaloxide_jpg","pedalalloy_jpg","pedalcarbon_jpg","pedalbrass_jpg","ampbritish_jpg","ampmodern_jpg","ampsilver_jpg","rackstudio_jpg","rackalloy_jpg","cabeight_jpg","cabfour_jpg","cabtwo_jpg","ravenemblem_png"};
        for(size_t i=0;i<names.size();++i) {int size=0;const auto* bytes=ChimeraArtworkData::getNamedResource(names[i],size);if(bytes)images[i]=juce::ImageFileFormat::loadFrom(bytes,(size_t)size);}
    }
    static const RasterBank& get() {static const RasterBank bank;return bank;}
};
inline void raster(juce::Graphics& g,Surface surface,juce::Rectangle<float> destination,juce::Rectangle<float> viewport={0,0,1,1},bool preserveAspect=false) {
    const auto& original=RasterBank::get().images[(size_t)surface];
    if(!original.isValid()) return;
    const auto region=juce::Rectangle<int>(juce::roundToInt(viewport.getX()*original.getWidth()),juce::roundToInt(viewport.getY()*original.getHeight()),juce::roundToInt(viewport.getWidth()*original.getWidth()),juce::roundToInt(viewport.getHeight()*original.getHeight()));
    const auto part=original.getClippedImage(region);
    g.setColour(juce::Colours::white);
    g.drawImage(part,destination,preserveAspect ? juce::RectanglePlacement::centred : juce::RectanglePlacement::stretchToFit);
}
inline Surface pedalSurface(int family,int model) {
    if((family==0 && model==1) || (family==3 && model==2) || (family==5 && model==2)) return Surface::pedalBrass;
    if((family==0 && model==0) || (family==4 && model==0)) return Surface::pedalOxide;
    if((family==5 && model==0) || (family==6 && model==0) || (family==4 && model>0)) return Surface::pedalAlloy;
    return Surface::pedalCarbon;
}
inline void pedal(juce::Graphics& g,juce::Rectangle<float> r,int family,int model) {
    const auto surface=pedalSurface(family,model);
    const juce::Rectangle<float> viewport=surface==Surface::pedalBrass ? juce::Rectangle<float>{.08f,.073f,.84f,.84f} : surface==Surface::pedalOxide ? juce::Rectangle<float>{.028f,.045f,.944f,.91f} : juce::Rectangle<float>{.037f,.022f,.925f,.946f};
    raster(g,surface,r,viewport);
    // Keep the actual photographed metal visible; a restrained wash gives all
    // variants enough text contrast without baking fake controls into the image.
    g.setColour(juce::Colours::black.withAlpha(surface==Surface::pedalAlloy ? .55f : .22f));g.fillRoundedRectangle(r.reduced(5),9);
}
inline void head(juce::Graphics& g,juce::Rectangle<float> r,int model) {
    const auto surface=model==0 || model==5 ? Surface::ampSilver : model==1 ? Surface::ampBritish : Surface::ampModern;
    raster(g,surface,r);
}
inline void rack(juce::Graphics& g,juce::Rectangle<float> r,int family,int model) {
    const bool alloy=(family==7 && model==2) || (family==8 && model==1) || (family==9 && model==2) || (family==2 && model==2);
    raster(g,alloy ? Surface::rackAlloy : Surface::rackStudio,r,alloy ? juce::Rectangle<float>{.004f,.195f,.992f,.493f} : juce::Rectangle<float>{.012f,.233f,.976f,.47f});
    g.setColour(juce::Colours::black.withAlpha(alloy ? .55f : .22f));g.fillRect(r.reduced(3));
}
}
