#pragma once
#include "AmpCatalog.h"
#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <string_view>

namespace spectralforge::ampHeadKnob {
// A knob is an extruded object above the mounting plane. Circular materials
// retain fixed studio reflections while their engraved index turns; shaped
// grips turn above a separately lit round boss. Static materials are rendered
// once at the output resolution and shared by every head control.
enum class Material { bakelite, cream, nickel, brass, chrome };
enum class Shape { skirt, bell, cylinder, fluted, chickenhead, pointer, bar, narrowBar };
// A profile identifies the complete geometry/material combination. Keeping it
// in the cache key prevents a metal cap from borrowing a different model's grip.
enum class Profile {
    americanSkirt, britishGold, blackPointer, chromeCylinder, blackSkirt,
    silverBell, compactSilver, blackSoftTouch, blackBar, blackNarrowBar, creamChickenhead, blackChickenhead,
    blackChickenheadLine, roundDot, silverDisc, silverScalloped, blackSoftLobe, blackFaceted, blackScalloped, chromeDisc,
    originalNickel
};
struct ProfileSpec {
    Shape shape;
    Material material;
    float capRadius;  // relative to the .395 slot-radius mounting footprint
    float flutes, fluteDepth;
    float markerFrom, markerTo;
    bool lightMarker;
};
inline ProfileSpec profileSpec(Profile profile) noexcept {
    switch(profile) {
        case Profile::americanSkirt: return {Shape::skirt,Material::chrome,.44f,36.f,.013f,.238f,.359f,true};
        case Profile::britishGold: return {Shape::bell,Material::brass,.78f,28.f,.014f,.095f,.275f,false};
        case Profile::blackPointer: return {Shape::pointer,Material::bakelite,0.f,0.f,0.f,.085f,.315f,true};
        case Profile::chromeCylinder: return {Shape::cylinder,Material::chrome,.92f,44.f,.010f,.31f,.31f,false};
        case Profile::blackSkirt: return {Shape::skirt,Material::bakelite,0.f,32.f,.015f,.227f,.361f,true};
        case Profile::silverBell: return {Shape::bell,Material::nickel,.73f,20.f,.024f,.055f,.265f,false};
        case Profile::compactSilver: return {Shape::cylinder,Material::nickel,.59f,28.f,.009f,.24f,.34f,true};
        case Profile::blackSoftTouch: return {Shape::cylinder,Material::bakelite,0.f,32.f,.008f,.13f,.325f,true};
        case Profile::blackBar: return {Shape::bar,Material::bakelite,0.f,0.f,0.f,.20f,.34f,true};
        case Profile::blackNarrowBar: return {Shape::narrowBar,Material::bakelite,0.f,0.f,0.f,.07f,.335f,true};
        case Profile::creamChickenhead: return {Shape::chickenhead,Material::cream,0.f,0.f,0.f,.13f,.32f,false};
        case Profile::blackChickenhead: return {Shape::chickenhead,Material::bakelite,0.f,0.f,0.f,.13f,.32f,true};
        case Profile::blackChickenheadLine: return {Shape::chickenhead,Material::bakelite,0.f,0.f,0.f,.13f,.32f,true};
        case Profile::roundDot: return {Shape::cylinder,Material::bakelite,0.f,20.f,.024f,.305f,.305f,true};
        case Profile::silverDisc: return {Shape::cylinder,Material::nickel,.94f,28.f,.009f,.05f,.329f,false};
        case Profile::silverScalloped: return {Shape::fluted,Material::nickel,.55f,10.f,.060f,.238f,.35f,true};
        case Profile::blackSoftLobe: return {Shape::fluted,Material::bakelite,0.f,6.f,.04f,.12f,.33f,true};
        case Profile::blackFaceted: return {Shape::fluted,Material::bakelite,0.f,8.f,.052f,.09f,.335f,true};
        case Profile::blackScalloped: return {Shape::fluted,Material::bakelite,0.f,6.f,.075f,.24f,.35f,true};
        case Profile::chromeDisc: return {Shape::cylinder,Material::chrome,.88f,32.f,.014f,.10f,.305f,false};
        case Profile::originalNickel: return {Shape::bell,Material::nickel,.795f,32.f,.013f,.105f,.258f,true};
    }
    return {Shape::cylinder,Material::bakelite,0.f,32.f,.013f,.105f,.258f,true};
}
inline Profile profileForModel(int model,const char* controlKey=nullptr) noexcept {
    const std::string_view key=controlKey==nullptr?std::string_view{}:std::string_view(controlKey);
    switch(static_cast<AmpModel>(model)) {
        case AmpModel::glass: case AmpModel::bassmanValve: return Profile::americanSkirt;
        case AmpModel::britEdge: return Profile::britishGold;
        case AmpModel::tight515: return Profile::blackPointer;
        case AmpModel::wideRect: return Profile::chromeCylinder;
        case AmpModel::liquidLead: return Profile::blackSkirt;
        case AmpModel::ironTube: return Profile::silverBell;
        case AmpModel::classicTube: return Profile::blackSoftTouch;
        case AmpModel::solidPunch: return Profile::compactSilver;
        case AmpModel::modernBass: return key.starts_with("hw.b7k.")?Profile::blackBar:Profile::blackSoftTouch;
        case AmpModel::chime30: return Profile::blackChickenheadLine;
        case AmpModel::orangeCrown: return Profile::roundDot;
        case AmpModel::subwayClean: return Profile::blackSoftLobe;
        case AmpModel::matchChime: return Profile::creamChickenhead;
        case AmpModel::silkODS: return Profile::blackChickenheadLine;
        case AmpModel::tastePunch: return Profile::silverScalloped;
        case AmpModel::zutaCinder: return Profile::blackNarrowBar;
        case AmpModel::ironCompact: return Profile::blackFaceted;
        case AmpModel::fourChannel: return Profile::blackScalloped;
        case AmpModel::sunMonolith: return Profile::americanSkirt;
        case AmpModel::evilHarvest: return Profile::blackChickenhead;
        case AmpModel::hotLead: return Profile::silverDisc;
        case AmpModel::blueStorm: return Profile::blackChickenhead;
        case AmpModel::specialEdition: return Profile::chromeDisc;
        case AmpModel::nastrond: case AmpModel::niflheimr: return Profile::originalNickel;
        case AmpModel::crystal120: return Profile::blackSkirt;
        case AmpModel::bluestack100: case AmpModel::eightyfire: return Profile::britishGold;
        case AmpModel::fortress103: return Profile::blackPointer;
        case AmpModel::northlight: return Profile::silverDisc;
        default: return Profile::blackSoftTouch;
    }
}
// Preserve the hardware's deliberate size hierarchy without changing hit
// targets or parameter identities. The larger Output keeps the full Mark-IV
// slot. Rockerverb EQ uses the smaller grip seen on the MKIII. No small-
// frequency-knob assumption is made for the D-800+.
inline float bodyScaleForModel(int model,const char* controlKey=nullptr) noexcept {
    const std::string_view key=controlKey==nullptr?std::string_view{}:std::string_view(controlKey);
    if(model==int(AmpModel::liquidLead))return key=="hw.output"?1.f:.88f;
    if(model==int(AmpModel::orangeCrown) && (key.ends_with(".bass") || key.ends_with(".middle") || key.ends_with(".treble")))return .80f;
    return 1.f;
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
inline bool shapedPointer(ProfileSpec spec) noexcept {
    return spec.shape==Shape::chickenhead || spec.shape==Shape::pointer;
}
inline juce::Image renderBody(Profile profile,int size) {
    const auto spec=profileSpec(profile);
    const auto material=spec.material;
    juce::Image image(juce::Image::ARGB,size,size,true);
    const float dimension=float(size);
    // Chickenhead/pointer grips sit on a visibly separate round boss. The
    // boss lighting stays fixed; only the raised grip above it turns.
    const float outerRadius=detail::outerRadius*(shapedPointer(spec)?.70f:spec.shape==Shape::narrowBar?.82f:1.f);
    const float verticalRadius=detail::verticalRadius*(shapedPointer(spec)?.70f:spec.shape==Shape::narrowBar?.82f:1.f);
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
    const float fluteCount=spec.flutes;
    for(int y=0;y<size;++y)for(int x=0;x<size;++x) {
        const float px=(float(x)+.5f)/dimension,py=(float(y)+.5f)/dimension;
        const float nx=(px-.5f)/outerRadius,ny=(py-capCentreY)/verticalRadius;
        const float radius=std::sqrt(nx*nx+ny*ny),angle=std::atan2(ny,nx);
        const float flute=1.f-spec.fluteDepth+spec.fluteDepth*std::cos(angle*fluteCount);
        const float rearY=(py-rearCentreY)/verticalRadius;
        const float rearRadius=std::sqrt(nx*nx+rearY*rearY);
        const float rearAngle=std::atan2(rearY,nx);
        const float rearFlute=1.f-spec.fluteDepth+spec.fluteDepth*std::cos(rearAngle*fluteCount);
        juce::Colour colour;
        float coverage=0.f;
        if(radius<=flute) {
            // The outer ring has a rounded shoulder and recessed grip grooves.
            // Use its surface normal for lighting instead of rotating a gradient.
            const float shoulderStart=profile==Profile::originalNickel?.80f:spec.shape==Shape::skirt?.54f:spec.shape==Shape::bell?.70f:.86f;
            const float shoulder=smooth(shoulderStart,.988f,radius);
            const float light=directional(nx*shoulder,ny*shoulder,1.f-shoulder*.77f);
            const float grooves=(.5f+.5f*std::cos(angle*fluteCount))*smooth(shoulderStart,.95f,radius);
            if(metal && radius<spec.capRadius) {
                const float capRadius=radius/spec.capRadius;
                // Spun metal reflects a broad fixed softbox in two unequal
                // wedges. Fine radial machining modulates that reflection;
                // it never follows the parameter rotation.
                const float reflection=std::pow(std::abs(std::cos(angle+.91f)),13.f);
                const float broad=std::pow(juce::jmax(0.f,std::cos(angle+2.32f)),2.f);
                const float machining=std::sin(capRadius*860.f)*.014f;
                const float grain=std::sin(float(x)*1.71f+float(y)*.39f)*.008f;
                float luminance=.29f+reflection*.70f+broad*.15f+machining+grain;
                if(profile==Profile::silverDisc)luminance=.66f+reflection*.23f+broad*.10f+machining*.40f;
                else if(profile==Profile::silverBell)luminance=.43f+reflection*.48f+broad*.14f+machining;
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
            } else if(profile==Profile::chromeCylinder) {
                // Rectifier hardware has a knurled metal wall, not a dark
                // plastic grip around a silver disc. The crossed grooves stay
                // on the cylinder while its small inset index turns.
                const float knurl=std::sin(angle*44.f+radius*96.f)*std::sin(angle*44.f-radius*96.f);
                colour=shade(.90f,.94f,1.f,.27f+.56f*light+.055f*knurl);
            } else if(cream) {
                const float capLight=directional(nx*.40f,ny*.40f,1.f);
                const float gloss=std::pow(juce::jmax(0.f,1.f-std::hypot(nx+.35f,ny+.42f)),5.f);
                colour=shade(1.f,.95f,.81f,.60f+.26f*(radius<.79f?capLight:light)+.18f*gloss-.06f*grooves);
            } else {
                const float edgeHighlight=std::pow(juce::jmax(0.f,std::cos(angle+2.32f)),8.f)*shoulder;
                const float topSoftbox=std::exp(-((nx+.33f)*(nx+.33f)*2.5f+(ny+.42f)*(ny+.42f)*8.f));
                const float hub=profile==Profile::originalNickel?.78f:spec.shape==Shape::skirt?.60f:spec.shape==Shape::fluted?.67f:.83f;
                const float capLight=radius<hub?directional(nx*.42f,ny*.42f,1.f):light;
                const bool blackCap=material==Material::bakelite;
                // A black cap needs a broad, soft specular window to reveal its
                // curvature against black painted steel. Keep the base dark;
                // lift only the upper-left reflection and rounded shoulder.
                const float sheen=blackCap?.220f:.060f;
                const float rim=blackCap?.32f:.22f;
                float luminance=.040f+.145f*capLight+rim*edgeHighlight+sheen*topSoftbox-.050f*grooves;
                if(radius>hub && radius<hub+.035f)luminance*=.43f;
                // A skirt has a broad lower flange below its narrower grip;
                // scalloped collets retain a small recessed centre. These
                // concentric shoulders are geometry, not coloured dial rings.
                if(spec.shape==Shape::skirt && radius>.86f)luminance*=.60f;
                if(spec.shape==Shape::fluted && profile!=Profile::blackSoftLobe && radius<.42f)luminance*=.68f;
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
            if(profile==Profile::chromeCylinder) {
                const float knurl=std::sin(sideAngle*44.f+py*180.f)*std::sin(sideAngle*44.f-py*180.f);
                colour=shade(.90f,.94f,1.f,.25f+luminance+.055f*knurl);
            } else colour=cream?shade(.97f,.90f,.73f,.45f+luminance*.8f):shade(.83f,.88f,.93f,luminance);
            coverage=clamp((rearFlute-rearRadius)*dimension*outerRadius);
        }
        if(coverage>0.f) {
            const auto previous=pixels.getPixelColour(x,y);
            pixels.setPixelColour(x,y,previous.overlaidWith(colour.withAlpha(coverage)));
        }
    }
    return image;
}
// Non-circular grips turn as real objects. The paths rotate before shading;
// the light and cast shadow stay in panel coordinates, never turn with the dial.
inline void drawGrip(juce::Graphics& g,juce::Rectangle<float> bounds,ProfileSpec spec,float angle) {
    juce::Path grip;
    if(spec.shape==Shape::bar || spec.shape==Shape::narrowBar) {
        grip.startNewSubPath(-.115f,-.365f);grip.lineTo(.115f,-.365f);
        grip.quadraticTo(.145f,-.35f,.145f,-.31f);grip.lineTo(.145f,.31f);
        grip.quadraticTo(.145f,.36f,.11f,.37f);grip.lineTo(-.11f,.37f);
        grip.quadraticTo(-.145f,.36f,-.145f,.31f);grip.lineTo(-.145f,-.31f);
        grip.quadraticTo(-.145f,-.35f,-.115f,-.365f);
    } else if(spec.shape==Shape::pointer) {
        // Wide heel and long, fine beak of the 5150-family molded grip.
        grip.startNewSubPath(0.f,-.395f);
        grip.cubicTo(.075f,-.365f,.075f,-.21f,.16f,-.065f);
        grip.lineTo(.23f,.245f);grip.quadraticTo(0.f,.34f,-.23f,.245f);
        grip.lineTo(-.16f,-.065f);grip.cubicTo(-.075f,-.21f,-.075f,-.365f,0.f,-.395f);
    } else {
        // A chickenhead has a round boss below a narrow raised grip: a
        // tapered nose and a blunt opposite heel, not a flat teardrop button.
        grip.startNewSubPath(0.f,-.395f);
        grip.cubicTo(.05f,-.36f,.09f,-.21f,.105f,-.07f);
        grip.lineTo(.13f,.31f);grip.quadraticTo(.13f,.345f,.10f,.35f);
        grip.lineTo(-.10f,.35f);grip.quadraticTo(-.13f,.345f,-.13f,.31f);
        grip.lineTo(-.105f,-.07f);grip.cubicTo(-.09f,-.21f,-.05f,-.36f,0.f,-.395f);
    }
    grip.closeSubPath();
    // B7K's broad raised bar spans about half the round base diameter;
    // the separately authored GBG120 grip remains narrow.
    if(spec.shape==Shape::bar)grip.applyTransform(juce::AffineTransform::scale(1.38f,1.f));
    else if(spec.shape==Shape::narrowBar)grip.applyTransform(juce::AffineTransform::scale(.76f,1.f));
    const float size=bounds.getWidth(),flatten=verticalRadius/outerRadius;
    grip.applyTransform(juce::AffineTransform::rotation(angle)
        .followedBy(juce::AffineTransform::scale(size,size*flatten))
        .followedBy(juce::AffineTransform::translation(bounds.getCentreX(),bounds.getY()+capCentreY*size)));
    const bool cream=spec.material==Material::cream;
    // Tight occlusion under the extruded grip, plus a soft directional shadow.
    for(int i=4;i>=0;--i) {
        auto shadow=grip;shadow.applyTransform(juce::AffineTransform::translation(size*(.025f+.006f*float(i)),size*(.065f+.006f*float(i))));
        g.setColour(juce::Colours::black.withAlpha(.075f));g.fillPath(shadow);
    }
    auto side=grip;side.applyTransform(juce::AffineTransform::translation(0.f,size*.065f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(cream?0xff897558:0xff20252b),bounds.getX(),bounds.getY(),
        juce::Colour(cream?0xff4c4031:0xff080a0c),bounds.getRight(),bounds.getBottom(),false));
    g.fillPath(side);
    g.setGradientFill(juce::ColourGradient(juce::Colour(cream?0xffeee2c5:0xff323a40),bounds.getX()+size*.2f,bounds.getY()+size*.2f,
        juce::Colour(cream?0xffb4a17c:0xff080d10),bounds.getRight(),bounds.getBottom(),false));
    g.fillPath(grip);
    // Rounded molded shoulder: the upper-left chamfer catches light, while
    // the lower-right edge remains dark. A uniform outline would flatten it.
    g.setGradientFill(juce::ColourGradient(juce::Colour(cream?0xfffff6df:0xffa2a9ac),bounds.getX()+size*.18f,bounds.getY()+size*.14f,
        juce::Colour(cream?0xff8b7655:0xff05090c),bounds.getX()+size*.52f,bounds.getY()+size*.55f,false));
    g.strokePath(grip,juce::PathStrokeType(juce::jmax(.65f,size*.018f)));
    auto top=grip;
    top.applyTransform(juce::AffineTransform::scale(.88f,.88f,bounds.getCentreX(),bounds.getY()+capCentreY*size));
    g.setGradientFill(juce::ColourGradient(juce::Colour(cream?0xffeadfc5:0xff20292e),bounds.getX()+size*.24f,bounds.getY()+size*.22f,
        juce::Colour(cream?0xffb6a47f:0xff090e11),bounds.getX()+size*.75f,bounds.getY()+size*.83f,false));
    g.fillPath(top);
    g.saveState();g.reduceClipRegion(top);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(cream?.18f:.20f),bounds.getX()+size*.34f,bounds.getY()+size*.31f,
        juce::Colours::transparentWhite,bounds.getX()+size*.69f,bounds.getY()+size*.62f,true));
    g.fillRect(bounds);g.restoreState();
    // The molded centre ridge catches the same overhead softbox. Its colour is
    // intentionally subdued; the silhouette itself is the position indicator.
    const auto centre=juce::Point<float>{bounds.getCentreX(),bounds.getY()+capCentreY*size};
    const auto point=[&](float radius){return centre+juce::Point<float>{std::sin(angle)*size*radius,-std::cos(angle)*size*radius*flatten};};
    if(spec.shape!=Shape::bar && spec.shape!=Shape::narrowBar) {
        g.setColour(juce::Colour(cream?0xfffff4d8:0xff868c90).withAlpha(cream?.35f:.17f));
        g.drawLine({point(-.18f),point(.335f)},juce::jmax(.65f,size*.018f));
    }
}
struct Cache {
    struct Entry {Profile profile;int size;juce::Image body;};
    std::vector<Entry> entries;
    size_t cachedPixels{};
    juce::Image body(Profile profile,int size) {
        for(auto& entry:entries)if(entry.profile==profile&&entry.size==size)return entry.body;
        // Quantised sizes and a bounded cache avoid unbounded memory growth if
        // the host continuously changes UI scale. Typical heads need 5 entries.
        const auto nextPixels=size_t(size)*size_t(size);
        constexpr size_t pixelBudget=8*1024*1024; // At most 32 MiB of ARGB bodies.
        while(!entries.empty() && (entries.size()>=96 || cachedPixels+nextPixels>pixelBudget)) {
            cachedPixels-=size_t(entries.front().size)*size_t(entries.front().size);
            entries.erase(entries.begin());
        }
        auto result=renderBody(profile,size);entries.push_back({profile,size,result});cachedPixels+=nextPixels;return result;
    }
};
}
// The last live head releases all JUCE images before plug-in unload. Never
// put native-backed images in a function-local static or DLL teardown storage.
class Renderer final {
    juce::SharedResourcePointer<detail::Cache> cache;
public:
void draw(juce::Graphics& g,juce::Rectangle<float> bounds,float value,float start,float end,int model,bool keyboardFocus,const char* controlKey=nullptr,float minimumBodySize=0.f) {
    if(bounds.getWidth()<1.f || bounds.getHeight()<1.f)return;
    bounds=bounds.withSizeKeepingCentre(juce::jmin(bounds.getWidth(),bounds.getHeight()),juce::jmin(bounds.getWidth(),bounds.getHeight()));
    const auto focusBounds=bounds;
    const float bodyScale=bodyScaleForModel(model,controlKey);
    // Enlarged controls retain their existing 30 px minimum drawing area;
    // hardware size hierarchies must not make a dense panel harder to read.
    // Compact room overviews pass zero and retain the full physical ratio.
    const float bodySize=juce::jmin(bounds.getWidth(),juce::jmax(minimumBodySize,bounds.getWidth()*bodyScale));
    bounds=bounds.withSizeKeepingCentre(bodySize,bodySize);
    const auto profile=profileForModel(model,controlKey);
    const auto spec=profileSpec(profile);
    const auto size=bounds.getWidth();
    const auto c=juce::Point<float>{bounds.getCentreX(),bounds.getY()+detail::capCentreY*size};
    const bool brightFace=model==1||model==5||model==8||model==9||model==10||model==12||model==13||model==14||model==17||model==19||model==21||(model>=26&&model<=29);
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
    g.drawImage(cache->body(profile,pixels),bounds);
    const float angle=start+juce::jlimit(0.f,1.f,value)*(end-start);
    if(detail::shapedPointer(spec) || spec.shape==Shape::bar || spec.shape==Shape::narrowBar)detail::drawGrip(g,bounds,spec,angle);
    const auto direction=juce::Point<float>(std::sin(angle),-std::cos(angle)*detail::verticalRadius/detail::outerRadius);
    const auto from=c+direction*(size*spec.markerFrom);
    const auto to=c+direction*(size*spec.markerTo);
    const float width=juce::jmax(1.15f,size*.035f);
    const bool lightMarker=spec.lightMarker;
    // The shallow engraved channel and its inlay turn together. Its cast
    // micro-shadow uses the same fixed lower-right light direction as the body.
    if(profile==Profile::roundDot || profile==Profile::chromeCylinder) {
        const float diameter=juce::jmax(2.f,size*.068f);
        g.setColour(juce::Colour(0xff0a0c0d));g.fillEllipse(to.x-diameter*.58f,to.y-diameter*.58f,diameter*1.16f,diameter*1.16f);
        g.setColour(juce::Colour(lightMarker?0xffeee9d9:0xff171c20));g.fillEllipse(to.x-diameter*.5f,to.y-diameter*.5f,diameter,diameter);
    } else if(profile!=Profile::creamChickenhead && profile!=Profile::blackChickenhead) {
        g.setColour(juce::Colours::black.withAlpha(.60f));g.drawLine({from.translated(size*.007f,size*.008f),to.translated(size*.007f,size*.008f)},width+size*.018f);
        g.setColour(juce::Colour(lightMarker?0xff161b1c:spec.material==Material::brass?0xfff5deb6:0xffd4d8d5).withAlpha(.85f));g.drawLine({from,to},width+size*.024f);
        g.setColour(juce::Colour(lightMarker?0xffeee9d9:0xff171c20));g.drawLine({from,to},width);
    }
    if(keyboardFocus) {
        g.setColour(juce::Colour(0xff94d4f0));g.drawEllipse(focusBounds.reduced(.6f),1.f);
    }
}
};
}
