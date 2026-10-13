#include "NAM/get_dsp.h"
#include "NAM/wavenet/a2_fast.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>

// Loads unchanged weights into changed histories only to measure format/runtime.
// Its output is deliberately NOT an acoustic candidate.
int main(int argc, char** argv) try {
    if (argc != 5) throw std::runtime_error("model input.f32 output.f32 block");
    nlohmann::json j; std::ifstream(argv[1]) >> j;
    int channels=0; const bool a2=nam::wavenet::a2_fast::is_a2_shape(j.at("config"), &channels);
    auto model=nam::get_dsp(std::filesystem::path(argv[1]));
    const int block=std::stoi(argv[4]);
    if (block<1 || block>4096) throw std::runtime_error("block range");
    std::ifstream f(argv[2],std::ios::binary|std::ios::ate);
    const auto bytes=f.tellg(); if(bytes<=0 || bytes%4)throw std::runtime_error("input");
    std::vector<float> raw(size_t(bytes)/4), rendered(raw.size()); f.seekg(0);f.read((char*)raw.data(),bytes);
    model->Reset(48000,block);
    std::vector<NAM_SAMPLE> in(block),out(block); std::vector<double> times;
    for(size_t p=0;p<raw.size();p+=block){
        const int count=int(std::min(size_t(block),raw.size()-p));
        for(int k=0;k<count;++k)in[k]=raw[p+k];
        NAM_SAMPLE* inputs[]{in.data()};NAM_SAMPLE* outputs[]{out.data()};
        const auto start=std::chrono::steady_clock::now();model->process(inputs,outputs,count);
        times.push_back(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count());
        for(int k=0;k<count;++k){if(!std::isfinite(out[k]))throw std::runtime_error("nonfinite");rendered[p+k]=float(out[k]);}
    }
    std::ofstream o(argv[3],std::ios::binary);o.write((char*)rendered.data(),rendered.size()*4);
    if(!o)throw std::runtime_error("output");
    std::sort(times.begin(),times.end());double sum=0;int misses=0;
    for(auto t:times){sum+=t;misses+=t>double(block)/48000;}
    nlohmann::json result={{"a2_gate",a2},{"block",block},{"samples",raw.size()},
        {"mean_seconds",sum/times.size()},{"p99_seconds",times[size_t(.99*(times.size()-1))]},
        {"deadline_misses",misses},{"audio_seconds",raw.size()/48000.},{"processing_seconds",sum},
        {"release_approved",false},{"scope","format and offline CPU only; changed dilation weights are not trained"}};
    std::cout<<result.dump()<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
