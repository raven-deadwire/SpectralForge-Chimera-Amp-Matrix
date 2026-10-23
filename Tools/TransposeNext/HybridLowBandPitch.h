#pragma once
#include "CorrelationPitch.h"
#include "PolyPitch.h"

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
// had drifted. boundMs caps the sustained low-band delay (8-80 ms).
class HybridLowBandPitch {
    struct Biquad {
        double b0{},b1{},b2{},a1{},a2{};
        std::array<double,2> z1{},z2{};
        float tick(float x,int c) noexcept {
            const double y=b0*x+z1[size_t(c)];
            z1[size_t(c)]=b1*x-a1*y+z2[size_t(c)];
            z2[size_t(c)]=b2*x-a2*y;return float(y);
        }
        void prepare(double rate,double hz,bool high) noexcept {
            const double w=2*juce::MathConstants<double>::pi*hz/rate;
            const double c=std::cos(w),alpha=std::sin(w)/std::sqrt(2.);
            b0=(high?1+c:1-c)*.5/(1+alpha);b1=(high?-2:2)*b0;b2=b0;
            a1=-2*c/(1+alpha);a2=(1-alpha)/(1+alpha);z1.fill(0);z2.fill(0);
        }
    };
    CorrelationPitch body;
    spectralforge::PolyPitch upper;
    std::array<Biquad,2> lowpass,highpass;   // cascaded Butterworth pairs = LR4
    juce::AudioBuffer<float> lowIn,highIn,lowOut,highOut,detector;
    int channels{},capacity{},shift{};
    bool anchored{};
public:
    void prepare(const juce::dsp::ProcessSpec& spec,double crossover,double boundMs=40,bool onset=false) {
        channels=int(spec.numChannels);capacity=std::max(1,int(spec.maximumBlockSize));
        anchored=onset;
        body.prepare(spec,boundMs,onset,onset);
        if(onset){body.setUpshiftOnsetReset(true);body.setBroadSmoothing(4);}
        upper.prepare(spec,0,int(spec.sampleRate*.010)/8*8);
        for(auto& b:lowpass)b.prepare(spec.sampleRate,crossover,false);
        for(auto& b:highpass)b.prepare(spec.sampleRate,crossover,true);
        for(auto* b:{&lowIn,&highIn,&lowOut,&highOut,&detector})b->setSize(channels,capacity);
    }
    int onsetResets() const noexcept {return body.onsetResets();}
    void setSemitones(int n) noexcept {shift=n;body.setSemitones(n);upper.setSemitones(n);}
    int latency() const noexcept {return -1;}
    void process(const juce::AudioBuffer<float>& input,juce::AudioBuffer<float>& output) noexcept {
        for(int off=0;off<input.getNumSamples();off+=capacity) {
            const int n=std::min(capacity,input.getNumSamples()-off);
            for(auto* b:{&lowIn,&highIn,&lowOut,&highOut,&detector})b->setSize(channels,n,false,false,true);
            for(int c=0;c<channels;++c)for(int i=0;i<n;++i) {
                const float x=input.getSample(c,off+i);
                detector.setSample(c,i,x);
                lowIn.setSample(c,i,lowpass[1].tick(lowpass[0].tick(x,c),c));
                highIn.setSample(c,i,highpass[1].tick(highpass[0].tick(x,c),c));
            }
            body.process(lowIn,lowOut,shift!=0,anchored?&detector:nullptr);
            upper.process(highIn,highOut,shift!=0);
            for(int c=0;c<channels;++c)for(int i=0;i<n;++i)
                output.setSample(c,off+i,shift==0 ? input.getSample(c,off+i) : lowOut.getSample(c,i)+highOut.getSample(c,i));
        }
    }
};
}
