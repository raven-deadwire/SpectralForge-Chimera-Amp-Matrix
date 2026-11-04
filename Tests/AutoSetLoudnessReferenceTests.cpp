#include "AutoSetLoudness.h"
#include <ebur128.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#ifdef CHIMERA_BASELINE_LOUDNESS
#include "BaselineLoudness.h"
#endif
using namespace spectralforge::autoset;

namespace {
struct ReferenceApi {
    decltype(&ebur128_init) init=ebur128_init;
    decltype(&ebur128_add_frames_float) add=ebur128_add_frames_float;
    decltype(&ebur128_loudness_global) measure=ebur128_loudness_global;
    decltype(&ebur128_destroy) destroy=ebur128_destroy;
};
double referenceLoudness(unsigned channels,unsigned long rate,const float* samples,size_t frames,
                         const ReferenceApi& api={}) {
    const auto release=[destroy=api.destroy](ebur128_state* state) noexcept {destroy(&state);};
    std::unique_ptr<ebur128_state,decltype(release)> ref(api.init(channels,rate,EBUR128_MODE_I),release);
    if(!ref)throw std::runtime_error("reference initialization failed");
    if(api.add(ref.get(),samples,frames)!=EBUR128_SUCCESS)throw std::runtime_error("reference frame addition failed");
    double expected;
    if(api.measure(ref.get(),&expected)!=EBUR128_SUCCESS)throw std::runtime_error("reference measurement failed");
    return expected;
}

// Fault injection uses real libebur128 allocations and destruction. This
// checks the same owner as the 144 comparisons, including exception unwinding.
int destroyed=0,added=0,measured=0;
void countedDestroy(ebur128_state** state) {++destroyed;ebur128_destroy(state);}
int countedAdd(ebur128_state* state,const float* samples,size_t frames) {
    ++added;return ebur128_add_frames_float(state,samples,frames);
}
int countedMeasure(ebur128_state* state,double* value) {++measured;return ebur128_loudness_global(state,value);}
void referenceOwnership() {
    const std::vector<float> silence(19200,0.f);
    for(int failure=0;failure<4;++failure) {
        destroyed=added=measured=0;
        ReferenceApi api;api.destroy=countedDestroy;api.add=countedAdd;api.measure=countedMeasure;
        if(failure==1)api.init=[](unsigned,unsigned long,int)->ebur128_state* {return nullptr;};
        if(failure==2)api.add=[](ebur128_state*,const float*,size_t)->int {++added;return EBUR128_ERROR_NOMEM;};
        if(failure==3)api.measure=[](ebur128_state*,double*)->int {++measured;return EBUR128_ERROR_INVALID_MODE;};
        std::string error;
        try {
            const auto value=referenceLoudness(1,48000,silence.data(),silence.size(),api);
            if(value!=-std::numeric_limits<double>::infinity())throw std::runtime_error("silent reference was not negative infinity");
        } catch(const std::runtime_error& e) {error=e.what();}
        const char* expected[]{"","reference initialization failed","reference frame addition failed","reference measurement failed"};
        if(error!=expected[failure] || destroyed!=(failure==1?0:1)
           || added!=(failure==1?0:1) || measured!=((failure==0 || failure==3)?1:0))
            throw std::runtime_error("reference ownership regression failed at case "+std::to_string(failure));
    }
    std::cerr<<"PASS reference ownership: success, initialization failure, frame failure, measurement failure (4 cases)\n";
}
}

int main(int argc,char** argv) {
    referenceOwnership();
    if(argc==2 && std::string(argv[1])=="--failure-paths")return 0;
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
        const double expected=referenceLoudness(channels,static_cast<unsigned long>(rate),interleaved.data(),frames.size());
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
