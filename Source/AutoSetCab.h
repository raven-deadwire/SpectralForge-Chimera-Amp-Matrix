#pragma once
#include "AutoSetAmp.h"
#include "CabLayoutModel.h"

namespace spectralforge::autoset {

// Phase 9: the modeled cabinet mic's position (cap to edge) and distance.
// The pass hears the amp/cab output (Tap::tone). The modeled cabinet's
// authored response (cabLayout::response) approximates the finite IR for
// tone fitting, so a candidate placement predicts what would be heard: the heard
// spectrum plus the candidate's response minus the current one, band by band.
// - The placement whose prediction sits closest to the Tone EQ target wins,
//   if it improves the fit clearly. The Tone EQ pass then corrects the rest.
// - Moving the mic also changes the level and can cut deep notches (the cones
//   interfere at a distance): candidates rougher than the current placement are
//   ruled out, and the mic's level keeps the loudness where it was.
struct CabPlacement {double position{},distanceCm{};bool operator==(const CabPlacement&) const=default;};
struct CabEnergySpectrum {
    double rate{};
    int order{};
    std::vector<double> power;
};
struct CabResult {
    bool playingDetected{},bass{};
    CabPlacement before{},proposed{};
    float toneErrorBeforeDb{},toneErrorAfterDb{};
    float roughnessBeforeDb{},roughnessAfterDb{};
    Spectrum heard{};
    CabEnergySpectrum energy;
    bool changed() const noexcept {return !(proposed==before);}
};
inline constexpr double cabPositionMaxMove=.35,cabPositionStep=.05,cabDistanceFactor=2,cabDistanceStepCm=.5;
inline constexpr int cabDistanceSteps=9;
inline constexpr float cabToneMinGainDb=.5f,cabRoughnessAllowanceDb=1.f;
// An average can hide a single deep array-cancellation notch. Bound the
// increase of each local peak/notch as well (relative to +/- 1/6 octave).
inline constexpr float cabLocalExtremaAllowanceDb=3.f;
inline constexpr int cabDetailStepsPerOctave=96,cabDetailNeighbour=16;
inline constexpr int cabDetailCount=786; // 27.5 Hz through 6.3 kHz, plus neighbours
using CabDetail=std::array<float,cabDetailCount>;
template<class Response> inline CabDetail cabDetailDb(Response response) {
    CabDetail db{};
    for(int k=0;k<cabDetailCount;++k) {
        const double hz=27.5*std::exp2(double(k-cabDetailNeighbour)/cabDetailStepsPerOctave);
        db[size_t(k)]=float(20*std::log10(std::max(1e-15,std::abs(response(hz)))));
    }
    return db;
}
inline CabDetail cabDetailDb(const cabLayout::Settings& s) {
    return cabDetailDb([&](double hz){return cabLayout::response(s,hz);});
}
inline float cabAddedExtremaDb(const CabDetail& before,const CabDetail& after) {
    float worst=0;
    for(int k=cabDetailNeighbour;k<cabDetailCount-cabDetailNeighbour;++k) {
        const auto curvature=[k](const auto& db){return .5f*(db[size_t(k-cabDetailNeighbour)]+db[size_t(k+cabDetailNeighbour)])-db[size_t(k)];};
        const float a=curvature(before),b=curvature(after);
        for(float sign:{-1.f,1.f})worst=std::max(worst,std::max(0.f,sign*b)-std::max(0.f,sign*a));
    }
    return worst;
}

inline cabLayout::Settings withPlacement(cabLayout::Settings s,CabPlacement p) {
    s.voice.base.position=p.position;s.voice.base.distanceCm=p.distanceCm;return s;
}
// Power of the model's response in each third-octave band (eight points per band).
inline std::array<float,toneBandCount> cabBandsDb(const cabLayout::Settings& s) {
    std::array<float,toneBandCount> bands{};
    for(int b=0;b<toneBandCount;++b) {
        double power=0;
        for(int k=0;k<8;++k)power+=std::norm(cabLayout::response(s,toneBandCentres[size_t(b)]*std::pow(2.,(k+.5)/24.-1./6.)));
        bands[size_t(b)]=float(10*std::log10(std::max(power/8,1e-30)));
    }
    return bands;
}
inline bool cabFitBand(int b) {const float fc=toneBandCentres[size_t(b)];return fc>=80.f && fc<=6300.f;}
// How far the bands stray from their neighbours' mean, 80 Hz-6.3 kHz (RMS): notches and peaks.
inline float cabRoughnessDb(const std::array<float,toneBandCount>& bands) {
    double sum=0;int count=0;
    for(int b=1;b+1<toneBandCount;++b)if(cabFitBand(b)){const double d=bands[size_t(b)]-.5*(bands[size_t(b-1)]+bands[size_t(b+1)]);sum+=d*d;++count;}
    return count?float(std::sqrt(sum/count)):0.f;
}
inline bool cabPlacementSafe(const cabLayout::Settings& current,CabPlacement next) {
    const auto moved=withPlacement(current,next);
    return cabRoughnessDb(cabBandsDb(moved))<=cabRoughnessDb(cabBandsDb(current))+cabRoughnessAllowanceDb
        && cabAddedExtremaDb(cabDetailDb(current),cabDetailDb(moved))<=cabLocalExtremaAllowanceDb;
}
// What would be heard with the model's bands changed from `now` to `next`.
inline Spectrum cabPredicted(const Spectrum& heard,const std::array<float,toneBandCount>& now,const std::array<float,toneBandCount>& next) {
    auto s=heard;s.strongestDb=-300;
    for(int b=0;b<toneBandCount;++b){s.bandsDb[size_t(b)]+=next[size_t(b)]-now[size_t(b)];s.strongestDb=std::max(s.strongestDb,s.bandsDb[size_t(b)]);}
    return s;
}
// Level needs the energy below the tone-fit range, including Drop A at 27.5 Hz,
// and the actual energy within a band (a note need not excite a whole band).
// Keep this measurement local to CAB; the shared Tone EQ targets are unchanged.
inline CabEnergySpectrum measureCabEnergy(const std::vector<float>& post,double rate) {
    CabEnergySpectrum result;
    if(!std::isfinite(rate) || rate<8000 || rate>384000)return result;
    result.rate=rate;result.order=std::clamp(int(std::round(std::log2(rate/1.5))),13,18);
    const int size=1<<result.order;
    juce::dsp::FFT fft(result.order);
    std::vector<float> frame(size_t(2*size)),window(static_cast<size_t>(size));
    result.power.resize(size_t(size/2+1));
    for(int n=0;n<size;++n)window[size_t(n)]=float(.5-.5*std::cos(2*originalCab::pi*n/size));
    for(size_t start=0;start+size_t(size)<=post.size();start+=size_t(size/2)) {
        for(int n=0;n<size;++n)frame[size_t(n)]=post[start+size_t(n)]*window[size_t(n)];
        std::fill(frame.begin()+size,frame.end(),0.f);
        fft.performRealOnlyForwardTransform(frame.data());
        for(int k=1;k<=size/2;++k)result.power[size_t(k)]+=std::norm(std::complex<double>(frame[size_t(2*k)],frame[size_t(2*k+1)]));
    }
    return result;
}
// The finite, tapered IR is what the player hears. In particular its lowest
// frequencies need not equal the continuous model used for the tone search.
// Evaluate only the applied placement's pair of IRs, not every search candidate.
inline float cabLevelChangeDb(const CabEnergySpectrum& heard,const cabLayout::Settings& current,CabPlacement next) {
    if(heard.power.empty() || next==CabPlacement{current.voice.base.position,current.voice.base.distanceCm})return 0;
    const int size=1<<heard.order;
    juce::dsp::FFT fft(heard.order);
    const auto response=[&](const cabLayout::Settings& settings) {
        const auto ir=cabLayout::generate(cabLayout::key(settings),heard.rate);
        std::vector<float> bins(size_t(2*size));
        std::copy(ir.begin(),ir.end(),bins.begin());
        fft.performRealOnlyForwardTransform(bins.data());
        return bins;
    };
    const auto now=response(current),moved=response(withPlacement(current,next));
    double before=0,after=0;
    for(int k=1;k<=size/2;++k) {
        const auto power=[k](const auto& bins){return std::norm(std::complex<double>(bins[size_t(2*k)],bins[size_t(2*k+1)]));};
        const double weight=k==size/2?.5:1.;
        before+=weight*heard.power[size_t(k)];
        after+=weight*heard.power[size_t(k)]*power(moved)/std::max(1e-24,power(now));
    }
    return before>0 && after>0?float(10*std::log10(after/before)):0.f;
}
// post: the amp/cab output; current: the modeled mic that is heard.
inline CabResult analyseCab(const std::vector<float>& post,double rate,const cabLayout::Settings& current,std::optional<bool> bass={},Style style=Style::rock) {
    CabResult r;r.before={current.voice.base.position,current.voice.base.distanceCm};r.proposed=r.before;
    r.heard=measureSpectrum(post,rate,bass);
    r.playingDetected=r.heard.playing;
    if(!r.playingDetected)return r;
    r.energy=measureCabEnergy(post,rate);
    r.bass=r.heard.bass;
    const auto now=cabBandsDb(current);
    const auto detail=cabDetailDb(current);
    r.roughnessBeforeDb=cabRoughnessDb(now);r.roughnessAfterDb=r.roughnessBeforeDb;
    r.toneErrorBeforeDb=ampToneErrorDb(r.heard,style);r.toneErrorAfterDb=r.toneErrorBeforeDb;
    float best=r.toneErrorBeforeDb,bestRoughness=r.roughnessBeforeDb;CabPlacement chosen=r.before;
    const double low=std::max(2.,r.before.distanceCm/cabDistanceFactor),high=std::min(60.,r.before.distanceCm*cabDistanceFactor);
    for(int i=0;i<cabDistanceSteps;++i) {
        // Log-spaced from half to twice the distance, on the 0.5 cm grid.
        const double distance=std::clamp(std::round(low*std::pow(high/low,i/double(cabDistanceSteps-1))/cabDistanceStepCm)*cabDistanceStepCm,2.,60.);
        for(double position=std::max(0.,r.before.position-cabPositionMaxMove);position<=std::min(1.,r.before.position+cabPositionMaxMove)+1e-9;position+=cabPositionStep) {
            const CabPlacement candidate{std::round(position*1000)/1000,distance};
            if(candidate==r.before)continue;
            const auto bands=cabBandsDb(withPlacement(current,candidate));
            const float roughness=cabRoughnessDb(bands);
            if(roughness>r.roughnessBeforeDb+cabRoughnessAllowanceDb)continue;
            if(const float e=ampToneErrorDb(cabPredicted(r.heard,now,bands),style);e<best) {
                if(cabAddedExtremaDb(detail,cabDetailDb(withPlacement(current,candidate)))>cabLocalExtremaAllowanceDb)continue;
                best=e;bestRoughness=roughness;chosen=candidate;
            }
        }
    }
    // Keep the mic unless the balance clearly improves.
    if(best<r.toneErrorBeforeDb-cabToneMinGainDb){r.proposed=chosen;r.toneErrorAfterDb=best;r.roughnessAfterDb=bestRoughness;}
    return r;
}
} // namespace spectralforge::autoset
