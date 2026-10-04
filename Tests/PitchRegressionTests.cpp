#include "PolyPitch.h"
#include "Fixtures/PolyPitchV110.h"
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Render {std::vector<float> audio;std::vector<double> times;int latency{};};
template<class Pitch> Render tone(double rate,int block,int shift,double frequency,bool impulse=false) {
    Pitch pitch;pitch.prepare({rate,juce::uint32(block),2});pitch.setSemitones(shift);
    juce::AudioBuffer<float> input(2,block),output(2,block);Render result;result.latency=pitch.latency();
    const int samples=impulse?pitch.latency()*3:int(rate*1.5);
    for(int offset=0;offset<samples;offset+=block) {
        for(int n=0;n<block;++n){const auto index=offset+n;const auto x=impulse?(index==0?.2f:0.f):float(.2*std::sin(juce::MathConstants<double>::twoPi*frequency*index/rate));input.setSample(0,n,x);input.setSample(1,n,-.65f*x);}
        const auto start=std::chrono::steady_clock::now();pitch.process(input,output);
        result.times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
        for(int n=0;n<block;++n){require(std::isfinite(output.getSample(0,n)),"Non-finite low pitch output");result.audio.push_back(output.getSample(0,n));}
    }
    return result;
}
double component(const Render& r,double rate,double frequency) {
    std::complex<double> sum{};const auto first=size_t(rate*.75);
    for(size_t n=first;n<r.audio.size();++n)sum+=double(r.audio[n])*std::polar(1.,-juce::MathConstants<double>::twoPi*frequency*double(n)/rate);
    return 2*std::abs(sum)/double(r.audio.size()-first);
}
double percentile(std::vector<double> values,double q){std::sort(values.begin(),values.end());return values[size_t(q*double(values.size()-1))];}
}
int main() {
    try {
        std::cout<<"fixture=synthetic_sine_0.2_L_negative0.65_R frequency=27.5Hz duration=1.5s analysis_last=0.75s; CPU is shared-runner evidence only\n";
        for(double rate:{44100.,48000.,96000.})for(int block:{64,128,256})for(int shift:{-12,-5,-2,-1,0,1,2,5,12}) {
            const auto before=tone<frozenPitchV110::PolyPitch>(rate,block,shift,27.5),after=tone<spectralforge::PolyPitch>(rate,block,shift,27.5);
            const auto target=27.5*std::pow(2.,double(shift)/12.);const auto a=component(before,rate,target),b=component(after,rate,target);
            require(before.latency==after.latency,"Low energy correction changed latency contract");
            if(shift>=0)require(before.audio==after.audio,"Unaffected pitch mapping changed output");
            else require(b>=a*.99 && b>.08 && b<.4,"Low-bin correction attenuated or amplified target excessively");
            std::cout<<"MEASURE rate="<<rate<<" block="<<block<<" shift="<<shift<<" nominal_latency="<<after.latency
                     <<" baseline_target_db="<<20*std::log10(a/.2)<<" candidate_target_db="<<20*std::log10(b/.2)
                     <<" baseline_p99_us="<<percentile(before.times,.99)<<" candidate_p95_us="<<percentile(after.times,.95)<<" candidate_p99_us="<<percentile(after.times,.99)<<'\n';
        }
        for(double rate:{44100.,48000.,96000.})for(int shift:{-12,-2,2,12}) {
            const auto r=tone<spectralforge::PolyPitch>(rate,64,shift,0,true);double energy=0,weighted=0;for(size_t n=0;n<r.audio.size();++n){const auto e=double(r.audio[n])*r.audio[n];energy+=e;weighted+=e*double(n);}
            require(energy>1e-8,"Pitch impulse disappeared");double cumulative=0;int lo=-1,hi=-1;
            for(size_t n=0;n<r.audio.size();++n){cumulative+=double(r.audio[n])*r.audio[n];if(lo<0 && cumulative>=energy*.05)lo=int(n);if(hi<0 && cumulative>=energy*.95)hi=int(n);}
            std::cout<<"MEASURE impulse rate="<<rate<<" shift="<<shift<<" latency="<<r.latency<<" centroid_samples="<<weighted/energy<<" energy5_95_samples="<<lo<<':'<<hi<<" smear_ms="<<1000*(hi-lo)/rate<<'\n';
        }
        std::cout<<"PASS low-bin target-energy correction and unchanged positive/unity raw mappings; live latency / real DI / stereo image / listening acceptance BLOCKED\n";
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
