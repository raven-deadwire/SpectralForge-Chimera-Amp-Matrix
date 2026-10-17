#include "PredictiveSpectral.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>

static std::atomic<size_t> allocations{0};
void* operator new(std::size_t n){allocations.fetch_add(1);if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
static void require(bool ok,const char* s){if(!ok)throw std::runtime_error(s);}

std::vector<float> render(double rate,int order,int block,int channels,int shift,bool reprepare=false) {
    transposePredictive::PredictiveSpectral dsp;
    juce::dsp::ProcessSpec spec{rate,juce::uint32(block),juce::uint32(channels)};
    dsp.prepare(spec,order);dsp.setSemitones(shift);
    const int total=int(rate*.35)+3;
    std::vector<float> result(size_t(total)*2);
    juce::AudioBuffer<float> input(channels,block),output(channels,block);
    if(reprepare){dsp.prepare(spec,order);dsp.setSemitones(shift);}
    for(int offset=0;offset<total;offset+=block) {
        int n=std::min(block,total-offset);input.setSize(channels,n,false,false,true);output.setSize(channels,n,false,false,true);
        for(int i=0;i<n;++i){const float x=offset+i<500?0.f:float(.2*std::sin(6.283185307179586*61.7354*(offset+i)/rate));for(int c=0;c<channels;++c)input.setSample(c,i,c==0?x:-.65f*x);}
        size_t before=allocations.load();dsp.process(input,output);require(allocations.load()==before,"process allocation");
        for(int i=0;i<n;++i)for(int c=0;c<channels;++c){float v=output.getSample(c,i);require(std::isfinite(v)&&std::abs(v)<2,"nonfinite/unbounded");if(shift==0)require(v==input.getSample(c,i),"unity not exact");result[2*size_t(offset+i)+size_t(c)]=v;}
    }
    dsp.reset();input.clear();output.clear();
    for(int i=0;i<100;++i){dsp.process(input,output);require(output.getMagnitude(0,output.getNumSamples())==0,"reset left stale audio");}
    return result;
}

int main(){try{
    int routes=0;
    for(double sr:{44100.,48000.,96000.})for(int order:{10,11,12})for(int channels:{1,2})for(int shift:{-2,-1,0,1,2}) {
        auto reference=render(sr,order,128,channels,shift);
        require(reference==render(sr,order,63,channels,shift),"block partition changed output");
        require(reference==render(sr,order,257,channels,shift,true),"reprepare/partition mismatch");
        if(channels==2){double err=0,energy=0;for(size_t n=0;n<reference.size();n+=2){const double e=reference[n+1]+.65*reference[n];err+=e*e;energy+=double(reference[n])*reference[n];}require(energy==0||std::sqrt(err/(energy+1.e-30))<.001,"antiphase stereo drift");}
        ++routes;
    }
    transposePredictive::PredictiveSpectral dsp;dsp.prepare({48000,128,2},12);
    juce::AudioBuffer<float> input(2,128),output(2,128);input.clear();
    for(int block=0;block<400;++block){dsp.setSemitones(block%5-2);for(int i=0;i<128;++i)input.setSample(0,i,float(.3*std::sin(.1*(block*128+i))));dsp.process(input,output);require(output.getMagnitude(1,0,128)==0,"left-to-right crosstalk");require(std::isfinite(output.getMagnitude(0,0,128)),"transition nonfinite");}
    std::cout<<"PASS "<<routes<<" routes: finite, unity, partition, reprepare, reset, antiphase; no observed C++ new in process; transition/crosstalk\n";
    std::cout<<"Not DI, tonal-quality, host/PDC, or realtime acceptance\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
