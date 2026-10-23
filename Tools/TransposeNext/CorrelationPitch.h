#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <stdexcept>

namespace transposeNext {
// Independent research implementation of variable-rate reading with
// waveform-similarity splice selection. No reference plug-in code or oracle
// frequency is used. This is not a fixed-latency, polyphonic product engine.
// Window/search durations are declared engineering choices, not Neural DSP data.
class CorrelationPitch {
    std::array<std::vector<float>,2> audio, filtered;
    std::array<float,2> low{};
    std::vector<float> fadeWeights;
    std::vector<double> detectorHistory;
    int count{}, capacity{}, write{}, window{}, stride{}, fadeLength{}, fadeIndex{};
    double rate{}, ratio{1}, delta{}, minimum{}, maximum{}, delay{}, nextDelay{};
    float coefficient{};
    bool fading{}, active{}, onsetReset{}, innovationReset{}, pendingOnset{}, externalPending{}, upshiftReset{};
    double upshiftHold{}, onsetAge{1.e9};
    double alignDelay{},alignLead{};
    bool alignCompensated{};
    int fadeStep{1},onsetFadeSamples{};
    double onsetFloor{};
    bool onsetFadeAdaptive{};
    std::array<float,2> noveltyLow1{},noveltyLow2{},noveltyLow3{},noveltyLow4{};
    float noveltyCoefficient{};
    std::array<double,41> peakBuckets{};
    int peakBucketPosition{},peakBucketSamples{},peakBucketLength{};
    double bucketPeak{},bucketMaximum{};
    double fastEnergy{},slowEnergy{},broadEnergy{},fastCoefficient{},broadCoefficient{},peakDecay{};
    int detectorPosition{};
    int cooldown{},cooldownLength{},onsets{};
    int spliceCount{};
    std::array<uint64_t,256> onsetSamples{};
    uint64_t processedSamples{};
    double minRead{}, maxRead{};

    float read(const std::vector<float>& ring, double age) const noexcept {
        double pos = double(write) - age;
        if(pos < 0) pos += capacity;
        const auto i = int(pos);
        const auto f = float(pos-i);
        return ring[size_t(i)] + f*(ring[size_t((i+1)%capacity)]-ring[size_t(i)]);
    }
    // Uncentred normalized squared mismatch includes amplitude differences.
    // Both channels contribute energy independently; antiphase stereo cannot
    // cancel the detector. Causality: only past samples, never future context.
    double cost(double proposed) const noexcept {
        double error=0, energy=1.e-20;
        for(int c=0;c<count;++c) for(int k=0;k<window;k+=stride) {
            const double x=read(filtered[size_t(c)],delay+k);
            const double y=read(filtered[size_t(c)],proposed+k);
            error+=(x-y)*(x-y);energy+=x*x+y*y;
        }
        return error/energy;
    }
    void splice() noexcept {
        // Leave a guard for the whole crossfade. Require meaningful progress
        // toward the opposite boundary, avoiding the trivial zero-lag minimum.
        const double guard=std::abs(delta)*fadeLength+2;
        const double lo=minimum+guard;
        double hi=maximum-guard;
        const double travel=(hi-lo)*.25;
        // After an upshift re-anchor, a backward jump must not reach the
        // attack again (a repeated attack/flam) or the material before it.
        if(upshiftReset && delta<0) {
            const double limit=onsetAge-rate*.008;
            if(limit>=delay+travel)hi=std::min(hi,limit);
        }
        const int first=int(std::ceil(delta>0 ? lo : delay+travel));
        const int last=int(std::floor(delta>0 ? delay-travel : hi));
        double best=delta>0 ? lo : hi, score=1.e100;
        const int step=juce::jmax(1, int(std::lround(rate/12000.)));
        for(int d=first;d<=last;d+=step) {
            const double s=cost(d);
            if(s<score) {score=s;best=d;}
        }
        const int centre=int(best);
        for(int d=juce::jmax(first,centre-step);d<=juce::jmin(last,centre+step);++d) {
            const double s=cost(d);
            if(s<score) {score=s;best=d;}
        }
        if(best>first && best<last) {
            const double left=cost(best-1),right=cost(best+1);
            const double curve=left-2*score+right;
            if(curve>1.e-12) best+=juce::jlimit(-.5,.5,.5*(left-right)/curve);
        }
        nextDelay=juce::jlimit(lo,hi,best);
        fadeIndex=0;fadeStep=1;fading=true;++spliceCount;
    }
public:
    void prepare(const juce::dsp::ProcessSpec& spec, double boundMs, bool resetAtOnset=false, bool detectInnovation=false) {
        if(spec.sampleRate<8000 || spec.sampleRate>192000 || spec.numChannels<1 || spec.numChannels>2
           || boundMs<8 || boundMs>80) throw std::invalid_argument("unsupported correlation configuration");
        rate=spec.sampleRate;count=int(spec.numChannels);
        onsetReset=resetAtOnset;innovationReset=detectInnovation;
        peakBucketLength=std::max(1,int(std::ceil(rate*.001)));
        noveltyCoefficient=float(1-std::exp(-2*juce::MathConstants<double>::pi*2500./rate));
        fastCoefficient=1-std::exp(-1/(rate*.0003));broadCoefficient=fastCoefficient;
        peakDecay=std::exp(-1/(rate*.060));cooldownLength=int(rate*.045);
        detectorHistory.assign(size_t(std::ceil(rate*.001)),0);
        minimum=rate*.002;maximum=rate*boundMs*.001;
        window=int(std::ceil(rate*.012));stride=juce::jmax(1,int(std::lround(rate/6000.)));
        fadeLength=juce::jmax(8,int(std::ceil(rate*.002)));
        capacity=int(std::ceil(maximum))+window+fadeLength+16;
        coefficient=float(1.-std::exp(-2.*juce::MathConstants<double>::pi*800./rate));
        for(auto* bank:{&audio,&filtered}) for(auto& r:*bank) r.assign(size_t(capacity),0);
        onsetFloor=minimum;onsetFadeSamples=fadeLength;
        fadeWeights.resize(size_t(fadeLength));
        for(int i=0;i<fadeLength;++i) fadeWeights[size_t(i)]=float(.5-.5*std::cos(juce::MathConstants<double>::pi*(i+1)/fadeLength));
        reset();
    }
    void reset() noexcept {
        for(auto* bank:{&audio,&filtered})for(auto& r:*bank)std::fill(r.begin(),r.end(),0);
        low.fill(0);noveltyLow1.fill(0);noveltyLow2.fill(0);noveltyLow3.fill(0);noveltyLow4.fill(0);pendingOnset=externalPending=false;write=fadeIndex=spliceCount=0;fading=active=false;
        fastEnergy=slowEnergy=broadEnergy=0;cooldown=onsets=0;processedSamples=0;onsetSamples.fill(0);
        peakBuckets.fill(0);peakBucketPosition=peakBucketSamples=0;bucketPeak=bucketMaximum=0;
        detectorPosition=0;std::fill(detectorHistory.begin(),detectorHistory.end(),0);
        delay=nextDelay=minimum;minRead=maximum;maxRead=minimum;onsetAge=1.e9;
    }
    void setSemitones(int semitones) noexcept {
        const double newRatio=std::pow(2.,juce::jlimit(-12,12,semitones)/12.);
        if(newRatio!=ratio) {
            ratio=newRatio;delta=1-ratio;
            // Preserve the current read position on automation. The sample
            // loop will schedule a bounded crossfade if a boundary is near.
        }
    }
    // Research: also re-anchor on upshift onsets. The read head runs toward
    // the present, so it is placed upshiftHold samples of travel above the
    // lower boundary; the attack plays uninterrupted for that long, then the
    // ordinary splice returns it to the long-delay side.
    void setUpshiftOnsetReset(bool enabled, double holdMs=25) noexcept {upshiftReset=enabled;upshiftHold=rate*holdMs*.001;}
    // Research: smooth the novelty denominator (broadband energy) over a few
    // ms. At 0.3 ms it follows the waveform of a low sustained note, so a
    // weak pick landing near a waveform peak is masked by the 41 ms maximum
    // of spikes near zero crossings. Slow amplitude modulation is still
    // rejected (synthetic sweep: 2-7 ms equivalent; above 10 ms tremolo
    // false-triggers).
    void setBroadSmoothing(double ms) noexcept {broadCoefficient=1-std::exp(-1/(rate*ms*.001));}
    // Research: place re-anchored attacks at a fixed delay (e.g. the 10 ms of
    // the hybrid's STFT band) so both bands start together. Downshift anchors
    // move up to it; upshift anchors (and their hold) are capped at it. 0 = off.
    // Research: a shorter crossfade and a lower read-delay floor for onset
    // re-anchors only (splices keep the 2 ms fade and 2 ms floor). Lowers the
    // downshift attack floor, (floor + guard + lead) / ratio, at -12.
    // adaptive: only when the aligned downshift anchor would sit on the 2 ms
    // floor (about -9 semitones and below); elsewhere the 2 ms fade avoids
    // the +6-8 dB high-frequency burst the short fade adds on repicks.
    void setOnsetFade(double fadeMs, double floorMs, bool adaptive=false) noexcept {
        onsetFadeAdaptive=adaptive;
        onsetFadeSamples=juce::jlimit(1,fadeLength,int(std::ceil(rate*fadeMs*.001)));
        onsetFloor=juce::jlimit(4.,minimum,rate*floorMs*.001);
    }
    void setAnchorAlignment(double ms, bool ratioCompensated=false, double leadMs=2.9) noexcept {
        alignDelay=ms>0 ? juce::jlimit(minimum,maximum,rate*ms*.001) : 0.;alignCompensated=ratioCompensated;alignLead=rate*leadMs*.001;
    }
    int latency() const noexcept {return -1;} // Deliberately NOT advertised as 2/10/40 ms fixed PDC.
    int splices() const noexcept {return spliceCount;}
    int onsetResets() const noexcept {return onsets;}
    const auto& onsetLocations() const noexcept {return onsetSamples;}
    double readMinimum() const noexcept {return minRead;}
    double readMaximum() const noexcept {return maxRead;}
    // externalOnset (optional, one flag per sample): a caller-detected new
    // event re-anchors the read head exactly like the internal onset reset.
    // A request during a crossfade is held until that fade completes.
    // onsetOut (optional, one flag per sample) reports internal onset resets.
    void process(const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, bool render=true, const juce::AudioBuffer<float>* detectorInput=nullptr, const uint8_t* externalOnset=nullptr, uint8_t* onsetOut=nullptr) noexcept {
        for(int n=0;n<input.getNumSamples();++n) {
            if(onsetOut!=nullptr)onsetOut[n]=0;
            double transientEnergy=0, inputEnergy=0;
            for(int c=0;c<count;++c) {
                const auto x=input.getSample(c,n);
                const float detectorX=detectorInput ? detectorInput->getSample(c,n) : x;
                inputEnergy+=double(detectorX)*detectorX;
                audio[size_t(c)][size_t(write)]=x;
                low[size_t(c)]+=coefficient*(x-low[size_t(c)]);
                filtered[size_t(c)][size_t(write)]=low[size_t(c)];
                double high=x-low[size_t(c)];
                if(innovationReset) {
                    noveltyLow1[size_t(c)]+=noveltyCoefficient*(detectorX-noveltyLow1[size_t(c)]);
                    const float h=detectorX-noveltyLow1[size_t(c)];
                    noveltyLow2[size_t(c)]+=noveltyCoefficient*(h-noveltyLow2[size_t(c)]);
                    const float h2=h-noveltyLow2[size_t(c)];
                    noveltyLow3[size_t(c)]+=noveltyCoefficient*(h2-noveltyLow3[size_t(c)]);
                    const float h3=h2-noveltyLow3[size_t(c)];
                    noveltyLow4[size_t(c)]+=noveltyCoefficient*(h3-noveltyLow4[size_t(c)]);
                    high=h3-noveltyLow4[size_t(c)];
                }
                transientEnergy+=high*high;
            }
            transientEnergy/=count;
            // Suppress the filter's release tail when the sidechain is silent.
            // This absolute floor is a research limitation, not gain invariance.
            if(innovationReset && inputEnergy<=1.e-14)transientEnergy=0;
            fastEnergy+=fastCoefficient*(transientEnergy-fastEnergy);
            broadEnergy+=broadCoefficient*(inputEnergy/count-broadEnergy);
            // A high-band fraction rejects smooth global amplitude modulation.
            // Keep the absolute noise floor below as a separate condition.
            const double novelty=innovationReset ? fastEnergy/std::max(broadEnergy,1.e-12) : fastEnergy;
            // Legacy: a 60 ms exponential peak, delayed by 1 ms.
            // Novelty: four 2.5 kHz high-pass sections plus a finite 41-bucket
            // maximum (1 ms buckets, ceil-rounded at each sample rate). Strong
            // events expire instead of suppressing weaker subsequent picks.
            // Both use energy from independent channels, not their sum.
            if(innovationReset) {
                bucketPeak=std::max(bucketPeak,detectorHistory[size_t(detectorPosition)]);
                if(++peakBucketSamples>=peakBucketLength) {
                    peakBuckets[size_t(peakBucketPosition)]=bucketPeak;
                    if(++peakBucketPosition==int(peakBuckets.size()))peakBucketPosition=0;
                    peakBucketSamples=0;bucketPeak=0;
                    bucketMaximum=*std::max_element(peakBuckets.begin(),peakBuckets.end());
                }
                slowEnergy=std::max(bucketMaximum,bucketPeak);
            } else slowEnergy=std::max(detectorHistory[size_t(detectorPosition)],slowEnergy*peakDecay);
            detectorHistory[size_t(detectorPosition)]=novelty;
            if(++detectorPosition==int(detectorHistory.size()))detectorPosition=0;
            if(cooldown>0)--cooldown;
            if(!render || ratio==1.) {
                for(int c=0;c<count;++c)output.setSample(c,n,input.getSample(c,n));
                active=false;fading=false;pendingOnset=false;externalPending=false;
            } else {
                if(!active) {delay=delta>0?minimum:maximum;fading=false;active=true;}
                const double guard=std::abs(delta)*fadeLength+2;
                // Optional downshift experiment: a detected new pick event
                // anchors the read head near the causal boundary. It still
                // resamples the entire signal; no unshifted tonal bypass.
                // Upshifts only with setUpshiftOnsetReset (anchor above the floor).
                if(innovationReset && onsetReset && (delta>0 || (upshiftReset && delta<0)) && cooldown==0 && inputEnergy>1.e-14 && fastEnergy>1.e-10 && novelty>4.*slowEnergy)
                    pendingOnset=true;
                const bool resettable=delta>0 || (upshiftReset && delta<0);
                // Clamped: with a short bound the upshift hold can exceed it
                // (12 ms bound, +12: anchor 27 ms, read past the buffer -> NaN).
                const bool shortOnset=!onsetFadeAdaptive || (delta>0 && alignCompensated && alignDelay*ratio-alignLead<minimum+guard);
                const int onsetFade=shortOnset ? onsetFadeSamples : fadeLength;
                const double floor=shortOnset ? onsetFloor : minimum;
                const double onsetGuard=std::abs(delta)*onsetFade+2;
                double anchor=std::min(delta>0 ? floor+onsetGuard : minimum+guard+upshiftHold*-delta,maximum-guard);
                // A downshift read head moves away from the present at 1-ratio,
                // so an attack reaches the output (anchor+lead)/ratio after it
                // happened (lead: detection plus crossfade, measured ~2.9 ms).
                if(alignDelay>0) anchor=delta>0 ? std::max(anchor,alignCompensated ? alignDelay*ratio-alignLead : alignDelay)
                                                : std::min(anchor,std::max(minimum+guard,alignDelay));
                anchor=std::max(anchor,delta>0 ? floor+onsetGuard : minimum+guard);
                if(externalOnset!=nullptr && externalOnset[n]!=0 && resettable)externalPending=true;
                if(externalPending && resettable && !fading) {
                    externalPending=false;
                    nextDelay=anchor;fadeIndex=0;fadeStep=std::max(1,fadeLength/onsetFade);fading=true;onsetAge=0;
                    if(onsets<int(onsetSamples.size()))onsetSamples[size_t(onsets)]=processedSamples;
                    ++onsets;
                }
                if(onsetReset && resettable && !fading && cooldown==0 &&
                   (innovationReset ? pendingOnset : (fastEnergy>1.e-8 && fastEnergy>2.5*slowEnergy))) {
                    pendingOnset=false;
                    nextDelay=anchor;fadeIndex=0;fadeStep=std::max(1,fadeLength/onsetFade);fading=true;onsetAge=0;
                    cooldown=cooldownLength;
                    if(onsets<int(onsetSamples.size()))onsetSamples[size_t(onsets)]=processedSamples;
                    ++onsets;
                    if(onsetOut!=nullptr)onsetOut[n]=1;
                }
                if(!fading && ((delta>0 && delay>=maximum-guard) || (delta<0 && delay<=minimum+guard))) splice();
                const float w=fading?fadeWeights[size_t(fadeIndex)]:0.f;
                for(int c=0;c<count;++c) {
                    const float old=read(audio[size_t(c)],delay);
                    output.setSample(c,n,fading ? old+w*(read(audio[size_t(c)],nextDelay)-old) : old);
                }
                minRead=std::min(minRead, fading?std::min(delay,nextDelay):delay);
                maxRead=std::max(maxRead, fading?std::max(delay,nextDelay):delay);
                delay+=delta;
                if(fading) {nextDelay+=delta;fadeIndex+=fadeStep;if(fadeIndex>=fadeLength){delay=nextDelay;fading=false;}}
            }
            if(++write==capacity)write=0;
            ++processedSamples;onsetAge+=1;
        }
    }
};
}
