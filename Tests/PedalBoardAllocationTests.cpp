// Linux evidence harness. Link with --wrap=malloc/calloc/realloc/free.
// C++ new/delete are also intercepted, including the JUCE heap-buffer paths.
#include "PedalBoardDSP.h"
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
thread_local bool counting=false;
thread_local size_t allocationCalls=0,freeCalls=0;
void allocated(){if(counting)++allocationCalls;}
void freed(void* p){if(counting && p)++freeCalls;}
}
extern "C" {
void* __real_malloc(size_t);void* __real_calloc(size_t,size_t);void* __real_realloc(void*,size_t);void __real_free(void*);
void* __wrap_malloc(size_t n){allocated();return __real_malloc(n);}
void* __wrap_calloc(size_t n,size_t s){allocated();return __real_calloc(n,s);}
void* __wrap_realloc(void* p,size_t n){allocated();return __real_realloc(p,n);}
void __wrap_free(void* p){freed(p);__real_free(p);}
}
void* operator new(size_t n){allocated();if(auto* p=__real_malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{freed(p);__real_free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,size_t)noexcept{::operator delete(p);}

int main(){
    using namespace spectralforge;size_t callbacks=0;
    for(double rate:{44100.,48000.,96000.})for(int channels:{1,2}) {
        PedalBoardDSP board;board.prepare({rate,1024,(juce::uint32)channels});
        PedalBoardState state;state.enabled=true;
        juce::AudioBuffer<float> buffer(channels,1024);
        for(int size:{64,256,1024}) {
            buffer.setSize(channels,size,false,false,true);
            for(int model:{0,1,9,20,29,30,31,34,38,39}) {
                for(auto& instance:state.instances)instance=defaultPedalInstance(model);
                for(int k=0;k<5;++k) {
                    for(int c=0;c<channels;++c)for(int n=0;n<size;++n)buffer.setSample(c,n,.1f*std::sin(float(n+k*size)*.075f));
                    state.lowTap=k;std::rotate(state.order.begin(),state.order.begin()+1,state.order.end());
                    state.instances[0].bypass=k>2;
                    counting=true;board.process(buffer,state);counting=false;++callbacks;
                    for(int c=0;c<channels;++c)for(int n=0;n<size;++n)
                        if(!std::isfinite(buffer.getSample(c,n))){std::cerr<<"FAIL nonfinite output\n";return 1;}
                }
            }
        }
    }
    std::cout<<"PRE callbacks="<<callbacks<<" allocation_calls="<<allocationCalls<<" free_calls="<<freeCalls<<" (malloc/calloc/realloc/free and C++ new/delete)\n";
    return allocationCalls==0 && freeCalls==0?0:1;
}
