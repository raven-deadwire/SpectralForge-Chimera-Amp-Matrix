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
    for(int size:{2048,4096,8192}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size);
        for(double hz:{31.,55.,63.,251.,997.,8011.}) {
            analyzer->reset(*fifo);
            for(int n=0;n<size;++n){const float x=float(.1*std::sin(2*juce::MathConstants<double>::pi*hz*n/48000));fifo->push({x,x,-x,-x});}
            analyzer->update(*fifo,48000);int peak=1;
            for(int bin=2;bin<size/2;++bin)if(analyzer->rawDB(false,bin)>analyzer->rawDB(false,peak))peak=bin;
            // One measured bin is the explicit tolerance. Display interpolation
            // cannot promise a finer low-frequency resolution than the FFT.
            near(48000.*peak/size,hz,48000./size,"Off-bin/low-frequency peak outside the measured resolution");
            check(analyzer->rawDB(false,peak)>-24 && analyzer->rawDB(false,peak)<-17,"Off-bin Hann peak calibration implausible");
        }
        analyzer->reset(*fifo);uint32_t noise=0x4251567u;
        for(int n=0;n<size;++n){noise^=noise<<13;noise^=noise>>17;noise^=noise<<5;const float x=(float(noise)/float(UINT32_MAX)-.5f)*.1f;fifo->push({x,x,-x,-x});}
        analyzer->update(*fifo,48000);float bandPeak=-120;
        for(int bin=int(std::ceil(100.*size/48000));bin<=int(std::floor(1000.*size/48000));++bin)bandPeak=std::max(bandPeak,analyzer->rawDB(false,bin));
        check(analyzer->displayDB(false,100,500,1000)>=bandPeak-.01f,"Broadband screen-pixel aggregation discarded its strongest measured bin");
        analyzer->reset(*fifo);
        const std::array<float,4> invalid{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::max()};
        for(int n=0;n<size;++n){const auto x=invalid[size_t(n%4)];fifo->push({x,x,-x,-x});}analyzer->update(*fifo,48000);
        for(int bin=0;bin<=size/2;++bin)for(bool output:{false,true}) {
            check(std::isfinite(analyzer->rawDB(output,bin)) && std::isfinite(analyzer->smoothedDB(output,bin)),"Malformed input produced a nonfinite spectrum");
            const double hz=48000.*bin/size;check(std::isfinite(analyzer->displayDB(output,hz*.99,hz,hz*1.01)),"Malformed input produced a nonfinite display path");
        }
        fillFIFO(*fifo,size,0,size,43);analyzer->update(*fifo,48000);near(analyzer->rawDB(false,43),-20,.02,"Valid tone did not recover after malformed input");
    }
    std::cout<<"PASS31/55Hz and stepped frequency peaks at honest bin tolerance / deterministic broadband / nonfinite input recovery\n";
}
void calibrationAndContinuity() {
    for(int size:{2048,4096,8192}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size);
        constexpr int bin=43;const double gain=std::pow(10.,6./20.);
        fillFIFO(*fifo,size-1,0,size,bin,.1,gain);
        analyzer->update(*fifo,48000);
        check(!analyzer->hasData() && analyzer->windowSamples()==size-1 && analyzer->frames()==0,
              "Spectrum was fabricated before a complete real observation window");
        fillFIFO(*fifo,1,size-1,size,bin,.1,gain);analyzer->update(*fifo,48000);
        check(analyzer->hasData() && analyzer->frames()==1,"First complete window was not transformed");
        near(analyzer->rawDB(false,bin),-20,.02,"Opposite-polarity input calibration");
        near(analyzer->rawDB(true,bin),-14,.02,"Actual output calibration");
        const double hz=48000.*bin/size;
        near(analyzer->displayDB(false,hz*.999,hz,hz*1.001),-20,.02,"Log pixel lost a measured peak");
        near(analyzer->displayDB(false,25000,26000,27000),-120,0,"Above-Nyquist pixels are not empty");

        // The same full-scale sine in a single channel measures 3 dB below the
        // equal-power stereo calibration; neither polarity nor channel count is
        // allowed to silently change the calibration definition.
        analyzer->reset(*fifo);fillFIFO(*fifo,size,0,size,bin,1,1,0);analyzer->update(*fifo,48000);
        near(analyzer->rawDB(false,bin),-3.0102999566,.02,"Single-channel power calibration");
        analyzer->reset(*fifo);fillFIFO(*fifo,size,0,size,bin,1,1,1);analyzer->update(*fifo,48000);
        near(analyzer->rawDB(false,bin),0,.02,"Coherent full-scale sine calibration");

        // DC and Nyquist are single-sided bins and have no factor-of-two gain.
        analyzer->reset(*fifo);for(int n=0;n<size;++n)fifo->push({.1f,.1f,-.1f,-.1f});analyzer->update(*fifo,48000);
        near(analyzer->rawDB(false,0),-20,.02,"DC endpoint calibration");
        analyzer->reset(*fifo);for(int n=0;n<size;++n){const float x=n%2?.1f:-.1f;fifo->push({x,x,-x,-x});}analyzer->update(*fifo,48000);
        near(analyzer->rawDB(false,size/2),-20,.02,"Nyquist endpoint calibration");

        analyzer->reset(*fifo);fillFIFO(*fifo,size,0,size,bin);analyzer->update(*fifo,48000);
        const auto before=analyzer->frames();fillFIFO(*fifo,size,0,size,bin+31);analyzer->update(*fifo,48000);
        check(analyzer->frames()==before+1,"More than one FFT batch per consumer update");
        near(analyzer->rawDB(false,bin+31),-20,.02,"Changed frequency did not move the actual peak");
        check(analyzer->rawDB(false,bin)<-100,"Old frequency survived a complete replacement window");
        check(analyzer->smoothedDB(false,bin)>analyzer->rawDB(false,bin),"Release smoothing is not active");
        const auto stableFrames=analyzer->frames();analyzer->update(*fifo,48000);
        check(analyzer->frames()==stableFrames && analyzer->fftMicros()==0,"Empty consumer repeated an FFT");

        // An overflow is an explicit discontinuity, not time-compressed old
        // samples. Recovery needs a fresh complete window before rendering.
        const auto discontinuities=analyzer->discontinuities();
        fillFIFO(*fifo,int(EQAnalyzerFIFO::capacity)+7,0,size,bin);analyzer->update(*fifo,48000);
        check(analyzer->discontinuities()==discontinuities+1 && !analyzer->hasData() && fifo->pendingSamples()==0,
              "Overflow retained a discontinuous spectrum or queued catch-up work");
        check(analyzer->lastConsumedSamples()<=EQAnalyzerFIFO::capacity,"Unbounded FIFO drain");
        fillFIFO(*fifo,size,0,size,bin);analyzer->update(*fifo,48000);
        check(analyzer->hasData(),"Recovery could not accept a new observation");

        // A rate transition invalidates existing sample timing and queued data.
        analyzer->update(*fifo,96000);check(!analyzer->hasData(),"Rate transition retained old spectrum");
        fillFIFO(*fifo,size,0,size,bin);analyzer->update(*fifo,96000);
        near(analyzer->rawDB(false,bin),-20,.02,"New sample-rate observation calibration");
        check(analyzer->lastSampleRate()==96000,"New sample rate not recorded");
        std::cout<<"PASS FFT "<<size<<" bin="<<48000./size<<" Hz window="<<1000.*size/48000
                 <<" ms / antiphase, amplitude, endpoints, frequency change, overflow, rate transition\n";
    }
}

void smoothingAndStop() {
    constexpr int size=4096,bin=43;
    std::array<double,3> after{};
    for(int mode=0;mode<3;++mode) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size);analyzer->setSmoothing(mode);
        fillFIFO(*fifo,size,0,size,bin);analyzer->update(*fifo,48000);
        for(int n=0;n<size;++n)fifo->push({});analyzer->update(*fifo,48000);
        const double expected=-20.-10./std::log(10.)*(double(size)/48000/analyzer->smoothingReleaseSeconds());
        after[(size_t)mode]=analyzer->smoothedDB(false,bin);
        near(after[(size_t)mode],expected,.02,"Power-domain release does not follow elapsed sample time");
        check(analyzer->rawDB(false,bin)<=-119.9f,"Silent audio was not analyzed as silence");
        const auto frames=analyzer->frames();const auto displayed=analyzer->smoothedDB(false,bin);
        analyzer->setSmoothing((mode+1)%3);
        check(analyzer->frames()==frames && analyzer->smoothedDB(false,bin)==displayed && analyzer->hasData(),
              "Changing smoothing needlessly discarded the live observation");
    }
    check(after[0]<after[1] && after[1]<after[2],"Fast/Balanced/Smooth release ordering");
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

// Opt-in diagnostic, not an assertion that dropping a visible transient is an
// acceptable product result. Run with --temporal-coverage-probe [output-folder].
// A real FIFO/analyzer receives the same short synthetic burst at two positions
// in a nominal 40 ms UI interval. This isolates observation-window coverage
// from FIFO overflow, amplitude calibration and source startup.
void temporalCoverageDiagnostic(const juce::File& directory) {
    std::ofstream out(directory.getChildFile("eq-analyzer-temporal-coverage.csv").getFullPathName().toStdString());
    out<<"rate,fft_size,ui_interval_ms,hop_samples,window_ms,overwritten_prefix_samples,burst_position,burst_start,burst_samples,input_energy,raw_peak_db,smoothed_peak_db,fft_batches,fifo_dropped_samples\n";
    for(double rate:{48000.,96000.,192000.})for(int size:{2048,4096,8192})for(bool late:{false,true}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size,rate);
        // Start from a complete, silent observation; this is a steady-state
        // probe and does not confuse initial window acquisition with coverage.
        for(int n=0;n<size;++n)fifo->push({});analyzer->update(*fifo,rate);
        check(analyzer->hasData(),"Temporal coverage probe did not acquire a complete initial window");
        const auto beforeFrames=analyzer->frames(),beforeDropped=fifo->dropped.load();
        const int hop=int(rate/25),burstSamples=384;
        const int burstStart=late?hop-std::min(size,hop)/2-burstSamples/2:64;
        double energy=0;
        for(int n=0;n<hop;++n) {
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
        const auto dropped=fifo->dropped.load()-beforeDropped,batches=analyzer->frames()-beforeFrames;
        check(energy>1.e-6 && dropped==0 && batches==1,"Temporal coverage fixture was silent, dropped FIFO data or did not perform one update");
        out<<rate<<','<<size<<",40,"<<hop<<','<<1000.*size/rate<<','<<std::max(0,hop-size)<<','
           <<(late?"late":"early")<<','<<burstStart<<','<<burstSamples<<','<<energy<<','<<rawPeak<<','<<smoothPeak<<','<<batches<<','<<dropped<<'\n';
        std::cout<<"COVERAGE rate="<<rate<<" FFT="<<size<<" position="<<(late?"late":"early")
                 <<" hop="<<hop<<" overwritten_prefix="<<std::max(0,hop-size)<<" samples input_energy="<<energy
                 <<" raw_peak="<<rawPeak<<" dBFS fifo_drops="<<dropped<<'\n';
    }
    check(out.good(),"Temporal coverage diagnostic could not be written");
}

void fftBenchmarks(const juce::File& directory) {
    std::ofstream out(directory.getChildFile("eq-analyzer-cpu.csv").getFullPathName().toStdString());
    out<<"fft_size,rate,bin_spacing_hz,hann_enbw_hz,window_ms,fft_batches,fft_p50_us,fft_p99_us,update_p50_us,update_p99_us,update_mean_us,analysis_cpu_at_25hz_percent\n";
    for(double rate:{48000.,96000.,192000.})for(int size:{2048,4096,8192}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size,rate);std::vector<double> fft,update;
        const int hop=int(rate/25);int offset=0;
        for(int k=0;k<180;++k) {
            fillFIFO(*fifo,hop,offset,size,43);offset+=hop;const auto before=analyzer->frames();
            analyzer->update(*fifo,rate);
            check(analyzer->frames()<=before+1 && analyzer->lastConsumedSamples()<=EQAnalyzerFIFO::capacity,
                  "Consumer work exceeds one bounded frame");
            if(k>=20 && analyzer->frames()>before){fft.push_back(analyzer->fftMicros());update.push_back(analyzer->updateMicros());}
        }
        check(fft.size()==160 && fifo->dropped.load()==0,"FFT timing fixture dropped or omitted samples");
        const double mean=std::accumulate(update.begin(),update.end(),0.)/double(update.size());
        out<<size<<','<<rate<<','<<rate/size<<','<<1.5*rate/size<<','<<1000.*size/ rate<<','<<fft.size()<<','
           <<percentile(fft,.5)<<','<<percentile(fft,.99)<<','<<percentile(update,.5)<<','<<percentile(update,.99)<<','<<mean<<','<<mean*25/10000<<'\n';
        std::cout<<"FFT CPU "<<size<<" @ "<<rate<<" Hz p50="<<percentile(fft,.5)<<" us update p50/p99="
                 <<percentile(update,.5)<<'/'<<percentile(update,.99)<<" us; observation="<<1000.*size/rate<<" ms\n";
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
    for(double rate:{48000.,96000.})for(int block:{64,128,512})for(int size:{0,2048,4096,8192}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();configure(*analyzer,p->graphicalEQ(0).analyzer,size==0?4096:size,rate);
        for(int e=0;e<2;++e){p->graphicalEQ(e).prepare(rate);p->graphicalEQ(e).analyzer.enabled.store(size>0 && e==0);}
        juce::AudioBuffer<float> audio(2,block);std::vector<double> times;times.reserve(600);int sinceUpdate=0;
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
    for(int size:{2048,4096,8192}) {
        auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
        for(auto* processor:{a.get(),b.get()}) {
            set(*processor,"mode",0);set(*processor,"ampon1",0);set(*processor,"cab1",0);
            for(int e=0;e<2;++e) {
                set(*processor,eqID(e,-1,"bypass"),0);set(*processor,eqID(e,0,"enabled"),1);
                set(*processor,eqID(e,0,"frequency"),e==0?700.f:3000.f);set(*processor,eqID(e,0,"gain"),e==0?4.f:-3.f);
            }
            processor->prepareToPlay(48000,128);
        }
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>(),finalAnalyzer=std::make_unique<EQSpectrumAnalyzer>();
        configure(*analyzer,b->graphicalEQ(0).analyzer,size);configure(*finalAnalyzer,b->graphicalEQ(1).analyzer,size);
        for(int e=0;e<2;++e)b->graphicalEQ(e).analyzer.enabled.store(true);
        const int latency=a->getLatencySamples();juce::AudioBuffer<float> x(2,128),y(2,128);juce::MidiBuffer midi;double maxError=0,energy=0;
        const int stallEnd=160+int(EQAnalyzerFIFO::capacity)/128+32;
        for(int k=0;k<stallEnd+64;++k) {
            if(k==32){b->graphicalEQ(0).analyzer.enabled.store(false);analyzer->reset(b->graphicalEQ(0).analyzer);}
            if(k==64){b->graphicalEQ(0).analyzer.enabled.store(true);b->graphicalEQ(1).analyzer.enabled.store(false);finalAnalyzer->reset(b->graphicalEQ(1).analyzer);}
            if(k==96)b->graphicalEQ(1).analyzer.enabled.store(true);
            if(k==128){analyzer->setFFTSize(size==8192?2048:size*2);analyzer->reset(b->graphicalEQ(0).analyzer);}
            for(int n=0;n<128;++n){const float value=(n==0 && k%73==0)?.4f:float(.05*std::sin(2*juce::MathConstants<double>::pi*997*(k*128+n)/48000));x.setSample(0,n,value);x.setSample(1,n,-value);}
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
            if((k<160 || k>=stallEnd) && k%15==14){analyzer->update(b->graphicalEQ(0).analyzer,48000);finalAnalyzer->update(b->graphicalEQ(1).analyzer,48000);}
            check(a->getLatencySamples()==latency && b->getLatencySamples()==latency,"Analysis changed host-reported audio latency");
        }
        check(maxError==0 && energy>1.e-6,"Passive analyzer changed actual whole-processor audio or fixture was silent");
        check(b->graphicalEQ(0).analyzer.dropped.load()>0 && b->graphicalEQ(1).analyzer.dropped.load()>0,
              "Whole-processor FIFO-full fixture did not actually overflow both banks");
        check(analyzer->discontinuities()>0 && finalAnalyzer->discontinuities()>0,"Whole-processor overflow was not recovered");
        a->releaseResources();b->releaseResources();
        std::cout<<"PASS full processor initial_fft="<<size<<" unchanged latency="<<latency
                 <<" samples / both banks / capture toggles / resolution switch / impulse / FIFOoverflow / max_error="<<maxError<<'\n';
    }
}
}

int main(int argc,char** argv) {
    try {
        juce::ScopedJuceInitialiser_GUI init;
        const bool coverageProbe=argc>1 && std::strcmp(argv[1],"--temporal-coverage-probe")==0;
        const auto directory=juce::File::getCurrentWorkingDirectory().getChildFile(coverageProbe?(argc>2?argv[2]:"eq-analyzer-coverage"):(argc>1?argv[1]:"eq-analyzer"));
        check(directory.createDirectory().wasOk(),"Could not create analyzer evidence directory");
        if(coverageProbe){temporalCoverageDiagnostic(directory);return 0;}
        calibrationAndContinuity();lowFrequencyAndFiniteInput();smoothingAndStop();fftBenchmarks(directory);callbackAndLatency(directory);
        std::cout<<"PASS variable-resolution real-audio analysis, bounded consumer, timing evidence, unchanged processing and host latency\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
