#include "BaselineConvolution.h"
#include "ModeledCabConvolution.h"
#include <bit>
#include <iostream>
#include <stdexcept>

int main() {try {
    juce::ScopedNoDenormals noDenormals;
    size_t samples=0,cases=0;
    for(int maxBlock:{1,17,32,64,128,256,512})for(int channels:{1,2}) {
        const int block=juce::nextPowerOfTwo(maxBlock),segment=(block>128 ? 1 : 3)*block;
        for(int length:{1,segment-1,segment,segment+1,2*segment,2*segment+1,8160}) {
            juce::AudioBuffer<float> ir(1,length);
            for(int n=0;n<length;++n)ir.setSample(0,n,float(.002*std::sin(n*.43)+(n==length-1 ? .3 : 0.)));
            using A=spectralforge::BaselineConvolution;using B=spectralforge::ModeledCabConvolution;
            auto pa=std::make_shared<const A::Prepared>(ir,maxBlock,channels==1);
            auto pb=std::make_shared<const B::Prepared>(ir,maxBlock,channels==1);
            const auto original=pb->impulseSpectra;
            A a(pa);B b(pb);
            for(int reset=0;reset<2;++reset) {
                a.reset();b.reset();
                int cursor=0,call=0;
                while(cursor<2*length+4*block) {
                    // Includes zero blocks, subblocks, crossing internal FFT
                    // boundaries, and full host blocks, with a complete tail.
                    const int chunks[]={0,1,maxBlock,3,17,maxBlock};
                    const int count=std::min(maxBlock,chunks[call++%6]);
                    juce::AudioBuffer<float> x(channels,count),y(channels,count);
                    for(int c=0;c<channels;++c)for(int n=0;n<count;++n) {
                        const int t=cursor+n;
                        uint32_t seed=uint32_t(t+1)*747796405u+uint32_t(c)*2891336453u;
                        const float v=reset ? (t<length ? float(int(seed>>8)-0x800000)*0x1p-26f : 0.f)
                            : (t==0 ? (c ? -.7f : .5f) : (t%2 ? -0.f : 0.f));
                        x.setSample(c,n,v);y.setSample(c,n,v);
                    }
                    a.process(x);b.process(y);
                    for(int c=0;c<channels;++c)for(int n=0;n<count;++n) {
                        if(!std::isfinite(y.getSample(c,n)) || std::bit_cast<uint32_t>(x.getSample(c,n))!=std::bit_cast<uint32_t>(y.getSample(c,n)))
                            throw std::runtime_error("bit mismatch block="+std::to_string(maxBlock)+" length="+std::to_string(length)+" sample="+std::to_string(cursor+n));
                        ++samples;
                    }
                    cursor+=count;
                }
                if(original!=pb->impulseSpectra || a.getLatency()!=0 || b.getLatency()!=0)
                    throw std::runtime_error("IR mutation/latency");
                ++cases;
            }
        }
    }
    std::cout<<"PASS boundary/reset parity cases="<<cases<<" bit_exact_samples="<<samples<<" latency=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
