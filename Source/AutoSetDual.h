#pragma once
#include "AutoSetAmp.h"

namespace spectralforge::autoset {

// Phase 13: the Dual routing's BLEND (how much of rig 2 is heard) or, with
// Dual CROSSOVER, the split frequency between the low rig and the high rig.
// The pass hears each rig's output on its own (Tap::lane1/lane2: after the
// rig's amp, cab and level, before the blend), so the result is checked
// against the Tone EQ target before anything is written:
// - Blend: the mix (1 - blend) * rig 1 + blend * rig 2 is computed sample by
//   sample, so the prediction is exact, cancellation between the rigs
//   included.
// - Crossover: the rigs' inputs change with the split, so the prediction is
//   linear: each rig's band powers are scaled by how much more or less of
//   its input a Linkwitz-Riley split at the new frequency passes, and the two
//   rigs add in power. Distortion makes this approximate, so the move is
//   limited to an octave either way.
// The Final EQ and output pass that follows restores the loudness.
struct DualResult {
    bool playingDetected{},bass{},crossover{};
    bool rig1Heard{},rig2Heard{};
    float before{},proposed{};                 // blend 0-1, or the crossover in Hz
    float toneErrorBeforeDb{},toneErrorAfterDb{};
    float levelChangeDb{};                      // the mix's 80 Hz-6.3 kHz level change
    float cancellationDb{};                     // blend: how far the mix sits below the rigs' power sum
    bool changed() const noexcept {return proposed!=before;}
};
inline constexpr float dualBlendMaxMove=.3f,dualBlendStep=.05f,dualMinGainDb=.5f,dualCancellationDb=3.f;
inline constexpr float dualCrossMin=60,dualCrossMax=4000,dualCrossFactor=2,dualCrossRatioLimitDb=12;
inline constexpr int dualCrossSteps=9;
// Fourth-order Linkwitz-Riley power response at f for a split at fc.
inline double dualLowPower(double f,double fc) noexcept {const double r=std::pow(f/fc,4.);return 1./((1.+r)*(1.+r));}
inline double dualHighPower(double f,double fc) noexcept {const double r=std::pow(f/fc,4.);return r*r/((1.+r)*(1.+r));}
// A band's mean power ratio (dB) between splits at `to` and at `from`, eight points per band.
inline float dualBandRatioDb(int band,double from,double to,bool high) {
    double now=0,next=0;
    for(int k=0;k<8;++k) {
        const double f=toneBandCentres[size_t(band)]*std::pow(2.,(k+.5)/24.-1./6.);
        now+=high?dualHighPower(f,from):dualLowPower(f,from);next+=high?dualHighPower(f,to):dualLowPower(f,to);
    }
    return float(std::clamp(10*std::log10(std::max(next,1e-30)/std::max(now,1e-30)),-double(dualCrossRatioLimitDb),double(dualCrossRatioLimitDb)));
}
// Power (linear) of a spectrum's bands within 80 Hz-6.3 kHz.
inline double dualFitPower(const std::array<float,toneBandCount>& bands) {
    double sum=0;for(int b=0;b<toneBandCount;++b)if(toneBandCentres[size_t(b)]>=80.f && toneBandCentres[size_t(b)]<=6300.f)sum+=std::pow(10.,bands[size_t(b)]/10.);
    return sum;
}
inline Spectrum dualWithBands(Spectrum s,const std::array<float,toneBandCount>& bands) {
    s.bandsDb=bands;s.strongestDb=-300;for(const float b:bands)s.strongestDb=std::max(s.strongestDb,b);return s;
}
// Crossover: the two rigs' band powers (dB) as heard with the split at
// `from`, predicted for a split at `to`, added in power.
inline std::array<float,toneBandCount> dualPredictCrossover(const std::array<float,toneBandCount>& low,const std::array<float,toneBandCount>& high,float from,float to) {
    std::array<float,toneBandCount> bands{};
    for(int band=0;band<toneBandCount;++band) {
        const double p=std::pow(10.,(low[size_t(band)]+dualBandRatioDb(band,from,to,false))/10.)
                      +std::pow(10.,(high[size_t(band)]+dualBandRatioDb(band,from,to,true))/10.);
        bands[size_t(band)]=float(10*std::log10(std::max(p,1e-30)));
    }
    return bands;
}
// rig1/rig2: the two rigs' outputs heard at the same time; current: the
// blend (0-1) or, when crossover, the split in Hz.
inline DualResult analyseDual(const std::vector<float>& rig1,const std::vector<float>& rig2,double rate,bool crossover,float current,
                              std::optional<bool> bass={},Style style=Style::rock) {
    DualResult r;r.crossover=crossover;r.before=current;r.proposed=current;
    const size_t n=std::min(rig1.size(),rig2.size());
    if(n==0)return r;
    std::vector<float> a(rig1.begin(),rig1.begin()+std::ptrdiff_t(n)),b(rig2.begin(),rig2.begin()+std::ptrdiff_t(n));
    const auto mixAt=[&](float blend){std::vector<float> y(n);for(size_t i=0;i<n;++i)y[i]=(1-blend)*a[i]+blend*b[i];return y;};
    std::vector<float> sum(n);for(size_t i=0;i<n;++i)sum[i]=a[i]+b[i];
    // The two rigs as heard now: in Blend weighted by the blend, in Crossover summed.
    const auto heard=measureSpectrum(crossover?sum:mixAt(current),rate,bass);
    r.playingDetected=heard.playing;
    if(!r.playingDetected)return r;
    r.bass=heard.bass;
    const auto s1=measureSpectrum(a,rate,r.bass),s2=measureSpectrum(b,rate,r.bass);
    r.rig1Heard=s1.playing;r.rig2Heard=s2.playing;
    if(!r.rig1Heard || !r.rig2Heard)return r;
    if(!crossover) {
        const auto spectrumAt=[&](float blend){return measureSpectrum(mixAt(blend),rate,r.bass);};
        const auto now=spectrumAt(current);
        r.toneErrorBeforeDb=ampToneErrorDb(now,style);r.toneErrorAfterDb=r.toneErrorBeforeDb;
        // Cancellation: the mix against the rigs' power sum at this blend.
        const double sum=std::pow(1-current,2.)*dualFitPower(s1.bandsDb)+std::pow(current,2.)*dualFitPower(s2.bandsDb),mix=dualFitPower(now.bandsDb);
        r.cancellationDb=sum>0 && mix>0?float(10*std::log10(sum/mix)):0.f;
        float best=r.toneErrorBeforeDb,chosen=current;Spectrum bestSpectrum=now;
        const float low=std::max(0.f,current-dualBlendMaxMove),high=std::min(1.f,current+dualBlendMaxMove);
        for(int i=-6;i<=6;++i) {
            const float blend=std::clamp(std::round((current+float(i)*dualBlendStep)*100.f)/100.f,low,high);
            if(i==0 || blend==current)continue;
            const auto s=spectrumAt(blend);
            if(const float e=ampToneErrorDb(s,style);e<best){best=e;chosen=blend;bestSpectrum=s;}
        }
        if(best<r.toneErrorBeforeDb-dualMinGainDb) {
            r.proposed=chosen;r.toneErrorAfterDb=best;
            r.levelChangeDb=float(10*std::log10(std::max(dualFitPower(bestSpectrum.bandsDb),1e-30)/std::max(mix,1e-30)));
        }
        return r;
    }
    // Crossover: rig 1 is the low rig, rig 2 the high rig. The heard mix is
    // their sum; predictions add the scaled rigs in power.
    const auto predictAt=[&](float to){return dualWithBands(heard,dualPredictCrossover(s1.bandsDb,s2.bandsDb,current,to));};
    const auto now=predictAt(current);
    r.toneErrorBeforeDb=ampToneErrorDb(now,style);r.toneErrorAfterDb=r.toneErrorBeforeDb;
    float best=r.toneErrorBeforeDb,chosen=current;Spectrum bestSpectrum=now;
    const double low=std::max(double(dualCrossMin),double(current)/dualCrossFactor),high=std::min(double(dualCrossMax),double(current)*dualCrossFactor);
    for(int i=0;i<dualCrossSteps;++i) {
        // Log-spaced from half to twice the split, on a 5 Hz grid.
        const float to=float(std::clamp(std::round(low*std::pow(high/low,i/double(dualCrossSteps-1))/5.)*5.,double(dualCrossMin),double(dualCrossMax)));
        if(to==current)continue;
        const auto s=predictAt(to);
        if(const float e=ampToneErrorDb(s,style);e<best){best=e;chosen=to;bestSpectrum=s;}
    }
    if(best<r.toneErrorBeforeDb-dualMinGainDb) {
        r.proposed=chosen;r.toneErrorAfterDb=best;
        r.levelChangeDb=float(10*std::log10(std::max(dualFitPower(bestSpectrum.bandsDb),1e-30)/std::max(dualFitPower(now.bandsDb),1e-30)));
    }
    return r;
}
} // namespace spectralforge::autoset
