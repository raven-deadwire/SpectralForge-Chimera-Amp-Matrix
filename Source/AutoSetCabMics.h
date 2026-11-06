#pragma once
#include "AutoSetDual.h"

namespace spectralforge::autoset {

// Phase 18: the Classic cabinet's two blended mics. With both mic A and mic B
// heard (blend between 0 and 1), the pass hears each mic on its own
// (Tap::micA/micB: after the mic's level, polarity and delay, before the A/B
// blend) and sets how they combine:
// - Alignment: the cross-correlation of the two mics (within +-5 ms) gives
//   the time offset between them and its sign their relative polarity. The
//   mic that arrives first is delayed by the offset (or the later one's delay
//   reduced), and mic B's polarity flips when the mics are opposed. It is
//   applied only when the mics are coherent (|correlation| at least 0.3) and
//   the aligned mix cancels at least 1 dB less, in its worst third-octave band
//   (80 Hz-6.3 kHz), than the mix now.
// - Blend: then the A/B blend, in 5% steps up to 30% either way and kept
//   within 5-95% so both mics stay heard, toward the Tone EQ target; it moves
//   only when the fit improves by more than 0.5 dB.
// Both are computed from the captures, so the prediction is the mix itself:
// a delay change is a fractional shift of the captured mic (linear
// interpolation, as the cabinet's own delay line).
struct MicsResult {
    bool playingDetected{},bass{},aHeard{},bHeard{},coherent{};
    float lagMs{},correlation{};               // mic B after mic A (ms), and the correlation at that lag
    float delayBeforeMs[2]{},delayAfterMs[2]{};// mic A, mic B delays
    bool invertBefore{},invertAfter{};         // mic B polarity
    float blendBefore{},blendAfter{};
    float cancellationBeforeDb{},cancellationAfterDb{};
    float toneErrorBeforeDb{},toneErrorAfterDb{};
    bool aligned() const noexcept {return delayAfterMs[0]!=delayBeforeMs[0] || delayAfterMs[1]!=delayBeforeMs[1] || invertAfter!=invertBefore;}
    bool changed() const noexcept {return aligned() || blendAfter!=blendBefore;}
};
inline constexpr float micsMaxLagMs=5,micsMaxDelayMs=20,micsCoherence=.3f,micsAlignGainDb=1,micsBlendMaxMove=.3f,micsBlendStep=.05f,
                       micsBlendMin=.05f,micsBlendMax=.95f,micsMinGainDb=.5f;
inline constexpr double micsSeconds=4;
// x shifted later by `samples` (negative: earlier), linear interpolation, zero outside.
inline std::vector<float> micsShift(const std::vector<float>& x,double samples) {
    std::vector<float> y(x.size());
    for(size_t n=0;n<y.size();++n) {
        const double t=double(n)-samples;const auto i=std::floor(t);const float f=float(t-i);
        const auto at=[&](double k){return k>=0 && k<double(x.size())?x[size_t(k)]:0.f;};
        y[n]=(1-f)*at(i)+f*at(i+1);
    }
    return y;
}
// Worst third-octave band, 80 Hz-6.3 kHz within 30 dB of the strongest, where
// the mix sits below the weighted mics' power sum (dB).
inline float micsCancellationDb(const Spectrum& mix,const Spectrum& a,const Spectrum& b,float blend) {
    float worst=0;
    for(int band=0;band<toneBandCount;++band) {
        const float fc=toneBandCentres[size_t(band)];
        if(fc<80.f || fc>6300.f)continue;
        const double sum=double(1-blend)*(1-blend)*std::pow(10.,a.bandsDb[size_t(band)]/10.)+double(blend)*blend*std::pow(10.,b.bandsDb[size_t(band)]/10.);
        if(sum<=0 || 10*std::log10(sum)<mix.strongestDb-30.f)continue;
        worst=std::max(worst,float(10*std::log10(sum/std::max(std::pow(10.,mix.bandsDb[size_t(band)]/10.),1e-30))));
    }
    return worst;
}
// The lag (samples, positive: b after a) of the largest |cross-correlation|
// within +-maxLag, refined to a fraction of a sample, and its normalised value.
inline std::pair<double,float> micsCorrelate(const std::vector<float>& a,const std::vector<float>& b,int maxLag) {
    const size_t n=std::min(a.size(),b.size());
    if(n<size_t(4*maxLag+8))return {0.,0.f};
    int order=1;while((size_t(1)<<order)<2*n)++order;
    order=std::min(order,19);
    const size_t size=size_t(1)<<order,used=std::min(n,size/2);
    juce::dsp::FFT fft(order);
    std::vector<float> fa(2*size),fb(2*size);
    std::copy(a.begin(),a.begin()+std::ptrdiff_t(used),fa.begin());std::copy(b.begin(),b.begin()+std::ptrdiff_t(used),fb.begin());
    fft.performRealOnlyForwardTransform(fa.data());fft.performRealOnlyForwardTransform(fb.data());
    // conj(A) * B: its inverse is r(lag) = sum a[i] b[i+lag].
    for(size_t k=0;k<size;++k) {
        const std::complex<float> x(fa[2*k],fa[2*k+1]),y(fb[2*k],fb[2*k+1]);
        const auto p=std::conj(x)*y;fa[2*k]=p.real();fa[2*k+1]=p.imag();
    }
    fft.performRealOnlyInverseTransform(fa.data());
    const auto r=[&](int lag){return fa[size_t(lag>=0?lag:int(size)+lag)];};
    int best=0;
    for(int lag=-maxLag;lag<=maxLag;++lag)if(std::abs(r(lag))>std::abs(r(best)))best=lag;
    double lag=best;
    if(best>-maxLag && best<maxLag) {
        const double y0=std::abs(r(best-1)),y1=std::abs(r(best)),y2=std::abs(r(best+1)),den=y0-2*y1+y2;
        if(den<0)lag+=std::clamp(.5*(y0-y2)/den,-.5,.5);
    }
    // The normalised correlation, measured directly at the integer lag.
    double ab=0,aa=0,bb=0;
    for(size_t i=0;i<used;++i) {
        const auto j=std::ptrdiff_t(i)+best;
        aa+=double(a[i])*a[i];
        if(j>=0 && size_t(j)<used){ab+=double(a[i])*b[size_t(j)];}
    }
    for(size_t i=0;i<used;++i)bb+=double(b[i])*b[i];
    return {lag,aa>0 && bb>0?float(ab/std::sqrt(aa*bb)):0.f};
}
// a/b: mic A and mic B as heard; delays (ms) and mic B's polarity now; blend now.
inline MicsResult analyseMics(const std::vector<float>& micA,const std::vector<float>& micB,double rate,std::array<float,2> delayMs,bool invertB,float blend,
                              std::optional<bool> bass={},Style style=Style::rock) {
    MicsResult r;
    for(int m=0;m<2;++m)r.delayBeforeMs[m]=r.delayAfterMs[m]=delayMs[size_t(m)];
    r.invertBefore=r.invertAfter=invertB;r.blendBefore=r.blendAfter=blend;
    const size_t n=std::min({micA.size(),micB.size(),size_t(rate*micsSeconds)});
    if(n==0)return r;
    const std::vector<float> a(micA.begin(),micA.begin()+std::ptrdiff_t(n)),b(micB.begin(),micB.begin()+std::ptrdiff_t(n));
    std::vector<float> mix(n);
    const auto mixOf=[&](const std::vector<float>& x,const std::vector<float>& y,float w,size_t from,size_t to)->std::vector<float> {
        std::vector<float> m(to-from);for(size_t i=from;i<to;++i)m[i-from]=(1-w)*x[i]+w*y[i];return m;
    };
    const auto now=measureSpectrum(mixOf(a,b,blend,0,n),rate,bass);
    r.playingDetected=now.playing;
    if(!r.playingDetected)return r;
    r.bass=now.bass;
    const auto sa=measureSpectrum(a,rate,r.bass),sb=measureSpectrum(b,rate,r.bass);
    r.aHeard=sa.playing;r.bHeard=sb.playing;
    r.toneErrorBeforeDb=r.toneErrorAfterDb=ampToneErrorDb(now,style);
    r.cancellationBeforeDb=r.cancellationAfterDb=micsCancellationDb(now,sa,sb,blend);
    if(!r.aHeard || !r.bHeard)return r;
    // Alignment.
    const auto [lag,rho]=micsCorrelate(a,b,int(std::round(rate*micsMaxLagMs*.001)));
    r.lagMs=float(lag/rate*1000);r.correlation=rho;r.coherent=std::abs(rho)>=micsCoherence;
    std::vector<float> x=a,y=b;
    if(r.coherent) {
        const double target=double(delayMs[0])-delayMs[1]+r.lagMs; // A's delay minus B's after alignment
        const float newA=float(std::clamp(target,0.,double(micsMaxDelayMs))),newB=float(std::clamp(-target,0.,double(micsMaxDelayMs)));
        const float shiftA=newA-delayMs[0],shiftB=newB-delayMs[1];
        const bool flip=rho<0;
        auto ax=micsShift(a,double(shiftA)*rate*.001),by=micsShift(b,double(shiftB)*rate*.001);
        if(flip)for(auto& v:by)v=-v;
        // Compare on the span both versions cover.
        const auto edge=size_t(std::ceil((std::abs(shiftA)+std::abs(shiftB))*rate*.001))+1;
        if(2*edge<n) {
            const auto trim=[&](const std::vector<float>& v){return std::vector<float>(v.begin()+std::ptrdiff_t(edge),v.end()-std::ptrdiff_t(edge));};
            const auto before=measureSpectrum(mixOf(a,b,blend,edge,n-edge),rate,r.bass),after=measureSpectrum(mixOf(ax,by,blend,edge,n-edge),rate,r.bass);
            const float cancelBefore=micsCancellationDb(before,measureSpectrum(trim(a),rate,r.bass),measureSpectrum(trim(b),rate,r.bass),blend);
            const float cancelAfter=micsCancellationDb(after,measureSpectrum(trim(ax),rate,r.bass),measureSpectrum(trim(by),rate,r.bass),blend);
            if(cancelBefore-cancelAfter>=micsAlignGainDb && (newA!=delayMs[0] || newB!=delayMs[1] || flip)) {
                x=trim(ax);y=trim(by);
                r.delayAfterMs[0]=newA;r.delayAfterMs[1]=newB;r.invertAfter=flip?!invertB:invertB;
                r.cancellationBeforeDb=cancelBefore;r.cancellationAfterDb=cancelAfter;
                r.toneErrorBeforeDb=ampToneErrorDb(before,style);r.toneErrorAfterDb=ampToneErrorDb(after,style);
            }
        }
    }
    // Blend, on the mics as they will be.
    const float errorAligned=r.toneErrorAfterDb;
    float best=errorAligned,chosen=blend;
    const float low=std::max(micsBlendMin,blend-micsBlendMaxMove),high=std::min(micsBlendMax,blend+micsBlendMaxMove);
    const size_t m=x.size();
    for(int i=-6;i<=6;++i) {
        const float w=std::clamp(std::round((blend+float(i)*micsBlendStep)*100.f)/100.f,low,high);
        if(i==0 || w==blend)continue;
        if(const float e=ampToneErrorDb(measureSpectrum(mixOf(x,y,w,0,m),rate,r.bass),style);e<best){best=e;chosen=w;}
    }
    if(best<errorAligned-micsMinGainDb){r.blendAfter=chosen;r.toneErrorAfterDb=best;}
    return r;
}
} // namespace spectralforge::autoset
