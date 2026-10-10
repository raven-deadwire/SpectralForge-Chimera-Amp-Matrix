#pragma once
#include "CabExpansionModel.h"
#include <bit>
#include <cstdint>

namespace spectralforge::cabQuadratureReference {
using originalCab::Complex;
using originalCab::Speaker;
using originalCab::Point;
using originalCab::pi;
using originalCab::soundSpeed;
using originalCab::propagation;

// Independent, uncached cone evaluators copied from source revision
// 0f46f9f6eba7bea38ff256e3a5eafb1ca83b7065. Preserve these original
// expressions and accumulation order; do not use the production geometry cache.
inline Complex originalConeField(double hz,const Speaker& d,Point driver,Point mic,double z) {
    const double coherentRadius=d.radius/std::sqrt(1+std::pow(hz/1700,2));
    Complex field{};
    for(int ring=0;ring<4;++ring)for(int sector=0;sector<12;++sector) {
        const double r=coherentRadius*std::sqrt((ring+.5)/4),theta=2*pi*(sector+.5*(ring%2))/12;
        const double dx=mic.x-driver.x-r*std::cos(theta),dy=mic.y-driver.y-r*std::sin(theta);
        const double path=std::sqrt(dx*dx+dy*dy+z*z);
        field+=.10/path*propagation(hz,path)/48.0;
    }
    return field;
}

inline Complex expandedConeField(double hz,const Speaker& d,Point source,Point pickup,double z,
                                 double coherence,const cabExpansion::Mic* mic) {
    const double radius=d.radius/std::sqrt(1+std::pow(hz/coherence,2));
    Complex field{};
    for(int ring=0;ring<4;++ring)for(int sector=0;sector<12;++sector) {
        const double r=radius*std::sqrt((ring+.5)/4),theta=2*pi*(sector+.5*(ring%2))/12;
        const double dx=pickup.x-source.x-r*std::cos(theta),dy=pickup.y-source.y-r*std::sin(theta);
        const double path=std::sqrt(dx*dx+dy*dy+z*z),cosine=z/path;
        double pickupGain=1;
        if(mic) {
            const double angleSine=std::sqrt(std::max(0.,1-cosine*cosine));
            const double aperture=2*pi*hz*mic->aperture*angleSine/soundSpeed;
            pickupGain=(mic->pressure+(1-mic->pressure)*cosine)/std::sqrt(1+aperture*aperture);
        }
        field+=pickupGain*.10/path*propagation(hz,path)/48.;
    }
    return field;
}

inline bool sameBits(double a,double b) {
    return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);
}
inline bool sameBits(Complex a,Complex b) {
    return sameBits(a.real(),b.real()) && sameBits(a.imag(),b.imag());
}
}
