#include "EQSpectrumAnalyzer.h"
// Both classes execute against the same JUCE implementation in this process.
// The frozen file is byte-identical to the already-fixed PR #52 analyzer.
#define EQSpectrumAnalyzer EQSpectrumAnalyzerBeb72f
#include "reference/EQSpectrumAnalyzerBeb72f.h"
#undef EQSpectrumAnalyzer
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>
#if defined(_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#else
 #include <time.h>
#endif

using namespace spectralforge;
namespace {
using Clock=std::chrono::steady_clock;
constexpr const char* baselineHash="ad0e9f7dc7fe46579b24707291bf63a5ee09363de701b1606c5cedf98f970f26";
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
double threadCpuSeconds() {
#if defined(_WIN32)
    FILETIME creation{},exit{},kernel{},user{};
    check(GetThreadTimes(GetCurrentThread(),&creation,&exit,&kernel,&user)!=0,"Thread CPU timer failed");
    ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
    return double(k.QuadPart+u.QuadPart)*1.e-7;
#else
    timespec t{};check(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)==0,"Thread CPU timer failed");
    return double(t.tv_sec)+double(t.tv_nsec)*1.e-9;
#endif
}
double median(std::vector<double> values) {
    check(!values.empty(),"No paired timing observations");
    std::sort(values.begin(),values.end());const auto n=values.size();
    return n%2?values[n/2]:.5*(values[n/2-1]+values[n/2]);
}
template<typename Analyzer>
void configure(Analyzer& analyzer,EQAnalyzerFIFO& fifo,int size,double rate,int smoothing=1) {
    analyzer.setFFTSize(size);analyzer.setSmoothing(smoothing);analyzer.reset(fifo);analyzer.update(fifo,rate);
}

// Precompute identical inputs outside measured regions: broadband energy,
// low fundamentals, off-bin tones, DC/Nyquist, antiphase stereo and short bursts.
std::vector<EQAnalyzerFrame> fixture(double rate,int size,int count) {
    std::vector<EQAnalyzerFrame> frames;frames.reserve(size_t(count));
    uint32_t random=0x85d02e9bu;
    for(int n=0;n<count;++n) {
        random=1664525u*random+1013904223u;
        const double noise=(double(random>>8)/16777216.-.5)*.011;
        const double time=double(n)/rate;
        const double burst=(n%(size*3)>size+17 && n%(size*3)<size+401)?
            .35*std::sin(2*juce::MathConstants<double>::pi*997*time):0.;
        const double tone=.09*std::sin(2*juce::MathConstants<double>::pi*55*time)+
                          .14*std::sin(2*juce::MathConstants<double>::pi*4001*time);
        const float x=float(.006+tone+burst+noise+.002*(n%2?1.:-1.));
        // Separate input and output powers and nonidentical stereo amplitudes.
        frames.push_back({x,.43f*x,-.73f*x,-.19f*x});
    }
    return frames;
}

void checkParity(const std::filesystem::path& directory) {
    std::ofstream out(directory/"eq-analyzer-performance-parity.csv");
    out<<std::setprecision(12);
    out<<"rate,fft_size,ui_interval_ms,smoothing,checked_updates,fft_windows,consumed_samples,remaining_hop_samples,max_raw_error_db,max_envelope_error_db,max_display_error_db,max_raw_power_relative_error,max_smoothed_power_relative_error,max_display_db,dropped_samples,discontinuities,parity_pass\n";
    double overallRaw=0,overallSmooth=0,overallDisplay=0;int cases=0;
    for(double rate:{44100.,48000.,96000.,192000.})for(int size:{2048,4096,8192}) {
      const auto data=fixture(rate,size,int(rate*.48)+2*size);
      for(int uiMs:{20,40,80})for(int mode=0;mode<3;++mode) {
        auto reference=std::make_unique<EQSpectrumAnalyzerBeb72f>();
        auto candidate=std::make_unique<EQSpectrumAnalyzer>();
        auto referenceFIFO=std::make_unique<EQAnalyzerFIFO>();
        auto candidateFIFO=std::make_unique<EQAnalyzerFIFO>();
        configure(*reference,*referenceFIFO,size,rate,mode);configure(*candidate,*candidateFIFO,size,rate,mode);
        double rawError=0,smoothError=0,displayError=0,rawPowerError=0,smoothPowerError=0;
        float peak=EQSpectrumAnalyzer::floorDB;int updates=0;
        const int batch=std::max(1,int(rate*uiMs/1000.));
        for(size_t start=0;start<data.size();start+=size_t(batch)) {
          const auto end=std::min(data.size(),start+size_t(batch));
          for(size_t n=start;n<end;++n){referenceFIFO->push(data[n]);candidateFIFO->push(data[n]);}
          reference->update(*referenceFIFO,rate);candidate->update(*candidateFIFO,rate);++updates;
          const auto expected=end<size_t(size)?0:1+(end-size_t(size))/size_t(size/4);
          check(reference->frames()==expected && candidate->frames()==expected,"A/B omitted or duplicated an N/4 analysis window");
          check(reference->lastFFTWindows()==candidate->lastFFTWindows() &&
                reference->lastConsumedSamples()==candidate->lastConsumedSamples() &&
                reference->lastAnalysedSamples()==candidate->lastAnalysedSamples() &&
                reference->samplesSinceLastFFT()==candidate->samplesSinceLastFFT(),"A/B analysis timing or snapshot accounting changed");
          for(bool output:{false,true})for(int bin=0;bin<=size/2;++bin) {
            const double a=reference->rawDB(output,bin),b=candidate->rawDB(output,bin);
            const double c=reference->smoothedDB(output,bin),d=candidate->smoothedDB(output,bin);
            check(std::isfinite(a) && std::isfinite(b) && std::isfinite(c) && std::isfinite(d),"Nonfinite A/B analysis bin");
            rawError=std::max(rawError,std::abs(a-b));smoothError=std::max(smoothError,std::abs(c-d));
            rawPowerError=std::max(rawPowerError,std::abs(std::pow(10.,(b-a)/10.)-1.));
            smoothPowerError=std::max(smoothPowerError,std::abs(std::pow(10.,(d-c)/10.)-1.));
          }
          for(bool output:{false,true})for(double hz:{20.,31.,55.,251.,997.,4000.,8000.,16000.,20000.}) {
            const float a=reference->displayDB(output,hz*.91,hz,hz*1.09);
            const float b=candidate->displayDB(output,hz*.91,hz,hz*1.09);
            displayError=std::max(displayError,double(std::abs(a-b)));peak=std::max(peak,b);
          }
          check(referenceFIFO->pendingSamples()==0 && candidateFIFO->pendingSamples()==0 &&
                referenceFIFO->dropped.load()==0 && candidateFIFO->dropped.load()==0 &&
                reference->discontinuities()==0 && candidate->discontinuities()==0,"A/B lost FIFO continuity");
        }
        // Float magnitude rounding can change the last bits of float power;
        // 0.002 dB matches the existing partition-invariance regression limit.
        check(rawError<=.002 && smoothError<=.002 && displayError<=.002,"A/B exceeded spectral preservation tolerance");
        check(candidate->totalConsumedSamples()==data.size(),"A/B did not consume every fixture sample");
        out<<rate<<','<<size<<','<<uiMs<<','<<mode<<','<<updates<<','<<candidate->frames()<<','<<candidate->totalConsumedSamples()<<','
           <<candidate->samplesSinceLastFFT()<<','<<rawError<<','<<smoothError<<','<<displayError<<','<<rawPowerError<<','<<smoothPowerError<<','<<peak<<",0,0,1\n";
        overallRaw=std::max(overallRaw,rawError);overallSmooth=std::max(overallSmooth,smoothError);overallDisplay=std::max(overallDisplay,displayError);++cases;
      }
    }
    check(out.good(),"Could not write A/B parity evidence");
    std::cout<<"PASS "<<cases<<" baseline/candidate spectral cases, max raw/envelope/display errors "
             <<overallRaw<<'/'<<overallSmooth<<'/'<<overallDisplay<<" dB; every chronological window and FIFO sample preserved\n";
}

// Probe corruption handling separately from musical spectral parity. Infinities,
// NaNs and finite values whose FFT overflows must not poison future frames.
void checkFiniteRecovery() {
    for(int size:{2048,4096,8192}) {
        auto reference=std::make_unique<EQSpectrumAnalyzerBeb72f>();auto candidate=std::make_unique<EQSpectrumAnalyzer>();
        auto referenceFIFO=std::make_unique<EQAnalyzerFIFO>();auto candidateFIFO=std::make_unique<EQAnalyzerFIFO>();
        configure(*reference,*referenceFIFO,size,192000.);configure(*candidate,*candidateFIFO,size,192000.);
        for(int n=0;n<2*size;++n) {
            const float value=n%3==0?std::numeric_limits<float>::infinity():n%3==1?
                std::numeric_limits<float>::quiet_NaN():std::numeric_limits<float>::max();
            const EQAnalyzerFrame frame{value,value,-value,-value};referenceFIFO->push(frame);candidateFIFO->push(frame);
        }
        reference->update(*referenceFIFO,192000.);candidate->update(*candidateFIFO,192000.);
        for(bool output:{false,true})for(int bin=0;bin<=size/2;++bin)
            check(std::isfinite(candidate->rawDB(output,bin)) && std::isfinite(candidate->smoothedDB(output,bin)),"Corrupt bins escaped finite guard");
        reference->reset(*referenceFIFO);candidate->reset(*candidateFIFO);
        const auto data=fixture(192000.,size,2*size);
        for(const auto& frame:data){referenceFIFO->push(frame);candidateFIFO->push(frame);}
        reference->update(*referenceFIFO,192000.);candidate->update(*candidateFIFO,192000.);
        for(bool output:{false,true})for(int bin=0;bin<=size/2;++bin)
            check(std::abs(reference->rawDB(output,bin)-candidate->rawDB(output,bin))<=.002f,"Corrupt data poisoned a fresh FFT");
    }
    std::cout<<"PASS nonfinite/overflow containment and fresh-spectrum recovery\n";
}

struct Timing {
    double updateUs{},fftUs{},wallMs{},threadCpuMs{},maxUpdateUs{};
    uint64_t windows{},samples{},updates{},drops{};
};
template<typename Analyzer>
Timing measure(Analyzer& analyzer,EQAnalyzerFIFO& fifo,int size,double rate,int batch,
               const std::vector<EQAnalyzerFrame>& data) {
    configure(analyzer,fifo,size,rate);
    // Prime FFT code/data and acquire a complete observation before timing.
    for(int n=0;n<size;++n)fifo.push(data[size_t(n)]);
    analyzer.update(fifo,rate);const auto initialWindows=analyzer.frames();
    Timing result{};
    const double cpuStart=threadCpuSeconds();const auto start=Clock::now();
    for(size_t offset=0;offset<data.size();offset+=size_t(batch)) {
        const auto end=std::min(data.size(),offset+size_t(batch));
        for(size_t n=offset;n<end;++n)fifo.push(data[n]);
        analyzer.update(fifo,rate);++result.updates;
        result.updateUs+=analyzer.updateMicros();result.fftUs+=analyzer.fftMicros();
        result.maxUpdateUs=std::max(result.maxUpdateUs,analyzer.updateMicros());
    }
    result.wallMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    result.threadCpuMs=1000.*(threadCpuSeconds()-cpuStart);
    result.windows=analyzer.frames()-initialWindows;result.samples=data.size();result.drops=fifo.dropped.load();
    check(result.windows==data.size()/size_t(size/4) && result.drops==0 && fifo.pendingSamples()==0 &&
          analyzer.discontinuities()==0,"Paired cost benchmark omitted a window or lost a sample");
    return result;
}

void costBenchmarks(const std::filesystem::path& directory) {
    std::ofstream raw(directory/"eq-analyzer-performance-paired.csv"),summary(directory/"eq-analyzer-performance-summary.csv");
    raw<<std::setprecision(12);summary<<std::setprecision(12);
    raw<<"rate,fft_size,fixture,ui_interval_ms,sequence,implementation,produced_samples,fft_windows,consumer_calls,analysis_update_wall_ms,fft_operation_wall_ms,feed_and_analysis_wall_ms,feed_and_analysis_thread_cpu_ms,max_update_us,dropped_samples\n";
    summary<<"rate,fft_size,fixture,ui_interval_ms,pairs,produced_samples_per_trial,simulated_audio_ms,fft_windows_per_trial,baseline_update_median_ms,candidate_update_median_ms,update_cost_reduction_percent,baseline_feed_analysis_cpu_median_ms,candidate_feed_analysis_cpu_median_ms,baseline_update_wall_equivalent_percent,candidate_update_wall_equivalent_percent,daw_performance_status\n";
    // ABBA BAAB limits order/cache/drift bias; measurements are comparative cost
    // evidence, never pass/fail claims about an audio-device deadline.
    constexpr std::array<bool,8> order{false,true,true,false,true,false,false,true};
    for(double rate:{44100.,48000.,96000.,192000.})for(int size:{2048,4096,8192}) {
      const auto pacedData=fixture(rate,size,int(2*rate));
      const auto fullSnapshot=fixture(rate,size,int(EQAnalyzerFIFO::capacity));
      for(int uiMs:{20,40,80,-1}) {
        const bool fullCapacity=uiMs<0;
        const auto& data=fullCapacity?fullSnapshot:pacedData;
        const char* fixtureName=fullCapacity?"full_capacity_catchup":"nominal_cadence";
        const auto interval=fullCapacity?std::string{}:std::to_string(uiMs);
        auto reference=std::make_unique<EQSpectrumAnalyzerBeb72f>();auto candidate=std::make_unique<EQSpectrumAnalyzer>();
        auto referenceFIFO=std::make_unique<EQAnalyzerFIFO>();auto candidateFIFO=std::make_unique<EQAnalyzerFIFO>();
        std::array<std::vector<double>,2> update,cpu;uint64_t expectedWindows=0;
        for(size_t k=0;k<order.size();++k) {
            const bool optimized=order[k];
            const int batch=fullCapacity?int(EQAnalyzerFIFO::capacity):std::max(1,int(rate*uiMs/1000.));
            const auto t=optimized?measure(*candidate,*candidateFIFO,size,rate,batch,data):
                                   measure(*reference,*referenceFIFO,size,rate,batch,data);
            update[size_t(optimized)].push_back(t.updateUs*.001);cpu[size_t(optimized)].push_back(t.threadCpuMs);
            expectedWindows=t.windows;
            raw<<rate<<','<<size<<','<<fixtureName<<','<<interval<<','<<k<<','<<(optimized?"candidate":"baseline_beb72f")<<','<<t.samples<<','<<t.windows<<','<<t.updates<<','
               <<t.updateUs*.001<<','<<t.fftUs*.001<<','<<t.wallMs<<','<<t.threadCpuMs<<','<<t.maxUpdateUs<<','<<t.drops<<'\n';
        }
        const double a=median(update[0]),b=median(update[1]);
        const double simulatedMs=1000.*double(data.size())/rate;
        summary<<rate<<','<<size<<','<<fixtureName<<','<<interval<<",4,"<<data.size()<<','<<simulatedMs<<','<<expectedWindows<<','<<a<<','<<b<<','<<100.*(1.-b/a)<<','
               <<median(cpu[0])<<','<<median(cpu[1])<<','<<100.*a/simulatedMs<<','<<100.*b/simulatedMs<<",NOT_ASSESSED\n";
        std::cout<<"PAIR rate="<<rate<<" FFT="<<size<<" fixture="<<fixtureName<<" UI="<<interval<<" ms median_update="<<a<<" -> "<<b
                 <<" ms reduction="<<100.*(1.-b/a)<<"%; windows="<<expectedWindows<<"; DAW=NOT_ASSESSED\n";
      }
    }
    check(raw.good() && summary.good(),"Could not write paired performance evidence");
}
}

int main(int argc,char** argv) {
    try {
        const std::filesystem::path directory=argc>1?argv[1]:".";
        std::filesystem::create_directories(directory);
        std::cout<<"Frozen analyzer SHA-256 "<<baselineHash<<"; same-executable ABBA BAAB; no real audio device or DAW\n";
        checkParity(directory);checkFiniteRecovery();costBenchmarks(directory);
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL EQ analyzer A/B: "<<e.what()<<'\n';return 1;}
}
