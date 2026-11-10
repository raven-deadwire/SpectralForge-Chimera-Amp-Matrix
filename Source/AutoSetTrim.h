#pragma once
#include "AutoSetAmp.h"

namespace spectralforge::autoset {

// Phase 24: the native head's INPUT TRIM. Phase 8 sets GAIN toward the
// style's drive target and says "out of this knob's reach" when GAIN at its
// end cannot get there; phase 20 first looks for a channel that can. The input
// trim moves the level into the head, so it reaches past GAIN's ends. After
// the amp step, the head (at the knobs phase 8 left) is rendered on the
// captured playing at other trims to find the drive (ampCompression) each
// gives.
// - Kept when the drive is within phase 8's tolerance (+-8%) of the target.
// - Otherwise the trim is bisected toward the target, within 12 dB of where it
//   is (and the control's +-24 dB), on a 0.5 dB grid. It moves only when the
//   drive comes closer to the target by more than half the tolerance (4%).
// - OUTPUT LEVEL then holds the head's loudness: it moves by the loudness the
//   trim adds, measured on the same renders.
// - Kept when the playing changes less than 3 dB between soft and hard notes:
//   the drive measure reads 0 there, which is not the same as clean, so it
//   would push the trim up for nothing (as phase 20 holds the channel).
struct TrimResult {
    bool playingDetected{},bass{},rendered{},steady{};
    float before{},proposed{};               // INPUT TRIM, dB
    float target{},driveBefore{},driveAfter{};
    float levelChangeDb{};                   // the head's loudness at the proposed trim against now
    bool reachable{true};                    // the target is within tolerance at the proposed trim
    bool changed() const noexcept {return proposed!=before;}
};
inline constexpr float trimMaxMoveDb=12.f,trimLimitDb=24.f,trimGridDb=.5f,trimMinGain=ampCompressionTolerance*.5f;
// Trim (dB) -> the head's output for the analysed input.
using TrimRenderer=std::function<std::vector<float>(float)>;
// Loudness of the playing (blocks within 30 dB of the loudest), after the warm-up.
inline float trimLoudnessDb(const std::vector<float>& output,double rate) {
    const size_t block=size_t(std::max(1.,rate*.020)),start=size_t(rate*ampWarmupSeconds);
    std::vector<double> powers;
    for(size_t b=start;b+block<=output.size();b+=block){double y=0;for(size_t n=b;n<b+block;++n)y+=double(output[n])*output[n];powers.push_back(y/double(block));}
    if(powers.empty())return -300.f;
    const double loud=*std::max_element(powers.begin(),powers.end());
    double sum=0;int count=0;for(const double p:powers)if(p>loud*1e-3){sum+=p;++count;}
    return count>0 && sum>0?float(10*std::log10(sum/count)):-300.f;
}
// input: the playing entering the amp (at most ampRenderSeconds is rendered);
// post: the amp/cab output heard; current: the trim now.
inline TrimResult analyseTrim(const std::vector<float>& input,const std::vector<float>& post,double rate,float current,const TrimRenderer& render,
                              std::optional<bool> bass={},Style style=Style::rock) {
    TrimResult r;r.before=r.proposed=current;
    const auto heard=measureSpectrum(post,rate,bass);
    const std::vector<float> segment(input.begin(),input.begin()+std::ptrdiff_t(std::min(input.size(),size_t(rate*ampRenderSeconds))));
    r.playingDetected=heard.playing && measureSpectrum(segment,rate,true).playing;
    if(!r.playingDetected)return r;
    r.bass=heard.bass;r.target=ampCompressionTarget(r.bass,style);
    if(playingSpreadDb(segment,rate)<3.f){r.steady=true;return r;}
    const auto drive=[&](float trim)->std::optional<std::pair<float,float>> {
        const auto out=render(trim);
        if(!measureSpectrum(out,rate,r.bass).playing)return std::nullopt;
        return std::pair{ampCompression(segment,out,rate),trimLoudnessDb(out,rate)};
    };
    const auto now=drive(current);
    if(!now)return r;
    r.rendered=true;
    r.driveBefore=r.driveAfter=now->first;
    r.reachable=std::abs(now->first-r.target)<=ampCompressionTolerance;
    if(r.reachable)return r;
    // The drive rises with the trim; bisect toward the target.
    float low=current,high=current;
    if(now->first<r.target)high=std::min(trimLimitDb,current+trimMaxMoveDb);else low=std::max(-trimLimitDb,current-trimMaxMoveDb);
    for(int i=0;i<7;++i) {
        const float mid=.5f*(low+high);const auto d=drive(mid);
        // A silent render (trim far down) counts as too little drive.
        ((!d || d->first<r.target)?low:high)=mid;
    }
    const float snapped=std::clamp(std::round(.5f*(low+high)/trimGridDb)*trimGridDb,-trimLimitDb,trimLimitDb);
    if(snapped==current)return r;
    const auto reached=drive(snapped);
    if(!reached)return r;
    if(std::abs(reached->first-r.target)<std::abs(now->first-r.target)-trimMinGain) {
        r.proposed=snapped;r.driveAfter=reached->first;r.levelChangeDb=reached->second-now->second;
        r.reachable=std::abs(reached->first-r.target)<=ampCompressionTolerance;
    }
    return r;
}
} // namespace spectralforge::autoset
