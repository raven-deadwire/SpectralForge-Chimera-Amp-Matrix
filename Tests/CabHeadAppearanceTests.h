#pragma once
#include "CabHeadView.h"
#include <cmath>
#include <stdexcept>
#include <string>

namespace chimeraTest {
// These are the released front positions at a 1000 px head width, captured
// before the roof/handle correction. A cosmetic roof change must not move the
// model-specific control anchors or resize the enclosure in the cabinet room.
inline constexpr std::array<juce::Point<float>,26> headFrontBaseline{{
    {95.327292f,286.733026f}, {82.406015f,350.375940f},
    {103.141474f,328.268826f}, {95.927127f,342.751274f},
    {136.902887f,436.745407f}, {121.049180f,426.229508f},
    {87.272727f,293.181818f}, {131.764706f,294.928208f},
    {91.574468f,357.446809f}, {121.454545f,432.727273f},
    {123.515989f,356.741122f}, {122.566038f,180.285247f},
    {118.425197f,441.402674f}, {112.413793f,393.103448f},
    {124.444444f,150.000000f}, {72.877847f,262.939959f},
    {163.529412f,410.588235f}, {90.270270f,362.162162f},
    {122.622951f,426.229508f}, {97.313433f,325.373134f},
    {86.027397f,339.726027f}, {90.645669f,359.606299f},
    {94.492754f,359.420290f}, {96.338028f,335.211268f},
    {96.571429f,340.000000f}, {114.754098f,349.180328f}
}};
static_assert(headFrontBaseline.size()==spectralforge::ampModelCount,
              "Review front placement when adding an amplifier");

inline void headAppearanceCheck(bool ok,int model,const char* message) {
    if(!ok)throw std::runtime_error(std::string(message)+" [head model "+std::to_string(model)+"]");
}

inline void checkHeadAppearance() {
    using namespace spectralforge;
    const juce::SharedResourcePointer<cabHead::Bank> bank;
    for(int model=0;model<ampModelCount;++model) {
        // Named catalogue exceptions are intentional. Cinder is rack-sized but
        // its source artwork has a red carrying strap and metal mounting feet.
        const bool expectedHandle=model!=int(AmpModel::solidPunch)
            && model!=int(AmpModel::modernBass) && model!=int(AmpModel::subwayClean)
            && model!=int(AmpModel::tastePunch);
        headAppearanceCheck(cabHead::hasTopHandle(model)==expectedHandle,model,
                            "Rack/desktop handle classification changed");
        const auto& face=bank->faces[static_cast<size_t>(model)];
        headAppearanceCheck(face.front.isValid() && face.material.isValid(),model,
                            "Head fascia or casing sample is missing");
        headAppearanceCheck(face.sourceBody.contains(face.materialSource)
            && !face.materialSource.intersects(face.handleSource),model,
            "Roof material includes source carrying hardware or transparent padding");
        headAppearanceCheck(face.handle.isValid()==expectedHandle
            && face.handleSource.isEmpty()!=expectedHandle,model,
            "Source carrying handle is missing or a handleless head gained hardware");
        // Independent pixel landmarks reviewed on the source assets. The old
        // alpha silhouette crop stopped at bodyTop and sliced these brackets.
        int mountingFootBottom=0;
        switch(model) {
            case 0:mountingFootBottom=32;break;
            case 1:mountingFootBottom=36;break;
            case 9:mountingFootBottom=38;break;
            case 10:mountingFootBottom=49;break;
            case 23:mountingFootBottom=84;break;
            case 24:mountingFootBottom=64;break;
            case 25:mountingFootBottom=79;break;
            default:break;
        }
        headAppearanceCheck(face.handleSource.getBottom()>=mountingFootBottom,model,
                            "Source handle extraction amputates its mounting feet");
        const auto& original=bank->originals->images[static_cast<size_t>(art::headStyle(model).surface)];
        headAppearanceCheck(original.getBounds().contains(face.sourceBody)
            && face.front.getWidth()==face.sourceBody.getWidth()
            && face.front.getHeight()==face.sourceBody.getHeight(),model,
            "Handle cleanup recropped or resized the original front artwork");
        const juce::Image::BitmapData sourcePixels(original,juce::Image::BitmapData::readOnly);
        const juce::Image::BitmapData frontPixels(face.front,juce::Image::BitmapData::readOnly);
        headAppearanceCheck(face.cleanedSource.isEmpty() || (face.sourceBody.contains(face.cleanedSource)
            && face.cleanedSource.getBottom()<=face.sourceBody.getY()+face.sourceBody.getHeight()*.12f),model,
            "Handle shadow cleanup extends into the control fascia");
        // This is the old mount's footprint in the front, not the newly placed
        // roof sprite. It must be erased once, otherwise a second base remains.
        if(mountingFootBottom>0) {
            const auto footprint=face.handleSource.getIntersection(face.sourceBody);
            int changed=0;
            for(int y=footprint.getY();y<footprint.getBottom();++y)
                for(int x=footprint.getX();x<footprint.getRight();++x)
                    changed+=sourcePixels.getPixelColour(x,y)!=frontPixels.getPixelColour(
                        x-face.sourceBody.getX(),y-face.sourceBody.getY()) ? 1 : 0;
            headAppearanceCheck(changed>0,model,"Original handle mounting feet remain duplicated on the front");
        }
        // Independently reviewed dark-arch landmarks. Removing only the new
        // sprite's mask left the OLD handle's opening and cast shadow visible
        // on these two fascias. The repaired patch should match nearby casing
        // lighting, not merely differ by a pixel from the old photograph.
        if(model==10 || model==24) {
            const juce::Rectangle<int> shadow=model==10
                ? juce::Rectangle<int>{390,40,220,7} : juce::Rectangle<int>{925,54,330,10};
            const juce::Rectangle<int> casing=model==10
                ? juce::Rectangle<int>{200,40,90,7} : juce::Rectangle<int>{450,54,90,10};
            headAppearanceCheck(face.cleanedSource.contains(shadow),model,
                                "Cleanup misses the original handle's dark arch");
            const auto brightness=[&](juce::Rectangle<int> area,bool repaired) {
                double total=0;
                for(int y=area.getY();y<area.getBottom();++y)for(int x=area.getX();x<area.getRight();++x) {
                    const auto pixel=repaired ? frontPixels.getPixelColour(x-face.sourceBody.getX(),y-face.sourceBody.getY())
                                              : sourcePixels.getPixelColour(x,y);
                    total+=(double(pixel.getRed())+pixel.getGreen()+pixel.getBlue())/3.;
                }
                return total/double(area.getWidth()*area.getHeight());
            };
            const auto before=brightness(shadow,false),after=brightness(shadow,true),reference=brightness(casing,false);
            headAppearanceCheck(after>before+8. && std::abs(after-reference)<std::abs(before-reference)*.65,model,
                                "Original dark handle arch or shadow remains on the fascia");
            // These source pixels are opaque casing beneath the grip. They
            // must not travel with the extracted handle as a grey crescent.
            // Retaining a known grip pixel prevents passing by erasing it all.
            const juce::Image::BitmapData hardwarePixels(face.handle,juce::Image::BitmapData::readOnly);
            const auto hardwareAlpha=[&](juce::Point<int> sourcePoint) {
                headAppearanceCheck(face.handleSource.contains(sourcePoint),model,
                                    "Reviewed handle landmark is outside the source extraction");
                return hardwarePixels.getPixelColour(sourcePoint.x-face.handleSource.getX(),
                    sourcePoint.y-face.handleSource.getY()).getAlpha();
            };
            const juce::Point<int> opening=model==10 ? juce::Point<int>{500,43} : juce::Point<int>{1086,64};
            const juce::Point<int> grip=model==10 ? juce::Point<int>{500,12} : juce::Point<int>{1086,16};
            headAppearanceCheck(sourcePixels.getPixelColour(opening.x,opening.y).getAlpha()>200
                && hardwareAlpha(opening)<=8 && hardwareAlpha(grip)>=200,model,
                "Extracted handle carries casing under its arch or loses its grip");
            if(model==10)headAppearanceCheck(hardwareAlpha({450,34})<=8,model,
                "Bassman carrying handle retains the off-centre grey casing crescent");
        }
        // Preserve artwork everywhere outside the explicit hardware/shadow
        // cleanup strip, including corner trim, logos and the control fascia.
        for(int row=0;row<16;++row)for(int column=0;column<16;++column) {
            const int x=face.sourceBody.getX()+(2*column+1)*face.sourceBody.getWidth()/32;
            const int y=face.sourceBody.getY()+(2*row+1)*face.sourceBody.getHeight()/32;
            if(face.cleanedSource.contains(x,y))continue;
            headAppearanceCheck(sourcePixels.getPixelColour(x,y)==frontPixels.getPixelColour(
                x-face.sourceBody.getX(),y-face.sourceBody.getY()),model,
                "Roof correction modified artwork outside the hardware cleanup");
        }
        const auto baseline=headFrontBaseline[static_cast<size_t>(model)];
        for(const float width:{240.f,640.f,1120.f}) {
            const auto physical=cabPhysical::head(model);
            const juce::Rectangle<float> bounds{17.f,23.f,width,
                width*cabHead::projectedHeight(model)/physical.width};
            const auto layout=cabHead::geometry(bounds,model);
            const auto handle=cabHead::handlePlacement(bounds,model,face);
            const float scale=width/1000.f;
            headAppearanceCheck(std::abs(layout.front.getX()-bounds.getX())<.01f
                && std::abs(layout.front.getWidth()-width)<.01f
                && std::abs(layout.front.getY()-bounds.getY()-baseline.x*scale)<.01f
                && std::abs(layout.front.getHeight()-baseline.y*scale)<.01f,model,
                "Roof/handle correction moved the released fascia/control anchors");
            headAppearanceCheck(handle.isEmpty()!=expectedHandle,model,
                                "Rendered carrying handle presence differs from the original model");
            if(expectedHandle) {
                headAppearanceCheck(bounds.expanded(.01f).contains(handle)
                    && handle.getBottom()<=layout.front.getY()+.01f
                    && handle.getBottom()>layout.roof[0].y,model,
                    "Handle floats outside the roof or covers the front controls");
                const float horizontalScale=handle.getWidth()/float(face.handle.getWidth());
                const float verticalScale=handle.getHeight()/float(face.handle.getHeight());
                headAppearanceCheck(std::isfinite(horizontalScale) && horizontalScale>0.f
                    && std::abs(horizontalScale-verticalScale)<.0001f,model,
                    "Carrying handle source aspect was stretched or flattened");
            }
        }
    }
}
} // namespace chimeraTest
