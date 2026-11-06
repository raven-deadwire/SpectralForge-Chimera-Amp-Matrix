#pragma once
#include "AutoSet.h"
#include "PostNativeDSP.h"
#include <functional>

namespace spectralforge::autoset {

// Phase 16: the POST bus compressor (the native Console VCA, POST section 1,
// model 0), as glue: a few dB of gain reduction on the loud passages, slow
// attack so picking transients pass, auto release. A result step of the
// de-esser + limiter pass: the same pass captures the POST input (Tap::post).
// The capture is rendered offline through the plug-in's own Console VCA for
// each candidate threshold:
// - Glue: the mean gain reduction (input minus output, makeup at 0) over the
//   20 ms blocks within 10 dB of the loudest playing. The threshold is
//   bisected to the style's target (clean 1.5, rock 2.5, metal 3 dB), scaled
//   by strength.
// - Make-up: the mean reduction over all playing blocks, so the loudness
//   holds; the Final EQ and output pass that follows trims the rest.
// A compressor the player already has in keeps its ratio, attack and release;
// only the threshold and make-up move.
struct BusSetting {float threshold{-18},makeup{};int attack{5},release{4},ratio{0};};
struct BusResult {
    bool playingDetected{},wasOn{},reachable{true};
    BusSetting before{},proposed{};
    float glueBeforeDb{},glueAfterDb{},glueTargetDb{};
    bool changed() const noexcept {return !wasOn || proposed.threshold!=before.threshold || proposed.makeup!=before.makeup;}
};
inline constexpr double busRenderSeconds=4,busWarmupSeconds=.5;
inline constexpr float busToleranceDb=.5f,busMinimumThreshold=-48,busMaximumThreshold=0;
inline constexpr int busThresholdControl=0,busMakeupControl=1,busAttackControl=2,busReleaseControl=3,busRatioControl=4,busInControl=5,busSidechainControl=6;
inline float busGlueTarget(Style style) noexcept {return style==Style::clean?1.5f:style==Style::metal?3.f:2.5f;}
// Mono input and threshold -> Console VCA output (make-up 0), for a ratio,
// attack and release (a BusRendererFor makes one per setting).
using BusRenderer=std::function<std::vector<float>(float threshold)>;
using BusRendererFor=std::function<BusRenderer(const BusSetting&)>;
struct BusGlue {float loudDb{},meanDb{};bool measured{};};
// Gain reduction (dB) per 20 ms block, after the warm-up: mean over the loud
// blocks (within 10 dB of the 95th percentile) and over all playing blocks.
inline BusGlue busGlue(const std::vector<float>& input,const std::vector<float>& output,double rate) {
    BusGlue g;
    const size_t block=size_t(std::max(1.,rate*.020)),start=size_t(rate*busWarmupSeconds);
    std::vector<float> in,reduction;
    for(size_t b=start;b+block<=std::min(input.size(),output.size());b+=block) {
        double x=0,y=0;for(size_t n=b;n<b+block;++n){x+=double(input[n])*input[n];y+=double(output[n])*output[n];}
        const float inDb=toDb(std::sqrt(x/double(block))),outDb=toDb(std::sqrt(y/double(block)));
        if(inDb>minimumPlayingDb){in.push_back(inDb);reduction.push_back(std::max(0.f,inDb-outDb));}
    }
    if(in.size()<20)return g;
    const float loud=percentile(in,.95f);double loudSum=0,allSum=0;int loudCount=0;
    for(size_t i=0;i<in.size();++i){allSum+=reduction[i];if(in[i]>loud-10.f){loudSum+=reduction[i];++loudCount;}}
    g.loudDb=loudCount?float(loudSum/loudCount):0.f;g.meanDb=float(allSum/double(in.size()));g.measured=true;
    return g;
}
// input: the POST input (mono); current: the Console VCA's settings and whether it is in.
// renderFor: the segment through the Console VCA at a setting (the caller
// renders `input` cut to the first busRenderSeconds).
inline BusResult analyseBus(const std::vector<float>& input,double rate,const BusSetting& current,bool on,const BusRendererFor& renderFor,Style style,float strength=1) {
    BusResult r;r.before=current;r.proposed=current;r.wasOn=on;
    std::vector<float> segment(input.begin(),input.begin()+std::ptrdiff_t(std::min(input.size(),size_t(rate*busRenderSeconds))));
    if(!on) {
        // A new compressor: glue settings by style.
        r.proposed.ratio=style==Style::metal?1:0;            // 4:1 / 2:1
        r.proposed.attack=style==Style::metal?4:5;           // 10 ms / 30 ms
        r.proposed.release=4;                                // AUTO
    }
    const auto render=renderFor(r.proposed);
    const auto renderWith=[&](float threshold){return busGlue(segment,render(threshold),rate);};
    const auto now=renderWith(current.threshold);
    r.playingDetected=now.measured;
    if(!r.playingDetected)return r;
    r.glueBeforeDb=on?now.loudDb:0.f;r.glueTargetDb=busGlueTarget(style)*strength;
    BusGlue reached=now;float threshold=current.threshold;
    if(!on || std::abs(now.loudDb-r.glueTargetDb)>busToleranceDb) {
        // Reduction falls as the threshold rises: bisect toward the target.
        float low=busMinimumThreshold,high=busMaximumThreshold;
        for(int i=0;i<8;++i){const float mid=.5f*(low+high);(renderWith(mid).loudDb>r.glueTargetDb?low:high)=mid;}
        threshold=std::round(.5f*(low+high)*2.f)*.5f;
        reached=renderWith(threshold);
        r.reachable=std::abs(reached.loudDb-r.glueTargetDb)<=busToleranceDb;
    }
    r.proposed.threshold=threshold;
    r.proposed.makeup=std::clamp(std::round(reached.meanDb*2.f)*.5f,0.f,20.f);
    r.glueAfterDb=reached.loudDb;
    if(on && r.proposed.threshold==current.threshold)r.proposed.makeup=current.makeup; // nothing moved
    return r;
}
// Renders the input (both channels the same) through a fresh native POST bus
// section with the Console VCA in, at the given threshold and make-up 0.
inline BusRenderer nativeBusRendererAt(PostNativeSectionState state,const BusSetting& setting,std::vector<float> input,double rate) {
    state.nativeEnabled=true;state.selected=0;
    auto& bank=state.banks[0];bank.bypass=false;
    bank.values[busMakeupControl]=0;bank.values[busAttackControl]=float(setting.attack);bank.values[busReleaseControl]=float(setting.release);
    bank.values[busRatioControl]=float(setting.ratio);bank.values[busInControl]=1;bank.values[busSidechainControl]=0;
    return [state,input=std::move(input),rate](float threshold) {
        auto s=state;s.banks[0].values[busThresholdControl]=threshold;
        constexpr int block=512;
        auto dsp=std::make_unique<PostNativeDSP>();dsp->prepare({rate,juce::uint32(block),2});
        std::vector<float> out(input.size());juce::AudioBuffer<float> buffer(2,block);
        for(size_t offset=0;offset<input.size();offset+=block) {
            const int count=int(std::min(size_t(block),input.size()-offset));
            buffer.setSize(2,count,false,false,true);
            for(int c=0;c<2;++c)buffer.copyFrom(c,0,input.data()+offset,count);
            dsp->processSection(buffer,0,s);
            std::copy(buffer.getReadPointer(0),buffer.getReadPointer(0)+count,out.begin()+std::ptrdiff_t(offset));
        }
        return out;
    };
}
inline BusRendererFor nativeBusRenderer(PostNativeSectionState state,std::vector<float> input,double rate) {
    return [state,input=std::move(input),rate](const BusSetting& setting)->BusRenderer {return nativeBusRendererAt(state,setting,input,rate);};
}
} // namespace spectralforge::autoset
