#include "Cabinet.h"
#include "CabLayoutModel.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <new>
#include <thread>
#include <time.h>

// Count C++ allocation/deallocation on this audio thread only. Direct C malloc
// is outside this detector; the cache's own path is fixed arrays/memcpy only.
static thread_local bool audioScope=false;
static thread_local unsigned allocations=0,deallocations=0;
void* operator new(std::size_t n) {if(audioScope)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {if(audioScope&&p)++deallocations;std::free(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
void* operator new(std::size_t n,std::align_val_t a) {if(audioScope)++allocations;void* p=nullptr;if(posix_memalign(&p,std::size_t(a),n?n:1)==0)return p;throw std::bad_alloc();}
void* operator new[](std::size_t n,std::align_val_t a) {return ::operator new(n,a);}
void operator delete(void* p,std::align_val_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::align_val_t) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t,std::align_val_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t,std::align_val_t) noexcept {::operator delete(p);}

using spectralforge::Cab;
namespace oc=spectralforge::originalCab;
namespace ce=spectralforge::cabExpansion;
namespace cl=spectralforge::cabLayout;
static long long cpuNs() {
    timespec ts{};
    if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&ts)!=0)std::abort();
    return static_cast<long long>(ts.tv_sec)*1000000000LL+ts.tv_nsec;
}
static uint64_t responseKey(int family,int mic) {
    oc::Settings base{true,1,1,mic,2,.4,.73,18.3};
    if(family==0)return oc::key(base);
    if(family==1)return ce::key({base,12,20-mic,2});
    return cl::key({{base,9,10+mic,2},7,5-mic});
}
static juce::AudioBuffer<float> response(int family,int mic) {
    const char* folder=std::getenv("CHIMERA_FFT_IR_DIR");
    const std::string path=folder?std::string(folder)+"/"+std::to_string(family)+"-"+std::to_string(mic)+".f32":"";
    if(folder && std::filesystem::exists(path)) {
        const auto bytes=std::filesystem::file_size(path);
        if(!bytes || bytes%sizeof(float))throw std::runtime_error("bad frozen IR");
        juce::AudioBuffer<float> b(1,int(bytes/sizeof(float)));std::ifstream in(path,std::ios::binary);
        in.read(reinterpret_cast<char*>(b.getWritePointer(0)),std::streamsize(bytes));
        if(!in)throw std::runtime_error("IR read failure");return b;
    }
    const auto key=responseKey(family,mic);
    const auto samples=family==0 ? oc::generate(oc::settings(key),96000.)
        : family==1 ? ce::generate(key,96000.) : cl::generate(key,96000.);
    juce::AudioBuffer<float> b(1,int(samples.size()));b.copyFrom(0,0,samples.data(),int(samples.size()));
    if(folder) {std::filesystem::create_directories(folder);std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(samples.data()),std::streamsize(samples.size()*sizeof(float)));if(!out)throw std::runtime_error("IR freeze failure");}
    return b;
}
static void run(int family,int block,int channels,int scenario,const std::string& dumpPath,const std::string& csvPath,int callbacks) {
    const juce::ScopedNoDenormals noDenormals;
    juce::dsp::ProcessSpec spec{96000.,juce::uint32(block),juce::uint32(channels)};
    std::array<juce::AudioBuffer<float>,2> ir{{response(family,0),response(family,1)}};
    std::array<std::unique_ptr<Cab>,3> rigs;
    std::array<juce::AudioBuffer<float>,3> audio;
    std::array<std::shared_ptr<const spectralforge::ModeledCabConvolution::Prepared>,2> prepared;
    for(int m=0;m<2;++m)prepared[size_t(m)]=std::make_shared<const spectralforge::ModeledCabConvolution::Prepared>(ir[size_t(m)],block,channels==1);
    auto kernel=[&](int m,int revision,bool fallback=false) {
        juce::AudioBuffer<float> copy;copy.makeCopyOf(ir[size_t(m)]);
        auto k=std::make_unique<Cab::Kernel>(std::move(copy),96000.,spec,fallback?3:0,unsigned(revision),fallback?0:responseKey(family,m),fallback?nullptr:prepared[size_t(m)]);
        if(k->getLatency()!=0)throw std::runtime_error("added latency");return k;
    };
    // Every new kernel is constructed before measured/audio processing begins.
    std::array<std::unique_ptr<Cab::Kernel>,6> replacements;
    for(int l=0;l<3;++l) {
        rigs[size_t(l)]=std::make_unique<Cab>();auto& c=*rigs[size_t(l)];c.blend=.5f;
        c.prepare(spec);c.setCuts(70,9000);c.secondMic()->setCuts(70,9000);
        for(int m=0;m<2;++m) {
            auto* slot=m?c.secondMic():&c;
            const bool fallback=scenario==2&&m==1;
            slot->requestedModel.store(fallback?0:responseKey(family,m));slot->install(kernel(m,1,fallback));
            replacements[size_t(2*l+m)]=kernel(1-m,2,fallback);
        }
        audio[size_t(l)].setSize(channels,block);
    }
    std::ofstream dump;if(dumpPath!="-")dump.open(dumpPath,std::ios::binary);
    std::ofstream csv(csvPath);
    csv<<"callback,samples,cpu_ns,wall_ns,hits,misses,overflow,new_calls,delete_calls,fft_forward_calls,fft_forward_ns,mac_calls,mac_ns\n";
    int frame=0;
    for(int cb=0;cb<callbacks;++cb) {
        const std::array<int,5> irregular{{1,17,63,block,3}};
        const int count=scenario?std::min(block,irregular[size_t(cb%5)]):block;
        for(int l=0;l<3;++l) {
            auto& c=*rigs[size_t(l)];
            if(scenario) {
                if(cb==100)c.blend=0;
                if(cb==350)c.blend=.5f;
                if(cb==450)c.secondMic()->setCuts(135,7300);
                if(cb==650)c.secondMic()->setCuts(70,9000);
                if(cb==700) {c.secondMic()->gainDb=-3;c.secondMic()->delayMs=.73f;c.secondMic()->invert=true;}
                if(cb==900)c.reset();
                if(cb==950||cb==1300) {
                    const int m=cb==950?0:1;auto* slot=m?c.secondMic():&c;
                    const bool fallback=scenario==2&&m==1;
                    slot->requestedModel.store(fallback?0:responseKey(family,1-m));
                    slot->publish(std::move(replacements[size_t(2*l+m)]));
                }
                if(cb==1650)c.enable(false);
                if(cb==1800)c.enable(true);
            }
            auto& b=audio[size_t(l)];b.setSize(channels,count,false,false,true);
            for(int ch=0;ch<channels;++ch)for(int n=0;n<count;++n) {
                const int t=frame+n;
                float x=cb>2000?0.f:float(.075*std::sin(t*(.023+.037*ch+.011*l))+(t%53==0?.17:0));
                if(scenario==3&&ch==1)x=0;
                b.setSample(ch,n,x);
            }
        }
        unsigned hits=0,misses=0,overflow=0;allocations=deallocations=0;
#if CHIMERA_CAB_PROFILE
        spectralforge::cabProfile::Counters counters;
        spectralforge::cabProfile::ThreadCapture capture(&counters);
#endif
        const auto wallStart=std::chrono::steady_clock::now();const auto cpuStart=cpuNs();
        audioScope=true;
        for(auto& c:rigs)c->process(audio[size_t(&c-rigs.data())]);
        audioScope=false;
        const auto cpu=cpuNs()-cpuStart;
        const auto wall=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-wallStart).count();
#if EXPERIMENT_CANDIDATE
        for(const auto& c:rigs) {const auto* stats=c->cacheStats();hits+=stats->hits;misses+=stats->misses;overflow+=stats->overflow;}
#endif
        unsigned long long forwardCalls=0,forwardNs=0,macCalls=0,macNs=0;
#if CHIMERA_CAB_PROFILE
        const auto f=unsigned(spectralforge::cabProfile::Stage::FFTForward),m=unsigned(spectralforge::cabProfile::Stage::SpectralMAC);
        forwardCalls=counters.calls[f];forwardNs=counters.nanoseconds[f];macCalls=counters.calls[m];macNs=counters.nanoseconds[m];
#endif
        csv<<cb<<','<<count<<','<<cpu<<','<<wall<<','<<hits<<','<<misses<<','<<overflow<<','<<allocations<<','<<deallocations<<','<<forwardCalls<<','<<forwardNs<<','<<macCalls<<','<<macNs<<'\n';
        if(allocations||deallocations)throw std::runtime_error("audio C++ allocation/deallocation");
        if(dump)for(const auto& b:audio)for(int ch=0;ch<channels;++ch)dump.write(reinterpret_cast<const char*>(b.getReadPointer(ch)),std::streamsize(count*sizeof(float)));
        frame+=count;
    }
    std::cout<<"COMPLETE family="<<family<<" block="<<block<<" channels="<<channels<<" scenario="<<scenario<<" callbacks="<<callbacks<<" latency=0\n";
}
int main(int argc,char** argv) {try {
    if(argc!=9)throw std::runtime_error("family block channels scenario dump csv callbacks parallel");
    const int family=std::stoi(argv[1]),block=std::stoi(argv[2]),channels=std::stoi(argv[3]),scenario=std::stoi(argv[4]),callbacks=std::stoi(argv[7]);
    if(std::stoi(argv[8])) {
        // Two concurrent processors never share the cache or mutable state.
        std::exception_ptr failure[2];std::thread threads[2];
        for(int i=0;i<2;++i)threads[i]=std::thread([&,i]{try{run(family,block,channels,scenario,std::string(argv[5])+std::to_string(i),std::string(argv[6])+std::to_string(i),callbacks);}catch(...){failure[i]=std::current_exception();}});
        for(auto& t:threads)t.join();for(const auto& e:failure)if(e)std::rethrow_exception(e);
    } else run(family,block,channels,scenario,argv[5],argv[6],callbacks);
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
