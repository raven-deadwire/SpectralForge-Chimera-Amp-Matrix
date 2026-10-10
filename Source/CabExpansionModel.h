#pragma once
#include "OriginalCabModel.h"

namespace spectralforge::cabExpansion {
using namespace originalCab;
// Version 2 is an independently authored role library, not fitted hardware.
// Keep originalCab v1 and the low 34 request bits unchanged for old sessions.
constexpr uint64_t versionBit=uint64_t{1}<<46;
struct Driver {
    const char* id; const char* name; const char* reference;
    bool bass; int inches;
    Speaker speaker;
    double vas, coherenceHz, notchHz, notchQ, notchDepth;
};
inline constexpr std::array<Driver,14> drivers{{
    {"ember-30-v2","Ember 30","Vintage 30",false,12,{.132,77,.68,4900,2950,2.1,.63},.029,1750,5600,1.4,.30},
    {"steel-75-v2","Steel 75","G12T-75",false,12,{.132,82,.61,5500,3900,2.0,.41},.025,2000,1150,.8,.34},
    {"verdant-25-v2","Verdant 25","G12M-25",false,12,{.132,72,.76,3900,2350,1.8,.55},.033,1450,4800,1.2,.38},
    {"granite-55-v2","Granite 55","G12H 55 Hz",false,12,{.132,58,.65,4400,2700,2.3,.46},.037,1610,6100,1.8,.22},
    {"silver-12-v2","Silver 12","Jensen clean ceramic",false,12,{.132,88,.58,6300,3400,1.0,.25},.026,2280,1850,1.3,.18},
    {"carnivore-12-v2","Crimson 12","Eminence Karnivore",false,12,{.132,66,.59,4700,1950,1.65,.70},.031,1530,4200,1.6,.43},
    {"raven-100-v2","Nocturne 100","Celestion G12-100 Raven",false,12,{.132,71,.55,5800,3200,1.55,.52},.024,2120,6500,2.0,.28},
    {"ruin-12-v2","Chimera Ruin 12","Chimera original",false,12,{.132,61,.53,5050,1650,1.45,.83},.028,1820,4700,1.7,.49},
    {"foundry-10-v2","Foundry 10","SVT sealed 10-inch",true,10,{.106,49,.66,3200,1150,1.7,.31},.036,1320,4100,1.2,.35},
    {"vector-10-v2","Vector 10","Modern neo 10-inch",true,10,{.106,43,.52,4500,1950,1.5,.29},.031,1900,6200,1.8,.24},
    {"clarity-12-v2","Clarity 12","Glockenklang / Vanderkley 12-inch",true,12,{.132,39,.51,4200,1250,.9,.12},.044,1750,5200,1.3,.16},
    {"alloy-10-v2","Alloy 10","Hartke hybrid 10-inch",true,10,{.106,47,.57,5400,2600,2.7,.45},.029,2300,4400,2.2,.30},
    {"monolith-15-v2","Monolith 15","Large 15-inch bass",true,15,{.168,35,.62,2500,820,1.35,.26},.074,1100,3100,1.1,.40},
    {"depth-12-v2","Chimera Depth 12","Chimera original",true,12,{.132,37,.49,3850,1550,1.3,.38},.040,1560,4600,1.5,.32}
}};
struct Mic {
    const char* id; const char* name; const char* catalogId;
    int kind; // dynamic / ribbon / condenser
    Microphone response;
    double pressure, aperture, presenceHz, presenceQ, presenceAmount;
};
// Pressure + pressure-gradient pickup, aperture averaging and two independent
// resonances distinguish each design, including its spatial/proximity response.
inline constexpr std::array<Mic,20> microphones{{
    {"needle-57-v2","Needle 57","dynamic-57",0,{64,12200,4200,1.25,.62,.45},.50,.010,6700,1.4,.20},
    {"hammer-421-v2","Hammer 421","dynamic-421",0,{35,15500,2300,.85,.38,.59},.50,.013,5100,1.1,.25},
    {"spear-441-v2","Spear 441","dynamic-441",0,{31,17400,3600,1.1,.25,.30},.37,.009,8500,1.6,.15},
    {"edge-906-v2","Edge 906","dynamic-906",0,{47,14600,4700,1.3,.47,.38},.37,.011,1700,1.0,-.12},
    {"anchor-20-v2","Anchor 20","dynamic-20",0,{26,14000,1600,.75,.16,.09},.50,.015,4200,1.0,.15},
    {"veil-7-v2","Veil 7","dynamic-7",0,{38,11800,2800,.9,.19,.51},.50,.014,6100,1.2,-.19},
    {"pin-201-v2","Pin 201","dynamic-201",0,{39,16900,5200,1.1,.29,.27},.25,.007,2200,1.2,.11},
    {"fang-88-v2","Fang 88","dynamic-88",0,{25,15100,3100,1.0,.40,.78},.25,.012,6200,1.7,.24},
    {"depth-112-v2","Depth 112","dynamic-112",0,{24,10200,110,.95,.54,.28},.50,.017,4100,1.8,.56},
    {"silk-121-v2","Silk 121","ribbon-121",1,{25,9200,720,.70,.20,1.0},.00,.020,4500,.8,-.18},
    {"focus-160-v2","Focus 160","ribbon-160",1,{36,12100,1650,1.05,.26,.65},.25,.010,6200,1.2,.15},
    {"velvet-4038-v2","Velvet 4038","ribbon-4038",1,{22,7300,420,.8,.28,1.12},.00,.024,3400,.85,-.21},
    {"halo-87-v2","Halo 87","condenser-87",2,{23,18700,6700,.9,.24,.39},.50,.014,2100,1.0,.08},
    {"prism-414-v2","Prism 414","condenser-414",2,{21,20500,8200,1.1,.19,.25},.50,.013,3100,.9,.13},
    {"pencil-184-v2","Pencil 184","condenser-184",2,{30,21200,9100,1.25,.30,.18},.50,.006,3800,1.1,.06},
    {"titan-47-v2","Titan 47 FET","condenser-47-fet",2,{20,17400,240,.65,.25,.56},.50,.016,4800,1.0,.18},
    {"crystal-4050-v2","Crystal 4050","condenser-4050",2,{21,20000,5600,.85,.10,.22},.50,.012,10500,1.0,.08},
    {"copper-201-v2","Copper 201 FET","condenser-201-fet",2,{25,17700,320,.7,.16,.44},.50,.014,7200,1.2,.28},
    {"ember-67-v2","Ember 67","condenser-67",2,{27,15700,900,.8,.17,.49},.50,.016,6100,.9,.16},
    {"chimera-strike-v2","Chimera Strike","chimera-strike",2,{24,19300,3900,1.1,.37,.17},.37,.009,9200,1.4,-.16}
}};
struct Settings {originalCab::Settings base; int driver{},mic{},tweeter{};};
inline uint64_t key(Settings p) {
    const auto d=std::clamp(p.driver,0,int(drivers.size()));
    const auto m=std::clamp(p.mic,0,int(microphones.size()));
    const auto t=std::clamp(p.tweeter,0,3);
    const auto v1=originalCab::key(p.base);
    return d || m || t ? v1 | versionBit | (uint64_t(d)<<34) | (uint64_t(m)<<38) | (uint64_t(t)<<43) : v1;
}
inline Settings settings(uint64_t k) {
    return {originalCab::settings(k),int((k>>34)&15),int((k>>38)&31),int((k>>43)&3)};
}
inline const Driver* driver(Settings p) {return p.driver>0 && p.driver<=int(drivers.size()) ? &drivers[size_t(p.driver-1)] : nullptr;}
inline const Mic* microphone(Settings p) {return p.mic>0 && p.mic<=int(microphones.size()) ? &microphones[size_t(p.mic-1)] : nullptr;}
inline bool isBass(Settings p) {return driver(p) ? driver(p)->bass : p.base.cabinet!=0;}
inline int diameter(Settings p) {return driver(p) ? driver(p)->inches : p.base.cabinet ? 10 : 12;}
inline Enclosure enclosure(Settings p) {
    if(const auto* d=driver(p)) {
        const double scale=double(d->inches)/(d->bass ? 10. : 12.);
        auto box=enclosures[size_t(d->bass)];
        box.width*=scale;box.height*=scale;box.depth*=std::sqrt(scale);
        box.volume*=scale*scale*std::sqrt(scale);box.vasPerDriver=d->vas;
        box.modeHz/=scale;return box;
    }
    return enclosures[size_t(p.base.cabinet)];
}
inline Complex coneField(double hz,const Speaker& d,Point source,Point pickup,double z,
                         double coherence,const Mic* mic) {
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
inline Complex response(Settings raw,double hz) {
    const auto p=settings(key(raw));
    if(!p.driver && !p.mic && !p.tweeter)return originalCab::response(p.base,hz);
    const auto* chosen=driver(p);const auto* mic=microphone(p);
    const auto& d=chosen ? chosen->speaker : speakers[size_t(p.base.cabinet)];
    const auto box=enclosure(p);const auto points=centres(box);const auto selected=points[size_t(p.base.unit)];
    const Point pickup{selected.x+p.base.position*d.radius,selected.y};
    const double z=p.base.distanceCm*.01;const Complex s{0,2*pi*hz};Complex field{};
    for(const auto source:points) {
        auto front=coneField(hz,d,source,pickup,z,chosen ? chosen->coherenceHz : 1700,mic);
        if(p.base.rear) {
            const double edge=std::min(box.width*.5-std::abs(source.x),box.height*.5-std::abs(source.y));
            const double path=std::hypot(std::hypot(pickup.x-source.x,pickup.y-source.y),z)+2*edge+box.depth;
            // Forward-arriving edge-diffracted image. No invented room reverb.
            front-=.10/path*propagation(hz,path)*low(s,900)*(mic ? mic->pressure+.5*(1-mic->pressure) : 1);
        }
        field+=front;
    }
    auto result=speakerResponse(s,d,box,p.base.rear!=0)*field*(1.0+box.modeAmount*mode(s,box.modeHz,box.modeQ));
    if(chosen)result*=1.0-chosen->notchDepth*mode(s,chosen->notchHz,chosen->notchQ);
    const double hornPath=std::hypot(std::hypot(pickup.x,pickup.y),z);
    constexpr std::array<double,4> crossover{3300,4100,2900,4800}, rolloff{17000,15000,18500,22000}, focus{1.5,1.1,2.1,.7};
    const auto t=size_t(p.tweeter);
    double hornPickup=1;
    if(mic) {
        const auto cosine=z/hornPath,aperture=2*pi*hz*mic->aperture*std::sqrt(std::max(0.,1-cosine*cosine))/soundSpeed;
        hornPickup=(mic->pressure+(1-mic->pressure)*cosine)/std::sqrt(1+aperture*aperture);
    }
    result+=p.base.tweeter*.35*high2(s,crossover[t],.707)*low(s,rolloff[t])*(.10/hornPath)
        *std::pow(z/hornPath,focus[t])*hornPickup*propagation(hz,hornPath);
    auto micResponse=microphoneResponse(s,mic ? mic->response : originalCab::microphones[size_t(p.base.mic)],z);
    if(mic)micResponse*=1.0+mic->presenceAmount*mode(s,mic->presenceHz,mic->presenceQ);
    return .45*result*micResponse;
}
inline std::vector<float> generate(uint64_t model,double rate,const GenerationCancellation& cancellation={}) {
    if(!(model&versionBit))return originalCab::generate(originalCab::settings(model),rate,cancellation);
    if(!std::isfinite(rate) || rate<8000 || rate>384000)throw std::invalid_argument("Expanded CAB sample rate");
    cancellation.checkpoint();
    const auto p=settings(model);
    size_t size=1;while(size<size_t(std::ceil(rate*.170)))size<<=1;
    std::vector<Complex> spectrum(size);
    for(size_t k=1;k<size/2;++k) {
        if((k&63)==0)cancellation.checkpoint();
        const double hz=double(k)*rate/double(size);
        const double taper=hz>rate*.40 ? .5+.5*std::cos(pi*(hz/rate-.40)/.10) : 1;
        spectrum[k]=response(p,hz)*taper;spectrum[size-k]=std::conj(spectrum[k]);
    }
    cancellation.checkpoint();
    inverseFFT(spectrum);std::vector<float> samples(size_t(std::ceil(rate*.085)));
    for(size_t n=0;n<samples.size();++n) {
        const double end=double(n)/double(samples.size());
        samples[n]=float(spectrum[n].real()*(end>.8 ? .5+.5*std::cos(pi*(end-.8)/.2) : 1));
    }
    cancellation.checkpoint();
    return samples;
}
}
