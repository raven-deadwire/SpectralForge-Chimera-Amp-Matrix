#include "OriginalAmpDSP.h"
#include "HybridTeacher.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <vector>

int main(int argc,char** argv) try {
    if(argc!=3)throw std::runtime_error("input.f32 channel-settings.json");
    std::ifstream f(argv[1],std::ios::binary|std::ios::ate);
    const auto bytes=f.tellg();if(bytes<=0 || bytes%4)throw std::runtime_error("input");
    std::vector<float> x(size_t(bytes)/4);f.seekg(0);f.read((char*)x.data(),bytes);
    nlohmann::json settings;std::ifstream(argv[2])>>settings;
    nlohmann::json results=nlohmann::json::array();
    for(double rate:{48000.,192000.})for(const auto& job:settings){
        spectralforge::original::State s;hybrid_research::original::State h;
        s.channel=h.channel=job.at("channel");s.modern=h.modern=true;
        for(size_t k=0;k<s.values.size();++k){
            auto id=spectralforge::original::controls[k].id;
            s.values[k]=h.values[k]=job.at("controls").at(id).get<float>();
        }
        spectralforge::original::OriginalAmpDSP native;
        hybrid_research::original::HybridTeacher hybrid;
        native.prepare(rate);hybrid.prepare(rate);native.set(s);hybrid.set(h);native.reset();hybrid.reset();
        double maximum=0;size_t checked=0;
        for(size_t n=0;n<x.size();++n){
            if(n==x.size()/3){s.values[0]*=.8f;h.values[0]=s.values[0];native.set(s);hybrid.set(h);}
            if(n==x.size()/2){native.reset();hybrid.reset();}
            for(int ch=0;ch<2;++ch){
                float input=ch==0?x[n]:-.7f*x[n];
                const float ref=native.tick(input,ch);
                const double boundary=hybrid.tickFront(input,ch);
                const float pred=hybrid.tickPost(boundary,ch);
                if(!std::isfinite(ref)||!std::isfinite(pred))throw std::runtime_error("nonfinite");
                maximum=std::max(maximum,std::abs(double(ref)-pred));++checked;
            }
        }
        results.push_back({{"channel",job.at("name")},{"internal_rate",rate},{"checked_samples",checked},{"max_abs_delta",maximum}});
        if(maximum!=0)throw std::runtime_error("Split differs from original");
    }
    std::cout<<nlohmann::json({{"cases",results},{"release_approved",false},
      {"scope","native core algebraic split with retained source memory, automation and reset; no neural replacement or oversampling/DAW performance claim"}}).dump(2)<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
