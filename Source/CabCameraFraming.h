#pragma once
#include <algorithm>
#include <cmath>
#include <span>

namespace spectralforge::cabCamera {
// A complete projected head + cabinet stack, in metres. Geometry within a rig
// always uses one scale. Focus framing does not change any equipment dimension.
struct Envelope {float width{},height{};};
inline constexpr float focusSideAllowance=.56f;
inline constexpr float roomSideAllowance=.08f;
inline constexpr float heightAllowance=.05f;
inline constexpr float verticalOccupancy=.90f;

inline bool valid(Envelope value) noexcept {
    return std::isfinite(value.width) && std::isfinite(value.height)
        && value.width>0.f && value.height>0.f;
}
inline Envelope stack(float cabinetWidth,float cabinetHeight,float headWidth,float headHeight) noexcept {
    return {std::max(cabinetWidth,headWidth),cabinetHeight+headHeight};
}
inline float fit(float width,float height,Envelope envelope,float sideAllowance) noexcept {
    if(!std::isfinite(width) || !std::isfinite(height) || width<=0.f || height<=0.f || !valid(envelope))return 0.f;
    return std::min(width/(envelope.width+sideAllowance),
        height*verticalOccupancy/(envelope.height+heightAllowance));
}
inline float focusScale(float width,float height,Envelope envelope) noexcept {
    // Fixed space for every supported microphone body and its travel. Neither
    // current mic identity nor position participates in camera framing.
    return fit(width,height,envelope,focusSideAllowance);
}
inline float roomScale(float width,float height,std::span<const Envelope> rigs) noexcept {
    Envelope complete{};
    for(const auto rig:rigs) {
        if(!valid(rig))return 0.f;
        complete.width=std::max(complete.width,rig.width);
        complete.height=std::max(complete.height,rig.height);
    }
    // The overview draws no microphone bodies. Fit the actual active stacks
    // together with one shared metre; unused catalogue entries cannot shrink it.
    return fit(width,height,complete,roomSideAllowance);
}
}
