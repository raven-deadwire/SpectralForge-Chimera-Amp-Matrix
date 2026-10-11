#pragma once
#include "CabHeadDimensions.h"
#include "RasterArtwork.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace spectralforge::cabHead {
// The room and focused view use the same elevated, frontal camera as the
// original cabinet artwork. Preserve each amp's photographic fascia while
// giving its enclosure a roof, recessed side rails, and an actual support plane.
// This is presentation only: it has no processing or state parameters.
struct Face {
    juce::Image front,material,handle;
    float aspect{3.f},handleWidth{.28f},handleHeight{.025f};
};
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
            face.front=image.getClippedImage({left,bodyTop,width,bodyBottom-bodyTop+1});
            face.aspect=float(face.front.getWidth())/float(face.front.getHeight());
            // The unprinted casing at the upper edge supplies the roof's
            // leather/paint grain. Small clipped image views share pixel data.
            const int materialHeight=juce::jmax(3,juce::roundToInt(width*.012f));
            face.material=image.getClippedImage({left+juce::roundToInt(width*.2f),bodyTop+1,
                juce::roundToInt(width*.6f),juce::jmin(materialHeight,bodyBottom-bodyTop)});
            // Extract the source's carrying handle separately, without old
            // feet or transparent padding affecting the cabinet contact point.
            if(bodyTop>top+2) {
                const int centreLeft=left+width/4,centreRight=right-width/4;
                int handleLeft=centreRight,handleRight=centreLeft,handleTop=bodyTop;
                for(int y=top;y<bodyTop-1;++y)for(int x=centreLeft;x<=centreRight;++x)
                    if(pixels.getPixelColour(x,y).getAlpha()>32) {
                        handleLeft=juce::jmin(handleLeft,x);handleRight=juce::jmax(handleRight,x);
                        handleTop=juce::jmin(handleTop,y);
                    }
                if(handleRight>handleLeft && handleTop<bodyTop-2) {
                    face.handle=image.getClippedImage({handleLeft,handleTop,handleRight-handleLeft+1,bodyTop-handleTop-1});
                    face.handleWidth=float(face.handle.getWidth())/float(width);
                    face.handleHeight=float(face.handle.getHeight())/float(width);
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
        && selected!=spectralforge::AmpModel::tastePunch
        && selected!=spectralforge::AmpModel::zutaCinder;
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
    const bool handle=hasTopHandle(selected);
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

    const auto top=polygon({roof[0],roof[1],roof[2],roof[3]});
    g.setGradientFill({juce::Colour(0xff323735),roof[0],juce::Colour(0xff161b1b),roof[3],false});g.fillPath(top);
    if(face.material.isValid()) {
        // Two affine triangles map the existing casing grain onto a proper
        // trapezoidal roof; a flat rectangular overlay would break perspective.
        const auto tw=float(face.material.getWidth()),th=float(face.material.getHeight());
        for(int triangle=0;triangle<2;++triangle) {
            const juce::Graphics::ScopedSaveState clipped(g);
            const auto a=roof[0],b=triangle ? roof[2] : roof[1],c=triangle ? roof[3] : roof[2];
            g.reduceClipRegion(polygon({a,b,c}));g.setOpacity(.63f);
            const auto transform=triangle
                ? juce::AffineTransform::fromTargetPoints(0.f,0.f,a.x,a.y,tw,th,b.x,b.y,0.f,th,c.x,c.y)
                : juce::AffineTransform::fromTargetPoints(0.f,0.f,a.x,a.y,tw,0.f,b.x,b.y,tw,th,c.x,c.y);
            g.drawImageTransformed(face.material,transform);
        }
    }
    g.setGradientFill({juce::Colours::white.withAlpha(.085f),roof[0],juce::Colours::black.withAlpha(.32f),roof[3],false});g.fillPath(top);
    g.setColour(juce::Colour(0xff969b92).withAlpha(.52f));g.drawLine({roof[0],roof[1]},edge);
    g.setColour(juce::Colour(0xff080d0e));g.drawLine({roof[0],roof[3]},edge);g.drawLine({roof[1],roof[2]},edge);

    // The full frontal shell keeps its measured width. The roof alone recedes;
    // artificial side rails must not steal width from a compact head's fascia.
    if(face.front.isValid()) {
        g.setColour(juce::Colours::white);g.drawImage(face.front,layout.front,juce::RectanglePlacement::stretchToFit);
    } else {
        g.setColour(juce::Colour(0xff1d2323));g.fillRoundedRectangle(layout.front,width*.013f);
    }
    g.setColour(juce::Colours::black.withAlpha(.5f));g.drawLine({layout.front.getBottomLeft(),layout.front.getBottomRight()},edge);
    g.setColour(juce::Colour(0xffa7a997).withAlpha(.24f));g.drawLine({roof[3],roof[2]},edge);

    if(!layout.handle.isEmpty()) {
        g.setColour(juce::Colours::black.withAlpha(.5f));
        g.fillEllipse(layout.handle.getX()-width*.01f,layout.handle.getBottom()-width*.003f,
            layout.handle.getWidth()+width*.02f,width*.009f);
        if(face.handle.isValid()) {
            g.setColour(juce::Colours::white);g.drawImage(face.handle,layout.handle,juce::RectanglePlacement::stretchToFit);
        } else {
            g.setColour(juce::Colour(0xff717671));g.drawRoundedRectangle(layout.handle,width*.007f,juce::jmax(1.f,width*.004f));
            g.setColour(juce::Colour(0xff111717));g.drawRoundedRectangle(layout.handle.translated(0.f,width*.003f),width*.007f,juce::jmax(1.f,width*.004f));
        }
    }
}
class View : public juce::Component {
    juce::SharedResourcePointer<Bank> bank;
    int model{};
public:
    View() {setInterceptsMouseClicks(false,false);setComponentID("cabPerspectiveHead");}
    void setModel(int next) {next=juce::jlimit(0,spectralforge::ampModelCount-1,next);if(model!=next){model=next;repaint();}}
    int getModel() const {return model;}
    void paint(juce::Graphics& g) override {cabHead::paint(g,getLocalBounds().toFloat(),model);}
};
}
