// Renders a mono float32 file through Chimera's production Transposer
// (Source/GlobalDSP.h) for the objective half of docs/TRANSPOSE_EVIDENCE_PROTOCOL.md.
// Offline, host-sized blocks, transpose enabled for the whole file.
//
//   ChimeraTransposeRender <rate> <block> <mode> <semitones> <in.f32> <out.f32>
//     mode 0|1|2: production CLASSIC / LOW-LAT / TIGHT
//     mode 10: research corr40-split-up (Tools/TransposeNext, from 0a54c10 / #117)
//     mode 11|12|13: research low-band time-domain / upper STFT hybrid, crossover 100|150|250 Hz
//
// Prints the reported latency in samples on stdout (-1: variable, not reported).
#include "GlobalDSP.h"
#include "../TransposeNext/TransientSplitPitch.h"
#include "../TransposeNext/HybridLowBandPitch.h"
#include <fstream>
#include <iostream>

int main(int argc,char** argv) {
    if(argc!=7){std::cerr<<"usage: ChimeraTransposeRender <rate> <block> <mode 0|1|2> <semitones> <in.f32> <out.f32>\n";return 2;}
    const double rate=std::atof(argv[1]);const int block=std::atoi(argv[2]),mode=std::atoi(argv[3]),semitones=std::atoi(argv[4]);
    std::ifstream in(argv[5],std::ios::binary|std::ios::ate);
    if(!in){std::cerr<<"FAIL cannot read input\n";return 1;}
    std::vector<float> samples(size_t(in.tellg())/sizeof(float));in.seekg(0);
    in.read(reinterpret_cast<char*>(samples.data()),std::streamsize(samples.size()*sizeof(float)));
    const juce::dsp::ProcessSpec spec{rate,juce::uint32(block),2};
    spectralforge::Transposer transposer;transposeNext::TransientSplitPitch research;transposeNext::HybridLowBandPitch hybrid;
    const bool researchEngine=mode==10,hybridEngine=mode>=11 && mode<=13;
    if(researchEngine){research.prepare(spec,true,true);research.setSemitones(semitones);}
    else if(hybridEngine){hybrid.prepare(spec,mode==11?100.:mode==12?150.:250.);hybrid.setSemitones(semitones);}
    else {transposer.prepare(spec);transposer.setMode(mode);}
    juce::AudioBuffer<float> source(2,block);
    juce::AudioBuffer<float> buffer(2,block);std::vector<float> out(samples.size());
    for(size_t offset=0;offset<samples.size();offset+=size_t(block)) {
        const int n=int(std::min<size_t>(size_t(block),samples.size()-offset));
        buffer.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i)for(int c=0;c<2;++c)buffer.setSample(c,i,samples[offset+size_t(i)]);
        if(researchEngine) {source.makeCopyOf(buffer,true);research.process(source,buffer);}
        else if(hybridEngine) {source.makeCopyOf(buffer,true);hybrid.process(source,buffer);}
        else transposer.process(buffer,true,semitones);
        for(int i=0;i<n;++i)out[offset+size_t(i)]=buffer.getSample(0,i);
    }
    std::ofstream file(argv[6],std::ios::binary);file.write(reinterpret_cast<const char*>(out.data()),std::streamsize(out.size()*sizeof(float)));
    std::cout<<(researchEngine?research.latency():hybridEngine?hybrid.latency():transposer.latency())<<'\n';
    return file?0:1;
}
