#pragma once
#include "AmpHeadLayout.h"
#include "CabHeadView.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <string>

namespace chimeraTest {
inline void headAppearanceCheck(bool ok,int model,const char* message) {
    if(!ok)throw std::runtime_error(std::string(message)+" [head model "+std::to_string(model)+"]");
}
inline juce::Rectangle<int> shellAlphaBounds(const juce::Image& image) {
    const juce::Image::BitmapData pixels(image,juce::Image::BitmapData::readOnly);
    int left=pixels.width,top=pixels.height,right=-1,bottom=-1;
    for(int y=0;y<pixels.height;++y)for(int x=0;x<pixels.width;++x)
        if(pixels.getPixelColour(x,y).getAlpha()>32) {
            left=juce::jmin(left,x);top=juce::jmin(top,y);
            right=juce::jmax(right,x);bottom=juce::jmax(bottom,y);
        }
    return right<left ? juce::Rectangle<int>{} : juce::Rectangle<int>{left,top,right-left+1,bottom-top+1};
}
inline bool sameShellPixels(const juce::Image& left,const juce::Image& right) {
    if(left.getBounds()!=right.getBounds())return false;
    const juce::Image::BitmapData a(left,juce::Image::BitmapData::readOnly),b(right,juce::Image::BitmapData::readOnly);
    for(int y=0;y<a.height;++y)for(int x=0;x<a.width;++x)
        if(a.getPixelColour(x,y)!=b.getPixelColour(x,y))return false;
    return true;
}

// Blank plate rectangles measured on the final raw assets by the visual
// review, independently of frontPlane or the runtime knob layout. Keep these
// fixtures explicit: fitting a control onto grille/trim is a visual regression.
inline constexpr std::array<spectralforge::headShell::PixelRect,31> reviewedHeadPlates{{
    {89,568,1596,135}, // 0
    {122,633,1292,158}, // 1
    {89,505,1599,187}, // 2
    {75,659,1385,170}, // 3
    {111,598,1314,201}, // 4
    {101,673,1248,165}, // 5
    {112,655,1223,181}, // 6
    {118,484,1433,217}, // 7
    {84,571,1506,150}, // 8
    {117,467,1296,357}, // 9
    {80,670,1288,136}, // 10
    {136,423,1400,210}, // 11
    {123,550,1590,197}, // 12
    {84,690,1282,153}, // 13
    {120,379,1432,229}, // 14
    {77,583,1518,139}, // 15
    {85,627,1277,247}, // 16
    {130,550,1495,153}, // 17
    {68,664,1315,159}, // 18
    {103,538,1560,154}, // 19
    {78,558,1298,222}, // 20
    {85,631,1279,133}, // 21
    {75,626,1295,147}, // 22
    {97,604,1347,161}, // 23
    {108,522,1555,225}, // 24
    {77,509,1514,261}, // 25
    // 26-30: plates written by Tools/generate_reference_head_shells.py.
    {80,568,1512,212}, // 26
    {80,548,1512,232}, // 27
    {80,548,1512,232}, // 28
    {80,540,1512,240}, // 29
    {280,551,1112,199}, // 30
}};
inline juce::Rectangle<float> reviewedHeadPlate(int model) {
    return spectralforge::cabHead::pixels(reviewedHeadPlates[static_cast<size_t>(model)]);
}

// Independently inspected landmarks on the two shells identified in the user's
// screenshots. Each polyline traverses a real roof shoulder into its front
// corner. A detached narrow lid with missing side panels cannot cover it.
inline std::array<std::array<juce::Point<int>,3>,2> reviewedShellCorners(int model) {
    using Side=std::array<juce::Point<int>,3>;
    if(model==6)return {Side{{{205,260},{125,347},{100,415}}},Side{{{1243,260},{1323,347},{1348,415}}}};
    if(model==17)return {Side{{{175,136},{103,200},{80,255}}},Side{{{1590,135},{1670,200},{1690,255}}}};
    return {};
}
inline void checkReviewedShellLandmarks(const juce::Image& source,const juce::Image& painted,
                                        float scale,juce::Point<float> offset,int model) {
    if(model!=6 && model!=17)return;
    const juce::Image::BitmapData input(source,juce::Image::BitmapData::readOnly),output(painted,juce::Image::BitmapData::readOnly);
    for(const auto& side:reviewedShellCorners(model))for(int segment=0;segment<2;++segment)
        for(int step=0;step<=20;++step) {
            const auto point=side[static_cast<size_t>(segment)].toFloat()
                +(side[static_cast<size_t>(segment+1)]-side[static_cast<size_t>(segment)]).toFloat()*(float(step)/20.f);
            const auto raw=point.roundToInt(),pixel=(point*scale+offset).roundToInt();
            headAppearanceCheck(input.getPixelColour(raw.x,raw.y).getAlpha()>=220
                && output.getPixelColour(pixel.x,pixel.y).getAlpha()>=200,model,
                "Roof shoulder and front corner are disconnected or missing");
        }
    // Fixed blank-panel landmarks, reviewed on the raw source, keep functional
    // controls on the intended metal plate instead of grille, roof or trim.
    const auto panel=reviewedHeadPlate(model);
    const auto& spec=spectralforge::headShell::definition(model);
    const auto front=spectralforge::cabHead::pixels(spec.frontPlane);
    const auto& native=spectralforge::ampNativePanel(model);
    for(size_t channel=0;channel<native.channels.size();++channel)
        for(const auto& knob:spectralforge::ampHead::knobs(model,int(channel))) {
            const juce::Point<float> centre{front.getX()+front.getWidth()*knob.x,front.getY()+front.getHeight()*knob.y};
            headAppearanceCheck(panel.reduced(8.f).contains(centre),model,
                                "A reviewed head's control anchor misses its authored blank panel");
        }
}

// Native/CI callers omit selection and always validate the whole catalogue.
// A selection supports authoring previews while the remaining assets are built.
inline void checkHeadAppearance(std::initializer_list<int> selection={}) {
    using namespace spectralforge;
    const juce::SharedResourcePointer<cabHead::Bank> bank;
    std::set<std::string> resourceNames;
    std::set<std::uint64_t> sourceFingerprints;
    for(int model=0;model<ampModelCount;++model) {
        if(selection.size()!=0 && std::find(selection.begin(),selection.end(),model)==selection.end())continue;
        const auto& spec=headShell::definition(model);
        headAppearanceCheck(spec.valid() && resourceNames.insert(spec.resource).second,model,
                            "Complete head shell metadata is missing or aliases another model");
        headAppearanceCheck(juce::String(spec.resource)=="headshell"+juce::String(model).paddedLeft('0',2)+"_png",model,
                            "Stable amplifier index references another model's shell resource");
        int byteCount=0;
        const auto* bytes=ChimeraArtworkData::getNamedResource(spec.resource,byteCount);
        headAppearanceCheck(bytes && byteCount>0,model,"Complete head shell resource is missing");
        std::uint64_t fingerprint=14695981039346656037ull;
        for(int i=0;i<byteCount;++i) {fingerprint^=static_cast<unsigned char>(bytes[i]);fingerprint*=1099511628211ull;}
        headAppearanceCheck(sourceFingerprints.insert(fingerprint).second,model,
                            "Different amplifiers use an identical shell asset");
        // Decode the embedded resource independently of the production Bank.
        // This catches silent source retouching/cropping as well as cache writes.
        const auto original=juce::ImageFileFormat::loadFrom(bytes,static_cast<size_t>(byteCount));
        const auto& shell=bank->faces[static_cast<size_t>(model)].shell;
        headAppearanceCheck(original.isValid() && original.hasAlphaChannel()
            && original.getWidth()==spec.pixelWidth && original.getHeight()==spec.pixelHeight
            && sameShellPixels(original,shell),model,"Shell bank changed the complete RGBA production asset");
        const auto measured=shellAlphaBounds(original);
        const juce::Rectangle<int> declared{spec.alphaBounds.x,spec.alphaBounds.y,spec.alphaBounds.width,spec.alphaBounds.height};
        const juce::Rectangle<int> front{spec.frontPlane.x,spec.frontPlane.y,spec.frontPlane.width,spec.frontPlane.height};
        headAppearanceCheck(measured==declared && original.getBounds().contains(front)
            && measured.contains(front) && spec.supportY>=front.getBottom()
            && spec.supportY<=measured.getBottom(),model,
            "Authored alpha bounds, front control plane or support baseline do not match the source");
        const bool expectedHandle=model!=int(AmpModel::solidPunch) && model!=int(AmpModel::modernBass)
            && model!=int(AmpModel::subwayClean) && model!=int(AmpModel::tastePunch)
            && model!=int(AmpModel::northlight);
        headAppearanceCheck(spec.topHandle==expectedHandle && cabHead::hasTopHandle(model)==expectedHandle,model,
                            "A rack/desktop shell gained a handle or a carrying handle was omitted");
        const auto& native=ampNativePanel(model);
        const auto plate=reviewedHeadPlate(model);
        const float rawKnobDiameter=float(front.getWidth())*ampHead::faceStyle(model).knobDiameter;
        for(size_t channel=0;channel<native.channels.size();++channel)
            for(const auto& knob:ampHead::knobs(model,int(channel))) {
                const juce::Point<float> centre{float(front.getX())+float(front.getWidth())*knob.x,
                    float(front.getY())+float(front.getHeight())*knob.y};
                const auto point=centre.roundToInt();
                headAppearanceCheck(original.getBounds().contains(point)
                    && original.getPixelAt(point.x,point.y).getAlpha()>=200,model,
                    "A channel's native knob is placed in transparent space outside its complete shell");
                const auto footprint=juce::Rectangle<float>{rawKnobDiameter,rawKnobDiameter}.withCentre(centre);
                if(!plate.contains(footprint))throw std::runtime_error("Knob "+std::string(knob.key)
                    +" leaves the reviewed blank plate [head model "+std::to_string(model)+", footprint "
                    +footprint.toString().toStdString()+", plate "+plate.toString().toStdString()+"]");
            }
        if(model==24 || model==25) {
            headAppearanceCheck(native.channels.size()==spec.channelWindows.size(),model,
                                "Photographed channel windows do not cover every native channel");
            for(size_t channel=0;channel<spec.channelWindows.size();++channel) {
                const auto window=cabHead::pixels(spec.channelWindows[channel]).toNearestInt();
                headAppearanceCheck(!window.isEmpty() && original.getBounds().contains(window)
                    && original.getPixelAt(window.getCentreX(),window.getCentreY()).getAlpha()>=200,model,
                    "A native channel's illuminated window misses the complete source artwork");
                for(size_t earlier=0;earlier<channel;++earlier)
                    headAppearanceCheck(!window.intersects(cabHead::pixels(spec.channelWindows[earlier]).toNearestInt()),model,
                                        "Native channel windows overlap on the authored shell");
            }
        }
        for(const float width:{240.f,640.f,1120.f}) {
            const float scale=width/float(measured.getWidth());
            const float visibleHeight=float(spec.supportY-measured.getY())*scale;
            const juce::Rectangle<float> bounds{40.f,40.f,width,visibleHeight};
            // Align the independently decoded image's authored baseline to the
            // requested support plane. Keep the subtraction in baseline order:
            // algebraically equivalent float reassociation changes JUCE's
            // subpixel interpolation by one alpha level at some window sizes.
            const juce::Point<float> offset{40.f-float(measured.getX())*scale,
                bounds.getBottom()-float(spec.supportY)*scale};
            const auto layout=cabHead::geometry(bounds,model);
            const juce::Rectangle<float> expectedFront{offset.x+float(front.getX())*scale,offset.y+float(front.getY())*scale,
                float(front.getWidth())*scale,float(front.getHeight())*scale};
            headAppearanceCheck(layout.front.getTopLeft().getDistanceFrom(expectedFront.getTopLeft())<.01f
                && layout.front.getBottomRight().getDistanceFrom(expectedFront.getBottomRight())<.01f
                && std::abs(layout.supportY-(offset.y+float(spec.supportY)*scale))<.01f
                && std::abs(layout.shell.getWidth()-width)<.01f,model,
                "Complete shell and controls use different scales, planes or support positions");
            const int canvasWidth=int(std::ceil(width))+80,canvasHeight=int(std::ceil(visibleHeight))+80;
            juce::Image actual(juce::Image::ARGB,canvasWidth,canvasHeight,true,juce::SoftwareImageType{});
            juce::Image expected(juce::Image::ARGB,canvasWidth,canvasHeight,true,juce::SoftwareImageType{});
            {juce::Graphics g(actual);cabHead::paint(g,bounds,model);}
            {
                juce::Graphics g(expected);g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
                g.setColour(juce::Colours::white);
                // Deliberately independent of geometry(). Draw the raw complete
                // image once, uniformly aligned to its measured silhouette.
                g.drawImageTransformed(original,juce::AffineTransform::scale(scale).translated(offset.x,offset.y));
            }
            headAppearanceCheck(sameShellPixels(actual,expected),model,
                "Head paint adds, removes or distorts pixels outside the single authored shell (second roof/feet/handle)");
            checkReviewedShellLandmarks(original,actual,scale,offset,model);
        }
        headAppearanceCheck(sameShellPixels(original,shell),model,"Rendering mutated the shared complete shell image");
    }
}
} // namespace chimeraTest
