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
    bool fading{}, active{}, onsetReset{}, innovationReset{}, pendingOnset{};
    std::array<float,2> noveltyLow1{},noveltyLow2{},noveltyLow3{},noveltyLow4{};
    float noveltyCoefficient{};
    std::array<double,41> peakBuckets{};
    int peakBucketPosition{},peakBucketSamples{},peakBucketLength{};
    double bucketPeak{},bucketMaximum{};
    double fastEnergy{},slowEnergy{},broadEnergy{},fastCoefficient{},peakDecay{};
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
        const double hi=maximum-guard;
        const double travel=(hi-lo)*.25;
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
        fadeIndex=0;fading=true;++spliceCount;
    }
public:
    void prepare(const juce::dsp::ProcessSpec& spec, double boundMs, bool resetAtOnset=false, bool detectInnovation=false) {
        if(spec.sampleRate<8000 || spec.sampleRate>192000 || spec.numChannels<1 || spec.numChannels>2
           || boundMs<8 || boundMs>80) throw std::invalid_argument("unsupported correlation configuration");
        rate=spec.sampleRate;count=int(spec.numChannels);
        onsetReset=resetAtOnset;innovationReset=detectInnovation;
        peakBucketLength=std::max(1,int(std::ceil(rate*.001)));
        noveltyCoefficient=float(1-std::exp(-2*juce::MathConstants<double>::pi*2500./rate));
        fastCoefficient=1-std::exp(-1/(rate*.0003));
        peakDecay=std::exp(-1/(rate*.060));cooldownLength=int(rate*.045);
        detectorHistory.assign(size_t(std::ceil(rate*.001)),0);
        minimum=rate*.002;maximum=rate*boundMs*.001;
        window=int(std::ceil(rate*.012));stride=juce::jmax(1,int(std::lround(rate/6000.)));
        fadeLength=juce::jmax(8,int(std::ceil(rate*.002)));
        capacity=int(std::ceil(maximum))+window+fadeLength+16;
        coefficient=float(1.-std::exp(-2.*juce::MathConstants<double>::pi*800./rate));
        for(auto* bank:{&audio,&filtered}) for(auto& r:*bank) r.assign(size_t(capacity),0);
        fadeWeights.resize(size_t(fadeLength));
        for(int i=0;i<fadeLength;++i) fadeWeights[size_t(i)]=float(.5-.5*std::cos(juce::MathConstants<double>::pi*(i+1)/fadeLength));
        reset();
    }
    void reset() noexcept {
        for(auto* bank:{&audio,&filtered})for(auto& r:*bank)std::fill(r.begin(),r.end(),0);
        low.fill(0);noveltyLow1.fill(0);noveltyLow2.fill(0);noveltyLow3.fill(0);noveltyLow4.fill(0);pendingOnset=false;write=fadeIndex=spliceCount=0;fading=active=false;
        fastEnergy=slowEnergy=broadEnergy=0;cooldown=onsets=0;processedSamples=0;onsetSamples.fill(0);
        peakBuckets.fill(0);peakBucketPosition=peakBucketSamples=0;bucketPeak=bucketMaximum=0;
        detectorPosition=0;std::fill(detectorHistory.begin(),detectorHistory.end(),0);
        delay=nextDelay=minimum;minRead=maximum;maxRead=minimum;
    }
    void setSemitones(int semitones) noexcept {
        const double newRatio=std::pow(2.,juce::jlimit(-12,12,semitones)/12.);
        if(newRatio!=ratio) {
            ratio=newRatio;delta=1-ratio;
            // Preserve the current read position on automation. The sample
            // loop will schedule a bounded crossfade if a boundary is near.
        }
    }
    int latency() const noexcept {return -1;} // Deliberately NOT advertised as 2/10/40 ms fixed PDC.
    int splices() const noexcept {return spliceCount;}
    int onsetResets() const noexcept {return onsets;}
    const auto& onsetLocations() const noexcept {return onsetSamples;}
    double readMinimum() const noexcept {return minRead;}
    double readMaximum() const noexcept {return maxRead;}
    void process(const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, bool render=true, const juce::AudioBuffer<float>* detectorInput=nullptr) noexcept {
        for(int n=0;n<input.getNumSamples();++n) {
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
            broadEnergy+=fastCoefficient*(inputEnergy/count-broadEnergy);
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
                active=false;fading=false;pendingOnset=false;
            } else {
                if(!active) {delay=delta>0?minimum:maximum;fading=false;active=true;}
                const double guard=std::abs(delta)*fadeLength+2;
                // Optional downshift experiment: a detected new pick event
                // anchors the read head near the causal boundary. It still
                // resamples the entire signal; no unshifted tonal bypass.
                // Disabled for upshifts, which need past material to repeat.
                if(innovationReset && onsetReset && delta>0 && cooldown==0 && inputEnergy>1.e-14 && fastEnergy>1.e-10 && novelty>4.*slowEnergy)
                    pendingOnset=true;
                if(onsetReset && delta>0 && !fading && cooldown==0 &&
                   (innovationReset ? pendingOnset : (fastEnergy>1.e-8 && fastEnergy>2.5*slowEnergy))) {
                    pendingOnset=false;
                    nextDelay=minimum+guard;fadeIndex=0;fading=true;
                    cooldown=cooldownLength;
                    if(onsets<int(onsetSamples.size()))onsetSamples[size_t(onsets)]=processedSamples;
                    ++onsets;
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
                if(fading) {nextDelay+=delta;if(++fadeIndex==fadeLength){delay=nextDelay;fading=false;}}
            }
            if(++write==capacity)write=0;
            ++processedSamples;
        }
    }
};
}
