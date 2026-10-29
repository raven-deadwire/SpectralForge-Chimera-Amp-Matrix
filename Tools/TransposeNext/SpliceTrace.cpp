// Research check: where a split engine splices and re-anchors. Processes one
// sample per call (v3 is block-invariant, so the output equals any block size)
// and prints the sample index of every transient-path splice, body-path
// splice and onset reset as JSON; writes the output like TransposeProbe.
// usage: SpliceTrace in.f32 out.f32 rate shift v2|v3|v3-sinc
#include "TransientSplitPitch.h"
#include <fstream>
#include <iostream>

int main(int argc,char** argv) {
    if(argc!=6){std::cerr<<"usage: SpliceTrace in.f32 out.f32 rate shift v2|v3|v3-sinc\n";return 1;}
    std::ifstream f(argv[1],std::ios::binary|std::ios::ate);const auto bytes=f.tellg();
    std::vector<float> x(size_t(bytes)/4),y(x.size());f.seekg(0);f.read(reinterpret_cast<char*>(x.data()),bytes);
    const double rate=std::stod(argv[3]);const int shift=std::stoi(argv[4]);const bool sinc=std::string(argv[5])=="v3-sinc",v3=sinc||std::string(argv[5])=="v3";
    juce::ScopedNoDenormals noDenormals;
    transposeNext::TransientSplitPitch e;e.prepare({rate,1u,2},true,true,true,true,true,v3);e.setSincOutput(sinc);e.setSemitones(shift);
    juce::AudioBuffer<float> in(2,1),out(2,1);std::vector<size_t> ts,bs,os;
    for(size_t n=0;n<x.size()/2;++n) {
        in.setSample(0,0,x[2*n]);in.setSample(1,0,x[2*n+1]);
        const int t0=e.transientSplices(),b0=e.bodySplices(),o0=e.onsetResets();
        e.process(in,out);
        if(e.transientSplices()!=t0)ts.push_back(n);
        if(e.bodySplices()!=b0)bs.push_back(n);
        if(e.onsetResets()!=o0)os.push_back(n);
        y[2*n]=out.getSample(0,0);y[2*n+1]=out.getSample(1,0);
    }
    std::ofstream(argv[2],std::ios::binary).write(reinterpret_cast<const char*>(y.data()),std::streamsize(y.size()*4));
    auto list=[](const std::vector<size_t>& v){std::string s="[";for(size_t i=0;i<v.size();++i)s+=(i?",":"")+std::to_string(v[i]);return s+"]";};
    std::cout<<"{\"transient_splices\":"<<list(ts)<<",\"body_splices\":"<<list(bs)<<",\"onset_resets\":"<<list(os)<<"}\n";
}
