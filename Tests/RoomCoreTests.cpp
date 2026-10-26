// Room Core 1.3.3 plumbing contracts (docs/ROOM_CORE_1_3_3.md section 7):
// room off is bit-identical and costs nothing, Matrix LOW DI never excites
// the room, lane controls and post-rig gates act on the send bus exactly as
// on the direct sound, and the callback stays allocation-free with the room on.
#include "PluginProcessor.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

namespace {thread_local bool countAllocations=false;thread_local size_t allocations=0;}
void* operator new(std::size_t n) {if(countAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
#if defined(__GLIBC__)
extern "C" {
void* __libc_malloc(std::size_t);void* __libc_calloc(std::size_t,std::size_t);void* __libc_realloc(void*,std::size_t);
void* malloc(std::size_t n) {if(countAllocations)++allocations;return __libc_malloc(n);}
void* calloc(std::size_t n,std::size_t s) {if(countAllocations)++allocations;return __libc_calloc(n,s);}
void* realloc(void* p,std::size_t n) {if(countAllocations)++allocations;return __libc_realloc(p,n);}
}
#endif

namespace {
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* parameter=p.parameters().getParameter(id);require(parameter!=nullptr,"Unknown room test parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
constexpr double rate=48000;constexpr int block=128;
std::unique_ptr<ChimeraProcessor> make() {
    auto p=std::make_unique<ChimeraProcessor>();
    set(*p,"gateon",0);set(*p,"input",0);set(*p,"output",0);set(*p,"oversampling",0);
    p->setRateAndBufferSizeDetails(rate,block);p->prepareToPlay(rate,block);
    return p;
}
// Renders `blocks` blocks of a two-tone signal; returns the output and the
// largest send-bus peak the room saw.
struct Render {std::vector<float> out;float roomPeak{};};
Render render(ChimeraProcessor& p,int blocks,double hz,float level,int& clock) {
    Render r;juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;
    for(int b=0;b<blocks;++b) {
        for(int n=0;n<block;++n){const float x=level*float(std::sin(juce::MathConstants<double>::twoPi*hz*double(clock+n)/rate));audio.setSample(0,n,x);audio.setSample(1,n,x);}
        clock+=block;p.processBlock(audio,midi);
        for(int n=0;n<block;++n)r.out.push_back(audio.getSample(0,n));
        r.roomPeak=std::max(r.roomPeak,p.roomInputPeak());
    }
    return r;
}
// Peak over the last blocks only, once smoothing has settled.
float settledPeak(ChimeraProcessor& p,double hz,float level,int& clock) {
    render(p,60,hz,level,clock);return render(p,20,hz,level,clock).roomPeak;
}

void parameterContract() {
    const auto p=std::make_unique<ChimeraProcessor>();
    int index=p->getParameters().size()-spectralforge::roomParameterCount;
    for(const auto* id:{"roomOn","roomSize","roomAbsorption","roomReturn","roomSend1","roomSend2","roomSend3"}) {
        const auto* parameter=p->parameters().getParameter(id);
        require(parameter && parameter->getParameterIndex()==index++ && parameter->getVersionHint()==spectralforge::roomParameterVersion && parameter->isAutomatable(),
                "Room Core parameters must append last, in order, at version 15");
    }
    require(p->parameters().getRawParameterValue("roomOn")->load()<.5f,"The room must default to off");
    std::cout<<"PASS Room Core parameters: 7 appended last at version 15, room off by default\n";
}
void roomOffIsIdentical() {
    auto plain=make(),changed=make();
    set(*changed,"roomSize",2);set(*changed,"roomAbsorption",10);set(*changed,"roomReturn",-20);
    for(int lane=1;lane<=3;++lane)set(*changed,"roomSend"+juce::String(lane),100);
    int a=0,b=0;
    const auto x=render(*plain,200,110.,.2f,a),y=render(*changed,200,110.,.2f,b);
    require(x.out==y.out,"Room off output depends on room parameters");
    require(plain->roomProcessedSamples()==0 && changed->roomProcessedSamples()==0,"Room off still ran room processing");
    std::cout<<"PASS room off: output bit-identical whatever the room settings, zero room samples processed\n";
}
void lowDiIsolation() {
    auto p=make();
    set(*p,"mode",2);set(*p,"mute2",1);set(*p,"mute3",1);   // only the LOW lane sounds
    set(*p,"roomOn",1);set(*p,"roomSend1",100);set(*p,"roomSend2",100);set(*p,"roomSend3",100);
    int clock=0;
    set(*p,"lowampmix",0);   // DI 100 %
    const float di=settledPeak(*p,60.,.2f,clock);
    set(*p,"lowampmix",1);
    const float amp=settledPeak(*p,60.,.2f,clock);
    set(*p,"ampon1",0);      // LOW AMP off, blend still at AMP
    const float ampOff=settledPeak(*p,60.,.2f,clock);
    std::cout<<"MEASURE Matrix LOW room excitation: DI 100 % "<<di<<", AMP 100 % "<<amp<<", LOW AMP off "<<ampOff<<"\n";
    require(di==0.f,"Matrix LOW DI excites the room");
    require(amp>1.e-3f,"Matrix LOW AMP/CAB does not excite the room");
    require(ampOff==0.f,"Matrix LOW with the amp off excites the room");
    std::cout<<"PASS Matrix LOW: DI 100 % and LOW AMP off send exactly nothing, AMP/CAB share excites\n";
}
void laneControls() {
    auto p=make();set(*p,"roomOn",1);set(*p,"roomSend1",100);
    int clock=0;
    const float open=settledPeak(*p,220.,.2f,clock);
    set(*p,"mute1",1);const float muted=settledPeak(*p,220.,.2f,clock);set(*p,"mute1",0);
    set(*p,"roomSend1",0);const float noSend=settledPeak(*p,220.,.2f,clock);
    require(open>1.e-3f,"Classic lane does not excite the room");
    require(muted==0.f && noSend==0.f,"A muted lane or a zero send still excites the room");
    // Lane weighting: Dual blend gives rig 2 a gain of `blend`. (The legacy
    // levelN is forced to 0 dB for native amps, the default, so it is not used.)
    set(*p,"mode",1);set(*p,"dualtype",0);set(*p,"roomSend1",0);set(*p,"roomSend2",100);
    set(*p,"dualblend",0);const float none=settledPeak(*p,220.,.2f,clock);
    set(*p,"dualblend",1);const float full=settledPeak(*p,220.,.2f,clock);
    set(*p,"dualblend",.5f);const float half=settledPeak(*p,220.,.2f,clock);
    const double drop=20*std::log10(double(half)/double(full));
    std::cout<<"MEASURE Dual rig 2 room excitation: blend 0 "<<none<<", blend 1 "<<full<<", blend 0.5 "<<drop<<" dB re blend 1\n";
    require(none==0.f,"A rig at zero Dual blend still excites the room");
    require(full>1.e-3f && std::abs(drop+6.02)<.3,"Dual blend does not scale the room send like the direct sound");
    std::cout<<"PASS lane controls: mute and send 0 give exactly zero excitation; Dual blend scales the send as the direct sound\n";
}
void gatePolicy() {
    auto p=make();set(*p,"roomOn",1);set(*p,"roomSend1",100);
    int clock=0;
    // One rig at send 100 %: the send bus is the direct sum, so both must
    // change by the same ratio when the post-rig gate attenuates (the later
    // default stages are linear). Peaks over the same settled blocks.
    const auto peaks=[&](float& room,float& direct){render(*p,60,220.,.02f,clock);const auto r=render(*p,20,220.,.02f,clock);
        room=r.roomPeak;direct=0;for(const float v:r.out)direct=std::max(direct,std::abs(v));};
    float openRoom{},openDirect{};peaks(openRoom,openDirect);
    // -34 dBFS signal under the highest threshold: the post-rig gate attenuates.
    set(*p,"gateAfterRig",1);set(*p,"gateon",1);set(*p,"gatethreshold",-15);
    for(const float gateMode:{0.f,1.f}) {
        set(*p,"gateMode",gateMode);
        float room{},direct{};peaks(room,direct);
        const double roomCut=20*std::log10(std::max(1.e-12,double(room))/double(openRoom));
        const double directCut=20*std::log10(std::max(1.e-12,double(direct))/double(openDirect));
        std::cout<<"MEASURE post-rig "<<(gateMode>.5f?"expander":"gate")<<": room send "<<roomCut<<" dB, direct output "<<directCut<<" dB\n";
        require(directCut<-10,"The post-rig gate did not attenuate in this test");
        require(std::abs(roomCut-directCut)<.5,"The room send is not attenuated like the direct sound by the post-rig gate");
    }
    std::cout<<"PASS gate policy: the room send follows the post-rig gate exactly like the direct sound (gate and expander modes)\n";
}
void noAllocation() {
    auto p=make();set(*p,"roomOn",1);for(int lane=1;lane<=3;++lane)set(*p,"roomSend"+juce::String(lane),100);
    int clock=0;render(*p,20,110.,.2f,clock);
    juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;
    for(int mode=0;mode<3;++mode) {
        set(*p,"mode",float(mode));render(*p,20,110.,.2f,clock);
        countAllocations=true;allocations=0;
        for(int b=0;b<50;++b){for(int c=0;c<2;++c)for(int n=0;n<block;++n)audio.setSample(c,n,.1f*std::sin(float(clock+n)*.01f));clock+=block;p->processBlock(audio,midi);}
        countAllocations=false;
        require(allocations==0,"Room on allocates in the audio callback");
    }
    std::cout<<"PASS room on: no allocation in the callback in Classic, Dual and Matrix\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {parameterContract();roomOffIsIdentical();lowDiIsolation();laneControls();gatePolicy();noAllocation();return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
