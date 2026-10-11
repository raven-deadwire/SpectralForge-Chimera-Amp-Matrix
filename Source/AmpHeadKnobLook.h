#pragma once
#include "AmpCatalog.h"
#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace spectralforge::ampHeadKnob {
// A knob is an extruded object, not a flat UI dial. The cap sits above the
// mounting plane; its lower sidewall, occlusion and fixed studio reflection
// remain visible while the engraved position marker turns. Static materials
// are rendered once at the output resolution and shared by every head control.
enum class Material { bakelite, cream, nickel, brass, chrome };
inline Material styleForModel(int model) noexcept {
    switch (static_cast<AmpModel>(model)) {
        case AmpModel::britEdge: case AmpModel::chime30: return Material::brass;
        case AmpModel::matchChime: return Material::cream;
        case AmpModel::wideRect: return Material::chrome;
        case AmpModel::modernBass: case AmpModel::subwayClean:
        case AmpModel::zutaCinder: case AmpModel::ironCompact:
        case AmpModel::classicTube: case AmpModel::evilHarvest:
        case AmpModel::blueStorm: case AmpModel::specialEdition:
        case AmpModel::nastrond: case AmpModel::niflheimr: return Material::nickel;
        default: return Material::bakelite;
    }
}
namespace detail {
constexpr float capCentreY=.470f, rearCentreY=.550f, outerRadius=.395f, verticalRadius=.365f;
inline float clamp(float v) noexcept {return juce::jlimit(0.f,1.f,v);}
inline float smooth(float edge0,float edge1,float value) noexcept {
    const float t=clamp((value-edge0)/(edge1-edge0));return t*t*(3.f-2.f*t);
}
inline float directional(float nx,float ny,float nz) noexcept {
    const float length=std::sqrt(nx*nx+ny*ny+nz*nz);
    return juce::jmax(0.f,(-.48f*nx-.63f*ny+.61f*nz)/length);
}
inline juce::Colour shade(float r,float g,float b,float luminance,float alpha=1.f) noexcept {
    return juce::Colour::fromFloatRGBA(clamp(r*luminance),clamp(g*luminance),clamp(b*luminance),clamp(alpha));
}
inline juce::Image renderBody(Material material,int size) {
    juce::Image image(juce::Image::ARGB,size,size,true);
    const float dimension=float(size);
    {
        juce::Graphics g(image);
        // Broad light arrives from the upper left, as on the head casing.
        // A tight dark mounting contact and a softer lower-right cast shadow
        // are separate from the visible, ribbed cylinder sidewall.
        for(int layer=11;layer>=0;--layer) {
            const float spread=.007f*float(layer);
            g.setColour(juce::Colours::black.withAlpha(.025f+.009f*float(11-layer)/11.f));
            g.fillEllipse((.5f-outerRadius-spread+.035f)*dimension,
                (rearCentreY-verticalRadius-spread+.033f)*dimension,
                (2.f*outerRadius+2.f*spread)*dimension,
                (2.f*verticalRadius+2.f*spread)*dimension);
        }
        g.setColour(juce::Colours::black.withAlpha(.65f));
        g.fillEllipse((.5f-outerRadius+.006f)*dimension,(rearCentreY-verticalRadius+.014f)*dimension,
            outerRadius*2.f*dimension,verticalRadius*2.f*dimension);
    }
    juce::Image::BitmapData pixels(image,juce::Image::BitmapData::readWrite);
    const bool metal=material==Material::nickel||material==Material::brass||material==Material::chrome;
    const bool cream=material==Material::cream;
    const float fluteCount=cream?12.f:32.f;
    for(int y=0;y<size;++y)for(int x=0;x<size;++x) {
        const float px=(float(x)+.5f)/dimension,py=(float(y)+.5f)/dimension;
        const float nx=(px-.5f)/outerRadius,ny=(py-capCentreY)/verticalRadius;
        const float radius=std::sqrt(nx*nx+ny*ny),angle=std::atan2(ny,nx);
        const float flute=.982f+(cream?.018f:.013f)*std::cos(angle*fluteCount);
        const float rearY=(py-rearCentreY)/verticalRadius;
        const float rearRadius=std::sqrt(nx*nx+rearY*rearY);
        const float rearAngle=std::atan2(rearY,nx);
        const float rearFlute=.982f+(cream?.018f:.013f)*std::cos(rearAngle*fluteCount);
        juce::Colour colour;
        float coverage=0.f;
        if(radius<=flute) {
            // The outer ring has a rounded shoulder and recessed grip grooves.
            // Use its surface normal for lighting instead of rotating a gradient.
            const float shoulder=smooth(.80f,.988f,radius);
            const float light=directional(nx*shoulder,ny*shoulder,1.f-shoulder*.77f);
            const float grooves=(.5f+.5f*std::cos(angle*fluteCount))*smooth(.80f,.93f,radius);
            if(metal && radius<.795f) {
                const float capRadius=radius/.795f;
                // Spun metal reflects a broad fixed softbox in two unequal
                // wedges. Fine radial machining modulates that reflection;
                // it never follows the parameter rotation.
                const float reflection=std::pow(std::abs(std::cos(angle+.91f)),13.f);
                const float broad=std::pow(juce::jmax(0.f,std::cos(angle+2.32f)),2.f);
                const float machining=std::sin(capRadius*860.f)*.014f;
                const float grain=std::sin(float(x)*1.71f+float(y)*.39f)*.008f;
                float luminance=.29f+reflection*.70f+broad*.15f+machining+grain;
                luminance*=.94f+.06f*(1.f-capRadius);
                // Cap bevel: a bright upper-left lip, shaded lower edge and
                // a narrow black gasket separating metal from the grip body.
                if(capRadius>.93f) {
                    const float bevelLight=directional(nx*1.2f,ny*1.2f,.30f);
                    luminance=.16f+bevelLight*1.00f;
                }
                if(capRadius>.987f)luminance*=.30f;
                if(material==Material::brass)colour=shade(1.f,.77f,.43f,luminance);
                else if(material==Material::chrome)colour=shade(.96f,.98f,1.f,luminance*1.12f);
                else colour=shade(.94f,.94f,.89f,luminance);
            } else if(cream) {
                const float capLight=directional(nx*.40f,ny*.40f,1.f);
                const float gloss=std::pow(juce::jmax(0.f,1.f-std::hypot(nx+.35f,ny+.42f)),5.f);
                colour=shade(1.f,.89f,.68f,.38f+.42f*(radius<.79f?capLight:light)+.28f*gloss-.08f*grooves);
            } else {
                const float edgeHighlight=std::pow(juce::jmax(0.f,std::cos(angle+2.32f)),8.f)*shoulder;
                const float topSoftbox=std::exp(-((nx+.33f)*(nx+.33f)*2.5f+(ny+.42f)*(ny+.42f)*8.f));
                const float capLight=radius<.78f?directional(nx*.42f,ny*.42f,1.f):light;
                const bool blackCap=material==Material::bakelite;
                // A black cap needs a broad, soft specular window to reveal its
                // curvature against black painted steel. Keep the base dark;
                // lift only the upper-left reflection and rounded shoulder.
                const float sheen=blackCap?.220f:.060f;
                const float rim=blackCap?.32f:.22f;
                float luminance=.040f+.145f*capLight+rim*edgeHighlight+sheen*topSoftbox-.050f*grooves;
                if(radius>.78f && radius<.81f)luminance*=.43f;
                colour=shade(.93f,.96f,1.f,luminance);
            }
            coverage=clamp((flute-radius)*dimension*outerRadius);
        } else if(py>=capCentreY && rearRadius<=rearFlute) {
            // The front disk is raised by eight percent of the diameter. This
            // region is the actual exposed cylinder below it, not a drop shadow.
            const float sideAngle=std::asin(juce::jlimit(-1.f,1.f,nx));
            const float grooves=.5f+.5f*std::cos(sideAngle*fluteCount);
            const float bottom=smooth(.60f,.87f,py);
            float luminance=.09f+.22f*juce::jmax(0.f,-nx)+.065f*grooves;
            luminance*=1.f-.64f*bottom;
            colour=cream?shade(1.f,.88f,.63f,.30f+luminance):shade(.83f,.88f,.93f,luminance);
            coverage=clamp((rearFlute-rearRadius)*dimension*outerRadius);
        }
        if(coverage>0.f) {
            const auto previous=pixels.getPixelColour(x,y);
            pixels.setPixelColour(x,y,previous.overlaidWith(colour.withAlpha(coverage)));
        }
    }
    return image;
}
struct Cache {
    struct Entry {Material material;int size;juce::Image body;};
    std::vector<Entry> entries;
    size_t cachedPixels{};
    juce::Image body(Material material,int size) {
        for(auto& entry:entries)if(entry.material==material&&entry.size==size)return entry.body;
        // Quantised sizes and a bounded cache avoid unbounded memory growth if
        // the host continuously changes UI scale. Typical heads need 5 entries.
        const auto nextPixels=size_t(size)*size_t(size);
        constexpr size_t pixelBudget=8*1024*1024; // At most 32 MiB of ARGB bodies.
        while(!entries.empty() && (entries.size()>=96 || cachedPixels+nextPixels>pixelBudget)) {
            cachedPixels-=size_t(entries.front().size)*size_t(entries.front().size);
            entries.erase(entries.begin());
        }
        auto result=renderBody(material,size);entries.push_back({material,size,result});cachedPixels+=nextPixels;return result;
    }
};
}
// The last live head releases all JUCE images before plug-in unload. Never
// put native-backed images in a function-local static or DLL teardown storage.
class Renderer final {
    juce::SharedResourcePointer<detail::Cache> cache;
public:
void draw(juce::Graphics& g,juce::Rectangle<float> bounds,float value,float start,float end,int model,bool keyboardFocus) {
    if(bounds.getWidth()<1.f || bounds.getHeight()<1.f)return;
    bounds=bounds.withSizeKeepingCentre(juce::jmin(bounds.getWidth(),bounds.getHeight()),juce::jmin(bounds.getWidth(),bounds.getHeight()));
    const auto material=styleForModel(model);
    const auto size=bounds.getWidth();
    const auto c=juce::Point<float>{bounds.getCentreX(),bounds.getY()+detail::capCentreY*size};
    const bool brightFace=model==1||model==5||model==8||model==9||model==10||model==12||model==13||model==14||model==15||model==17||model==19||model==21;
    const auto scaleInk=juce::Colour(brightFace?0xff342f26:0xffd0c9b8).withAlpha(.68f);
    // Sparse engraved ticks belong to the panel plane, behind the projecting
    // body. They are not a glowing circular application control.
    g.setColour(scaleInk);
    for(int i=0;i<=10;++i) {
        const float angle=start+float(i)*(end-start)/10.f;
        const float inner=size*(i%5==0?.425f:.442f),outer=size*.467f;
        g.drawLine(c.x+std::sin(angle)*inner,c.y-std::cos(angle)*inner,
            c.x+std::sin(angle)*outer,c.y-std::cos(angle)*outer,juce::jmax(.65f,size*.015f));
    }
    // Two-times supersampling prevents shimmer at 75% UI scale. Account for
    // retina/host scaling in the cache key; no per-value bitmap is generated.
    const float outputScale=juce::jmax(1.f,g.getInternalContext().getPhysicalPixelScaleFactor());
    const int pixels=juce::jlimit(48,384,4*((juce::roundToInt(size*outputScale*2.f)+3)/4));
    g.drawImage(cache->body(material,pixels),bounds);
    const float angle=start+juce::jlimit(0.f,1.f,value)*(end-start);
    const auto from=c+juce::Point<float>(std::sin(angle),-std::cos(angle))*(size*.105f);
    const auto to=c+juce::Point<float>(std::sin(angle),-std::cos(angle))*(size*.258f);
    const float width=juce::jmax(1.15f,size*.035f);
    const bool lightMarker=material==Material::bakelite||material==Material::nickel||material==Material::chrome;
    // The shallow engraved channel and its inlay turn together. Its cast
    // micro-shadow uses the same fixed lower-right light direction as the body.
    g.setColour(juce::Colours::black.withAlpha(.60f));g.drawLine({from.translated(size*.007f,size*.008f),to.translated(size*.007f,size*.008f)},width+size*.018f);
    g.setColour(juce::Colour(lightMarker?0xff161b1c:0xfff5deb6).withAlpha(.85f));g.drawLine({from,to},width+size*.024f);
    g.setColour(juce::Colour(lightMarker?0xffeee9d9:0xff272117));g.drawLine({from,to},width);
    if(keyboardFocus) {
        g.setColour(juce::Colour(0xff94d4f0));g.drawEllipse(bounds.reduced(.6f),1.f);
    }
}
};
}
