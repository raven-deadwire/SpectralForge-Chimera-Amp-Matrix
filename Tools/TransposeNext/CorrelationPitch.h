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
    std::array<std::vector<double>,2> reference;
    std::array<float,2> low{};
    std::vector<float> fadeWeights, spliceWeights;
    const float* activeWeights{};
    std::vector<double> detectorHistory;
    int count{}, capacity{}, write{}, window{}, stride{}, fadeLength{}, fadeIndex{}, spliceFadeLength{}, activeFade{};
    double rate{}, ratio{1}, delta{}, minimum{}, maximum{}, delay{}, nextDelay{};
    float coefficient{};
    bool fading{}, active{}, onsetReset{}, innovationReset{}, pendingOnset{}, externalPending{}, upshiftReset{};
    double upshiftHold{}, onsetAge{1.e9};
    double retrigger{}, resetPeak{}, reanchorGuard{.008}, relativeFloor{};
    std::array<double,100> hfBuckets{};
    int hfBucketPosition{},hfBucketSamples{};
    double hfBucketPeak{},hfRecentMax{};
    int resetAge{1<<30}, peakWindow{};
    bool holdSplices{}, reportAtDetection{};
    int heldSamples{};
    // Sample-accurate hold (research): when set, a due downshift splice waits
    // while `holdSource` spliced within the previous holdWindow samples. Only
    // splices at or before the current sample count, so the result does not
    // depend on how the host splits blocks.
    const CorrelationPitch* holdSource{};
    uint64_t holdWindow{};
    static constexpr uint64_t noSplice=~uint64_t{};
    std::array<uint64_t,8> spliceLog{};
    int spliceLogPosition{};
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
    // Same value as cost() for an integer candidate, bit for bit: the current
    // segment (fractional delay) is read once per splice into `reference`,
    // and an integer age needs no interpolation (read() adds 0 x difference).
    // Same accumulation order. This is the splice search's hot loop.
    void captureReference() noexcept {
        for(int c=0;c<count;++c) for(int k=0,j=0;k<window;k+=stride,++j)
            reference[size_t(c)][size_t(j)]=read(filtered[size_t(c)],delay+k);
    }
    double integerCost(int proposed) const noexcept {
        double error=0, energy=1.e-20;
        for(int c=0;c<count;++c) {
            const float* ring=filtered[size_t(c)].data();const double* x=reference[size_t(c)].data();
            int i=write-proposed;if(i<0)i+=capacity;
            for(int k=0,j=0;k<window;k+=stride,++j) {
                const double y=ring[i];
                error+=(x[j]-y)*(x[j]-y);energy+=x[j]*x[j]+y*y;
                i-=stride;if(i<0)i+=capacity;
            }
        }
        return error/energy;
    }
    void splice() noexcept {
        // Leave a guard for the whole crossfade. Require meaningful progress
        // toward the opposite boundary, avoiding the trivial zero-lag minimum.
        const double guard=std::abs(delta)*std::max(fadeLength,spliceFadeLength)+2;
        const double lo=minimum+guard;
        double hi=maximum-guard;
        const double travel=(hi-lo)*.25;
        // After an upshift re-anchor, a backward jump must not reach the
        // attack again (a repeated attack/flam) or the material before it.
        if(upshiftReset && delta<0) {
            const double limit=onsetAge-rate*reanchorGuard;
            if(limit>=delay+travel)hi=std::min(hi,limit);
        }
        const int first=int(std::ceil(delta>0 ? lo : delay+travel));
        const int last=int(std::floor(delta>0 ? delay-travel : hi));
        captureReference();
        double best=delta>0 ? lo : hi, score=1.e100;
        const int step=juce::jmax(1, int(std::lround(rate/12000.)));
        for(int d=first;d<=last;d+=step) {
            const double s=integerCost(d);
            if(s<score) {score=s;best=d;}
        }
        const int centre=int(best);
        for(int d=juce::jmax(first,centre-step);d<=juce::jmin(last,centre+step);++d) {
            const double s=integerCost(d);
            if(s<score) {score=s;best=d;}
        }
        if(best>first && best<last) {
            const double left=integerCost(int(best)-1),right=integerCost(int(best)+1);
            const double curve=left-2*score+right;
            if(curve>1.e-12) best+=juce::jlimit(-.5,.5,.5*(left-right)/curve);
        }
        nextDelay=juce::jlimit(lo,hi,best);
        fadeIndex=0;fading=true;++spliceCount;
        spliceLog[size_t(spliceLogPosition)]=processedSamples;spliceLogPosition=(spliceLogPosition+1)%int(spliceLog.size());
        if(spliceFadeLength>0){activeWeights=spliceWeights.data();activeFade=spliceFadeLength;}
        else{activeWeights=fadeWeights.data();activeFade=fadeLength;}
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
        for(auto& r:reference) r.assign(size_t((window+stride-1)/stride),0);
        fadeWeights.resize(size_t(fadeLength));
        for(int i=0;i<fadeLength;++i) fadeWeights[size_t(i)]=float(.5-.5*std::cos(juce::MathConstants<double>::pi*(i+1)/fadeLength));
        spliceFadeLength=0;spliceWeights.clear();activeWeights=fadeWeights.data();activeFade=fadeLength;
        reset();
    }
    void reset() noexcept {
        for(auto* bank:{&audio,&filtered})for(auto& r:*bank)std::fill(r.begin(),r.end(),0);
        low.fill(0);noveltyLow1.fill(0);noveltyLow2.fill(0);noveltyLow3.fill(0);noveltyLow4.fill(0);pendingOnset=externalPending=false;write=fadeIndex=spliceCount=0;spliceLog.fill(noSplice);spliceLogPosition=0;fading=active=false;
        fastEnergy=slowEnergy=broadEnergy=0;cooldown=onsets=0;processedSamples=0;onsetSamples.fill(0);
        peakBuckets.fill(0);peakBucketPosition=peakBucketSamples=0;bucketPeak=bucketMaximum=0;
        hfBuckets.fill(0);hfBucketPosition=hfBucketSamples=0;hfBucketPeak=hfRecentMax=0;
        detectorPosition=0;std::fill(detectorHistory.begin(),detectorHistory.end(),0);
        delay=nextDelay=minimum;minRead=maximum;maxRead=minimum;onsetAge=1.e9;resetAge=1<<30;resetPeak=0;heldSamples=0;
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
    // Research: during the refractory interval, accept a new onset whose
    // novelty exceeds `factor` x the peak novelty of the last reset (tracked
    // over its first 2 ms). A faint false reset (e.g. a note's fade-out
    // corner) then cannot mask a real pick that follows within 45 ms.
    void setRetrigger(double factor) noexcept {retrigger=factor;peakWindow=int(std::ceil(rate*.002));}
    // Research: while held, a downshift splice due at the boundary waits (the
    // read head may overshoot by at most 32 samples of delay). Used so two paths never
    // run their ~0.5 ms correlation search in the same callback.
    void setSplicesHeld(bool held) noexcept {holdSplices=held;}
    // Block-invariant alternative to setSplicesHeld (research). Pass nullptr to
    // disable. Both paths must count the same samples (TransientSplitPitch).
    void setHoldSource(const CorrelationPitch* source,int windowSamples) noexcept {holdSource=source;holdWindow=uint64_t(std::max(0,windowSamples));}
    bool splicedWithin(uint64_t now,uint64_t window) const noexcept {
        for(const auto t:spliceLog)if(t!=noSplice && t<=now && now-t<window)return true;
        return false;
    }
    // Research: after an upshift re-anchor, how far past the onset the next
    // backward jump must land (default 8 ms). Shorter lets a period-aligned
    // jump exist sooner; 0 re-reads the attack (flam).
    void setReanchorGuard(double ms) noexcept {reanchorGuard=ms*.001;}
    // Research: an onset also needs its high-band energy above `ratio` x the
    // largest high-band energy of the previous 100 ms (1 ms buckets). A note's
    // fade-out corner is a faint high-band blip right after a loud attack;
    // a real pick, even a soft one after a loud one, is far stronger.
    void setRelativeFloor(double ratio) noexcept {relativeFloor=ratio;}
    // Research: report an onset (onsetOut) when it is detected rather than
    // when its re-anchor runs. A re-anchor waits for a running splice fade;
    // a caller arming on the report then still sees pre-onset history.
    void setReportAtDetection(bool enabled) noexcept {reportAtDetection=enabled;}
    // Research: cross-fade length for ordinary splices only (onset re-anchors
    // keep the 2 ms fade so attacks stay sharp). Call after prepare(), never
    // from the audio thread (allocates). At most 4x the onset fade.
    void setSpliceFade(double ms) {
        spliceFadeLength=juce::jlimit(fadeLength,4*fadeLength,int(std::ceil(rate*ms*.001)));
        spliceWeights.resize(size_t(spliceFadeLength));
        for(int i=0;i<spliceFadeLength;++i) spliceWeights[size_t(i)]=float(.5-.5*std::cos(juce::MathConstants<double>::pi*(i+1)/spliceFadeLength));
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
            if(relativeFloor>0) {
                hfBucketPeak=std::max(hfBucketPeak,fastEnergy);
                if(++hfBucketSamples>=peakBucketLength) {
                    hfBuckets[size_t(hfBucketPosition)]=hfBucketPeak;
                    if(++hfBucketPosition==int(hfBuckets.size()))hfBucketPosition=0;
                    hfBucketSamples=0;hfBucketPeak=0;
                    hfRecentMax=*std::max_element(hfBuckets.begin(),hfBuckets.end());
                }
            }
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
            if(resetAge<peakWindow)resetPeak=std::max(resetPeak,novelty);
            if(resetAge<(1<<30))++resetAge;
            if(retrigger>0 && cooldown>0 && resetAge>=peakWindow && novelty>retrigger*resetPeak)cooldown=0;
            if(!render || ratio==1.) {
                for(int c=0;c<count;++c)output.setSample(c,n,input.getSample(c,n));
                active=false;fading=false;pendingOnset=false;externalPending=false;
            } else {
                if(!active) {delay=delta>0?minimum:maximum;fading=false;active=true;}
                const double guard=std::abs(delta)*std::max(fadeLength,spliceFadeLength)+2;
                // Optional downshift experiment: a detected new pick event
                // anchors the read head near the causal boundary. It still
                // resamples the entire signal; no unshifted tonal bypass.
                // Upshifts only with setUpshiftOnsetReset (anchor above the floor).
                if(innovationReset && onsetReset && (delta>0 || (upshiftReset && delta<0)) && cooldown==0 && inputEnergy>1.e-14 && fastEnergy>1.e-10 && novelty>4.*slowEnergy
                   && (relativeFloor<=0 || fastEnergy>relativeFloor*hfRecentMax)) {
                    if(reportAtDetection && !pendingOnset && onsetOut!=nullptr)onsetOut[n]=1;
                    pendingOnset=true;
                }
                const bool resettable=delta>0 || (upshiftReset && delta<0);
                const double anchor=minimum+guard+(delta<0 ? upshiftHold*-delta : 0.);
                if(externalOnset!=nullptr && externalOnset[n]!=0 && resettable)externalPending=true;
                if(externalPending && resettable && !fading) {
                    externalPending=false;
                    nextDelay=anchor;fadeIndex=0;fading=true;onsetAge=0;activeWeights=fadeWeights.data();activeFade=fadeLength;
                    if(onsets<int(onsetSamples.size()))onsetSamples[size_t(onsets)]=processedSamples;
                    ++onsets;
                }
                if(onsetReset && resettable && !fading && cooldown==0 &&
                   (innovationReset ? pendingOnset : (fastEnergy>1.e-8 && fastEnergy>2.5*slowEnergy))) {
                    pendingOnset=false;
                    nextDelay=anchor;fadeIndex=0;fading=true;onsetAge=0;activeWeights=fadeWeights.data();activeFade=fadeLength;
                    cooldown=cooldownLength;resetAge=0;resetPeak=novelty;
                    if(onsets<int(onsetSamples.size()))onsetSamples[size_t(onsets)]=processedSamples;
                    ++onsets;
                    if(onsetOut!=nullptr && !reportAtDetection)onsetOut[n]=1;
                }
                const bool due=!fading && ((delta>0 && delay>=maximum-guard) || (delta<0 && delay<=minimum+guard));
                // Downshift only: a late upshift splice measurably lengthens the
                // next backward jump (+0.6 ms low-band centroid, guitar A1 +2).
                const bool held=holdSplices || (holdSource!=nullptr && holdSource->splicedWithin(processedSamples,holdWindow));
                if(due && held && delta>0 && heldSamples<int(32./delta))++heldSamples;
                else if(due){heldSamples=0;splice();}
                const float w=fading?activeWeights[fadeIndex]:0.f;
                for(int c=0;c<count;++c) {
                    const float old=read(audio[size_t(c)],delay);
                    output.setSample(c,n,fading ? old+w*(read(audio[size_t(c)],nextDelay)-old) : old);
                }
                minRead=std::min(minRead, fading?std::min(delay,nextDelay):delay);
                maxRead=std::max(maxRead, fading?std::max(delay,nextDelay):delay);
                delay+=delta;
                if(fading) {nextDelay+=delta;if(++fadeIndex==activeFade){delay=nextDelay;fading=false;}}
            }
            if(++write==capacity)write=0;
            ++processedSamples;onsetAge+=1;
        }
    }
};
}
