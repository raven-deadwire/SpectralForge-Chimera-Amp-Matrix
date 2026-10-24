#pragma once
#include <juce_dsp/juce_dsp.h>
#include <complex>
#include <vector>

namespace spectralforge {
// Streaming STFT pitch shifter. Instantaneous frequencies, not FFT-bin centres,
// are transposed; local peak phase locking keeps each partial's bins coherent.
// All storage is reserved in prepare(). Latency is one analysis window for every
// shift. The analysis window equals the FFT length unless prepare() is given a
// shorter analysisLength, which is zero-padded into the next power-of-two FFT.
class PolyPitch {
    struct Channel {
        std::vector<float> input,ola,previousPhase,phase,magnitude,frequency,sourcePhase;
        std::vector<std::complex<float>> spectrum,transform,mappedPhase;
        std::vector<int> peaks;
        std::vector<unsigned char> initialised;
        // Opt-in formant-mode state (allocated in prepare, idle unless on).
        // inputMagnitude/envelope are length bins; symLog/envLog and the teA/teC
        // FFT scratch are full length (size) for the cepstral true-envelope.
        std::vector<float> inputMagnitude,envelope,symLog,envLog;
        std::vector<std::complex<float>> teA,teC;
    };
    std::array<Channel,2> channels;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;
    int size{},length{},hop{},inputPosition{},outputPosition{},clock{},channelCount{};
    float ratio{1};
    bool synthesising{};
    uint64_t framesProcessed{};
    // Optional formant-preservation mode; default off so the released code path
    // and its frozen regression fixture are byte-for-byte unchanged until a host
    // opts in.
    bool formantPreserve{};
    int cepOrder{16};
    double sampleRate{},fftRoundTrip{1};
    // Robel-Rodet true envelope of channel.inputMagnitude[0..bins) into
    // channel.envelope. Iterative cepstral liftering drives the envelope above
    // the spectral peaks, so it tracks formants rather than averaging across the
    // harmonic valleys the way a box filter does. Scratch and the class FFT are
    // reused; fftRoundTrip divides out whatever scaling the inverse/forward pair
    // applies so exp() stays well-scaled regardless of the FFT's convention.
    void trueEnvelope(Channel& channel,int bins) noexcept
    {
        using Complex=std::complex<float>;
        const int n=size;
        for(int b=0;b<bins;++b) channel.symLog[(size_t)b]=std::log(std::max(channel.inputMagnitude[(size_t)b],1e-9f));
        for(int b=1;b<bins-1;++b) channel.symLog[(size_t)(n-b)]=channel.symLog[(size_t)b];
        std::fill(channel.envLog.begin(),channel.envLog.end(),-60.f);
        for(int iteration=0;iteration<5;++iteration) {
            for(int k=0;k<n;++k) channel.teA[(size_t)k]=Complex(std::max(channel.symLog[(size_t)k],channel.envLog[(size_t)k]),0.f);
            fft->perform(channel.teA.data(),channel.teC.data(),true);                 // real cepstrum
            for(int k=cepOrder+1;k<n-cepOrder;++k) channel.teC[(size_t)k]=Complex{};   // keep low quefrencies
            fft->perform(channel.teC.data(),channel.teA.data(),false);                // back to a smooth log spectrum
            for(int k=0;k<n;++k) channel.envLog[(size_t)k]=float(channel.teA[(size_t)k].real()/fftRoundTrip);
        }
        for(int b=0;b<bins;++b) channel.envelope[(size_t)b]=std::exp(channel.envLog[(size_t)b]);
    }
    void frame(Channel& channel)
    {
        ++framesProcessed;
        using Complex=std::complex<float>;
        constexpr float pi=juce::MathConstants<float>::pi,twoPi=juce::MathConstants<float>::twoPi;
        for(int n=0;n<length;++n) channel.transform[(size_t)n]=Complex(channel.input[(size_t)((inputPosition+n)%length)]*window[(size_t)n],0);
        for(int n=length;n<size;++n) channel.transform[(size_t)n]=Complex{};
        fft->perform(channel.transform.data(),channel.spectrum.data(),false);
        const int bins=size/2+1;
        std::fill(channel.magnitude.begin(),channel.magnitude.end(),0);
        std::fill(channel.frequency.begin(),channel.frequency.end(),0);
        std::fill(channel.mappedPhase.begin(),channel.mappedPhase.end(),Complex{});
        // Formant-mode pre-pass: per-bin magnitude and its smoothed envelope.
        // Gated so the default path below is untouched when the mode is off.
        if(formantPreserve) {
            for(int b=0;b<bins;++b) channel.inputMagnitude[(size_t)b]=std::abs(channel.spectrum[(size_t)b]);
            trueEnvelope(channel,bins);
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
        // Formant mode: re-impose the source spectral envelope on the shifted
        // (whitened) excitation. Whitening by the envelope and multiplying it
        // back repositions the formants with a single estimate; peaks above were
        // picked from the whitened/shifted magnitude.
        if(formantPreserve)
            for(int b=1;b<bins-1;++b) channel.magnitude[(size_t)b]*=channel.envelope[(size_t)b];
        std::fill(channel.spectrum.begin(),channel.spectrum.end(),Complex{});
        size_t peakIndex=0;
        for(int b=1;b<bins-1;++b) {
            int peak=b;
            if(!channel.peaks.empty()) {
                while(peakIndex+1<channel.peaks.size() && std::abs(channel.peaks[peakIndex+1]-b)<std::abs(channel.peaks[peakIndex]-b)) ++peakIndex;
                peak=channel.peaks[peakIndex];
            }
            const float phase=channel.phase[(size_t)peak]-pi*float(length-1)/size*float(b-peak);
            channel.spectrum[(size_t)b]=std::polar(channel.magnitude[(size_t)b],phase);
            channel.spectrum[(size_t)(size-b)]=std::conj(channel.spectrum[(size_t)b]);
        }
        double outputEnergy=0;for(int b=1;b<bins-1;++b) outputEnergy+=std::norm(channel.spectrum[(size_t)b]);
        const float energyGain=outputEnergy>1e-16 ? float(std::sqrt(inputEnergy/outputEnergy)) : 1.f;
        fft->perform(channel.spectrum.data(),channel.transform.data(),true);
        const float normalisation=2.f*hop/length*energyGain;
        for(int n=0;n<length;++n) channel.ola[(size_t)((outputPosition+n)%(length*2))]+=channel.transform[(size_t)n].real()*window[(size_t)n]*normalisation;

    }
public:
    // windowOverride (0 = default): request a specific FFT window length, rounded
    // up to a power of two, to trade latency against low-frequency resolution.
    // Omitted by every existing caller, so the released ~32 ms window, its latency
    // contract and the frozen regression output are unchanged.
    // analysisLength (0 = the FFT length): a shorter analysis/synthesis window,
    // rounded down to a multiple of 8 and zero-padded into the next power-of-two
    // FFT. Latency follows this window, not the FFT size, so a millisecond target
    // need not be rounded up to a power of two. When it equals the FFT length the
    // arithmetic is identical to the power-of-two path.
    void prepare(const juce::dsp::ProcessSpec& spec,int windowOverride=0,int analysisLength=0)
    {
        int order=8;while((1<<order)<spec.sampleRate*.032) ++order;
        if(windowOverride>0){order=0;while((1<<order)<windowOverride) ++order;}
        if(analysisLength>0){analysisLength=juce::jmax(64,analysisLength/8*8);order=0;while((1<<order)<analysisLength) ++order;}
        size=1<<order;length=analysisLength>0 ? analysisLength : size;hop=length/8;channelCount=(int)spec.numChannels;fft=std::make_unique<juce::dsp::FFT>(order);
        sampleRate=spec.sampleRate;
        // Cepstral lifter order ~1.7 ms of quefrency: wide enough to smooth the
        // harmonic structure, tight enough to follow formants. Idle unless on.
        cepOrder=juce::jlimit(16,size/4,(int)std::lround(sampleRate/600.0));
        // Measure the FFT's inverse->forward round-trip gain once so the true
        // envelope is independent of the library's normalization convention.
        {
            std::vector<std::complex<float>> probe((size_t)size,std::complex<float>{}),scratch((size_t)size,std::complex<float>{});
            probe[0]={1.f,0.f};
            fft->perform(probe.data(),scratch.data(),true);
            fft->perform(scratch.data(),probe.data(),false);
            fftRoundTrip=std::abs(probe[0].real())>1e-20f ? double(probe[0].real()) : 1.0;
        }
        window.resize((size_t)length);for(int i=0;i<length;++i) window[(size_t)i]=std::sin(juce::MathConstants<float>::pi*(i+.5f)/length);
        for(auto& channel:channels) {
            channel.input.assign((size_t)length,0);channel.ola.assign((size_t)length*2,0);
            for(auto* values:{&channel.previousPhase,&channel.phase,&channel.magnitude,&channel.frequency,&channel.sourcePhase,
                              &channel.inputMagnitude,&channel.envelope}) values->assign((size_t)size/2+1,0);
            channel.symLog.assign((size_t)size,0);channel.envLog.assign((size_t)size,0);
            channel.teA.resize((size_t)size);channel.teC.resize((size_t)size);
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
    int latency() const {return length;}
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
            inputPosition=(inputPosition+1)%length;outputPosition=(outputPosition+1)%(length*2);
            if(++clock==hop) {clock=0;if(render)for(int c=0;c<channelCount;++c) frame(channels[(size_t)c]);}
        }
    }
};
}
