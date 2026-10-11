#include "GlobalDSP.h"
#include "PostRigGate.h"
#include "RelativePhasePitch.h"
#ifdef CHIMERA_LAB_SIGNALSMITH
#include "signalsmith-stretch.h"
#endif
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace {
// Deliberately simple causal two-read-head control, NOT a product candidate.
// The 8 ms read range is not a fixed latency; crossfades may cancel low partials.
class DualDelay {
    std::array<std::vector<float>,2> ring;
    int write{}, length{}, channels{};
    double span{}, phase{}, increment{};
public:
    void prepare(double rate,int count,int shift) {
        span=rate*.008; length=int(std::ceil(span))+8;channels=count;
        increment=(1.-std::pow(2.,shift/12.))/span;
        for(auto& r:ring)r.assign(size_t(length),0.f);
    }
    void process(const juce::AudioBuffer<float>& in,juce::AudioBuffer<float>& out) {
        for(int n=0;n<in.getNumSamples();++n) {
            const double p2=phase>=.5?phase-.5:phase+.5;
            const double w=std::pow(std::sin(juce::MathConstants<double>::pi*phase),2);
            for(int c=0;c<channels;++c) {
                ring[size_t(c)][size_t(write)]=in.getSample(c,n);
                auto read=[&](double p) {
                    double index=write-2.-p*span;
                    while(index<0)index+=length;
                    const int first=int(index);const float f=float(index-first);
                    return ring[size_t(c)][size_t(first)]*(1-f)+ring[size_t(c)][size_t((first+1)%length)]*f;
                };
                out.setSample(c,n,float(w*read(phase)+(1-w)*read(p2)));
            }
            write=(write+1)%length;phase+=increment;phase-=std::floor(phase);
        }
    }
};
struct Engine {
    std::string name;
    spectralforge::PolyPitch baseline;
    transposeResearch::PolyPitch relative;
    spectralforge::Transposer wrapper;
    DualDelay delay;
#ifdef CHIMERA_LAB_SIGNALSMITH
    signalsmith::stretch::SignalsmithStretch<float> stretch{8};
#endif
    int nominal{},shift{};
    Engine(std::string id,double rate,int block,int st):name(std::move(id)),shift(st) {
        juce::dsp::ProcessSpec spec{rate,juce::uint32(block),2};
        if(name=="baseline") {baseline.prepare(spec);baseline.setSemitones(st);nominal=baseline.latency();}
        else if(name=="relative") {relative.prepare(spec);relative.setSemitones(st);nominal=relative.latency();}
        else if(name=="dual-delay-8ms") {delay.prepare(rate,2,st);nominal=-1;}
        else if(name=="wrapper" || name=="bypass") {wrapper.prepare(spec);nominal=name=="wrapper"&&st!=0?wrapper.latency():0;}
#ifdef CHIMERA_LAB_SIGNALSMITH
        else if(name=="signalsmith-matched" || name=="signalsmith-default") {
            int n=256;while(n<rate*.032)n*=2;
            // Same window and hop as production. No FFT-window reduction.
            if(name=="signalsmith-default")stretch.presetDefault(2,float(rate),false);
            else stretch.configure(2,n,n/8,false);
            stretch.setTransposeSemitones(float(st));
            nominal=stretch.inputLatency()+stretch.outputLatency();
        }
#endif
        else throw std::runtime_error("unknown/unavailable algorithm: "+name);
    }
    void process(const juce::AudioBuffer<float>& in,juce::AudioBuffer<float>& out) {
        if(name=="baseline") baseline.process(in,out);
        else if(name=="relative") relative.process(in,out);
        else if(name=="dual-delay-8ms") delay.process(in,out);
        else if(name=="wrapper" || name=="bypass") {out.makeCopyOf(in,true);wrapper.process(out,name=="wrapper"&&shift!=0,shift);}
#ifdef CHIMERA_LAB_SIGNALSMITH
        else stretch.process(in.getArrayOfReadPointers(),in.getNumSamples(),out.getArrayOfWritePointers(),out.getNumSamples());
#endif
    }
};
double quantile(std::vector<double> x,double q) {std::sort(x.begin(),x.end());return x[size_t(q*(x.size()-1))];}
}
int main(int argc,char** argv) {
    try {
        if(argc!=11)throw std::runtime_error("usage: TransposeLab input.f32 output.f32 rate block shift algorithm gate(off/pre/post) threshold release_ms hold_ms");
        const double rate=std::stod(argv[3]);const int block=std::stoi(argv[4]),shift=std::stoi(argv[5]);
        const std::string algorithm=argv[6],position=argv[7];
        if(rate<8000 || rate>192000 || block<1 || block>8192 || shift<-12 || shift>12)throw std::runtime_error("invalid processing configuration");
        if(position!="off" && position!="pre" && position!="post")throw std::runtime_error("invalid gate position");
        std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("input missing");
        const auto bytes=file.tellg();if(bytes<=0 || bytes%8!=0)throw std::runtime_error("expected nonempty interleaved stereo float32");
        std::vector<float> input(size_t(bytes)/sizeof(float));file.seekg(0);file.read(reinterpret_cast<char*>(input.data()),bytes);
        if(!file)throw std::runtime_error("input read failed");
        for(float x:input)if(!std::isfinite(x))throw std::runtime_error("nonfinite input");
        Engine engine(algorithm,rate,block,shift);
        if(engine.nominal<0 && position=="post")throw std::runtime_error("variable-delay control has no fixed post-gate alignment");
        spectralforge::NoiseGate gate;gate.prepare(rate);
        spectralforge::PostRigGate post;post.prepare({rate,juce::uint32(block),2},int(rate));
        juce::AudioBuffer<float> in(2,block),out(2,block);
        std::vector<float> result(input.size());std::vector<double> times,totalTimes;
        const float threshold=std::stof(argv[8]),release=std::stof(argv[9]),hold=std::stof(argv[10]);
        for(size_t offset=0;offset<input.size()/2;offset+=size_t(block)) {
            const int count=int(std::min(size_t(block),input.size()/2-offset));
            in.setSize(2,count,false,false,true);out.setSize(2,count,false,false,true);
            for(int n=0;n<count;++n)for(int c=0;c<2;++c)in.setSample(c,n,input[2*(offset+size_t(n))+size_t(c)]);
            const auto allStart=std::chrono::steady_clock::now();
            if(position=="post")post.detect(in,true,threshold,release,hold,engine.nominal);
            if(position=="pre")gate.process(in,true,threshold,release,hold);
            const auto start=std::chrono::steady_clock::now();engine.process(in,out);
            const auto stop=std::chrono::steady_clock::now();
            if(position=="post")post.apply(out);
            totalTimes.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-allStart).count());
            times.push_back(std::chrono::duration<double,std::micro>(stop-start).count());
            for(int n=0;n<count;++n)for(int c=0;c<2;++c) {
                const float value=out.getSample(c,n);if(!std::isfinite(value))throw std::runtime_error("nonfinite output");
                result[2*(offset+size_t(n))+size_t(c)]=value;
            }
        }
        std::ofstream output(argv[2],std::ios::binary);output.write(reinterpret_cast<const char*>(result.data()),std::streamsize(result.size()*sizeof(float)));
        if(!output)throw std::runtime_error("output write failed");
        const auto misses=std::count_if(totalTimes.begin(),totalTimes.end(),[&](double us){return us>block/rate*1e6;});
        std::cout<<"{\"nominal_samples\":"<<engine.nominal<<",\"callbacks\":"<<times.size()
                 <<",\"core_mean_us\":"<<std::accumulate(times.begin(),times.end(),0.)/times.size()
                 <<",\"core_p95_us\":"<<quantile(times,.95)<<",\"core_p99_us\":"<<quantile(times,.99)
                 <<",\"chain_p99_us\":"<<quantile(totalTimes,.99)<<",\"chain_max_us\":"<<*std::max_element(totalTimes.begin(),totalTimes.end())
                 <<",\"deadline_misses\":"<<misses<<"}\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
