// Room Core 1.3.3 plumbing contracts (docs/ROOM_CORE_1_3_3.md section 7):
// room off is bit-identical and costs nothing, Matrix LOW DI never excites
// the room, lane controls and post-rig gates act on the send bus exactly as
// on the direct sound, and the callback stays allocation-free with the room on.
#include "PluginProcessor.h"
#include "RoomModel.h"
#include "CabRoom.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
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
    int index=p->getParameters().size()-spectralforge::roomParameterCount-spectralforge::roomAdvancedParameterCount-spectralforge::roomSpaceParameterCount;
    for(const auto* id:{"roomOn","roomSize","roomAbsorption","roomReturn","roomSend1","roomSend2","roomSend3"}) {
        const auto* parameter=p->parameters().getParameter(id);
        require(parameter && parameter->getParameterIndex()==index++ && parameter->getVersionHint()==spectralforge::roomParameterVersion && parameter->isAutomatable(),
                "Room Core parameters must append in order at version 15, before Room Advanced");
    }
    require(p->parameters().getRawParameterValue("roomOn")->load()<.5f,"The room must default to off");
    std::cout<<"PASS Room Core parameters: 7 appended in order at version 15, room off by default\n";
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
    // The room bus is compared with the direct sound of an identical processor
    // whose room is off (the room's own return is deliberately not gated).
    auto p=make(),direct=make();set(*p,"roomOn",1);set(*p,"roomSend1",100);
    int clock=0,directClock=0;
    const auto peaks=[&](float& room,float& dry){render(*p,60,220.,.02f,clock);room=render(*p,20,220.,.02f,clock).roomPeak;
        render(*direct,60,220.,.02f,directClock);const auto d=render(*direct,20,220.,.02f,directClock);dry=0;for(const float v:d.out)dry=std::max(dry,std::abs(v));};
    float openRoom{},openDirect{};peaks(openRoom,openDirect);
    // -34 dBFS signal under the highest threshold: the post-rig gate attenuates.
    for(auto* q:{p.get(),direct.get()}){set(*q,"gateAfterRig",1);set(*q,"gateon",1);set(*q,"gatethreshold",-15);}
    for(const float gateMode:{0.f,1.f}) {
        set(*p,"gateMode",gateMode);set(*direct,"gateMode",gateMode);
        float room{},dry{};peaks(room,dry);
        const double roomCut=20*std::log10(std::max(1.e-12,double(room))/double(openRoom));
        const double directCut=20*std::log10(std::max(1.e-12,double(dry))/double(openDirect));
        std::cout<<"MEASURE post-rig "<<(gateMode>.5f?"expander":"gate")<<": room send "<<roomCut<<" dB, direct output (room off) "<<directCut<<" dB\n";
        require(directCut<-10,"The post-rig gate did not attenuate in this test");
        require(std::abs(roomCut-directCut)<.1,"The room send is not attenuated like the direct sound by the post-rig gate");
    }
    std::cout<<"PASS gate policy: the room send follows the post-rig gate exactly like the direct sound (gate and expander modes)\n";
}
// Room model alone (docs/ROOM_CORE_1_3_3.md section 4).
double rt60(const std::vector<float>& h,double sampleRate,size_t from) {
    std::vector<double> e(h.size()+1,0.);for(size_t i=h.size();i-->from;)e[i]=e[i+1]+double(h[i])*h[i];
    size_t a=0,b=0;for(size_t i=from;i<h.size();++i){const double db=10*std::log10(e[i]/e[from]+1e-300);if(a==0&&db<=-5)a=i;if(db<=-25){b=i;break;}}
    return a&&b ? 3.0*double(b-a)/sampleRate : -1;
}
void roomModelResponse() {
    const std::array<double,3> windowMs{21,41,82};
    for(double sampleRate:{44100.,48000.,96000.,192000.})for(int size=0;size<3;++size) {
        double previousHf=1e9;
        for(float absorption:{0.f,.5f,1.f}) {
            spectralforge::RoomModel model;model.prepare(sampleRate);model.configure(size,absorption);
            const size_t n=size_t(sampleRate*(size==2?6:3));std::vector<float> in(n,0.f),left(n,0.f),right(n,0.f);in[0]=1;
            for(size_t i=0;i<n;i+=512){const int k=int(std::min<size_t>(512,n-i));const float* ins[2]{in.data()+i,in.data()+i};float* outs[2]{left.data()+i,right.data()+i};model.process(ins,outs,2,k,1.f);}
            const double measured=rt60(left,sampleRate,size_t(.1*sampleRate)),target=spectralforge::RoomModel::geometry(size).rt60AtHalf*spectralforge::RoomModel::rt60Scale(absorption);
            double all=0,hf=0,energy=0;
            for(size_t i=size_t(.15*sampleRate)+1;i<size_t(.4*sampleRate);++i){all+=double(left[i])*left[i];const double d=left[i]-left[i-1];hf+=d*d;}
            for(size_t i=0;i<n;++i)energy+=.5*(double(left[i])*left[i]+double(right[i])*right[i]);
            const double lastMs=1000.*model.lastReflectionDelay()/sampleRate;
            require(std::abs(measured/target-1)<.1,"Room RT60 is not within 10 % of its target");
            require(lastMs<=windowMs[size_t(size)] && model.reflectionCount(0)==24 && model.reflectionCount(1)==24,"Early reflections fall outside the declared window");
            require(std::isfinite(left.back()) && std::abs(left.back())<1.e-4f,"Room tail does not decay");
            require(hf/all<previousHf,"More absorption does not darken the late tail");previousHf=hf/all;
            if(absorption==.5f && sampleRate==48000.) {
                std::cout<<"MEASURE room size "<<size<<" at 48 kHz, 50 % absorption: RT60 "<<measured<<" s (target "<<target<<"), last reflection "<<lastMs<<" ms, total wet "<<10*std::log10(energy)<<" dB\n";
                require(std::abs(10*std::log10(energy)+9.0)<.3,"Calibrated wet level is not -9 dB at 50 % absorption");
            }
        }
    }
    std::cout<<"PASS room model: RT60 within 10 %, reflections inside 21/41/82 ms, absorption darkens the tail, decays at 44.1-192 kHz, wet -9 dB at 50 %\n";
}
void roomAudibleAndRingOut() {
    auto p=make(),plain=make();set(*p,"roomOn",1);set(*p,"roomSend1",100);
    int a=0,b=0;
    const auto withRoom=render(*p,200,220.,.2f,a),without=render(*plain,200,220.,.2f,b);
    double diff=0,ref=0;for(size_t i=0;i<withRoom.out.size();++i){diff+=double(withRoom.out[i]-without.out[i])*(withRoom.out[i]-without.out[i]);ref+=double(without.out[i])*without.out[i];}
    std::cout<<"MEASURE room on, send 100 %: output differs from room off by "<<10*std::log10(diff/ref)<<" dB\n";
    require(diff/ref>1.e-3,"Room on does not change the output");
    // The host is told about the room's tail while it is on.
    const double reportedTail=p->getTailLengthSeconds()-plain->getTailLengthSeconds();
    require(reportedTail>0.,"Room on does not extend the reported tail");
    // Room off: no new input, the tail rings out, then the room goes idle.
    set(*p,"roomOn",0);
    require(p->getTailLengthSeconds()==plain->getTailLengthSeconds(),"Room off still reports a room tail");
    const auto running=p->roomProcessedSamples();render(*p,10,220.,.2f,a);render(*plain,10,220.,.2f,b);
    require(p->roomProcessedSamples()>running,"Room off cut the tail instead of letting it ring out");
    // Medium room: decaying ~110 dB to the -120 dBFS floor at RT60 0.7 s takes ~1.3 s.
    int ringBlocks=0;
    for(;ringBlocks<4000;++ringBlocks){const auto before=p->roomProcessedSamples();render(*p,1,220.,.2f,a);render(*plain,1,220.,.2f,b);if(p->roomProcessedSamples()==before)break;}
    require(ringBlocks<4000,"The room tail never ended");
    const auto idle=p->roomProcessedSamples();
    std::cout<<"MEASURE Room off ring-out: "<<1000.*ringBlocks*block/rate<<" ms until idle (reported tail "<<1000.*reportedTail<<" ms)\n";
    require(double(ringBlocks)*block/rate<=reportedTail,"The room rings longer than the tail it reports to the host");
    const auto after=render(*p,50,220.,.2f,a),reference=render(*plain,50,220.,.2f,b);
    require(p->roomProcessedSamples()==idle,"The room keeps processing after its tail has ended");
    require(after.out==reference.out,"After the tail the output is not identical to a processor without a room");
    std::cout<<"PASS Room off: the tail rings out, then the room idles and the output is bit-identical to room off\n";
}
void switchingIsSmooth() {
    // Every switch on a held low tone: the wet part (output minus a room-off
    // twin) must show no edge (second difference) beyond twice its steady one.
    auto p=make(),plain=make();set(*p,"roomSend1",100);int a=0,b=0;
    const auto edge=[&](int blocks){const auto x=render(*p,blocks,41.2,.2f,a),y=render(*plain,blocks,41.2,.2f,b);float e=0;
        const auto wet=[&](size_t i){return x.out[i]-y.out[i];};
        for(size_t i=2;i<x.out.size();++i)e=std::max(e,std::abs(wet(i)-2.f*wet(i-1)+wet(i-2)));return e;};
    edge(66);set(*p,"roomOn",1);const float on=edge(60);   // 66 blocks: the switch lands on a tone peak
    edge(100);const float steady=edge(40);
    float worst=on/steady;
    for(const auto& [id,value]:std::vector<std::pair<const char*,float>>{{"roomSend1",0},{"roomSend1",100},{"roomReturn",-36},{"roomReturn",0},
            {"roomAbsorption",0},{"roomAbsorption",100},{"roomAbsorption",50},{"roomSize",0},{"roomSize",2},{"roomSize",1},{"roomOn",0}}) {
        set(*p,id,value);
        const float x=edge(100)/steady;worst=std::max(worst,x);
        std::cout<<"MEASURE "<<id<<"="<<value<<": wet edge "<<x<<" x steady\n";
    }
    require(worst<2.f,"A room switch produces a wet edge beyond the room's own");
    std::cout<<"PASS on/off, send, return, absorption and size changes: no wet edge beyond the room's own (on "<<on/steady<<" x)\n";
}
void modeChangeKeepsRoom() {
    // A routing change keeps the room and its tail; lanes the new mode does
    // not have stop sending. (The direct sound's own switch edge, which the
    // room repeats, is the engine's and not tested here.)
    auto p=make(),plain=make();int a=0,b=0;
    for(auto* q:{p.get(),plain.get()})set(*q,"mode",2);
    set(*p,"roomOn",1);for(int lane=1;lane<=3;++lane)set(*p,"roomSend"+juce::String(lane),100);
    render(*p,200,110.,.2f,a);render(*plain,200,110.,.2f,b);
    set(*p,"roomSend1",0);render(*p,20,110.,.2f,a);render(*plain,20,110.,.2f,b);   // rig 1 alone sends nothing
    for(auto* q:{p.get(),plain.get()})set(*q,"mode",0);
    const auto running=p->roomProcessedSamples();
    render(*p,8,110.,.2f,a);render(*plain,8,110.,.2f,b);   // the old routing fades out first
    const auto x=render(*p,40,110.,.2f,a),y=render(*plain,40,110.,.2f,b);
    require(p->roomProcessedSamples()==running+uint64_t(48*block),"A routing change stopped the room");
    require(x.roomPeak==0.f,"Lanes the routing mode does not have still excite the room");
    double wet=0;for(size_t i=0;i<x.out.size();++i)wet+=double(x.out[i]-y.out[i])*(x.out[i]-y.out[i]);
    require(wet>1.e-6,"A routing change cut the room tail");
    std::cout<<"PASS mode change: the room keeps running and its tail, missing lanes send nothing\n";
}
void routingSwitchIsSmooth() {
    // The direct sound across Classic -> Dual -> Matrix -> Classic on a held
    // low tone: no edge beyond twice the larger steady edge of the two modes.
    auto p=make();int clock=0;
    const auto edge=[&](int blocks){const auto x=render(*p,blocks,41.2,.2f,clock).out;float e=0;
        for(size_t i=2;i<x.size();++i)e=std::max(e,std::abs(x[i]-2.f*x[i-1]+x[i-2]));return e;};
    edge(100);float before=edge(120),worst=0;
    for(int mode:{1,2,0}) {
        // Matrix's own level moves, so its steady edge needs a long window.
        set(*p,"mode",float(mode));const float during=edge(60);edge(60);const float after=edge(120);
        const float x=during/std::max(before,after);worst=std::max(worst,x);before=after;
        std::cout<<"MEASURE mode "<<mode<<": direct edge "<<x<<" x steady\n";
    }
    require(worst<2.f,"A routing change produces an edge in the direct sound");
    std::cout<<"PASS routing change: no direct edge beyond the modes' own\n";
}
template<class T> T* find(juce::Component& root,const juce::String& id) {
    if(root.getComponentID()==id)if(auto* match=dynamic_cast<T*>(&root))return match;
    for(auto* child:root.getChildren())if(auto* match=find<T>(*child,id))return match;
    return nullptr;
}
std::vector<float> snapshot(ChimeraProcessor& p) {std::vector<float> values;for(auto* parameter:p.getParameters())values.push_back(parameter->getValue());return values;}
void roomControls() {
    const auto p=std::make_unique<ChimeraProcessor>();
    const auto before=snapshot(*p);
    {
        CabRoomOverview view(*p);view.setSize(1140,412);
        require(snapshot(*p)==before,"Opening the cabinet room changed a parameter");
        auto* button=find<juce::Button>(view,"cabRoom.roomButton");
        require(button!=nullptr && button->isVisible() && view.roomButtonCaption()=="ROOM OFF","Room entry missing or not showing ROOM OFF");
        for(int lane=0;lane<view.activeRigCount();++lane)
            require(!view.getRigBounds(lane).intersects(button->getBounds()),"The room entry overlaps a rig");
        set(*p,"roomOn",1);set(*p,"roomSize",2);view.refreshState();
        require(view.roomButtonCaption()=="ROOM LARGE","Room entry does not show the active room size");
        set(*p,"roomOn",0);set(*p,"roomSize",1);view.refreshState();
    }
    require(snapshot(*p)==before,"Closing the cabinet room changed a parameter");
    spectralforge::cabRoom::RoomControls panel(*p);panel.setSize(620,60);
    require(snapshot(*p)==before,"Opening the room controls changed a parameter");
    for(int count=1;count<=3;++count) {
        panel.setRigCount(count);
        int shown=0;for(int lane=1;lane<=3;++lane){auto* send=find<juce::Slider>(panel,"cabRoom.room.send"+juce::String(lane));require(send!=nullptr,"Room send slider missing");shown+=send->isVisible()?1:0;}
        require(shown==count,"Room sends do not match the rigs in this routing mode");
    }
    auto* power=find<juce::Button>(panel,"cabRoom.room.on");auto* size=find<juce::ComboBox>(panel,"cabRoom.room.size");
    auto* absorption=find<juce::Slider>(panel,"cabRoom.room.absorption");auto* level=find<juce::Slider>(panel,"cabRoom.room.return");
    auto* send2=find<juce::Slider>(panel,"cabRoom.room.send2");
    require(power && size && absorption && level && send2,"Room control missing");
    const auto raw=[&](const char* id){return p->parameters().getRawParameterValue(id)->load();};
    power->setToggleState(true,juce::sendNotificationSync);require(raw("roomOn")>.5f,"ROOM button does not switch the room on");
    size->setSelectedId(3,juce::sendNotificationSync);require(juce::roundToInt(raw("roomSize"))==2,"Size menu does not select Large");
    absorption->setValue(80,juce::sendNotificationSync);require(std::abs(raw("roomAbsorption")-80)<.01f,"Absorption slider is not attached");
    level->setValue(-12,juce::sendNotificationSync);require(std::abs(raw("roomReturn")+12)<.01f,"Return slider is not attached");
    send2->setValue(25,juce::sendNotificationSync);require(std::abs(raw("roomSend2")-25)<.01f,"Rig 2 send is not attached");
    require(std::abs(raw("roomSend1")-50)<.01f && std::abs(raw("roomSend3")-50)<.01f,"Editing one send changed another");
    std::cout<<"PASS room controls: entry in the room's title band shows ROOM OFF / size, rigs keep their layout; panel shows 1/2/3 sends and each control moves only its parameter; opening changes nothing\n";
}
float raw(ChimeraProcessor& p,const char* id){return p.parameters().getRawParameterValue(id)->load();}
void setRoom(ChimeraProcessor& p,bool on,int size,float absorption,float level,float s1,float s2,float s3) {
    set(p,"roomOn",on?1.f:0.f);set(p,"roomSize",float(size));set(p,"roomAbsorption",absorption);set(p,"roomReturn",level);
    set(p,"roomSend1",s1);set(p,"roomSend2",s2);set(p,"roomSend3",s3);
}
bool roomIs(ChimeraProcessor& p,bool on,int size,float absorption,float level,float s1,float s2,float s3) {
    const auto near=[](float a,float b){return std::abs(a-b)<.02f;};
    return (raw(p,"roomOn")>.5f)==on && juce::roundToInt(raw(p,"roomSize"))==size && near(raw(p,"roomAbsorption"),absorption) && near(raw(p,"roomReturn"),level)
        && near(raw(p,"roomSend1"),s1) && near(raw(p,"roomSend2"),s2) && near(raw(p,"roomSend3"),s3);
}
void stateAndRecall() {
    // Project save/restore.
    auto source=std::make_unique<ChimeraProcessor>();setRoom(*source,true,2,70,-6,100,30,0);
    juce::MemoryBlock state;source->getStateInformation(state);
    auto restored=std::make_unique<ChimeraProcessor>();restored->setStateInformation(state.getData(),int(state.getSize()));
    require(roomIs(*restored,true,2,70,-6,100,30,0),"Room settings do not survive a project save and restore");
    // A project saved before Room Core has no room parameters: it opens with
    // the room off, also into a processor whose room was on.
    auto legacy=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));require(legacy!=nullptr,"State is not binary XML");
    int removed=0;
    std::function<void(juce::XmlElement&)> strip=[&](juce::XmlElement& e){
        for(int i=e.getNumChildElements();--i>=0;){auto* c=e.getChildElement(i);if(c->getStringAttribute("id").startsWith("room")){e.removeChildElement(c,true);++removed;}else strip(*c);}};
    strip(*legacy);
    require(removed>=spectralforge::roomParameterCount,"The legacy fixture still carries room parameters");
    juce::MemoryBlock old;juce::AudioProcessor::copyXmlToBinary(*legacy,old);
    for(const bool roomWasOn:{false,true}) {
        auto target=std::make_unique<ChimeraProcessor>();if(roomWasOn)setRoom(*target,true,0,10,3,10,10,10);
        target->setStateInformation(old.getData(),int(old.getSize()));
        require(roomIs(*target,false,1,50,0,50,50,50),"A project without room parameters does not open with the room off and default settings");
    }
    // A/B: each slot keeps its own room.
    auto ab=std::make_unique<ChimeraProcessor>();setRoom(*ab,true,0,20,-3,80,0,0);
    ab->copyComparison();ab->selectComparison(1);setRoom(*ab,false,2,90,-20,10,10,10);
    ab->selectComparison(0);require(roomIs(*ab,true,0,20,-3,80,0,0),"A/B slot A lost its room");
    ab->selectComparison(1);require(roomIs(*ab,false,2,90,-20,10,10,10),"A/B slot B lost its room");
    // Factory presets keep their authored sound: no room.
    auto preset=std::make_unique<ChimeraProcessor>();setRoom(*preset,true,2,10,0,100,100,100);preset->loadFactoryPreset(0);
    require(raw(*preset,"roomOn")<.5f,"A factory preset leaves the room on");
    std::cout<<"PASS room state: project save/restore, projects without room parameters open with the room off (also over an active room), A/B slots keep their own room, factory presets turn it off\n";
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
    try {parameterContract();roomOffIsIdentical();lowDiIsolation();laneControls();gatePolicy();roomModelResponse();roomAudibleAndRingOut();switchingIsSmooth();modeChangeKeepsRoom();routingSwitchIsSmooth();roomControls();stateAndRecall();noAllocation();return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
