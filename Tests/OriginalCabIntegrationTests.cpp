#include "IRLibrary.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
thread_local bool watch=false;
thread_local size_t allocations=0, deletions=0;
void* operator new(std::size_t n){if(watch)++allocations;if(auto* p=std::malloc(std::max(n,size_t(1))))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {if(watch && p)++deletions;std::free(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
using namespace spectralforge;
void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
void process(Cab& c,juce::AudioBuffer<float>& b){watch=true;c.process(b);watch=false;}
std::vector<float> render(Cab& cab,const juce::dsp::ProcessSpec& spec) {
    juce::AudioBuffer<float> b(int(spec.numChannels),int(spec.maximumBlockSize));
    for(int n=0;n<200;++n){b.clear();process(cab,b);}cab.reset();
    std::vector<float> result;
    for(int block=0;block<150;++block){b.clear();if(block==0)b.setSample(0,0,.25f);process(cab,b);
        for(int n=0;n<b.getNumSamples();++n)result.push_back(b.getSample(0,n));
        if(b.getNumChannels()==2)require(b.getMagnitude(1,0,b.getNumSamples())<1e-8,"no stereo crosstalk");}
    return result;
}
double delta(const std::vector<float>& a,const std::vector<float>& b){double d=0;for(size_t n=0;n<a.size();++n)d=std::max(d,std::abs(double(a[n]-b[n])));return d;}
int main(){try {
    for(double sr:{44100.,48000.,96000.})for(int block:{64,256})for(int channels:{1,2}) {
        juce::dsp::ProcessSpec spec{sr,juce::uint32(block),juce::uint32(channels)};
        std::array<Cab,3> cabs;for(auto& c:cabs)c.prepare(spec);
        IRLibrary library({&cabs[0],&cabs[1],&cabs[2]});
        originalCab::Settings a{true,1,0,0,0,.3,.25,10},b=a;b.unit=1;b.mic=1;b.distanceCm=18;
        for(auto& cab:cabs){cab.requestedModel=originalCab::key(a);cab.secondMic()->requestedModel=originalCab::key(b);cab.blend=.5;}
        library.prepare(spec,{0,0,0});library.stop();
        auto& cab=cabs[0];cab.blend=0;const auto outA=render(cab,spec);cab.blend=1;const auto outB=render(cab,spec);
        require(delta(outA,outB)>1e-4,"independent same cabinet / different unit / mic");
        cab.blend=.5;const auto mix=render(cab,spec);auto expected=outA;
        for(size_t n=0;n<expected.size();++n)expected[n]=.5f*(outA[n]+outB[n]);require(delta(mix,expected)<2e-6,"modeled constant-sum blend");
        cab.secondMic()->requestedModel=originalCab::key(a);library.prepare(spec,{0,0,0});library.stop();
        require(delta(outA,render(cab,spec))<2e-6,"same unit and same mic unity at midpoint");
        // Six stereo mic paths, including publication overlap and latest-request convergence.
        for(auto& c:cabs)c.blend=.5;
        library.prepare(spec,{0,0,0});
        juce::AudioBuffer<float> audio(channels,block);
        std::array<juce::AudioBuffer<float>,3> laneAudio;for(auto& lane:laneAudio)lane.setSize(channels,block);
        std::vector<double> times;times.reserve(1600);
        unsigned misses=0;double peak=0,maxStep=0;float previous=0;
        for(int n=0;n<1200;++n) {
            if(n<80)for(auto& c:cabs){a.position=double(n%60)/60;c.requestedModel=originalCab::key(a);b.distanceCm=5+n*.2;c.secondMic()->requestedModel=originalCab::key(b);}
            for(int ch=0;ch<channels;++ch)for(int k=0;k<block;++k)audio.setSample(ch,k,float(.05*std::sin((n*block+k)*.07)));
            for(auto& lane:laneAudio)lane.makeCopyOf(audio,true);
            const auto start=std::chrono::steady_clock::now();
            // Independent lane inputs; copying is outside this CAB-only measurement.
            for(int lane=0;lane<3;++lane)process(cabs[lane],laneAudio[lane]);
            const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();times.push_back(us);if(us>1e6*block/sr)++misses;
            for(int k=0;k<block;++k){const float x=laneAudio[0].getSample(0,k);require(std::isfinite(x),"finite transitions");peak=std::max(peak,std::abs(double(x)));maxStep=std::max(maxStep,std::abs(double(x-previous)));previous=x;}
            if(n%8==0)juce::Thread::sleep(1);
        }
        bool ready=false;
        for(int n=0;n<10000 && !ready;++n) {
            ready=true;for(auto& c:cabs){audio.clear();process(c,audio);ready=ready && c.activeModel==c.requestedModel && c.secondMic()->activeModel==c.secondMic()->requestedModel;}
            if(!ready)juce::Thread::sleep(1);
        }
        require(ready,"latest generation converges after automation storm");require(peak<2 && maxStep<.3,"bounded swap transition");
        library.stop();
        // Deterministic worst-case publication: six complete engines ready before
        // the same callback. Construction is outside the watched/timed callback.
        a.position=.85;const auto nextKey=originalCab::key(a);const auto wave=originalCab::generate(a,sr);
        for(auto& c:cabs)for(auto* slot:{&c,c.secondMic()}) {
            juce::AudioBuffer<float> samples(1,int(wave.size()));samples.copyFrom(0,0,wave.data(),int(wave.size()));
            slot->requestedModel=nextKey;slot->publish(std::make_unique<Cab::Kernel>(std::move(samples),sr,spec,0,100,nextKey));
        }
        for(int n=0;n<400;++n) {
            for(auto& lane:laneAudio)for(int ch=0;ch<channels;++ch)for(int k=0;k<block;++k)lane.setSample(ch,k,float(.05*std::sin(((n+1200)*block+k)*.07)));
            const auto start=std::chrono::steady_clock::now();for(int lane=0;lane<3;++lane)process(cabs[lane],laneAudio[lane]);
            const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();times.push_back(us);if(us>1e6*block/sr)++misses;
            for(int k=0;k<block;++k){const auto x=laneAudio[0].getSample(0,k);require(std::isfinite(x),"finite six-slot swap");peak=std::max(peak,std::abs(double(x)));maxStep=std::max(maxStep,std::abs(double(x-previous)));previous=x;}
        }
        require(peak<2 && maxStep<.3,"bounded six-slot swap");
        for(auto& c:cabs)c.clear();require(library.resourcesReleased(),"worker teardown");
        std::sort(times.begin(),times.end());
        require(times[1584]<1e6*block/sr,"CAB-only p99 exceeds one block period (runner CPU gate)");
        std::cout<<"TIMING sr="<<sr<<" block="<<block<<" channels="<<channels<<" three_cabs_six_mics p50_us="<<times[800]<<" p99_us="<<times[1584]<<" max_us="<<times.back()<<" misses="<<misses<<"/1600 peak="<<peak<<" max_step="<<maxStep<<'\n';
    }
    require(allocations==0 && deletions==0,"callback new/delete observed");
    std::cout<<"PASS callback_new="<<allocations<<" callback_delete="<<deletions<<" (thread-local C++ operators only; not a universal malloc/lock tracer)\n";return 0;
}catch(const std::exception& e){watch=false;std::cerr<<e.what()<<'\n';return 1;}}
