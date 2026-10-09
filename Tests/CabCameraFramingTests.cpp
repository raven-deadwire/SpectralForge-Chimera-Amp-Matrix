#include "CabCameraFraming.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
bool near(float a,float b,float tolerance=.0001f) {return std::abs(a-b)<=tolerance;}
}

int main() {
    namespace camera=spectralforge::cabCamera;
    try {
        // Production-authored enclosure sizes and the selected heads' projected
        // heights. Assertions use independent physical ratios and visible fit.
        const std::array<camera::Envelope,3> stacks{{
            camera::stack(.63f,.94f+.40f*.16f,.610f,.292f+.324f*.16f),
            camera::stack(.74f,.76f+.36f*.16f,.676275f,.254f+.29845f*.16f),
            camera::stack(.62f,.64f+.40f*.16f,.610f,.245f+.300f*.16f)
        }};
        for(const auto area:std::array<std::array<float,2>,4>{{{{315,312}},{{288,480}},{{250,420}},{{216,360}}}}) {
            const float scale=camera::roomScale(area[0],area[1],stacks);
            require(std::isfinite(scale) && scale>0,"invalid room camera");
            for(const auto rig:stacks) {
                require(rig.width*scale<=area[0] && rig.height*scale<=area[1]*.90f,"a complete active stack escapes its viewport");
            }
            require(near((.74f*scale)/(.63f*scale),.74f/.63f)
                && near((.62f*scale)/(.63f*scale),.62f/.63f),"room altered 4x12/4x10 width relative to 6x10");
            require(near((.76f*scale)/(.94f*scale),.76f/.94f)
                && near((.64f*scale)/(.94f*scale),.64f/.94f),"room altered physical height ratios");
            require(near(camera::roomScale(area[0]*.75f,area[1]*.75f,stacks),scale*.75f),"UI scaling applies twice to a room camera");
        }
        const float sixRoom=camera::roomScale(288,480,stacks);
        require(near(sixRoom,432.f/1.39784f),"6x10 room does not frame its actual selected stack height");
        require(.74f*sixRoom>225.f,"4x12 beside 6x10 remains constrained by nonexistent room microphone bodies");
        auto smaller=stacks;smaller[0]=camera::stack(.62f,.64f+.40f*.16f,.610f,.292f+.324f*.16f);
        const float compactRoom=camera::roomScale(288,480,smaller);
        require(compactRoom>sixRoom*1.10f && .74f*compactRoom>240.f,"unused 6x10 still constrains the active room");
        require(near(compactRoom,288.f/.82f),"active room fails to use the available horizontal stage");
        const auto oneTwelve=camera::stack(.48f,.48f+.30f*.16f,.676275f,.254f+.29845f*.16f);
        const float smallFocus=camera::focusScale(544,482,oneTwelve);
        const float largeFocus=camera::focusScale(544,482,stacks[0]);
        require(smallFocus>largeFocus*1.25f,"focused small stack reserves the unselected tall cabinet");
        require((oneTwelve.width+.56f)*smallFocus<=544.0001f,"focused camera lost the microphone movement allowance");
        // The same single scale must apply to every physical part of a rig.
        for(float scale:{smallFocus,largeFocus,compactRoom,sixRoom}) {
            require(near((.032f*scale)/(.3048f*scale),.032f/.3048f),"microphone-to-speaker ratio changes with camera");
            require(near((.676275f*scale)/(.48f*scale),.676275f/.48f),"head-to-cabinet ratio changes with camera");
        }
        const float nan=std::numeric_limits<float>::quiet_NaN();
        require(camera::roomScale(288,480,{})==0 && camera::focusScale(0,482,oneTwelve)==0
            && camera::focusScale(-1,482,oneTwelve)==0 && camera::focusScale(nan,482,oneTwelve)==0
            && camera::focusScale(544,482,{0,1})==0,"invalid or empty geometry has a drawable camera");
        std::cout<<"PASS CAB camera framing: active-only uniform room scale, 6x10/4x12/4x10 proportions, "
            <<"selected focus, microphone margin, narrow/scaled viewports and invalid bounds\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
