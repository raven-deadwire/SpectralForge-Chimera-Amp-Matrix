#pragma once
#include "AutoSetAmp.h"

namespace spectralforge::autoset {

// Phase 15: the cabinet's LOW and HIGH cuts (Classic routing, rig 1). A result
// step of the cabinet mic pass: the same capture after the cab (Tap::cab).
// The cuts are second-order Butterworth filters (Cab::setCuts), so a new
// cut's effect is known exactly: the heard spectrum plus the new cut's band
// response minus the current one (the digital filter's own response, which
// bends near Nyquist) predicts what would be heard.
// - LOW: the lowest bands (guitar 50-160 Hz, bass 50-100 Hz) against the
//   Tone EQ target aligned at 250 Hz-1 kHz. Excess low-band energy raises
//   it; a thin low end lowers it. It never rises past the
//   Drop A preservation ceiling (guitar 55 Hz; bass 25 Hz). Existing
//   higher cuts are only lowered when the target fit also improves.
// - HIGH: the top bands (5-10 kHz) against the target: fizz lowers it, a dull
//   top raises it.
// Each cut moves at most an octave, and only when its region's fit improves
// by more than 0.5 dB. The Tone EQ pass that follows corrects what is left.
struct CutResult {
    bool playingDetected{},bass{};
    float lowBefore{},lowAfter{},highBefore{},highAfter{};
    float lowErrorBeforeDb{},lowErrorAfterDb{},highErrorBeforeDb{},highErrorAfterDb{};
    bool lowChanged() const noexcept {return lowAfter!=lowBefore;}
    bool highChanged() const noexcept {return highAfter!=highBefore;}
    bool changed() const noexcept {return lowChanged() || highChanged();}
};
inline constexpr float cutMinGainDb=.5f,cutMoveFactor=2,cutBandLimitDb=24;
inline constexpr int cutSteps=9;
// Conservative fallback without pitch tracking: protect A1 (55 Hz) and
// A0 (27.5 Hz) for every style. A Butterworth cutoff at or below the
// fundamental loses at most about 3 dB there. Round down to the 5 Hz grid.
// This bounds automatic increases, not an existing higher user cutoff.
inline float cutLowLimit(bool bass,Style) noexcept {return bass?25.f:55.f;}
// The cut the cabinet plays (Cab::setCuts: JUCE's second-order high or low
// pass at the session rate), and its power response at f.
inline auto cutCoefficients(double fc,bool lowCut,double rate) {
    return lowCut?juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(rate,float(fc)):juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(rate,float(fc));
}
template<typename Coefficients> double cutPower(double f,const Coefficients& c,double rate) {
    const std::complex<double> z=std::polar(1.,-2*juce::MathConstants<double>::pi*f/rate);
    const auto num=double(c[0])+double(c[1])*z+double(c[2])*z*z,den=double(c[3])+double(c[4])*z+double(c[5])*z*z;
    return std::norm(num/den);
}
// A band's power change (dB) when a cut moves from `from` to `to`, eight points per band.
inline float cutBandChangeDb(int band,double from,double to,bool lowCut,double rate) {
    const auto now=cutCoefficients(from,lowCut,rate),next=cutCoefficients(to,lowCut,rate);
    double before=0,after=0;
    for(int k=0;k<8;++k) {
        const double f=toneBandCentres[size_t(band)]*std::pow(2.,(k+.5)/24.-1./6.);
        if(f>=rate*.5)continue;
        before+=cutPower(f,now,rate);after+=cutPower(f,next,rate);
    }
    return float(std::clamp(10*std::log10(std::max(after,1e-30)/std::max(before,1e-30)),-double(cutBandLimitDb),double(cutBandLimitDb)));
}
// RMS distance from the aligned target over a region's bands with content.
inline float cutRegionErrorDb(const std::array<float,toneBandCount>& bands,const std::array<float,toneBandCount>& target,float strongest,float from,float to,double rate) {
    double sum=0;int count=0;
    for(int b=0;b<toneBandCount;++b) {
        const float fc=toneBandCentres[size_t(b)];
        if(fc<from || fc>to || fc>rate*.4 || bands[size_t(b)]<strongest-60.f)continue;
        const double d=bands[size_t(b)]-target[size_t(b)];sum+=d*d;++count;
    }
    return count?float(std::sqrt(sum/count)):0.f;
}
// heard: the cab output's spectrum (playing); offset: per-band change still
// to come before the Tone EQ (phase 9's mic move, 0 when none).
inline CutResult analyseCuts(const Spectrum& heard,double rate,float low,float high,const std::array<float,toneBandCount>& offset,Style style=Style::rock) {
    CutResult r;r.lowBefore=r.lowAfter=low;r.highBefore=r.highAfter=high;
    r.playingDetected=heard.playing;
    if(!r.playingDetected)return r;
    r.bass=heard.bass;
    auto now=heard;for(int b=0;b<toneBandCount;++b)now.bandsDb[size_t(b)]+=offset[size_t(b)];
    const auto target=alignedTarget(now,style);
    const float lowTop=r.bass?100.f:160.f,topFrom=5000.f,topTo=10000.f;
    const auto predicted=[&](float lowCut,float highCut){
        auto bands=now.bandsDb;
        for(int b=0;b<toneBandCount;++b)bands[size_t(b)]+=cutBandChangeDb(b,low,lowCut,true,rate)+cutBandChangeDb(b,high,highCut,false,rate);
        return bands;
    };
    const auto lowError=[&](float lowCut){return cutRegionErrorDb(predicted(lowCut,high),target,now.strongestDb,50,lowTop,rate);};
    const auto highError=[&](float highCut){return cutRegionErrorDb(predicted(low,highCut),target,now.strongestDb,topFrom,topTo,rate);};
    r.lowErrorBeforeDb=r.lowErrorAfterDb=lowError(low);r.highErrorBeforeDb=r.highErrorAfterDb=highError(high);
    // Candidates: log-spaced from half to twice the cut.
    const auto search=[&](float current,double minimum,double maximum,double grid,const auto& error,float& chosen,float& after) {
        // A cut above the limit can still come down to it (a one-candidate range).
        const double from=std::min(maximum,std::max(minimum,double(current)/cutMoveFactor)),to=std::min(maximum,double(current)*cutMoveFactor);
        if(to<from){chosen=current;return;}
        float best=after;
        for(int i=0;i<cutSteps;++i) {
            const float candidate=float(std::clamp(std::round(from*std::pow(to/from,i/double(cutSteps-1))/grid)*grid,minimum,maximum));
            if(candidate==current)continue;
            if(const float e=error(candidate);e<best){best=e;chosen=candidate;}
        }
        if(best<after-cutMinGainDb)after=best;else chosen=current;
    };
    search(low,20,cutLowLimit(r.bass,style),5,lowError,r.lowAfter,r.lowErrorAfterDb);
    search(high,r.bass?2500.:4000.,std::min(20000.,rate*.45),100,highError,r.highAfter,r.highErrorAfterDb);
    return r;
}
} // namespace spectralforge::autoset
