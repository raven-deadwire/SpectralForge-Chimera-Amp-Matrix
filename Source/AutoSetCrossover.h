#pragma once
#include "AutoSetMatrix.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <optional>
#include <vector>

namespace spectralforge::autoset {

// Phase 25: the Matrix routing's crossovers X1 (LOW/MID) and X2 (MID/HIGH).
// The split happens before the lanes' amps, so moving a crossover changes what
// each amp is driven with; the lane captures cannot predict that exactly. The
// prediction is linear: each lane is taken as a gain per third-octave band
// behind its Linkwitz-Riley split (fourth order, as the engine splits), the
// lanes add coherently with their polarities, and the mix as heard moves by
// the modelled change.
// - A band where a lane's response now is more than 60 dB down keeps that
//   lane's part: what the lane plays there is its amp's harmonics.
// - Candidates: X1 and X2 each within a third of an octave of where they are,
//   in sixth-octave steps, within their controls' ranges, with X2 at least an
//   octave above X1.
// - A move is made only when the Tone EQ fit improves by more than 0.1 dB: a
//   crossover reshapes about an octave around it, so the whole-spectrum fit
//   moves far less than for the broad moves of the other steps.
// The rows report the prediction; it is approximate for driven lanes.
struct CrossoverResult {
    bool playingDetected{},bass{};
    std::array<bool,3> heard{};
    float x1Before{},x2Before{},x1After{},x2After{};
    float toneErrorBeforeDb{},toneErrorAfterDb{};
    int evaluations{};
    bool changed() const noexcept {return x1After!=x1Before || x2After!=x2Before;}
};
inline constexpr float crossoverMinGainDb=.1f,crossoverIgnoreBelowDb=-60.f,crossoverMinRatio=2.f;
inline constexpr int crossoverSteps=2;          // sixth-octave steps each way
inline constexpr std::array<float,2> crossoverX1Range{60.f,350.f},crossoverX2Range{500.f,4000.f};
// The engine's fourth-order Linkwitz-Riley lane responses at f (complex):
// LOW = LP(X1) AP(X2), MID = HP(X1) LP(X2), HIGH = HP(X1) HP(X2); in
// polarity they sum flat.
inline std::complex<double> lr4(double f,double fc,bool high) {
    const std::complex<double> s{0.,f/fc},b=1./(s*s+std::sqrt(2.)*s+1.),h=high?s*s*b:b;
    return h*h;
}
inline std::complex<double> crossoverLaneResponse(int lane,double f,double x1,double x2) {
    if(lane==0)return lr4(f,x1,false)*(lr4(f,x2,false)+lr4(f,x2,true));
    return lr4(f,x1,true)*lr4(f,x2,lane==2);
}
inline constexpr int crossoverPoints=8;     // per third-octave band
inline double crossoverPointHz(int band,int k){return toneBandCentres[size_t(band)]*std::pow(2.,(k+.5)/24.-1./6.);}
// A lane's mean power response over a third-octave band (dB).
inline float crossoverLaneBandDb(int lane,int band,double x1,double x2) {
    double p=0;
    for(int k=0;k<crossoverPoints;++k)p+=std::norm(crossoverLaneResponse(lane,crossoverPointHz(band,k),x1,x2));
    return float(10*std::log10(std::max(p/crossoverPoints,1e-30)));
}
// heard: the mix as heard; lanes: the lanes as heard; levelDb: level changes
// already decided for each lane (phase 17); inverted: each lane's polarity.
// Each lane is modelled as a gain per band behind its split (the lane's band
// level over its response there), the lanes adding coherently as the engine
// splits; the mix as heard moves by the modelled change. A band where a
// lane's response is more than 60 dB down keeps that lane's part as it is.
inline Spectrum crossoverPredicted(const Spectrum& heard,const std::array<Spectrum,3>& lanes,const std::array<float,3>& levelDb,const std::array<bool,3>& inverted,
                                   double x1,double x2,double newX1,double newX2) {
    Spectrum s=heard;s.strongestDb=-300;
    for(int b=0;b<toneBandCount;++b) {
        std::array<double,3> gain{},level{};std::array<bool,3> fixed{};
        for(int k=0;k<3;++k) {
            if(!lanes[size_t(k)].playing)continue;
            const float r=crossoverLaneBandDb(k,b,x1,x2);
            fixed[size_t(k)]=r<crossoverIgnoreBelowDb;
            gain[size_t(k)]=(inverted[size_t(k)]?-1.:1.)*std::pow(10.,(lanes[size_t(k)].bandsDb[size_t(b)]-r)/20.);
            level[size_t(k)]=std::pow(10.,levelDb[size_t(k)]/20.);
        }
        double before=0,after=0;
        for(int i=0;i<crossoverPoints;++i) {
            const double f=crossoverPointHz(b,i);
            std::complex<double> m0,m1;
            for(int k=0;k<3;++k) {
                const auto h0=crossoverLaneResponse(k,f,x1,x2);
                m0+=gain[size_t(k)]*h0;
                m1+=gain[size_t(k)]*level[size_t(k)]*(fixed[size_t(k)]?h0:crossoverLaneResponse(k,f,newX1,newX2));
            }
            before+=std::norm(m0);after+=std::norm(m1);
        }
        s.bandsDb[size_t(b)]=heard.bandsDb[size_t(b)]+float(10*std::log10(std::max(after,1e-30)/std::max(before,1e-30)));
        s.strongestDb=std::max(s.strongestDb,s.bandsDb[size_t(b)]);
    }
    return s;
}
inline CrossoverResult analyseCrossover(const std::vector<float>& low,const std::vector<float>& mid,const std::vector<float>& high,double rate,
                                        float x1,float x2,std::array<float,3> levelDb={},std::array<bool,3> inverted={},std::optional<bool> bass={},Style style=Style::rock) {
    CrossoverResult r;r.x1Before=r.x1After=x1;r.x2Before=r.x2After=x2;
    const size_t n=std::min({low.size(),mid.size(),high.size(),size_t(rate*matrixSeconds)});
    if(n==0)return r;
    std::vector<float> sum(n);
    for(size_t i=0;i<n;++i)sum[i]=low[i]+mid[i]+high[i];
    const auto heard=measureSpectrum(sum,rate,bass);
    r.playingDetected=heard.playing;
    if(!r.playingDetected)return r;
    r.bass=heard.bass;
    const auto head=[n](const std::vector<float>& v){return std::vector<float>(v.begin(),v.begin()+std::ptrdiff_t(n));};
    const std::array<Spectrum,3> lanes{measureSpectrum(head(low),rate,r.bass),measureSpectrum(head(mid),rate,r.bass),measureSpectrum(head(high),rate,r.bass)};
    for(int k=0;k<3;++k)r.heard[size_t(k)]=lanes[size_t(k)].playing;
    const auto errorAt=[&](double a,double b){++r.evaluations;return ampToneErrorDb(crossoverPredicted(heard,lanes,levelDb,inverted,x1,x2,a,b),style);};
    r.toneErrorBeforeDb=r.toneErrorAfterDb=errorAt(x1,x2);
    if(!r.heard[0] || !r.heard[1] || !r.heard[2])return r;
    float best=r.toneErrorBeforeDb;double bestX1=x1,bestX2=x2;
    for(int i=-crossoverSteps;i<=crossoverSteps;++i)for(int j=-crossoverSteps;j<=crossoverSteps;++j) {
        if(i==0 && j==0)continue;
        const double a=std::round(std::clamp(x1*std::pow(2.,i/6.),double(crossoverX1Range[0]),double(crossoverX1Range[1])));
        const double b=std::round(std::clamp(x2*std::pow(2.,j/6.),double(crossoverX2Range[0]),double(crossoverX2Range[1])));
        if(b<crossoverMinRatio*a || (a==x1 && b==x2))continue;
        // Ties keep the smaller move.
        const float e=errorAt(a,b);
        const double move=std::abs(std::log2(a/x1))+std::abs(std::log2(b/x2)),bestMove=std::abs(std::log2(bestX1/x1))+std::abs(std::log2(bestX2/x2));
        if(e<best-1e-4f || (e<=best+1e-4f && move<bestMove)){best=e;bestX1=a;bestX2=b;}
    }
    if(best<r.toneErrorBeforeDb-crossoverMinGainDb){r.x1After=float(bestX1);r.x2After=float(bestX2);r.toneErrorAfterDb=best;}
    return r;
}
} // namespace spectralforge::autoset
