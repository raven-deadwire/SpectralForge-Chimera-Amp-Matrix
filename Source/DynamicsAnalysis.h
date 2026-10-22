#pragma once
#include "SpectrumTransport.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <vector>

namespace spectralforge {
enum class DynamicsStage { none=-1, gate=0, compressor, limiter, multiband, deesser, preCompressor, postCompressor, lowCompressor };

// A single visible dynamics editor owns this analysis-only stream. Audio is
// copied at the actual module boundary; no FFT, allocation or locking occurs
// in the callback. Hidden editors disable capture. On source changes the UI
// uses EQCaptureBoundary to retire in-flight audio before draining this FIFO.
class DynamicsAnalysis {
public:
    EQAnalyzerFIFO fifo;
    // Message-thread-only consumer ownership; the audio thread never reads it.
    void* uiConsumer{nullptr};
    std::atomic<int> selected{int(DynamicsStage::none)};
    // Native PRE can contain more than one compressor. Select its stable owner,
    // never concatenate different pedals into a fabricated spectrum.
    std::atomic<int> preOwner{-1};
    // -1: observe the actual POST slot; 0: legacy compressor; 1: native bank.
    // A native detail view that is not on the active audio route must not
    // display the legacy compressor's signal underneath native controls.
    std::atomic<int> postNativeSource{-1};
    void prepare(double sr,int maximumBlock) {
        rate.store(std::max(8000.,sr),std::memory_order_release);
        before.resize(size_t(std::max(1,maximumBlock)));
        captured=0;
    }
    double sampleRate() const noexcept {return rate.load(std::memory_order_acquire);}
    void select(DynamicsStage stage) noexcept {selected.store(int(stage),std::memory_order_release);}
    void disable() noexcept {fifo.enabled.store(false,std::memory_order_release);}
    bool isSelected(DynamicsStage stage) const noexcept {
        return fifo.enabled.load(std::memory_order_relaxed) && selected.load(std::memory_order_relaxed)==int(stage);
    }
    class Scope {
    public:
        Scope(DynamicsAnalysis* owner,DynamicsStage stage,juce::AudioBuffer<float>& audio) noexcept
            :bus(owner),source(stage),buffer(audio),active(bus && bus->begin(source,buffer)){}
        ~Scope() noexcept {if(active)bus->finish(source,buffer);}
        Scope(const Scope&)=delete;
        Scope& operator=(const Scope&)=delete;
    private:
        DynamicsAnalysis* bus;DynamicsStage source;juce::AudioBuffer<float>& buffer;bool active;
    };
private:
    bool begin(DynamicsStage stage,const juce::AudioBuffer<float>& audio) noexcept {
        if(!isSelected(stage) || captured!=0 || audio.getNumChannels()==0 || audio.getNumSamples()<=0)return false;
        const auto n=audio.getNumSamples();
        if(size_t(n)>before.size()){fifo.dropped.fetch_add(uint64_t(n),std::memory_order_relaxed);return false;}
        const auto* left=audio.getReadPointer(0);const auto* right=audio.getReadPointer(std::min(1,audio.getNumChannels()-1));
        for(int i=0;i<n;++i)before[size_t(i)]={left[i],right[i]};
        captured=n;return true;
    }
    void finish(DynamicsStage stage,const juce::AudioBuffer<float>& audio) noexcept {
        const auto n=captured;captured=0;
        if(!isSelected(stage) || n!=audio.getNumSamples() || audio.getNumChannels()==0)return;
        const auto* left=audio.getReadPointer(0);const auto* right=audio.getReadPointer(std::min(1,audio.getNumChannels()-1));
        for(int i=0;i<n;++i)fifo.push({before[size_t(i)][0],left[i],before[size_t(i)][1],right[i]});
    }
    std::vector<std::array<float,2>> before;
    std::atomic<double> rate{48000.};int captured{};
};
}
