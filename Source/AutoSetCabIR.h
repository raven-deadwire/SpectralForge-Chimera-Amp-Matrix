#pragma once
#include "AutoSetCab.h"
#include <complex>

namespace spectralforge::autoset {

// Phase 23: the factory cabinet IR of the heard mic (V30 / SM57 or Jensen /
// SM57). Phase 9 places a modeled mic; a mic that plays a factory IR has no
// placement, but the other factory IR may suit the style better. The pass
// hears the rig alone (Tap::cab), and an IR is a fixed filter in series, so
// the other IR predicts what would be heard: the heard bands plus the other
// IR's bands minus the current IR's, each IR level-normalised as the cabinet
// loads it.
// - The other IR is chosen when its prediction sits closer to the Tone EQ
//   target by more than 0.5 dB (as phase 9). Applied whole: an IR is a choice.
// - Bass is kept: both factory IRs are guitar cabinets.
// - The low end is judged on its own: the balance counts from 80 Hz, so a
//   switch that would cut the 50 and 63 Hz bands (44-71 Hz: a seven-string's
//   A1 in Drop A, 55 Hz, and an eight-string's F#1) by more than 3 dB AND
//   leave them more than 3 dB under the style's target there, where the
//   playing has them (within 30 dB of its strongest band), is not made.
//   Cutting low end that sits above the target is the balance's business.
// - The level is not touched; the change the heard playing would get is
//   reported (the Final EQ + output step sets the level).
struct CabIRResult {
    bool playingDetected{},bass{};
    int before{},proposed{};                 // cabinet source: 1 V30 / SM57, 2 Jensen / SM57
    float toneErrorBeforeDb{},toneErrorAfterDb{},levelChangeDb{};
    std::array<float,toneBandCount> moveDb{}; // what the switch adds to each band
    // The other IR's change to the heard 50-63 Hz power (0 when the playing has none there), and
    // whether that kept the IR although its balance was closer.
    // lowEndAfterDb: the 50-63 Hz power after the switch against the target's there.
    float lowEndChangeDb{},lowEndAfterDb{};bool lowEndHeld{};int otherSource{};
    bool changed() const noexcept {return proposed!=before;}
};
inline constexpr float cabIRMinGainDb=.5f,cabIRMaxLowLossDb=3.f,cabIRLowRangeDb=30.f;
inline constexpr bool cabIRLowBand(int band) noexcept {return toneBandCentres[size_t(band)]<70.f;}
inline const char* cabIRName(int source) noexcept {return source==1?"V30 / SM57":source==2?"Jensen / SM57":source==3?"User IR":"Filters only";}
// Third-octave band power of an IR scaled as juce::dsp::Convolution's
// Normalise::yes loads it (0.125 / sqrt of its loudest channel's energy),
// from its first channel at its own rate.
inline std::array<float,toneBandCount> cabIRBandsDb(const juce::AudioBuffer<float>& ir,double irRate) {
    std::array<float,toneBandCount> bands{};bands.fill(-300.f);
    if(ir.getNumChannels()<1 || ir.getNumSamples()<1 || !std::isfinite(irRate) || irRate<8000)return bands;
    double loudest=0;
    for(int c=0;c<ir.getNumChannels();++c){double e=0;for(int n=0;n<ir.getNumSamples();++n)e+=double(ir.getSample(c,n))*ir.getSample(c,n);loudest=std::max(loudest,e);}
    if(loudest<=0)return bands;
    const double scale=.125/std::sqrt(loudest);
    int order=12;while((1<<order)<std::max(ir.getNumSamples(),int(irRate)) && order<18)++order;
    const int size=1<<order;
    juce::dsp::FFT fft(order);
    std::vector<float> bins(size_t(2*size));
    for(int n=0;n<std::min(size,ir.getNumSamples());++n)bins[size_t(n)]=float(scale*ir.getSample(0,n));
    fft.performRealOnlyForwardTransform(bins.data());
    for(int b=0;b<toneBandCount;++b) {
        const double fc=toneBandCentres[size_t(b)],lo=fc*std::pow(2.,-1./6.),hi=std::min(fc*std::pow(2.,1./6.),irRate/2);
        double power=0;int count=0;
        for(int k=std::max(1,int(std::ceil(lo*size/irRate)));k<=size/2 && k*irRate/size<hi;++k){power+=std::norm(std::complex<double>(bins[size_t(2*k)],bins[size_t(2*k+1)]));++count;}
        if(count>0)bands[size_t(b)]=float(10*std::log10(std::max(power/count,1e-30)));
    }
    return bands;
}
// The low end needs the IR's response at the notes themselves: a cabinet
// resonates around 50-100 Hz, so a third-octave average can even get the
// sign of a change at 55 Hz wrong. CabIRLow is an IR's power response (as
// loaded) from 40 to 75 Hz in 0.25 Hz steps, by a direct transform.
using CabIRLow=std::vector<float>;
inline constexpr double cabIRLowFrom=40,cabIRLowTo=75,cabIRLowStep=.25,cabIRLowEdgeLo=44.5,cabIRLowEdgeHi=70.7;
inline CabIRLow cabIRLowResponse(const juce::AudioBuffer<float>& ir,double irRate) {
    CabIRLow out;
    if(ir.getNumChannels()<1 || ir.getNumSamples()<1 || !std::isfinite(irRate) || irRate<8000)return out;
    double loudest=0;
    for(int c=0;c<ir.getNumChannels();++c){double e=0;for(int n=0;n<ir.getNumSamples();++n)e+=double(ir.getSample(c,n))*ir.getSample(c,n);loudest=std::max(loudest,e);}
    if(loudest<=0)return out;
    const double scale=.125/std::sqrt(loudest);
    for(double f=cabIRLowFrom;f<=cabIRLowTo+1e-9;f+=cabIRLowStep) {
        std::complex<double> h{};const double w=2*juce::MathConstants<double>::pi*f/irRate;
        for(int n=0;n<ir.getNumSamples();++n)h+=double(ir.getSample(0,n))*std::polar(1.,-w*n);
        out.push_back(float(std::norm(h*scale)));
    }
    return out;
}
inline double cabIRLowAt(const CabIRLow& r,double f) {
    const double x=std::clamp((f-cabIRLowFrom)/cabIRLowStep,0.,double(r.size()-1));
    const size_t i=std::min(size_t(x),r.size()-2);const double t=x-double(i);
    return (1-t)*r[i]+t*r[i+1];
}
// How a switch from the `now` IR to the `next` one changes the heard notes in
// 44.5-70.7 Hz (dB): the peaks of the heard playing's fine spectrum (~0.7 Hz
// bins, the loud frames as measureSpectrum chooses them), each scaled by
// |H_next|^2 / |H_now|^2 at its frequency.
inline std::optional<float> cabIRLowChangeDb(const std::vector<float>& post,double rate,const CabIRLow& now,const CabIRLow& next) {
    if(now.size()<2 || next.size()!=now.size())return {};
    const int order=std::clamp(int(std::ceil(std::log2(rate/.75))),14,18),size=1<<order;
    if(post.size()<size_t(size))return {};
    juce::dsp::FFT fft(order);std::vector<float> frame(static_cast<size_t>(2*size)),window(static_cast<size_t>(size));
    for(int n=0;n<size;++n)window[size_t(n)]=float(.5-.5*std::cos(2*juce::MathConstants<double>::pi*n/size));
    const int lo=int(std::ceil(cabIRLowEdgeLo*size/rate)),hi=int(std::floor(cabIRLowEdgeHi*size/rate));
    std::vector<float> levels;std::vector<std::vector<double>> powers;
    for(size_t start=0;start+size_t(size)<=post.size();start+=size_t(size/4)) {
        double e=0;std::fill(frame.begin(),frame.end(),0.f);
        for(int n=0;n<size;++n){const float x=post[start+size_t(n)];e+=double(x)*x;frame[size_t(n)]=x*window[size_t(n)];}
        levels.push_back(toDb(std::sqrt(e/size)));
        fft.performRealOnlyForwardTransform(frame.data(),true);
        std::vector<double> p;for(int k=lo;k<=hi;++k)p.push_back(std::norm(std::complex<double>(frame[size_t(2*k)],frame[size_t(2*k+1)])));
        powers.push_back(std::move(p));
    }
    if(levels.empty())return {};
    const float loud=percentile(levels,.90f);
    // The loud frames' mean power per bin, then its peaks within 10 dB of the strongest: the notes'
    // fundamentals. A line at f fills its main lobe (+-2 bins), so each peak's power changes by the
    // IR ratio at the peak's own frequency (parabolic interpolation), not at each bin's.
    std::vector<double> mean(size_t(hi-lo+1),0.);int used=0;
    for(size_t f=0;f<levels.size();++f)if(levels[f]>=loud-15.f && levels[f]>minimumPlayingDb){++used;for(size_t k=0;k<mean.size();++k)mean[k]+=powers[f][k];}
    if(used==0)return {};
    const double top=*std::max_element(mean.begin(),mean.end());
    if(top<=0)return {};
    double before=0,after=0;
    for(size_t k=1;k+1<mean.size();++k) {
        if(mean[k]<mean[k-1] || mean[k]<mean[k+1] || mean[k]<top*.1)continue;
        const double l=std::log(std::max(mean[k-1],1e-300)),c=std::log(mean[k]),r=std::log(std::max(mean[k+1],1e-300));
        const double den=l-2*c+r,offset=std::abs(den)>1e-12?std::clamp(.5*(l-r)/den,-.5,.5):0.;
        const double hz=(double(lo)+double(k)+offset)*rate/size;
        double power=0;for(size_t m=k>=2?k-2:0;m<=std::min(mean.size()-1,k+2);++m)power+=mean[m];
        const double a=cabIRLowAt(now,hz),b=cabIRLowAt(next,hz);
        if(a<=0)continue;
        before+=power;after+=power*b/a;
    }
    if(before<=0 || after<=0)return {};
    return float(10*std::log10(after/before));
}
// post: the cabinet output; source: the heard mic's source (1 or 2);
// factoryBands: cabIRBandsDb of the two factory IRs (V30, Jensen).
// factoryLow: cabIRLowResponse of the two factory IRs; without them the low
// end is judged from the third-octave difference.
inline CabIRResult analyseCabIR(const std::vector<float>& post,double rate,int source,const std::array<std::array<float,toneBandCount>,2>& factoryBands,
                                std::optional<bool> bass={},Style style=Style::rock,const std::array<CabIRLow,2>* factoryLow=nullptr) {
    CabIRResult r;r.before=r.proposed=source;
    if(source!=1 && source!=2)return r;
    const auto heard=measureSpectrum(post,rate,bass);
    r.playingDetected=heard.playing;
    if(!r.playingDetected)return r;
    r.bass=heard.bass;
    r.toneErrorBeforeDb=r.toneErrorAfterDb=ampToneErrorDb(heard,style);
    if(r.bass)return r;
    const int other=3-source;
    const auto& now=factoryBands[size_t(source-1)];const auto& next=factoryBands[size_t(other-1)];
    std::array<float,toneBandCount> move{};
    for(int b=0;b<toneBandCount;++b)move[size_t(b)]=now[size_t(b)]>-299.f && next[size_t(b)]>-299.f?next[size_t(b)]-now[size_t(b)]:0.f;
    std::array<float,toneBandCount> zero{};
    const float e=ampToneErrorDb(cabPredicted(heard,zero,move),style);
    r.otherSource=other;
    {
        const auto predicted=cabPredicted(heard,zero,move);
        const auto target=alignedTarget(predicted,style);
        double lowNow=0,lowNext=0,wanted=0;
        for(int b=0;b<toneBandCount;++b)if(cabIRLowBand(b) && heard.bandsDb[size_t(b)]>=heard.strongestDb-cabIRLowRangeDb) {
            const double p=std::pow(10.,heard.bandsDb[size_t(b)]/10.);lowNow+=p;lowNext+=p*std::pow(10.,move[size_t(b)]/10.);wanted+=std::pow(10.,target[size_t(b)]/10.);
        }
        r.lowEndChangeDb=lowNow>0 && lowNext>0?float(10*std::log10(lowNext/lowNow)):0.f;
        if(lowNow>0 && factoryLow)if(const auto fine=cabIRLowChangeDb(post,rate,(*factoryLow)[size_t(source-1)],(*factoryLow)[size_t(other-1)])){
            r.lowEndChangeDb=*fine;lowNext=lowNow*std::pow(10.,*fine/10.);
        }
        r.lowEndAfterDb=lowNext>0 && wanted>0?float(10*std::log10(lowNext/wanted)):0.f;
    }
    if(e<r.toneErrorBeforeDb-cabIRMinGainDb && r.lowEndChangeDb<-cabIRMaxLowLossDb && r.lowEndAfterDb<-cabIRMaxLowLossDb){r.lowEndHeld=true;r.toneErrorAfterDb=e;return r;}
    if(e<r.toneErrorBeforeDb-cabIRMinGainDb) {
        r.proposed=other;r.toneErrorAfterDb=e;r.moveDb=move;
        double before=0,after=0;
        for(int b=0;b<toneBandCount;++b){const double p=std::pow(10.,heard.bandsDb[size_t(b)]/10.);before+=p;after+=p*std::pow(10.,move[size_t(b)]/10.);}
        r.levelChangeDb=before>0 && after>0?float(10*std::log10(after/before)):0.f;
    }
    return r;
}
} // namespace spectralforge::autoset
