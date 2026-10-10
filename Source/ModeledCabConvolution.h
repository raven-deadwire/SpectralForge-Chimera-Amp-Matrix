#pragma once
#include <juce_dsp/juce_dsp.h>
#include <complex>
#include <memory>
#include <vector>
#include "CabRealtimeProfile.h"
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <xmmintrin.h>
#endif

namespace spectralforge {
// A modeled IR is mono and is applied independently to both input channels.
// Linearity lets one complex convolution carry left + i * right: its real and
// imaginary outputs are the two separate convolutions, not a stereo downmix.
// Keep the existing small-block uniform partition schedule. In particular, no
// larger, intermittent tail FFT or extra block of processing latency is added.
// Construction and all allocation are worker-only; process/reset are bounded.
class ModeledCabConvolution {
    using Complex = std::complex<float>;
    static int orderFor(int size) noexcept
    {
        int order=0;while((1<<order)<size)++order;return order;
    }
    static void packSpectrum(const Complex* from,float* to,int size) noexcept
    {
        for(int i=0;i<size;++i) {to[i]=from[i].real();to[size+i]=from[i].imag();}
    }
    static void packRealSpectrum(const float* from,float* to,int bins) noexcept
    {
        for(int i=0;i<bins;++i) {to[i]=from[2*i];to[bins+i]=from[2*i+1];}
    }
public:
    // Only immutable authored spectra are shared. Every microphone, channel
    // pair and fading kernel retains its own FFT object, input ring and tail.
    // The worker cache keys this response by model, sample rate and block size;
    // shared ownership is acquired/released only during worker construction
    // and destruction, never while processing an audio callback.
    struct Prepared {
        const bool mono;
        const int blockSize, fftSize, bins, segmentSize, segments, inputSegments;
        std::vector<float> impulseSpectra;
        Prepared(const juce::AudioBuffer<float>& samples,int maximumBlockSize,bool monoInput=false)
            : mono(monoInput), blockSize(juce::nextPowerOfTwo(maximumBlockSize)), fftSize(4*blockSize),
              bins(mono ? fftSize/2+1 : fftSize),
              segmentSize(fftSize-blockSize), segments(samples.getNumSamples()/segmentSize+1),
              inputSegments(3*segments), impulseSpectra(size_t(segments)*size_t(2*bins))
        {
            jassert(samples.getNumChannels()==1 && maximumBlockSize>0 && maximumBlockSize<=128);
            juce::dsp::FFT fft(orderFor(fftSize));
            if(mono) {
                std::vector<float> transform(size_t(2*fftSize),0.f);
                for(int partition=0;partition<segments;++partition) {
                    std::fill(transform.begin(),transform.end(),0.f);
                    const int offset=partition*segmentSize;
                    const int count=juce::jmin(segmentSize,samples.getNumSamples()-offset);
                    for(int i=0;i<count;++i)transform[size_t(i)]=samples.getSample(0,offset+i);
                    fft.performRealOnlyForwardTransform(transform.data(),true);
                    packRealSpectrum(transform.data(),impulseSpectra.data()+size_t(partition)*size_t(2*bins),bins);
                }
                return;
            }
            std::vector<Complex> inputTime(size_t(fftSize),Complex{}), transform(size_t(fftSize),Complex{});
            for(int partition=0;partition<segments;++partition) {
                std::fill(inputTime.begin(),inputTime.end(),Complex{});
                const int offset=partition*segmentSize;
                const int count=juce::jmin(segmentSize,samples.getNumSamples()-offset);
                for(int i=0;i<count;++i)inputTime[size_t(i)]={samples.getSample(0,offset+i),0.f};
                // FFT::perform requires distinct input and output arrays.
                fft.perform(inputTime.data(),transform.data(),false);
                packSpectrum(transform.data(),impulseSpectra.data()+size_t(partition)*size_t(2*fftSize),fftSize);
            }
        }
    };
private:
    std::shared_ptr<const Prepared> prepared;
    const bool mono;
    const int blockSize, fftSize, bins, segments, inputSegments;
    juce::dsp::FFT fft;
    // Per-partition planar real/imaginary spectra keep the multiply/accumulate
    // vectorizable and contiguous instead of traversing separately allocated
    // channel/partition buffers. Both input channels share each IR spectrum.
    std::vector<float> inputSpectra, pastSum, outputSpectrum;
    std::vector<Complex> inputTime, transform, outputTime, overlap;
    std::vector<float> monoInput, monoTransform, monoOverlap;
    int currentSegment{}, inputPosition{};

    void accumulate(const float* __restrict input,const float* __restrict impulse,float* __restrict output) const noexcept
    {
        // Fuse the four previous vector passes: load each input/IR component
        // once and read/write each output only once per partition. The buffers
        // are disjoint; restrict makes that contract available to the compiler.
        // Keep the original arithmetic order and do not enable fast-math.
        int i=0;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        // Native Windows profiling identified sustained spectral-MAC cost in
        // this loop. Make its four-float SIMD work explicit on MSVC instead of
        // depending on auto-vectorization of the two variable-offset stores.
        // Mono has N/2+1 bins, so neither the imaginary plane nor every input
        // partition is SIMD-aligned. Unaligned loads/stores are intentional.
        for(;i+4<=bins;i+=4) {
            const auto inputReal=_mm_loadu_ps(input+i);
            const auto inputImag=_mm_loadu_ps(input+bins+i);
            const auto impulseReal=_mm_loadu_ps(impulse+i);
            const auto impulseImag=_mm_loadu_ps(impulse+bins+i);
            const auto outputReal=_mm_loadu_ps(output+i);
            const auto outputImag=_mm_loadu_ps(output+bins+i);
            const auto real=_mm_sub_ps(_mm_add_ps(outputReal,_mm_mul_ps(inputReal,impulseReal)),
                                      _mm_mul_ps(inputImag,impulseImag));
            const auto imag=_mm_add_ps(_mm_add_ps(outputImag,_mm_mul_ps(inputReal,impulseImag)),
                                      _mm_mul_ps(inputImag,impulseReal));
            _mm_storeu_ps(output+i,real);
            _mm_storeu_ps(output+bins+i,imag);
        }
#endif
        for(;i<bins;++i) {
            const float re=(output[i]+input[i]*impulse[i])-input[bins+i]*impulse[bins+i];
            const float im=(output[bins+i]+input[i]*impulse[bins+i])+input[bins+i]*impulse[i];
            output[i]=re;output[bins+i]=im;
        }
    }
    void sumPartitions(const float* currentInput,bool firstChunk) noexcept
    {
        // Delayed partitions cannot change during a partial host block.
        // Recompute only the leading partition for subsequent partial calls.
        if(firstChunk) {
            std::fill(pastSum.begin(),pastSum.end(),0.f);
            int index=currentSegment;
            for(int partition=1;partition<segments;++partition) {
                index+=3;if(index>=inputSegments)index-=inputSegments;
                accumulate(inputSpectra.data()+size_t(index)*size_t(2*bins),
                    prepared->impulseSpectra.data()+size_t(partition)*size_t(2*bins),pastSum.data());
            }
        }
        juce::FloatVectorOperations::copy(outputSpectrum.data(),pastSum.data(),2*bins);
        accumulate(currentInput,prepared->impulseSpectra.data(),outputSpectrum.data());
    }
    void processMono(juce::AudioBuffer<float>& buffer) noexcept
    {
        auto* channel=buffer.getWritePointer(0);
        for(int offset=0;offset<buffer.getNumSamples();) {
            const bool firstChunk=inputPosition==0;
            const int count=juce::jmin(buffer.getNumSamples()-offset,blockSize-inputPosition);
            juce::FloatVectorOperations::copy(monoInput.data()+inputPosition,channel+offset,count);
            {
                SF_CAB_PROFILE_SCOPE(FFTForward);
                juce::FloatVectorOperations::copy(monoTransform.data(),monoInput.data(),fftSize);
                // Keep the platform's real FFT (notably vDSP on macOS). Only
                // non-negative bins, including DC and Nyquist, enter the FDL.
                fft.performRealOnlyForwardTransform(monoTransform.data(),true);
            }
            {
                SF_CAB_PROFILE_SCOPE(SpectralMAC);
                auto* currentInput=inputSpectra.data()+size_t(currentSegment)*size_t(2*bins);
                packRealSpectrum(monoTransform.data(),currentInput,bins);
                sumPartitions(currentInput,firstChunk);
                for(int i=0;i<bins;++i) {
                    monoTransform[size_t(2*i)]=outputSpectrum[size_t(i)];
                    monoTransform[size_t(2*i+1)]=outputSpectrum[size_t(bins+i)];
                }
            }
            {
                SF_CAB_PROFILE_SCOPE(FFTInverse);
                fft.performRealOnlyInverseTransform(monoTransform.data());
            }
            juce::FloatVectorOperations::add(channel+offset,monoTransform.data()+inputPosition,monoOverlap.data()+inputPosition,count);
            inputPosition+=count;offset+=count;
            if(inputPosition==blockSize) {
                std::fill(monoInput.begin(),monoInput.end(),0.f);inputPosition=0;
                juce::FloatVectorOperations::add(monoTransform.data()+blockSize,monoOverlap.data()+blockSize,fftSize-2*blockSize);
                juce::FloatVectorOperations::copy(monoOverlap.data(),monoTransform.data()+blockSize,fftSize-blockSize);
                currentSegment=currentSegment>0 ? currentSegment-1 : inputSegments-1;
            }
        }
    }
public:
    explicit ModeledCabConvolution(std::shared_ptr<const Prepared> response)
        : prepared(std::move(response)), mono(prepared->mono), blockSize(prepared->blockSize), fftSize(prepared->fftSize), bins(prepared->bins),
          segments(prepared->segments), inputSegments(prepared->inputSegments), fft(orderFor(fftSize)),
          inputSpectra(size_t(inputSegments)*size_t(2*bins)),
          pastSum(size_t(2*bins)), outputSpectrum(size_t(2*bins)),
          inputTime(size_t(mono ? 0 : fftSize)), transform(size_t(mono ? 0 : fftSize)),
          outputTime(size_t(mono ? 0 : fftSize)), overlap(size_t(mono ? 0 : fftSize)),
          monoInput(size_t(mono ? fftSize : 0)), monoTransform(size_t(mono ? 2*fftSize : 0)), monoOverlap(size_t(mono ? fftSize : 0))
    {
        reset();
    }
    ModeledCabConvolution(const juce::AudioBuffer<float>& samples,int maximumBlockSize,bool monoInput=false)
        : ModeledCabConvolution(std::make_shared<const Prepared>(samples,maximumBlockSize,monoInput)) {}
    int getLatency() const noexcept {return 0;}
    void reset() noexcept
    {
        std::fill(inputSpectra.begin(),inputSpectra.end(),0.f);
        std::fill(pastSum.begin(),pastSum.end(),0.f);
        std::fill(outputSpectrum.begin(),outputSpectrum.end(),0.f);
        std::fill(inputTime.begin(),inputTime.end(),Complex{});
        std::fill(transform.begin(),transform.end(),Complex{});
        std::fill(outputTime.begin(),outputTime.end(),Complex{});
        std::fill(overlap.begin(),overlap.end(),Complex{});
        std::fill(monoInput.begin(),monoInput.end(),0.f);
        std::fill(monoTransform.begin(),monoTransform.end(),0.f);
        std::fill(monoOverlap.begin(),monoOverlap.end(),0.f);
        currentSegment=0;inputPosition=0;
    }
    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        jassert(buffer.getNumChannels()<=2);
        jassert(!mono || buffer.getNumChannels()<=1);
        if(buffer.getNumChannels()==0 || buffer.getNumSamples()==0)return;
        if(mono) {processMono(buffer);return;}
        auto* left=buffer.getWritePointer(0);
        auto* right=buffer.getNumChannels()>1 ? buffer.getWritePointer(1) : nullptr;
        for(int offset=0;offset<buffer.getNumSamples();) {
            const bool firstChunk=inputPosition==0;
            const int count=juce::jmin(buffer.getNumSamples()-offset,blockSize-inputPosition);
            for(int i=0;i<count;++i)inputTime[size_t(inputPosition+i)]={left[offset+i],right ? right[offset+i] : 0.f};
            {
                SF_CAB_PROFILE_SCOPE(FFTForward);
                fft.perform(inputTime.data(),transform.data(),false);
            }
            {
                SF_CAB_PROFILE_SCOPE(SpectralMAC);
                auto* currentInput=inputSpectra.data()+size_t(currentSegment)*size_t(2*fftSize);
                packSpectrum(transform.data(),currentInput,fftSize);
                sumPartitions(currentInput,firstChunk);
                for(int i=0;i<fftSize;++i)transform[size_t(i)]={outputSpectrum[size_t(i)],outputSpectrum[size_t(fftSize+i)]};
            }
            {
                SF_CAB_PROFILE_SCOPE(FFTInverse);
                fft.perform(transform.data(),outputTime.data(),true);
            }
            for(int i=0;i<count;++i) {
                const auto value=outputTime[size_t(inputPosition+i)]+overlap[size_t(inputPosition+i)];
                left[offset+i]=value.real();if(right)right[offset+i]=value.imag();
            }
            inputPosition+=count;offset+=count;
            if(inputPosition==blockSize) {
                std::fill(inputTime.begin(),inputTime.end(),Complex{});
                inputPosition=0;
                for(int i=blockSize;i<fftSize-blockSize;++i)outputTime[size_t(i)]+=overlap[size_t(i)];
                std::copy(outputTime.begin()+blockSize,outputTime.end(),overlap.begin());
                currentSegment=currentSegment>0 ? currentSegment-1 : inputSegments-1;
            }
        }
    }
};
}
