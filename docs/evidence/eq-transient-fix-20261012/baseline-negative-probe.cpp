#include "EQSpectrumAnalyzer.h"
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

}
int main(){try{juce::ScopedJuceInitialiser_GUI init;const auto directory=juce::File::getCurrentWorkingDirectory().getChildFile("gate-results");check(directory.createDirectory().wasOk(),"output directory");temporalCoverageDiagnostic(directory,true);return 0;}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<"\n";return 1;}}
