#pragma once
#include "AutoSetDual.h"

namespace spectralforge::autoset {

// Phase 21: the polarity of Dual rig 2, or of the Matrix MID and HIGH lanes.
// Phases 13 and 17 report when the rigs or lanes partly cancel; this step
// flips the lane polarity that removes it. Each lane is heard on its own
// (Tap::lane1-lane3, after the lane's level and polarity), so a flip is the
// capture negated and the prediction is the mix itself:
//   mix = w0 * lane0 + sum over k of s_k * w_k * lane_k,  s_k = +1 or -1
// - Lane 0 (rig 1, or the Matrix LOW lane) is the reference: flipping every
//   lane at once changes nothing heard, so only the others move.
// - Cancellation: the mix against the weighted lanes' power sum in the worst
//   third-octave band, 80 Hz-6.3 kHz, within 30 dB of the strongest band (as
//   phase 17 reports it).
// - Every sign combination is tried. A flip is made only when the mix now
//   cancels by more than 3 dB and the best combination cancels at least 1 dB
//   less. Ties keep the current polarity.
struct PolarityResult {
    bool playingDetected{},bass{};
    std::vector<bool> heard;                  // per lane
    std::vector<bool> flip;                   // per lane after the reference (lane 1...)
    float cancellationBeforeDb{},cancellationAfterDb{};
    bool changed() const noexcept {return std::any_of(flip.begin(),flip.end(),[](bool f){return f;});}
};
inline constexpr float polarityMinCancellationDb=3,polarityMinGainDb=1;
inline constexpr double polaritySeconds=4;
// Worst-band cancellation of a mix against the weighted lanes' power sum.
inline float polarityCancellationDb(const Spectrum& mix,const std::vector<Spectrum>& lanes,const std::vector<float>& weights) {
    float worst=0;
    for(int band=0;band<toneBandCount;++band) {
        const float fc=toneBandCentres[size_t(band)];
        if(fc<80.f || fc>6300.f)continue;
        double sum=0;
        for(size_t k=0;k<lanes.size();++k)if(lanes[k].playing)sum+=double(weights[k])*weights[k]*std::pow(10.,lanes[k].bandsDb[size_t(band)]/10.);
        if(sum<=0 || 10*std::log10(sum)<mix.strongestDb-30.f)continue;
        worst=std::max(worst,float(10*std::log10(sum/std::max(std::pow(10.,mix.bandsDb[size_t(band)]/10.),1e-30))));
    }
    return worst;
}
// lanes: each lane as heard (lane 0 the reference); weights: each lane's
// weight in the mix (Dual blend: 1 - blend and blend; otherwise 1).
inline PolarityResult analysePolarity(const std::vector<const std::vector<float>*>& lanes,const std::vector<float>& weights,double rate,std::optional<bool> bass={}) {
    PolarityResult r;
    const size_t count=lanes.size();
    r.heard.assign(count,false);r.flip.assign(count>0?count-1:0,false);
    if(count<2 || weights.size()!=count)return r;
    size_t n=size_t(rate*polaritySeconds);
    for(const auto* lane:lanes)n=std::min(n,lane->size());
    if(n==0)return r;
    std::vector<float> mix(n);
    const auto mixAt=[&](unsigned signs)->const std::vector<float>& {
        for(size_t i=0;i<n;++i) {
            double v=weights[0]*(*lanes[0])[i];
            for(size_t k=1;k<count;++k)v+=((signs>>(k-1))&1u?-1.:1.)*weights[k]*(*lanes[k])[i];
            mix[i]=float(v);
        }
        return mix;
    };
    const auto now=measureSpectrum(mixAt(0),rate,bass);
    r.playingDetected=now.playing;
    if(!r.playingDetected)return r;
    r.bass=now.bass;
    std::vector<Spectrum> spectra;
    for(size_t k=0;k<count;++k) {
        const std::vector<float> head(lanes[k]->begin(),lanes[k]->begin()+std::ptrdiff_t(n));
        spectra.push_back(measureSpectrum(head,rate,r.bass));r.heard[k]=spectra.back().playing;
    }
    r.cancellationBeforeDb=r.cancellationAfterDb=polarityCancellationDb(now,spectra,weights);
    if(std::any_of(r.heard.begin(),r.heard.end(),[](bool h){return !h;}))return r;
    if(r.cancellationBeforeDb<=polarityMinCancellationDb)return r;
    unsigned best=0;float bestDb=r.cancellationBeforeDb;
    for(unsigned signs=1;signs<(1u<<(count-1));++signs) {
        const float c=polarityCancellationDb(measureSpectrum(mixAt(signs),rate,r.bass),spectra,weights);
        if(c<bestDb-1e-4f){bestDb=c;best=signs;}
    }
    if(best!=0 && bestDb<=r.cancellationBeforeDb-polarityMinGainDb) {
        for(size_t k=1;k<count;++k)r.flip[k-1]=((best>>(k-1))&1u)!=0;
        r.cancellationAfterDb=bestDb;
    }
    return r;
}
} // namespace spectralforge::autoset
