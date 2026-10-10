#pragma once
#include "CabExpansionModel.h"

namespace spectralforge::cabLayout {
using namespace originalCab;
// Authored v3 templates. Dimensions in metres, net volume in m^3.
// Values describe our designs, not measurements of the reference products.
// Release 1.3 replaces the unshipped 8x10 preview at ordinal 7 with a 6x10.
struct Layout {
    const char* id; const char* name;
    int columns,rows,inches; bool bass;
    double width,height,depth,volume;
};
inline constexpr std::array<Layout,9> layouts{{
    {"g112-v3","Guitar 1x12",1,1,12,false,.48,.48,.30,.050},
    {"g212-v3","Guitar 2x12",2,1,12,false,.78,.49,.32,.088},
    {"g412-v3","Guitar 4x12",2,2,12,false,.74,.76,.36,.155},
    {"b115-v3","Bass 1x15",1,1,15,true,.56,.60,.43,.112},
    {"b210-v3","Bass 2x10",2,1,10,true,.61,.40,.37,.070},
    {"b410-v3","Bass 4x10",2,2,10,true,.62,.64,.40,.125},
    {"b610-v3","Bass 6x10",2,3,10,true,.63,.94,.40,.1785},
    {"b112-v3","Bass 1x12",1,1,12,true,.49,.52,.39,.072},
    {"b212-v3","Bass 2x12",2,1,12,true,.77,.49,.40,.118}
}};
constexpr uint64_t versionBit=uint64_t{1}<<54;
struct Settings {cabExpansion::Settings voice; int layout{},unit{};};
inline int layoutIndex(int index) {return std::clamp(index,0,int(layouts.size()));}
inline int count(int index) {
    if(!layoutIndex(index))return 4;
    const auto& l=layouts[size_t(layoutIndex(index)-1)];return l.columns*l.rows;
}
inline bool isBass(Settings p) {
    const auto index=layoutIndex(p.layout);
    return index ? layouts[size_t(index-1)].bass : p.voice.base.cabinet!=0;
}
inline int nominalInches(Settings p) {
    const auto index=layoutIndex(p.layout);
    return index ? layouts[size_t(index-1)].inches : isBass(p) ? 10 : 12;
}
inline bool isDriverCompatible(Settings p) {
    return p.voice.driver>=0 && p.voice.driver<=int(cabExpansion::drivers.size())
        && cabExpansion::isBass(p.voice)==isBass(p)
        && cabExpansion::diameter(p.voice)==nominalInches(p);
}
// Resolve a restored or automated mismatch without writing to host parameters.
// An authored cabinet is a fixed baffle: another speaker cannot enlarge its holes.
inline Settings effectiveSettings(Settings p) {
    p.layout=layoutIndex(p.layout);
    if(!isDriverCompatible(p)) {
        // The cabinet selector owns the family, including selector zero. A
        // driver automation change cannot switch its family or mounting size.
        const bool bass=isBass(p);const int inches=nominalInches(p);
        for(int driver=1;driver<=int(cabExpansion::drivers.size());++driver) {
            auto candidate=p;candidate.voice.driver=driver;
            if(cabExpansion::isBass(candidate.voice)==bass && cabExpansion::diameter(candidate.voice)==inches) {p.voice.driver=driver;break;}
        }
    }
    return p;
}
inline int effectiveUnit(Settings p) {
    const auto layout=layoutIndex(p.layout);
    auto unit=layout ? p.unit : p.voice.base.unit;
    // Preserve the column of the removed bottom row in 8x10 preview states.
    // Raw automation/state stays untouched; DSP and visuals share this mapping.
    if(layout==7 && unit>=6)unit=4+std::clamp(unit-6,0,1);
    return std::clamp(unit,0,count(layout)-1);
}
inline uint64_t key(Settings p) {
    p=effectiveSettings(p);
    const auto previous=cabExpansion::key(p.voice);
    // Valid four-unit requests retain their previous cache identity. Incompatible
    // stored drivers resolve to the fixed four-unit family's supported diameter.
    if(!p.layout)return previous;
    return previous | versionBit | (uint64_t(p.layout)<<47) | (uint64_t(effectiveUnit(p))<<51);
}
inline Settings settings(uint64_t k) {
    return effectiveSettings({cabExpansion::settings(k),k&versionBit ? layoutIndex(int((k>>47)&15)) : 0,int((k>>51)&7)});
}
struct Geometry {
    Enclosure box;
    std::array<Point,8> centres{};
    Point horn{};
    int count{};
    double radius{};
};
inline Geometry geometry(Settings p) {
    p=effectiveSettings(p);
    const auto* driver=cabExpansion::driver(p.voice);
    const auto& speaker=driver ? driver->speaker : speakers[size_t(p.voice.base.cabinet!=0)];
    Geometry g{};g.radius=speaker.radius;g.count=count(p.layout);
    if(!layoutIndex(p.layout)) {
        g.box=enclosures[size_t(isBass(p))];
        if(driver)g.box.vasPerDriver=driver->vas;
        const auto legacy=originalCab::centres(g.box);
        std::copy(legacy.begin(),legacy.end(),g.centres.begin());return g;
    }
    const auto& l=layouts[size_t(layoutIndex(p.layout)-1)];
    g.box={l.width,l.height,l.depth,l.volume,
        driver ? driver->vas : enclosures[size_t(p.voice.base.cabinet!=0)].vasPerDriver,0,1.6,0};
    const double pitch=l.inches*.0254*1.10;
    for(int row=0;row<l.rows;++row)for(int col=0;col<l.columns;++col)
        g.centres[size_t(row*l.columns+col)]={(col-(l.columns-1)*.5)*pitch,((l.rows-1)*.5-row)*pitch};
    // The four-driver bass baffle carries its horn between the cones. Both
    // drawing and acoustic propagation consume this same physical location.
    g.horn={0,l.bass && l.columns==2 && l.rows==2 ? 0. : g.box.height*.44};return g;
}
inline Complex driverTransfer(Settings p,double hz,const Geometry& g) {
    p=effectiveSettings(p);
    const auto* chosen=cabExpansion::driver(p.voice);
    const auto& d=chosen ? chosen->speaker : speakers[size_t(p.voice.base.cabinet!=0)];
    const Complex s{0,2*pi*hz};
    const double load=p.voice.base.rear ? 1. : std::sqrt(1.+g.count*g.box.vasPerDriver/g.box.volume);
    auto result=high2(s,d.resonance*load,d.q*load)*low(s,d.inductiveHz)*low(s,d.inductiveHz)
        *(1.+d.breakupAmount*mode(s,d.breakupHz,d.breakupQ));
    // Three damped axial enclosure modes; explicitly not a room reverberator.
    const double rearDamping=p.voice.base.rear ? .35 : 1.;
    result*=1.+rearDamping*(.045*mode(s,soundSpeed/(2*g.box.width),1.6)
        +.040*mode(s,soundSpeed/(2*g.box.height),1.5)+.035*mode(s,soundSpeed/(2*g.box.depth),1.4));
    if(chosen)result*=1.-chosen->notchDepth*mode(s,chosen->notchHz,chosen->notchQ);
    return result;
}
inline Complex response(Settings raw,double hz) {
    const auto p=settings(key(raw));
    if(!p.layout)return cabExpansion::response(p.voice,hz);
    const auto g=geometry(p);const auto& v=p.voice;const auto& b=v.base;
    const auto* chosen=cabExpansion::driver(v);const auto* mic=cabExpansion::microphone(v);
    const auto& d=chosen ? chosen->speaker : speakers[size_t(b.cabinet!=0)];
    const auto target=g.centres[size_t(effectiveUnit(p))];
    const Point pickup{target.x+b.position*g.radius,target.y};
    const double z=b.distanceCm*.01;const Complex s{0,2*pi*hz};Complex field{};
    for(int n=0;n<g.count;++n) {
        const auto source=g.centres[size_t(n)];
        auto ray=cabExpansion::coneField(hz,d,source,pickup,z,chosen ? chosen->coherenceHz : 1700,mic);
        if(b.rear) {
            const double edge=std::min(g.box.width*.5-std::abs(source.x),g.box.height*.5-std::abs(source.y));
            const double path=std::hypot(std::hypot(pickup.x-source.x,pickup.y-source.y),z)+2*edge+g.box.depth;
            ray-=.10/path*propagation(hz,path)*low(s,900)*(mic ? mic->pressure+.5*(1-mic->pressure) : 1);
        }
        field+=ray; // Coherent complex sum: interference is not an EQ preset.
    }
    auto result=driverTransfer(p,hz,g)*field;
    const double path=std::hypot(std::hypot(pickup.x-g.horn.x,pickup.y-g.horn.y),z),cosine=z/path;
    constexpr std::array<double,4> cross{3300,4100,2900,4800},roll{17000,15000,18500,22000},focus{1.5,1.1,2.1,.7};
    const auto t=size_t(v.tweeter);double gain=1;
    if(mic) {
        const double aperture=2*pi*hz*mic->aperture*std::sqrt(std::max(0.,1-cosine*cosine))/soundSpeed;
        gain=(mic->pressure+(1-mic->pressure)*cosine)/std::sqrt(1+aperture*aperture);
    }
    result+=b.tweeter*.35*high2(s,cross[t],.707)*low(s,roll[t])*(.10/path)*std::pow(cosine,focus[t])*gain*propagation(hz,path);
    auto pickupResponse=microphoneResponse(s,mic ? mic->response : originalCab::microphones[size_t(b.mic)],z);
    if(mic)pickupResponse*=1.+mic->presenceAmount*mode(s,mic->presenceHz,mic->presenceQ);
    return .45*result*pickupResponse;
}
inline std::vector<float> generate(uint64_t model,double rate,const GenerationCancellation& cancellation={}) {
    model=key(settings(model));
    if(!(model&versionBit))return cabExpansion::generate(model,rate,cancellation);
    if(!std::isfinite(rate) || rate<8000 || rate>384000)throw std::invalid_argument("Layout CAB sample rate");
    cancellation.checkpoint();
    const auto p=settings(model);size_t size=1;while(size<size_t(std::ceil(rate*.170)))size<<=1;
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
