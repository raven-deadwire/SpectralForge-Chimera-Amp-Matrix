#include "PolyPitch.h"
#include "PredictiveSpectral.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <numeric>

int main(int argc,char** argv) {
    try {
        if(argc!=8)throw std::runtime_error("Probe input.f32 output.f32 rate block shift engine callbacks.csv; stereo float32");
        double rate=std::stod(argv[3]);int block=std::stoi(argv[4]),shift=std::stoi(argv[5]);
        std::string engine=argv[6];
        if(rate<8000 || rate>192000 || block<1 || block>8192 || shift<-12 || shift>12)throw std::runtime_error("invalid configuration");
        if(engine!="baseline" && engine!="predict1024" && engine!="predict2048" && engine!="predict4096")throw std::runtime_error("unknown engine");
        std::ifstream f(argv[1],std::ios::binary|std::ios::ate);
        if(!f)throw std::runtime_error("input missing");
        const auto bytes=f.tellg();if(bytes<=0 || bytes%8)throw std::runtime_error("invalid stereo f32 size");
        std::vector<float>x(size_t(bytes)/4),y(x.size());f.seekg(0);f.read(reinterpret_cast<char*>(x.data()),bytes);
        if(!f)throw std::runtime_error("short input");for(float a:x)if(!std::isfinite(a))throw std::runtime_error("nonfinite input");
        juce::dsp::ProcessSpec spec{rate,juce::uint32(block),2};
        spectralforge::PolyPitch base;transposePredictive::PredictiveSpectral candidate;
        if(engine=="baseline"){base.prepare(spec);base.setSemitones(shift);}
        else{candidate.prepare(spec,engine=="predict1024"?10:engine=="predict2048"?11:12);candidate.setSemitones(shift);}
        juce::AudioBuffer<float> in(2,block),out(2,block);
        std::vector<double> durations;durations.reserve((x.size()/2+block-1)/block);
        std::vector<int> sizes;sizes.reserve(durations.capacity());
        for(size_t start=0;start<x.size()/2;start+=size_t(block)) {
            int n=int(std::min(size_t(block),x.size()/2-start));
            in.setSize(2,n,false,false,true);out.setSize(2,n,false,false,true);
            for(int i=0;i<n;++i)for(int c=0;c<2;++c)in.setSample(c,i,x[2*(start+size_t(i))+size_t(c)]);
            auto t=std::chrono::steady_clock::now();
            if(engine=="baseline")base.process(in,out);else candidate.process(in,out);
            durations.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count());sizes.push_back(n);
            for(int i=0;i<n;++i)for(int c=0;c<2;++c){float v=out.getSample(c,i);if(!std::isfinite(v))throw std::runtime_error("nonfinite output");y[2*(start+size_t(i))+size_t(c)]=v;}
        }
        std::ofstream o(argv[2],std::ios::binary);o.write(reinterpret_cast<const char*>(y.data()),std::streamsize(y.size()*4));
        std::ofstream csv(argv[7]);csv<<"samples,wall_us\n";for(size_t i=0;i<sizes.size();++i)csv<<sizes[i]<<','<<durations[i]<<'\n';
        if(!o || !csv)throw std::runtime_error("output write failure");
        std::cout<<"{\"reported_samples\":"<<(engine=="baseline"?base.latency():candidate.latency())<<",\"frames\":"<<x.size()/2<<"}\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
