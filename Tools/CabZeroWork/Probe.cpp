// Deterministic CAB-only replay. No worker scheduling or product gate changes.
#include "Cabinet.h"
#include "CabLayoutModel.h"
#include "CabTimingDiagnostics.h"
#include <bit>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <new>

thread_local bool watch=false;
thread_local uint64_t allocations=0,deletions=0;
void* operator new(std::size_t n){if(watch)++allocations;if(auto* p=std::malloc(std::max(n,size_t(1))))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {if(watch&&p)++deletions;std::free(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
using namespace spectralforge;
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
uint64_t hashWord(uint64_t h,uint64_t v){for(int b=0;b<8;++b){h^=(v>>(b*8))&255;h*=1099511628211ull;}return h;}
uint64_t slots(const std::array<Cab,3>& cabs) {
    uint64_t h=1469598103934665603ull;
    for(const auto& cab:cabs)for(const auto* c:{&cab,static_cast<const Cab*>(cab.secondMic())}) {
        const auto s=c->traceSnapshot();
        for(uint64_t v:{s.requestedModel,s.activeModel,s.fadingModel,uint64_t(s.activeGeneration),
            uint64_t(s.fadingGeneration),uint64_t(s.activePresent),uint64_t(s.fadingPresent),uint64_t(s.pendingPresent),
            uint64_t(s.fadeRemaining),uint64_t(s.fadeLength),uint64_t(s.rejected),uint64_t(s.requestedSource),uint64_t(s.activeSource)})h=hashWord(h,v);
    }return h;
}
struct Row {double wall{},cpu{};uint64_t pre{},post{},output{};int fadingBefore{},fadingAfter{};};
int main(int argc,char** argv) {try {
    require(argc==2,"usage: probe OUTPUT_DIRECTORY");
    const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
    juce::ScopedNoDenormals noDenormals;
    std::ofstream irs(out/"ir.csv");irs<<"engine,block,channels,mic,generation,key,samples,sample_hash,segments,bins\n";
    for(int engine=0;engine<3;++engine)for(int block:{32,64})for(int channels:{1,2}) {
        constexpr double rate=96000;constexpr int callbacks=2000;
        const std::string route=std::to_string(engine)+"-"+std::to_string(block)+"-"+std::to_string(channels);
        const juce::dsp::ProcessSpec spec{rate,juce::uint32(block),juce::uint32(channels)};
        std::array<Cab,3> cabs;std::array<juce::AudioBuffer<float>,3> audio;
        std::array<std::array<std::unique_ptr<Cab::Kernel>,6>,2> kernels;
        std::array<std::array<uint64_t,2>,2> keys{};
        for(int generation=0;generation<2;++generation)for(int mic=0;mic<2;++mic) {
            originalCab::Settings s{true,1,0,mic,mic,.3,.25,mic ? 18. : 10.};
            if(generation)s.position=.85;
            const auto key=engine==0 ? originalCab::key(s) : engine==1 ? cabExpansion::key({s,12,mic ? 10 : 20,2})
                : cabLayout::key({{s,9,mic ? 10 : 20,2},7,mic ? 7 : s.unit});
            keys[generation][mic]=key;const auto wave=cabLayout::generate(key,rate);
            juce::AudioBuffer<float> ir(1,int(wave.size()));ir.copyFrom(0,0,wave.data(),int(wave.size()));
            auto prepared=std::make_shared<const ModeledCabConvolution::Prepared>(ir,block,channels==1);
            uint64_t hash=1469598103934665603ull;for(float x:wave)hash=hashWord(hash,std::bit_cast<uint32_t>(x));
            irs<<engine<<','<<block<<','<<channels<<','<<mic<<','<<generation<<','<<key<<','<<wave.size()<<','<<hash<<','<<prepared->segments<<','<<prepared->bins<<'\n';
            for(int lane=0;lane<3;++lane) {
                juce::AudioBuffer<float> copy;copy.makeCopyOf(ir);
                kernels[generation][lane*2+mic]=std::make_unique<Cab::Kernel>(std::move(copy),rate,spec,0,generation+1,key,prepared);
            }
        }
        for(int lane=0;lane<3;++lane) {
            auto& c=cabs[lane];c.blend=.5f;c.prepare(spec);audio[lane].setSize(channels,block);
            for(int mic=0;mic<2;++mic) {auto* slot=mic ? c.secondMic() : &c;slot->requestedModel=keys[0][mic];slot->install(std::move(kernels[0][lane*2+mic]));}
        }
        std::vector<Row> rows(callbacks);
        std::vector<float> rendered(size_t(callbacks)*3*channels*block);
        size_t outputIndex=0;
        for(int n=0;n<callbacks;++n) {
            if(n==800)for(int lane=0;lane<3;++lane)for(int mic=0;mic<2;++mic) {
                auto* slot=mic ? cabs[lane].secondMic() : &cabs[lane];slot->requestedModel=keys[1][mic];
                slot->publish(std::move(kernels[1][lane*2+mic]));
            }
            // Complete deterministic publication occurs outside the measured
            // callback. No sleeps, priority, affinity or worker changes.
            for(int lane=0;lane<3;++lane)for(int ch=0;ch<channels;++ch)for(int k=0;k<block;++k) {
                const int t=n*block+k;
                const float x=float(.04*std::sin(t*(ch ? .113 : .071)+lane*.7)+(t%379==0 ? .11 : 0.));
                audio[lane].setSample(ch,k,x);
            }
            auto& row=rows[n];row.pre=slots(cabs);
            for(auto& c:cabs)row.fadingBefore+=c.transitioningMicCount();
            const auto cpu0=cabTiming::readThreadClock();const auto w0=std::chrono::steady_clock::now();
            watch=true;for(int lane=0;lane<3;++lane)cabs[lane].process(audio[lane]);watch=false;
            const auto w1=std::chrono::steady_clock::now();const auto cpu1=cabTiming::readThreadClock();
            require(cpu0.cpuUs>=0 && cpu1.cpuUs>=cpu0.cpuUs,"thread CPU clock unavailable");
            row.cpu=cpu1.cpuUs-cpu0.cpuUs;row.wall=std::chrono::duration<double,std::micro>(w1-w0).count();
            row.post=slots(cabs);row.output=1469598103934665603ull;
            for(auto& c:cabs){require(c.transitioningMicCount()<=1,"fade serialization");row.fadingAfter+=c.transitioningMicCount();}
            for(const auto& b:audio)for(int ch=0;ch<channels;++ch)for(int k=0;k<block;++k) {
                const float x=b.getSample(ch,k);require(std::isfinite(x),"nonfinite output");
                row.output=hashWord(row.output,std::bit_cast<uint32_t>(x));rendered[outputIndex++]=x;
            }
        }
        require(allocations==0 && deletions==0,"callback allocation/deletion");
        for(auto& c:cabs) {
            require(c.transitioningMicCount()==0,"fade did not drain");
            require(c.activeModel==keys[1][0] && c.secondMic()->activeModel==keys[1][1],"IR convergence");
            c.collect();c.secondMic()->collect();c.clear();require(!c.hasResources(),"resource leak");
        }
        std::ofstream csv(out/(route+".csv"));csv<<std::setprecision(12);
        csv<<"callback,phase,wall_us,cpu_us,budget_us,wall_miss,cpu_over,wall_only,state_before,state_after,output_hash,fading_before,fading_after,allocations,deletions\n";
        const double budget=1e6*block/rate;
        for(int n=0;n<callbacks;++n) {
            const auto& r=rows[n];const char* phase=n<400 ? "cold" : n<800 ? "steady_before" : n<1600 ? "publication" : "steady_after";
            csv<<n<<','<<phase<<','<<r.wall<<','<<r.cpu<<','<<budget<<','<<(r.wall>budget)<<','<<(r.cpu>budget)<<','<<(r.wall>budget&&r.cpu<=budget)<<','<<r.pre<<','<<r.post<<','<<r.output<<','<<r.fadingBefore<<','<<r.fadingAfter<<",0,0\n";
        }
        std::ofstream binary(out/(route+".f32"),std::ios::binary);binary.write(reinterpret_cast<const char*>(rendered.data()),std::streamsize(rendered.size()*sizeof(float)));
        std::cout<<"PASS route="<<route<<" callbacks="<<callbacks<<" budget_us="<<std::setprecision(12)<<budget<<" latency=0 allocation=0 deletion=0\n";
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
