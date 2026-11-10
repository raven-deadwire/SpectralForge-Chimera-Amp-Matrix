// Research check: does a split engine's output depend on how the host splits
// blocks? Renders one input with fixed and seeded random block sizes and prints
// the largest difference from the fixed-128 render, per pattern (JSON).
// usage: BlockInvariance in.f32 rate shift v2|v3|v4|v3-sinc
#include "TransientSplitPitch.h"
#include <fstream>
#include <iostream>
#include <random>
#include <bit>

namespace {
std::vector<float> x;
bool sincOutput=false,edgeReject=false; // research options, both OFF by default
template<class Next> std::vector<float> render(double rate,int shift,int version,int maxBlock,Next next) {
    juce::ScopedNoDenormals noDenormals;
    transposeNext::TransientSplitPitch e;
    e.prepare({rate,juce::uint32(maxBlock),2},true,true,true,true,true,version>=3,version>=4);e.setSincOutput(sincOutput);e.setEdgeReject(edgeReject);e.setSemitones(shift);
    const size_t frames=x.size()/2;std::vector<float> y(x.size());
    juce::AudioBuffer<float> in(2,maxBlock),out(2,maxBlock);
    for(size_t off=0;off<frames;) {
        const int n=int(std::min<size_t>(size_t(next()),frames-off));
        in.setSize(2,n,false,false,true);out.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i)for(int c=0;c<2;++c)in.setSample(c,i,x[2*(off+size_t(i))+size_t(c)]);
        e.process(in,out);
        for(int i=0;i<n;++i)for(int c=0;c<2;++c)y[2*(off+size_t(i))+size_t(c)]=out.getSample(c,i);
        off+=size_t(n);
    }
    return y;
}
double maxDiff(const std::vector<float>& a,const std::vector<float>& b) {
    double d=0;for(size_t i=0;i<a.size();++i) {
        if(!std::isfinite(a[i]) || !std::isfinite(b[i]))throw std::runtime_error("nonfinite output");
        if(a[i]==b[i] && std::bit_cast<uint32_t>(a[i])!=std::bit_cast<uint32_t>(b[i]))
            throw std::runtime_error("signed-zero bit mismatch");
        d=std::max(d,double(std::abs(a[i]-b[i])));
    }return d;
}
}

int main(int argc,char** argv) {
    if(argc!=5){std::cerr<<"usage: BlockInvariance in.f32 rate shift v2|v3|v4|v3-sinc\n";return 1;}
    std::ifstream f(argv[1],std::ios::binary|std::ios::ate);const auto bytes=f.tellg();
    if(!f || bytes<=0 || bytes%8){std::cerr<<"expected nonempty stereo f32 input\n";return 1;}
    x.resize(size_t(bytes)/4);f.seekg(0);f.read(reinterpret_cast<char*>(x.data()),bytes);
    if(!f || !std::all_of(x.begin(),x.end(),[](float v){return std::isfinite(v);})){std::cerr<<"invalid input\n";return 1;}
    const double rate=std::stod(argv[2]);const int shift=std::stoi(argv[3]);const std::string name=argv[4];
    if(name!="v2" && name!="v3" && name!="v4" && name!="v3-sinc" && name!="v3-edge" && name!="v3-sinc-edge")return 1;
    if(!std::isfinite(rate)||rate<8000||rate>192000||shift<-12||shift>12)return 1;
    sincOutput=name=="v3-sinc"||name=="v3-sinc-edge";
    edgeReject=name=="v3-edge"||name=="v3-sinc-edge";
    const int version=name=="v4"?4:(name=="v3"||sincOutput||edgeReject)?3:2;
    const auto ref=render(rate,shift,version,128,[]{return 128;});
    std::cout<<"{";bool first=true;
    auto emit=[&](const std::string& k,double v){std::cout<<(first?"":",")<<'"'<<k<<"\":"<<v;first=false;};
    for(int b:{1,37,64,100,256,512,1024,2048,4096})emit("fixed"+std::to_string(b),maxDiff(render(rate,shift,version,b,[b]{return b;}),ref));
    for(unsigned seed:{1u,2u,3u}) {
        std::mt19937 r(seed);std::uniform_int_distribution<int> d(1,512);
        emit("random512_seed"+std::to_string(seed),maxDiff(render(rate,shift,version,512,[&]{return d(r);}),ref));
    }
    std::mt19937 r(9);std::uniform_int_distribution<int> d(1,4096);
    emit("random4096",maxDiff(render(rate,shift,version,4096,[&]{return d(r);}),ref));
    std::mt19937 fresh(2026103113u);
    emit("random4096_seed2026103113",maxDiff(render(rate,shift,version,4096,[&]{return d(fresh);}),ref));
    std::cout<<"}\n";
}
