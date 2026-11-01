#pragma once
#include "AutoSetAmp.h"
#include "CabLayoutModel.h"

namespace spectralforge::autoset {

// Phase 9: the modeled cabinet mic's position (cap to edge) and distance.
// The pass hears the amp/cab output (Tap::tone). The modeled cabinet's
// response is known exactly (cabLayout::response, the curve its IR is made
// from), so a candidate placement predicts what would be heard: the heard
// spectrum plus the candidate's response minus the current one, band by band.
// - The placement whose prediction sits closest to the Tone EQ target wins,
//   if it improves the fit clearly. The Tone EQ pass then corrects the rest.
// - Moving the mic also changes the level and can cut deep notches (the cones
//   interfere at a distance): candidates rougher than the current placement are
//   ruled out, and the mic's level keeps the loudness where it was.
struct CabPlacement {double position{},distanceCm{};bool operator==(const CabPlacement&) const=default;};
struct CabResult {
    bool playingDetected{},bass{};
    CabPlacement before{},proposed{};
    float toneErrorBeforeDb{},toneErrorAfterDb{};
    float roughnessBeforeDb{},roughnessAfterDb{};
    Spectrum heard{};
    bool changed() const noexcept {return !(proposed==before);}
};
inline constexpr double cabPositionMaxMove=.35,cabPositionStep=.05,cabDistanceFactor=2,cabDistanceStepCm=.5;
inline constexpr int cabDistanceSteps=9;
inline constexpr float cabToneMinGainDb=.5f,cabRoughnessAllowanceDb=1.f;

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
// What would be heard with the model's bands changed from `now` to `next`.
inline Spectrum cabPredicted(const Spectrum& heard,const std::array<float,toneBandCount>& now,const std::array<float,toneBandCount>& next) {
    auto s=heard;s.strongestDb=-300;
    for(int b=0;b<toneBandCount;++b){s.bandsDb[size_t(b)]+=next[size_t(b)]-now[size_t(b)];s.strongestDb=std::max(s.strongestDb,s.bandsDb[size_t(b)]);}
    return s;
}
// Level change (dB) of the playing heard, 80 Hz-6.3 kHz, if the mic moved from current to next.
inline float cabLevelChangeDb(const Spectrum& heard,const cabLayout::Settings& current,CabPlacement next) {
    const auto now=cabBandsDb(current),moved=cabBandsDb(withPlacement(current,next));
    double before=0,after=0;
    for(int b=0;b<toneBandCount;++b)if(cabFitBand(b)) {
        before+=std::pow(10.,heard.bandsDb[size_t(b)]/10.);
        after+=std::pow(10.,(heard.bandsDb[size_t(b)]+moved[size_t(b)]-now[size_t(b)])/10.);
    }
    return before>0 && after>0?float(10*std::log10(after/before)):0.f;
}
// post: the amp/cab output; current: the modeled mic that is heard.
inline CabResult analyseCab(const std::vector<float>& post,double rate,const cabLayout::Settings& current,std::optional<bool> bass={},Style style=Style::rock) {
    CabResult r;r.before={current.voice.base.position,current.voice.base.distanceCm};r.proposed=r.before;
    r.heard=measureSpectrum(post,rate,bass);
    r.playingDetected=r.heard.playing;
    if(!r.playingDetected)return r;
    r.bass=r.heard.bass;
    const auto now=cabBandsDb(current);
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
            if(const float e=ampToneErrorDb(cabPredicted(r.heard,now,bands),style);e<best){best=e;bestRoughness=roughness;chosen=candidate;}
        }
    }
    // Keep the mic unless the balance clearly improves.
    if(best<r.toneErrorBeforeDb-cabToneMinGainDb){r.proposed=chosen;r.toneErrorAfterDb=best;r.roughnessAfterDb=bestRoughness;}
    return r;
}
} // namespace spectralforge::autoset
