#pragma once
#include "AutoSetDual.h"
#include <complex>

namespace spectralforge::autoset {

// Phase 21: the polarity of Dual rig 2, or of the Matrix MID and HIGH lanes.
// Phases 13 and 17 report when the rigs or lanes partly cancel; this step
// flips the lane polarity that removes it. Each lane is heard on its own
// (Tap::lane1-lane3, after the lane's level and polarity), so a flip is the
// capture negated and the prediction is the mix itself:
//   mix = w0 * lane0 + sum over k of s_k * w_k * lane_k,  s_k = +1 or -1
// - Lane 0 (rig 1, or the Matrix LOW lane) is the reference: flipping every
//   lane at once changes nothing heard, so only the others move.
// - Bands: third octaves 20 Hz-6.3 kHz, within 30 dB of the lanes' strongest
//   band. 20 Hz reaches a five-string bass in Drop A (A0, 27.5 Hz). Every
//   lane and the mix are measured in the same frames (chosen by the lanes'
//   level), so a mix that cancels to silence is still measured.
// - Cancellation: per band, the weighted lanes' power sum against the mix;
//   the worst band counts.
// - A flip is made only when the mix now cancels by more than 3 dB and the
//   best combination cancels at least 1 dB less, and only when it makes no
//   band cancel (> 3 dB) by 1 dB more than now: lanes opposed in some bands
//   only (the fundamental in polarity, the harmonics not) are phase, not
//   polarity, and a flip only moves the loss.
// - A time offset is not polarity: when one lane is the others shifted by up
//   to 10 ms (the lag of its strongest correlation with them, coherence at
//   least 0.9), and at that lag the lanes add up (3 dB or less) and cancel
//   at least 1 dB less than any flip, nothing flips and the offset is
//   reported.
// - Ties keep the current polarity.
struct PolarityResult {
    bool playingDetected{},bass{};
    std::vector<bool> heard;                  // per lane
    std::vector<bool> flip;                   // per lane after the reference (lane 1...)
    float cancellationBeforeDb{},cancellationAfterDb{};
    float worstBeforeHz{};                    // the band of cancellationBeforeDb
    // Kept with no flip, when the flips tried make another band cancel: the band of the least-cancelling
    // such flip, and its cancellation after it.
    float harmedHz{},harmedDb{};
    // Kept because a lane is time-shifted: the lane (1...), its offset (> 0: late), whether it is also
    // opposed, and the worst-band cancellation with it aligned.
    int delayLane{-1};float delayMs{},alignedDb{};bool delayOpposed{};
    bool changed() const noexcept {return std::any_of(flip.begin(),flip.end(),[](bool f){return f;});}
    bool harmHeld() const noexcept {return harmedHz>0;}
    bool delayHeld() const noexcept {return delayLane>0;}
};
inline constexpr float polarityMinCancellationDb=3,polarityMinGainDb=1,polarityBandRangeDb=30,polarityMaxDelayMs=10,polarityMinDelayMs=.1f,polarityCapDb=60,polarityMinCoherence=.9f;
inline constexpr double polaritySeconds=4;
inline constexpr std::array<float,26> polarityBandCentres{20,25,31.5f,40,50,63,80,100,125,160,200,250,315,400,500,630,800,1000,1250,1600,2000,2500,3150,4000,5000,6300};
inline constexpr int polarityBandCount=int(polarityBandCentres.size());

// The lanes' spectra in the same frames (Hann, ~1.5 Hz bins, 75% overlap):
// any sign combination, and any lane shifted in time, is the weighted sum of
// the stored bins, so every candidate is measured on the same footing.
class PolaritySpectra {
public:
    using Bands=std::array<float,polarityBandCount>;
    PolaritySpectra(const std::vector<const std::vector<float>*>& lanes,const std::vector<float>& weights,double sampleRate,size_t n)
        : rate(sampleRate),w(weights),count(lanes.size()) {
        order=std::clamp(int(std::round(std::log2(rate/1.5))),13,17);size=1<<order;
        const double edge=std::pow(2.,1./6.);
        for(int b=0;b<polarityBandCount;++b) {
            const double fc=polarityBandCentres[size_t(b)];
            bins[size_t(b)]={std::max(1,int(std::ceil(fc/edge*size/rate))),std::min(size/2,int(std::ceil(fc*edge*size/rate)))};
        }
        firstBin=bins[0].first;binCount=bins[size_t(polarityBandCount-1)].second-firstBin;
        std::vector<float> window(size_t(size),0.f);
        for(int i=0;i<size;++i)window[size_t(i)]=float(.5-.5*std::cos(2*juce::MathConstants<double>::pi*i/size));
        // Frames: every quarter frame; a capture shorter than one frame is one zero-padded frame.
        const size_t hop=size_t(size/4);
        std::vector<size_t> starts;
        if(n<size_t(size))starts.push_back(0);
        else for(size_t s=0;s+size_t(size)<=n;s+=hop)starts.push_back(s);
        // The lanes' level per frame (weighted power sum) chooses the frames, as measureSpectrum does with one signal.
        std::vector<float> levels;
        for(const auto s:starts) {
            double e=0;const size_t end=std::min(n,s+size_t(size));
            for(size_t k=0;k<count;++k){double ek=0;for(size_t i=s;i<end;++i){const double x=(*lanes[k])[i];ek+=x*x;}e+=double(w[k])*w[k]*ek;}
            levels.push_back(toDb(std::sqrt(e/size)));
        }
        const float loud=percentile(levels,.90f);
        juce::dsp::FFT fft(order);std::vector<float> frame(size_t(2*size));
        for(size_t f=0;f<starts.size();++f) {
            if(levels[f]<loud-15.f || levels[f]<=minimumPlayingDb)continue;
            ++frames;
            for(size_t k=0;k<count;++k) {
                std::fill(frame.begin(),frame.end(),0.f);
                const size_t end=std::min(n,starts[f]+size_t(size));
                for(size_t i=starts[f];i<end;++i)frame[i-starts[f]]=(*lanes[k])[i]*window[i-starts[f]];
                fft.performRealOnlyForwardTransform(frame.data(),true);
                for(int j=0;j<binCount;++j)spectra.push_back({frame[size_t(2*(firstBin+j))],frame[size_t(2*(firstBin+j)+1)]});
            }
        }
        // The lanes' weighted power sum per band, and which bands count.
        std::array<double,polarityBandCount> sum{};
        for(size_t k=0;k<count;++k) {
            std::vector<std::complex<float>> only(count);only[k]={w[k],0.f};
            const auto p=power(only);
            for(int b=0;b<polarityBandCount;++b)sum[size_t(b)]+=p[size_t(b)];
        }
        double strongest=0;for(const auto s:sum)strongest=std::max(strongest,s);
        for(int b=0;b<polarityBandCount;++b) {
            sumDb[size_t(b)]=float(10*std::log10(std::max(1e-30,sum[size_t(b)])));
            counted[size_t(b)]=sum[size_t(b)]>0 && strongest>0 && sumDb[size_t(b)]>=float(10*std::log10(strongest))-polarityBandRangeDb;
        }
    }
    bool measured() const noexcept {return frames>0;}
    // Per band: how far the mix (signs: bit k-1 flips lane k; lags: samples
    // each lane is advanced) sits below the lanes' power sum. Bands that do
    // not count are 0.
    Bands cancellation(unsigned signs,const std::vector<double>& lags={}) const {
        std::vector<std::complex<float>> gain(count);
        for(size_t k=0;k<count;++k)gain[k]={(k>0 && ((signs>>(k-1))&1u)?-1.f:1.f)*w[k],0.f};
        const auto p=power(gain,lags);
        Bands c{};
        for(int b=0;b<polarityBandCount;++b)
            c[size_t(b)]=counted[size_t(b)]?std::min(polarityCapDb,sumDb[size_t(b)]-float(10*std::log10(std::max(1e-30,p[size_t(b)])))):0.f;
        return c;
    }
    static float worst(const Bands& c,int* band=nullptr) {
        const auto it=std::max_element(c.begin(),c.end());
        if(band)*band=int(it-c.begin());
        return std::max(0.f,*it);
    }
    // The lag (samples within +-maxLag; > 0: lane k late) at which lane k
    // best matches the other lanes as heard, the sign of that match, and how
    // closely lane k is the others shifted (|correlation| over the geometric
    // mean of their powers: 1 for a scaled, delayed copy).
    struct Lag {int samples{};float sign{1},coherence{};};
    Lag strongestLag(size_t k,int maxLag) const {
        // The cross spectrum (lane k x conj of the others), summed over the
        // frames, in a full-size buffer: its inverse transform is the
        // correlation by lag, Re sum C e^{+jwL} (negative lags wrap).
        std::vector<std::complex<float>> cross(static_cast<size_t>(size)),out(static_cast<size_t>(size));
        double laneEnergy=0,othersEnergy=0;
        for(int f=0;f<frames;++f)for(int j=0;j<binCount;++j) {
            std::complex<float> others{};
            for(size_t i=0;i<count;++i)if(i!=k)others+=w[i]*at(f,i,j);
            cross[size_t(firstBin+j)]+=at(f,k,j)*std::conj(others);
            laneEnergy+=double(std::norm(at(f,k,j)));othersEnergy+=double(std::norm(others));
        }
        juce::dsp::FFT fft(order);fft.perform(cross.data(),out.data(),true);
        Lag best;float bestValue=0;
        for(int lag=-maxLag;lag<=maxLag;++lag) {
            const float v=out[size_t((lag+size)%size)].real();
            if(std::abs(v)>std::abs(bestValue)){bestValue=v;best.samples=lag;}
        }
        best.sign=bestValue<0?-1.f:1.f;
        double c=0;
        for(int j=0;j<binCount;++j)c+=double((cross[size_t(firstBin+j)]*std::polar(1.f,float(2*juce::MathConstants<double>::pi*(firstBin+j)*best.samples/size))).real());
        best.coherence=laneEnergy>0 && othersEnergy>0?float(std::abs(c)/std::sqrt(laneEnergy*othersEnergy)):0.f;
        return best;
    }
private:
    const std::complex<float>& at(int frame,size_t lane,int bin) const {return spectra[(size_t(frame)*count+lane)*size_t(binCount)+size_t(bin)];}
    std::array<double,polarityBandCount> power(const std::vector<std::complex<float>>& gain,const std::vector<double>& lags={}) const {
        std::array<double,polarityBandCount> p{};
        if(frames==0)return p;
        // A lane advanced by L samples: its bins times e^{+jwL}.
        std::vector<std::vector<std::complex<float>>> rotation(count);
        for(size_t k=0;k<count;++k)if(k<lags.size() && std::abs(lags[k])>0) {
            rotation[k].resize(size_t(binCount));
            for(int j=0;j<binCount;++j)rotation[k][size_t(j)]=std::polar(1.f,float(2*juce::MathConstants<double>::pi*(firstBin+j)*lags[k]/size));
        }
        for(int b=0;b<polarityBandCount;++b) {
            double total=0;
            for(int f=0;f<frames;++f)for(int j=bins[size_t(b)].first-firstBin;j<bins[size_t(b)].second-firstBin;++j) {
                std::complex<float> m{};
                for(size_t k=0;k<count;++k){auto x=gain[k]*at(f,k,j);if(!rotation[k].empty())x*=rotation[k][size_t(j)];m+=x;}
                total+=double(std::norm(m));
            }
            p[size_t(b)]=total/frames;
        }
        return p;
    }
    double rate;std::vector<float> w;size_t count;
    int order{},size{},firstBin{},binCount{},frames{};
    std::array<std::pair<int,int>,polarityBandCount> bins{};
    std::vector<std::complex<float>> spectra;   // frame, lane, bin
    Bands sumDb{};std::array<bool,polarityBandCount> counted{};
};
// The worst-band cancellation of the lanes as given (weights as in analysePolarity).
inline float polarityCancellationDb(const std::vector<const std::vector<float>*>& lanes,const std::vector<float>& weights,double rate) {
    size_t n=size_t(rate*polaritySeconds);for(const auto* lane:lanes)n=std::min(n,lane->size());
    return PolaritySpectra::worst(PolaritySpectra(lanes,weights,rate,n).cancellation(0));
}
// lanes: each lane as heard (lane 0 the reference); weights: each lane's
// weight in the mix (Dual blend: 1 - blend and blend; otherwise 1).
inline PolarityResult analysePolarity(const std::vector<const std::vector<float>*>& lanes,const std::vector<float>& weights,double rate,std::optional<bool> bass={}) {
    PolarityResult r;
    const size_t count=lanes.size();
    r.heard.assign(count,false);r.flip.assign(count>0?count-1:0,false);
    if(count<2 || weights.size()!=count)return r;
    size_t n=size_t(rate*polaritySeconds);
    for(const auto* lane:lanes)n=std::min(n,lane->size());
    if(n==0)return r;
    // Each lane on its own decides whether it plays: the mix may cancel to silence.
    for(size_t k=0;k<count;++k) {
        const std::vector<float> head(lanes[k]->begin(),lanes[k]->begin()+std::ptrdiff_t(n));
        const auto s=measureSpectrum(head,rate,bass);r.heard[k]=s.playing;
        if(s.playing && !r.playingDetected){r.playingDetected=true;r.bass=s.bass;}
    }
    if(!r.playingDetected)return r;
    const PolaritySpectra spectra(lanes,weights,rate,n);
    if(!spectra.measured()){r.playingDetected=false;return r;}
    const auto now=spectra.cancellation(0);
    int worstBand=0;
    r.cancellationBeforeDb=r.cancellationAfterDb=PolaritySpectra::worst(now,&worstBand);
    r.worstBeforeHz=polarityBandCentres[size_t(worstBand)];
    if(std::any_of(r.heard.begin(),r.heard.end(),[](bool h){return !h;}))return r;
    if(r.cancellationBeforeDb<=polarityMinCancellationDb)return r;
    // Every sign combination; one that makes another band cancel is not a polarity fix.
    unsigned best=0;float bestDb=r.cancellationBeforeDb;
    float rejectedDb=std::numeric_limits<float>::max(),rejectedHz=0,rejectedBandDb=0;
    for(unsigned signs=1;signs<(1u<<(count-1));++signs) {
        const auto c=spectra.cancellation(signs);
        const float worst=PolaritySpectra::worst(c);
        int harmed=-1;
        for(int b=0;b<polarityBandCount;++b)
            if(c[size_t(b)]>polarityMinCancellationDb && c[size_t(b)]>now[size_t(b)]+polarityMinGainDb && (harmed<0 || c[size_t(b)]>c[size_t(harmed)]))harmed=b;
        if(harmed>=0) {
            if(worst<rejectedDb-1e-4f){rejectedDb=worst;rejectedHz=polarityBandCentres[size_t(harmed)];rejectedBandDb=c[size_t(harmed)];}
            continue;
        }
        if(worst<bestDb-1e-4f){bestDb=worst;best=signs;}
    }
    const bool flips=best!=0 && bestDb<=r.cancellationBeforeDb-polarityMinGainDb;
    // A time offset: a lane shifted to its strongest match with the others, at the sign of that match.
    const int maxLag=int(std::round(rate*polarityMaxDelayMs/1000.)),minLag=std::max(1,int(std::round(rate*polarityMinDelayMs/1000.)));
    const float fixedDb=flips?bestDb:r.cancellationBeforeDb;
    for(size_t k=1;k<count;++k) {
        const auto lag=spectra.strongestLag(k,maxLag);
        if(std::abs(lag.samples)<minLag || lag.coherence<polarityMinCoherence)continue;
        std::vector<double> lags(count,0.);lags[k]=lag.samples;
        const float aligned=PolaritySpectra::worst(spectra.cancellation(lag.sign<0?1u<<(k-1):0u,lags));
        // Aligned, the lanes must add up (and do better than any flip): a partial match of a periodic tone at some lag is not an offset.
        if(aligned<=polarityMinCancellationDb && aligned<=fixedDb-polarityMinGainDb && (!r.delayHeld() || aligned<r.alignedDb)) {
            r.delayLane=int(k);r.delayMs=float(lag.samples*1000./rate);r.delayOpposed=lag.sign<0;r.alignedDb=aligned;
        }
    }
    if(r.delayHeld())return r;
    if(flips) {
        for(size_t k=1;k<count;++k)r.flip[k-1]=((best>>(k-1))&1u)!=0;
        r.cancellationAfterDb=bestDb;
    }
    else if(rejectedHz>0){r.harmedHz=rejectedHz;r.harmedDb=rejectedBandDb;}
    return r;
}
} // namespace spectralforge::autoset
