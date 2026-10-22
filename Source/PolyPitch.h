#pragma once
#include <juce_dsp/juce_dsp.h>
#include <complex>
#include <vector>

namespace spectralforge {
// Streaming STFT pitch shifter. Instantaneous frequencies, not FFT-bin centres,
// are transposed; local peak phase locking keeps each partial's bins coherent.
// All storage is reserved in prepare(). Latency is one FFT window for every shift.
class PolyPitch {
    struct Channel {
        std::vector<float> input,ola,previousPhase,phase,magnitude,frequency,sourcePhase;
        std::vector<std::complex<float>> spectrum,transform,mappedPhase;
        std::vector<int> peaks;
        std::vector<unsigned char> initialised;
        // Opt-in formant-mode state (allocated in prepare, idle unless on).
        std::vector<float> inputMagnitude,envelope,shiftedEnvelope;
    };
    std::array<Channel,2> channels;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;
    int size{},hop{},inputPosition{},outputPosition{},clock{},channelCount{};
    float ratio{1};
    bool synthesising{};
    uint64_t framesProcessed{};
    // Optional formant-preservation mode; default off so the released code path
    // and its frozen regression fixture are byte-for-byte unchanged until a host
    // opts in.
    bool formantPreserve{};
    int envHalf{1};
    double sampleRate{};
    // Moving-average spectral envelope over +/- half bins; O(bins), no allocation.
    static void boxAverage(const float* src,float* dst,int n,int half) noexcept
    {
        double sum=0; int lo=0,hi=-1;
        for(int b=0;b<n;++b) {
            const int L=b-half<0?0:b-half, H=b+half>n-1?n-1:b+half;
            while(hi<H){++hi;sum+=src[(size_t)hi];}
            while(lo<L){sum-=src[(size_t)lo];++lo;}
            dst[(size_t)b]=float(sum/double(H-L+1));
        }
    }
    void frame(Channel& channel)
    {
        ++framesProcessed;
        using Complex=std::complex<float>;
        constexpr float pi=juce::MathConstants<float>::pi,twoPi=juce::MathConstants<float>::twoPi;
        for(int n=0;n<size;++n) channel.transform[(size_t)n]=Complex(channel.input[(size_t)((inputPosition+n)%size)]*window[(size_t)n],0);
        fft->perform(channel.transform.data(),channel.spectrum.data(),false);
        const int bins=size/2+1;
        std::fill(channel.magnitude.begin(),channel.magnitude.end(),0);
        std::fill(channel.frequency.begin(),channel.frequency.end(),0);
        std::fill(channel.mappedPhase.begin(),channel.mappedPhase.end(),Complex{});
        // Formant-mode pre-pass: per-bin magnitude and its smoothed envelope.
        // Gated so the default path below is untouched when the mode is off.
        if(formantPreserve) {
            for(int b=0;b<bins;++b) channel.inputMagnitude[(size_t)b]=std::abs(channel.spectrum[(size_t)b]);
            boxAverage(channel.inputMagnitude.data(),channel.envelope.data(),bins,envHalf);
        }
        const float step=twoPi*float(hop)/float(size);
        double inputEnergy=0;
        for(int b=1;b<bins-1;++b)
        {
            const auto value=channel.spectrum[(size_t)b]; const float magnitude=std::abs(value),phase=std::arg(value);
            const float residual=std::remainder(phase-channel.previousPhase[(size_t)b]-step*b,twoPi);
            channel.previousPhase[(size_t)b]=phase;
            const float actual=float(b)+residual/step;
            const float mapped=b*ratio; const int first=(int)mapped;
            const float fraction=mapped-first;
            // A downshifted low bin may land partly below bin 1. Its
            // surviving side still enters the output; excluding that source
            // from the energy reference spuriously attenuates bass notes.
            if((first>0 && first<bins-1) || (fraction>0.f && first+1>0 && first+1<bins-1))
                inputEnergy+=double(magnitude)*magnitude;
            // Formant mode spreads the envelope-flattened (whitened) magnitude so
            // the shift moves the excitation, not the formants. Energy reference
            // above still uses the true magnitude.
            const float source=formantPreserve ? magnitude/std::max(channel.envelope[(size_t)b],1e-12f) : magnitude;
            for(int side=0;side<2;++side) {
                const int destination=first+side;
                if(destination<1 || destination>=bins-1) continue;
                const float weight=source*(side==0 ? 1-fraction : fraction);
                channel.magnitude[(size_t)destination]+=weight;
                channel.frequency[(size_t)destination]+=weight*actual*ratio;
                channel.mappedPhase[(size_t)destination]+=std::polar(weight,phase);
            }
        }
        channel.peaks.clear();
        for(int b=1;b<bins-1;++b) {
            const float magnitude=channel.magnitude[(size_t)b];
            channel.sourcePhase[(size_t)b]=std::arg(channel.mappedPhase[(size_t)b]);
            if(magnitude>1e-8f) {
                if(!channel.initialised[(size_t)b]) {channel.phase[(size_t)b]=channel.sourcePhase[(size_t)b];channel.initialised[(size_t)b]=1;}
                else channel.phase[(size_t)b]=std::remainder(channel.phase[(size_t)b]+step*channel.frequency[(size_t)b]/magnitude,twoPi);
            } else channel.initialised[(size_t)b]=0;
            if(magnitude>1e-8f && magnitude>=channel.magnitude[(size_t)b-1] && magnitude>channel.magnitude[(size_t)b+1]) channel.peaks.push_back(b);
        }
        // Formant mode: restore the source envelope at each destination bin by
        // dividing out the shifted envelope and multiplying the source envelope
        // back in. Peaks were picked from the whitened/shifted magnitude above.
        if(formantPreserve) {
            boxAverage(channel.magnitude.data(),channel.shiftedEnvelope.data(),bins,envHalf);
            for(int b=1;b<bins-1;++b) channel.magnitude[(size_t)b]*=channel.envelope[(size_t)b]/std::max(channel.shiftedEnvelope[(size_t)b],1e-12f);
        }
        std::fill(channel.spectrum.begin(),channel.spectrum.end(),Complex{});
        size_t peakIndex=0;
        for(int b=1;b<bins-1;++b) {
            int peak=b;
            if(!channel.peaks.empty()) {
                while(peakIndex+1<channel.peaks.size() && std::abs(channel.peaks[peakIndex+1]-b)<std::abs(channel.peaks[peakIndex]-b)) ++peakIndex;
                peak=channel.peaks[peakIndex];
            }
            const float phase=channel.phase[(size_t)peak]-pi*float(size-1)/size*float(b-peak);
            channel.spectrum[(size_t)b]=std::polar(channel.magnitude[(size_t)b],phase);
            channel.spectrum[(size_t)(size-b)]=std::conj(channel.spectrum[(size_t)b]);
        }
        double outputEnergy=0;for(int b=1;b<bins-1;++b) outputEnergy+=std::norm(channel.spectrum[(size_t)b]);
        const float energyGain=outputEnergy>1e-16 ? float(std::sqrt(inputEnergy/outputEnergy)) : 1.f;
        fft->perform(channel.spectrum.data(),channel.transform.data(),true);
        const float normalisation=2.f*hop/size*energyGain;
        for(int n=0;n<size;++n) channel.ola[(size_t)((outputPosition+n)%(size*2))]+=channel.transform[(size_t)n].real()*window[(size_t)n]*normalisation;

    }
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        int order=8;while((1<<order)<spec.sampleRate*.032) ++order;
        size=1<<order;hop=size/8;channelCount=(int)spec.numChannels;fft=std::make_unique<juce::dsp::FFT>(order);
        sampleRate=spec.sampleRate;
        // Envelope smoothing half-width: ~350 Hz, wide enough to ride over the
        // harmonics while tracking formants. Idle unless formant mode is on.
        envHalf=juce::jmax(1,(int)std::lround(350.0/(sampleRate/double(size))));
        window.resize((size_t)size);for(int i=0;i<size;++i) window[(size_t)i]=std::sin(juce::MathConstants<float>::pi*(i+.5f)/size);
        for(auto& channel:channels) {
            channel.input.assign((size_t)size,0);channel.ola.assign((size_t)size*2,0);
            for(auto* values:{&channel.previousPhase,&channel.phase,&channel.magnitude,&channel.frequency,&channel.sourcePhase,
                              &channel.inputMagnitude,&channel.envelope,&channel.shiftedEnvelope}) values->assign((size_t)size/2+1,0);
            channel.initialised.assign((size_t)size/2+1,0);channel.spectrum.resize((size_t)size);channel.transform.resize((size_t)size);channel.mappedPhase.resize((size_t)size/2+1);channel.peaks.reserve((size_t)size/2+1);
        }
        inputPosition=outputPosition=clock=0;synthesising=false;framesProcessed=0;
    }
    void reset() {
        for(auto& channel:channels) {
            std::fill(channel.input.begin(),channel.input.end(),0);std::fill(channel.ola.begin(),channel.ola.end(),0);
            std::fill(channel.previousPhase.begin(),channel.previousPhase.end(),0);std::fill(channel.phase.begin(),channel.phase.end(),0);std::fill(channel.initialised.begin(),channel.initialised.end(),0);
        }
        inputPosition=outputPosition=clock=0;synthesising=false;framesProcessed=0;
    }
    int latency() const {return size;}
    uint64_t processedFrames() const {return framesProcessed;}
    void setSemitones(int value) {ratio=std::pow(2.f,float(juce::jlimit(-12,12,value))/12.f);}
    // Optional formant-preservation mode, default off. Toggling is allocation-free
    // and may be called between blocks; buffers are reserved in prepare().
    void setFormantPreservation(bool enabled) {formantPreserve=enabled;}
    bool formantPreservation() const {return formantPreserve;}
    void process(const juce::AudioBuffer<float>& input,juce::AudioBuffer<float>& output,bool render=true)
    {
        // Keep the input ring warm without running FFTs when no shifted audio
        // is audible. Clear old overlap/phase state once, never allocate here.
        if(!render && synthesising) {
            for(auto& channel:channels) {
                std::fill(channel.ola.begin(),channel.ola.end(),0);
                std::fill(channel.previousPhase.begin(),channel.previousPhase.end(),0);
                std::fill(channel.phase.begin(),channel.phase.end(),0);
                std::fill(channel.initialised.begin(),channel.initialised.end(),0);
            }
        }
        synthesising=render;
        for(int n=0;n<input.getNumSamples();++n) {
            for(int c=0;c<channelCount;++c) {
                auto& channel=channels[(size_t)c];channel.input[(size_t)inputPosition]=input.getSample(c,n);
                output.setSample(c,n,channel.ola[(size_t)outputPosition]);channel.ola[(size_t)outputPosition]=0;
            }
            inputPosition=(inputPosition+1)%size;outputPosition=(outputPosition+1)%(size*2);
            if(++clock==hop) {clock=0;if(render)for(int c=0;c<channelCount;++c) frame(channels[(size_t)c]);}
        }
    }
};
}
