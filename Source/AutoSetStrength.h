#pragma once
#include <cmath>

namespace spectralforge::autoset {
inline float scaledInputGainDb(float before,float proposed,float strength) noexcept {
    if(strength>=1.f)return proposed;
    // Input gain has a 0.01 dB parameter interval. Normalisation can leave
    // tiny errors (notably on FMA targets) that flip a 0.25 dB tie by a
    // whole 0.5 dB step. Recover the legal decimal endpoints first, then
    // interpolate in double precision and round ties away from zero.
    // Keep genuine 0.01 dB user settings; do not snap them to 0.5 dB.
    const double beforeCents=std::round(double(before)*100.0);
    const double proposedCents=std::round(double(proposed)*100.0);
    const double cents=beforeCents+double(strength)*(proposedCents-beforeCents);
    return float(std::round(cents/50.0)*0.5);
}
}
