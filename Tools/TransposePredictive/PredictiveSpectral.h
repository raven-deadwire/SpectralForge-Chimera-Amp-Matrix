#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <complex>
#include <stdexcept>

namespace transposePredictive {
// Independent research oscillator-bank shifter. Past-only Hann analysis;
// instantaneous peak frequencies drive phase-continuous current-time synthesis.
// No note oracle, proprietary implementation, future samples or dry-tone bypass.
// No residual model: transient/noise loss is an explicit disqualifying risk.
// No fixed acoustic/PDC latency is claimed by the absence of an output FIFO.
class PredictiveSpectral {
    static constexpr int maxTracks = 64;
    static constexpr double tau = 6.2831853071795864769;
    struct Track {
        double frequency{}, phase{}, amplitude{}, target{}, delta{}, omega{};
        bool used{};
    };
    struct Channel {
        std::vector<float> ring, windowedPhase, magnitude;
        std::vector<std::complex<float>> input, spectrum;
        std::array<Track, maxTracks> tracks{};
    };
    std::array<Channel, 2> channels;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<double> window;
    int size{}, hop{128}, position{}, clock{}, count{}, frames{};
    double rate{}, ratio{1.0};

    void analyse(Channel& c) noexcept {
        for (int n=0;n<size;++n)
            c.input[size_t(n)]={float(c.ring[size_t((position+n)%size)]*window[size_t(n)]),0.f};
        fft->perform(c.input.data(),c.spectrum.data(),false);
        double maximum=0;
        for(int k=0;k<=size/2;++k) {
            c.magnitude[size_t(k)]=std::abs(c.spectrum[size_t(k)]);
            maximum=std::max(maximum,double(c.magnitude[size_t(k)]));
        }
        for(auto& t:c.tracks)t.used=false;
        // Bounded peak list ordered by amplitude; retain weak partials down to
        // -60 dB relative to the strongest bin, with a numerical floor.
        std::array<int,maxTracks> peaks{};int number=0;
        for(int k=1;k<size/2;++k) {
            const double m=c.magnitude[size_t(k)];
            if(m<std::max(1.e-9,maximum*.001) || m<=c.magnitude[size_t(k-1)] || m<c.magnitude[size_t(k+1)])continue;
            int j=number;
            if(j==maxTracks){if(m<=c.magnitude[size_t(peaks.back())])continue;--j;}
            while(j>0 && m>c.magnitude[size_t(peaks[size_t(j-1)])]) {
                if(j<maxTracks)peaks[size_t(j)]=peaks[size_t(j-1)];--j;
            }
            peaks[size_t(j)]=k;number=std::min(number+1,maxTracks);
        }
        for(int p=0;p<number;++p) {
            const int k=peaks[size_t(p)];
            const double phase=std::arg(c.spectrum[size_t(k)]);
            const double a=std::log(std::max(1.e-20f,c.magnitude[size_t(k-1)]));
            const double b=std::log(std::max(1.e-20f,c.magnitude[size_t(k)]));
            const double d=std::log(std::max(1.e-20f,c.magnitude[size_t(k+1)]));
            const double curvature=a-2*b+d;
            const double offset=curvature<-1.e-12 ? std::clamp(.5*(a-d)/curvature,-.5,.5):0;
            const double binOmega=tau*k/size;
            double omega=tau*(k+offset)/size;
            if(frames>0) {
                const double residual=std::remainder(phase-c.windowedPhase[size_t(k)]-binOmega*hop,tau);
                const double inferred=binOmega+residual/hop;
                // Reject incoherent phase estimates, not unfavourable scores.
                if(std::abs(inferred-omega)<tau*1.5/size)omega=inferred;
            }
            const double f=omega*rate/tau;
            if(f<15 || f*ratio>=rate*.475)continue;
            const double amplitude=4*std::exp(b-.25*(a-d)*offset)/size;
            int best=-1;double distance=1.5*rate/size;
            for(int j=0;j<maxTracks;++j) {
                const auto& t=c.tracks[size_t(j)];
                const double diff=std::abs(t.frequency-f);
                if(!t.used && t.amplitude>1.e-12 && diff<distance){best=j;distance=diff;}
            }
            bool fresh=false;
            if(best<0)for(int j=0;j<maxTracks;++j)
                if(!c.tracks[size_t(j)].used && c.tracks[size_t(j)].amplitude<=1.e-12){best=j;fresh=true;break;}
            if(best<0)continue;
            auto& t=c.tracks[size_t(best)];
            if(fresh) {
                // Hann centre is N/2. Extrapolate measured input phase to the
                // next output sample, then integrate the shifted frequency.
                t.phase=std::remainder(phase+binOmega*size*.5+omega*size*.5,tau);
                t.amplitude=0;
            }
            t.used=true;t.frequency=f;t.omega=omega*ratio;
            t.target=amplitude;t.delta=(amplitude-t.amplitude)/hop;
        }
        for(auto& t:c.tracks)if(!t.used){t.target=0;t.delta=-t.amplitude/hop;}
        for(int k=0;k<=size/2;++k)c.windowedPhase[size_t(k)]=std::arg(c.spectrum[size_t(k)]);
    }
public:
    void prepare(const juce::dsp::ProcessSpec& spec,int order) {
        if(spec.numChannels<1 || spec.numChannels>2 || spec.sampleRate<8000 || spec.sampleRate>192000 || order<9 || order>13)
            throw std::invalid_argument("unsupported predictive configuration");
        rate=spec.sampleRate;count=int(spec.numChannels);size=1<<order;
        fft=std::make_unique<juce::dsp::FFT>(order);window.resize(size_t(size));
        for(int n=0;n<size;++n)window[size_t(n)]=.5-.5*std::cos(tau*n/size);
        for(auto& c:channels) {
            c.ring.assign(size_t(size),0);c.windowedPhase.assign(size_t(size/2+1),0);
            c.magnitude.assign(size_t(size/2+1),0);c.input.resize(size_t(size));c.spectrum.resize(size_t(size));
        }
        reset();
    }
    void reset() noexcept {
        position=clock=frames=0;
        for(auto& c:channels){std::fill(c.ring.begin(),c.ring.end(),0);std::fill(c.windowedPhase.begin(),c.windowedPhase.end(),0);c.tracks={};}
    }
    void setSemitones(int shift) noexcept {ratio=std::pow(2.0,std::clamp(shift,-12,12)/12.0);}
    int latency() const noexcept {return -1;} // Undefined fixed PDC, not zero.
    void process(const juce::AudioBuffer<float>& in,juce::AudioBuffer<float>& out) noexcept {
        for(int n=0;n<in.getNumSamples();++n) {
            for(int ch=0;ch<count;++ch) {
                auto& c=channels[size_t(ch)];const float x=in.getSample(ch,n);
                c.ring[size_t(position)]=x;double value=0;
                for(auto& t:c.tracks) {
                    t.amplitude+=t.delta;
                    if(clock==hop-1)t.amplitude=t.target;
                    if(t.amplitude>1.e-12)value+=t.amplitude*std::cos(t.phase);
                    t.phase=std::remainder(t.phase+t.omega,tau);
                }
                out.setSample(ch,n,ratio==1?x:float(value));
            }
            position=(position+1)%size;
            if(++clock==hop){clock=0;for(int ch=0;ch<count;++ch)analyse(channels[size_t(ch)]);++frames;}
        }
    }
};
}
