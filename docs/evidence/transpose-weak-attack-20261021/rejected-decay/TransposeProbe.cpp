#include "PolyPitch.h"
#include "RelativePhasePitch.h"
#include "CorrelationPitch.h"
#include "GlobalDSP.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <ctime>

double threadCpuMicroseconds() {
#if defined(__linux__)
    timespec ts{};
    if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&ts)!=0)throw std::runtime_error("thread CPU clock unavailable");
    return double(ts.tv_sec)*1.e6+double(ts.tv_nsec)*.001;
#else
    throw std::runtime_error("thread CPU timing is supported on Linux only");
#endif
}

int main(int argc,char** argv) {
    try {
        if(argc!=8 && argc!=9)throw std::runtime_error("usage: TransposeProbe in.f32 out.f32 rate block shift engine callbacks.csv [cpu]");
        const bool cpuTiming=argc==9;
        if(cpuTiming && std::string(argv[8])!="cpu")throw std::runtime_error("invalid timing flag");
        const double rate=std::stod(argv[3]);const int block=std::stoi(argv[4]),shift=std::stoi(argv[5]);
        const std::string name=argv[6];
        if(rate<8000 || rate>192000 || block<1 || block>8192 || shift<-12 || shift>12)throw std::runtime_error("invalid config");
        if(name!="baseline" && name!="relative" && name!="corr10" && name!="corr40" && name!="corr40-onset" && name!="corr40-novelty" && name!="wrapper" && name!="bypass")throw std::runtime_error("invalid engine");
        std::ifstream f(argv[1],std::ios::binary|std::ios::ate);
        if(!f)throw std::runtime_error("input missing");
        const auto bytes=f.tellg();if(bytes<=0 || bytes%8)throw std::runtime_error("expected stereo interleaved f32");
        std::vector<float> x(size_t(bytes)/4),y(x.size());f.seekg(0);f.read(reinterpret_cast<char*>(x.data()),bytes);
        if(!f)throw std::runtime_error("short read");
        for(float v:x)if(!std::isfinite(v))throw std::runtime_error("nonfinite input");
        const juce::dsp::ProcessSpec spec{rate,juce::uint32(block),2};
        spectralforge::PolyPitch baseline;transposeResearch::PolyPitch relative;
        transposeNext::CorrelationPitch correlated;spectralforge::Transposer wrapper;
        int nominal=-1;
        if(name=="baseline"){baseline.prepare(spec);baseline.setSemitones(shift);nominal=baseline.latency();}
        else if(name=="relative"){relative.prepare(spec);relative.setSemitones(shift);nominal=relative.latency();}
        else if(name=="wrapper" || name=="bypass"){wrapper.prepare(spec);nominal=name=="wrapper"&&shift!=0?wrapper.latency():0;}
        else {correlated.prepare(spec,name=="corr10"?10:40,name=="corr40-onset" || name=="corr40-novelty",name=="corr40-novelty");correlated.setSemitones(shift);}
        juce::AudioBuffer<float> in(2,block),out(2,block);
        const size_t callbacks=(x.size()/2+size_t(block)-1)/size_t(block);
        std::vector<double> times(callbacks),cpuTimes(callbacks);std::vector<int> counts(callbacks);
        size_t callback=0,misses=0;
        for(size_t off=0;off<x.size()/2;off+=size_t(block),++callback) {
            const int n=int(std::min(size_t(block),x.size()/2-off));
            in.setSize(2,n,false,false,true);out.setSize(2,n,false,false,true);
            for(int i=0;i<n;++i)for(int c=0;c<2;++c)in.setSample(c,i,x[2*(off+size_t(i))+size_t(c)]);
            const auto start=std::chrono::steady_clock::now();
            const double cpuStart=cpuTiming?threadCpuMicroseconds():0;
            if(name=="baseline")baseline.process(in,out);
            else if(name=="relative")relative.process(in,out);
            else if(name=="wrapper" || name=="bypass") {out.makeCopyOf(in,true);wrapper.process(out,name=="wrapper"&&shift!=0,shift);}
            else correlated.process(in,out);
            if(cpuTiming)cpuTimes[callback]=threadCpuMicroseconds()-cpuStart;
            const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
            times[callback]=us;counts[callback]=n;if(us>n/rate*1e6)++misses;
            for(int i=0;i<n;++i)for(int c=0;c<2;++c) {
                const float v=out.getSample(c,i);if(!std::isfinite(v))throw std::runtime_error("nonfinite output");
                y[2*(off+size_t(i))+size_t(c)]=v;
            }
        }
        std::ofstream o(argv[2],std::ios::binary);o.write(reinterpret_cast<const char*>(y.data()),std::streamsize(y.size()*4));
        std::ofstream csv(argv[7]);csv<<"samples,microseconds,thread_cpu_us\n";
        for(size_t i=0;i<times.size();++i)csv<<counts[i]<<','<<times[i]<<','<<cpuTimes[i]<<'\n';
        if(!o || !csv)throw std::runtime_error("output failure");
        const double mean=std::accumulate(times.begin(),times.end(),0.)/times.size();
        std::sort(times.begin(),times.end());
        auto q=[&](double p){return times[size_t(p*(times.size()-1))];};
        std::cout<<"{\"nominal_samples\":"<<nominal<<",\"mean_us\":"<<mean
          <<",\"p95_us\":"<<q(.95)<<",\"p99_us\":"<<q(.99)<<",\"max_us\":"<<times.back()
          <<",\"thread_cpu_measured\":"<<(cpuTiming?"true":"false")
          <<",\"thread_cpu_mean_us\":"<<std::accumulate(cpuTimes.begin(),cpuTimes.end(),0.)/cpuTimes.size()
          <<",\"deadline_misses\":"<<misses<<",\"callbacks\":"<<times.size()
          <<",\"splices\":"<<correlated.splices()<<",\"onset_resets\":"<<correlated.onsetResets()<<",\"read_min_samples\":"<<correlated.readMinimum()
          <<",\"read_max_samples\":"<<correlated.readMaximum()<<"}\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
