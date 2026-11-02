#pragma once
#include "AutoSet.h"

namespace spectralforge::autoset {

// Phase 11: the POST delay and reverb, from how the player plays. A result
// step of the input pass: the same 10 ms windows of the playing after input
// gain give the note onsets (how fast and how regularly the notes come), the
// pauses between phrases and how long notes ring.
// - Reverb: a style amount (clean wide, metal tight), less for dense playing
//   whose notes would wash into each other, more for sparse playing.
// - Delay: for clean and rock guitar that is not dense, only with an explicit
//   synced tempo. Onset gaps alone cannot identify the musical beat/subdivision.
struct PlayingFeel {
    bool measured{};
    float onsetsPerSecond{},beatMs{},pauseFraction{},sustainMs{};
    float peakOnsetsPerSecond{};
    int onsets{};
};
struct SpaceSetting {bool on{};float mix{},size{},damping{},timeMs{},feedback{};};
struct SpaceResult {
    PlayingFeel feel{};
    bool dense{},sparse{};
    SpaceSetting reverb{},delay{};
};
inline constexpr float spaceOnsetRiseDb=6.f,spaceDenseOnsets=6.f,spaceSparseOnsets=2.5f,spaceDelayMaxOnsets=4.f;
inline constexpr double spaceRefractoryMs=80,spaceMinBeatMs=250,spaceMaxBeatMs=1000;
// Onsets: a window at least 6 dB above the quietest of the three before it,
// within 24 dB of the loudest playing, at most one per 80 ms.
inline PlayingFeel measurePlayingFeel(const std::vector<Window>& w,double windowMs=10) {
    PlayingFeel f;
    if(w.size()<100 || !std::isfinite(windowMs) || windowMs<=0)return f;
    for(const auto& x:w)if(!std::isfinite(x.rmsDb))return f;
    std::vector<float> rms;for(const auto& x:w)rms.push_back(x.rmsDb);
    const float loud=percentile(rms,.95f);
    if(loud<minimumPlayingDb)return f;
    std::vector<size_t> onsets;
    for(size_t i=0;i<w.size();++i) {
        float before=-120.f;
        if(i>=3)before=std::min({rms[i-1],rms[i-2],rms[i-3]});
        if(rms[i]>loud-24.f && rms[i]-before>=spaceOnsetRiseDb && (onsets.empty() || double(i-onsets.back())*windowMs>=spaceRefractoryMs))onsets.push_back(i);
    }
    size_t pauses=0;for(const float r:rms)pauses+=r<loud-30.f;
    f.pauseFraction=float(pauses)/float(rms.size());
    f.onsets=int(onsets.size());
    f.onsetsPerSecond=float(onsets.size()/(double(w.size())*windowMs*.001));
    // A long rest must not make a fast phrase eligible for a new delay.
    // Half-open one-second windows avoid counting both boundary onsets.
    for(size_t left=0,right=0;right<onsets.size();++right) {
        while(double(onsets[right]-onsets[left])*windowMs>=1000.)++left;
        f.peakOnsetsPerSecond=std::max(f.peakOnsetsPerSecond,float(right-left+1));
    }
    // Ring: from each onset's peak (within 50 ms) to 20 dB below it, at most 2 s.
    std::vector<float> rings,gaps;
    for(size_t k=0;k<onsets.size();++k) {
        size_t peak=onsets[k];for(size_t i=onsets[k];i<std::min(w.size(),onsets[k]+5);++i)if(rms[i]>rms[peak])peak=i;
        const size_t end=k+1<onsets.size()?onsets[k+1]:w.size();size_t i=peak;
        while(i<end && rms[i]>rms[peak]-20.f && double(i-peak)*windowMs<2000)++i;
        if(i<end)rings.push_back(float(double(i-peak)*windowMs));
        if(k>0)gaps.push_back(float(double(onsets[k]-onsets[k-1])*windowMs));
    }
    if(!rings.empty())f.sustainMs=percentile(rings,.5f);
    if(gaps.size()>=3) {
        // Diagnostic pulse candidate only, NOT a confident musical BPM.
        // Even perfectly regular eighths and quarters are indistinguishable.
        double beat=percentile(gaps,.5f);
        while(beat<spaceMinBeatMs)beat*=2;
        while(beat>spaceMaxBeatMs)beat*=.5;
        f.beatMs=float(beat);
    }
    f.measured=f.onsets>=2;
    return f;
}
inline SpaceResult analyseSpace(const std::vector<Window>& windows,bool bass,Style style,float syncedTempoBpm=0) {
    SpaceResult r;r.feel=measurePlayingFeel(windows);
    if(!r.feel.measured)return r;
    r.dense=r.feel.onsetsPerSecond>spaceDenseOnsets;
    r.sparse=r.feel.onsetsPerSecond<spaceSparseOnsets;
    const float density=r.dense?.6f:r.sparse?1.25f:1.f;
    // Reverb by style; bass much drier.
    const float mix=bass?(style==Style::clean?.08f:style==Style::metal?.03f:.05f):(style==Style::clean?.20f:style==Style::metal?.07f:.12f);
    r.reverb.mix=std::clamp(std::round(mix*density*100.f)/100.f,0.f,.6f);
    r.reverb.size=std::clamp((style==Style::clean?.5f:style==Style::metal?.25f:.35f)+(r.sparse?.1f:r.dense?-.1f:0.f),0.f,1.f);
    r.reverb.damping=style==Style::clean?.45f:style==Style::metal?.65f:.55f;
    r.reverb.on=r.reverb.mix>=.03f;
    // Fail closed without a user-selected sync reference. IOI regularity is
    // not confidence in metrical tempo (half/double and triplet ambiguity).
    // Accept only the configured tempo range; never fold an out-of-range BPM.
    if(!bass && style!=Style::metal && r.feel.peakOnsetsPerSecond<=spaceDelayMaxOnsets
       && std::isfinite(syncedTempoBpm) && syncedTempoBpm>=40.f && syncedTempoBpm<=240.f) {
        r.delay.on=true;r.delay.timeMs=60000.f/syncedTempoBpm;
        r.delay.feedback=style==Style::clean?.3f:.22f;
        r.delay.mix=std::round((style==Style::clean?.15f:.10f)*(r.sparse?1.2f:1.f)*100.f)/100.f;
    }
    return r;
}
} // namespace spectralforge::autoset
