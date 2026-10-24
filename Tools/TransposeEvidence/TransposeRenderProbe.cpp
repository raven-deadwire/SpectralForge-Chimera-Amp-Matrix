// Renders a mono float32 file through Chimera's production Transposer
// (Source/GlobalDSP.h) for the objective half of docs/TRANSPOSE_EVIDENCE_PROTOCOL.md.
// Offline, host-sized blocks, transpose enabled for the whole file.
//
//   ChimeraTransposeRender <rate> <block> <mode> <semitones> <in.f32> <out.f32>
//     mode 0|1|2: production CLASSIC / LOW-LAT / TIGHT
//     mode 10: research corr40-split-up (Tools/TransposeNext, from 0a54c10 / #117)
//     mode 11|12|13: research low-band time-domain / upper STFT hybrid, crossover 100|150|250 Hz
//     mode 14|15|16: 150 Hz hybrid, low-band onset re-anchor, delay bound 40|20|12 ms
//     mode 17: mode 14 with low-band attacks anchored at 10 ms (aligned with the STFT band)
//     mode 18: mode 17 with the downshift anchor scaled by the pitch ratio
//     mode 19: mode 18 with a 0.5 ms onset crossfade and 0.5 ms onset delay floor
//     mode 20: mode 19, short onset fade only where the aligned anchor is floor-bound
//     mode 21|22: mode 20, low band only | upper band only (crossover diagnostics)
//     mode 23|24|25: mode 20|21|22 with an LR8 crossover
//     mode 26|27|28: mode 20|21|22 with the STFT band phase-locked to the low band
//     mode 29: mode 26 with low-band attacks aligned at 9 ms
//     mode 30|31: mode 29, low band only | upper band only
//     mode 32|33|34|35: mode 20 (AL10RA) with the crossover at 200|250|300|400 Hz
//     mode 36|37|38: mode 20 with the crossover at 600|800|1200 Hz
//     mode 39: mode 37 (AL10RA-800) with the attack-latency contract (crossover group delay
//              compensated, latency() = the STFT window, lead 1.4 ms)
//     mode 40: mode 39 with legato re-anchoring (LegatoDetector)
//     mode 41|42|43: mode 40 with phase-aligned re-anchors, +-2.5 ms legato only | +-2.5 ms picks too | +-6 ms picks too
//
// Prints the reported latency in samples on stdout (-1: variable, not reported).
// Options after <out.f32>:
//   timing            per-block process() time on stderr: mean / p99.9 / max us
//   switch=<s>@<sec>  change to <s> semitones at <sec> seconds (automation)
//   nodenormals       flush denormals like the plug-in's processBlock (ScopedNoDenormals)
//   stats             research engine counters on stderr (hybrid: legato events)
//   delays=<path>     hybrid: low-band read delay per sample (float32), plus the
//                     crossover group delay in samples on stderr
#include "GlobalDSP.h"
#include "../TransposeNext/TransientSplitPitch.h"
#include "../TransposeNext/HybridLowBandPitch.h"
#include <algorithm>
#include <cstdlib>
#include <optional>
#include <chrono>
#include <fstream>
#include <string>
#include <iostream>

int main(int argc,char** argv) {
    if(argc<7){std::cerr<<"usage: ChimeraTransposeRender <rate> <block> <mode> <semitones> <in.f32> <out.f32> [timing] [switch=<s>@<sec>]\n";return 2;}
    bool timing=false,flush=false,stats=false;std::string delayPath;int switchTo=0;double switchAt=-1;
    for(int a=7;a<argc;++a) {
        const std::string option(argv[a]);
        if(option=="timing")timing=true;
        else if(option=="nodenormals")flush=true;
        else if(option=="stats")stats=true;
        else if(option.rfind("delays=",0)==0)delayPath=option.substr(7);
        else if(option.rfind("switch=",0)==0){const auto at=option.find('@');switchTo=std::atoi(option.substr(7,at-7).c_str());switchAt=std::atof(option.substr(at+1).c_str());}
        else {std::cerr<<"unknown option "<<option<<"\n";return 2;}
    }
    const double rate=std::atof(argv[1]);const int block=std::atoi(argv[2]),mode=std::atoi(argv[3]),semitones=std::atoi(argv[4]);
    std::ifstream in(argv[5],std::ios::binary|std::ios::ate);
    if(!in){std::cerr<<"FAIL cannot read input\n";return 1;}
    std::vector<float> samples(size_t(in.tellg())/sizeof(float));in.seekg(0);
    in.read(reinterpret_cast<char*>(samples.data()),std::streamsize(samples.size()*sizeof(float)));
    const juce::dsp::ProcessSpec spec{rate,juce::uint32(block),2};
    spectralforge::Transposer transposer;transposeNext::TransientSplitPitch research;transposeNext::HybridLowBandPitch hybrid;
    const bool researchEngine=mode==10,hybridEngine=mode>=11 && mode<=43;
    if(researchEngine){research.prepare(spec,true,true);research.setSemitones(semitones);}
    else if(hybridEngine){
        // Research hybrid variants; see the mode list above.
        const bool sweep=mode>=32;                                   // AL10RA at another crossover
        const bool aligned=mode>=17, compensated=mode>=18, shortFade=mode>=19, adaptive=mode>=20;
        const bool lr8=mode>=23 && mode<=25, lock=mode>=26 && mode<=31;
        const double crossover=mode==11?100.:mode==13?250.:mode==32?200.:mode==33?250.:mode==34?300.:mode==35?400.:mode==36?600.:mode==37?800.:mode==38?1200.:mode>=39?800.:150.;
        const double align=aligned ? (mode>=29 && mode<=31 ? 9. : 10.) : 0.;
        const int solo=sweep ? 0 : mode>=30 ? mode-29 : mode>=27 && mode<=28 ? mode-26 : mode>=24 && mode<=25 ? mode-23 : mode>=21 && mode<=22 ? mode-20 : 0;
        hybrid.prepare(spec,crossover,mode==15?20.:mode==16?12.:40.,mode>=14,align,compensated,shortFade?.5:0.,2.9,adaptive,lr8,lock);
        if(mode>=39)hybrid.setAttackLatency(1.4);
        if(mode>=40)hybrid.setLegatoReanchor(true);
        if(mode>=41)hybrid.setReanchorPhaseSearch(mode==43?6.:2.5,mode>=42);   // lead measured by a lead sweep (docs/TRANSPOSE_HYBRID_LATENCY_CONTRACT.md)
        hybrid.setSolo(solo);hybrid.setSemitones(semitones);
    }
    else {transposer.prepare(spec);transposer.setMode(mode);}
    juce::AudioBuffer<float> source(2,block);
    juce::AudioBuffer<float> buffer(2,block);std::vector<float> out(samples.size());
    std::vector<double> blockMicros;if(timing)blockMicros.reserve(samples.size()/size_t(block)+1);
    int current=semitones;std::vector<float> readDelays;if(!delayPath.empty())readDelays.reserve(samples.size());
    for(size_t offset=0;offset<samples.size();offset+=size_t(block)) {
        const int n=int(std::min<size_t>(size_t(block),samples.size()-offset));
        if(switchAt>=0 && current!=switchTo && double(offset)>=switchAt*rate) {
            current=switchTo;research.setSemitones(current);hybrid.setSemitones(current);
        }
        buffer.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i)for(int c=0;c<2;++c)buffer.setSample(c,i,samples[offset+size_t(i)]);
        source.makeCopyOf(buffer,true);
        std::optional<juce::ScopedNoDenormals> noDenormals;if(flush)noDenormals.emplace();
        const auto start=std::chrono::steady_clock::now();
        if(researchEngine) research.process(source,buffer);
        else if(hybridEngine) hybrid.process(source,buffer);
        else transposer.process(buffer,true,current);
        if(timing)blockMicros.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
        for(int i=0;i<n;++i)out[offset+size_t(i)]=buffer.getSample(0,i);
        if(!delayPath.empty() && hybridEngine && n<=block)for(int i=0;i<n;++i)readDelays.push_back(float(hybrid.lastReadDelays()[i]));
    }
    std::ofstream file(argv[6],std::ios::binary);file.write(reinterpret_cast<const char*>(out.data()),std::streamsize(out.size()*sizeof(float)));
    if(timing && std::getenv("CHIMERA_TIMING_DUMP")!=nullptr) {   // research: raw per-block times, one per line
        std::ofstream dump(std::getenv("CHIMERA_TIMING_DUMP"));for(double v:blockMicros)dump<<v<<'\n';
    }
    if(timing && !blockMicros.empty()) {
        double sum=0;for(double v:blockMicros)sum+=v;
        std::vector<double> sorted(blockMicros);std::sort(sorted.begin(),sorted.end());
        std::cerr<<"timing blocks="<<sorted.size()<<" mean_us="<<sum/double(sorted.size())
                 <<" p999_us="<<sorted[size_t(double(sorted.size()-1)*.999)]<<" max_us="<<sorted.back()
                 <<" budget_us="<<1.e6*block/rate
                 <<" over_half="<<std::count_if(sorted.begin(),sorted.end(),[&](double v){return v>.5e6*block/rate;})
                 <<" over_budget="<<std::count_if(sorted.begin(),sorted.end(),[&](double v){return v>1.e6*block/rate;})<<'\n';
    }
    if(stats && hybridEngine)std::cerr<<"stats legato_events="<<hybrid.legatoEvents()<<" onset_resets="<<hybrid.onsetResets()<<'\n';
    if(!delayPath.empty()){std::ofstream d(delayPath,std::ios::binary);d.write(reinterpret_cast<const char*>(readDelays.data()),std::streamsize(readDelays.size()*sizeof(float)));
        std::cerr<<"crossover_delay_samples="<<(hybridEngine?hybrid.crossoverDelaySamples():0.)<<'\n';}
    std::cout<<(researchEngine?research.latency():hybridEngine?hybrid.latency():transposer.latency())<<'\n';
    return file?0:1;
}
