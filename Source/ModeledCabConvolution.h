#pragma once
#include <juce_dsp/juce_dsp.h>
#include <complex>
#include <vector>

namespace spectralforge {
// A modeled IR is mono and is applied independently to both input channels.
// Linearity lets one complex convolution carry left + i * right: its real and
// imaginary outputs are the two separate convolutions, not a stereo downmix.
// Keep the existing small-block uniform partition schedule. In particular, no
// larger, intermittent tail FFT or extra block of processing latency is added.
// Construction and all allocation are worker-only; process/reset are bounded.
class ModeledCabConvolution {
    using Complex = std::complex<float>;
    const int blockSize, fftSize, segmentSize, segments, inputSegments;
    juce::dsp::FFT fft;
    // Per-partition planar real/imaginary spectra keep the multiply/accumulate
    // vectorizable and contiguous instead of traversing separately allocated
    // channel/partition buffers. Both input channels share each IR spectrum.
    std::vector<float> impulseSpectra, inputSpectra, pastSum, outputSpectrum;
    std::vector<Complex> inputTime, transform, outputTime, overlap;
    int currentSegment{}, inputPosition{};

    static int orderFor(int size) noexcept
    {
        int order=0;while((1<<order)<size)++order;return order;
    }
    void packSpectrum(const Complex* from,float* to) const noexcept
    {
        for(int i=0;i<fftSize;++i) {to[i]=from[i].real();to[fftSize+i]=from[i].imag();}
    }
    void accumulate(const float* input,const float* impulse,float* output) const noexcept
    {
        juce::FloatVectorOperations::addWithMultiply(output,input,impulse,fftSize);
        juce::FloatVectorOperations::subtractWithMultiply(output,input+fftSize,impulse+fftSize,fftSize);
        juce::FloatVectorOperations::addWithMultiply(output+fftSize,input,impulse+fftSize,fftSize);
        juce::FloatVectorOperations::addWithMultiply(output+fftSize,input+fftSize,impulse,fftSize);
    }
public:
    ModeledCabConvolution(const juce::AudioBuffer<float>& samples,int maximumBlockSize)
        : blockSize(juce::nextPowerOfTwo(maximumBlockSize)), fftSize(4*blockSize),
          segmentSize(fftSize-blockSize), segments(samples.getNumSamples()/segmentSize+1),
          inputSegments(3*segments), fft(orderFor(fftSize)),
          impulseSpectra(size_t(segments)*size_t(2*fftSize)),
          inputSpectra(size_t(inputSegments)*size_t(2*fftSize)),
          pastSum(size_t(2*fftSize)), outputSpectrum(size_t(2*fftSize)),
          inputTime(size_t(fftSize)), transform(size_t(fftSize)),
          outputTime(size_t(fftSize)), overlap(size_t(fftSize))
    {
        jassert(samples.getNumChannels()==1 && maximumBlockSize>0 && maximumBlockSize<=128);
        for(int partition=0;partition<segments;++partition) {
            std::fill(inputTime.begin(),inputTime.end(),Complex{});
            const int offset=partition*segmentSize;
            const int count=juce::jmin(segmentSize,samples.getNumSamples()-offset);
            for(int i=0;i<count;++i)inputTime[size_t(i)]={samples.getSample(0,offset+i),0.f};
            // FFT::perform requires distinct input and output arrays.
            fft.perform(inputTime.data(),transform.data(),false);
            packSpectrum(transform.data(),impulseSpectra.data()+size_t(partition)*size_t(2*fftSize));
        }
        reset();
    }
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
        currentSegment=0;inputPosition=0;
    }
    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        jassert(buffer.getNumChannels()<=2);
        if(buffer.getNumChannels()==0 || buffer.getNumSamples()==0)return;
        auto* left=buffer.getWritePointer(0);
        auto* right=buffer.getNumChannels()>1 ? buffer.getWritePointer(1) : nullptr;
        for(int offset=0;offset<buffer.getNumSamples();) {
            const bool firstChunk=inputPosition==0;
            const int count=juce::jmin(buffer.getNumSamples()-offset,blockSize-inputPosition);
            for(int i=0;i<count;++i)inputTime[size_t(inputPosition+i)]={left[offset+i],right ? right[offset+i] : 0.f};
            fft.perform(inputTime.data(),transform.data(),false);
            auto* currentInput=inputSpectra.data()+size_t(currentSegment)*size_t(2*fftSize);
            packSpectrum(transform.data(),currentInput);
            // Delayed partitions cannot change during a partial host block.
            // Recompute only the leading partition for subsequent partial calls.
            if(firstChunk) {
                std::fill(pastSum.begin(),pastSum.end(),0.f);
                int index=currentSegment;
                for(int partition=1;partition<segments;++partition) {
                    index+=3;if(index>=inputSegments)index-=inputSegments;
                    accumulate(inputSpectra.data()+size_t(index)*size_t(2*fftSize),
                        impulseSpectra.data()+size_t(partition)*size_t(2*fftSize),pastSum.data());
                }
            }
            juce::FloatVectorOperations::copy(outputSpectrum.data(),pastSum.data(),2*fftSize);
            accumulate(currentInput,impulseSpectra.data(),outputSpectrum.data());
            for(int i=0;i<fftSize;++i)transform[size_t(i)]={outputSpectrum[size_t(i)],outputSpectrum[size_t(fftSize+i)]};
            fft.perform(transform.data(),outputTime.data(),true);
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
