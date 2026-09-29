#include "PedalOctaverTests.h"
#ifdef PEDAL_OCTAVER_ALLOCATION_TEST
#include <cstdlib>
#include <new>
namespace {thread_local bool counting=false;thread_local size_t allocations=0,frees=0;}
extern "C" {
void* __real_malloc(size_t);void* __real_calloc(size_t,size_t);void* __real_realloc(void*,size_t);void __real_free(void*);
void* __wrap_malloc(size_t n){if(counting)++allocations;return __real_malloc(n);}
void* __wrap_calloc(size_t n,size_t s){if(counting)++allocations;return __real_calloc(n,s);}
void* __wrap_realloc(void* p,size_t n){if(counting)++allocations;return __real_realloc(p,n);}
void __wrap_free(void* p){if(counting&&p)++frees;__real_free(p);}
}
void* operator new(size_t n){if(counting)++allocations;if(auto* p=__real_malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(counting&&p)++frees;__real_free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,size_t)noexcept{::operator delete(p);}
int main() {
    using namespace spectralforge;size_t callbacks=0;
    for(double rate:{44100.,48000.,96000.})for(int channels:{1,2}) {
        PedalMonoOctaver mono;PedalPolyOctaver poly;
        mono.prepare({rate,1024,(juce::uint32)channels});poly.prepare({rate,1024,(juce::uint32)channels});juce::AudioBuffer<float> b(channels,1024);
        for(int block:{64,111,256,1024}) {
            b.setSize(channels,block,false,false,true);
            for(int iteration=0;iteration<40;++iteration) {
                for(int c=0;c<channels;++c)for(int n=0;n<block;++n)b.setSample(c,n,float(.1*std::sin((n+iteration*block)*.01)));
                counting=true;mono.process(b,.3f,.4f,.5f);counting=false;++callbacks;
                counting=true;poly.process(b,.3f,.4f,.5f);counting=false;++callbacks;
                for(int c=0;c<channels;++c)for(int n=0;n<block;++n)if(!std::isfinite(b.getSample(c,n)))return 2;
            }
        }
    }
    std::cout<<"Octaver callbacks="<<callbacks<<" allocations="<<allocations<<" frees="<<frees<<'\n';return allocations==0 && frees==0?0:1;
}
#else
int main(){std::cout<<std::unitbuf;try{pedalOctaverTests::run();return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
#endif
