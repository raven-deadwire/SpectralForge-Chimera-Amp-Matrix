// Test-only extension: retain the frozen comparison and production headers.
#define main preservedPerformanceMain
#include "EQAnalyzerPerformanceTests.cpp"
#undef main
#include <random>
#if defined(__linux__)
#include <sched.h>
#endif
namespace {
int observedCpu() {
#if defined(__linux__)
 return sched_getcpu();
#else
 return -1; // Not observed; do not infer Apple core placement.
#endif
}
}
// This isolated stage control uses the same JUCE real transform for both arms.
// The baseline post-stage reproduces JUCE 8.0.8 frequency-only magnitude/clear;
// it is not the production analyzer and does not certify full-update costs.
volatile double stageSink=0;
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void consumeStagePower(const float* power) {double total=0;for(int bin=0;bin<1025;++bin)total+=power[bin];stageSink=total;}
void stageControls(const std::filesystem::path& directory) {
 std::ofstream out(directory/"fft-stage-controls.csv");out<<std::setprecision(12);
 out<<"round,implementation,transforms,prepare_ms,real_fft_ms,power_and_tail_ms,total_thread_cpu_ms,checksum\n";
 juce::dsp::FFT fft(11);std::array<float,4096> work{};std::array<float,2048> input{};
 std::array<float,1025> power{};double windowSum=0;
 for(int n=0;n<2048;++n){const double w=.5-.5*std::cos(2*juce::MathConstants<double>::pi*n/2048);windowSum+=w;input[n]=float(w*std::sin(.137*n));}
 const double interiorGain=2./(windowSum*windowSum);
 for(int round=0;round<24;++round)for(int step=0;step<2;++step) {
  const bool candidate=(round%2)==step;double prep=0,kernel=0,post=0;
  const auto cpu=threadCpuSeconds();
  for(int repeat=0;repeat<1024;++repeat) {
   const auto start=Clock::now();std::copy(input.begin(),input.end(),work.begin());
   if(!candidate)std::fill(work.begin()+2048,work.end(),0.f);
   const auto prepared=Clock::now();fft.performRealOnlyForwardTransform(work.data(),true);
   const auto transformed=Clock::now();
   if(!candidate) {
    for(int bin=0;bin<1025;++bin)work[bin]=std::abs(std::complex<float>(work[2*bin],work[2*bin+1]));
    std::fill(work.begin()+1025,work.end(),0.f);
    for(int bin=0;bin<1025;++bin){if(!std::isfinite(work[bin]))continue;
     const double gain=(bin==0 || bin==1024)?1./windowSum:2./windowSum;
     const double amplitude=work[bin]*gain;power[bin]=float(std::min(.5*amplitude*amplitude,double(std::numeric_limits<float>::max())*.25));}
   }else {
    for(int bin=0;bin<1025;++bin){const double real=work[2*bin],imaginary=work[2*bin+1];
     if(!std::isfinite(real)||!std::isfinite(imaginary))continue;
     const double squared=real*real+imaginary*imaginary;
     constexpr double limit=std::numeric_limits<float>::max();if(squared>limit*limit)continue;
     const double gain=(bin==0||bin==1024)?interiorGain*.25:interiorGain;
     power[bin]=float(std::min(squared*gain,limit*.25));}
   }
   const auto completed=Clock::now();consumeStagePower(power.data());
   prep+=std::chrono::duration<double,std::milli>(prepared-start).count();
   kernel+=std::chrono::duration<double,std::milli>(transformed-prepared).count();
   post+=std::chrono::duration<double,std::milli>(completed-transformed).count();
  }
  const double elapsedCpu=1000*(threadCpuSeconds()-cpu);
  double checksum=0;for(float value:power)checksum+=value;
  out<<round<<','<<(candidate?"candidate":"baseline")<<",1024,"<<prep<<','<<kernel<<','<<post<<','<<elapsedCpu<<','<<checksum<<'\n';
 }
 check(out.good(),"Could not save stage controls");
}
int main(int argc,char** argv) {
 try {
  const std::filesystem::path directory=argc>1?argv[1]:".";
  std::filesystem::create_directories(directory);
  const int rounds=argc>2?std::stoi(argv[2]):12;
  const int repeats=argc>3?std::stoi(argv[3]):16;
  check(rounds>0 && repeats>0,"Positive rounds/repeats required");
  checkParity(directory);checkFiniteRecovery();stageControls(directory);
  std::ofstream out(directory/"cost-investigation.csv");out<<std::setprecision(12);
  out<<"round,slot,rate,input_rate,fft_size,implementation,repeats,start_cpu,end_cpu,start_thread_cpu_s,end_thread_cpu_s,start_wall_ns,end_wall_ns,update_ms,fft_inclusive_ms,feed_analysis_wall_ms,feed_analysis_cpu_ms,windows,samples,drops\n";
  std::mt19937 random(0x52);
  // Identical bytes at every rate distinguish rate math from input content.
  const auto data=fixture(192000.,2048,int(EQAnalyzerFIFO::capacity));
  std::array<std::unique_ptr<EQSpectrumAnalyzerBeb72f>,8> baseline;
  std::array<std::unique_ptr<EQSpectrumAnalyzer>,8> candidate;
  for(auto& analyzer:baseline)analyzer=std::make_unique<EQSpectrumAnalyzerBeb72f>();
  for(auto& analyzer:candidate)analyzer=std::make_unique<EQSpectrumAnalyzer>();
  auto fifo=std::make_unique<EQAnalyzerFIFO>();
  struct Cell {double rate;int implementation;};
  for(int round=0;round<rounds;++round) {
   std::vector<Cell> cells;
   for(double rate:{44100.,48000.,96000.,192000.})for(int implementation:{0,1,2,3})cells.push_back({rate,implementation});
   std::shuffle(cells.begin(),cells.end(),random);
   int slot=0;
   for(auto cell:cells) {
    const int rateIndex=cell.rate==44100.?0:cell.rate==48000.?1:cell.rate==96000.?2:3;
    const int index=rateIndex*2+cell.implementation/2;
    Timing sum;const int firstCpu=observedCpu();const double cpuStart=threadCpuSeconds();const auto start=Clock::now();
    for(int repeat=0;repeat<repeats;++repeat) {
     Timing t;
     if(cell.implementation==0)t=measure(*baseline[index],*fifo,2048,cell.rate,int(data.size()),data);
     if(cell.implementation==1)t=measure(*candidate[index],*fifo,2048,cell.rate,int(data.size()),data);
     if(cell.implementation==2)t=measure(*baseline[index],*fifo,2048,cell.rate,int(data.size()),data);
     if(cell.implementation==3)t=measure(*candidate[index],*fifo,2048,cell.rate,int(data.size()),data);
     sum.updateUs+=t.updateUs;sum.fftUs+=t.fftUs;sum.wallMs+=t.wallMs;sum.threadCpuMs+=t.threadCpuMs;
     sum.windows+=t.windows;sum.samples+=t.samples;sum.drops+=t.drops;
    }
    const auto end=Clock::now();const double cpuEnd=threadCpuSeconds();const int lastCpu=observedCpu();
    const char* names[]={"baseline","candidate","baseline_AA","candidate_AA"};
    out<<round<<','<<slot++<<','<<cell.rate<<",192000,2048,"<<names[cell.implementation]<<','<<repeats<<','<<firstCpu<<','<<lastCpu<<','<<cpuStart<<','<<cpuEnd<<','
       <<std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count()<<','
       <<std::chrono::duration_cast<std::chrono::nanoseconds>(end.time_since_epoch()).count()<<','
       <<sum.updateUs*.001<<','<<sum.fftUs*.001<<','<<sum.wallMs<<','<<sum.threadCpuMs<<','<<sum.windows<<','<<sum.samples<<','<<sum.drops<<'\n';
   }
   std::cout<<"Completed round "<<round+1<<'/'<<rounds<<std::endl;
  }
  check(out.good(),"Could not save investigation rows");
  std::cout<<"DAW/interface acceptance NOT_ASSESSED; inclusive FFT timers differ between implementations\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
