#pragma once
#include "AutoSetCabIR.h"
#include "AutoSetLaneCuts.h"
#include <complex>
#include <optional>
#include <vector>

namespace spectralforge::autoset {

// Phase 29: the factory cabinet IR (V30 / SM57 or Jensen / SM57) of each Dual
// rig or Matrix lane's heard mic. Phase 23 (AUTO_SET_PHASE23.md) chooses it
// for the Classic rig, from the rig heard alone. Here the lanes are heard
// mixed: a lane's IR is a fixed filter in series in that lane, so the other
// IR changes the lane exactly by the two IRs' response ratio. As in phase 27
// (AutoSetLaneCuts.h), each lane's spectrum is re-weighted by that ratio, the
// lanes are added coherently as the engine mixes them, and the heard mix's
// bands move by the modelled change. An IR's delay counts: the two IRs do not
// arrive together, so a switch also moves the lane against the others.
// The choice is phase 23's: the other IR when the mix's balance (the amp
// step's tone error) improves by more than 0.5 dB; bass is kept; a switch
// that would cut the heard 44.5-70.7 Hz power by more than 3 dB and leave the
// 50-63 Hz bands more than 3 dB under the style's target is not made. Lanes
// are judged one after another, each with the IRs already chosen.
struct LaneIRChoice {
    int lane{};                       // index into the lanes passed in
    bool heard{};
    int before{},proposed{},otherSource{};
    float toneErrorBeforeDb{},toneErrorAfterDb{},levelChangeDb{};
    float lowEndChangeDb{},lowEndAfterDb{};bool lowEndHeld{};
    bool changed() const noexcept {return proposed!=before;}
};
struct LaneIRResult {
    bool playingDetected{},bass{};
    std::vector<LaneIRChoice> lanes;
    bool changed() const noexcept {for(const auto& l:lanes)if(l.changed())return true;return false;}
};
// A switch may not be modelled as more than this per bin (an IR's nulls) or per band.
inline constexpr double laneIRBinLimit=31.6,laneIRBandLimitDb=24.;

// An IR's first channel as the cabinet plays it at `rate`: resampled when its
// own rate differs, then scaled as juce::dsp::Convolution's Normalise::yes
// (0.125 / sqrt of its energy). Trim::no: its delay is kept.
inline std::vector<float> laneIRLoaded(const juce::AudioBuffer<float>& ir,double irRate,double rate) {
    std::vector<float> out;
    if(ir.getNumChannels()<1 || ir.getNumSamples()<1 || !std::isfinite(irRate) || irRate<8000 || rate<8000)return out;
    const int n=ir.getNumSamples();
    if(std::abs(irRate-rate)<1e-6)out.assign(ir.getReadPointer(0),ir.getReadPointer(0)+n);
    else {
        const double ratio=irRate/rate;
        out.resize(size_t(std::ceil(n/ratio)));
        std::vector<float> padded(size_t(n)+64,0.f);std::copy(ir.getReadPointer(0),ir.getReadPointer(0)+n,padded.begin());
        juce::LagrangeInterpolator resampler;
        resampler.process(ratio,padded.data(),out.data(),int(out.size()),int(padded.size()),0);
    }
    double energy=0;for(const float v:out)energy+=double(v)*v;
    if(energy<=0){out.clear();return out;}
    const float scale=float(.125/std::sqrt(energy));
    for(auto& v:out)v*=scale;
    return out;
}

// lanes, weights: as analyseLaneCuts. candidates: the lanes whose IR may be
// switched, with the source each plays now (1 V30 / SM57, 2 Jensen / SM57).
// factory: the two factory IRs as loaded at `rate` (laneIRLoaded).
// evaluate (tests): no choice; every candidate is switched and predicted.
inline LaneIRResult analyseLaneIR(const std::vector<const std::vector<float>*>& lanes,const std::vector<double>& weights,
                                  const std::vector<std::pair<int,int>>& candidates,double rate,const std::array<std::vector<float>,2>& factory,
                                  std::optional<bool> bass={},Style style=Style::rock,bool evaluate=false) {
    LaneIRResult r;
    if(lanes.empty() || lanes.size()!=weights.size() || rate<8000 || factory[0].empty() || factory[1].empty())return r;
    size_t length=size_t(rate*laneCutsMaxSeconds);
    for(const auto* l:lanes)length=std::min(length,l->size());
    if(length<size_t(rate))return r;
    std::vector<float> mix(length,0.f);
    for(size_t k=0;k<lanes.size();++k)for(size_t n=0;n<length;++n)mix[n]+=float(weights[k])*(*lanes[k])[n];
    const auto heard=measureSpectrum(mix,rate,bass);
    r.playingDetected=heard.playing;
    if(!r.playingDetected)return r;
    r.bass=heard.bass;
    // One FFT of each lane and of each IR, on the same bins.
    int order=1;while((size_t(1)<<order)<std::max(length+size_t(rate*.25),std::max(factory[0].size(),factory[1].size())))++order;
    const size_t size=size_t(1)<<order;
    juce::dsp::FFT fft(order);
    std::vector<float> buffer(2*size);
    const auto transform=[&](const float* x,size_t n) {
        std::fill(buffer.begin(),buffer.end(),0.f);std::copy(x,x+std::ptrdiff_t(n),buffer.begin());
        fft.performRealOnlyForwardTransform(buffer.data(),true);
        std::vector<std::complex<double>> bins(size/2+1);
        for(size_t b=0;b<=size/2;++b)bins[b]={buffer[2*b],buffer[2*b+1]};
        return bins;
    };
    std::vector<std::vector<std::complex<double>>> spectra;std::vector<double> lanePower(lanes.size(),0);
    for(size_t k=0;k<lanes.size();++k) {
        for(size_t n=0;n<length;++n)lanePower[k]+=double((*lanes[k])[n])*(*lanes[k])[n];
        spectra.push_back(transform(lanes[k]->data(),length));
    }
    const std::array<std::vector<std::complex<double>>,2> response{transform(factory[0].data(),factory[0].size()),transform(factory[1].data(),factory[1].size())};
    // What a switch from source `from` multiplies a lane's bin by.
    const auto ratio=[&](int from,size_t bin) {
        const auto now=response[size_t(from-1)][bin],next=response[size_t(2-from)][bin];
        const double a=std::abs(now),b=std::abs(next);
        if(a<=0)return std::complex<double>(1.,0.);
        const auto q=next/now;
        return b>a*laneIRBinLimit?q*(laneIRBinLimit*a/b):q;
    };
    const double strongestLane=*std::max_element(lanePower.begin(),lanePower.end());
    std::vector<int> captured(lanes.size(),0),chosen(lanes.size(),0);
    for(const auto& [lane,source]:candidates)if(lane>=0 && size_t(lane)<lanes.size() && (source==1 || source==2))captured[size_t(lane)]=chosen[size_t(lane)]=source;
    // The mix's power over bins [from,to) with the given source on each lane.
    const auto power=[&](size_t from,size_t to,const std::vector<int>& sources) {
        double p=0;
        for(size_t bin=from;bin<to;++bin) {
            std::complex<double> y{};
            for(size_t k=0;k<spectra.size();++k) {
                auto v=spectra[k][bin]*weights[k];
                if(sources[k]!=captured[k])v*=ratio(captured[k],bin);
                y+=v;
            }
            p+=std::norm(y);
        }
        return p;
    };
    std::array<std::pair<size_t,size_t>,toneBandCount> bandBins{};
    std::array<double,toneBandCount> heardPower{};
    for(int b=0;b<toneBandCount;++b) {
        const double edge=std::pow(2.,1./6.),fc=toneBandCentres[size_t(b)];
        bandBins[size_t(b)]={std::max<size_t>(1,size_t(std::ceil(fc/edge*double(size)/rate))),std::min(size/2,size_t(std::ceil(fc*edge*double(size)/rate)))};
        heardPower[size_t(b)]=power(bandBins[size_t(b)].first,bandBins[size_t(b)].second,captured);
    }
    const size_t lowFrom=size_t(std::ceil(cabIRLowEdgeLo*double(size)/rate)),lowTo=size_t(std::ceil(cabIRLowEdgeHi*double(size)/rate));
    const double lowHeard=power(lowFrom,lowTo,captured);
    // The predicted mix for a set of sources, and the modelled change per band.
    const auto predict=[&](const std::vector<int>& sources,std::array<float,toneBandCount>& move) {
        move.fill(0.f);
        for(int b=0;b<toneBandCount;++b) {
            if(toneBandCentres[size_t(b)]>rate*.4 || heardPower[size_t(b)]<=0)continue;
            const double change=10*std::log10(std::max(power(bandBins[size_t(b)].first,bandBins[size_t(b)].second,sources),1e-30)/heardPower[size_t(b)]);
            move[size_t(b)]=float(std::clamp(change,-laneIRBandLimitDb,laneIRBandLimitDb));
        }
        const std::array<float,toneBandCount> zero{};
        return cabPredicted(heard,zero,move);
    };
    std::array<float,toneBandCount> move{};
    float errorNow=ampToneErrorDb(predict(chosen,move),style);
    for(const auto& [lane,source]:candidates) {
        if(lane<0 || size_t(lane)>=lanes.size() || (source!=1 && source!=2))continue;
        LaneIRChoice c;c.lane=lane;c.before=c.proposed=source;c.otherSource=3-source;
        c.toneErrorBeforeDb=c.toneErrorAfterDb=errorNow;
        c.heard=lanePower[size_t(lane)]>0 && 10*std::log10(lanePower[size_t(lane)]/strongestLane)>laneCutsSilentDb;
        if(!c.heard || (r.bass && !evaluate)){r.lanes.push_back(c);continue;}
        auto trial=chosen;trial[size_t(lane)]=c.otherSource;
        const auto predicted=predict(trial,move);
        const float e=ampToneErrorDb(predicted,style);
        // The low end, at the heard notes' own bins.
        const auto target=alignedTarget(predicted,style);
        double lowNow=0,wanted=0;
        for(int b=0;b<toneBandCount;++b)if(cabIRLowBand(b) && heard.bandsDb[size_t(b)]>=heard.strongestDb-cabIRLowRangeDb){lowNow+=std::pow(10.,heard.bandsDb[size_t(b)]/10.);wanted+=std::pow(10.,target[size_t(b)]/10.);}
        if(lowNow>0 && lowHeard>0) {
            c.lowEndChangeDb=float(10*std::log10(std::max(power(lowFrom,lowTo,trial),1e-30)/lowHeard));
            c.lowEndAfterDb=wanted>0?float(10*std::log10(lowNow*std::pow(10.,c.lowEndChangeDb/10.)/wanted)):0.f;
        }
        const bool closer=e<errorNow-cabIRMinGainDb;
        if(!evaluate && closer && c.lowEndChangeDb<-cabIRMaxLowLossDb && c.lowEndAfterDb<-cabIRMaxLowLossDb){c.lowEndHeld=true;c.toneErrorAfterDb=e;r.lanes.push_back(c);continue;}
        if(evaluate || closer) {
            c.proposed=c.otherSource;c.toneErrorAfterDb=e;
            double before=0,after=0;
            for(int b=0;b<toneBandCount;++b){const double p=std::pow(10.,heard.bandsDb[size_t(b)]/10.);before+=p;after+=p*std::pow(10.,(predicted.bandsDb[size_t(b)]-heard.bandsDb[size_t(b)])/10.);}
            c.levelChangeDb=before>0 && after>0?float(10*std::log10(after/before)):0.f;
            chosen[size_t(lane)]=c.otherSource;errorNow=e;
        }
        r.lanes.push_back(c);
    }
    return r;
}
} // namespace spectralforge::autoset
