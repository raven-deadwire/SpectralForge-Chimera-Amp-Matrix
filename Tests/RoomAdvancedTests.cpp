// Room Advanced 1.8, first part (docs/ROOM_ADVANCED_1_8.md): balance, decay,
// diffusion, wet low/high cut and width, measured on the room's wet return.
#include <juce_dsp/juce_dsp.h>
#include "RoomCore.h"
#include "reference/RoomCoreReleased.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {thread_local bool countAllocations=false;thread_local size_t allocations=0;}
void* operator new(std::size_t n) {if(countAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

namespace {
using namespace spectralforge;
constexpr double rate=48000,pi=3.14159265358979323846;constexpr int block=128;
void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
double db(double v){return 20*std::log10(std::max(v,1.e-30));}
// A host holding only the room parameters, so the room runs alone.
struct Host final : juce::AudioProcessor {
    juce::AudioProcessorValueTreeState state;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout(){juce::AudioProcessorValueTreeState::ParameterLayout l;appendRoomParameters(l);appendRoomAdvancedParameters(l);return l;}
    Host():state(*this,nullptr,"ROOM",layout()){}
    void set(const char* id,float v){auto* p=state.getParameter(id);require(p!=nullptr,std::string("no parameter ")+id);p->setValueNotifyingHost(p->convertTo0to1(v));}
    const juce::String getName() const override {return "room";}
    void prepareToPlay(double,int) override {}void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override {return 0;}bool acceptsMidi() const override {return false;}bool producesMidi() const override {return false;}
    juce::AudioProcessorEditor* createEditor() override {return nullptr;}bool hasEditor() const override {return false;}
    int getNumPrograms() override {return 1;}int getCurrentProgram() override {return 0;}void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return {};}void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}void setStateInformation(const void*,int) override {}
};
struct Stereo {std::vector<float> l,r;};
// Runs a room (current or released) on a stereo excitation and returns only its wet output.
template<class Room,class Input> Stereo wet(Host& host,Room& room,int samples,Input input) {
    room.bind(host.state);room.prepare({rate,(juce::uint32)block,2});Stereo out{std::vector<float>((size_t)samples),std::vector<float>((size_t)samples)};
    juce::AudioBuffer<float> output(2,block);
    for(int offset=0;offset<samples;offset+=block){
        auto& bus=room.bus(2,block);for(int n=0;n<block;++n){bus.setSample(0,n,input(offset+n,0));bus.setSample(1,n,input(offset+n,1));}
        output.clear();room.process(&bus,output);
        for(int n=0;n<block && offset+n<samples;++n){out.l[size_t(offset+n)]=output.getSample(0,n);out.r[size_t(offset+n)]=output.getSample(1,n);}
    }
    return out;
}
float noise(int n,int c){std::uint32_t x=std::uint32_t(n)*2654435761u^std::uint32_t(c*40503+12345);x^=x>>13;x*=0x5bd1e995u;x^=x>>15;return float(x&0xffff)/32768.f-1.f;}
auto impulse(){return [](int n,int c){return n==0&&c==0?1.f:0.f;};}
double energy(const std::vector<float>& v,int from,int to){double e=0;for(int n=from;n<to&&n<int(v.size());++n)e+=double(v[size_t(n)])*v[size_t(n)];return e;}

void parity() {
    // At the defaults the advanced room is the released Room Core, sample for sample.
    size_t compared=0;
    for(int size=0;size<3;++size)for(float absorption:{0.f,50.f,100.f}) {
        Host host;host.set("roomOn",1);host.set("roomSize",float(size));host.set("roomAbsorption",absorption);
        RoomCore now;room_core_released::RoomCore before;
        const auto x=[](int n,int c){return .3f*noise(n,c);};
        const auto a=wet(host,now,24000,x),b=wet(host,before,24000,x);
        for(size_t n=0;n<a.l.size();++n)require(a.l[n]==b.l[n] && a.r[n]==b.r[n],"Default room differs from Room Core");
        compared+=a.l.size()*2;
    }
    std::cout<<"PASS defaults bit-identical to Room Core: 3 sizes x 3 absorptions, "<<compared<<" samples\n";
}

void balanceAndDecay() {
    const auto response=[](float balance,float decay,float diffusion=0){Host h;h.set("roomOn",1);h.set("roomEarlyLate",balance);h.set("roomDecay",decay);h.set("roomDiffusion",diffusion);RoomCore r;return wet(h,r,96000,impulse());};
    const auto calibrated=response(0,100),early=response(-100,100),late=response(100,100);
    const int tailStart=int(rate*.2);
    const double earlyTail=energy(early.l,tailStart,96000),earlyFirst=energy(early.l,0,int(rate*.04));
    std::cout<<"MEASURE balance -100 %: tail energy after 200 ms "<<earlyTail<<"; early window "<<db(std::sqrt(earlyFirst))<<" dB\n";
    require(earlyTail==0 && earlyFirst>0,"Early-only balance still carries the late tail");
    // Late only + early only = calibrated (both weights are linear, 1 at 0 %).
    double residual=0;for(size_t n=0;n<calibrated.l.size();++n)residual=std::max(residual,double(std::abs(calibrated.l[n]-early.l[n]-late.l[n])));
    std::cout<<"MEASURE early-only + late-only vs calibrated: largest difference "<<residual<<"\n";
    require(residual<1.e-6,"Balance weights do not split the calibrated room");
    // Decay: T30-style slope of the late energy, 50 % / 100 % / 200 %.
    const auto t60=[&](const Stereo& r){const auto level=[&](double t){const int n=int(t*rate);return 10*std::log10(energy(r.l,n,n+int(rate*.05))+1.e-30);};return 60.*(.6-.2)/(level(.2)-level(.6));};
    const double shorter=t60(response(0,50)),nominal=t60(calibrated),longer=t60(response(0,200));
    std::cout<<"MEASURE decay (Medium, 50 % absorption, design 0.7 s): 50 % "<<shorter<<" s, 100 % "<<nominal<<" s, 200 % "<<longer<<" s\n";
    require(std::abs(shorter/nominal-.5)<.08 && std::abs(longer/nominal-2)<.25,"Decay does not scale the late RT60");
    Host h;h.set("roomOn",1);RoomCore r;r.bind(h.state);const double tail100=r.tailSeconds();h.set("roomDecay",200);const double tail200=r.tailSeconds();
    std::cout<<"MEASURE host tail: "<<tail100<<" s at 100 %, "<<tail200<<" s at 200 %\n";
    require(tail200>tail100*1.8,"Host tail does not follow the decay");
    // Diffusion: the build-up of the tail is smoother (lower crest factor in
    // 10 ms frames). The all-passes are lossless, so the total wet energy is
    // unchanged; they move the energy centroid later, which a window that
    // starts mid-tail reads as a level rise.
    const auto crest=[](const Stereo& r){double worst=0;for(int f=int(rate*.03);f<int(rate*.12);f+=480){double peak=0,e=0;for(int n=f;n<f+480;++n){peak=std::max(peak,double(std::abs(r.l[size_t(n)])));e+=double(r.l[size_t(n)])*r.l[size_t(n)];}worst+=peak/std::sqrt(e/480+1.e-30);}return worst;};
    const auto diffuse=response(100,100,100),plain=response(100,100,0);
    const double crestPlain=crest(plain),crestDiffuse=crest(diffuse),lateChange=10*std::log10((energy(diffuse.l,0,96000)+energy(diffuse.r,0,96000))/(energy(plain.l,0,96000)+energy(plain.r,0,96000)));
    const auto centroid=[](const Stereo& r){double e=0,t=0;for(size_t n=0;n<r.l.size();++n){const double v=double(r.l[n])*r.l[n]+double(r.r[n])*r.r[n];e+=v;t+=v*double(n);}return t/e/rate;};
    const double centroidShift=centroid(diffuse)-centroid(plain);
    std::cout<<"MEASURE diffusion 100 % (late only): summed crest factor 30-120 ms "<<crestPlain<<" -> "<<crestDiffuse<<"; total wet energy change "<<lateChange<<" dB; energy centroid "<<centroidShift*1000<<" ms later\n";
    require(crestDiffuse<crestPlain*.9 && std::abs(lateChange)<.5,"Diffusion does not smooth the tail build-up");
    std::cout<<"PASS early/late balance, decay scaling and host tail, diffusion\n";
}

void filtersAndWidth() {
    // The wet path is linear: a steady sine's wet level with a cut, over the
    // same without, is the cut's response (12 dB/oct Butterworth).
    const auto level=[](float low,float high,double hz){Host h;h.set("roomOn",1);h.set("roomLowCut",low);h.set("roomHighCut",high);RoomCore r;
        const auto out=wet(h,r,72000,[hz](int n,int){return float(.2*std::sin(2*pi*hz*n/rate));});return std::sqrt(energy(out.l,36000,72000));};
    const auto change=[&](float low,float high,double hz){return db(level(low,high,hz)/level(20,20000,hz));};
    const auto butterworth=[](double ratio){return db(1/std::sqrt(1+std::pow(ratio,4)));};   // ratio = fc/f (high-pass) or f/fc (low-pass)
    const auto warp=[](double hz){return std::tan(pi*hz/rate);};
    const double hp200=change(200,20000,200),hp100=change(200,20000,100),lp5k=change(20,5000,5000),lp10k=change(20,5000,10000);
    const double hp100Expected=butterworth(warp(200)/warp(100)),lp10kExpected=butterworth(warp(10000)/warp(5000));
    std::cout<<"MEASURE low cut 200 Hz: "<<hp200<<" dB at 200 Hz, "<<hp100<<" dB at 100 Hz (analytic "<<hp100Expected<<"); high cut 5 kHz: "<<lp5k<<" dB at 5 kHz, "<<lp10k<<" dB at 10 kHz (analytic "<<lp10kExpected<<")\n";
    require(std::abs(hp200+3.01)<.15 && std::abs(hp100-hp100Expected)<.15 && std::abs(lp5k+3.01)<.15 && std::abs(lp10k-lp10kExpected)<.15,"Wet cuts differ from the 12 dB/oct response");
    // Width: 0 % is mono, 200 % doubles the side signal (4x its energy).
    const auto widthRun=[](float width){Host h;h.set("roomOn",1);h.set("roomWidth",width);RoomCore r;return wet(h,r,48000,[](int n,int c){return .3f*noise(n,c);});};
    const auto mono=widthRun(0),normal=widthRun(100),wide=widthRun(200);
    double monoSide=0;for(size_t n=24000;n<mono.l.size();++n)monoSide=std::max(monoSide,double(std::abs(mono.l[n]-mono.r[n])));
    const auto sideEnergy=[](const Stereo& s){double e=0;for(size_t n=24000;n<s.l.size();++n){const double d=.5*(double(s.l[n])-s.r[n]);e+=d*d;}return e;};
    const auto midEnergy=[](const Stereo& s){double e=0;for(size_t n=24000;n<s.l.size();++n){const double m=.5*(double(s.l[n])+s.r[n]);e+=m*m;}return e;};
    const double sideRatio=sideEnergy(wide)/sideEnergy(normal),midRatio=midEnergy(wide)/midEnergy(normal);
    std::cout<<"MEASURE width: 0 % largest L-R "<<monoSide<<"; 200 % side energy x"<<sideRatio<<", mid energy x"<<midRatio<<"\n";
    require(monoSide<1.e-6 && std::abs(sideRatio-4)<.02 && std::abs(midRatio-1)<.001,"Width does not scale the side signal");
    // Switching a cut in while playing: 20 ms mix, no step.
    Host h;h.set("roomOn",1);RoomCore r;r.bind(h.state);r.prepare({rate,(juce::uint32)block,2});juce::AudioBuffer<float> out(2,block);std::vector<float> y;
    for(int k=0;k<750;++k){if(k==375)h.set("roomLowCut",400);auto& bus=r.bus(2,block);for(int n=0;n<block;++n){const float x=float(.2*std::sin(2*pi*220*(k*block+n)/rate));bus.setSample(0,n,x);bus.setSample(1,n,x);}out.clear();r.process(&bus,out);for(int n=0;n<block;++n)y.push_back(out.getSample(0,n));}
    // The cut changes the steady wet sound, so the switch is compared with the
    // larger of the settled states before and after it.
    double before=0,after=0,switching=0;for(size_t n=2;n<y.size();++n){const double d=std::abs(y[n]-2*y[n-1]+y[n-2]);if(n>20000&&n<48000)before=std::max(before,d);if(n>=48000&&n<50000)switching=std::max(switching,d);if(n>=60000)after=std::max(after,d);}
    std::cout<<"MEASURE switching the low cut in: largest second difference "<<switching<<"; settled "<<before<<" before, "<<after<<" after\n";
    require(switching<std::max(before,after)*1.5,"Switching a wet cut in steps the return");
    std::cout<<"PASS wet low/high cut responses, width, click-free switching\n";
}

void realtime() {
    Host h;for(auto [id,v]:std::initializer_list<std::pair<const char*,float>>{{"roomOn",1},{"roomEarlyLate",30},{"roomDecay",250},{"roomDiffusion",70},{"roomLowCut",150},{"roomHighCut",6000},{"roomWidth",160}})h.set(id,v);
    RoomCore r;r.bind(h.state);r.prepare({rate,(juce::uint32)block,2});juce::AudioBuffer<float> out(2,block);
    // The host-side parameter change is outside the count: only the audio callback is measured.
    for(int k=0;k<400;++k){if(k==200)h.set("roomDecay",60);auto& bus=r.bus(2,block);for(int n=0;n<block;++n){bus.setSample(0,n,.3f*noise(k*block+n,0));bus.setSample(1,n,.3f*noise(k*block+n,1));}out.clear();countAllocations=true;r.process(&bus,out);countAllocations=false;}
    bool finite=true;for(int c=0;c<2;++c)for(int n=0;n<block;++n)finite=finite&&std::isfinite(out.getSample(c,n));
    std::cout<<"MEASURE allocations "<<allocations<<", finite "<<finite<<"\n";require(allocations==0 && finite,"Room Advanced allocated or became non-finite");
    std::cout<<"PASS no allocation with every advanced control engaged and a decay change\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {parity();balanceAndDecay();filtersAndWidth();realtime();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
