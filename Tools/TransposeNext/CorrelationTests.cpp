#include "CorrelationPitch.h"
#include "TransientSplitPitch.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>

static thread_local bool watching=false;
static thread_local size_t allocationCount=0;
void* operator new(std::size_t n){if(watching)++allocationCount;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}

struct Candidate {
    transposeNext::CorrelationPitch a;
    transposeNext::TransientSplitPitch b;
    bool split{};
    void prepare(const juce::dsp::ProcessSpec& spec,double limit) {
        split=limit==-42;
        if(split)b.prepare(spec);else a.prepare(spec,limit==-41?40:std::abs(limit),limit<0,limit==-41);
    }
    void setSemitones(int n){if(split)b.setSemitones(n);else a.setSemitones(n);}
    void process(const juce::AudioBuffer<float>& in,juce::AudioBuffer<float>& out,bool render=true){if(split)b.process(in,out,render);else a.process(in,out,render);}
    int latency()const{return split?b.latency():a.latency();}
    void reset(){if(split)b.reset();else a.reset();}
};
std::vector<float> render(double rate,int block,int channels,int shift,double limit,bool bypass=false,bool silent=false,bool events=false) {
    Candidate p;p.prepare({rate,juce::uint32(block),juce::uint32(channels)},limit);p.setSemitones(shift);
    const int total=int(rate*.75);std::vector<float> result(size_t(total)*channels);
    juce::AudioBuffer<float> in(channels,block),out(channels,block);
    int off=0,event=0;
    const std::array<int,5> eventAt{int(rate*.1),int(rate*.2),int(rate*.3),int(rate*.4),int(rate*.5)};
    const std::array<int,5> eventShift{12,-12,0,2,-2};
    bool enabled=!bypass;
    while(off<total){
        if(events && event<int(eventAt.size()) && off==eventAt[size_t(event)]){p.setSemitones(eventShift[size_t(event)]);++event;}
        int n=std::min(block,total-off);
        if(events && event<int(eventAt.size()))n=std::min(n,eventAt[size_t(event)]-off);
        in.setSize(channels,n,false,false,true);out.setSize(channels,n,false,false,true);
        for(int i=0;i<n;++i)for(int c=0;c<channels;++c){
            const double t=(off+i)/rate;
            const float x=silent?0.f:float(.2*std::sin(2*juce::MathConstants<double>::pi*30.867706*t)+.06*std::sin(2*juce::MathConstants<double>::pi*92.603118*t));
            in.setSample(c,i,c==0?x:-.65f*x);
        }
        watching=true;p.process(in,out,enabled);watching=false;
        require(allocationCount==0,"C++ allocation during process");
        for(int i=0;i<n;++i)for(int c=0;c<channels;++c){
            const auto x=out.getSample(c,i);require(std::isfinite(x)&&std::abs(x)<.5f,"invalid/bounded output");
            if(bypass || (shift==0&&!events))require(x==in.getSample(c,i),"non-exact bypass/unity");
            if(silent)require(x==0,"silence output");
            if(c==1)require(std::abs(x+.65f*out.getSample(0,i))<1.e-6f,"linked stereo polarity changed");
            result[size_t(off+i)*channels+size_t(c)]=x;
        }
        off+=n;
    }
    require(p.latency()==-1,"variable delay advertised fixed");
    // Empty block and reset must be safe and erase stale history.
    in.setSize(channels,0,false,false,true);out.setSize(channels,0,false,false,true);p.process(in,out);
    p.reset();in.setSize(channels,block,false,false,true);out.setSize(channels,block,false,false,true);in.clear();p.process(in,out);
    for(int c=0;c<channels;++c)for(int n=0;n<block;++n)require(out.getSample(c,n)==0,"reset stale samples");
    return result;
}
int main(){try{
    int cases=0;
    for(double rate:{44100.,48000.,96000.})for(double limit:{10.,40.,-40.,-41.,-42.})for(int channels:{1,2}){
        for(int shift:{-12,-5,-2,-1,0,1,2,5,12}){
            auto a=render(rate,64,channels,shift,limit);
            require(a==render(rate,257,channels,shift,limit),"partition-dependent output");
            render(rate,128,channels,shift,limit,true);render(rate,128,channels,shift,limit,false,true);++cases;
        }
        require(render(rate,64,channels,-2,limit,false,false,true)==render(rate,257,channels,-2,limit,false,false,true),"event partition changed");
    }
    std::cout<<"PASS "<<cases<<" rate/bound/layout/shift groups: partition, exact bypass/unity, silence, linked stereo, reset, automation finite, zero C++ allocations. Research only; no DAW/PDC acceptance.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
