#pragma once
#include <juce_graphics/juce_graphics.h>
#include "ChimeraArtworkData.h"
#include <array>

namespace spectralforge::art {
// Every selectable hardware reference has an original material/trim render.
// Two Mu-Tron directions share their physical enclosure, not unrelated skins.
enum class Surface { workbench, emblem, p808, pcentaur, prat, pm87, pdyna, pdiamond, pqtron, pmutron, pmuff, pface, pbender, prc, prange, pmicro, aglass, abrit, a515, arect, amark, asvt, agk, ahybrid, rsslvca, r1176, rla2a, rn73, rv5, risa, rssleq, rn73eq, rpultec, rce2, rdimension, rstone, rmistress, reddy, rpulsar, r2290, rre201, rmemory, remt, rlex, rspring, cmesa, campeg, cbassman, cdelta, pfet, pmu, pbddi, pb3k, pm82, paw3, pwool, pfactory, pep, plpb, count, cabEight=campeg, cabFour=cmesa, cabTwo=cbassman };
// Keep values stable: the editor styles working JUCE knobs from this palette.
enum KnobStyle { black=0, cream=1, silver=2, gold=3, chrome=4, ssl=5, red=6, yellow=7, neve=8 };
struct ModelStyle { Surface surface; int knobStyle; bool brightFace; };
inline ModelStyle surfaceStyle(Surface surface) {
    static constexpr std::array<ModelStyle,static_cast<size_t>(Surface::count)> styles{{
        {Surface::workbench,black,false}, {Surface::emblem,black,false},
        {Surface::p808,black,false},
        {Surface::pcentaur,cream,true},
        {Surface::prat,black,false},
        {Surface::pm87,black,true},
        {Surface::pdyna,black,false},
        {Surface::pdiamond,cream,true},
        {Surface::pqtron,black,false},
        {Surface::pmutron,silver,false},
        {Surface::pmuff,black,true},
        {Surface::pface,black,false},
        {Surface::pbender,black,true},
        {Surface::prc,black,true},
        {Surface::prange,cream,false},
        {Surface::pmicro,black,true},
        {Surface::aglass,black,false},
        {Surface::abrit,gold,true},
        {Surface::a515,black,false},
        {Surface::arect,chrome,false},
        {Surface::amark,black,false},
        {Surface::asvt,black,true},
        {Surface::agk,black,false},
        {Surface::ahybrid,silver,false},
        {Surface::rsslvca,ssl,false},
        {Surface::r1176,black,false},
        {Surface::rla2a,black,true},
        {Surface::rn73,red,false},
        {Surface::rv5,silver,true},
        {Surface::risa,yellow,false},
        {Surface::rssleq,ssl,false},
        {Surface::rn73eq,neve,false},
        {Surface::rpultec,black,true},
        {Surface::rce2,black,true},
        {Surface::rdimension,silver,false},
        {Surface::rstone,black,true},
        {Surface::rmistress,black,true},
        {Surface::reddy,cream,false},
        {Surface::rpulsar,black,false},
        {Surface::r2290,silver,false},
        {Surface::rre201,silver,false},
        {Surface::rmemory,black,true},
        {Surface::remt,cream,true},
        {Surface::rlex,cream,false},
        {Surface::rspring,cream,false},
        {Surface::cmesa,black,false},
        {Surface::campeg,black,false},
        {Surface::cbassman,black,false},
        {Surface::cdelta,black,false},
        {Surface::pfet,silver,true},
        {Surface::pmu,cream,false},
        {Surface::pbddi,gold,false},
        {Surface::pb3k,silver,false},
        {Surface::pm82,black,false},
        {Surface::paw3,black,true},
        {Surface::pwool,black,true},
        {Surface::pfactory,black,true},
        {Surface::pep,black,false},
        {Surface::plpb,black,true}
    }};
    return styles[static_cast<size_t>(surface)];
}
struct RasterBank {
    std::array<juce::Image,static_cast<size_t>(Surface::count)> images;
    RasterBank() {
        constexpr std::array<const char*,static_cast<size_t>(Surface::count)> names{
            "ravenworkbench_jpg","ravenemblem_png","p808_jpg","pcentaur_jpg","prat_jpg","pm87_jpg","pdyna_jpg","pdiamond_jpg","pqtron_jpg","pmutron_jpg","pmuff_jpg","pface_jpg","pbender_jpg","prc_jpg","prange_jpg","pmicro_jpg","aglass_jpg","abrit_jpg","a515_jpg","arect_jpg","amark_jpg","asvt_jpg","agk_jpg","ahybrid_jpg","rsslvca_jpg","r1176_jpg","rla2a_jpg","rn73_jpg","rv5_jpg","risa_jpg","rssleq_jpg","rn73eq_jpg","rpultec_jpg","rce2_jpg","rdimension_jpg","rstone_jpg","rmistress_jpg","reddy_jpg","rpulsar_jpg","r2290_jpg","rre201_jpg","rmemory_jpg","remt_jpg","rlex_jpg","rspring_jpg","cmesa_jpg","campeg_jpg","cbassman_jpg","cdelta_jpg","pfet_jpg","pmu_jpg","pbddi_jpg","pb3k_jpg","pm82_jpg","paw3_jpg","pwool_jpg","pfactory_jpg","pep_jpg","plpb_jpg"
        };
        constexpr std::array<const char*,static_cast<size_t>(Surface::count)> alphaNames{
            nullptr,nullptr,"p808alpha_png","pcentauralpha_png","pratalpha_png","pm87alpha_png","pdynaalpha_png","pdiamondalpha_png","pqtronalpha_png","pmutronalpha_png","pmuffalpha_png","pfacealpha_png","pbenderalpha_png","prcalpha_png","prangealpha_png","pmicroalpha_png","aglassalpha_png","abritalpha_png","a515alpha_png","arectalpha_png","amarkalpha_png","asvtalpha_png","agkalpha_png","ahybridalpha_png","rsslvcaalpha_png","r1176alpha_png","rla2aalpha_png","rn73alpha_png","rv5alpha_png","risaalpha_png","rssleqalpha_png","rn73eqalpha_png","rpultecalpha_png","rce2alpha_png","rdimensionalpha_png","rstonealpha_png","rmistressalpha_png","reddyalpha_png","rpulsaralpha_png","r2290alpha_png","rre201alpha_png","rmemoryalpha_png","remtalpha_png","rlexalpha_png","rspringalpha_png","cmesaalpha_png","campegalpha_png","cbassmanalpha_png","cdeltaalpha_png","pfetalpha_png","pmualpha_png","pbddialpha_png","pb3kalpha_png","pm82alpha_png","paw3alpha_png","pwoolalpha_png","pfactoryalpha_png","pepalpha_png","plpbalpha_png"
        };
        for(size_t i=0;i<names.size();++i) {
            int size=0;
            const auto* bytes=ChimeraArtworkData::getNamedResource(names[i],size);
            if(bytes) images[i]=juce::ImageFileFormat::loadFrom(bytes,static_cast<size_t>(size));
            if(alphaNames[i] && images[i].isValid()) {
                const auto* alphaBytes=ChimeraArtworkData::getNamedResource(alphaNames[i],size);
                const auto alpha=alphaBytes ? juce::ImageFileFormat::loadFrom(alphaBytes,static_cast<size_t>(size)) : juce::Image{};
                jassert(alpha.isValid() && alpha.getBounds()==images[i].getBounds());
                if(!alpha.isValid() || alpha.getBounds()!=images[i].getBounds()) {images[i]={};continue;}
                // Separate lossless opacity keeps photographic gradients and
                // original antialiased cutouts without a large RGBA PNG payload.
                images[i]=images[i].convertedToFormat(juce::Image::ARGB);
                juce::Image::BitmapData colourPixels(images[i],juce::Image::BitmapData::readWrite);
                const juce::Image::BitmapData alphaPixels(alpha,juce::Image::BitmapData::readOnly);
                for(int y=0;y<colourPixels.height;++y)for(int x=0;x<colourPixels.width;++x)
                    colourPixels.setPixelColour(x,y,colourPixels.getPixelColour(x,y).withAlpha(alphaPixels.getPixelColour(x,y).getRed()));
            }
        }
    }
    static const RasterBank& get() {static const RasterBank bank;return bank;}
};
inline void raster(juce::Graphics& g,Surface surface,juce::Rectangle<float> destination,juce::Rectangle<float> viewport={0,0,1,1},bool preserveAspect=false) {
    const auto& original=RasterBank::get().images[static_cast<size_t>(surface)];
    if(!original.isValid()) return;
    const auto region=juce::Rectangle<int>(juce::roundToInt(viewport.getX()*original.getWidth()),juce::roundToInt(viewport.getY()*original.getHeight()),juce::roundToInt(viewport.getWidth()*original.getWidth()),juce::roundToInt(viewport.getHeight()*original.getHeight()));
    g.setColour(juce::Colours::white);
    g.drawImage(original.getClippedImage(region),destination,preserveAspect ? juce::RectanglePlacement::centred : juce::RectanglePlacement::stretchToFit);
}
inline ModelStyle modelStyle(int family,int model) {
    using S=Surface;
    static constexpr std::array<int,11> counts{5,3,3,5,5,5,5,3,3,3,6};
    static constexpr std::array<std::array<Surface,6>,11> surfaces{{
        {{S::p808,S::pcentaur,S::prat,S::pbddi,S::pb3k,S::p808}},
        {{S::r2290,S::rre201,S::rmemory,S::r2290,S::r2290,S::r2290}},
        {{S::remt,S::rlex,S::rspring,S::remt,S::remt,S::remt}},
        {{S::pm87,S::pdyna,S::pdiamond,S::pfet,S::pmu,S::pm87}},
        {{S::pqtron,S::pmutron,S::pmutron,S::pm82,S::paw3,S::pqtron}},
        {{S::pmuff,S::pface,S::pbender,S::pwool,S::pfactory,S::pmuff}},
        {{S::prc,S::prange,S::pmicro,S::pep,S::plpb,S::prc}},
        {{S::rsslvca,S::r1176,S::rla2a,S::rsslvca,S::rsslvca,S::rsslvca}},
        {{S::rn73,S::rv5,S::risa,S::rn73,S::rn73,S::rn73}},
        {{S::rssleq,S::rn73eq,S::rpultec,S::rssleq,S::rssleq,S::rssleq}},
        {{S::rce2,S::rdimension,S::rstone,S::rmistress,S::reddy,S::rpulsar}}
    }};
    const auto f=static_cast<size_t>(juce::jlimit(0,10,family));
    return surfaceStyle(surfaces[f][static_cast<size_t>(juce::jlimit(0,counts[f]-1,model))]);
}
inline ModelStyle pedalStyle(int family,int model) {return modelStyle(family,model);}
inline ModelStyle rackStyle(int family,int model) {return modelStyle(family,model);}
inline ModelStyle headStyle(int model) {
    constexpr std::array<Surface,8> heads{Surface::aglass,Surface::abrit,Surface::a515,Surface::arect,Surface::amark,Surface::asvt,Surface::agk,Surface::ahybrid};
    return surfaceStyle(heads[static_cast<size_t>(juce::jlimit(0,7,model))]);
}
inline Surface pedalSurface(int family,int model) {return pedalStyle(family,model).surface;}
inline void pedal(juce::Graphics& g,juce::Rectangle<float> r,int family,int model) {
    raster(g,pedalStyle(family,model).surface,r);
}
inline void head(juce::Graphics& g,juce::Rectangle<float> r,int model) {
    raster(g,headStyle(model).surface,r,{0,0,1,1},true);
}
inline void rack(juce::Graphics& g,juce::Rectangle<float> r,int family,int model) {
    raster(g,rackStyle(family,model).surface,r);
}
}
