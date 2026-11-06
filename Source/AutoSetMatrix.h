#pragma once
#include "AutoSetDual.h"

namespace spectralforge::autoset {

// Phase 17: the Matrix routing's lane levels. Matrix splits the input into
// LOW, MID and HIGH bands, each through its own amp and cab, and adds the
// three lanes after their level and polarity. The pass hears each lane on its
// own (Tap::lane1-lane3, after the lane's level and polarity), so the mix for
// any level change is computed sample by sample:
//   mix = low + g(mid) * mid + g(high) * high
// The prediction is exact, cancellation between the lanes at the crossovers
// included. The lanes' levels are linear (the legacy lane LEVEL, or a native
// head's OUTPUT LEVEL, scales the lane after the amp's nonlinearity).
// - LOW is the anchor: the Tone EQ target is a shape (aligned to the level
//   heard), so one lane's level is free, and the LOW lane's level would also
//   move its DI/amp mix on a native head. MID and HIGH move relative to it.
// - Each of MID and HIGH moves at most 6 dB, in 0.5 dB steps, within its
//   control's range; coordinate descent over 1 dB candidates, then a 0.5 dB
//   refinement.
// - A change is made only when the Tone EQ fit improves by more than 0.5 dB.
// - Any lane not heard (muted or soloed away) keeps the levels.
// - Cancellation (the mix more than 3 dB below the lanes' power sum in any
//   third-octave band, 80 Hz-6.3 kHz) is reported, not fixed: polarity is
//   left to the player.
// The Final EQ and output pass that follows restores the loudness.
struct MatrixResult {
    bool playingDetected{},bass{};
    std::array<bool,3> heard{};               // LOW, MID, HIGH lanes playing
    std::array<float,2> changeDb{};           // MID, HIGH level change
    float toneErrorBeforeDb{},toneErrorAfterDb{};
    float levelChangeDb{};                    // the mix's 80 Hz-6.3 kHz level change
    float cancellationDb{};                   // the mix now, below the lanes' power sum, in the worst band
    int evaluations{};                        // mixes measured
    bool changed() const noexcept {return changeDb[0]!=0.f || changeDb[1]!=0.f;}
};
inline constexpr float matrixMaxMoveDb=6,matrixStepDb=1,matrixFineStepDb=.5f,matrixMinGainDb=.5f,matrixCancellationDb=3;
inline constexpr double matrixSeconds=4;     // the first 4 s heard are mixed
// low/mid/high: each lane as heard; room: how far each of MID and HIGH may
// move (dB, {down, up}), from its control's range.
inline MatrixResult analyseMatrix(const std::vector<float>& low,const std::vector<float>& mid,const std::vector<float>& high,double rate,
                                  std::optional<bool> bass={},Style style=Style::rock,
                                  std::array<std::pair<float,float>,2> room={std::pair{-matrixMaxMoveDb,matrixMaxMoveDb},std::pair{-matrixMaxMoveDb,matrixMaxMoveDb}}) {
    MatrixResult r;
    const size_t n=std::min({low.size(),mid.size(),high.size(),size_t(rate*matrixSeconds)});
    if(n==0)return r;
    const auto head=[n](const std::vector<float>& x){return std::vector<float>(x.begin(),x.begin()+std::ptrdiff_t(n));};
    const auto a=head(low),b=head(mid),c=head(high);
    // One buffer for every mix the analysis hears, rewritten in place.
    std::vector<float> mix(n);
    const auto gain=[](float db){return float(std::pow(10.,db/20.));};
    const auto mixAt=[&](float midDb,float highDb)->const std::vector<float>& {
        const float gm=gain(midDb),gh=gain(highDb);
        for(size_t i=0;i<n;++i)mix[i]=a[i]+gm*b[i]+gh*c[i];
        return mix;
    };
    const auto now=measureSpectrum(mixAt(0,0),rate,bass);
    r.playingDetected=now.playing;
    if(!r.playingDetected)return r;
    r.bass=now.bass;
    const auto s1=measureSpectrum(a,rate,r.bass),s2=measureSpectrum(b,rate,r.bass),s3=measureSpectrum(c,rate,r.bass);
    r.heard={s1.playing,s2.playing,s3.playing};
    // Cancellation: the mix against the lanes' power sum, in the worst
    // third-octave band (crossover notches are narrow), 80 Hz-6.3 kHz, within
    // 30 dB of the strongest band.
    const double heardPower=dualFitPower(now.bandsDb);
    r.cancellationDb=0;
    for(int band=0;band<toneBandCount;++band) {
        const float fc=toneBandCentres[size_t(band)];
        if(fc<80.f || fc>6300.f)continue;
        double sum=0;
        for(const auto* s:{&s1,&s2,&s3})if(s->playing)sum+=std::pow(10.,s->bandsDb[size_t(band)]/10.);
        const double heardBand=std::pow(10.,now.bandsDb[size_t(band)]/10.);
        if(sum<=0 || 10*std::log10(sum)<now.strongestDb-30.f)continue;
        r.cancellationDb=std::max(r.cancellationDb,float(10*std::log10(sum/std::max(heardBand,1e-30))));
    }
    r.toneErrorBeforeDb=r.toneErrorAfterDb=ampToneErrorDb(now,style);
    // LOW is the reference: without it the balance would be set for a sound the player does not normally hear.
    if(!r.heard[0] || !r.heard[1] || !r.heard[2])return r;
    std::array<float,2> best{},lo{},hi{};
    for(size_t k=0;k<2;++k){lo[k]=std::max(-matrixMaxMoveDb,std::min(0.f,room[k].first));hi[k]=std::min(matrixMaxMoveDb,std::max(0.f,room[k].second));}
    float bestError=r.toneErrorBeforeDb;Spectrum bestSpectrum=now;
    // Ties keep the smaller move.
    const auto tryAt=[&](std::array<float,2> at) {
        const auto s=measureSpectrum(mixAt(at[0],at[1]),rate,r.bass);++r.evaluations;
        const float e=ampToneErrorDb(s,style);
        if(e<bestError-1e-4f || (e<=bestError+1e-4f && std::abs(at[0])+std::abs(at[1])<std::abs(best[0])+std::abs(best[1]))){bestError=e;best=at;bestSpectrum=s;}
    };
    for(const float step:{matrixStepDb,matrixFineStepDb}) {
        for(int round=0;round<3;++round) {
            const auto start=best;
            for(size_t k=0;k<2;++k) {
                const float centre=best[k];
                const float from=step==matrixStepDb?lo[k]:centre-step,to=step==matrixStepDb?hi[k]:centre+step;
                for(float v=std::ceil(from/step)*step;v<=to+1e-4f;v+=step) {
                    if(v<lo[k]-1e-4f || v>hi[k]+1e-4f || std::abs(v-centre)<1e-4f)continue;
                    auto at=best;at[k]=v;tryAt(at);
                }
            }
            if(best==start)break;
        }
    }
    if(bestError<r.toneErrorBeforeDb-matrixMinGainDb) {
        r.changeDb=best;r.toneErrorAfterDb=bestError;
        r.levelChangeDb=float(10*std::log10(std::max(dualFitPower(bestSpectrum.bandsDb),1e-30)/std::max(heardPower,1e-30)));
    }
    return r;
}
} // namespace spectralforge::autoset
