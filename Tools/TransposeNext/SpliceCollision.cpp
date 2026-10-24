// Research check (T5): how often both split paths run a splice search in the
// same host callback, and the per-callback thread CPU time, for a fixed block.
// The coordinated hold exists to keep the two ~0.1-0.5 ms searches apart.
// usage: SpliceCollision in.f32 rate block shift v2|v3
#include "TransientSplitPitch.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <iostream>

static double cpuUs(){timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return t.tv_sec*1e6+t.tv_nsec*1e-3;}

int main(int argc,char** argv) {
    if(argc!=6){std::cerr<<"usage: SpliceCollision in.f32 rate block shift v2|v3\n";return 1;}
    std::ifstream f(argv[1],std::ios::binary|std::ios::ate);const auto bytes=f.tellg();
    std::vector<float> x(size_t(bytes)/4);f.seekg(0);f.read(reinterpret_cast<char*>(x.data()),bytes);
    const double rate=std::stod(argv[2]);const int block=std::stoi(argv[3]),shift=std::stoi(argv[4]);const bool v3=std::string(argv[5])=="v3";
    juce::ScopedNoDenormals noDenormals;
    transposeNext::TransientSplitPitch e;e.prepare({rate,juce::uint32(block),2},true,true,true,true,true,v3);e.setSemitones(shift);
    juce::AudioBuffer<float> in(2,block),out(2,block);
    const size_t frames=x.size()/2;int callbacks=0,both=0,bodyOnly=0,transientOnly=0;std::vector<double> times;
    for(size_t off=0;off<frames;off+=size_t(block)) {
        const int n=int(std::min<size_t>(size_t(block),frames-off));
        in.setSize(2,n,false,false,true);out.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i)for(int c=0;c<2;++c)in.setSample(c,i,x[2*(off+size_t(i))+size_t(c)]);
        const int b0=e.bodySplices(),t0=e.transientSplices();const double c0=cpuUs();
        e.process(in,out);
        times.push_back(cpuUs()-c0);++callbacks;
        const bool b=e.bodySplices()!=b0,t=e.transientSplices()!=t0;
        both+=b&&t;bodyOnly+=b&&!t;transientOnly+=t&&!b;
    }
    std::sort(times.begin(),times.end());
    std::cout<<"{\"callbacks\":"<<callbacks<<",\"both_searched\":"<<both<<",\"body_only\":"<<bodyOnly<<",\"transient_only\":"<<transientOnly
             <<",\"cpu_max_us\":"<<times.back()<<",\"cpu_p99_us\":"<<times[size_t(.99*(times.size()-1))]<<"}\n";
}
