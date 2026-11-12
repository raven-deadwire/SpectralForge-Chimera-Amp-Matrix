#pragma once
#include "AutoSetCabCuts.h"
#include <complex>
#include <optional>
#include <vector>

namespace spectralforge::autoset {

// Phase 27: the cabinet LOW and HIGH cuts of Dual rig 2 and of the Matrix MID
// and HIGH lanes (cablow2/3, cabhigh2/3). A result step of the Dual or the
// Matrix pass: the lane captures there are after each lane's cabinet (the
// cuts included), level and polarity. A cut is a second-order Butterworth
// filter after the amp and cabinet (Cab::setCuts), so moving it changes the
// lane exactly by the new cut's response over the current one. The step
// re-weights each lane's spectrum by that ratio, adds the lanes coherently as
// the engine mixes them, and predicts the heard mix's change band by band.
// The fit is judged as in phase 15 (AUTO_SET_PHASE15.md): the low end (guitar
// 50-160 Hz, bass 50-100 Hz) and the top (5-10 kHz) against the aligned Tone
// EQ target, the same candidates (half to twice the cut), limits (the Drop A
// ceiling for a LOW cut) and the 0.5 dB minimum gain. Lanes are judged one
// after another, each with the cuts already chosen for the earlier ones.
struct LaneCut {
    int lane{};                 // index into the lanes passed in
    bool heard{};               // the lane played (not muted or soloed away)
    float lowBefore{},lowAfter{},highBefore{},highAfter{};
    float lowErrorBeforeDb{},lowErrorAfterDb{},highErrorBeforeDb{},highErrorAfterDb{};
    bool lowChanged() const noexcept {return lowAfter!=lowBefore;}
    bool highChanged() const noexcept {return highAfter!=highBefore;}
    bool changed() const noexcept {return lowChanged() || highChanged();}
};
struct LaneCutsResult {
    bool playingDetected{},bass{};
    std::vector<LaneCut> lanes;
    bool changed() const noexcept {for(const auto& l:lanes)if(l.changed())return true;return false;}
};
inline constexpr double laneCutsMaxSeconds=6;
inline constexpr float laneCutsSilentDb=-60.f;

namespace laneCutsDetail {
// The cut Cab::setCuts plays: JUCE's second-order filter, its cutoff clamped as there.
struct Cut {
    std::array<double,6> c{};
    Cut(double fc,bool lowCut,double rate) {
        fc=lowCut?std::clamp(fc,10.,rate*.44):std::clamp(fc,100.,rate*.45);
        const auto k=cutCoefficients(fc,lowCut,rate);
        for(size_t i=0;i<6;++i)c[i]=double(k[i]);
    }
    std::complex<double> at(std::complex<double> z) const {return (c[0]+c[1]*z+c[2]*z*z)/(c[3]+c[4]*z+c[5]*z*z);}
};
struct LaneSpectrum {std::vector<std::complex<double>> bins;double weight{};float low{},high{};};
}

// lanes: the captures (mono, same length); weights: each lane's gain in the mix
// relative to its capture (blend, level change, a polarity flip as -1);
// adjustable: which lanes' cuts may move, with their current LOW and HIGH cuts.
// evaluate (tests): no search; each adjustable lane's "after" is these cuts,
// with the fit predicted for them.
inline LaneCutsResult analyseLaneCuts(const std::vector<const std::vector<float>*>& lanes,const std::vector<double>& weights,
                                      const std::vector<std::pair<int,std::array<float,2>>>& adjustable,double rate,
                                      std::optional<bool> bass={},Style style=Style::rock,std::optional<std::array<float,2>> evaluate={}) {
    using namespace laneCutsDetail;
    LaneCutsResult r;
    if(lanes.empty() || lanes.size()!=weights.size() || rate<8000)return r;
    size_t length=size_t(rate*laneCutsMaxSeconds);
    for(const auto* l:lanes)length=std::min(length,l->size());
    if(length<size_t(rate))return r;
    // The heard mix and its long-term spectrum (the starting point, as the Tone EQ step hears it).
    std::vector<float> mix(length,0.f);
    for(size_t k=0;k<lanes.size();++k)for(size_t n=0;n<length;++n)mix[n]+=float(weights[k])*(*lanes[k])[n];
    const auto heard=measureSpectrum(mix,rate,bass);
    r.playingDetected=heard.playing;
    if(!r.playingDetected)return r;
    r.bass=heard.bass;
    const auto target=alignedTarget(heard,style);
    // One FFT of each lane over the whole capture (zero-padded, so a cut's
    // ringing stays inside the transform).
    int order=1;while((size_t(1)<<order)<length+size_t(rate*.25))++order;
    const size_t size=size_t(1)<<order;
    juce::dsp::FFT fft(order);
    std::vector<float> buffer(2*size);
    std::vector<LaneSpectrum> spectra(lanes.size());
    std::vector<double> lanePower(lanes.size(),0);
    for(size_t k=0;k<lanes.size();++k) {
        std::fill(buffer.begin(),buffer.end(),0.f);
        std::copy(lanes[k]->begin(),lanes[k]->begin()+std::ptrdiff_t(length),buffer.begin());
        for(size_t n=0;n<length;++n)lanePower[k]+=double((*lanes[k])[n])*(*lanes[k])[n];
        fft.performRealOnlyForwardTransform(buffer.data(),true);
        auto& s=spectra[k];s.weight=weights[k];s.bins.resize(size/2+1);
        for(size_t b=0;b<=size/2;++b)s.bins[b]={buffer[2*b],buffer[2*b+1]};
    }
    // Bins of each third-octave band (as measureSpectrum's band edges).
    std::array<std::pair<size_t,size_t>,toneBandCount> bandBins{};
    for(int b=0;b<toneBandCount;++b) {
        const double edge=std::pow(2.,1./6.),fc=toneBandCentres[size_t(b)];
        bandBins[size_t(b)]={std::max<size_t>(1,size_t(std::ceil(fc/edge*double(size)/rate))),std::min(size/2,size_t(std::ceil(fc*edge*double(size)/rate)))};
    }
    const double strongestLane=*std::max_element(lanePower.begin(),lanePower.end());
    // The cuts each lane plays now (captured) and as chosen so far.
    std::vector<std::array<float,2>> captured(lanes.size(),{0.f,0.f}),chosen(lanes.size(),{0.f,0.f});
    std::vector<bool> movable(lanes.size(),false);
    for(const auto& [lane,cuts]:adjustable)if(lane>=0 && size_t(lane)<lanes.size()){captured[size_t(lane)]=chosen[size_t(lane)]=cuts;movable[size_t(lane)]=true;}
    // The mix's power per band with the given cuts on each movable lane.
    const auto bandPower=[&](int band,const std::vector<std::array<float,2>>& cuts) {
        struct Change {size_t lane;Cut lowNow,lowNext,highNow,highNext;};
        std::vector<Change> changes;
        for(size_t k=0;k<spectra.size();++k)if(movable[k] && cuts[k]!=captured[k])
            changes.push_back({k,Cut(captured[k][0],true,rate),Cut(cuts[k][0],true,rate),Cut(captured[k][1],false,rate),Cut(cuts[k][1],false,rate)});
        double power=0;
        const auto [from,to]=bandBins[size_t(band)];
        std::vector<std::complex<double>> ratio(changes.size());
        for(size_t bin=from;bin<to;++bin) {
            const std::complex<double> z=std::polar(1.,-2*juce::MathConstants<double>::pi*double(bin)/double(size));
            for(size_t i=0;i<changes.size();++i){const auto& c=changes[i];ratio[i]=c.lowNext.at(z)/c.lowNow.at(z)*c.highNext.at(z)/c.highNow.at(z);}
            std::complex<double> y{};
            for(size_t k=0;k<spectra.size();++k) {
                std::complex<double> v=spectra[k].bins[bin]*spectra[k].weight;
                for(size_t i=0;i<changes.size();++i)if(changes[i].lane==k)v*=ratio[i];
                y+=v;
            }
            power+=std::norm(y);
        }
        return power;
    };
    std::array<double,toneBandCount> heardPower{};
    for(int b=0;b<toneBandCount;++b)heardPower[size_t(b)]=bandPower(b,captured);
    const float lowTop=r.bass?100.f:160.f,topFrom=5000.f,topTo=10000.f;
    // Predicted bands in [from,to]: the heard level moved by the modelled change.
    const auto regionError=[&](const std::vector<std::array<float,2>>& cuts,float from,float to) {
        auto bands=heard.bandsDb;
        for(int b=0;b<toneBandCount;++b) {
            const float fc=toneBandCentres[size_t(b)];
            if(fc<from || fc>to || fc>rate*.4 || heardPower[size_t(b)]<=0)continue;
            const double change=10*std::log10(std::max(bandPower(b,cuts),1e-30)/heardPower[size_t(b)]);
            bands[size_t(b)]+=float(std::clamp(change,-double(cutBandLimitDb),double(cutBandLimitDb)));
        }
        return cutRegionErrorDb(bands,target,heard.strongestDb,from,to,rate);
    };
    for(const auto& [lane,cuts]:adjustable) {
        if(lane<0 || size_t(lane)>=lanes.size())continue;
        LaneCut c;c.lane=lane;c.lowBefore=c.lowAfter=cuts[0];c.highBefore=c.highAfter=cuts[1];
        c.heard=lanePower[size_t(lane)]>0 && 10*std::log10(lanePower[size_t(lane)]/strongestLane)>laneCutsSilentDb;
        if(!c.heard){r.lanes.push_back(c);continue;}
        auto trial=chosen;
        const auto lowError=[&](float lowCut){trial[size_t(lane)]={lowCut,chosen[size_t(lane)][1]};return regionError(trial,50,lowTop);};
        const auto highError=[&](float highCut){trial[size_t(lane)]={chosen[size_t(lane)][0],highCut};return regionError(trial,topFrom,topTo);};
        c.lowErrorBeforeDb=c.lowErrorAfterDb=lowError(cuts[0]);
        c.highErrorBeforeDb=c.highErrorAfterDb=highError(cuts[1]);
        if(evaluate) {
            c.lowAfter=(*evaluate)[0];c.lowErrorAfterDb=lowError(c.lowAfter);chosen[size_t(lane)][0]=c.lowAfter;
            c.highAfter=(*evaluate)[1];c.highErrorAfterDb=highError(c.highAfter);chosen[size_t(lane)][1]=c.highAfter;
            r.lanes.push_back(c);continue;
        }
        // Phase 15's search: log-spaced candidates from half to twice the cut.
        const auto search=[&](float current,double minimum,double maximum,double grid,const auto& error,float& pick,float& after) {
            const double from=std::min(maximum,std::max(minimum,double(current)/cutMoveFactor)),to=std::min(maximum,double(current)*cutMoveFactor);
            if(to<from){pick=current;return;}
            float best=after;
            for(int i=0;i<cutSteps;++i) {
                const float candidate=float(std::clamp(std::round(from*std::pow(to/from,i/double(cutSteps-1))/grid)*grid,minimum,maximum));
                if(candidate==current)continue;
                if(const float e=error(candidate);e<best){best=e;pick=candidate;}
            }
            if(best<after-cutMinGainDb)after=best;else pick=current;
        };
        search(cuts[0],20,cutLowLimit(r.bass,style),5,lowError,c.lowAfter,c.lowErrorAfterDb);
        chosen[size_t(lane)][0]=c.lowAfter;
        search(cuts[1],r.bass?2500.:4000.,std::min(20000.,rate*.45),100,highError,c.highAfter,c.highErrorAfterDb);
        chosen[size_t(lane)][1]=c.highAfter;
        r.lanes.push_back(c);
    }
    return r;
}
} // namespace spectralforge::autoset
