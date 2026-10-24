#pragma once
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>
#include "PolyPitch.h"

// HYBRID transpose mode (Transposer mode 3). Design, measurements and limits:
// docs/TRANSPOSE_HYBRID_LATENCY_CONTRACT.md and
// docs/TRANSPOSE_SYNTHETIC_EVIDENCE_20261023.md (synthetic material only; no
// DI recordings or blind listening yet).
//
// CorrelationPitch and LegatoDetector below are copies of the research
// sources in Tools/TransposeNext (ebd3761, plus the per-update pick ordering
// and the steady guard), only moved into this namespace.
// Tests/PitchRegressionTests.cpp checks that HybridTransposer is
// bit-identical to the research render probe's mode 45 (ChimeraTransposeRender),
// so a change to either copy must be made to both.

namespace spectralforge::hybrid {
// Independent research implementation of variable-rate reading with
// waveform-similarity splice selection. No reference plug-in code or oracle
// frequency is used. This is not a fixed-latency, polyphonic product engine.
// Window/search durations are declared engineering choices, not Neural DSP data.
class CorrelationPitch {
    std::array<std::vector<float>,2> audio, filtered;
    std::array<std::vector<double>,2> reference;
    std::array<float,2> low{};
    std::vector<float> fadeWeights;
    std::vector<double> detectorHistory;
    int count{}, capacity{}, write{}, window{}, stride{}, fadeLength{}, fadeIndex{};
    double rate{}, ratio{1}, delta{}, minimum{}, maximum{}, delay{}, nextDelay{};
    float coefficient{};
    bool fading{}, active{}, onsetReset{}, innovationReset{}, pendingOnset{}, externalPending{}, upshiftReset{};
    double upshiftHold{}, onsetAge{1.e9};
    double retrigger{}, resetPeak{};
    int resetAge{1<<30}, peakWindow{};
    bool holdSplices{};
    int heldSamples{};
    double alignDelay{},alignLead{},alignInput{};
    bool alignExact{},phasePicks{};
    double phaseSearch{};
    double externalAgeSamples{};
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
    // Research: move a re-anchor target by up to +-phaseSearch samples to the
    // position whose recent waveform best matches what the read head has just
    // played (the splice cost), so the jump does not cut mid-cycle. Arrival
    // then moves by up to phaseSearch / ratio.
    double phaseAligned(double target) noexcept {
        if(phaseSearch<=0) return target;
        captureReference();
        const int guardSamples=int(std::ceil(std::abs(delta)*fadeLength+2));
        const int first=juce::jmax(int(std::ceil(minimum))+guardSamples,int(std::floor(target-phaseSearch)));
        const int last=juce::jmin(int(std::floor(maximum))-guardSamples,int(std::ceil(target+phaseSearch)));
        double best=target,score=1.e100;
        for(int d=first;d<=last;++d){const double s=integerCost(d);if(s<score){score=s;best=d;}}
        return best;
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
        for(auto& r:reference) r.assign(size_t((window+stride-1)/stride),0);
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
    // Research: place re-anchored attacks at a fixed delay (e.g. the 10 ms of
    // the hybrid's STFT band) so both bands start together. Downshift anchors
    // move up to it; upshift anchors (and their hold) are capped at it. 0 = off.
    void setAnchorAlignment(double ms, bool ratioCompensated=false, double leadMs=2.9) noexcept {
        alignDelay=ms>0 ? juce::jlimit(minimum,maximum,rate*ms*.001) : 0.;alignCompensated=ratioCompensated;alignLead=rate*leadMs*.001;
    }
    // Research: delay the caller adds before this engine (e.g. a crossover's
    // group delay). With it the aligned attack time is inputMs + (anchor +
    // lead) / ratio, so anchors aim at (target - inputMs). 0 = the 2.9 ms lead
    // calibration above, which was measured with a 150 Hz LR4 in front.
    // Setting it also switches upshifts to the same exact target (latency
    // contract): both directions anchor at (target - inputMs) * ratio - lead.
    void setAnchorInputDelay(double ms) noexcept {alignInput=rate*ms*.001;alignExact=true;}
    // Research: phase-aligned re-anchors (see phaseAligned); picks optionally.
    void setReanchorPhaseSearch(double ms, bool picks) noexcept {phaseSearch=rate*ms*.001;phasePicks=picks;}
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
    // externalAge (optional, with externalOnset): samples since the flagged
    // event began. With the exact alignment (setAnchorInputDelay) the anchor
    // grows by (1 - ratio) x age, so a late-detected change still lands on the
    // target: D0 = r L - r tau + (1 - r) age (read-head geometry).
    void process(const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, bool render=true, const juce::AudioBuffer<float>* detectorInput=nullptr, const uint8_t* externalOnset=nullptr, uint8_t* onsetOut=nullptr, double* delayOut=nullptr, const int* externalAge=nullptr) noexcept {
        for(int n=0;n<input.getNumSamples();++n) {
            if(onsetOut!=nullptr)onsetOut[n]=0;
            if(delayOut!=nullptr)delayOut[n]=0;
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
            if(resetAge<peakWindow)resetPeak=std::max(resetPeak,novelty);
            if(resetAge<(1<<30))++resetAge;
            if(retrigger>0 && cooldown>0 && resetAge>=peakWindow && novelty>retrigger*resetPeak)cooldown=0;
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
                const bool shortOnset=!onsetFadeAdaptive || (delta>0 && alignCompensated && (alignDelay-alignInput)*ratio-alignLead<minimum+guard);
                const int onsetFade=shortOnset ? onsetFadeSamples : fadeLength;
                const double floor=shortOnset ? onsetFloor : minimum;
                const double onsetGuard=std::abs(delta)*onsetFade+2;
                double anchor=std::min(delta>0 ? floor+onsetGuard : minimum+guard+upshiftHold*-delta,maximum-guard);
                // A downshift read head moves away from the present at 1-ratio,
                // so an attack reaches the output (anchor+lead)/ratio after it
                // happened (lead: detection plus crossfade, measured ~2.9 ms).
                if(alignDelay>0 && alignExact) anchor=(alignDelay-alignInput)*ratio-alignLead;
                else if(alignDelay>0) anchor=delta>0 ? std::max(anchor,alignCompensated ? (alignDelay-alignInput)*ratio-alignLead : alignDelay-alignInput)
                                                : std::min(anchor,std::max(minimum+guard,alignDelay-alignInput));
                anchor=std::max(anchor,delta>0 ? floor+onsetGuard : minimum+guard);
                anchor=std::min(anchor,maximum-guard);
                if(externalOnset!=nullptr && externalOnset[n]!=0 && resettable){externalPending=true;externalAgeSamples=externalAge!=nullptr ? std::max(0,externalAge[n]) : 0.;}
                else if(externalPending)externalAgeSamples+=1;
                if(externalPending && resettable && !fading) {
                    externalPending=false;
                    nextDelay=phaseAligned(alignExact ? juce::jlimit(minimum+guard,maximum-guard,anchor+delta*std::min(externalAgeSamples,alignDelay)) : anchor);fadeIndex=0;fadeStep=std::max(1,fadeLength/onsetFade);fading=true;onsetAge=0;
                    if(onsets<int(onsetSamples.size()))onsetSamples[size_t(onsets)]=processedSamples;
                    ++onsets;
                }
                if(onsetReset && resettable && !fading && cooldown==0 &&
                   (innovationReset ? pendingOnset : (fastEnergy>1.e-8 && fastEnergy>2.5*slowEnergy))) {
                    pendingOnset=false;
                    nextDelay=phasePicks ? phaseAligned(anchor) : anchor;fadeIndex=0;fadeStep=std::max(1,fadeLength/onsetFade);fading=true;onsetAge=0;
                    cooldown=cooldownLength;resetAge=0;resetPeak=novelty;
                    if(onsets<int(onsetSamples.size()))onsetSamples[size_t(onsets)]=processedSamples;
                    ++onsets;
                    if(onsetOut!=nullptr)onsetOut[n]=1;
                }
                const bool due=!fading && ((delta>0 && delay>=maximum-guard) || (delta<0 && delay<=minimum+guard));
                // Downshift only: a late upshift splice measurably lengthens the
                // next backward jump (+0.6 ms low-band centroid, guitar A1 +2).
                if(due && holdSplices && delta>0 && heldSamples<int(32./delta))++heldSamples;
                else if(due){heldSamples=0;splice();}
                const float w=fading?fadeWeights[size_t(fadeIndex)]:0.f;
                for(int c=0;c<count;++c) {
                    const float old=read(audio[size_t(c)],delay);
                    output.setSample(c,n,fading ? old+w*(read(audio[size_t(c)],nextDelay)-old) : old);
                }
                // delayOut (optional): the read delay this sample used (the dominant
                // side of a crossfade), for phase-locking another band to this one.
                if(delayOut!=nullptr)delayOut[n]=(fading && w>.5f) ? nextDelay : delay;
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

namespace spectralforge::hybrid {
// Research only (2026-10-23): flags a pitch change without a new pick
// (legato, hammer-on, pull-off) so a read-head engine can re-anchor there,
// and estimates how long ago the change began.
//
// A sustained note repeats with its period P, so the period-prediction error
//   e = sum (x[t] - x[t-P])^2 / sum (x[t]^2 + x[t-P]^2)
// over the last 4 ms stays low. Only history is used, so a change shows up
// within a few ms. The input is differentiated first: a pitch change of df
// moves harmonic k by k df, so weighting high harmonics detects it sooner.
// P is re-fitted every 1 ms within +-4 %, which follows vibrato and bends
// (about 0.8 % per 20 ms at +-20 cents / 5.5 Hz or 200 cents / 300 ms).
//
// Tuned on synthetic material (docs/TRANSPOSE_HYBRID_LATENCY_CONTRACT.md):
// - lock only when e < 0.02 (non-periodic chords never lock) and only for
//   periods <= 20 ms (fundamental >= 50 Hz; lower notes were unreliable),
// - an event needs 10 ms of lock, e > 6 x its running baseline, e > 0.06 and
//   a rise > 0.06 within 5 ms (slow drift and beating do not qualify),
// - a candidate is confirmed only if, 2 ms later, the error at the old period
//   is still > 0.15: an equal-tempered triad (C4 major, ~10 Hz beating of
//   the major third) briefly breaks the lock and then fits again (0.002-0.079
//   after 2 ms over 25 false events), a real change keeps failing it
//   (0.198-1.018 over 15 legato / hammer-on events); the 2 ms are part of the age,
// - a second route catches small steps the +-4 % re-fit follows like a bend:
//   a one-semitone legato moves the fitted period 2.9 % in 5 ms (518 -> 503
//   samples, E2 +1) while its error stays < 0.06; vibrato or a 200-cent bend
//   move it about 0.2 % in 5 ms and C4-major beating 0.14 %. A drift > 1.5 %
//   within 5 ms is a candidate, confirmed against the period before the drift
//   and dated from the last steady fit (drift < 0.3 % in 5 ms),
// - the change is dated at the last 1 ms update that still fitted (age),
// - no lock while more than 20 % of the energy passes a two-pole 45 Hz
//   low-pass (one pole at 55 Hz also blocked a 62 Hz D#2): notes under
//   the 50 Hz floor (B0, E1) otherwise lock onto a sub-period and false-trigger
//   (B0 power chord: 9 events in 3 s without this guard).
class LegatoDetector {
    std::vector<float> ring;
    std::array<double,6> recent{}, periods{};
    int size{}, write{}, hop{}, clock{}, window{}, minLag{}, maxLag{};
    int stable{}, cooldown{}, searchHold{}, events{}, recentCount{}, sinceFit{}, sinceSteady{}, confirm{}, rejected{}, confirmAgeFrom{};
    int steadyGuard{}, guarded{}, sampleRateHz{};
    double period{}, confirmPeriod{}, baseline{}, previous{}, deep{}, deeper{}, deepCoefficient{}, deepEnergy{}, allEnergy{}, energyCoefficient{};
    static constexpr int stableNeeded=10, cooldownLength=30, rise=5, confirmUpdates=2;
    static constexpr double searchThreshold=.15, lockThreshold=.02, eventRatio=6, eventFloor=.06, energyFloor=1.e-7, confirmFloor=.15,
                            driftEvent=.015, driftSteady=.003;
    float at(int age) const noexcept {int i=write-1-age;if(i<0)i+=size;return ring[size_t(i)];}
    double error(int lag) const noexcept {
        double e=0,s=1.e-20;
        for(int k=0;k<window;++k){const double a=at(k),b=at(k+lag);e+=(a-b)*(a-b);s+=a*a+b*b;}
        return e/s;
    }
public:
    void prepare(double rate) {
        sampleRateHz=int(rate);hop=std::max(1,int(rate*.001));window=int(rate*.004);
        minLag=int(rate*.0025);maxLag=int(rate*.020);
        size=maxLag+window+hop+8;ring.assign(size_t(size),0.f);
        deepCoefficient=1-std::exp(-2*juce::MathConstants<double>::pi*45/rate);energyCoefficient=1-std::exp(-1/(rate*.02));reset();
    }
    void reset() noexcept {
        std::fill(ring.begin(),ring.end(),0.f);write=clock=stable=cooldown=searchHold=events=recentCount=sinceFit=sinceSteady=confirm=rejected=confirmAgeFrom=guarded=0;
        period=baseline=previous=deep=deeper=deepEnergy=allEnergy=0;
    }
    // A pick elsewhere re-anchors anyway: drop the lock and wait.
    void notePick() noexcept {period=0;stable=0;recentCount=0;confirm=0;cooldown=cooldownLength;}
    int legatoEvents() const noexcept {return events;}
    int rejectedCandidates() const noexcept {return rejected;}
    // Research option (off = 0): a candidate needs a steady fit (drift < 0.3 %
    // in 5 ms) within `ms` before the change. A legato step leaves a steady
    // note just before it; wide vibrato (+-100 cents, 7 Hz) or a fast bend
    // has been moving for tens of ms (re-anchoring there measured worse, see
    // Tools/TransposeEvidence/legato_modulation_probe.py).
    void setSteadyGuard(double ms) noexcept {steadyGuard=ms>0 ? int(sampleRateHz*ms/1000) : 0;}
    int guardedCandidates() const noexcept {return guarded;}
    // True when the next push() runs the 1 ms update. notePick() only touches
    // update state, so a pick reported any time before that push is applied
    // exactly as if it were reported on its own sample.
    bool nextPushUpdates() const noexcept {return clock+1>=hop;}
    // One mono sample. Returns the estimated age of the change in samples on
    // the sample where a legato change is flagged, otherwise -1.
    int push(float x) noexcept {
        const float emphasised=x-float(previous);previous=x;
        deep+=deepCoefficient*(x-deep);deeper+=deepCoefficient*(deep-deeper);deepEnergy+=energyCoefficient*(deeper*deeper-deepEnergy);allEnergy+=energyCoefficient*(double(x)*x-allEnergy);
        ring[size_t(write)]=emphasised;if(++write==size)write=0;
        ++sinceFit;++sinceSteady;
        if(++clock<hop) return -1;
        clock=0;
        if(cooldown>0)--cooldown;
        double energy=0;for(int k=0;k<window;++k)energy+=double(at(k))*at(k);
        if(energy<energyFloor){period=0;stable=0;recentCount=0;confirm=0;return -1;}
        if(deepEnergy>.2*allEnergy){period=0;stable=0;recentCount=0;confirm=0;return -1;}
        if(confirm>0) {
            // Still at the old period? Then it was beating, not a new note.
            if(error(int(confirmPeriod))<=confirmFloor){confirm=0;++rejected;return -1;}
            if(--confirm>0) return -1;
            ++events;period=0;stable=0;recentCount=0;cooldown=cooldownLength;
            return confirmAgeFrom==1 ? sinceSteady : sinceFit;
        }
        if(period<=0) {
            if(searchHold>0){--searchHold;return -1;}
            // First dip below the search threshold, walked to its local
            // minimum (YIN-like: a multiple of the period is not preferred).
            int lag=minLag;double e=error(lag);
            while(lag+2<=maxLag && e>=searchThreshold){lag+=2;e=error(lag);}
            if(e<searchThreshold) {
                for(double next=error(lag+1);lag+1<=maxLag && next<e;next=error(lag+1)){++lag;e=next;}
                if(e<lockThreshold){period=lag;stable=0;recentCount=0;baseline=std::max(e,.005);sinceFit=sinceSteady=0;return -1;}
            }
            searchHold=9;   // full search at most every 10 ms
            return -1;
        }
        const int lo=std::max(minLag,int(period*.96)),hi=std::min(maxLag,int(period*1.04)+1);
        double best=1.e9;int found=lo;
        for(int lag=lo;lag<=hi;++lag){const double e=error(lag);if(e<best){best=e;found=lag;}}
        const double old=recentCount>=rise ? recent[size_t((recentCount-rise)%int(recent.size()))] : best;   // 5 updates ago
        const double before=recentCount>=rise ? periods[size_t((recentCount-rise)%int(periods.size()))] : period;   // fitted 5 updates ago
        const double drift=std::abs(found-before)/before;
        recent[size_t(recentCount%recent.size())]=best;periods[size_t(recentCount%periods.size())]=found;++recentCount;
        if(stable>=stableNeeded && cooldown==0 && best>eventRatio*baseline && best>eventFloor && recentCount>rise && best-old>eventFloor) {
            if(steadyGuard>0 && sinceSteady-sinceFit>steadyGuard){++guarded;return -1;}
            confirm=confirmUpdates;confirmPeriod=period;confirmAgeFrom=0;   // the age keeps counting from the last fit
            return -1;
        }
        if(stable>=stableNeeded && cooldown==0 && recentCount>rise && drift>driftEvent && best<lockThreshold) {
            if(steadyGuard>0 && sinceSteady>steadyGuard){++guarded;return -1;}
            confirm=confirmUpdates;confirmPeriod=before;confirmAgeFrom=1;   // age from the last steady fit
            return -1;
        }
        if(best<searchThreshold && best<3*baseline+.02 && best<lockThreshold) {
            period=found;if(stable<stableNeeded)++stable;baseline=.9*baseline+.1*std::max(best,.002);sinceFit=0;
            if(drift<driftSteady)sinceSteady=0;
        } else if(best>.6){period=0;stable=0;recentCount=0;}
        return -1;
    }
};
}

namespace spectralforge::hybrid {
// 800 Hz LR4 split: below, the correlation read head with onset re-anchoring
// (picks, and legato changes from LegatoDetector, phase-aligned within
// +-2.5 ms); above, the 10 ms STFT of the TIGHT mode. Reports the STFT window
// as its latency and anchors picked attacks there (attack-latency contract).
// Sustained low-band delay still varies (2-40 ms) by design.
class HybridTransposer {
    struct Biquad {
        double b0{},b1{},b2{},a1{},a2{};
        std::array<double,2> z1{},z2{};
        float tick(float x,int c) noexcept {
            const double y=b0*x+z1[size_t(c)];
            z1[size_t(c)]=b1*x-a1*y+z2[size_t(c)];
            z2[size_t(c)]=b2*x-a2*y;return float(y);
        }
        void clear() noexcept {z1.fill(0);z2.fill(0);}
        void prepare(double rate,double hz,bool high) noexcept {
            const double w=2*juce::MathConstants<double>::pi*hz/rate;
            // Written as in the research source (q = 1/sqrt 2) so the coefficients
            // round identically: the equivalence test compares bit for bit.
            const double q=1/std::sqrt(2.);
            const double c=std::cos(w),alpha=std::sin(w)/(2*q);
            b0=(high?1+c:1-c)*.5/(1+alpha);b1=(high?-2:2)*b0;b2=b0;
            a1=-2*c/(1+alpha);a2=(1-alpha)/(1+alpha);clear();
        }
    };
    static constexpr double crossover=800.,boundMs=40.,leadMs=1.4,phaseSearchMs=2.5,steadyGuardMs=10.;
    CorrelationPitch body;
    LegatoDetector legato;
    spectralforge::PolyPitch upper;
    std::array<Biquad,2> lowpass,highpass;
    juce::AudioBuffer<float> lowIn,highIn,lowOut,highOut,detector;
    std::vector<double> lowDelay;
    std::vector<uint8_t> legatoFlags,pickFlags;
    std::vector<int> legatoAges;
    int channels{},capacity{},window{},shift{};
    bool available{},wasActive{};
public:
    // False outside 8-192 kHz or 1-2 channels (the read head's range); the
    // caller then keeps another mode.
    static bool supports(const juce::dsp::ProcessSpec& spec) noexcept {
        return spec.sampleRate>=8000 && spec.sampleRate<=192000 && spec.numChannels>=1 && spec.numChannels<=2;
    }
    bool isAvailable() const noexcept {return available;}
    void prepare(const juce::dsp::ProcessSpec& spec) {
        available=supports(spec);
        if(!available) return;
        channels=int(spec.numChannels);capacity=std::max(1,int(spec.maximumBlockSize));
        window=int(spec.sampleRate*.010)/8*8;
        body.prepare(spec,boundMs,true,true);
        body.setUpshiftOnsetReset(true);body.setBroadSmoothing(4);
        body.setOnsetFade(.5,.5,true);
        // Attack-latency contract: anchors at (L + lead - tau) x ratio - lead,
        // tau the LR4's DC group delay 2 sqrt(2) / w_c.
        body.setAnchorAlignment(1000.*window/spec.sampleRate+leadMs,true,leadMs);
        body.setAnchorInputDelay(1000.*2*std::sqrt(2.)/(2*juce::MathConstants<double>::pi*crossover));
        body.setReanchorPhaseSearch(phaseSearchMs,false);
        upper.prepare(spec,0,window);
        // Probe mode 45: no legato re-anchor unless the period was steady
        // within 10 ms (wide vibrato and fast bends measured worse with it).
        legato.prepare(spec.sampleRate);legato.setSteadyGuard(steadyGuardMs);
        for(auto& b:lowpass)b.prepare(spec.sampleRate,crossover,false);
        for(auto& b:highpass)b.prepare(spec.sampleRate,crossover,true);
        for(auto* b:{&lowIn,&highIn,&lowOut,&highOut,&detector})b->setSize(channels,capacity);
        lowDelay.assign(size_t(capacity),0);legatoFlags.assign(size_t(capacity),0);pickFlags.assign(size_t(capacity),0);legatoAges.assign(size_t(capacity),0);
        shift=0;wasActive=false;
    }
    void reset() noexcept {
        if(!available) return;
        body.reset();legato.reset();upper.reset();wasActive=false;
        for(auto& b:lowpass)b.clear();for(auto& b:highpass)b.clear();
    }
    int latency() const noexcept {return available ? window : 0;}
    uint64_t processedFrames() const noexcept {return available ? upper.processedFrames() : 0;}
    void setSemitones(int semitones) noexcept {shift=juce::jlimit(-12,12,semitones);body.setSemitones(shift);upper.setSemitones(shift);}
    // render=false keeps every history warm and returns the input unchanged
    // (no read-head output, no FFT frames).
    void process(const juce::AudioBuffer<float>& input,juce::AudioBuffer<float>& output,bool render) noexcept {
        // Crossover-filter state decaying in silence reaches denormals and
        // tripled p99.9 block time at 32 samples without host flushing.
        const juce::ScopedNoDenormals noDenormals;
        const bool active=available && render && shift!=0;
        // The legato detector only feeds re-anchors, so it is skipped while
        // nothing renders (two thirds of the idle cost) and restarts clean
        // when rendering resumes: its history would predate the bypass.
        if(active && !wasActive) legato.reset();
        wasActive=active;
        for(int off=0;off<input.getNumSamples();off+=capacity) {
            const int n=std::min(capacity,input.getNumSamples()-off);
            for(auto* b:{&lowIn,&highIn,&lowOut,&highOut,&detector})b->setSize(channels,n,false,false,true);
            for(int c=0;c<channels;++c)for(int i=0;i<n;++i) {
                const float x=input.getSample(c,off+i);
                detector.setSample(c,i,x);
                lowIn.setSample(c,i,lowpass[1].tick(lowpass[0].tick(x,c),c));
                highIn.setSample(c,i,highpass[1].tick(highpass[0].tick(x,c),c));
            }
            // Chunks end before each 1 ms legato update, so a pick reaches the
            // detector before its next update whatever the host block size.
            for(int start=0;start<n;) {
                int end=start;
                for(;end<n;) {
                    float mono=0;for(int c=0;c<channels;++c)mono+=lowIn.getSample(c,end);
                    const int age=active ? legato.push(mono/float(channels)) : -1;
                    legatoFlags[size_t(end)]=age>=0 ? 1 : 0;legatoAges[size_t(end)]=std::max(age,0);
                    ++end;if(active && legato.nextPushUpdates())break;
                }
                const juce::AudioBuffer<float> lowChunk(lowIn.getArrayOfWritePointers(),channels,start,end-start),detectorChunk(detector.getArrayOfWritePointers(),channels,start,end-start);
                juce::AudioBuffer<float> outChunk(lowOut.getArrayOfWritePointers(),channels,start,end-start);
                body.process(lowChunk,outChunk,active,&detectorChunk,legatoFlags.data()+start,pickFlags.data()+start,lowDelay.data()+start,legatoAges.data()+start);
                for(int i=start;i<end;++i) if(pickFlags[size_t(i)]!=0){legato.notePick();break;}
                start=end;
            }
            upper.process(highIn,highOut,active);
            for(int c=0;c<channels;++c)for(int i=0;i<n;++i)
                output.setSample(c,off+i,active ? lowOut.getSample(c,i)+highOut.getSample(c,i) : input.getSample(c,off+i));
        }
    }
};
}
