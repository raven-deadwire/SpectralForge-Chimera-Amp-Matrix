#pragma once
#include "AutoSetCab.h"
#include "AutoSetSpace.h"
#include "PerformanceUtilities.h"

namespace spectralforge::autoset {

// Phase 14: the stereo doubler (DOUBLER on/off and its spread). A result step:
// the rig sound a sample pass already heard (Tone EQ input, else the cabinet
// output) is rendered offline through the plug-in's own doubler for each
// candidate spread, and the playing the input pass heard says how fast the
// notes come.
// - Width: 1 - the correlation of left and right. The doubler's mix is fixed,
//   so above a few ms the spread changes the width little; it is checked, not
//   targeted (a spread must widen audibly).
// - Mono: summed to mono, the delayed copy combs. The roughness of the mono
//   sum against the dry sound (third-octave bands, 80 Hz-6.3 kHz) is what the
//   spread changes most: the least rough spread in the style's range wins, so
//   the guitar survives a mono speaker.
// - The spread range follows the style (metal tight, clean wide: a longer
//   delay is heard as a second player); fast playing keeps a doubler off (a
//   delayed copy smears quick notes) and uses the shorter half of the range
//   for one that is on. Bass keeps the doubler as it is: its low end stays centred.
struct WidthCandidate {float spreadMs{},width{},monoRoughnessDb{};};
struct WidthResult {
    bool playingDetected{},bass{},fast{},feelMeasured{};
    bool onBefore{},onAfter{};
    float spreadBefore{},spreadAfter{};
    float widthBefore{},widthAfter{},monoBeforeDb{},monoAfterDb{};
    std::vector<WidthCandidate> candidates;
    bool changed() const noexcept {return onAfter!=onBefore || spreadAfter!=spreadBefore;}
};
inline constexpr double widthRenderSeconds=3,widthWarmupSeconds=.25;
inline constexpr float widthMonoLimitDb=1.5f,widthMinimum=.08f,widthFastOnsets=6.f,widthStepMs=2.f,widthKeepDb=.3f;
inline std::pair<float,float> widthSpreadRange(Style style) noexcept {
    return style==Style::metal?std::pair{4.f,10.f}:style==Style::clean?std::pair{8.f,18.f}:std::pair{6.f,14.f};
}
// The plug-in's doubler on a mono input: left and right outputs.
inline std::pair<std::vector<float>,std::vector<float>> renderDoubler(const std::vector<float>& mono,double rate,float spreadMs) {
    PerformanceUtilities doubler;constexpr int block=512;
    doubler.prepare({rate,juce::uint32(block),2});
    std::vector<float> left(mono.size()),right(mono.size());
    juce::AudioBuffer<float> buffer(2,block);
    for(size_t offset=0;offset<mono.size();offset+=block) {
        const int count=int(std::min(size_t(block),mono.size()-offset));
        buffer.setSize(2,count,false,false,true);
        for(int c=0;c<2;++c)buffer.copyFrom(c,0,mono.data()+offset,count);
        doubler.process(buffer,120,true,spreadMs,false,false);
        std::copy(buffer.getReadPointer(0),buffer.getReadPointer(0)+count,left.begin()+std::ptrdiff_t(offset));
        std::copy(buffer.getReadPointer(1),buffer.getReadPointer(1)+count,right.begin()+std::ptrdiff_t(offset));
    }
    return {left,right};
}
// Width and mono roughness of the doubler at a spread, after the mix ramp.
inline WidthCandidate measureDoubler(const std::vector<float>& mono,double rate,float spreadMs,const Spectrum& dry) {
    WidthCandidate c;c.spreadMs=spreadMs;
    const auto [left,right]=renderDoubler(mono,rate,spreadMs);
    const size_t start=std::min(mono.size(),size_t(rate*widthWarmupSeconds));
    double ll=0,rr=0,lr=0;
    std::vector<float> sum(mono.size()-start);
    for(size_t n=start;n<mono.size();++n){ll+=double(left[n])*left[n];rr+=double(right[n])*right[n];lr+=double(left[n])*right[n];sum[n-start]=.5f*(left[n]+right[n]);}
    c.width=ll>0 && rr>0?float(1-lr/std::sqrt(ll*rr)):0.f;
    const auto summed=measureSpectrum(sum,rate,dry.bass);
    std::array<float,toneBandCount> difference{};
    for(int b=0;b<toneBandCount;++b)difference[size_t(b)]=summed.bandsDb[size_t(b)]-dry.bandsDb[size_t(b)];
    c.monoRoughnessDb=cabRoughnessDb(difference);
    return c;
}
// rig: the mono rig sound (at most widthRenderSeconds are rendered); windows:
// the input pass's playing (empty: not heard); on/spread: the doubler now.
inline WidthResult analyseWidth(const std::vector<float>& rig,double rate,const std::vector<Window>& windows,bool on,float spreadMs,
                                std::optional<bool> bass={},Style style=Style::rock) {
    WidthResult r;r.onBefore=r.onAfter=on;r.spreadBefore=r.spreadAfter=spreadMs;
    std::vector<float> segment(rig.begin(),rig.begin()+std::ptrdiff_t(std::min(rig.size(),size_t(rate*widthRenderSeconds))));
    const auto dry=measureSpectrum(std::vector<float>(segment.begin()+std::ptrdiff_t(std::min(segment.size(),size_t(rate*widthWarmupSeconds))),segment.end()),rate,bass);
    r.playingDetected=dry.playing;
    if(!r.playingDetected)return r;
    r.bass=dry.bass;
    const auto feel=measurePlayingFeel(windows);
    r.feelMeasured=feel.measured;r.fast=feel.measured && feel.peakOnsetsPerSecond>widthFastOnsets;
    if(on){const auto now=measureDoubler(segment,rate,spreadMs,dry);r.widthBefore=now.width;r.monoBeforeDb=now.monoRoughnessDb;}
    r.widthAfter=r.widthBefore;r.monoAfterDb=r.monoBeforeDb;
    if(r.bass)return r;
    if(!on && r.fast)return r;
    auto [low,high]=widthSpreadRange(style);
    if(r.fast)high=low+.5f*(high-low); // quick notes: the shorter half
    const WidthCandidate* best=nullptr;
    for(float spread=low;spread<=high+1e-3f;spread+=widthStepMs)r.candidates.push_back(measureDoubler(segment,rate,spread,dry));
    for(const auto& c:r.candidates)
        if(c.width>=widthMinimum && c.monoRoughnessDb<=widthMonoLimitDb && (!best || c.monoRoughnessDb<best->monoRoughnessDb))best=&c;
    if(!best)return r; // every spread combs in mono, or barely widens
    // A doubler that is on keeps a spread in the range unless another is clearly smoother in mono.
    if(on && spreadMs>=low-1e-3f && spreadMs<=high+1e-3f && r.monoBeforeDb<=best->monoRoughnessDb+widthKeepDb)return r;
    r.onAfter=true;r.spreadAfter=best->spreadMs;r.widthAfter=best->width;r.monoAfterDb=best->monoRoughnessDb;
    return r;
}
} // namespace spectralforge::autoset
