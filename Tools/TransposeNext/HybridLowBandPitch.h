#pragma once
#include "CorrelationPitch.h"
#include "PhaseLockPitch.h"
#include "LegatoDetector.h"

namespace transposeNext {
// Research only (2026-10-23): low band through the time-domain correlation
// engine, upper band through the 10 ms STFT (production PolyPitch with the
// Tight analysis window). Synthetic evidence showed the time-domain engine
// keeps low-note pitch in every direction but fails on chords, while the STFT
// handles chords but mistunes notes below ~100 Hz. A fourth-order
// Linkwitz-Riley split at `crossover` separates them; both bands are
// transposed, never dry-mixed. The low band's delay is variable, so the bands
// are not time-aligned and the crossover region can colour. No fixed PDC.
// Optional (onset=true): the low band re-anchors its read head on a new pick,
// detected from the full-band input (#117 innovation detector, upshifts too),
// so a note starts near the causal boundary instead of wherever the read head
// had drifted. boundMs caps the sustained low-band delay (8-80 ms); alignMs
// places those attacks at a fixed delay (10 = the STFT band's latency).
class HybridLowBandPitch {
    struct Biquad {
        double b0{},b1{},b2{},a1{},a2{};
        std::array<double,2> z1{},z2{};
        float tick(float x,int c) noexcept {
            const double y=b0*x+z1[size_t(c)];
            z1[size_t(c)]=b1*x-a1*y+z2[size_t(c)];
            z2[size_t(c)]=b2*x-a2*y;return float(y);
        }
        void prepare(double rate,double hz,bool high,double q=1/std::sqrt(2.)) noexcept {
            const double w=2*juce::MathConstants<double>::pi*hz/rate;
            const double c=std::cos(w),alpha=std::sin(w)/(2*q);
            b0=(high?1+c:1-c)*.5/(1+alpha);b1=(high?-2:2)*b0;b2=b0;
            a1=-2*c/(1+alpha);a2=(1-alpha)/(1+alpha);z1.fill(0);z2.fill(0);
        }
    };
    CorrelationPitch body;
    PhaseLockPitch upper;   // research copy of PolyPitch; identical unless setPhaseLock
    std::array<Biquad,4> lowpass,highpass;   // LR4: two Butterworth sections; LR8: four (Q 0.541/1.307, each twice)
    int sections{2};
    juce::AudioBuffer<float> lowIn,highIn,lowOut,highOut,detector;
    int channels{},capacity{},shift{};
    bool anchored{};
    double sampleRate{},crossoverHz{};
    int reportedLatency{-1};
    std::vector<double> lowDelay;
    LegatoDetector legato;
    std::vector<uint8_t> legatoFlags,pickFlags;
    std::vector<int> legatoAges;
    bool legatoOn{};
    bool phaseLock{};
    int solo{};   // diagnostics: 0 both bands, 1 low band only, 2 upper band only
public:
    void prepare(const juce::dsp::ProcessSpec& spec,double crossover,double boundMs=40,bool onset=false,double alignMs=0,bool alignCompensated=false,double onsetFadeMs=0,double leadMs=2.9,bool fadeAdaptive=false,bool lr8=false,bool lock=false,float lockOffset=-1) {
        channels=int(spec.numChannels);capacity=std::max(1,int(spec.maximumBlockSize));
        anchored=onset;sampleRate=spec.sampleRate;crossoverHz=crossover;reportedLatency=-1;
        body.prepare(spec,boundMs,onset,onset);
        if(onset){body.setUpshiftOnsetReset(true);body.setBroadSmoothing(4);body.setAnchorAlignment(alignMs,alignCompensated,leadMs);if(onsetFadeMs>0)body.setOnsetFade(onsetFadeMs,onsetFadeMs,fadeAdaptive);}
        upper.prepare(spec,0,int(spec.sampleRate*.010)/8*8);
        phaseLock=lock;upper.setPhaseLock(lock ? 3*crossover : 0,lockOffset);
        lowDelay.assign(size_t(capacity),0);
        legato.prepare(spec.sampleRate);legatoFlags.assign(size_t(capacity),0);legatoAges.assign(size_t(capacity),0);pickFlags.assign(size_t(capacity),0);legatoOn=false;
        sections=lr8?4:2;
        for(int k=0;k<4;++k) {
            const double q=lr8 ? (k%2==0 ? .5411961 : 1.3065630) : 1/std::sqrt(2.);
            lowpass[size_t(k)].prepare(spec.sampleRate,crossover,false,q);highpass[size_t(k)].prepare(spec.sampleRate,crossover,true,q);
        }
        for(auto* b:{&lowIn,&highIn,&lowOut,&highOut,&detector})b->setSize(channels,capacity);
    }
    int onsetResets() const noexcept {return body.onsetResets();}
    void setSolo(int band) noexcept {solo=band;}
    // Research: re-anchor the low band on pitch changes without a pick
    // (LegatoDetector on the low-band input), like a detected pick.
    void setLegatoReanchor(bool on) noexcept {legatoOn=on;legato.reset();}
    int legatoEvents() const noexcept {return legato.legatoEvents();}
    int legatoGuarded() const noexcept {return legato.guardedCandidates();}
    void setLegatoSteadyGuard(double ms) noexcept {legato.setSteadyGuard(ms);}
    void setLegatoDriftEvent(double fraction) noexcept {legato.setDriftEvent(fraction);}
    void setReanchorPhaseSearch(double ms, bool picks) noexcept {body.setReanchorPhaseSearch(ms,picks);}
    // Research: centre held downshifts on the reported latency (CorrelationPitch::setSustainCentre),
    // i.e. a low-band read delay of L - tau. Call after setAttackLatency.
    void setSustainCentre(double costLimit) noexcept {body.setSustainCentre(1000.*(reportedLatency-crossoverDelaySamples())/sampleRate,costLimit);}
    int splices() const noexcept {return body.splices();}
    int earlySplices() const noexcept {return body.earlySpliceCount();}
    // Research: the low band's read delay (samples) for each sample of the last
    // process() chunk, and this crossover's group delay (samples).
    const double* lastReadDelays() const noexcept {return lowDelay.data();}
    double crossoverDelaySamples() const noexcept {return sampleRate*2*std::sqrt(2.)/(2*juce::MathConstants<double>::pi*crossoverHz);}
    void setSemitones(int n) noexcept {shift=n;body.setSemitones(n);upper.setSemitones(n);}
    // Reported latency (research latency contract): with setAttackLatency the
    // hybrid promises picked attacks at `reportedLatency` samples in both bands;
    // sustained low-band delay still varies (2-40 ms). -1 = not reported.
    int latency() const noexcept {return reportedLatency;}
    // Align low-band attacks to the STFT band's latency (its analysis window),
    // compensating this crossover's LR4 group delay at DC, 2 sqrt(2) / w_c, and
    // report that latency. leadMs: detection + crossfade lead, measured.
    void setAttackLatency(double leadMs) noexcept {
        const double window=double(int(sampleRate*.010)/8*8);
        reportedLatency=int(window);
        // Measured (CHIMERA_LEAD_MS sweep, 800 Hz): lead 1.4 ms makes the attack
        // delay independent of the ratio, 1.4 ms short of the target, so the
        // target is raised by the lead.
        body.setAnchorAlignment(1000.*window/sampleRate+leadMs,true,leadMs);
        body.setAnchorInputDelay(1000.*2*std::sqrt(2.)/(2*juce::MathConstants<double>::pi*crossoverHz));
    }
    void process(const juce::AudioBuffer<float>& input,juce::AudioBuffer<float>& output) noexcept {
        // Measured: crossover-filter state decaying in silence reaches denormals
        // and triples p99.9 block time at 32 samples when the host does not
        // flush them. Do not rely on the caller's ScopedNoDenormals.
        const juce::ScopedNoDenormals noDenormals;
        for(int off=0;off<input.getNumSamples();off+=capacity) {
            const int n=std::min(capacity,input.getNumSamples()-off);
            for(auto* b:{&lowIn,&highIn,&lowOut,&highOut,&detector})b->setSize(channels,n,false,false,true);
            for(int c=0;c<channels;++c)for(int i=0;i<n;++i) {
                const float x=input.getSample(c,off+i);
                detector.setSample(c,i,x);
                float lo=x,hi=x;
                for(int k=0;k<sections;++k){lo=lowpass[size_t(k)].tick(lo,c);hi=highpass[size_t(k)].tick(hi,c);}
                lowIn.setSample(c,i,lo);highIn.setSample(c,i,hi);
            }
            if(legatoOn) {
                // Chunks end before each 1 ms detector update, so a pick reaches
                // the detector before its next update whatever the block size
                // (notePick once per block made the output block-size dependent).
                for(int start=0;start<n;) {
                    int end=start;
                    for(;end<n;) {
                        float mono=0;for(int c=0;c<channels;++c)mono+=lowIn.getSample(c,end);
                        const int age=legato.push(mono/float(channels));
                        legatoFlags[size_t(end)]=age>=0 ? 1 : 0;legatoAges[size_t(end)]=std::max(age,0);
                        ++end;if(legato.nextPushUpdates())break;
                    }
                    const juce::AudioBuffer<float> lowChunk(lowIn.getArrayOfWritePointers(),channels,start,end-start),detectorChunk(detector.getArrayOfWritePointers(),channels,start,end-start);
                    juce::AudioBuffer<float> outChunk(lowOut.getArrayOfWritePointers(),channels,start,end-start);
                    body.process(lowChunk,outChunk,shift!=0,anchored?&detectorChunk:nullptr,legatoFlags.data()+start,pickFlags.data()+start,lowDelay.data()+start,legatoAges.data()+start);
                    for(int i=start;i<end;++i) if(pickFlags[size_t(i)]!=0){legato.notePick();break;}
                    start=end;
                }
            } else body.process(lowIn,lowOut,shift!=0,anchored?&detector:nullptr,nullptr,nullptr,lowDelay.data(),nullptr);
            upper.process(highIn,highOut,shift!=0,phaseLock?lowDelay.data():nullptr);
            for(int c=0;c<channels;++c)for(int i=0;i<n;++i)
                output.setSample(c,off+i,shift==0 ? input.getSample(c,off+i) : (solo!=2 ? lowOut.getSample(c,i) : 0.f)+(solo!=1 ? highOut.getSample(c,i) : 0.f));
        }
    }
};
}
