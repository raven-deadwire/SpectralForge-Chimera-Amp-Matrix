// Diagnostic: per splice, jump length (nextDelay-delay) of both split paths.
// usage: JumpTrace in.f32 out.f32 rate shift v3|v3-sinc
#include "TransientSplitPitch.h"
#include <fstream>
#include <iostream>
#include <cstdio>
namespace transposeNext {
struct SplitDiagnostics {
    static CorrelationPitch& body(TransientSplitPitch& e){return e.body;}
    static CorrelationPitch& transient(TransientSplitPitch& e){return e.transient;}
};
struct CorrelationDiagnostics {
    static double delay(const CorrelationPitch& p){return p.delay;}
    static double next(const CorrelationPitch& p){return p.nextDelay;}
};
}
using namespace transposeNext;
int main(int argc,char** argv){
    if(argc!=6){std::cerr<<"usage: JumpTrace in.f32 out.f32 rate shift v3|v3-sinc|v3-sinc-edge\n";return 1;}
    std::ifstream f(argv[1],std::ios::binary|std::ios::ate);const auto bytes=f.tellg();
    if(!f || bytes<0){std::cerr<<"cannot read "<<argv[1]<<"\n";return 1;}
    std::vector<float> x(size_t(bytes)/4),y(x.size());f.seekg(0);f.read(reinterpret_cast<char*>(x.data()),bytes);
    const double rate=std::stod(argv[3]);const int shift=std::stoi(argv[4]);const std::string eng=argv[5];const bool sinc=eng.rfind("v3-sinc",0)==0;
    juce::ScopedNoDenormals nd;
    TransientSplitPitch e;e.prepare({rate,1u,2},true,true,true,true,true,true);e.setSincOutput(sinc);e.setEdgeReject(eng=="v3-sinc-edge");e.setSemitones(shift);
    juce::AudioBuffer<float> in(2,1),out(2,1);
    auto& T=SplitDiagnostics::transient(e);auto& B=SplitDiagnostics::body(e);
    for(size_t n=0;n<x.size()/2;++n){
        in.setSample(0,0,x[2*n]);in.setSample(1,0,x[2*n+1]);
        const int t0=T.splices(),b0=B.splices();
        e.process(in,out);
        if(T.splices()!=t0)std::printf("T %zu %.6f %.6f\n",n,CorrelationDiagnostics::delay(T),CorrelationDiagnostics::next(T));
        if(B.splices()!=b0)std::printf("B %zu %.6f %.6f\n",n,CorrelationDiagnostics::delay(B),CorrelationDiagnostics::next(B));
        y[2*n]=out.getSample(0,0);y[2*n+1]=out.getSample(1,0);
    }
    std::ofstream(argv[2],std::ios::binary).write(reinterpret_cast<const char*>(y.data()),std::streamsize(y.size()*4));
}
