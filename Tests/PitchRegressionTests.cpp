#include "PolyPitch.h"
#include "HybridTranspose.h"
#include "../Tools/TransposeNext/HybridLowBandPitch.h"
#include "Fixtures/PolyPitchV110.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Render {std::vector<float> audio;std::vector<double> times;int latency{};};
template<class Pitch> Render tone(double rate,int block,int shift,double frequency,bool impulse=false,int windowOverride=0,int analysisLength=0) {
    Pitch pitch;
    if constexpr(std::is_same_v<Pitch,spectralforge::PolyPitch>)pitch.prepare({rate,juce::uint32(block),2},windowOverride,analysisLength);
    else pitch.prepare({rate,juce::uint32(block),2});
    pitch.setSemitones(shift);
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
// HYBRID material: a decaying harmonic note (or chord), 0.5 s of silence first.
std::vector<float> pluck(double rate,std::initializer_list<double> notes,double seconds=2.) {
    std::vector<float> x(size_t(rate*(seconds+.5)),0.f);
    for(double f0:notes)for(int k=1;k<=12 && k*f0<.45*rate;++k)
        for(size_t n=size_t(rate*.5);n<x.size();++n){const double t=double(n)/rate-.5;x[n]+=float(.1/k*std::sin(juce::MathConstants<double>::twoPi*k*f0*t)*std::exp(-t/(1.2/(1+.35*k)))*std::min(1.,t/.002));}
    return x;
}
// The same note re-picked every `every` seconds (each pick restarts the decay).
std::vector<float> repicked(double rate,double f0,double every,double seconds=2.) {
    std::vector<float> x(size_t(rate*(seconds+.5)),0.f);
    for(size_t n=size_t(rate*.5);n<x.size();++n){const double t=double(n)/rate-.5,u=std::fmod(t,every);
        for(int k=1;k<=12 && k*f0<.45*rate;++k)x[n]+=float(.1/k*std::sin(juce::MathConstants<double>::twoPi*k*f0*t)*std::exp(-u/(1.2/(1+.35*k)))*std::min(1.,u/.002));}
    return x;
}
// blocks: host callback sizes, cycled (each <= the prepared maximum).
template<class Engine,class Process> std::vector<float> renderHybrid(Engine& engine,const std::vector<int>& blocks,const std::vector<float>& x,Process&& process) {
    const int largest=*std::max_element(blocks.begin(),blocks.end());
    juce::AudioBuffer<float> input(2,largest),output(2,largest);std::vector<float> y;
    for(size_t offset=0,k=0;offset<x.size();offset+=size_t(blocks[k++%blocks.size()])) {
        const int n=int(std::min<size_t>(size_t(blocks[k%blocks.size()]),x.size()-offset));
        input.setSize(2,n,false,false,true);output.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i){input.setSample(0,i,x[offset+size_t(i)]);input.setSample(1,i,-.65f*x[offset+size_t(i)]);}
        process(engine,input,output);
        for(int i=0;i<n;++i){require(std::isfinite(output.getSample(0,i)) && std::isfinite(output.getSample(1,i)),"Non-finite HYBRID output");y.push_back(output.getSample(0,i));}
    }
    return y;
}
template<class Engine,class Process> std::vector<float> renderHybrid(Engine& engine,double,int block,const std::vector<float>& x,Process&& process) {
    return renderHybrid(engine,std::vector<int>{block},x,process);
}
double componentOf(const std::vector<float>& y,double rate,double frequency,double from,double to) {
    std::complex<double> sum{};const auto a=size_t(rate*from),b=std::min(y.size(),size_t(rate*to));
    for(size_t n=a;n<b;++n)sum+=double(y[n])*std::polar(1.,-juce::MathConstants<double>::twoPi*frequency*double(n)/rate);
    return 2*std::abs(sum)/double(b-a);
}
void hybridChecks() {
    // 1. The product copy equals the research engine in the render probe's
    //    mode 41 (Tools/TransposeEvidence/TransposeRenderProbe.cpp), bit for bit.
    for(double rate:{44100.,48000.,96000.})for(int shift:{-12,-5,-2,2,7,12})for(int material=0;material<2;++material) {
        const auto x=material==0 ? pluck(rate,{82.41}) : pluck(rate,{82.41,123.47,164.81});
        const juce::dsp::ProcessSpec spec{rate,128,2};
        spectralforge::hybrid::HybridTransposer product;product.prepare(spec);product.setSemitones(shift);
        transposeNext::HybridLowBandPitch research;
        research.prepare(spec,800.,40.,true,10.,true,.5,2.9,true,false,false);
        research.setAttackLatency(1.4);research.setLegatoReanchor(true);research.setReanchorPhaseSearch(2.5,false);research.setSolo(0);research.setSemitones(shift);
        const auto a=renderHybrid(product,rate,128,x,[](auto& e,auto& in,auto& out){e.process(in,out,true);});
        const auto b=renderHybrid(research,rate,128,x,[](auto& e,auto& in,auto& out){e.process(in,out);});
        require(a==b,"HYBRID product copy differs from the research engine (probe mode 41)");
        require(product.latency()==int(rate*.010)/8*8 && research.latency()==product.latency(),"HYBRID latency is not the 10 ms STFT window");
    }
    // 2. Low notes and a power chord come out at the shifted pitch, where the
    //    10 ms STFT (TIGHT) mistunes them (synthetic evidence: 60-370 cents).
    for(double rate:{44100.,48000.,96000.})for(double f0:{41.2,82.41})for(int shift:{-5,-2,2,5}) {
        const auto x=pluck(rate,{f0});const auto target=f0*std::pow(2.,shift/12.);
        spectralforge::hybrid::HybridTransposer h;h.prepare({rate,256,2});h.setSemitones(shift);
        const auto y=renderHybrid(h,rate,256,x,[](auto& e,auto& in,auto& out){e.process(in,out,true);});
        const double wanted=componentOf(y,rate,target,.6,1.6),source=componentOf(y,rate,f0,.6,1.6),input=componentOf(x,rate,f0,.6,1.6);
        require(wanted>.5*input && source<.1*wanted,"HYBRID lost a low note's shifted fundamental");
        std::cout<<"MEASURE hybrid rate="<<rate<<" f0="<<f0<<" shift="<<shift<<" target_db_re_input="<<20*std::log10(wanted/input)
                 <<" source_residual_db="<<20*std::log10(std::max(source,1e-12)/wanted)<<'\n';
    }
    // 3. Block-size invariance, including re-picked notes: a pick must reach
    //    the legato detector before its next 1 ms update whatever the block
    //    (it was applied at block end: 512 at 48 kHz and 500 at 44.1 kHz then
    //    differed from 128 by up to 2.4e-3 after the second pick).
    for(double rate:{44100.,48000.,96000.})for(int material=0;material<3;++material)for(int shift:{-5,7}) {
        const auto x=material==0 ? pluck(rate,{82.41,123.47,164.81}) : repicked(rate,82.41,material==1 ? .3 : .125);
        std::vector<float> reference;
        for(const auto& blocks:std::vector<std::vector<int>>{{128},{1},{32},{100},{500},{512},{1024},{1,77,512,13,256,500,3,128}}) {
            const int largest=*std::max_element(blocks.begin(),blocks.end());
            spectralforge::hybrid::HybridTransposer h;h.prepare({rate,juce::uint32(largest),2});h.setSemitones(shift);
            const auto y=renderHybrid(h,blocks,x,[](auto& e,auto& in,auto& out){e.process(in,out,true);});
            if(reference.empty())reference=y;else require(y==reference,"HYBRID output depends on the block size");
        }
    }
    std::cout<<"PASS HYBRID transpose: bit-identical to research probe mode 41, low notes shifted, block-size invariant (synthetic only; DI and listening not done)\n";
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
        // Tight transpose (Source/GlobalDSP.h): a 10 ms analysis window zero-padded
        // into a power-of-two FFT. Equal window/FFT lengths must be bit-identical
        // to the power-of-two path that Classic/Low-Latency keep using.
        for(double rate:{44100.,48000.})for(int shift:{-2,2}) {
            int defaultWindow=256;while(defaultWindow<rate*.032)defaultWindow<<=1;
            const auto plain=tone<spectralforge::PolyPitch>(rate,128,shift,220,false,defaultWindow/4);
            const auto same=tone<spectralforge::PolyPitch>(rate,128,shift,220,false,0,defaultWindow/4);
            require(plain.latency==same.latency && plain.audio==same.audio,"Equal analysis/FFT length changed the power-of-two path");
        }
        for(double rate:{44100.,48000.,96000.,192000.})for(int block:{64,128}) {
            const int tight=int(rate*.010)/8*8;
            int defaultWindow=256;while(defaultWindow<rate*.032)defaultWindow<<=1;
            for(int shift:{-2,-1,1,2}) {
                for(double frequency:{110.,220.,440.}) {
                    const auto now=tone<spectralforge::PolyPitch>(rate,block,shift,frequency,false,0,tight);
                    const auto before=tone<spectralforge::PolyPitch>(rate,block,shift,frequency,false,defaultWindow/4);
                    require(now.latency==tight && 1000.*now.latency/rate<=10.0+1e-9,"Tight transpose exceeds 10 ms added latency");
                    const auto target=frequency*std::pow(2.,double(shift)/12.);
                    const auto wanted=component(now,rate,target),source=component(now,rate,frequency),previous=component(before,rate,target);
                    require(wanted>.08 && source<wanted*.5,"Tight transpose lost the shifted pitch");
                    require(wanted>=previous*.9,"Tight 10 ms window weakened the shifted pitch versus the 11 ms window");
                    if(block==128)std::cout<<"MEASURE tight rate="<<rate<<" shift="<<shift<<" f="<<frequency<<" latency_samples="<<now.latency
                             <<" latency_ms="<<1000.*now.latency/rate<<" previous_ms="<<1000.*before.latency/rate
                             <<" target_db="<<20*std::log10(wanted/.2)<<" previous_target_db="<<20*std::log10(previous/.2)
                             <<" source_residual_db="<<20*std::log10(std::max(source,1e-12)/.2)<<'\n';
                }
            }
        }
        hybridChecks();
        std::cout<<"PASS Tight transpose <=10 ms algorithmic latency at 44.1/48/96/192 kHz (±1/±2 semitones); host round trip and real DI not measured\n";
        std::cout<<"PASS low-bin target-energy correction and unchanged positive/unity raw mappings; live latency / real DI / stereo image / listening acceptance BLOCKED\n";
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
