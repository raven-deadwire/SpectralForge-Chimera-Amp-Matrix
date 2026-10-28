// Safety contracts for the opt-in tail candidates; synthetic, no audibility claim.
#include "UpshiftTailResearch.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
static thread_local bool watching=false;
static thread_local size_t allocations=0;
void* operator new(std::size_t n) {if(watching)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
int main() {
    juce::ScopedNoDenormals noDenormals;
    int cases=0;
    for(double rate:{44100.,48000.,96000.})for(unsigned channels:{1u,2u})
    for(const auto* base:{"v3","v4"})for(const auto* suffix:{"g8","h40","g8h40","g12h50"})
    for(int shift:{-2,-1,0,1,2}) {
        transposeNext::UpshiftTailResearch config(std::string(base)+"-"+suffix);
        transposeNext::TransientSplitPitch e;
        e.prepare({rate,128,channels},true,true,true,true,true,true);
        e.setSincOutput(std::string(base)=="v4");config.apply(e,shift);e.setSemitones(shift);
        juce::AudioBuffer<float> in(int(channels),128),out(int(channels),128);
        std::vector<float> first;
        for(int pass=0;pass<2;++pass) {
            if(pass)e.reset();
            for(int block=0;block<160;++block) {
                for(unsigned c=0;c<channels;++c)for(int n=0;n<128;++n) {
                    const int t=block*128+n;
                    const double u=std::fmod(t/rate,.075);
                    const float x=float((.12*std::sin(2*juce::MathConstants<double>::pi*55*t/rate)
                        + .02*std::sin(2*juce::MathConstants<double>::pi*5300*t/rate)*std::exp(-u/.002))*(c?-.65:1));
                    in.setSample(int(c),n,x);
                }
                watching=true;e.process(in,out);watching=false;
                require(allocations==0,"allocation in candidate process");
                for(unsigned c=0;c<channels;++c)for(int n=0;n<128;++n) {
                    const float y=out.getSample(int(c),n);require(std::isfinite(y),"non-finite candidate output");
                    if(shift==0)require(y==in.getSample(int(c),n),"unity is not exact");
                    const size_t i=(size_t(block)*channels+c)*128+size_t(n);
                    if(pass==0)first.push_back(y);else require(first[i]==y,"reset did not reproduce candidate output");
                }
            }
            if(shift!=0){require(e.readMinimum()>=rate*.002-1,"read newer than causal floor");require(e.readMaximum()<=rate*.040+33,"read beyond bounded history");}
        }
        // prepare must restore the existing engine configuration after an experiment.
        e.prepare({rate,128,channels},true,true,true,true,true,true);
        e.setSincOutput(std::string(base)=="v4");e.setSemitones(shift);
        transposeNext::TransientSplitPitch ref;ref.prepare({rate,128,channels},true,true,true,true,true,true);
        ref.setSincOutput(std::string(base)=="v4");ref.setSemitones(shift);
        juce::AudioBuffer<float> expected(int(channels),128);
        for(int k=0;k<30;++k){e.process(in,out);ref.process(in,expected);for(unsigned c=0;c<channels;++c)for(int n=0;n<128;++n)require(out.getSample(int(c),n)==expected.getSample(int(c),n),"prepare leaked experimental settings");}
        ++cases;
    }
    bool rejected=false;
    try {transposeNext::TransientSplitPitch e;transposeNext::UpshiftTailResearch("v3-h40").apply(e,12);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"out-of-scope shift accepted");
    std::cout<<"PASS "<<cases<<" candidate groups: finite output, causal/bounded reads, exact unity, deterministic reset, zero C++ process allocations, prepare restores defaults; +/-12 experiment rejected.\n";
}
