// Per-sample read-only trace; all file I/O is outside DSP process().
#include "EnvelopeResearch.h"
#include <fstream>
#include <iostream>
#include <iomanip>
int main(int argc,char** argv) {
    try {
        if(argc!=5)throw std::runtime_error("usage: EnvelopeTrace input.f32 rate shift engine");
        std::ifstream f(argv[1],std::ios::binary|std::ios::ate);
        if(!f)throw std::runtime_error("missing input");
        const auto bytes=f.tellg();if(bytes<=0||bytes%8)throw std::runtime_error("invalid input");
        std::vector<float> x(size_t(bytes)/4);f.seekg(0);f.read(reinterpret_cast<char*>(x.data()),bytes);
        const double rate=std::stod(argv[2]);const int shift=std::stoi(argv[3]);const std::string engine=argv[4];
        transposeNext::TransientSplitPitch p;
        p.prepare({rate,1,2},true,true,true,true,true,true,engine!="corr40-split-v3");
        transposeNext::SplitDiagnostics::configure(p,engine,shift);p.setSemitones(shift);
        juce::AudioBuffer<float> in(2,1),out(2,1);
        std::cout<<std::setprecision(17)<<"sample,input,output,old_age,next_age,weight,old_tap,new_tap,reconstructed,age_since_reset,resets,splices,fade_index,fade_length,body_input,body_output,high_output,low_confirms,high_resets\n";
        for(size_t n=0;n<x.size()/2;++n) {
            for(int c=0;c<2;++c)in.setSample(c,0,x[2*n+size_t(c)]);
            const auto b=transposeNext::SplitDiagnostics::before(p);p.process(in,out);
            std::cout<<n<<','<<x[2*n]<<','<<out.getSample(0,0);transposeNext::SplitDiagnostics::emit(std::cout,p,b);std::cout<<'\n';
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
