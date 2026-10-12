#include "EQSpectrumAnalyzer.h"
#include "PluginProcessor.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

// Only count allocations made by the current audio callback thread. FFT plans,
// buffers, timing vectors, and the consumer are constructed before this scope.
static thread_local bool countAllocations=false;
static thread_local size_t allocations=0;
void* operator new(std::size_t n) {if(countAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

using namespace spectralforge;
namespace {
using Clock=std::chrono::steady_clock;
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
void near(double actual,double expected,double tolerance,const char* message) {
    if(!std::isfinite(actual) || !std::isfinite(expected) || std::abs(actual-expected)>tolerance) {
        std::cerr<<message<<": actual="<<actual<<" expected="<<expected<<" tolerance="<<tolerance<<'\n';
        throw std::runtime_error(message);
    }
}

void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* parameter=p.parameters().getParameter(id);check(parameter!=nullptr,"Missing host parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
double percentile(std::vector<double> values,double fraction) {
    check(!values.empty(),"No timing observations");std::sort(values.begin(),values.end());
    return values[std::min(values.size()-1,size_t(fraction*double(values.size()-1)))];
}
void fillFIFO(EQAnalyzerFIFO& fifo,int samples,int offset,int fftSize,int bin,
              double amplitude=.1,double outputGain=1.,double rightGain=-1.) {
    for(int n=0;n<samples;++n) {
        const float input=float(amplitude*std::sin(2*juce::MathConstants<double>::pi*bin*(offset+n)/fftSize));
        fifo.push({input,float(input*outputGain),float(input*rightGain),float(input*outputGain*rightGain)});
    }
}
void configure(EQSpectrumAnalyzer& analyzer,EQAnalyzerFIFO& fifo,int size,double rate=48000) {
    analyzer.setFFTSize(size);analyzer.reset(fifo);analyzer.update(fifo,rate);
}

void lowFrequencyAndFiniteInput() {
    for(double rate:{48000.,96000.,192000.})for(int size:{2048,4096,8192}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size,rate);
        for(double hz:{31.,55.,63.,251.,997.,8011.}) {
            analyzer->reset(*fifo);
            for(int n=0;n<size;++n){const float x=float(.1*std::sin(2*juce::MathConstants<double>::pi*hz*n/rate));fifo->push({x,x,-x,-x});}
            analyzer->update(*fifo,rate);int peak=1;
            for(int bin=2;bin<size/2;++bin)if(analyzer->rawDB(false,bin)>analyzer->rawDB(false,peak))peak=bin;
            // One measured bin is the explicit tolerance. Display interpolation
            // cannot promise a finer low-frequency resolution than the FFT.
            // A sub-bin bass fundamental is unresolved, so its phase-dependent
            // partial cycle must not be asserted to have coherent sine gain.
            near(rate*peak/size,hz,rate/size,"Off-bin/low-frequency peak outside the measured resolution");
            if(hz>=2*rate/size)
                check(analyzer->rawDB(false,peak)>-24 && analyzer->rawDB(false,peak)<-17,"Resolved off-bin Hann peak calibration implausible");
            else check(analyzer->rawDB(false,peak)>-60 && analyzer->rawDB(false,peak)<-10,"Unresolved low-frequency observation was empty or implausible");
        }
        analyzer->reset(*fifo);uint32_t noise=0x4251567u;
        for(int n=0;n<size;++n){noise^=noise<<13;noise^=noise>>17;noise^=noise<<5;const float x=(float(noise)/float(UINT32_MAX)-.5f)*.1f;fifo->push({x,x,-x,-x});}
        analyzer->update(*fifo,rate);float bandPeak=-120;
        for(int bin=int(std::ceil(100.*size/rate));bin<=int(std::floor(1000.*size/rate));++bin)bandPeak=std::max(bandPeak,analyzer->rawDB(false,bin));
        check(analyzer->displayDB(false,100,500,1000)>=bandPeak-.01f,"Broadband screen-pixel aggregation discarded its strongest measured bin");
        analyzer->reset(*fifo);
        const std::array<float,4> invalid{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::max()};
        for(int n=0;n<size;++n){const auto x=invalid[size_t(n%4)];fifo->push({x,x,-x,-x});}analyzer->update(*fifo,rate);
        for(int bin=0;bin<=size/2;++bin)for(bool output:{false,true}) {
            check(std::isfinite(analyzer->rawDB(output,bin)) && std::isfinite(analyzer->smoothedDB(output,bin)),"Malformed input produced a nonfinite spectrum");
            const double hz=rate*bin/size;check(std::isfinite(analyzer->displayDB(output,hz*.99,hz,hz*1.01)),"Malformed input produced a nonfinite display path");
        }
        fillFIFO(*fifo,size,0,size,43);analyzer->update(*fifo,rate);near(analyzer->rawDB(false,43),-20,.02,"Valid tone did not recover after malformed input");
    }
    std::cout<<"PASS 48/96/192 kHz: 31/55 Hz and stepped frequency peaks at honest bin tolerance / deterministic broadband / nonfinite input recovery\n";
}
void calibrationAndContinuity() {
    for(double rate:{48000.,96000.,192000.})for(int size:{2048,4096,8192}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size,rate);const int hop=size/4;
        constexpr int bin=43;const double gain=std::pow(10.,6./20.);
        fillFIFO(*fifo,size-1,0,size,bin,.1,gain);
        analyzer->update(*fifo,rate);
        check(!analyzer->hasData() && analyzer->windowSamples()==size-1 && analyzer->frames()==0,
              "Spectrum was fabricated before a complete real observation window");
        fillFIFO(*fifo,1,size-1,size,bin,.1,gain);analyzer->update(*fifo,rate);
        check(analyzer->hasData() && analyzer->frames()==1,"First complete window was not transformed");
        near(analyzer->rawDB(false,bin),-20,.02,"Opposite-polarity input calibration");
        near(analyzer->rawDB(true,bin),-14,.02,"Actual output calibration");
        const double hz=rate*bin/size;
        near(analyzer->displayDB(false,hz*.999,hz,hz*1.001),-20,.02,"Log pixel lost a measured peak");
        near(analyzer->displayDB(false,rate*.5+1000,rate*.5+2000,rate*.5+3000),-120,0,"Above-Nyquist pixels are not empty");

        // The same full-scale sine in a single channel measures 3 dB below the
        // equal-power stereo calibration; neither polarity nor channel count is
        // allowed to silently change the calibration definition.
        analyzer->reset(*fifo);fillFIFO(*fifo,size,0,size,bin,1,1,0);analyzer->update(*fifo,rate);
        near(analyzer->rawDB(false,bin),-3.0102999566,.02,"Single-channel power calibration");
        analyzer->reset(*fifo);fillFIFO(*fifo,size,0,size,bin,1,1,1);analyzer->update(*fifo,rate);
        near(analyzer->rawDB(false,bin),0,.02,"Coherent full-scale sine calibration");

        // DC and Nyquist are single-sided bins and have no factor-of-two gain.
        analyzer->reset(*fifo);for(int n=0;n<size;++n)fifo->push({.1f,.1f,-.1f,-.1f});analyzer->update(*fifo,rate);
        near(analyzer->rawDB(false,0),-20,.02,"DC endpoint calibration");
        analyzer->reset(*fifo);for(int n=0;n<size;++n){const float x=n%2?.1f:-.1f;fifo->push({x,x,-x,-x});}analyzer->update(*fifo,rate);
        near(analyzer->rawDB(false,size/2),-20,.02,"Nyquist endpoint calibration");

        analyzer->reset(*fifo);fillFIFO(*fifo,size,0,size,bin);analyzer->update(*fifo,rate);
        const auto before=analyzer->frames();fillFIFO(*fifo,size,0,size,bin+31);analyzer->update(*fifo,rate);
        check(analyzer->frames()==before+uint64_t(size/hop),"Consumer omitted complete overlapping observation windows");
        near(analyzer->rawDB(false,bin+31),-20,.02,"Changed frequency did not move the actual peak");
        check(analyzer->rawDB(false,bin)<-100,"Old frequency survived a complete replacement window");
        check(analyzer->smoothedDB(false,bin)>analyzer->rawDB(false,bin),"Release smoothing is not active");
        const auto stableFrames=analyzer->frames();analyzer->update(*fifo,rate);
        check(analyzer->frames()==stableFrames && analyzer->fftMicros()==0,"Empty consumer repeated an FFT");
        fillFIFO(*fifo,hop-1,0,size,bin+31);analyzer->update(*fifo,rate);
        check(analyzer->frames()==stableFrames && analyzer->samplesSinceLastFFT()==uint64_t(hop-1),
              "Incomplete hop triggered a duplicate or timer-dependent FFT");
        fillFIFO(*fifo,1,hop-1,size,bin+31);analyzer->update(*fifo,rate);
        check(analyzer->frames()==stableFrames+1 && analyzer->lastAnalysedSamples()==uint64_t(hop),
              "Complete hop did not advance the sample-clocked FFT exactly once");

        // An overflow is an explicit discontinuity, not time-compressed old
        // samples. Recovery needs a fresh complete window before rendering.
        const auto discontinuities=analyzer->discontinuities(),dropped=fifo->dropped.load();
        fillFIFO(*fifo,int(EQAnalyzerFIFO::capacity)+7,0,size,bin);
        check(fifo->pendingSamples()==EQAnalyzerFIFO::capacity && fifo->dropped.load()==dropped+7,
              "FIFO overflow fixture did not isolate exact finite-capacity loss");
        analyzer->update(*fifo,rate);
        check(analyzer->discontinuities()==discontinuities+1 && !analyzer->hasData() && fifo->pendingSamples()==0,
              "Overflow retained a discontinuous spectrum or queued catch-up work");
        check(analyzer->lastConsumedSamples()<=EQAnalyzerFIFO::capacity,"Unbounded FIFO drain");
        fillFIFO(*fifo,size,0,size,bin);analyzer->update(*fifo,rate);
        check(analyzer->hasData(),"Recovery could not accept a new observation");

        // A rate transition invalidates existing sample timing and queued data.
        const double newRate=rate==192000?96000.:192000.;
        analyzer->update(*fifo,newRate);check(!analyzer->hasData(),"Rate transition retained old spectrum");
        fillFIFO(*fifo,size,0,size,bin);analyzer->update(*fifo,newRate);
        near(analyzer->rawDB(false,bin),-20,.02,"New sample-rate observation calibration");
        check(analyzer->lastSampleRate()==newRate,"New sample rate not recorded");
        std::cout<<"PASS FFT "<<size<<" @ "<<rate<<" Hz bin="<<rate/size<<" Hz window="<<1000.*size/rate
                 <<" ms / antiphase, amplitude, endpoints, frequency change, overflow, rate transition\n";
    }
}

void smoothingAndStop() {
    constexpr int size=4096,bin=43,hop=size/4;
    for(double rate:{48000.,96000.,192000.}) {
      std::array<double,3> after{};
      for(int mode=0;mode<3;++mode) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size,rate);analyzer->setSmoothing(mode);
        fillFIFO(*fifo,size,0,size,bin);analyzer->update(*fifo,rate);
        // The first four overlapping windows contain progressively less tone.
        // Check exact release only after the history is entirely silent, using
        // the measured preceding envelope as the initial condition.
        for(int n=0;n<size;++n)fifo->push({});
        analyzer->update(*fifo,rate);
        const double previousPower=std::pow(10.,analyzer->smoothedDB(false,bin)/10.);
        for(int n=0;n<hop;++n)fifo->push({});
        analyzer->update(*fifo,rate);
        const double expected=10.*std::log10(EQSpectrumAnalyzer::floorPower+
            (previousPower-EQSpectrumAnalyzer::floorPower)*std::exp(-double(hop)/rate/analyzer->smoothingReleaseSeconds()));
        after[(size_t)mode]=analyzer->smoothedDB(false,bin);
        near(after[(size_t)mode],expected,.02,"Power-domain release does not follow elapsed sample time");
        check(analyzer->rawDB(false,bin)<=-119.9f,"Silent audio was not analyzed as silence");
        const auto frames=analyzer->frames();const auto displayed=analyzer->smoothedDB(false,bin);
        analyzer->setSmoothing((mode+1)%3);
        check(analyzer->frames()==frames && analyzer->smoothedDB(false,bin)==displayed && analyzer->hasData(),
              "Changing smoothing needlessly discarded the live observation");
      }
      check(after[0]<after[1] && after[1]<after[2],"Fast/Balanced/Smooth release ordering");
    }
    auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
    configure(*analyzer,*fifo,size);fillFIFO(*fifo,size,0,size,bin);analyzer->update(*fifo,48000);
    const auto frames=analyzer->frames();const float displayed=analyzer->smoothedDB(false,bin);
    std::this_thread::sleep_for(std::chrono::milliseconds(180));analyzer->update(*fifo,48000);
    check(analyzer->isIdle() && analyzer->windowSamples()==0 && analyzer->frames()==frames,
          "Stopped source did not clear history without new transforms");
    check(analyzer->rawDB(false,bin)<=-119.9f && analyzer->smoothedDB(false,bin)<displayed,
          "Stopped source leaves a frozen live spectrum");
    fillFIFO(*fifo,size-1,0,size,bin+20);analyzer->update(*fifo,48000);
    check(analyzer->frames()==frames,"Resumed source spliced in pre-stop samples");
    fillFIFO(*fifo,1,size-1,size,bin+20);analyzer->update(*fifo,48000);
    near(analyzer->rawDB(false,bin+20),-20,.02,"Resumed source did not replace the old frequency");
    std::cout<<"PASS sample-time smoothing / no reset on ballistics changes / idle decay / fresh resumed window\n";
}

// Raw bins describe the LAST real FFT. The visible envelope must retain a
// transient observed by earlier windows within a single UI consumer batch.
// The optional probe writes old/new measurements without imposing that gate;
// the normal test and --temporal-coverage-regression enforce actual detection.
void temporalCoverageDiagnostic(const juce::File& directory,bool enforceDetection) {
    std::ofstream out(directory.getChildFile("eq-analyzer-temporal-coverage.csv").getFullPathName().toStdString());
    out<<"rate,fft_size,ui_interval_ms,ui_batch_samples,analysis_hop_samples,window_ms,legacy_unobserved_prefix_samples,burst_position,burst_start,burst_samples,input_energy,raw_last_peak_db_at_40ms,smoothed_peak_db_at_40ms,display_peak_db_at_40ms,fft_windows_at_40ms,extra_observation_samples,bounded_display_peak_db,fifo_dropped_samples\n";
    bool correctCadence=true;
    for(double rate:{48000.,96000.,192000.})for(int size:{2048,4096,8192})for(bool late:{false,true}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size,rate);
        // Start from a complete, silent observation; this is a steady-state
        // probe and does not confuse initial window acquisition with coverage.
        for(int n=0;n<size;++n)fifo->push({});
        analyzer->update(*fifo,rate);
        check(analyzer->hasData(),"Temporal coverage probe did not acquire a complete initial window");
        const auto beforeFrames=analyzer->frames(),beforeDropped=fifo->dropped.load();
        const int batch=int(rate/25),analysisHop=size/4,burstSamples=384;
        const int burstStart=late?batch-std::min(size,batch)/2-burstSamples/2:64;
        double energy=0;
        for(int n=0;n<batch;++n) {
            const bool inBurst=n>=burstStart && n<burstStart+burstSamples;
            const float x=inBurst?float(.4*std::sin(2*juce::MathConstants<double>::pi*997*(n-burstStart)/rate)):0.f;
            energy+=double(x)*x;fifo->push({x,x,-x,-x});
        }
        analyzer->update(*fifo,rate);
        float rawPeak=EQSpectrumAnalyzer::floorDB,smoothPeak=EQSpectrumAnalyzer::floorDB;
        for(int bin=1;bin<=size/2;++bin) {
            rawPeak=std::max(rawPeak,analyzer->rawDB(false,bin));
            smoothPeak=std::max(smoothPeak,analyzer->smoothedDB(false,bin));
        }
        const auto dropped=fifo->dropped.load()-beforeDropped,windows=analyzer->frames()-beforeFrames;
        check(energy>1.e-6 && dropped==0 && fifo->pendingSamples()==0,
              "Temporal coverage fixture was silent, dropped FIFO data or left pending samples");
        const float displayAt40=analyzer->displayDB(false,100,997,4000);
        // At large N the next scheduled endpoint may be after the paint, or a
        // late burst may still be near a Hann edge. Give it a bounded N/2+hop
        // observation tail; do not invent a FFT at an arbitrary paint boundary.
        const int extra=size/2+analysisHop;
        for(int n=0;n<extra;++n)fifo->push({});
        analyzer->update(*fifo,rate);
        const float boundedDisplay=std::max(displayAt40,analyzer->displayDB(false,100,997,4000));
        check(fifo->dropped.load()==beforeDropped,"Temporal coverage tail overflowed its FIFO");
        out<<rate<<','<<size<<",40,"<<batch<<','<<analysisHop<<','<<1000.*size/rate<<','<<std::max(0,batch-size)<<','
           <<(late?"late":"early")<<','<<burstStart<<','<<burstSamples<<','<<energy<<','<<rawPeak<<','<<smoothPeak<<','<<displayAt40<<','<<windows<<','<<extra<<','<<boundedDisplay<<','<<dropped<<'\n';
        std::cout<<"COVERAGE rate="<<rate<<" FFT="<<size<<" position="<<(late?"late":"early")
                 <<" batch="<<batch<<" samples windows="<<windows<<" input_energy="<<energy
                 <<" raw_last="<<rawPeak<<" display_at_40ms="<<displayAt40<<" bounded_display="<<boundedDisplay
                 <<" dBFS fifo_drops="<<dropped<<'\n';
        if(enforceDetection) {
            correctCadence=correctCadence && windows==uint64_t(batch/analysisHop);
            check(boundedDisplay>-60.f,"Short input attack disappeared from the actual displayed spectrum");
            if(!late && batch>size)
                check(displayAt40>-60.f,"Early attack was overwritten before the 40 ms display update without FIFO loss");
        }
    }
    if(enforceDetection)check(correctCadence,"Temporal consumer skipped a complete analysis hop");
    check(out.good(),"Temporal coverage diagnostic could not be written");
}

void partitionInvariantCoverage(const juce::File& directory) {
    std::ofstream out(directory.getChildFile("eq-analyzer-partition-invariance.csv").getFullPathName().toStdString());
    out<<"rate,fft_size,smoothing,burst_start,burst_samples,checked_ui_boundaries,fft_windows,max_raw_error_db,max_envelope_error_db,max_pixel_error_db,max_display_db,fifo_dropped_samples\n";
    for(double rate:{96000.,192000.})for(int size:{2048,4096,8192})for(int mode=0;mode<3;++mode) {
        const int hop=size/4,batch=int(rate/25),burstSamples=384;
        // Both sides of a hop and a burst spanning the nominal UI boundary.
        for(int burstStart:{64,hop-1,hop+1,batch-192,batch+1}) {
            auto batched=std::make_unique<EQSpectrumAnalyzer>(),reference=std::make_unique<EQSpectrumAnalyzer>(),irregular=std::make_unique<EQSpectrumAnalyzer>();
            auto batchedFIFO=std::make_unique<EQAnalyzerFIFO>(),referenceFIFO=std::make_unique<EQAnalyzerFIFO>(),irregularFIFO=std::make_unique<EQAnalyzerFIFO>();
            for(auto pair:{std::make_pair(batched.get(),batchedFIFO.get()),std::make_pair(reference.get(),referenceFIFO.get()),std::make_pair(irregular.get(),irregularFIFO.get())}) {
                configure(*pair.first,*pair.second,size,rate);pair.first->setSmoothing(mode);
                for(int n=0;n<size;++n)pair.second->push({});
                pair.first->update(*pair.second,rate);
            }
            const auto initialFrames=batched->frames();double rawError=0,smoothError=0,pixelError=0;
            float displayedPeak=EQSpectrumAnalyzer::floorDB;
            const int totalSamples=2*batch+size,intervals=(totalSamples+batch-1)/batch;
            for(int start=0;start<totalSamples;start+=batch) {
                const int end=std::min(totalSamples,start+batch);
                for(int n=start;n<end;++n) {
                    const bool inBurst=n>=burstStart && n<burstStart+burstSamples;
                    const float x=inBurst?float(.4*std::sin(2*juce::MathConstants<double>::pi*997*(n-burstStart)/rate)):0.f;
                    const EQAnalyzerFrame frame{x,x*.5f,-x,-x*.5f};
                    batchedFIFO->push(frame);referenceFIFO->push(frame);irregularFIFO->push(frame);
                    if((n+1)%hop==0)reference->update(*referenceFIFO,rate);
                    // Deliberate cuts inside hops and across the burst. Neither
                    // extra UI callbacks nor a partial FIFO drain may reset hop
                    // phase, rerun a window, or change envelope sample timing.
                    if((n+17)%(hop/2+23)==0)irregular->update(*irregularFIFO,rate);
                }
                batched->update(*batchedFIFO,rate);
                if(referenceFIFO->pendingSamples()>0)reference->update(*referenceFIFO,rate);
                if(irregularFIFO->pendingSamples()>0)irregular->update(*irregularFIFO,rate);
                check(batched->frames()==initialFrames+uint64_t(end/hop) &&
                      reference->frames()==batched->frames() && irregular->frames()==batched->frames(),
                      "UI batch boundaries changed the number of real analysis windows");
                for(int bin=0;bin<=size/2;++bin)for(bool output:{false,true})for(const auto* other:{reference.get(),irregular.get()}) {
                    rawError=std::max(rawError,double(std::abs(batched->rawDB(output,bin)-other->rawDB(output,bin))));
                    smoothError=std::max(smoothError,double(std::abs(batched->smoothedDB(output,bin)-other->smoothedDB(output,bin))));
                }
                for(double hz:{31.,55.,251.,997.,4000.,8000.})for(bool output:{false,true})for(const auto* other:{reference.get(),irregular.get()})
                    pixelError=std::max(pixelError,double(std::abs(batched->displayDB(output,hz*.9,hz,hz*1.1)-other->displayDB(output,hz*.9,hz,hz*1.1))));
                displayedPeak=std::max(displayedPeak,batched->displayDB(false,100,997,4000));
                check(batchedFIFO->pendingSamples()==0 && referenceFIFO->pendingSamples()==0 && irregularFIFO->pendingSamples()==0,
                      "Partition coverage fixture did not consume its complete input");
                check(batchedFIFO->dropped.load()==0 && referenceFIFO->dropped.load()==0 && irregularFIFO->dropped.load()==0,
                      "Partition coverage fixture mixed queue overflow with temporal coverage");
            }
            near(rawError,0,.002,"Raw observations depend on UI update partitioning");
            near(smoothError,0,.002,"Attack/release envelope depends on UI update partitioning");
            near(pixelError,0,.002,"Actual displayed spectrum depends on UI update partitioning");
            check(displayedPeak>-60.f,"Boundary-position attack was absent from every displayed observation");
            out<<rate<<','<<size<<','<<mode<<','<<burstStart<<','<<burstSamples<<','<<intervals<<','<<batched->frames()-initialFrames<<','
               <<rawError<<','<<smoothError<<','<<pixelError<<','<<displayedPeak<<",0\n";
        }
    }
    check(out.good(),"Partition invariance evidence could not be written");
    std::cout<<"PASS 90 transient/boundary/ballistics cases: 96/192 kHz, all FFT sizes, per-hop/25 Hz/irregular consumers agree\n";
}

void fifoStallsAndBoundedCatchup(const juce::File& directory) {
    std::ofstream out(directory.getChildFile("eq-analyzer-fifo-stalls.csv").getFullPathName().toStdString());
    out<<"rate,fft_size,fixture,simulated_stall_ms,capacity_samples,produced_samples,consumed_samples,discarded_samples,dropped_samples,pending_samples,fft_windows,update_us,fft_us,recovery_complete\n";
    for(double rate:{96000.,192000.})for(int size:{2048,4096,8192})for(int stall:{100,250,500,0}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size,rate);fillFIFO(*fifo,size,0,size,43);analyzer->update(*fifo,rate);
        const auto consumedBefore=analyzer->totalConsumedSamples(),discardedBefore=analyzer->discardedSamples();
        const auto droppedBefore=fifo->dropped.load(),framesBefore=analyzer->frames(),discontinuitiesBefore=analyzer->discontinuities();
        const int produced=stall>0?int(rate*stall/1000.):int(EQAnalyzerFIFO::capacity);
        const uint64_t expectedDropped=uint64_t(std::max(0,produced-int(EQAnalyzerFIFO::capacity)));
        fillFIFO(*fifo,produced,size,size,43);analyzer->update(*fifo,rate);
        const auto consumed=analyzer->totalConsumedSamples()-consumedBefore,discarded=analyzer->discardedSamples()-discardedBefore;
        const auto dropped=fifo->dropped.load()-droppedBefore,windows=analyzer->frames()-framesBefore;
        const auto pending=fifo->pendingSamples();
        const double elapsed=analyzer->updateMicros(),fftTime=analyzer->fftMicros();
        check(dropped==expectedDropped && uint64_t(produced)==consumed+discarded+dropped+pending,
              "Finite FIFO stall lost samples outside consumed/discarded/dropped/pending accounting");
        check(pending==0 && windows<=EQAnalyzerFIFO::capacity/uint32_t(size/4)+1,
              "Stall recovery retained queued catch-up work or exceeded its bounded FFT budget");
        if(expectedDropped==0) {
            check(consumed==uint64_t(produced) && discarded==0 && windows==uint64_t(produced/(size/4)),
                  "A stall within FIFO capacity omitted complete observation windows");
            check(analyzer->hasData() && analyzer->discontinuities()==discontinuitiesBefore,
                  "A lossless consumer stall was falsely treated as a discontinuity");
        } else {
            check(consumed==0 && discarded==EQAnalyzerFIFO::capacity && windows==0 && !analyzer->hasData() &&
                  analyzer->discontinuities()==discontinuitiesBefore+1,
                  "Known FIFO loss was joined into a misleading continuous spectrum");
            fillFIFO(*fifo,size,0,size,43);analyzer->update(*fifo,rate);
            check(analyzer->hasData(),"FIFO overflow recovery failed to acquire a fresh complete window");
            near(analyzer->rawDB(false,43),-20,.02,"FIFO recovery changed coherent amplitude calibration");
        }
        out<<rate<<','<<size<<','<<(stall>0?"simulated_stall":"full_capacity")<<','<<(stall>0?double(stall):1000.*produced/rate)<<','
           <<EQAnalyzerFIFO::capacity<<','<<produced<<','<<consumed<<','<<discarded<<','<<dropped<<','<<pending<<','<<windows<<','<<elapsed<<','<<fftTime<<",1\n";
    }
    check(out.good(),"FIFO stall evidence could not be written");
    std::cout<<"PASS exact FIFO sample accounting: simulated 100/250/500 ms stalls, full-capacity bounded catch-up, fresh recovery\n";
}

void fftBenchmarks(const juce::File& directory) {
    std::ofstream out(directory.getChildFile("eq-analyzer-cpu.csv").getFullPathName().toStdString());
    out<<"fft_size,rate,bin_spacing_hz,hann_enbw_hz,window_ms,hop_samples,hop_ms,ui_hz,measured_update_calls,zero_fft_update_calls,fft_windows,fft_windows_min,fft_windows_max,fft_total_p50_us,fft_total_p99_us,fft_per_window_mean_us,update_p50_us,update_p99_us,update_max_us,update_mean_us,analysis_cpu_at_25hz_percent,fifo_dropped_samples,max_pending_samples,max_consumed_samples\n";
    for(double rate:{48000.,96000.,192000.})for(int size:{2048,4096,8192}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size,rate);std::vector<double> fft,update;
        const int batch=int(rate/25),hop=size/4;int offset=0,zeroWindows=0;
        uint64_t totalWindows=0,minWindows=UINT64_MAX,maxWindows=0;uint32_t maxPending=0,maxConsumed=0;
        for(int k=0;k<180;++k) {
            fillFIFO(*fifo,batch,offset,size,43);offset+=batch;const auto before=analyzer->frames();
            maxPending=std::max(maxPending,fifo->pendingSamples());
            analyzer->update(*fifo,rate);
            const auto windows=analyzer->frames()-before;
            const uint64_t expectedFrames=offset<size?0:1+uint64_t((offset-size)/hop);
            check(analyzer->frames()==expectedFrames && windows<=EQAnalyzerFIFO::capacity/uint32_t(hop)+1 &&
                  analyzer->lastConsumedSamples()<=EQAnalyzerFIFO::capacity && analyzer->lastFFTWindows()==windows,
                  "Consumer skipped a scheduled window or exceeded its bounded snapshot work");
            maxConsumed=std::max(maxConsumed,analyzer->lastConsumedSamples());
            if(k>=20) {
                // Every visible consumer callback belongs in the CPU
                // denominator, including updates that have no complete hop.
                fft.push_back(analyzer->fftMicros());update.push_back(analyzer->updateMicros());
                totalWindows+=windows;minWindows=std::min(minWindows,windows);maxWindows=std::max(maxWindows,windows);
                if(windows==0)++zeroWindows;
            }
        }
        check(update.size()==160 && totalWindows>0 && fifo->dropped.load()==0,"FFT timing fixture dropped or omitted samples/consumer observations");
        const double mean=std::accumulate(update.begin(),update.end(),0.)/double(update.size());
        const double perWindow=std::accumulate(fft.begin(),fft.end(),0.)/double(totalWindows);
        out<<size<<','<<rate<<','<<rate/size<<','<<1.5*rate/size<<','<<1000.*size/rate<<','<<hop<<','<<1000.*hop/rate<<",25,"<<update.size()<<','<<zeroWindows<<','
           <<totalWindows<<','<<minWindows<<','<<maxWindows<<','<<percentile(fft,.5)<<','<<percentile(fft,.99)<<','<<perWindow<<','<<percentile(update,.5)<<','
           <<percentile(update,.99)<<','<<*std::max_element(update.begin(),update.end())<<','<<mean<<','<<mean*25/10000<<','<<fifo->dropped.load()<<','<<maxPending<<','<<maxConsumed<<'\n';
        std::cout<<"FFT CPU "<<size<<" @ "<<rate<<" Hz windows/update="<<minWindows<<".."<<maxWindows<<" (zero="<<zeroWindows<<") update p50/p99="
                 <<percentile(update,.5)<<'/'<<percentile(update,.99)<<" us; 25 Hz CPU="<<mean*25/10000<<"%; observation="<<1000.*size/rate<<" ms\n";
    }
    check(out.good(),"Analyzer timing evidence could not be written");
}

void callbackAndLatency(const juce::File& directory) {
    auto p=std::make_unique<ChimeraProcessor>();
    for(int e=0;e<2;++e) {
        set(*p,eqID(e,-1,"bypass"),0);
        for(int b=0;b<graphicalEQBands;++b) {
            set(*p,eqID(e,b,"enabled"),1);set(*p,eqID(e,b,"type"),float(b%3));
            set(*p,eqID(e,b,"gain"),b%2?2.f:-2.f);set(*p,eqID(e,b,"q"),1);
        }
    }
    std::ofstream out(directory.getChildFile("eq-analyzer-callback.csv").getFullPathName().toStdString());
    out<<"capture,fft_size,rate,block,channels,active_eq_bands,captured_banks,callbacks,eq_pair_p50_us,eq_pair_p99_us,eq_pair_max_us,budget_us,overruns,audio_allocations\n";
    for(double rate:{48000.,96000.,192000.})for(int block:{64,128,512})for(int size:{0,2048,4096,8192}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();configure(*analyzer,p->graphicalEQ(0).analyzer,size==0?4096:size,rate);
        for(int e=0;e<2;++e){p->graphicalEQ(e).prepare(rate);p->graphicalEQ(e).analyzer.enabled.store(size>0 && e==0);}
        juce::AudioBuffer<float> audio(2,block);std::vector<double> times;times.reserve(600);int sinceUpdate=0;allocations=0;
        for(int k=0;k<640;++k) {
            for(int n=0;n<block;++n){const float x=float(.001*std::sin(2*juce::MathConstants<double>::pi*997*(k*block+n)/rate));audio.setSample(0,n,x);audio.setSample(1,n,-x);}
            const auto start=Clock::now();countAllocations=true;
            p->graphicalEQ(0).process(audio);p->graphicalEQ(1).process(audio);countAllocations=false;
            const double elapsed=std::chrono::duration<double,std::micro>(Clock::now()-start).count();
            if(k>=40)times.push_back(elapsed);
            sinceUpdate+=block;if(sinceUpdate>=int(rate/25)){if(size>0)analyzer->update(p->graphicalEQ(0).analyzer,rate);sinceUpdate=0;}
        }
        const double budget=block/rate*1.e6;const auto overruns=std::count_if(times.begin(),times.end(),[budget](double x){return x>budget;});
        check(allocations==0,"Analyzer tap allocated on the audio callback thread");
        check(p->graphicalEQ(0).analyzer.dropped.load()==0,"Callback benchmark overflowed its intended25Hz consumer");
        out<<(size>0)<<','<<size<<','<<rate<<','<<block<<",2,24,"<<(size>0)<<','<<times.size()<<','<<percentile(times,.5)<<','
           <<percentile(times,.99)<<','<<*std::max_element(times.begin(),times.end())<<','<<budget<<','<<overruns<<','<<allocations<<'\n';
    }
    check(out.good(),"Callback evidence could not be written");

    // Whole-processor comparison uses two identically configured live routes.
    // Passive capture on both banks, visibility-style toggles, a resolution
    // transition, impulses, and a deliberately full FIFO must never alter one
    // output sample or add to the plugin's existing reported host latency.
    std::ofstream invariance(directory.getChildFile("eq-analyzer-audio-invariance.csv").getFullPathName().toStdString());
    invariance<<"rate,initial_fft_size,block,channels,checked_blocks,host_latency_samples,max_audio_error,bit_identical,tone_fifo_drops,final_fifo_drops,tone_discontinuities,final_discontinuities\n";
    for(double rate:{48000.,96000.,192000.})for(int size:{2048,4096,8192}) {
        auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
        for(auto* processor:{a.get(),b.get()}) {
            set(*processor,"mode",0);set(*processor,"ampon1",0);set(*processor,"cab1",0);
            for(int e=0;e<2;++e) {
                set(*processor,eqID(e,-1,"bypass"),0);set(*processor,eqID(e,0,"enabled"),1);
                set(*processor,eqID(e,0,"frequency"),e==0?700.f:3000.f);set(*processor,eqID(e,0,"gain"),e==0?4.f:-3.f);
            }
            processor->prepareToPlay(rate,128);
        }
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>(),finalAnalyzer=std::make_unique<EQSpectrumAnalyzer>();
        configure(*analyzer,b->graphicalEQ(0).analyzer,size,rate);configure(*finalAnalyzer,b->graphicalEQ(1).analyzer,size,rate);
        for(int e=0;e<2;++e)b->graphicalEQ(e).analyzer.enabled.store(true);
        const int latency=a->getLatencySamples();juce::AudioBuffer<float> x(2,128),y(2,128);juce::MidiBuffer midi;double maxError=0,energy=0;
        const int stallEnd=160+int(EQAnalyzerFIFO::capacity)/128+32;
        for(int k=0;k<stallEnd+64;++k) {
            if(k==32){b->graphicalEQ(0).analyzer.enabled.store(false);analyzer->reset(b->graphicalEQ(0).analyzer);}
            if(k==64){b->graphicalEQ(0).analyzer.enabled.store(true);b->graphicalEQ(1).analyzer.enabled.store(false);finalAnalyzer->reset(b->graphicalEQ(1).analyzer);}
            if(k==96)b->graphicalEQ(1).analyzer.enabled.store(true);
            if(k==128){analyzer->setFFTSize(size==8192?2048:size*2);analyzer->reset(b->graphicalEQ(0).analyzer);}
            for(int n=0;n<128;++n){const float value=(n==0 && k%73==0)?.4f:float(.05*std::sin(2*juce::MathConstants<double>::pi*997*(k*128+n)/rate));x.setSample(0,n,value);x.setSample(1,n,-value);}
            y.makeCopyOf(x,true);a->processBlock(x,midi);b->processBlock(y,midi);
            for(int c=0;c<2;++c) {
                for(int n=0;n<128;++n) {
                    const float reference=x.getSample(c,n),captured=y.getSample(c,n);
                    check(std::isfinite(reference) && std::isfinite(captured),"Nonfinite whole-processor output cannot establish audio invariance");
                    maxError=std::max(maxError,double(std::abs(reference-captured)));energy+=double(captured)*captured;
                }
                check(std::memcmp(x.getReadPointer(c),y.getReadPointer(c),128*sizeof(float))==0,
                      "Analyzer capture changed the actual output bits");
            }
            if((k<160 || k>=stallEnd) && k%15==14){analyzer->update(b->graphicalEQ(0).analyzer,rate);finalAnalyzer->update(b->graphicalEQ(1).analyzer,rate);}
            check(a->getLatencySamples()==latency && b->getLatencySamples()==latency,"Analysis changed host-reported audio latency");
        }
        check(maxError==0 && energy>1.e-6,"Passive analyzer changed actual whole-processor audio or fixture was silent");
        check(b->graphicalEQ(0).analyzer.dropped.load()>0 && b->graphicalEQ(1).analyzer.dropped.load()>0,
              "Whole-processor FIFO-full fixture did not actually overflow both banks");
        check(analyzer->discontinuities()>0 && finalAnalyzer->discontinuities()>0,"Whole-processor overflow was not recovered");
        invariance<<rate<<','<<size<<",128,2,"<<stallEnd+64<<','<<latency<<','<<maxError<<",1,"<<b->graphicalEQ(0).analyzer.dropped.load()<<','
                  <<b->graphicalEQ(1).analyzer.dropped.load()<<','<<analyzer->discontinuities()<<','<<finalAnalyzer->discontinuities()<<'\n';
        a->releaseResources();b->releaseResources();
        std::cout<<"PASS full processor rate="<<rate<<" initial_fft="<<size<<" unchanged latency="<<latency
                 <<" samples / both banks / capture toggles / resolution switch / impulse / FIFOoverflow / max_error="<<maxError<<'\n';
    }
    check(invariance.good(),"Audio invariance evidence could not be written");
}
}

int main(int argc,char** argv) {
    try {
        juce::ScopedJuceInitialiser_GUI init;
        const bool coverageProbe=argc>1 && std::strcmp(argv[1],"--temporal-coverage-probe")==0;
        const bool coverageRegression=argc>1 && std::strcmp(argv[1],"--temporal-coverage-regression")==0;
        const auto directory=juce::File::getCurrentWorkingDirectory().getChildFile((coverageProbe || coverageRegression)?(argc>2?argv[2]:"eq-analyzer-coverage"):(argc>1?argv[1]:"eq-analyzer"));
        check(directory.createDirectory().wasOk(),"Could not create analyzer evidence directory");
        if(coverageProbe || coverageRegression){temporalCoverageDiagnostic(directory,coverageRegression);if(coverageRegression)partitionInvariantCoverage(directory);return 0;}
        temporalCoverageDiagnostic(directory,true);partitionInvariantCoverage(directory);calibrationAndContinuity();
        lowFrequencyAndFiniteInput();smoothingAndStop();fifoStallsAndBoundedCatchup(directory);fftBenchmarks(directory);callbackAndLatency(directory);
        std::cout<<"PASS variable-resolution real-audio analysis, bounded consumer, timing evidence, unchanged processing and host latency\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
