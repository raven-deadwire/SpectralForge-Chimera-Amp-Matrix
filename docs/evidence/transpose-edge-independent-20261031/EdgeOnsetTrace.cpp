// Read-only event trace. All CSV I/O occurs outside process(), never in DSP.
// usage: OnsetTrace input.f32 rate shift v3|v4
#include "TransientSplitPitch.h"
#include <fstream>
#include <iostream>
#include <iomanip>
namespace transposeNext {
struct CorrelationDiagnostics {
    static void emit(const CorrelationPitch& p) {
        std::cout << ',' << p.delay << ',' << p.nextDelay << ',' << p.minimum
                  << ',' << p.onsetAge << ',' << p.fading;
    }
};
struct SplitDiagnostics {
    static void emit(const TransientSplitPitch& p) {
        CorrelationDiagnostics::emit(p.body);
        std::cout << ',' << p.lowFast << ',' << p.lowThreshold << ','
                  << p.predictor.fraction() << ',' << p.fractionThreshold;
    }
    static bool detected(const TransientSplitPitch& p) {return p.broadFlags[0]!=0;}
    static int applied(const TransientSplitPitch& p) {return p.body.onsetResets();}
};
}
int main(int argc,char** argv) {
    try {
        if(argc!=5)throw std::runtime_error("usage: OnsetTrace input.f32 rate shift v3|v4");
        std::ifstream f(argv[1],std::ios::binary|std::ios::ate);
        if(!f)throw std::runtime_error("input missing");
        const auto bytes=f.tellg();if(bytes<=0 || bytes%8)throw std::runtime_error("invalid stereo f32");
        std::vector<float> x(size_t(bytes)/4);f.seekg(0);f.read(reinterpret_cast<char*>(x.data()),bytes);
        const std::string version=argv[4];if(version!="v3"&&version!="v3-edge"&&version!="v3-sinc"&&version!="v3-sinc-edge")throw std::runtime_error("invalid engine");
        const double rate=std::stod(argv[2]);const int shift=std::stoi(argv[3]);
        transposeNext::TransientSplitPitch p;p.prepare({rate,1,2},true,true,true,true,true,true,false);p.setSincOutput(version=="v3-sinc"||version=="v3-sinc-edge");p.setEdgeReject(version=="v3-edge"||version=="v3-sinc-edge");p.setSemitones(shift);
        juce::AudioBuffer<float> in(2,1),out(2,1);
        int high=0,confirmed=0,applied=0,splices=0;
        std::cout<<std::setprecision(12)<<"sample,event,delay_samples,next_delay_samples,minimum_samples,onset_age_samples,fading,low_energy,energy_threshold,prediction_fraction,fraction_threshold\n";
        for(size_t i=0;i<x.size()/2;++i) {
            for(int c=0;c<2;++c)in.setSample(c,0,x[2*i+size_t(c)]);
            p.process(in,out);
            auto emit=[&](const char* event){std::cout<<i<<','<<event;transposeNext::SplitDiagnostics::emit(p);std::cout<<'\n';};
            if(transposeNext::SplitDiagnostics::detected(p))emit("detected");
            if(p.onsetResets()!=high){high=p.onsetResets();emit("high_reset");}
            if(p.lowOnsetResets()!=confirmed){confirmed=p.lowOnsetResets();emit("low_confirm");}
            if(transposeNext::SplitDiagnostics::applied(p)!=applied){applied=transposeNext::SplitDiagnostics::applied(p);emit("low_reset");}
            if(p.bodySplices()!=splices){splices=p.bodySplices();emit("body_splice");}
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
