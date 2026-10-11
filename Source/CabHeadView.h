#pragma once
#include "AmpHeadShellMetadata.h"
#include "CabHeadDimensions.h"
#include "ChimeraArtworkData.h"
#include "LifecycleTrace.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace spectralforge::cabHead {
struct Face {
    juce::Image shell;
};
struct Bank {
    std::array<Face,spectralforge::ampModelCount> faces;
    Bank() {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        const lifecycle::Scope trace("head.shell.bank.create",this);
        for(int model=0;model<spectralforge::ampModelCount;++model) {
            const auto& spec=headShell::definition(model);
            if(!spec.valid()) {jassertfalse;continue;}
            int size=0;
            const auto* bytes=ChimeraArtworkData::getNamedResource(spec.resource,size);
            if(!bytes || size<=0) {jassertfalse;continue;}
            auto image=juce::ImageFileFormat::loadFrom(bytes,static_cast<size_t>(size));
            if(!image.isValid() || image.getWidth()!=spec.pixelWidth || image.getHeight()!=spec.pixelHeight) {
                jassertfalse;continue;
            }
            faces[static_cast<size_t>(model)].shell=std::move(image);
        }
    }
    ~Bank() {
        const lifecycle::Scope trace("head.shell.bank.destroy",this);
        // All native image resources die with the last scene/editor owner.
        // No process-static graphics resources survive host UI shutdown.
        faces.fill({});
    }
};

inline juce::Rectangle<float> pixels(headShell::PixelRect rect) noexcept {
    return {float(rect.x),float(rect.y),float(rect.width),float(rect.height)};
}
struct Geometry {
    juce::Rectangle<float> front,shell,image;
    juce::AffineTransform sourceToView;
    float supportY{};
};
inline constexpr bool hasTopHandle(int model) noexcept {
    return headShell::definition(model).topHandle;
}
inline constexpr float projectedHeight(int model) noexcept {
    const auto& spec=headShell::definition(model);
    // Depth is already pictured in the complete shell. Adding depth again
    // produced the second lid/backboard in the old composed renderer.
    return spec.valid() ? cabPhysical::head(model).width
        *float(spec.supportY-spec.alphaBounds.y)/float(spec.alphaBounds.width)
        : cabPhysical::head(model).height;
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
    Geometry result;
    result.supportY=bounds.getBottom();
    const auto& spec=headShell::definition(model);
    if(!spec.valid() || bounds.getWidth()<=0.f)return result;
    const float scale=bounds.getWidth()/float(spec.alphaBounds.width);
    result.sourceToView=juce::AffineTransform::scale(scale).translated(
        bounds.getX()-float(spec.alphaBounds.x)*scale,
        result.supportY-float(spec.supportY)*scale);
    result.front=pixels(spec.frontPlane).transformedBy(result.sourceToView);
    result.shell=pixels(spec.alphaBounds).transformedBy(result.sourceToView);
    result.image=juce::Rectangle<float>{0.f,0.f,float(spec.pixelWidth),float(spec.pixelHeight)}
        .transformedBy(result.sourceToView);
    return result;
}
inline void paint(juce::Graphics& g,juce::Rectangle<float> bounds,int model) {
    if(bounds.getWidth()<=0.f || bounds.getHeight()<=0.f)return;
    const auto selected=juce::jlimit(0,spectralforge::ampModelCount-1,model);
    const juce::SharedResourcePointer<Bank> bank;
    const auto& shell=bank->faces[static_cast<size_t>(selected)].shell;
    if(!shell.isValid())return;
    const auto layout=geometry(bounds,selected);
    const juce::Graphics::ScopedSaveState saved(g);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.setColour(juce::Colours::white);
    // Exactly one complete asset. Never add another roof, trim strip, carrying
    // handle, shadow or set of feet around the photographed enclosure.
    g.drawImageTransformed(shell,layout.sourceToView);
}
class View : public juce::Component {
    juce::SharedResourcePointer<Bank> bank;
    int model{};
    std::function<void()> open;
    std::function<void(juce::Graphics&,juce::Rectangle<float>)> controls;
public:
    void setControlPainter(std::function<void(juce::Graphics&,juce::Rectangle<float>)> painter) {controls=std::move(painter);repaint();}
    void setOpenAction(std::function<void()> action) {open=std::move(action);setInterceptsMouseClicks(bool(open),false);setWantsKeyboardFocus(bool(open));setMouseCursor(open?juce::MouseCursor::PointingHandCursor:juce::MouseCursor::NormalCursor);setTitle("Open amplifier");}
    void mouseUp(const juce::MouseEvent& event) override {if(open && !event.mouseWasDraggedSinceMouseDown() && getLocalBounds().contains(event.getPosition()))open();}
    bool keyPressed(const juce::KeyPress& key) override {if(open && (key==juce::KeyPress::returnKey || key==juce::KeyPress::spaceKey)){open();return true;}return false;}
    View() {setInterceptsMouseClicks(false,false);setComponentID("cabPerspectiveHead");}
    void setModel(int next) {next=juce::jlimit(0,spectralforge::ampModelCount-1,next);if(model!=next){model=next;repaint();}}
    int getModel() const {return model;}
    void paint(juce::Graphics& g) override {const auto area=getLocalBounds().toFloat();cabHead::paint(g,area,model);if(controls)controls(g,area);}
};
}
