#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace spectralforge::originalCab {
// Immutable v1 authored designs. No measured response, third-party IR, fitting,
// or ML data. Keep v1 equations and definitions for session reproducibility.
constexpr double pi=3.14159265358979323846, soundSpeed=343.0;
constexpr int generatorVersion=1;
// Worker-only cooperative cancellation. A cancelled calculation is never an
// empty/invalid response: the distinct exception lets its owner discard all
// temporary state without reporting an IR error or publishing a partial kernel.
struct GenerationCancelled final : std::exception {
    const char* what() const noexcept override {return "CAB response generation cancelled";}
};
struct GenerationCancellation {
    const void* context{};
    bool (*requested)(const void*){};
    void checkpoint() const {
        if(requested && requested(context))throw GenerationCancelled{};
    }
};
struct Settings {
    bool enabled{};
    int cabinet{}, rear{}, mic{}, unit{};
    double tweeter{}, position{.25}, distanceCm{10};
};
inline double bounded(double v,double lo,double hi,double fallback) {
    return std::isfinite(v) ? std::clamp(v,lo,hi) : fallback;
}
// One lock-free value transfers a complete, quantized request. 1/1000 radius,
// 1 mm distance and 1% tweeter steps are explicit host/control resolutions.
inline uint64_t key(Settings s) {
    return uint64_t(s.enabled) | (uint64_t(std::clamp(s.cabinet,0,1))<<1)
        | (uint64_t(std::clamp(s.rear,0,1))<<2) | (uint64_t(std::clamp(s.mic,0,2))<<3)
        | (uint64_t(std::clamp(s.unit,0,3))<<5)
        | (uint64_t(std::lround(bounded(s.tweeter,0,1,0)*100))<<7)
        | (uint64_t(std::lround(bounded(s.position,0,1,.25)*1000))<<14)
        | (uint64_t(std::lround(bounded(s.distanceCm,2,60,10)*10))<<24);
}
inline Settings settings(uint64_t k) {
    return {bool(k&1),int((k>>1)&1),int((k>>2)&1),int((k>>3)&3),int((k>>5)&3),
        double((k>>7)&127)/100,double((k>>14)&1023)/1000,double((k>>24)&1023)/10};
}
struct Speaker {
    double radius, resonance, q, inductiveHz, breakupHz, breakupQ, breakupAmount;
};
struct Enclosure { double width,height,depth,volume,vasPerDriver,modeHz,modeQ,modeAmount; };
struct Microphone { double lowHz,highHz,bodyHz,bodyQ,bodyAmount,gradient; };
constexpr std::array<Speaker,2> speakers{{
    {.132,76,.69,4700,2550,2.4,.48}, {.106,46,.60,3600,1550,1.65,.20}}};
constexpr std::array<Enclosure,2> enclosures{{
    {.74,.76,.36,.155,.028,185,2.1,.11}, {.62,.64,.40,.125,.035,132,1.7,.08}}};
// Authored roles, not brand emulations: attack dynamic, body ribbon, detail condenser.
constexpr std::array<Microphone,3> microphones{{
    {62,12500,3800,1.15,.55,.45}, {28,8500,680,.65,.16,1.0}, {22,19000,7200,.85,.10,.28}}};
using Complex=std::complex<double>;
inline Complex low(Complex s,double hz) { return 1.0/(1.0+s/(2*pi*hz)); }
inline Complex high2(Complex s,double hz,double q) { const auto w=s/(2*pi*hz);return w*w/(1.0+w/q+w*w); }
inline Complex mode(Complex s,double hz,double q) {const auto w=s/(2*pi*hz);return (w/q)/(1.0+w/q+w*w);}
inline Complex propagation(double hz,double metres) {return std::polar(1.0,-2*pi*hz*metres/soundSpeed);}
inline Complex speakerResponse(Complex s,const Speaker& d,const Enclosure& box,bool open) {
    // Closed-box compliance raises fc and Q by sqrt(1 + total Vas/Vb).
    const double load=open ? 1.0 : std::sqrt(1.0+4*box.vasPerDriver/box.volume);
    return high2(s,d.resonance*load,d.q*load)*low(s,d.inductiveHz)*low(s,d.inductiveHz)
        *(1.0+d.breakupAmount*mode(s,d.breakupHz,d.breakupQ));
}
inline Complex microphoneResponse(Complex s,const Microphone& m,double distance) {
    // Bounded authored pressure-gradient proximity term; no actual mic measurement.
    return high2(s,m.lowHz,.707)*low(s,m.highHz)
        *(1.0+m.bodyAmount*mode(s,m.bodyHz,m.bodyQ))
        *(1.0+m.gradient*.10/(distance+.06)*low(s,180));
}
struct Point {double x,y;};
inline std::array<Point,4> centres(const Enclosure& box) {
    return {{{-box.width*.245,box.height*.245},{box.width*.245,box.height*.245},
             {-box.width*.245,-box.height*.245},{box.width*.245,-box.height*.245}}};
}
// Reduced-order radiating surface: equal-area quadrature over each cone with
// geometric spreading and propagation phase. High-frequency breakup uses an
// authored shrinking coherent radius, NOT a rigid piston accuracy claim.
inline Complex coneField(double hz,const Speaker& d,Point driver,Point mic,double z) {
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
inline Complex response(Settings raw,double hz) {
    const auto p=settings(key(raw));
    const auto& d=speakers[size_t(p.cabinet)];const auto& box=enclosures[size_t(p.cabinet)];
    const auto points=centres(box);const auto selected=points[size_t(p.unit)];
    const Point mic{selected.x+p.position*d.radius,selected.y};const double z=p.distanceCm*.01;
    const Complex s{0,2*pi*hz};Complex field{};
    for(const auto& driver:points) {
        auto front=coneField(hz,d,driver,mic,z);
        if(p.rear) {
            // Inverted rear radiation diffracts around the nearest cabinet edge.
            // Explicit low-passed delayed image path, not room reverb or an EQ switch.
            const double edge=std::min(box.width*.5-std::abs(driver.x),box.height*.5-std::abs(driver.y));
            const double path=std::sqrt(std::pow(mic.x-driver.x,2)+std::pow(mic.y-driver.y,2)+z*z)+2*edge+box.depth;
            front-=.10/path*propagation(hz,path)*low(s,900);
        }
        field+=front;
    }
    auto result=speakerResponse(s,d,box,p.rear!=0)*field*(1.0+box.modeAmount*mode(s,box.modeHz,box.modeQ));
    // Optional central tweeter, independent crossover and propagation path.
    const double hornPath=std::sqrt(mic.x*mic.x+mic.y*mic.y+z*z);
    result+=p.tweeter*.35*high2(s,3300,.707)*low(s,17000)*(.10/hornPath)
        *std::pow(z/hornPath,1.5)*propagation(hz,hornPath);
    return .45*result*microphoneResponse(s,microphones[size_t(p.mic)],z);
}
// Standalone deterministic radix-2 inverse transform; worker only.
inline void inverseFFT(std::vector<Complex>& a) {
    const size_t n=a.size();
    for(size_t i=1,j=0;i<n;++i) {size_t bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(a[i],a[j]);}
    for(size_t len=2;len<=n;len<<=1) {
        const auto step=std::polar(1.0,2*pi/double(len));
        for(size_t i=0;i<n;i+=len) {Complex w{1,0};for(size_t j=0;j<len/2;++j) {
            const auto u=a[i+j],v=a[i+j+len/2]*w;a[i+j]=u+v;a[i+j+len/2]=u-v;w*=step;
        }}
    }
    for(auto& x:a)x/=double(n);
}
inline std::vector<float> generate(Settings p,double rate,const GenerationCancellation& cancellation={}) {
    if(!std::isfinite(rate) || rate<8000 || rate>384000)throw std::invalid_argument("Original CAB sample rate");
    cancellation.checkpoint();
    // >= 170 ms captures LF damping; 85 ms output + tail taper, no onset trimming.
    size_t fftSize=1;while(fftSize<size_t(std::ceil(rate*.170)))fftSize<<=1;
    std::vector<Complex> spectrum(fftSize);
    for(size_t k=1;k<fftSize/2;++k) {
        if((k&63)==0)cancellation.checkpoint();
        const double hz=double(k)*rate/double(fftSize);
        // Smooth anti-alias boundary in physical frequency before sampling.
        const double taper=hz>rate*.40 ? .5+.5*std::cos(pi*(hz/rate-.40)/.10) : 1;
        spectrum[k]=response(p,hz)*taper;spectrum[fftSize-k]=std::conj(spectrum[k]);
    }
    cancellation.checkpoint();
    inverseFFT(spectrum);
    std::vector<float> samples(size_t(std::ceil(rate*.085)));
    for(size_t n=0;n<samples.size();++n) {
        const double end=double(n)/double(samples.size());
        const double taper=end>.8 ? .5+.5*std::cos(pi*(end-.8)/.2) : 1;
        samples[n]=float(spectrum[n].real()*taper);
    }
    cancellation.checkpoint();
    return samples;
}
}
