#pragma once
#include "GraphicalEQ.h"
#include <juce_dsp/juce_dsp.h>
#include <limits>

namespace spectralforge {

// Analysis-only, single message-thread consumer of the existing SPSC tap.
// All FFT plans and sample/transform/power buffers are allocated at construction.
// update() first copies one bounded FIFO snapshot out of the audio queue, then
// analyzes EVERY complete N/4-hop window in chronological order (75% overlap).
// The envelope advances per analysis window, not per UI timer, so an attack
// between paints still contributes even when the latest raw window is silent.
// It never processes, delays or writes host audio.
//
// Display calibration is coherent sinusoidal amplitude in dBFS. L/R powers are
// averaged (not their waveforms), so opposite-polarity stereo cannot disappear.
// Hann ENBW is 1.5 bins: changing FFT size changes measured frequency resolution
// and observation time. displayDB() interpolates/aggregates for screen pixels;
// it cannot recover information below that measured resolution.
class EQSpectrumAnalyzer {
public:
    static constexpr int maximumFFTSize=8192;
    static constexpr int maximumBins=maximumFFTSize/2+1;
    static constexpr float floorDB=-120.f;
    static constexpr float floorPower=1.e-12f;
    static constexpr double idleGraceSeconds=.12;

    EQSpectrumAnalyzer() {makeWindow();clearHistory();}

    // Call reset(fifo) at the same UI transition, before enabling capture again.
    // If a caller omits that flush, the next update defensively discards pending
    // old-resolution data instead of interpreting it as a fresh observation.
    void setFFTSize(int size) noexcept {
        if(frozen)return; // The snapshot's frequency mapping is immutable.
        size=size==2048?2048:size==8192?8192:4096;
        if(size==selectedSize)return;
        selectedSize=size;makeWindow();clearHistory();pendingReset=true;
    }
    // Ballistics changes preserve the current spectrum and observation window.
    void setSmoothing(int selection) noexcept {if(!frozen)smoothing=std::clamp(selection,0,2);}

    // Message thread only, like update/reset. Freeze holds the last COMPLETE
    // finite real frame, not an idle-decayed envelope or a partial observation.
    // The caller may disable capture while frozen. Pending producer data is
    // discarded without touching host audio or waiting for the audio thread.
    // The snapshot survives visibility/LIVE resets and sample-rate changes;
    // resolution/smoothing controls are locked until thaw. It is editor-local
    // display state, never a host parameter, preset, A/B or undo operation.
    void setFrozen(bool shouldFreeze,EQAnalyzerFIFO& fifo) noexcept {
        if(frozen==shouldFreeze)return;
        frozen=shouldFreeze;
        reset(fifo);
        if(!frozen) {
            // There must be no old-rate transition flush after fresh data
            // arrives, and no previous frame may reappear on an empty refreeze.
            sampleRate=0;completed.valid=false;
        }
    }
    bool isFrozen() const noexcept {return frozen;}
    bool canFreezeSnapshot() const noexcept {return completed.valid;}

    void reset(EQAnalyzerFIFO& fifo) noexcept {
        resetSignalHistory(fifo);
        lastConsumed=lastPending=lastWindows=0;lastTransformMicros=lastUpdateMicros=0;
        lastConsumerInterval=0;previousUpdateValid=false;
    }

    // True means visible spectrum/state changed. The caller owns the visibility
    // gate and must not call update for a hidden or disabled analyzer.
    bool update(EQAnalyzerFIFO& fifo,double rate) noexcept {
        const auto started=Clock::now();
        lastConsumerInterval=previousUpdateValid?1000.*seconds(started-previousUpdate):0.;
        previousUpdate=started;previousUpdateValid=true;++updateCount;
        bool changed=false;lastConsumed=lastWindows=lastPending=0;lastTransformMicros=0;
        if(frozen) {
            lastPending=fifo.pendingSamples();discarded+=fifo.discardPending();
            observedDropped=fifo.dropped.load(std::memory_order_relaxed);
            return finishUpdate(started,false);
        }
        if(!std::isfinite(rate) || rate<8000.) {
            ++discontinuityCount;resetSignalHistory(fifo);sampleRate=0;
            return finishUpdate(started,true);
        }
        if(pendingReset) {resetSignalHistory(fifo);changed=true;}
        if(sampleRate>0 && rate!=sampleRate) {
            ++discontinuityCount;resetSignalHistory(fifo);sampleRate=rate;return finishUpdate(started,true);
        }
        sampleRate=rate;
        const auto droppedAtStart=fifo.dropped.load(std::memory_order_relaxed);
        if(droppedAtStart!=observedDropped) {
            recoverDiscontinuity(fifo);return finishUpdate(started,true);
        }

        const auto available=std::min(EQAnalyzerFIFO::capacity,fifo.pendingSamples());
        lastPending=available;
        // Free producer headroom BEFORE the FFT catch-up work. Storage is
        // preallocated; this snapshot cannot grow while the producer runs.
        while(lastConsumed<available && fifo.pop(staged[(size_t)lastConsumed]))++lastConsumed;
        totalConsumed+=lastConsumed; // Successful pops, including an invalidated snapshot.
        // A concurrent overflow invalidates continuity even if its counter
        // changed while this snapshot was being copied. Never join both sides.
        if(fifo.dropped.load(std::memory_order_relaxed)!=observedDropped) {
            recoverDiscontinuity(fifo);return finishUpdate(started,true);
        }
        for(uint32_t n=0;n<lastConsumed;++n) {
            const auto& frame=staged[(size_t)n];
            const size_t p=(size_t)writePosition;
            const bool valid=std::isfinite(frame.inputL) && std::isfinite(frame.outputL) &&
                             std::isfinite(frame.inputR) && std::isfinite(frame.outputR);
            consecutiveValidSamples=valid?std::min(selectedSize,consecutiveValidSamples+1):0;
            history[0][p]=finite(frame.inputL);history[1][p]=finite(frame.outputL);
            history[2][p]=finite(frame.inputR);history[3][p]=finite(frame.outputR);
            writePosition=(writePosition+1)&(selectedSize-1);
            collected=std::min(selectedSize,collected+1);++samplesSinceTransform;
            if(collected==selectedSize && (!windowClockStarted || samplesSinceTransform>=uint64_t(hopSamples()))) {
                if(consecutiveValidSamples==selectedSize) {
                    changed=analyze()||changed;++lastWindows;
                } else smoothingInitialized=false;
                windowClockStarted=true;samplesSinceTransform=0;
            }
        }
        // Work on this complete snapshot is bounded by capacity / hop + 1.
        // If the new producer backlog overflowed meanwhile, reset rather than
        // displaying a misleading continuous history or silently skipping work.
        if(fifo.dropped.load(std::memory_order_relaxed)!=observedDropped) {
            recoverDiscontinuity(fifo);return finishUpdate(started,true);
        }

        // Publish one coherent IN/OUT + raw/smoothed frame per consumer update,
        // rather than copying four bin arrays for every N/4-hop FFT. Failed
        // windows never alter raw/smoothed powers, so the latest good window
        // remains available even if malformed input follows in this snapshot.
        if(changed && validSpectrum)rememberCompletedFrame();

        if(lastConsumed>0) {
            lastSamplesAt=started;lastIdleDecayAt=started;idle=false;
        } else if(seconds(started-lastSamplesAt)>idleGraceSeconds) {
            changed=decayIdle(started)||changed;
        }
        return finishUpdate(started,changed);
    }

    int fftSize() const noexcept {return frozen && completed.valid?completed.size:selectedSize;}
    int hopSamples() const noexcept {return fftSize()/4;}
    int smoothingMode() const noexcept {return frozen && completed.valid?completed.smoothingMode:smoothing;}
    int windowSamples() const noexcept {return collected;}
    uint32_t lastConsumedSamples() const noexcept {return lastConsumed;}
    uint32_t lastPendingSamples() const noexcept {return lastPending;}
    uint32_t lastFFTWindows() const noexcept {return lastWindows;}
    uint64_t totalConsumedSamples() const noexcept {return totalConsumed;}
    uint64_t discardedSamples() const noexcept {return discarded;}
    uint64_t updates() const noexcept {return updateCount;}
    double lastConsumerIntervalMs() const noexcept {return lastConsumerInterval;}
    uint64_t samplesSinceLastFFT() const noexcept {return samplesSinceTransform;}
    uint64_t lastAnalysedSamples() const noexcept {return analysedSampleSpan;}
    double lastSampleRate() const noexcept {return frozen && completed.valid?completed.rate:sampleRate;}
    uint64_t frames() const noexcept {return analysedFrames;}
    double fftMicros() const noexcept {return lastTransformMicros;}
    double updateMicros() const noexcept {return lastUpdateMicros;}
    bool hasData() const noexcept {return frozen?completed.valid:validSpectrum;}
    bool isIdle() const noexcept {return idle;}
    uint64_t droppedFrames() const noexcept {return observedDropped;}
    uint64_t discontinuities() const noexcept {return discontinuityCount;}
    double smoothingAttackSeconds() const noexcept {return attackSeconds[(size_t)smoothing];}
    double smoothingReleaseSeconds() const noexcept {return releaseSeconds[(size_t)smoothing];}

    float rawDB(bool output,int bin) const noexcept {
        const auto& powers=frozen?completed.raw:rawPower;
        return hasData()?powerDB(powers[output?1:0][(size_t)std::clamp(bin,0,fftSize()/2)]):floorDB;
    }
    float smoothedDB(bool output,int bin) const noexcept {
        const auto& powers=frozen?completed.smooth:smoothPower;
        return hasData()?powerDB(powers[output?1:0][(size_t)std::clamp(bin,0,fftSize()/2)]):floorDB;
    }

    // low/high are this pixel's logarithmic frequency edges. Bin peaks contained
    // in that interval survive downsampling. At finer screen spacing, interpolate
    // measured bin power instead of repeating nearest-bin staircase segments.
    float displayDB(bool output,double lowHz,double centreHz,double highHz) const noexcept {
        const double displayRate=lastSampleRate();const int displaySize=fftSize();
        if(!hasData() || displayRate<=0 || !std::isfinite(lowHz) ||
           !std::isfinite(centreHz) || !std::isfinite(highHz))return floorDB;
        if(lowHz>highHz)std::swap(lowHz,highHz);
        const double nyquist=displayRate*.5;
        if(lowHz>nyquist || highHz<0)return floorDB;
        const double scale=displaySize/displayRate;
        const double lo=std::clamp(lowHz,0.,nyquist)*scale;
        const double hi=std::clamp(highHz,0.,nyquist)*scale;
        const double centre=std::clamp(centreHz,0.,nyquist)*scale;
        const auto& powers=(frozen?completed.smooth:smoothPower)[output?1:0];
        float peak=std::max(interpolatedPower(powers,centre),
                            std::max(interpolatedPower(powers,lo),interpolatedPower(powers,hi)));
        const int first=std::max(0,int(std::ceil(lo))),last=std::min(displaySize/2,int(std::floor(hi)));
        for(int bin=first;bin<=last;++bin)peak=std::max(peak,powers[(size_t)bin]);
        return powerDB(peak);
    }

private:
    using Clock=std::chrono::steady_clock;
    using PowerBins=std::array<float,maximumBins>;
    struct CompletedFrame {
        std::array<PowerBins,2> raw{},smooth{};
        double rate{};int size{4096},smoothingMode{1};bool valid{};
    };
    static constexpr std::array<double,3> attackSeconds{.008,.035,.09};
    static constexpr std::array<double,3> releaseSeconds{.12,.30,.65};

    static float finite(float value) noexcept {return std::isfinite(value)?value:0.f;}
    static double seconds(Clock::duration duration) noexcept {
        return std::chrono::duration<double>(duration).count();
    }
    static float powerDB(float power) noexcept {
        return std::isfinite(power)?10.f*std::log10(std::max(floorPower,power)):floorDB;
    }
    bool finishUpdate(Clock::time_point started,bool changed) noexcept {
        lastUpdateMicros=std::chrono::duration<double,std::micro>(Clock::now()-started).count();return changed;
    }
    void makeWindow() noexcept {
        double windowSum=0;
        for(int n=0;n<selectedSize;++n) {
            const double value=.5-.5*std::cos(2*juce::MathConstants<double>::pi*n/selectedSize);
            window[(size_t)n]=(float)value;windowSum+=value;
        }
        // Interior one-sided amplitude is 2 / sum(window). Average stereo
        // powers directly, without a magnitude square root followed by square.
        interiorPowerGain=2./(windowSum*windowSum);
    }
    void clearHistory() noexcept {
        writePosition=collected=0;samplesSinceTransform=analysedSampleSpan=0;
        consecutiveValidSamples=0;validSpectrum=smoothingInitialized=windowClockStarted=idle=false;
        for(auto& p:rawPower)p.fill(floorPower);
        for(auto& p:smoothPower)p.fill(floorPower);
    }
    void resetSignalHistory(EQAnalyzerFIFO& fifo) noexcept {
        // Read the drop generation before flushing. A producer drop occurring
        // during/after that flush must still force recovery on the next update.
        // This only clears signal history: recovery must retain this update's
        // actual dequeue count and FFT cost in the diagnostics.
        observedDropped=fifo.dropped.load(std::memory_order_relaxed);discarded+=fifo.discardPending();
        clearHistory();pendingReset=false;lastSamplesAt=Clock::now();lastIdleDecayAt=lastSamplesAt;
    }
    void recoverDiscontinuity(EQAnalyzerFIFO& fifo) noexcept {
        ++discontinuityCount;resetSignalHistory(fifo);
    }
    juce::dsp::FFT& transform() noexcept {
        return selectedSize==2048?fft2048:selectedSize==8192?fft8192:fft4096;
    }
    bool analyze() noexcept {
        juce::ScopedNoDenormals noDenormals;
        const int bins=selectedSize/2+1;
        bool validFrame=true;
        for(auto& p:candidatePower)std::fill_n(p.begin(),bins,0.f);
        for(int stream=0;stream<4;++stream) {
            // This chronological N/4-hop endpoint ends at writePosition.
            int cursor=writePosition;
            for(int n=0;n<selectedSize;++n) {
                work[(size_t)n]=history[(size_t)stream][(size_t)cursor]*window[(size_t)n];
                cursor=(cursor+1)&(selectedSize-1);
            }
            const auto fftStarted=Clock::now();
            // The real FFT reads only the first N samples and returns complex
            // bins. Its frequency-only wrapper would take N/2+1 square roots
            // and clear the unused tail, then we would immediately square it.
            transform().performRealOnlyForwardTransform(work.data(),true);
            lastTransformMicros+=std::chrono::duration<double,std::micro>(Clock::now()-fftStarted).count();
            auto& power=candidatePower[(size_t)(stream%2)];
            for(int bin=0;bin<bins;++bin) {
                // Reject the complete observation if any stream/bin is corrupt.
                // A partially sanitized frame is never a valid Freeze source.
                const double real=work[(size_t)(2*bin)],imaginary=work[(size_t)(2*bin+1)];
                if(!std::isfinite(real) || !std::isfinite(imaginary)) {validFrame=false;continue;}
                const double magnitudeSquared=real*real+imaginary*imaginary;
                constexpr double maximumMagnitude=std::numeric_limits<float>::max();
                if(magnitudeSquared>maximumMagnitude*maximumMagnitude) {validFrame=false;continue;}
                // DC/Nyquist have no conjugate partner. Interior sinusoidal
                // amplitudes receive the one-sided factor of two.
                const double gain=(bin==0 || bin==selectedSize/2)?interiorPowerGain*.25:interiorPowerGain;
                const double value=magnitudeSquared*gain;
                power[(size_t)bin]+=float(std::min(value,double(std::numeric_limits<float>::max())*.25));
            }
        }
        if(!validFrame) {smoothingInitialized=false;return false;}
        analysedSampleSpan=samplesSinceTransform;
        const double elapsed=double(samplesSinceTransform)/sampleRate;
        const float attack=(float)std::exp(-elapsed/smoothingAttackSeconds());
        const float release=(float)std::exp(-elapsed/smoothingReleaseSeconds());
        for(int stream=0;stream<2;++stream)for(int bin=0;bin<bins;++bin) {
            auto& raw=rawPower[(size_t)stream][(size_t)bin];
            raw=std::max(floorPower,candidatePower[(size_t)stream][(size_t)bin]);
            auto& smooth=smoothPower[(size_t)stream][(size_t)bin];
            if(!smoothingInitialized)smooth=raw;
            else {const float coefficient=raw>smooth?attack:release;smooth=raw+coefficient*(smooth-raw);}
        }
        validSpectrum=smoothingInitialized=true;++analysedFrames;return true;
    }
    void rememberCompletedFrame() noexcept {
        const int bins=selectedSize/2+1;
        for(size_t stream=0;stream<2;++stream) {
            std::copy_n(rawPower[stream].begin(),bins,completed.raw[stream].begin());
            std::copy_n(smoothPower[stream].begin(),bins,completed.smooth[stream].begin());
        }
        completed.rate=sampleRate;completed.size=selectedSize;completed.smoothingMode=smoothing;completed.valid=true;
    }
    bool decayIdle(Clock::time_point now) noexcept {
        if(!idle) {
            // No future FFT may splice pre-stop history with resumed audio.
            collected=writePosition=consecutiveValidSamples=0;samplesSinceTransform=0;
            smoothingInitialized=windowClockStarted=false;idle=true;
            for(auto& p:rawPower)p.fill(floorPower);
            lastIdleDecayAt=lastSamplesAt+std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(idleGraceSeconds));
        }
        if(!validSpectrum) {lastIdleDecayAt=now;return false;}
        const double elapsed=std::max(0.,seconds(now-lastIdleDecayAt));lastIdleDecayAt=now;
        const float coefficient=(float)std::exp(-elapsed/smoothingReleaseSeconds());
        float maximum=floorPower;
        for(auto& p:smoothPower)for(int bin=0;bin<=selectedSize/2;++bin) {
            auto& power=p[(size_t)bin];power=floorPower+coefficient*(power-floorPower);maximum=std::max(maximum,power);
        }
        if(maximum<=floorPower*1.01f) {
            validSpectrum=false;for(auto& p:smoothPower)p.fill(floorPower);
        }
        return true;
    }
    float interpolatedPower(const PowerBins& power,double position) const noexcept {
        const int left=std::clamp(int(std::floor(position)),0,fftSize()/2);
        const int right=std::min(fftSize()/2,left+1);
        const float fraction=(float)std::clamp(position-left,0.,1.);
        return power[(size_t)left]+fraction*(power[(size_t)right]-power[(size_t)left]);
    }

    juce::dsp::FFT fft2048{11},fft4096{12},fft8192{13};
    std::array<std::array<float,maximumFFTSize>,4> history{};
    std::array<EQAnalyzerFrame,EQAnalyzerFIFO::capacity> staged{};
    std::array<float,maximumFFTSize> window{};
    std::array<float,2*maximumFFTSize> work{};
    std::array<PowerBins,2> rawPower{},smoothPower{},candidatePower{};
    CompletedFrame completed;
    int selectedSize{4096},smoothing{1},writePosition{},collected{},consecutiveValidSamples{};
    uint32_t lastConsumed{},lastPending{},lastWindows{};
    uint64_t analysedFrames{},samplesSinceTransform{},analysedSampleSpan{},observedDropped{},discontinuityCount{};
    uint64_t totalConsumed{},discarded{},updateCount{};
    double sampleRate{},interiorPowerGain{},lastTransformMicros{},lastUpdateMicros{};
    double lastConsumerInterval{};
    bool validSpectrum{},smoothingInitialized{},pendingReset{},idle{},previousUpdateValid{},frozen{},windowClockStarted{};
    Clock::time_point lastSamplesAt{Clock::now()},lastIdleDecayAt{lastSamplesAt};
    Clock::time_point previousUpdate{};
};
}
