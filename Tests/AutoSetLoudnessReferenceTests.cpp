#include "AutoSetLoudness.h"
#include <ebur128.h>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#ifdef CHIMERA_BASELINE_LOUDNESS
#include "BaselineLoudness.h"
#endif
using namespace spectralforge::autoset;

int main() {
    int major,minor,patch;ebur128_get_version(&major,&minor,&patch);
    std::cerr<<"libebur128 "<<major<<'.'<<minor<<'.'<<patch<<'\n';
    std::cout<<"rate,signal,layout,reference_lufs,chimera_lufs,error_lu\n";
    double maximum=0;int cases=0,failures=0;
    for(const int rate:{44100,48000,96000})
    for(const char* signal:{"997Hz","27.5Hz","bass_harmonics","noise","picking","single_attack","gated","silence"})
    for(int layout=0;layout<6;++layout) {
        const char* layouts[]{"mono","dual_mono","left_only","antiphase","uncorrelated","unequal"};
        std::mt19937 random(7301);std::uniform_real_distribution<float> noise(-1,1);
        std::vector<OutputFrame> frames(size_t(rate)*6);
        std::vector<float> interleaved;const unsigned channels=layout==0?1u:2u;
        interleaved.reserve(frames.size()*channels);
        for(size_t n=0;n<frames.size();++n) {
            const double t=double(n)/rate,tau=2*std::numbers::pi*t;
            const std::string name(signal);double x=0;
            if(name=="997Hz")x=.1*std::sin(997*tau);
            if(name=="27.5Hz")x=.3*std::sin(27.5*tau);
            if(name=="bass_harmonics")x=.22*std::sin(27.5*tau)+.06*std::sin(55*tau)+.025*std::sin(110*tau);
            if(name=="noise")x=.05*noise(random);
            if(name=="picking")x=.65*std::sin(220*tau)*std::exp(-std::fmod(t,.5)*65);
            if(name=="single_attack" && t>.7 && t<.72)x=.8*std::sin(440*tau);
            if(name=="gated")x=(t<1?.000001:t<2?.04:t<4?.0001:.12)*std::sin(997*tau);
            auto& frame=frames[n];frame[0]=float(x);
            frame[1]=layout==0 || layout==2?0.f:layout==3?-float(x):layout==4?.03f*noise(random):layout==5?.17f*float(x):float(x);
            interleaved.push_back(frame[0]);if(channels==2)interleaved.push_back(frame[1]);
        }
        auto* ref=ebur128_init(channels,static_cast<unsigned long>(rate),EBUR128_MODE_I);
        if(!ref || ebur128_add_frames_float(ref,interleaved.data(),frames.size())!=EBUR128_SUCCESS)throw std::runtime_error("reference setup failed");
        double expected;const auto status=ebur128_loudness_global(ref,&expected);ebur128_destroy(&ref);
        if(status!=EBUR128_SUCCESS)throw std::runtime_error("reference measurement failed");
#ifdef CHIMERA_BASELINE_LOUDNESS
        std::vector<float> mean;for(auto f:frames)mean.push_back(channels==1?f[0]:.5f*(f[0]+f[1]));
        const auto actual=baseline::integratedLoudness(mean,rate);
#else
        const auto actual=integratedLoudness(frames,rate);
#endif
        double error=actual && std::isfinite(expected)?std::abs(*actual-expected):0;
        const bool ok=std::isfinite(expected)?actual && error<.02:!actual;
        maximum=std::max(maximum,error);++cases;if(!ok)++failures;
        std::cout<<rate<<','<<signal<<','<<layouts[layout]<<','<<std::setprecision(10)<<expected<<',';
        if(actual)std::cout<<*actual;else std::cout<<"unmeasured";
        std::cout<<','<<error<<'\n';
    }
    std::cerr<<cases<<" cases; "<<failures<<" failures; max finite error "<<maximum<<" LU\n";
    return failures?1:0;
}
