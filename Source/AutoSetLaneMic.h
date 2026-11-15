#pragma once
#include "AutoSetCab.h"
#include "AutoSetLaneCuts.h"
#include <complex>
#include <optional>
#include <vector>

namespace spectralforge::autoset {

// Phase 30: the modeled cabinet mic's position and distance on each Dual rig
// or Matrix lane. Phase 9 (AUTO_SET_PHASE9.md) places the Classic rig's mic
// from the rig heard alone. Here the lanes are heard mixed. The modeled
// cabinet's IR is its authored response (cabLayout::response, propagation
// delays included), so a candidate placement changes its lane by the complex
// ratio of the candidate's response to the current one. That ratio is smooth,
// so it is taken per 1/96 octave: the lanes' spectra are reduced once to their
// cross-powers in each such step (per tone band), and a candidate then costs
// one small sum per step. The lanes add coherently, as the engine mixes them.
// - The lane's level is held: its power with the candidate response is
//   brought back to its power now, as phase 9's mic level does, and the mix is
//   judged with that gain.
// - Candidates, the fit (the amp step's tone error of the predicted mix), the
//   0.5 dB minimum gain and the roughness and local peak/notch limits are
//   phase 9's. Lanes are judged one after another, each with the placements
//   already chosen.
struct LaneMicChoice {
    int lane{};                  // index into the lanes passed in
    bool heard{};
    CabPlacement before{},proposed{};
    float toneErrorBeforeDb{},toneErrorAfterDb{};
    float holdGainDb{};          // the lane gain that keeps its level at the proposed placement
    bool changed() const noexcept {return !(proposed==before);}
};
struct LaneMicResult {
    bool playingDetected{},bass{};
    std::vector<LaneMicChoice> lanes;
    bool changed() const noexcept {for(const auto& l:lanes)if(l.changed())return true;return false;}
};
inline constexpr int laneMicStepsPerOctave=96;
inline constexpr double laneMicFromHz=27.5,laneMicRatioLimit=31.6,laneMicBandLimitDb=24.;

// lanes, weights: as analyseLaneCuts. candidates: the lanes whose modeled mic
// may move, with the model each plays now.
// evaluate (tests): no search; each candidate lane's "proposed" is this
// placement, with the fit predicted for it.
inline LaneMicResult analyseLaneMics(const std::vector<const std::vector<float>*>& lanes,const std::vector<double>& weights,
                                     const std::vector<std::pair<int,cabLayout::Settings>>& candidates,double rate,
                                     std::optional<bool> bass={},Style style=Style::rock,std::optional<CabPlacement> evaluate={}) {
    using Complex=std::complex<double>;
    LaneMicResult r;
    const size_t count=lanes.size();
    if(count==0 || count>3 || count!=weights.size() || rate<8000)return r;
    size_t length=size_t(rate*laneCutsMaxSeconds);
    for(const auto* l:lanes)length=std::min(length,l->size());
    if(length<size_t(rate))return r;
    std::vector<float> mix(length,0.f);
    for(size_t k=0;k<count;++k)for(size_t n=0;n<length;++n)mix[n]+=float(weights[k])*(*lanes[k])[n];
    const auto heard=measureSpectrum(mix,rate,bass);
    r.playingDetected=heard.playing;
    if(!r.playingDetected)return r;
    r.bass=heard.bass;
    int order=1;while((size_t(1)<<order)<length+size_t(rate*.25))++order;
    const size_t size=size_t(1)<<order;
    juce::dsp::FFT fft(order);
    std::vector<float> buffer(2*size);
    std::vector<std::vector<Complex>> spectra(count);std::vector<double> lanePower(count,0);
    for(size_t k=0;k<count;++k) {
        std::fill(buffer.begin(),buffer.end(),0.f);
        std::copy(lanes[k]->begin(),lanes[k]->begin()+std::ptrdiff_t(length),buffer.begin());
        for(size_t n=0;n<length;++n)lanePower[k]+=double((*lanes[k])[n])*(*lanes[k])[n];
        fft.performRealOnlyForwardTransform(buffer.data(),true);
        spectra[k].resize(size/2+1);
        for(size_t b=0;b<=size/2;++b)spectra[k][b]=Complex(buffer[2*b],buffer[2*b+1])*weights[k];
    }
    // The lanes' cross-powers per 1/96-octave step, split at the tone bands' edges.
    // band -1: below or between the tone bands (it still counts for a lane's level).
    struct Cell {int band,step;std::array<std::array<Complex,3>,3> cross{};};
    std::vector<Cell> cells;
    const double top=std::min(rate*.4,20000.);
    const auto bandOf=[](double hz) {
        const double edge=std::pow(2.,1./6.);
        for(int b=0;b<toneBandCount;++b)if(hz>=toneBandCentres[size_t(b)]/edge && hz<toneBandCentres[size_t(b)]*edge)return b;
        return -1;
    };
    for(size_t bin=std::max<size_t>(1,size_t(std::ceil(laneMicFromHz/std::pow(2.,.5/laneMicStepsPerOctave)*double(size)/rate)));bin<=size/2;++bin) {
        const double hz=double(bin)*rate/double(size);
        if(hz>top)break;
        // The first bin can sit on the lower edge of step 0, where rounding could give -1.
        const int step=std::max(0,int(std::lround(laneMicStepsPerOctave*std::log2(hz/laneMicFromHz)))),band=bandOf(hz);
        if(cells.empty() || cells.back().band!=band || cells.back().step!=step)cells.push_back({band,step,{}});
        auto& cross=cells.back().cross;
        for(size_t k=0;k<count;++k)for(size_t l=0;l<count;++l)cross[k][l]+=spectra[k][bin]*std::conj(spectra[l][bin]);
    }
    if(cells.empty())return r;
    const int steps=cells.back().step+1;
    const auto stepHz=[](int step){return laneMicFromHz*std::exp2(double(step)/laneMicStepsPerOctave);};
    // A model's response at every step.
    const auto responseOf=[&](const cabLayout::Settings& s) {
        std::vector<Complex> h(static_cast<size_t>(steps));
        for(int i=0;i<steps;++i)h[size_t(i)]=cabLayout::response(s,stepHz(i));
        return h;
    };
    // Each lane's change per step: 1 while its mic stays.
    using Ratios=std::vector<std::vector<Complex>>;
    Ratios chosen(count,std::vector<Complex>(static_cast<size_t>(steps),Complex(1.,0.)));
    const auto bandPowers=[&](const Ratios& ratios) {
        std::array<double,toneBandCount> power{};
        for(const auto& cell:cells) {
            if(cell.band<0)continue;
            double p=0;
            for(size_t k=0;k<count;++k)for(size_t l=0;l<count;++l)p+=std::real(ratios[k][size_t(cell.step)]*std::conj(ratios[l][size_t(cell.step)])*cell.cross[k][l]);
            power[size_t(cell.band)]+=p;
        }
        return power;
    };
    const auto heardPower=bandPowers(chosen);
    const auto predict=[&](const Ratios& ratios) {
        const auto power=bandPowers(ratios);
        std::array<float,toneBandCount> move{};const std::array<float,toneBandCount> zero{};
        for(int b=0;b<toneBandCount;++b)if(heardPower[size_t(b)]>0)
            move[size_t(b)]=float(std::clamp(10*std::log10(std::max(power[size_t(b)],1e-30)/heardPower[size_t(b)]),-laneMicBandLimitDb,laneMicBandLimitDb));
        return cabPredicted(heard,zero,move);
    };
    // A lane's ratio for a candidate response, with the gain that holds the lane's level.
    const auto ratioFor=[&](size_t lane,const std::vector<Complex>& now,const std::vector<Complex>& next,float& holdDb) {
        std::vector<Complex> q(static_cast<size_t>(steps));
        for(int i=0;i<steps;++i) {
            const double a=std::abs(now[size_t(i)]),b=std::abs(next[size_t(i)]);
            q[size_t(i)]=a<=0?Complex(1.,0.):b>a*laneMicRatioLimit?next[size_t(i)]/now[size_t(i)]*(laneMicRatioLimit*a/b):next[size_t(i)]/now[size_t(i)];
        }
        double before=0,after=0;
        for(const auto& cell:cells){const double p=std::real(cell.cross[lane][lane]);before+=p;after+=p*std::norm(q[size_t(cell.step)]);}
        const double gain=before>0 && after>0?std::sqrt(before/after):1.;
        holdDb=float(20*std::log10(gain));
        for(auto& v:q)v*=gain;
        return q;
    };
    const double strongestLane=*std::max_element(lanePower.begin(),lanePower.end());
    float errorNow=ampToneErrorDb(predict(chosen),style);
    for(const auto& [lane,current]:candidates) {
        if(lane<0 || size_t(lane)>=count)continue;
        LaneMicChoice c;c.lane=lane;c.before=c.proposed={current.voice.base.position,current.voice.base.distanceCm};
        c.toneErrorBeforeDb=c.toneErrorAfterDb=errorNow;
        c.heard=lanePower[size_t(lane)]>0 && 10*std::log10(lanePower[size_t(lane)]/strongestLane)>laneCutsSilentDb;
        if(!c.heard){r.lanes.push_back(c);continue;}
        const auto now=responseOf(current);
        auto trial=chosen;
        const auto errorOf=[&](CabPlacement p,float& holdDb) {
            trial[size_t(lane)]=ratioFor(size_t(lane),now,responseOf(withPlacement(current,p)),holdDb);
            return ampToneErrorDb(predict(trial),style);
        };
        if(evaluate) {
            c.proposed=*evaluate;c.toneErrorAfterDb=errorOf(c.proposed,c.holdGainDb);
            chosen[size_t(lane)]=trial[size_t(lane)];errorNow=c.toneErrorAfterDb;
            r.lanes.push_back(c);continue;
        }
        // Phase 9's candidates and limits.
        const auto nowBands=cabBandsDb(current);const auto detail=cabDetailDb(current);
        const float roughnessNow=cabRoughnessDb(nowBands);
        float best=errorNow,bestHold=0;CabPlacement pick=c.before;
        const double low=std::max(2.,c.before.distanceCm/cabDistanceFactor),high=std::min(60.,c.before.distanceCm*cabDistanceFactor);
        for(int i=0;i<cabDistanceSteps;++i) {
            const double distance=std::clamp(std::round(low*std::pow(high/low,i/double(cabDistanceSteps-1))/cabDistanceStepCm)*cabDistanceStepCm,2.,60.);
            for(double position=std::max(0.,c.before.position-cabPositionMaxMove);position<=std::min(1.,c.before.position+cabPositionMaxMove)+1e-9;position+=cabPositionStep) {
                const CabPlacement candidate{std::round(position*1000)/1000,distance};
                if(candidate==c.before)continue;
                const auto moved=withPlacement(current,candidate);
                if(cabRoughnessDb(cabBandsDb(moved))>roughnessNow+cabRoughnessAllowanceDb)continue;
                float hold=0;
                if(const float e=errorOf(candidate,hold);e<best) {
                    if(cabAddedExtremaDb(detail,cabDetailDb(moved))>cabLocalExtremaAllowanceDb)continue;
                    best=e;bestHold=hold;pick=candidate;
                }
            }
        }
        if(best<errorNow-cabToneMinGainDb) {
            c.proposed=pick;c.toneErrorAfterDb=errorOf(pick,c.holdGainDb);c.holdGainDb=bestHold;
            chosen[size_t(lane)]=trial[size_t(lane)];errorNow=c.toneErrorAfterDb;
        }
        r.lanes.push_back(c);
    }
    return r;
}
} // namespace spectralforge::autoset
