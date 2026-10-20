// Separate FFT microbenchmark; never enable nested clocks in the CAB probe.
#include <juce_dsp/juce_dsp.h>
#include "CabTimingDiagnostics.h"
#include <bit>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {try {
    if(argc!=2)throw std::runtime_error("usage: fft-probe OUTPUT");
    const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
    juce::ScopedNoDenormals noDenormals;
    std::ofstream csv(out/"fft.csv");csv<<std::setprecision(12);
    csv<<"order,batch,transforms,apple_backend,wall_us,cpu_us\n";
    std::ofstream bits(out/"fft.f32",std::ios::binary);
    for(int order:{7,8,9,10}) {
        juce::dsp::FFT fft(order);const int n=1<<order;
        constexpr int transforms=128, batches=200;
        std::vector<std::complex<float>> input(n*transforms),output(n*transforms);
        for(size_t i=0;i<input.size();++i)input[i]={float(.2*std::sin(i*.071)),float(.3*std::sin(i*.113))};
        auto work=[&] {
            for(int t=0;t<transforms;++t) {
#if CAB_APPLE_CANDIDATE
                fft.performChimeraCabForwardNoUnity(input.data()+t*n,output.data()+t*n);
#else
                fft.perform(input.data()+t*n,output.data()+t*n,false);
#endif
            }
        };
        for(int w=0;w<20;++w)work();
        for(int b=0;b<batches;++b) {
            const auto c0=cabTiming::readThreadClock();const auto w0=std::chrono::steady_clock::now();
            work();
            const auto w1=std::chrono::steady_clock::now();const auto c1=cabTiming::readThreadClock();
            if(c0.cpuUs<0 || c1.cpuUs<c0.cpuUs)throw std::runtime_error("CPU clock unavailable");
            csv<<order<<','<<b<<','<<transforms<<','<<fft.isChimeraCabAppleFFT()<<','
               <<std::chrono::duration<double,std::micro>(w1-w0).count()<<','<<c1.cpuUs-c0.cpuUs<<'\n';
        }
        bits.write(reinterpret_cast<const char*>(output.data()),std::streamsize(output.size()*sizeof(output[0])));
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
