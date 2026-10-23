// Renders a mono float32 file through Chimera's production Transposer
// (Source/GlobalDSP.h) for the objective half of docs/TRANSPOSE_EVIDENCE_PROTOCOL.md.
// Offline, host-sized blocks, transpose enabled for the whole file.
//
//   ChimeraTransposeRender <rate> <block> <mode 0|1|2> <semitones> <in.f32> <out.f32>
//
// Prints the reported latency in samples on stdout.
#include "GlobalDSP.h"
#include <fstream>
#include <iostream>

int main(int argc,char** argv) {
    if(argc!=7){std::cerr<<"usage: ChimeraTransposeRender <rate> <block> <mode 0|1|2> <semitones> <in.f32> <out.f32>\n";return 2;}
    const double rate=std::atof(argv[1]);const int block=std::atoi(argv[2]),mode=std::atoi(argv[3]),semitones=std::atoi(argv[4]);
    std::ifstream in(argv[5],std::ios::binary|std::ios::ate);
    if(!in){std::cerr<<"FAIL cannot read input\n";return 1;}
    std::vector<float> samples(size_t(in.tellg())/sizeof(float));in.seekg(0);
    in.read(reinterpret_cast<char*>(samples.data()),std::streamsize(samples.size()*sizeof(float)));
    spectralforge::Transposer transposer;
    transposer.prepare({rate,juce::uint32(block),2});transposer.setMode(mode);
    juce::AudioBuffer<float> buffer(2,block);std::vector<float> out(samples.size());
    for(size_t offset=0;offset<samples.size();offset+=size_t(block)) {
        const int n=int(std::min<size_t>(size_t(block),samples.size()-offset));
        buffer.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i)for(int c=0;c<2;++c)buffer.setSample(c,i,samples[offset+size_t(i)]);
        transposer.process(buffer,true,semitones);
        for(int i=0;i<n;++i)out[offset+size_t(i)]=buffer.getSample(0,i);
    }
    std::ofstream file(argv[6],std::ios::binary);file.write(reinterpret_cast<const char*>(out.data()),std::streamsize(out.size()*sizeof(float)));
    std::cout<<transposer.latency()<<'\n';
    return file?0:1;
}
