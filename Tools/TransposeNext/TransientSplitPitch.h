#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "CorrelationPitch.h"

namespace transposeNext {
// Research: causal low-band linear predictor. The body input is decimated to
// about 4 kHz; every 1 ms an order-10 predictor is refit (autocorrelation
// method, Hann-windowed 32 ms of past samples only). fraction() is the 1 ms
// prediction-error energy over the 1 ms signal energy. The decaying tail of
// a previous note is predictable; a new pluck is not, even at equal energy.
class LowPredictor {
    static constexpr int order=10;
    int factor{1},phase{},window{},hop{},count{},position{},fractionPosition{};
    std::vector<double> history,hann,fractions;
    std::array<double,order> a{};
    double signalEnergy{},errorEnergy{},coefficient{},fraction_{};
    double past(int age) const noexcept {int i=position-1-age;while(i<0)i+=int(history.size());return history[size_t(i)];}
    void refit() noexcept {
        std::array<double,order+1> r{};
        for(int k=0;k<=order;++k)for(int i=k;i<window;++i)
            r[size_t(k)]+=hann[size_t(i)]*past(window-1-i)*hann[size_t(i-k)]*past(window-1-i+k);
        if(r[0]<=1.e-14)return;
        r[0]*=1.0001;
        std::array<double,order> next{},previous{};double error=r[0];
        for(int m=0;m<order;++m) {
            double acc=r[size_t(m+1)];for(int j=0;j<m;++j)acc-=next[size_t(j)]*r[size_t(m-j)];
            const double k=acc/error;previous=next;next[size_t(m)]=k;
            for(int j=0;j<m;++j)next[size_t(j)]=previous[size_t(j)]-k*previous[size_t(m-1-j)];
            error*=1-k*k;if(error<=0)return;
        }
        a=next;
    }
public:
    void prepare(double rate) {
        factor=std::max(1,int(std::lround(rate/4000.)));const double fd=rate/factor;
        window=int(std::lround(.032*fd));hop=std::max(1,int(std::lround(.001*fd)));
        history.assign(size_t(window+order+1),0);hann.resize(size_t(window));
        for(int i=0;i<window;++i)hann[size_t(i)]=.5-.5*std::cos(2*juce::MathConstants<double>::pi*i/window);
        fractions.assign(size_t(std::max(1,int(std::lround(.025*fd)))),0);
        coefficient=1-std::exp(-1/(fd*.001));reset();
    }
    void reset() noexcept {
        std::fill(history.begin(),history.end(),0);std::fill(fractions.begin(),fractions.end(),0);a.fill(0);
        phase=count=position=fractionPosition=0;signalEnergy=errorEnergy=fraction_=0;
    }
    void push(double x) noexcept {
        if(phase++!=0){if(phase==factor)phase=0;return;}
        if(phase==factor)phase=0;
        if(count%hop==0 && count>=window)refit();
        double predicted=0;for(int k=0;k<order;++k)predicted+=a[size_t(k)]*past(k);
        const double error=x-predicted;
        history[size_t(position)]=x;if(++position==int(history.size()))position=0;++count;
        signalEnergy+=coefficient*(x*x-signalEnergy);errorEnergy+=coefficient*(error*error-errorEnergy);
        // Flush decayed tails: denormal arithmetic stalls the audio thread.
        if(signalEnergy<1.e-30)signalEnergy=0;
        if(errorEnergy<1.e-30)errorEnergy=0;
        fraction_=errorEnergy/std::max(signalEnergy,1.e-12);
        fractions[size_t(fractionPosition)]=fraction_;if(++fractionPosition==int(fractions.size()))fractionPosition=0;
    }
    double fraction() const noexcept {return fraction_;}
    // Rare (on an armed onset); O(25 ms at ~4 kHz) scan.
    double recentMaximum() const noexcept {return *std::max_element(fractions.begin(),fractions.end());}
};
// Research only: preserve the low-frequency read trajectory while reanchoring
// the high-pass branch. A 350 Hz Linkwitz-Riley fourth-order crossover
// limits late high-band leakage from the body branch. Both branches are
// transposed, never dry-mixed. Independent delays can colour the crossover.
// This does NOT provide a common/fixed latency across frequency or host PDC.
class TransientSplitPitch {
    CorrelationPitch body, transient;
    juce::AudioBuffer<float> bodyIn, highIn, bodyOut, highOut, detector;
    struct Biquad {
        double b0{},b1{},b2{},a1{},a2{};
        std::array<double,2> z1{},z2{};
        float tick(float x,int c) noexcept {
            const double y=b0*x+z1[size_t(c)];
            z1[size_t(c)]=b1*x-a1*y+z2[size_t(c)];
            z2[size_t(c)]=b2*x-a2*y;return float(y);
        }
        void clear() noexcept {z1.fill(0);z2.fill(0);}
        void prepare(double rate,bool high) noexcept {
            const double w=2*juce::MathConstants<double>::pi*350./rate;
            const double c=std::cos(w),alpha=std::sin(w)/std::sqrt(2.);
            b0=(high?1+c:1-c)*.5/(1+alpha);b1=(high?-2:2)*b0;b2=b0;
            a1=-2*c/(1+alpha);a2=(1-alpha)/(1+alpha);clear();
        }
    };
    std::array<Biquad,2> lowpass,highpass;
    int channels{}, capacity{};
    // Optional low-band re-anchor (research). An onset found by the transient
    // path arms a 12 ms window; the body is re-anchored only if its own 1 ms
    // energy rises above twice the largest value of the preceding 25 ms. A
    // marker over a sustained low note fails that test, so the body keeps its
    // trajectory (the warble case); a new low note passes. No resets when
    // the transient path sees no onset (tremolo, vibrato).
    bool lowReset{},upshift{},predictive{},coordinated{},targeted{};
    LowPredictor predictor;
    double fractionThreshold{};
    std::vector<uint8_t> lowFlags,broadFlags;
    std::vector<double> lowHistory;
    double lowFast{},lowFastCoefficient{},lowThreshold{};
    int lowHistoryPosition{},lowArmed{},lowWindowLength{},lowResets{};
public:
    void prepare(const juce::dsp::ProcessSpec& spec, bool resetBodyOnLowOnset=false, bool resetOnUpshift=false, bool predictiveConfirm=false, bool coordinate=false, bool targetFixes=false) {
        lowReset=resetBodyOnLowOnset;upshift=resetOnUpshift;predictive=predictiveConfirm;coordinated=coordinate;targeted=targetFixes;predictor.prepare(spec.sampleRate);
        lowFastCoefficient=1-std::exp(-1/(spec.sampleRate*.001));
        lowWindowLength=int(spec.sampleRate*.012);
        lowHistory.assign(size_t(std::ceil(spec.sampleRate*.025)),0);
        lowFlags.assign(size_t(std::max(1,int(spec.maximumBlockSize))),0);
        broadFlags.assign(lowFlags.size(),0);
        body.prepare(spec,40,false);
        transient.prepare(spec,40,true,true);
        body.setUpshiftOnsetReset(upshift);transient.setUpshiftOnsetReset(upshift);
        if(upshift)transient.setBroadSmoothing(4);
        if(coordinated)transient.setRetrigger(8);
        // T1/T4 fixes (2026-10-23 targets): a 4 ms high-path re-anchor guard
        // lets the post-hold upshift splice align without re-reading the
        // attack, a 4 ms splice cross-fade keeps that splice's high-band
        // residue below faint picks, and the relative floor rejects
        // fade-out corner onsets. Onsets are reported at detection so the
        // longer fade cannot delay the low-band confirmation window.
        if(targeted){transient.setReanchorGuard(4);transient.setSpliceFade(4);transient.setRelativeFloor(2.e-5);transient.setReportAtDetection(true);}
        channels=int(spec.numChannels);capacity=std::max(1,int(spec.maximumBlockSize));
        for(auto& b:lowpass)b.prepare(spec.sampleRate,false);
        for(auto& b:highpass)b.prepare(spec.sampleRate,true);
        for(auto* b:{&bodyIn,&highIn,&bodyOut,&highOut,&detector})b->setSize(channels,capacity);
        reset();
    }
    void reset() noexcept {
        body.reset();transient.reset();for(auto& b:lowpass)b.clear();for(auto& b:highpass)b.clear();
        std::fill(lowHistory.begin(),lowHistory.end(),0);predictor.reset();lowFast=lowThreshold=0;lowHistoryPosition=lowArmed=lowResets=0;
    }
    int lowOnsetResets() const noexcept {return lowResets;}
    void setSemitones(int n) noexcept {body.setSemitones(n);transient.setSemitones(n);shift=n;}
    int latency() const noexcept {return -1;}
    int splices() const noexcept {return body.splices()+transient.splices();}
    int onsetResets() const noexcept {return transient.onsetResets();}
    const auto& onsetLocations() const noexcept {return transient.onsetLocations();}
    double readMinimum() const noexcept {return std::min(body.readMinimum(),transient.readMinimum());}
    double readMaximum() const noexcept {return std::max(body.readMaximum(),transient.readMaximum());}
    void process(const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, bool render=true) noexcept {
        for(int off=0;off<input.getNumSamples();off+=capacity) {
            const int n=std::min(capacity,input.getNumSamples()-off);
            for(auto* b:{&bodyIn,&highIn,&bodyOut,&highOut,&detector})b->setSize(channels,n,false,false,true);
            for(int c=0;c<channels;++c)for(int i=0;i<n;++i) {
                const float x=input.getSample(c,off+i);
                const float low=lowpass[1].tick(lowpass[0].tick(x,c),c);
                const float high=highpass[1].tick(highpass[0].tick(x,c),c);
                detector.setSample(c,i,x);bodyIn.setSample(c,i,low);highIn.setSample(c,i,high);
            }
            const int transientSplices=transient.splices();
            transient.process(highIn,highOut,render,&detector,nullptr,lowReset?broadFlags.data():nullptr);
            // Coordinated (research): defer the body's splice search while the
            // transient path spliced in this chunk.
            if(coordinated)body.setSplicesHeld(transient.splices()!=transientSplices);
            if(lowReset) for(int i=0;i<n;++i) {
                double energy=0;for(int c=0;c<channels;++c){const double v=bodyIn.getSample(c,i);energy+=v*v;}
                lowFast+=lowFastCoefficient*(energy/channels-lowFast);
                if(predictive){double mono=0;for(int c=0;c<channels;++c)mono+=bodyIn.getSample(c,i);predictor.push(mono/channels);}
                if(broadFlags[size_t(i)]!=0 && render && (shift<0 || (upshift && shift>0))) {
                    // Rare (transient cooldown bounds it); O(25 ms) scan.
                    lowThreshold=2.*(*std::max_element(lowHistory.begin(),lowHistory.end()))+1.e-12;
                    lowArmed=lowWindowLength;
                    if(predictive)fractionThreshold=3.*predictor.recentMaximum();
                }
                lowHistory[size_t(lowHistoryPosition)]=lowFast;
                if(++lowHistoryPosition==int(lowHistory.size()))lowHistoryPosition=0;
                // Predictive confirm (research): or the low-band prediction-error
                // fraction exceeds 3x its preceding 25 ms maximum and 0.1.
                const bool onset=lowArmed>0 && (lowFast>lowThreshold ||
                    (predictive && predictor.fraction()>fractionThreshold && predictor.fraction()>.1));
                if(lowArmed>0)--lowArmed;
                lowFlags[size_t(i)]=onset?1:0;
                if(onset){lowArmed=0;++lowResets;}
            }
            body.process(bodyIn,bodyOut,render,nullptr,lowReset?lowFlags.data():nullptr);
            for(int c=0;c<channels;++c)for(int i=0;i<n;++i)
                output.setSample(c,off+i,(!render || shift==0) ? input.getSample(c,off+i)
                    : bodyOut.getSample(c,i)+highOut.getSample(c,i));
        }
    }
private:
    int shift{};
};
}
