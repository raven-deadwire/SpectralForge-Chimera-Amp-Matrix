#include "MultibandBypassCandidate.h"
#include <chrono>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
using namespace spectralforge;
double cpuNow(){
#if defined(CLOCK_THREAD_CPUTIME_ID)
    timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return double(t.tv_sec)+double(t.tv_nsec)*1.e-9;
#else
    return double(std::clock())/CLOCKS_PER_SEC;
#endif
}
template<class Core> int run(double sr,int channels,int block,int count,const std::string& mode){
    Core core;MultibandSettings s;s.on=mode!="off";
    core.setSettings(s);core.prepare(sr);
    std::array<std::vector<float>,2> input{std::vector<float>(size_t(block)),std::vector<float>(size_t(block))},buffer=input;
    for(int n=0;n<block;++n){input[0][size_t(n)]=float(.2*std::sin(2.*multiband_detail::pi*700*n/sr)+.1*std::sin(2.*multiband_detail::pi*27.5*n/sr));input[1][size_t(n)]=-.7f*input[0][size_t(n)];}
    float* p[]{buffer[0].data(),buffer[1].data()};
    volatile float checksum=0.f;
    std::vector<double> wall(size_t(count),0.),cpu(size_t(count),0.);
    const int cadence=std::max(1,int(sr*.25)/block);
    for(int b=-100;b<count;++b){
        for(int c=0;c<channels;++c)std::copy(input[size_t(c)].begin(),input[size_t(c)].end(),buffer[size_t(c)].begin());
        if(mode=="toggle")s.on=(std::max(0,b)/cadence)%2==0;
        const auto t=std::chrono::steady_clock::now();const auto ct=cpuNow();
        core.setSettings(s);core.process(p,channels,block);
        const double elapsed=cpuNow()-ct;
        const double w=std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count();
        checksum=checksum+p[0][0];
        if(b>=0){wall[size_t(b)]=w*1.e6;cpu[size_t(b)]=elapsed*1.e6;}
    }
    std::cout<<"{\"checksum\":"<<checksum<<",\"wall_us\":[";
    for(int i=0;i<count;++i)std::cout<<(i?",":"")<<wall[size_t(i)];
    std::cout<<"],\"cpu_us\":[";
    for(int i=0;i<count;++i)std::cout<<(i?",":"")<<cpu[size_t(i)];
    std::cout<<"]}\n";return 0;
}
int main(int argc,char** argv){if(argc!=7)return 2;
    if(std::string(argv[1])=="candidate")return run<research::MultibandBypassCandidate>(std::stod(argv[2]),std::stoi(argv[3]),std::stoi(argv[4]),std::stoi(argv[5]),argv[6]);
    if(std::string(argv[1])=="baseline")return run<MultibandDynamicsCore>(std::stod(argv[2]),std::stoi(argv[3]),std::stoi(argv[4]),std::stoi(argv[5]),argv[6]);
    return 2;
}
