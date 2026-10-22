// Standalone research contracts; never selects the candidate in the product.
#include "../Tools/experiments/MultibandBypassCandidate.h"
#include <vector>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>
static bool counting=false;
static size_t allocations=0;
static void (*volatile releaseMemory)(void*)=&std::free;
void* operator new(std::size_t n) { if(counting) ++allocations; if(auto* p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {releaseMemory(p);}
void operator delete[](void* p) noexcept {releaseMemory(p);}
void operator delete(void* p,std::size_t) noexcept {releaseMemory(p);}
void operator delete[](void* p,std::size_t) noexcept {releaseMemory(p);}
using namespace spectralforge;
using Candidate=research::MultibandBypassCandidate;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
using Audio=std::array<std::vector<float>,2>;
Audio signal(int length,double rate){
    Audio a{std::vector<float>(size_t(length)),std::vector<float>(size_t(length))};
    unsigned r=391;
    for(int n=0;n<length;++n){r=r*1664525u+1013904223u;
        const float noise=float(double(r)/4294967296.-.5)*.03f;
        a[0][size_t(n)]=float(.2*std::sin(2.*multiband_detail::pi*27.5*n/rate)+.1*std::sin(2.*multiband_detail::pi*700*n/rate))+noise;
        a[1][size_t(n)]=float(.17*std::sin(2.*multiband_detail::pi*82.4*n/rate+.8))-noise;
    }return a;
}
template<class P> void render(P& p,Audio& a,int channels,int block,int begin,int end){
    for(int n=begin;n<end;n+=block){float* ptr[]{a[0].data()+n,a[1].data()+n};
        counting=true;p.process(ptr,channels,std::min(block,end-n));counting=false;}
}
int main(){try{
    for(double rate:{44100.,48000.,96000.,192000.})for(int channels:{1,2}){
        const int size=int(rate*.6);const auto dry=signal(size,rate);
        MultibandSettings s;Candidate c;c.prepare(rate);
        auto bypass=dry;render(c,bypass,channels,137,0,size);
        check(bypass==dry,"Default OFF changed samples");check(c.latencySamples()==0,"Reported latency changed");
        s.on=true;s.mix=.63f;s.bands[0].makeup=3;s.bands[2].makeup=-7;
        s.bands[1].mode=1;s.bands[1].range=9;
        MultibandDynamicsCore base;base.setSettings(s);base.prepare(rate);
        c.setSettings(s);c.prepare(rate);
        auto a=dry,b=dry;render(base,a,channels,257,0,size);render(c,b,channels,31,0,size);
        check(a==b,"Settled active candidate changed production samples");
        // Cold ON must change the raw signal and reach the warmed production path.
        c.setSettings(MultibandSettings{});c.prepare(rate);c.setSettings(s);
        b=dry;render(c,b,channels,127,0,size);check(b!=dry,"Cold ON never activated");
        for(int ch=0;ch<channels;++ch)for(int n=int(rate*.16);n<size;++n)
            check(b[size_t(ch)][size_t(n)]==a[size_t(ch)][size_t(n)],"ON endpoint differs from warmed core");
        // Exact raw OFF endpoint and no writes to the unused mono channel.
        s.on=false;c.setSettings(s);b=dry;render(c,b,channels,257,0,size);
        for(int ch=0;ch<channels;++ch)for(int n=int(rate*.112);n<size;++n)
            check(b[size_t(ch)][size_t(n)]==dry[size_t(ch)][size_t(n)],"OFF endpoint is not exact raw");
        if(channels==1)check(b[1]==dry[1],"Mono wrote another channel");
        // Reversals before completion, crossover automation, silence, extra/zero blocks.
        auto stress=[&](int block){Candidate p;p.prepare(rate);auto x=dry;MultibandSettings v;
            const std::array<int,9> events{0,1,7,int(rate*.019),int(rate*.025),int(rate*.08),int(rate*.087),int(rate*.25),size};
            for(size_t e=0;e+1<events.size();++e){v.on=(e%2)==0;v.crossovers=e>3?std::array<float,3>{40,56,78.4f}:std::array<float,3>{120,700,3500};
                p.setSettings(v);p.process(nullptr,0,0);render(p,x,channels,block,events[e],events[e+1]);}
            return x;};
        a=stress(1);b=stress(257);check(a==b,"Reversal/crossover sequence depends on host partition");
        for(const auto& ch:b)for(float v:ch)check(std::isfinite(v)&&std::abs(v)<4.,"Stress output nonfinite/unbounded");
        auto quiet=Audio{std::vector<float>(size),std::vector<float>(size)};
        c.prepare(rate);render(c,quiet,channels,2049,0,size);
        for(const auto& ch:quiet)for(float v:ch)check(v==0.f,"Silent reset emitted output");
    }
    check(allocations==0,"Research processing allocated memory");
    std::cout<<"PASS 4 rates x mono/stereo: exact OFF/default/unused channel, zero latency, bit-exact active output, cold ON endpoint, rapid reversal + crossover partition parity, finite output, silence, allocation count="<<allocations<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
