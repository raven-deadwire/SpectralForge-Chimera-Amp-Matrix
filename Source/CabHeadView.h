#pragma once
#include "CabHeadDimensions.h"
#include "RasterArtwork.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <vector>

namespace spectralforge::cabHead {
// Front artwork and its controls keep their measured physical envelope. Roof
// grain and carrying hardware are separate layers, sampled from the same art.
struct Face {
    juce::Image front,material,handle;
    juce::Rectangle<int> sourceBody,materialSource,handleSource,cleanedSource;
    juce::Colour casingColour{0xff252829};
    float aspect{3.f},handleWidth{},handleHeight{};
};
namespace detail {
struct SourceRegions {
    int referenceWidth,handleLeft,handleRight,handleBottom;
    int materialLeft,materialTop,materialWidth,materialHeight;
};
// Reviewed against each shipped source: mounting bases can extend BELOW the
// first opaque casing row. Roof samples exclude hardware, trim, vents and type.
// Coordinates use the source's reference width, so replacement resolutions scale.
inline constexpr std::array<SourceRegions,spectralforge::ampModelCount> sourceRegions{{
    {1000,332,669,34,150,38,110,14}, // Glass
    {1000,318,683,37,150,41,110,14}, // Brit Edge
    {1000,353,648,29,150,31,110,12}, // Tight 515
    {1000,296,704,31,150,33,110,11}, // Wide Rect
    {1000,337,666,24,150,28,110,11}, // Liquid Lead
    {1000,350,649,25,150,29,110,11}, // Iron Tube
    {1000,0,0,0,350,6,300,14},       // Solid Punch: no handle
    {1000,0,0,0,490,8,20,12},        // Modern Bass: avoid its cooling vents
    {1000,359,642,42,150,44,110,12}, // Chime 30
    {1000,285,717,40,150,45,110,18}, // Orange Crown
    {1000,309,693,50,150,53,110,13}, // Vintage Valve
    {1000,0,0,0,350,31,300,15},      // Metro Clean: no handle
    {1000,346,651,43,150,46,110,12}, // Prism Chime
    {1000,348,653,41,150,43,110,12}, // Silk Lead
    {1000,0,0,0,350,43,300,22},      // Taste Punch: no handle
    {1000,317,683,39,150,42,110,12}, // Cinder: original red carrying strap
    {1000,313,688,42,150,43,110,12}, // Iron Compact
    {1000,316,685,39,150,42,110,12}, // Fourfold
    {1000,324,676,39,150,42,110,12}, // Classic Tube
    {1000,317,685,39,150,42,110,12}, // Monolith
    {1000,311,691,41,150,42,110,12}, // Night Harvest
    {1000,313,688,40,150,42,110,12}, // Hot Lead
    {1000,314,687,41,150,42,110,12}, // Blue Storm
    {2172,664,1509,88,300,96,240,24}, // Special Edition
    {2172,651,1524,68,300,30,240,30}, // Nastroend: already photographed from above
    {2172,670,1473,84,300,88,200,30}  // Niflheimr
}};
inline int reflected(int value,int size) noexcept {
    const int phase=value%(size*2);
    return phase<size ? phase : size*2-phase-1;
}
inline juce::Image mirroredMaterial(const juce::Image& image,juce::Rectangle<int> sample,
                                    int model,juce::Colour& diffuse) {
    // A rounded front rail contains a strong vertical lighting gradient. Remove
    // row/column means before repeating its micro-grain: otherwise each repeat
    // copies the rail's highlight, turning the roof into corrugated metal.
    using Channels=std::array<float,3>;
    std::vector<Channels> rows(static_cast<size_t>(sample.getHeight()));
    std::vector<Channels> columns(static_cast<size_t>(sample.getWidth()));
    Channels mean{};
    const juce::Image::BitmapData source(image,juce::Image::BitmapData::readOnly);
    for(int y=0;y<sample.getHeight();++y)for(int x=0;x<sample.getWidth();++x) {
        const auto colour=source.getPixelColour(sample.getX()+x,sample.getY()+y);
        const Channels rgb{float(colour.getRed()),float(colour.getGreen()),float(colour.getBlue())};
        for(size_t channel=0;channel<3;++channel) {
            rows[static_cast<size_t>(y)][channel]+=rgb[channel]/float(sample.getWidth());
            columns[static_cast<size_t>(x)][channel]+=rgb[channel]/float(sample.getHeight());
            mean[channel]+=rgb[channel]/float(sample.getWidth()*sample.getHeight());
        }
    }
    // Upper rail highlights are not the diffuse colour of a flat leather lid.
    // Already elevated metal/Nastroend sources and coloured coverings retain
    // their source exposure; ordinary black tolex uses its darker diffuse tone.
    const bool metal=model==6 || model==7 || model==11 || model==14;
    const float exposure=metal || model==24 ? .90f : (model==9 || model==15 ? .80f : .57f);
    const auto byte=[](float value){return juce::uint8(juce::jlimit(0,255,juce::roundToInt(value)));};
    diffuse=juce::Colour(byte(mean[0]*exposure),byte(mean[1]*exposure),byte(mean[2]*exposure));
    juce::Image tile(juce::Image::ARGB,sample.getWidth()*2,sample.getHeight()*2,true);
    juce::Image::BitmapData target(tile,juce::Image::BitmapData::writeOnly);
    for(int y=0;y<target.height;++y)for(int x=0;x<target.width;++x) {
        const int sx=reflected(x,sample.getWidth()),sy=reflected(y,sample.getHeight());
        const auto colour=source.getPixelColour(sample.getX()+sx,sample.getY()+sy);
        const Channels rgb{float(colour.getRed()),float(colour.getGreen()),float(colour.getBlue())};
        Channels result{};
        for(size_t channel=0;channel<3;++channel) {
            const float grain=rgb[channel]-rows[static_cast<size_t>(sy)][channel]
                -columns[static_cast<size_t>(sx)][channel]+mean[channel];
            result[channel]=mean[channel]*exposure+grain*.58f;
        }
        target.setPixelColour(x,y,juce::Colour(byte(result[0]),byte(result[1]),byte(result[2])));
    }
    return tile;
}
}
struct Bank {
    juce::SharedResourcePointer<spectralforge::art::RasterBank> originals;
    std::array<Face,spectralforge::ampModelCount> faces;
    Bank() {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        for(int model=0;model<spectralforge::ampModelCount;++model) {
            const auto& image=originals->images[static_cast<size_t>(spectralforge::art::headStyle(model).surface)];
            if(!image.isValid())continue;
            const juce::Image::BitmapData pixels(image,juce::Image::BitmapData::readOnly);
            int left=image.getWidth(),right=-1,top=image.getHeight(),bottom=-1;
            for(int y=0;y<pixels.height;++y)for(int x=0;x<pixels.width;++x)
                if(pixels.getPixelColour(x,y).getAlpha()>32) {
                    left=juce::jmin(left,x);right=juce::jmax(right,x);
                    top=juce::jmin(top,y);bottom=juce::jmax(bottom,y);
                }
            if(right<=left || bottom<=top)continue;
            const int width=right-left+1;
            int bodyTop=bottom,bodyBottom=top;
            for(int y=top;y<=bottom;++y) {
                int opaque=0;
                for(int x=left;x<=right;++x)opaque+=pixels.getPixelColour(x,y).getAlpha()>32 ? 1 : 0;
                if(opaque>width*.78f) {bodyTop=juce::jmin(bodyTop,y);bodyBottom=juce::jmax(bodyBottom,y);}
            }
            if(bodyBottom<=bodyTop) {bodyTop=top;bodyBottom=bottom;}
            auto& face=faces[static_cast<size_t>(model)];
            face.sourceBody={left,bodyTop,width,bodyBottom-bodyTop+1};
            face.front=image.getClippedImage(face.sourceBody);
            face.aspect=float(face.front.getWidth())/float(face.front.getHeight());
            const auto& regions=detail::sourceRegions[static_cast<size_t>(model)];
            const float sourceScale=float(image.getWidth())/float(regions.referenceWidth);
            const auto px=[sourceScale](int value){return juce::roundToInt(value*sourceScale);};
            face.materialSource=juce::Rectangle<int>{px(regions.materialLeft),px(regions.materialTop),
                px(regions.materialWidth),px(regions.materialHeight)}.getIntersection(face.sourceBody);
            if(!face.materialSource.isEmpty())
                face.material=detail::mirroredMaterial(image,face.materialSource,model,face.casingColour);
            if(regions.handleRight<=regions.handleLeft)continue;
            face.handleSource=juce::Rectangle<int>{px(regions.handleLeft),top,
                px(regions.handleRight-regions.handleLeft),px(regions.handleBottom)-top}.getIntersection(image.getBounds());
            if(face.handleSource.isEmpty())continue;
            face.handle=image.getClippedImage(face.handleSource).createCopy();
            face.handleWidth=float(face.handle.getWidth())/float(width);
            face.handleHeight=float(face.handle.getHeight())/float(width);

            // Keep the original opening contour, not a guessed quadratic. In
            // the central grip span, the first transparent gap separates the
            // handle from the opaque casing photographed behind it. Preserve
            // the end mounting shoes, which legitimately meet that casing.
            // Nastroend's source paints the opening almost black with opaque
            // alpha; recognise that reviewed gap only below the upper grip.
            juce::Image::BitmapData handlePixels(face.handle,juce::Image::BitmapData::readWrite);
            const float endFraction=model==24 ? .18f : .14f;
            int centreGap=bodyTop-top;
            for(int x=juce::roundToInt(handlePixels.width*endFraction);
                x<juce::roundToInt(handlePixels.width*(1.f-endFraction));++x) {
                bool seenGrip=false;
                int gap=0,cutoff=handlePixels.height;
                for(int y=0;y<handlePixels.height;++y) {
                    const auto colour=handlePixels.getPixelColour(x,y);
                    const int brightest=juce::jmax(colour.getRed(),colour.getGreen(),colour.getBlue());
                    const bool authoredBlackOpening=model==24 && y>=bodyTop-top && brightest<=3;
                    const bool opening=colour.getAlpha()<8 || authoredBlackOpening;
                    if(seenGrip && opening) {
                        if(++gap>=2) {cutoff=y-gap+1;break;}
                    } else {
                        gap=0;
                        if(colour.getAlpha()>64 && (model!=24 || brightest>3))seenGrip=true;
                    }
                }
                if(x==handlePixels.width/2 && cutoff<handlePixels.height)centreGap=cutoff;
                for(int y=cutoff;y<handlePixels.height;++y)
                    handlePixels.setPixelColour(x,y,juce::Colours::transparentBlack);
            }

            // Where the source gap closes near a mounting shoe, keep a smooth
            // lower arch boundary as well. Gap detection alone would leave
            // rectangular casing strips in those otherwise opaque columns.
            const float hw=float(handlePixels.width),hh=float(handlePixels.height);
            const float centreOpening=float(centreGap);
            const float openingEdge=hh-1.f;
            juce::Path hardwareMask;
            hardwareMask.startNewSubPath(0.f,0.f);hardwareMask.lineTo(hw,0.f);
            hardwareMask.lineTo(hw,hh);hardwareMask.lineTo(hw*.86f,hh);
            hardwareMask.lineTo(hw*.86f,openingEdge);
            hardwareMask.quadraticTo(hw*.5f,2.f*centreOpening-openingEdge,hw*.14f,openingEdge);
            hardwareMask.lineTo(hw*.14f,hh);hardwareMask.lineTo(0.f,hh);hardwareMask.closeSubPath();
            juce::Image mask(juce::Image::SingleChannel,handlePixels.width,handlePixels.height,true);
            {juce::Graphics g(mask);g.setColour(juce::Colours::white);g.fillPath(hardwareMask);}
            const juce::Image::BitmapData maskPixels(mask,juce::Image::BitmapData::readOnly);
            for(int y=0;y<handlePixels.height;++y)for(int x=0;x<handlePixels.width;++x) {
                const auto colour=handlePixels.getPixelColour(x,y);
                handlePixels.setPixelColour(x,y,colour.withMultipliedAlpha(maskPixels.getPixelColour(x,y).getFloatAlpha()));
            }

            // Remove only the old handle's overlap with the source casing. A
            // detached copy protects the shared RasterBank used by other views.
            // Same-row casing samples preserve its existing vertical lighting.
            // Include the old contact shadow just below the mounting hardware.
            // It is removed from the casing, never carried along with the grip.
            face.cleanedSource=face.handleSource.withHeight(face.handleSource.getHeight()+px(4))
                .expanded(px(4),0).getIntersection(face.sourceBody);
            const auto oldMounts=face.cleanedSource;
            if(!oldMounts.isEmpty() && !face.materialSource.isEmpty()) {
                face.front=face.front.createCopy();
                juce::Image::BitmapData frontPixels(face.front,juce::Image::BitmapData::readWrite);
                const int feather=juce::jmax(1,px(3));
                const auto byte=[](float value){return juce::uint8(juce::jlimit(0,255,juce::roundToInt(value)));};
                using Channels=std::array<float,3>;
                for(int y=oldMounts.getY();y<oldMounts.getBottom();++y) {
                    Channels leftMean{},rightMean{},sampleMean{};
                    const int border=juce::jmax(2,px(20)),gap=juce::jmax(2,px(18));
                    for(int offset=0;offset<border;++offset) {
                        const auto lc=pixels.getPixelColour(juce::jmax(left,oldMounts.getX()-gap-offset),y);
                        const auto rc=pixels.getPixelColour(juce::jmin(right,oldMounts.getRight()+gap+offset),y);
                        const Channels l{float(lc.getRed()),float(lc.getGreen()),float(lc.getBlue())};
                        const Channels r{float(rc.getRed()),float(rc.getGreen()),float(rc.getBlue())};
                        for(size_t channel=0;channel<3;++channel) {
                            leftMean[channel]+=l[channel]/float(border);rightMean[channel]+=r[channel]/float(border);
                        }
                    }
                    for(int x=face.materialSource.getX();x<face.materialSource.getRight();++x) {
                        const auto c=pixels.getPixelColour(x,y);
                        sampleMean[0]+=float(c.getRed())/float(face.materialSource.getWidth());
                        sampleMean[1]+=float(c.getGreen())/float(face.materialSource.getWidth());
                        sampleMean[2]+=float(c.getBlue())/float(face.materialSource.getWidth());
                    }
                    for(int x=oldMounts.getX();x<oldMounts.getRight();++x) {
                        const int sampleX=face.materialSource.getX()+detail::reflected(x-oldMounts.getX(),face.materialSource.getWidth());
                        const auto sample=pixels.getPixelColour(sampleX,y);
                        const Channels rgb{float(sample.getRed()),float(sample.getGreen()),float(sample.getBlue())};
                        const float t=float(x-oldMounts.getX())/float(juce::jmax(1,oldMounts.getWidth()-1));
                        Channels cleanRGB{};
                        for(size_t channel=0;channel<3;++channel)
                            cleanRGB[channel]=leftMean[channel]+(rightMean[channel]-leftMean[channel])*t
                                +(rgb[channel]-sampleMean[channel])*.75f;
                        const auto clean=juce::Colour(byte(cleanRGB[0]),byte(cleanRGB[1]),byte(cleanRGB[2]));
                        const int distance=juce::jmin(x-oldMounts.getX(),oldMounts.getRight()-1-x);
                        const float edgeBlend=juce::jlimit(0.f,1.f,float(distance+1)/float(feather));
                        const float amount=edgeBlend;
                        const auto original=pixels.getPixelColour(x,y);
                        frontPixels.setPixelColour(x-left,y-bodyTop,original.interpolatedWith(clean,amount));
                    }
                }
            }
        }
    }
};

struct Geometry {
    juce::Rectangle<float> front;
    // Clockwise from the rear left. The rear edge narrows toward the same
    // vanishing point as the cabinet roof; the front fascia remains level.
    std::array<juce::Point<float>,4> roof;
    std::array<juce::Rectangle<float>,2> feet;
    juce::Rectangle<float> handle;
    float supportY{};
};
inline constexpr float roofDepthProjection=.16f;
inline constexpr bool hasTopHandle(int model) noexcept {
    // Desktop and rack amplifiers have no invented carrying handle on the roof.
    const auto selected=static_cast<spectralforge::AmpModel>(model);
    return selected!=spectralforge::AmpModel::solidPunch
        && selected!=spectralforge::AmpModel::modernBass
        && selected!=spectralforge::AmpModel::subwayClean
        && selected!=spectralforge::AmpModel::tastePunch;
}
// The existing physical fascia/foot decomposition remains stable. Cinder's
// photographed strap fits in its projected roof, without shrinking its panel.
inline constexpr bool reservesHandleHeight(int model) noexcept {
    return hasTopHandle(model) && model!=static_cast<int>(spectralforge::AmpModel::zutaCinder);
}
inline constexpr float projectedHeight(int model) noexcept {
    const auto dimensions=spectralforge::cabPhysical::head(model);
    return dimensions.height+dimensions.depth*roofDepthProjection;
}
inline juce::Rectangle<float> physicalBoundsAboveCabinet(juce::Rectangle<float> cabinet,
                                                        float pixelsPerMetre,int model,float supportY) {
    const auto dimensions=spectralforge::cabPhysical::head(model);
    const auto scale=juce::jmax(0.f,pixelsPerMetre);
    const float width=dimensions.width*scale,height=projectedHeight(model)*scale;
    return {cabinet.getCentreX()-width*.5f,supportY-height,width,height};
}
// Retained for legacy callers only. Production scenes use physicalBoundsAboveCabinet.
inline juce::Rectangle<float> boundsAboveCabinet(juce::Rectangle<float> cabinet,float fixedWidth=0.f) {
    const float width=fixedWidth>0.f ? fixedWidth : cabinet.getWidth()*.89f;
    return {cabinet.getCentreX()-width*.5f,cabinet.getY()+cabinet.getWidth()*.035f-width*.395f,
        width,width*.395f};
}
inline Geometry geometry(juce::Rectangle<float> bounds,int model) {
    const auto selected=juce::jlimit(0,spectralforge::ampModelCount-1,model);
    const auto dimensions=spectralforge::cabPhysical::head(selected);
    const float width=juce::jmax(0.f,bounds.getWidth()),scale=width/dimensions.width;
    const bool handle=reservesHandleHeight(selected);
    // Product height is the complete front elevation, including feet/handle.
    // The elevated camera adds only the separately projected physical depth.
    // Bitmap alpha bounds are used for texture extraction, never for geometry.
    const float height=dimensions.height*scale;
    const float footHeight=juce::jmin(dimensions.height*.10f,handle ? .010f : .006f)*scale;
    const float handleHeight=handle ? juce::jmin(dimensions.height*.12f,.022f)*scale : 0.f;
    const float roofDepth=dimensions.depth*roofDepthProjection*scale;
    const float frontHeight=juce::jmax(0.f,height-footHeight-handleHeight);
    Geometry result;
    result.supportY=bounds.getBottom();
    result.front={bounds.getX(),result.supportY-footHeight-frontHeight,width,frontHeight};
    const float backInset=juce::jmin(dimensions.depth*.07f,dimensions.width*.12f)*scale;
    result.roof={{{bounds.getX()+backInset,result.front.getY()-roofDepth},
        {bounds.getRight()-backInset,result.front.getY()-roofDepth},
        result.front.getTopRight(),result.front.getTopLeft()}};
    const float footOverlap=juce::jmin(.0025f*scale,frontHeight*.05f);
    for(int i=0;i<2;++i)result.feet[static_cast<size_t>(i)]={bounds.getX()+width*(i ? .80f : .12f),
        result.front.getBottom()-footOverlap,width*.08f,footHeight+footOverlap};
    if(handle) {
        const float handleWidth=juce::jmin(.20f,dimensions.width*.36f)*scale;
        result.handle={bounds.getCentreX()-handleWidth*.5f,result.roof[0].y+roofDepth*.44f-handleHeight,
            handleWidth,handleHeight};
    }
    return result;
}
// Actual photographic hardware is placed uniformly, independently of the old
// height allowance retained by geometry(). Its feet sit on the roof plane.
inline juce::Rectangle<float> handlePlacement(juce::Rectangle<float> bounds,int model,const Face& face) {
    if(!hasTopHandle(model) || !face.handle.isValid() || bounds.isEmpty())return {};
    const auto layout=geometry(bounds,model);
    const float roofDepth=layout.roof[3].y-layout.roof[0].y;
    const float baseY=layout.roof[0].y+roofDepth*.58f;
    float scale=bounds.getWidth()/float(face.sourceBody.getWidth());
    scale=juce::jmin(scale,juce::jmax(0.f,baseY-bounds.getY())/float(face.handle.getHeight()));
    const float width=face.handle.getWidth()*scale,height=face.handle.getHeight()*scale;
    return {bounds.getCentreX()-width*.5f,baseY-height,width,height};
}
inline juce::Path polygon(std::initializer_list<juce::Point<float>> points) {
    juce::Path path;bool first=true;
    for(const auto p:points) {if(first)path.startNewSubPath(p);else path.lineTo(p);first=false;}
    path.closeSubPath();return path;
}
inline void paint(juce::Graphics& g,juce::Rectangle<float> bounds,int model) {
    if(bounds.getWidth()<=0.f || bounds.getHeight()<=0.f)return;
    const juce::SharedResourcePointer<Bank> bank;
    const auto& face=bank->faces[static_cast<size_t>(juce::jlimit(0,spectralforge::ampModelCount-1,model))];
    const auto layout=geometry(bounds,model);
    const auto& roof=layout.roof;
    const float width=bounds.getWidth(),edge=juce::jmax(.6f,width*.0021f);
    const juce::Graphics::ScopedSaveState saved(g);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    // Broad ambient contact and two dark, sharply anchored rubber feet. Both
    // feet end on the cabinet's top plane, not on its transparent image border.
    g.setColour(juce::Colours::black.withAlpha(.42f));
    g.fillEllipse(bounds.getX()+width*.055f,layout.supportY-width*.022f,width*.89f,width*.022f);
    for(const auto foot:layout.feet) {
        g.setColour(juce::Colour(0xff070a0b));g.fillRoundedRectangle(foot,width*.006f);
        g.setColour(juce::Colour(0xff454a49));g.drawHorizontalLine(juce::roundToInt(foot.getY()),foot.getX()+edge,foot.getRight()-edge);
    }

    // The source casing has rounded shoulders. Let the roof enter that casing
    // slightly below its alpha-bound top, instead of hanging over its corners
    // as a rectangular board. The physical front/control coordinate space stays
    // unchanged; this is only the covered rear layer.
    const bool metal=model==6 || model==7 || model==11 || model==14;
    const float shoulder=width*(metal ? .095f : .037f);
    const float overlap=width*(metal ? .033f : .014f);
    const std::array<juce::Point<float>,4> paintRoof{{
        {roof[0].x+shoulder,roof[0].y},{roof[1].x-shoulder,roof[1].y},
        {roof[2].x-shoulder,roof[2].y+overlap},
        {roof[3].x+shoulder,roof[3].y+overlap}}};
    const auto top=polygon({paintRoof[0],paintRoof[1],paintRoof[2],paintRoof[3]});
    g.setColour(face.casingColour.darker(.10f));g.fillPath(top);
    if(face.material.isValid()) {
        // Tile a detached, mirrored casing patch over the physical surface,
        // then project its depth. This compresses grain with perspective;
        // a narrow fascia strip must never be stretched into a whole lid.
        const auto dimensions=spectralforge::cabPhysical::head(model);
        const float tw=float(face.sourceBody.getWidth());
        const float th=tw*dimensions.depth/dimensions.width;
        for(int triangle=0;triangle<2;++triangle) {
            const juce::Graphics::ScopedSaveState clipped(g);
            const auto a=paintRoof[0],b=triangle ? paintRoof[2] : paintRoof[1],c=triangle ? paintRoof[3] : paintRoof[2];
            g.reduceClipRegion(polygon({a,b,c}));
            const auto transform=triangle
                ? juce::AffineTransform::fromTargetPoints(0.f,0.f,a.x,a.y,tw,th,b.x,b.y,0.f,th,c.x,c.y)
                : juce::AffineTransform::fromTargetPoints(0.f,0.f,a.x,a.y,tw,0.f,b.x,b.y,tw,th,c.x,c.y);
            g.addTransform(transform);
            g.setTiledImageFill(face.material,0,0,.82f);
            g.fillRect(juce::Rectangle<float>{0.f,0.f,tw,th});
        }
    }
    g.setGradientFill({juce::Colours::white.withAlpha(.025f),paintRoof[0],juce::Colours::black.withAlpha(.22f),paintRoof[3],false});g.fillPath(top);
    g.setColour(face.casingColour.brighter(.18f).withAlpha(.60f));g.drawLine({paintRoof[0],paintRoof[1]},edge);
    g.setColour(juce::Colours::black.withAlpha(.65f));g.drawLine({paintRoof[0],paintRoof[3]},edge);g.drawLine({paintRoof[1],paintRoof[2]},edge);

    // The full frontal shell keeps its measured width. The roof alone recedes;
    // artificial side rails must not steal width from a compact head's fascia.
    if(face.front.isValid()) {
        g.setColour(juce::Colours::white);g.drawImage(face.front,layout.front,juce::RectanglePlacement::stretchToFit);
    } else {
        g.setColour(juce::Colour(0xff1d2323));g.fillRoundedRectangle(layout.front,width*.013f);
    }
    g.setColour(juce::Colours::black.withAlpha(.5f));g.drawLine({layout.front.getBottomLeft(),layout.front.getBottomRight()},edge);


    const auto hardware=handlePlacement(bounds,model,face);
    if(!hardware.isEmpty()) {
        // Two mounting shoes, with short contact shadows, anchor the handle.
        // A broad hard ellipse under the entire arch made it look pasted on.
        const float contactWidth=hardware.getWidth()*.15f;
        for(int side=0;side<2;++side) {
            const float x=side ? hardware.getRight()-contactWidth : hardware.getX();
            g.setColour(juce::Colours::black.withAlpha(.19f));
            g.fillEllipse(x-width*.004f,hardware.getBottom()-width*.0025f,contactWidth+width*.008f,width*.007f);
            g.setColour(juce::Colours::black.withAlpha(.43f));
            g.fillEllipse(x,hardware.getBottom()-width*.0015f,contactWidth,width*.0035f);
        }
        g.setColour(juce::Colours::white);
        g.drawImage(face.handle,hardware,juce::RectanglePlacement::centred);
    }

}
class View : public juce::Component {
    juce::SharedResourcePointer<Bank> bank;
    int model{};
    std::function<void()> open;
public:
    void setOpenAction(std::function<void()> action) {open=std::move(action);setInterceptsMouseClicks(bool(open),false);setWantsKeyboardFocus(bool(open));setMouseCursor(open?juce::MouseCursor::PointingHandCursor:juce::MouseCursor::NormalCursor);setTitle("Open amplifier");}
    void mouseUp(const juce::MouseEvent& event) override {if(open && !event.mouseWasDraggedSinceMouseDown() && getLocalBounds().contains(event.getPosition()))open();}
    bool keyPressed(const juce::KeyPress& key) override {if(open && (key==juce::KeyPress::returnKey || key==juce::KeyPress::spaceKey)){open();return true;}return false;}
    View() {setInterceptsMouseClicks(false,false);setComponentID("cabPerspectiveHead");}
    void setModel(int next) {next=juce::jlimit(0,spectralforge::ampModelCount-1,next);if(model!=next){model=next;repaint();}}
    int getModel() const {return model;}
    void paint(juce::Graphics& g) override {cabHead::paint(g,getLocalBounds().toFloat(),model);}
};
}
