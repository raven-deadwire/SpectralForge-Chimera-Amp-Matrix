#pragma once
#include "AutoSet.h"
#include "AmpNativeDSP.h"
#include <functional>

namespace spectralforge::autoset {

// Phase 8: the amp head's GAIN, BASS, MIDDLE and TREBLE. The pass captures the
// signal entering the amp (Tap::amp) and the amp/cab output (Tap::cab; Tap::tone on main) at the
// same time. Candidate knob settings are rendered offline through the same
// native amp DSP, so each decision is checked on the player's own playing:
// - GAIN: how much the amp compresses the playing's dynamics, against a style
//   target (clean amps keep the dynamics, metal amps flatten them).
// - BASS/MIDDLE/TREBLE: the rendered amp spectrum plus the cabinet's measured
//   response (amp/cab output minus the amp rendered at the current settings),
//   against the Tone EQ target. The Tone EQ pass then corrects what is left.
enum AmpRole {ampGain=0,ampBass=1,ampMiddle=2,ampTreble=3};
inline constexpr const char* ampRoleNames[]{"GAIN","BASS","MIDDLE","TREBLE"};
struct AmpKnob {bool present{};float value{},minimum{},maximum{1};};
struct AmpResult {
    bool playingDetected{},bass{};
    std::array<AmpKnob,4> before{},proposed{};
    float compressionBefore{},compressionAfter{},compressionTarget{};
    bool gainReachable{true};
    float toneErrorBeforeDb{},toneErrorAfterDb{};
    bool changed(int role) const noexcept {return proposed[size_t(role)].present && proposed[size_t(role)].value!=before[size_t(role)].value;}
    int changedKnobs() const noexcept {int n=0;for(int r=0;r<4;++r)n+=changed(r);return n;}
};
inline constexpr double ampRenderSeconds=3,ampWarmupSeconds=.25;
inline constexpr float ampCompressionTolerance=.08f,ampToneMaxMove=.3f,ampToneMinGainDb=.5f;
// Fraction of the playing's level spread the amp removes.
inline float ampCompressionTarget(bool bass,Style style) noexcept {
    if(bass)return style==Style::clean?.10f:style==Style::metal?.50f:.30f;
    return style==Style::clean?.15f:style==Style::metal?.65f:.45f;
}
// Level spread (90th - 20th percentile of 20 ms block RMS) of the playing
// blocks in the input, and how much of it is left at the output: 0 keeps the
// dynamics, 1 flattens them. The warm-up is left out.
inline float ampCompression(const std::vector<float>& input,const std::vector<float>& output,double rate) {
    const size_t block=size_t(std::max(1.,rate*.020)),start=size_t(rate*ampWarmupSeconds);
    std::vector<float> in,out;
    for(size_t b=start;b+block<=std::min(input.size(),output.size());b+=block) {
        double x=0,y=0;
        for(size_t n=b;n<b+block;++n){x+=double(input[n])*input[n];y+=double(output[n])*output[n];}
        in.push_back(toDb(std::sqrt(x/double(block))));out.push_back(toDb(std::sqrt(y/double(block))));
    }
    if(in.size()<20)return 0;
    const float loud=percentile(in,.95f);
    std::vector<float> playingIn,playingOut;
    for(size_t i=0;i<in.size();++i)if(in[i]>loud-30.f && in[i]>minimumPlayingDb){playingIn.push_back(in[i]);playingOut.push_back(out[i]);}
    if(playingIn.size()<20)return 0;
    const float spreadIn=percentile(playingIn,.9f)-percentile(playingIn,.2f),spreadOut=percentile(playingOut,.9f)-percentile(playingOut,.2f);
    if(spreadIn<3.f)return 0;
    return std::clamp(1.f-spreadOut/spreadIn,0.f,1.f);
}
// Knob values (GAIN, BASS, MIDDLE, TREBLE) -> amp output for the analysed input.
using AmpRenderer=std::function<std::vector<float>(const std::array<float,4>&)>;
// RMS distance (one-octave smoothed, as the Tone EQ fit) of a spectrum from the
// target over the bands with content, 80 Hz-6.3 kHz.
inline float ampToneErrorDb(const Spectrum& predicted,Style style) {
    const auto target=alignedTarget(predicted,style);
    std::array<float,toneBandCount> deviation{},weight{};
    for(int b=0;b<toneBandCount;++b) {
        const float fc=toneBandCentres[size_t(b)];
        deviation[size_t(b)]=target[size_t(b)]-predicted.bandsDb[size_t(b)];
        weight[size_t(b)]=fc>=80.f && fc<=6300.f && predicted.bandsDb[size_t(b)]>predicted.strongestDb-45.f && deviation[size_t(b)]<20.f?1.f:0.f;
    }
    double sum=0,count=0;
    for(int b=0;b<toneBandCount;++b) {
        double s=0,w=0;
        for(int k=std::max(0,b-1);k<=std::min(toneBandCount-1,b+1);++k){const double kw=(k==b?2.:1.)*weight[size_t(k)];s+=kw*deviation[size_t(k)];w+=kw;}
        if(w>0 && weight[size_t(b)]>0){const double e=s/w;sum+=e*e;++count;}
    }
    return count>0?float(std::sqrt(sum/count)):0.f;
}
// input: the signal entering the amp (at most ampRenderSeconds is rendered);
// post: the amp/cab output heard at the same time; knobs: the current values.
inline AmpResult analyseAmp(const std::vector<float>& input,const std::vector<float>& post,double rate,const std::array<AmpKnob,4>& knobs,
                            const AmpRenderer& render,std::optional<bool> bass={},Style style=Style::rock) {
    AmpResult r;r.before=knobs;r.proposed=knobs;
    const auto heard=measureSpectrum(post,rate,bass);
    std::vector<float> segment(input.begin(),input.begin()+std::min(input.size(),size_t(rate*ampRenderSeconds)));
    r.playingDetected=heard.playing && measureSpectrum(segment,rate,true).playing;
    if(!r.playingDetected)return r;
    r.bass=heard.bass;r.compressionTarget=ampCompressionTarget(r.bass,style);
    std::array<float,4> initial{};for(int k=0;k<4;++k)initial[size_t(k)]=knobs[size_t(k)].value;
    auto values=initial;
    const auto compression=[&](const std::array<float,4>& v){return ampCompression(segment,render(v),rate);};
    r.compressionBefore=compression(values);r.compressionAfter=r.compressionBefore;
    const auto snap=[](const AmpKnob& k,float v){const float range=k.maximum-k.minimum;return std::clamp(k.minimum+std::round((v-k.minimum)/range*100.f)/100.f*range,k.minimum,k.maximum);};
    // GAIN: compression rises with the knob; bisect toward the target. Run
    // again after the tone knobs move, since the tone also changes the drive.
    const auto& g=knobs[ampGain];
    const auto setGain=[&](float current) {
        if(!g.present || g.maximum<=g.minimum || std::abs(current-r.compressionTarget)<=ampCompressionTolerance){r.gainReachable=true;return current;}
        float low=g.minimum,high=g.maximum;
        if(current<r.compressionTarget)low=values[ampGain];else high=values[ampGain];
        auto probe=values;
        for(int i=0;i<7;++i){probe[ampGain]=.5f*(low+high);(compression(probe)<r.compressionTarget?low:high)=probe[ampGain];}
        probe[ampGain]=snap(g,.5f*(low+high));
        const float reached=compression(probe);
        r.gainReachable=std::abs(reached-r.compressionTarget)<=ampCompressionTolerance;
        if(std::abs(reached-r.compressionTarget)<std::abs(current-r.compressionTarget)){values=probe;return reached;}
        return current;
    };
    r.compressionAfter=setGain(r.compressionBefore);
    // The cabinet (and everything else after the amp) as heard: post minus the
    // amp rendered at the settings that were playing.
    const auto renderedNow=measureSpectrum(render(initial),rate,r.bass);
    if(!renderedNow.playing){r.proposed[ampGain].value=values[ampGain];return r;}
    std::array<float,toneBandCount> after{};
    for(int b=0;b<toneBandCount;++b)after[size_t(b)]=heard.bandsDb[size_t(b)]-renderedNow.bandsDb[size_t(b)];
    const auto toneError=[&](const std::array<float,4>& v) {
        auto s=measureSpectrum(render(v),rate,r.bass);
        if(!s.playing)return 1e9f;
        s.strongestDb=-300;
        for(int b=0;b<toneBandCount;++b){s.bandsDb[size_t(b)]+=after[size_t(b)];s.strongestDb=std::max(s.strongestDb,s.bandsDb[size_t(b)]);}
        return ampToneErrorDb(s,style);
    };
    float best=toneError(values);
    r.toneErrorBeforeDb=best;
    // Coordinate search, coarse then fine, within +-30% of each knob's range.
    for(const float step:{.3f,.15f,.075f})for(int k=ampBass;k<=ampTreble;++k) {
        const auto& knob=knobs[size_t(k)];
        if(!knob.present || knob.maximum<=knob.minimum)continue;
        const float range=knob.maximum-knob.minimum;
        for(const float direction:{-1.f,1.f}) {
            auto probe=values;
            probe[size_t(k)]=snap(knob,std::clamp(values[size_t(k)]+direction*step*range,std::max(knob.minimum,knob.value-ampToneMaxMove*range),std::min(knob.maximum,knob.value+ampToneMaxMove*range)));
            if(probe[size_t(k)]==values[size_t(k)])continue;
            if(const float e=toneError(probe);e<best-.05f){best=e;values=probe;}
        }
    }
    // Keep the tone knobs unless the balance clearly improves.
    if(best<r.toneErrorBeforeDb-ampToneMinGainDb) {
        r.toneErrorAfterDb=best;
        r.compressionAfter=setGain(compression(values));
        if(g.present)r.toneErrorAfterDb=toneError(values);
    } else {
        for(int k=ampBass;k<=ampTreble;++k)values[size_t(k)]=initial[size_t(k)];
        r.toneErrorAfterDb=r.toneErrorBeforeDb;
    }
    for(int k=0;k<4;++k)r.proposed[size_t(k)].value=values[size_t(k)];
    return r;
}
// Renders the input through a fresh copy of the native amp with the given
// knob values placed on the given controls (-1: role not on this panel).
inline AmpRenderer nativeAmpRenderer(const AmpNativeState& state,const std::array<int,4>& controls,std::vector<float> input,double rate) {
    return [state,controls,input=std::move(input),rate](const std::array<float,4>& values) {
        auto s=state;
        for(int k=0;k<4;++k)if(controls[size_t(k)]>=0)s.values[size_t(controls[size_t(k)])]=values[size_t(k)];
        auto dsp=std::make_unique<AmpNativeDSP>();dsp->prepare(rate);dsp->set(s);dsp->reset();
        std::vector<float> out(input.size());
        for(size_t n=0;n<input.size();++n)out[n]=dsp->tick(input[n],0);
        return out;
    };
}
} // namespace spectralforge::autoset
