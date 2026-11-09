#pragma once
#include "AutoSetCab.h"

namespace spectralforge::autoset {

// Phase 23: the factory cabinet IR of the heard mic (V30 / SM57 or Jensen /
// SM57). Phase 9 places a modeled mic; a mic that plays a factory IR has no
// placement, but the other factory IR may suit the style better. The pass
// hears the rig alone (Tap::cab), and an IR is a fixed filter in series, so
// the other IR predicts what would be heard: the heard bands plus the other
// IR's bands minus the current IR's, each IR level-normalised as the cabinet
// loads it.
// - The other IR is chosen when its prediction sits closer to the Tone EQ
//   target by more than 0.5 dB (as phase 9). Applied whole: an IR is a choice.
// - Bass is kept: both factory IRs are guitar cabinets.
// - The level is not touched; the change the heard playing would get is
//   reported (the Final EQ + output step sets the level).
struct CabIRResult {
    bool playingDetected{},bass{};
    int before{},proposed{};                 // cabinet source: 1 V30 / SM57, 2 Jensen / SM57
    float toneErrorBeforeDb{},toneErrorAfterDb{},levelChangeDb{};
    std::array<float,toneBandCount> moveDb{}; // what the switch adds to each band
    bool changed() const noexcept {return proposed!=before;}
};
inline constexpr float cabIRMinGainDb=.5f;
inline const char* cabIRName(int source) noexcept {return source==1?"V30 / SM57":source==2?"Jensen / SM57":source==3?"User IR":"Filters only";}
// Third-octave band power of an IR scaled as juce::dsp::Convolution's
// Normalise::yes loads it (0.125 / sqrt of its loudest channel's energy),
// from its first channel at its own rate.
inline std::array<float,toneBandCount> cabIRBandsDb(const juce::AudioBuffer<float>& ir,double irRate) {
    std::array<float,toneBandCount> bands{};bands.fill(-300.f);
    if(ir.getNumChannels()<1 || ir.getNumSamples()<1 || !std::isfinite(irRate) || irRate<8000)return bands;
    double loudest=0;
    for(int c=0;c<ir.getNumChannels();++c){double e=0;for(int n=0;n<ir.getNumSamples();++n)e+=double(ir.getSample(c,n))*ir.getSample(c,n);loudest=std::max(loudest,e);}
    if(loudest<=0)return bands;
    const double scale=.125/std::sqrt(loudest);
    int order=12;while((1<<order)<std::max(ir.getNumSamples(),int(irRate)) && order<18)++order;
    const int size=1<<order;
    juce::dsp::FFT fft(order);
    std::vector<float> bins(size_t(2*size));
    for(int n=0;n<std::min(size,ir.getNumSamples());++n)bins[size_t(n)]=float(scale*ir.getSample(0,n));
    fft.performRealOnlyForwardTransform(bins.data());
    for(int b=0;b<toneBandCount;++b) {
        const double fc=toneBandCentres[size_t(b)],lo=fc*std::pow(2.,-1./6.),hi=std::min(fc*std::pow(2.,1./6.),irRate/2);
        double power=0;int count=0;
        for(int k=std::max(1,int(std::ceil(lo*size/irRate)));k<=size/2 && k*irRate/size<hi;++k){power+=std::norm(std::complex<double>(bins[size_t(2*k)],bins[size_t(2*k+1)]));++count;}
        if(count>0)bands[size_t(b)]=float(10*std::log10(std::max(power/count,1e-30)));
    }
    return bands;
}
// post: the cabinet output; source: the heard mic's source (1 or 2);
// factoryBands: cabIRBandsDb of the two factory IRs (V30, Jensen).
inline CabIRResult analyseCabIR(const std::vector<float>& post,double rate,int source,const std::array<std::array<float,toneBandCount>,2>& factoryBands,
                                std::optional<bool> bass={},Style style=Style::rock) {
    CabIRResult r;r.before=r.proposed=source;
    if(source!=1 && source!=2)return r;
    const auto heard=measureSpectrum(post,rate,bass);
    r.playingDetected=heard.playing;
    if(!r.playingDetected)return r;
    r.bass=heard.bass;
    r.toneErrorBeforeDb=r.toneErrorAfterDb=ampToneErrorDb(heard,style);
    if(r.bass)return r;
    const int other=3-source;
    const auto& now=factoryBands[size_t(source-1)];const auto& next=factoryBands[size_t(other-1)];
    std::array<float,toneBandCount> move{};
    for(int b=0;b<toneBandCount;++b)move[size_t(b)]=now[size_t(b)]>-299.f && next[size_t(b)]>-299.f?next[size_t(b)]-now[size_t(b)]:0.f;
    std::array<float,toneBandCount> zero{};
    const float e=ampToneErrorDb(cabPredicted(heard,zero,move),style);
    if(e<r.toneErrorBeforeDb-cabIRMinGainDb) {
        r.proposed=other;r.toneErrorAfterDb=e;r.moveDb=move;
        double before=0,after=0;
        for(int b=0;b<toneBandCount;++b){const double p=std::pow(10.,heard.bandsDb[size_t(b)]/10.);before+=p;after+=p*std::pow(10.,move[size_t(b)]/10.);}
        r.levelChangeDb=before>0 && after>0?float(10*std::log10(after/before)):0.f;
    }
    return r;
}
} // namespace spectralforge::autoset
