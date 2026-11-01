#pragma once
#include "AutoSetAmp.h"
#include "PedalBoardDSP.h"

namespace spectralforge::autoset {

// Phase 10: a boost pedal in front of a driven amp. With rock or metal gain,
// low notes going into the amp's distortion smear the low end; a mid boost
// with a bass cut in front of the amp tightens it. The pass captures the
// signal entering the amp (Tap::amp, after the pedal board) and renders it
// offline through the Green Drive pedal and a fresh copy of the native amp at
// its current settings, with and without the pedal:
// - Loose low end: the amp output's 80-200 Hz level relative to 400 Hz-
//   2.5 kHz, above the style target's same difference.
// - A pedal setting is used only when it tightens the low end clearly, without
//   driving the amp far past the style's target. The amp pass that follows
//   re-sets GAIN for the drive the pedal adds.
inline constexpr int boostPedalModel=1; // Green Drive: OVER DRIVE, TONE, LEVEL; 180 Hz input high pass
struct BoostSetting {float drive{},tone{.5f},level{.5f};};
struct BoostResult {
    bool playingDetected{},bass{},engaged{};
    float lowExcessBeforeDb{},lowExcessAfterDb{};
    float compressionBefore{},compressionAfter{},compressionTarget{};
    BoostSetting setting{};
};
inline constexpr float boostLooseLowDb=3.f,boostMinTightenDb=2.f,boostDriveAllowance=.15f;
inline constexpr std::array<float,2> boostDrives{0.f,.2f};
inline constexpr std::array<float,3> boostLevels{.6f,.8f,1.f};
// Amp output for the analysed input, with the pedal (a setting) or without it.
using BoostRenderer=std::function<std::vector<float>(const std::optional<BoostSetting>&)>;
inline float boostBandDb(const std::array<float,toneBandCount>& bands,float from,float to) {
    double power=0;
    for(int b=0;b<toneBandCount;++b)if(toneBandCentres[size_t(b)]>=from && toneBandCentres[size_t(b)]<=to)power+=std::pow(10.,bands[size_t(b)]/10.);
    return float(10*std::log10(std::max(power,1e-30)));
}
// The amp output's low end (80-200 Hz) against its mids (400 Hz-2.5 kHz),
// above the same difference of the style's guitar target.
inline float boostLowExcessDb(const Spectrum& s,Style style) {
    std::array<float,toneBandCount> target{};
    for(int b=0;b<toneBandCount;++b)target[size_t(b)]=toneTargetDb(false,toneBandCentres[size_t(b)],style);
    const auto balance=[](const std::array<float,toneBandCount>& bands){return boostBandDb(bands,80,200)-boostBandDb(bands,400,2500);};
    return balance(s.bandsDb)-balance(target);
}
// input: the signal entering the amp (at most ampRenderSeconds is rendered).
inline BoostResult analyseBoost(const std::vector<float>& input,double rate,const BoostRenderer& render,std::optional<bool> bass={},Style style=Style::rock) {
    BoostResult r;
    std::vector<float> segment(input.begin(),input.begin()+std::min(input.size(),size_t(rate*ampRenderSeconds)));
    const auto dry=render(std::nullopt);
    const auto heard=measureSpectrum(dry,rate,bass);
    r.playingDetected=heard.playing && measureSpectrum(segment,rate,true).playing;
    if(!r.playingDetected)return r;
    r.bass=heard.bass;r.compressionTarget=ampCompressionTarget(r.bass,style);
    r.lowExcessBeforeDb=boostLowExcessDb(heard,style);r.lowExcessAfterDb=r.lowExcessBeforeDb;
    r.compressionBefore=ampCompression(segment,dry,rate);r.compressionAfter=r.compressionBefore;
    // A boost is for driven guitar amps whose low end is loose.
    if(r.bass || style==Style::clean || r.lowExcessBeforeDb<=boostLooseLowDb)return r;
    const float driveLimit=std::max(r.compressionBefore,r.compressionTarget)+boostDriveAllowance;
    float best=r.lowExcessBeforeDb;
    for(const float drive:boostDrives)for(const float level:boostLevels) {
        const BoostSetting candidate{drive,.5f,level};
        const auto out=render(candidate);
        const auto spectrum=measureSpectrum(out,rate,false);
        if(!spectrum.playing)continue;
        const float excess=boostLowExcessDb(spectrum,style),compression=ampCompression(segment,out,rate);
        if(compression>driveLimit || excess>=best)continue;
        best=excess;r.setting=candidate;r.lowExcessAfterDb=excess;r.compressionAfter=compression;
    }
    r.engaged=best<r.lowExcessBeforeDb-boostMinTightenDb;
    if(!r.engaged){r.setting={};r.lowExcessAfterDb=r.lowExcessBeforeDb;r.compressionAfter=r.compressionBefore;}
    return r;
}
inline PedalInstanceState boostPedal(const BoostSetting& s) {
    auto pedal=defaultPedalInstance(boostPedalModel);
    pedal.controls[0]=s.drive;pedal.controls[1]=s.tone;pedal.controls[2]=s.level;return pedal;
}
// Renders the input through a fresh Green Drive (when a setting is given) and
// a fresh copy of the native amp as it is set now.
inline BoostRenderer nativeBoostRenderer(const AmpNativeState& state,std::vector<float> input,double rate) {
    return [state,input=std::move(input),rate](const std::optional<BoostSetting>& setting) {
        auto x=input;
        if(setting) {
            constexpr int block=512;
            auto pedal=std::make_unique<PedalEffectInstance>();
            pedal->prepare({rate,juce::uint32(block),1});
            const auto pedalState=boostPedal(*setting);
            juce::AudioBuffer<float> buffer(1,block);
            for(size_t offset=0;offset<x.size();offset+=block) {
                const int count=int(std::min(size_t(block),x.size()-offset));
                buffer.setSize(1,count,false,false,true);
                buffer.copyFrom(0,0,x.data()+offset,count);
                pedal->process(buffer,pedalState);
                std::copy(buffer.getReadPointer(0),buffer.getReadPointer(0)+count,x.begin()+std::ptrdiff_t(offset));
            }
        }
        auto dsp=std::make_unique<AmpNativeDSP>();dsp->prepare(rate);dsp->set(state);dsp->reset();
        std::vector<float> out(x.size());
        for(size_t n=0;n<x.size();++n)out[n]=dsp->tick(x[n],0);
        return out;
    };
}
} // namespace spectralforge::autoset
