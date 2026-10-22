#pragma once
#include "CorrelationPitch.h"

namespace transposeNext {
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
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {
        body.prepare(spec,40,false);
        transient.prepare(spec,40,true,true);
        channels=int(spec.numChannels);capacity=std::max(1,int(spec.maximumBlockSize));
        for(auto& b:lowpass)b.prepare(spec.sampleRate,false);
        for(auto& b:highpass)b.prepare(spec.sampleRate,true);
        for(auto* b:{&bodyIn,&highIn,&bodyOut,&highOut,&detector})b->setSize(channels,capacity);
        reset();
    }
    void reset() noexcept {body.reset();transient.reset();for(auto& b:lowpass)b.clear();for(auto& b:highpass)b.clear();}
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
            body.process(bodyIn,bodyOut,render);transient.process(highIn,highOut,render,&detector);
            for(int c=0;c<channels;++c)for(int i=0;i<n;++i)
                output.setSample(c,off+i,(!render || shift==0) ? input.getSample(c,off+i)
                    : bodyOut.getSample(c,i)+highOut.getSample(c,i));
        }
    }
private:
    int shift{};
};
}
