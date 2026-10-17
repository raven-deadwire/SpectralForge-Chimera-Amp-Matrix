#pragma once
#include <algorithm>
namespace spectralforge {
// A disabled modeled mic with no selected capture is OFF, not an implicit dry
// filters path. Preserve legacy filters-only rigs and explicit captured/model
// hybrids. Keep the user's stored blend so re-enabling the mic restores it.
struct CabMicRouting {
    bool offA{},offB{};
    float blend{};
};
inline CabMicRouting cabMicRouting(bool modelA,bool modelB,int sourceA,int sourceB,float blend) noexcept {
    const bool offA=!modelA && modelB && sourceA==0;
    const bool offB=!modelB && modelA && sourceB==0;
    return {offA,offB,offA?1.f:offB?0.f:std::clamp(blend,0.f,1.f)};
}
}
