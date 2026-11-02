// Room Advanced 1.8 spatial stage (docs/ROOM_SPACE_1_8.md): a declared room
// with surface materials, cabinet placement and a room-mic pair, measured on
// the room's wet return against the room-acoustics formulas it declares.
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "RoomControls.h"
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
    static juce::AudioProcessorValueTreeState::ParameterLayout layout(){juce::AudioProcessorValueTreeState::ParameterLayout l;appendRoomParameters(l);appendRoomAdvancedParameters(l);appendRoomSpaceParameters(l);return l;}
    Host():state(*this,nullptr,"ROOM",layout()){set("roomOn",1);}
    void set(const char* id,float v){auto* p=state.getParameter(id);require(p!=nullptr,std::string("no parameter ")+id);p->setValueNotifyingHost(p->convertTo0to1(v));}
    void space(const RoomSpace& s){
        set("roomSpace",1);set("roomSpaceLength",s.length);set("roomSpaceWidth",s.width);set("roomSpaceHeight",s.height);
        set("roomFloor",float(s.floor));set("roomWalls",float(s.walls));set("roomCeiling",float(s.ceiling));
        set("roomCabDistance",s.cabWall);set("roomCabAcross",s.cabAcross*100);set("roomCabHeight",s.cabHeight);
        set("roomMicDistance",s.micDistance);set("roomMicAcross",s.micAcross*100);set("roomMicHeight",s.micHeight);set("roomMicSpacing",s.micSpacing);
    }
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
// The send fades in over 20 ms when the room starts, so the impulse comes
// later and the response is returned from it on.
constexpr int start=2048;
auto impulse(){return [](int n,int){return n==start?1.f:0.f;};}
Stereo trimmed(Stereo s){s.l.erase(s.l.begin(),s.l.begin()+start);s.r.erase(s.r.begin(),s.r.begin()+start);return s;}
Stereo response(const RoomSpace& s,float balance,int samples){Host h;h.space(s);h.set("roomEarlyLate",balance);RoomCore r;return trimmed(wet(h,r,samples+start,impulse()));}
double distance(std::array<double,3> a,std::array<double,3> b){double e=0;for(int i=0;i<3;++i)e+=(a[size_t(i)]-b[size_t(i)])*(a[size_t(i)]-b[size_t(i)]);return std::sqrt(e);}
double mid(int material){const auto& a=roomMaterials[size_t(material)].absorption;return .5*(a[2]+a[3]);}

// A room whose floor bounce arrives first and alone: 10 x 8 x 4 m, cabinet low
// at 2 m from the back wall, a single mic (spacing 0) 3 m out and 1.5 m high.
RoomSpace probeRoom(int floor){RoomSpace s;s.length=10;s.width=8;s.height=4;s.floor=floor;s.walls=1;s.ceiling=2;s.cabWall=2;s.cabAcross=.45f;s.cabHeight=.5f;s.micDistance=3;s.micAcross=.55f;s.micHeight=1.5f;s.micSpacing=0;return s;}

void spaceOffIsRoomCore() {
    // Space off: the released room, bit for bit, whatever the space controls say.
    size_t compared=0;
    for(int size=0;size<3;++size)for(float absorption:{0.f,50.f,100.f}) {
        Host host;host.set("roomSize",float(size));host.set("roomAbsorption",absorption);
        host.space(probeRoom(0));host.set("roomSpace",0);
        RoomCore now;room_core_released::RoomCore before;
        const auto x=[](int n,int c){return .3f*noise(n,c);};
        const auto a=wet(host,now,24000,x),b=wet(host,before,24000,x);
        for(size_t n=0;n<a.l.size();++n)require(a.l[n]==b.l[n] && a.r[n]==b.r[n],"Space off differs from Room Core");
        compared+=a.l.size()*2;
    }
    std::cout<<"PASS Space off bit-identical to Room Core with the space controls moved: "<<compared<<" samples\n";
}

void imageSources() {
    // The default declared geometry is the Medium room's: the same reflection times.
    const auto nonzero=[](const Stereo& s){std::vector<int> at;for(int n=0;n<int(s.l.size());++n)if(s.l[size_t(n)]!=0 || s.r[size_t(n)]!=0)at.push_back(n);return at;};
    Host preset;preset.set("roomEarlyLate",-100);RoomCore a;const auto medium=trimmed(wet(preset,a,9600+start,impulse()));
    const auto declared=response(RoomSpace{},-100,9600);
    const auto pm=nonzero(medium),pd=nonzero(declared);
    std::cout<<"MEASURE default declared room vs Medium: "<<pd.size()<<" / "<<pm.size()<<" non-zero early samples\n";
    require(!pm.empty() && pm==pd,"The default declared room does not reproduce the Medium room's reflection times");
    // First-order reflections of the probe room arrive at the analytic image delays.
    const auto s=probeRoom(0);const auto out=response(s,-100,24000);
    const std::array<double,3> src{2,3.6,.5},mic{5,4.4,1.5};const double direct=distance(src,mic);
    const std::array<std::array<double,3>,6> images{{{-2,3.6,.5},{18,3.6,.5},{2,-3.6,.5},{2,12.4,.5},{2,3.6,-.5},{2,3.6,7.5}}};
    int first=1<<30;
    for(const auto& image:images){const int d=int(std::lround((distance(image,mic)-direct)/343.0*rate));first=std::min(first,d);require(out.l[size_t(d)]!=0,"A first-order reflection is missing at its image delay "+std::to_string(d));}
    int earliest=-1;for(int n=0;n<int(out.l.size());++n)if(out.l[size_t(n)]!=0){earliest=n;break;}
    std::cout<<"MEASURE probe room: first reflection (floor) at sample "<<earliest<<", analytic "<<first<<"\n";
    require(earliest==first,"Something arrives before the floor bounce");
    // Floor bounce level: Medium early trim x 2.5 m / path x sqrt(1 - alpha floor).
    const double floorPath=distance({2,3.6,-.5},mic);const int floorAt=int(std::lround((floorPath-direct)/343.0*rate));
    for(const int material:{0,6,7}) {
        const auto o=response(probeRoom(material),-100,2400);
        const double expected=.21294*2.5/floorPath*std::sqrt(1-mid(material));
        std::cout<<"MEASURE floor bounce, "<<roomMaterials[size_t(material)].name<<": "<<o.l[size_t(floorAt)]<<" (predicted "<<expected<<")\n";
        require(std::abs(o.l[size_t(floorAt)]/expected-1)<1.e-3,"The floor bounce does not follow the floor material");
    }
    // Mic spacing 0: both channels hear the same reflections; 1 m: they differ.
    double same=0;for(size_t n=0;n<out.l.size();++n)same=std::max(same,double(std::abs(out.l[n]-out.r[n])));
    auto spaced=probeRoom(0);spaced.micSpacing=1;const auto wide=response(spaced,-100,24000);
    double differ=0;for(size_t n=0;n<wide.l.size();++n)differ=std::max(differ,double(std::abs(wide.l[n]-wide.r[n])));
    std::cout<<"MEASURE early L-R: spacing 0 m "<<same<<", spacing 1 m "<<differ<<"\n";
    require(same==0 && differ>1.e-3,"Mic spacing does not set the pair's difference");
    std::cout<<"PASS image sources: Medium timing reproduced, analytic first-order delays, floor material level, mic spacing\n";
}

// Octave-band filter: three cascaded RBJ band-passes (Q 1.41), so a slower
// neighbouring band cannot leak into the decay fit.
std::vector<double> band(const std::vector<float>& x,double hz){
    const double w=2*pi*hz/rate,alpha=std::sin(w)/(2*1.41),a0=1+alpha;
    const double b0=alpha/a0,b2=-alpha/a0,a1=-2*std::cos(w)/a0,a2=(1-alpha)/a0;
    std::vector<double> y(x.begin(),x.end());
    for(int pass=0;pass<3;++pass){double x1=0,x2=0,y1=0,y2=0;for(auto& s:y){const double v=b0*s+b2*x2-a1*y1-a2*y2;x2=x1;x1=s;y2=y1;y1=v;s=v;}}
    return y;
}
// Schroeder backward integral, T20 fit (-5 to -25 dB).
double bandT60(const std::vector<float>& x,double hz){
    auto y=band(x,hz);for(auto& v:y)v*=v;
    for(size_t n=y.size()-1;n-->0;)y[n]+=y[n+1];
    const double top=y[0];size_t t5=0,t25=0;
    for(size_t n=0;n<y.size();++n){const double level=10*std::log10(y[n]/top+1.e-300);if(t5==0&&level<=-5)t5=n;if(level<=-25){t25=n;break;}}
    require(t5>0 && t25>t5,"Decay range too short to measure");
    return 3.0*double(t25-t5)/rate;
}
double bandEnergy(const std::vector<float>& x,double hz){double e=0;for(const double v:band(x,hz))e+=v*v;return e;}
void decayAndLevel() {
    // Late-only responses of three declared rooms against Eyring.
    struct Case{const char* name;RoomSpace s;};
    RoomSpace live;live.length=12;live.width=9;live.height=5;live.floor=0;live.walls=1;live.ceiling=2;live.cabWall=3;live.micDistance=4;
    RoomSpace room;room.length=8;room.width=6;room.height=3.5f;room.floor=4;room.walls=3;room.ceiling=8;room.cabWall=2;room.micDistance=3;
    const RoomSpace standard;
    std::array<double,3> energy500{},volume{},eyring500{};
    int index=0;
    for(const Case& c:{Case{"live (concrete / brick / plaster, 12 x 9 x 5 m)",live},Case{"default (carpet / wood panel / gypsum, 7 x 5 x 3 m)",standard},Case{"treated (wood floor / gypsum / acoustic tile, 8 x 6 x 3.5 m)",room}}) {
        const double low=c.s.eyring(2),high=c.s.eyring(5);
        const auto out=response(c.s,100,int(rate*std::max(1.5,1.6*low)));
        const double m500=bandT60(out.l,500),m4k=bandT60(out.l,4000);
        std::cout<<"MEASURE T60 "<<c.name<<": 500 Hz "<<m500<<" s (Eyring "<<low<<"), 4 kHz "<<m4k<<" s (Eyring + air "<<high<<")\n";
        require(std::abs(m500/low-1)<.15 && std::abs(m4k/high-1)<.2,"Declared-room decay does not follow Eyring");
        energy500[size_t(index)]=bandEnergy(out.l,500)+bandEnergy(out.r,500);volume[size_t(index)]=c.s.volume();eyring500[size_t(index)]=low;++index;
    }
    // Diffuse energy follows T60 / V (relative to the default room).
    for(int i=0;i<3;i+=2){
        const double measured=10*std::log10(energy500[size_t(i)]/energy500[1]),predicted=10*std::log10((eyring500[size_t(i)]/volume[size_t(i)])/(eyring500[1]/volume[1]));
        std::cout<<"MEASURE 500 Hz late energy vs the default room: "<<measured<<" dB (T60 / V predicts "<<predicted<<" dB)\n";
        require(std::abs(measured-predicted)<2,"Late level does not follow T60 / V");
    }
    Host h;RoomCore r;r.bind(h.state);h.space(live);const double tail=r.tailSeconds();
    std::cout<<"MEASURE host tail, live room: "<<tail<<" s (2 x Eyring "<<2*live.eyring(2)<<" s)\n";
    require(tail>=2*live.eyring(2),"Host tail shorter than two decay times");
    std::cout<<"PASS declared decay at 500 Hz / 4 kHz, diffuse level law, host tail\n";
}

void transitions() {
    // Moving the mic, changing a surface and switching Space while a tone
    // plays: crossfades, no step, no allocation.
    Host h;h.space(RoomSpace{});RoomCore r;r.bind(h.state);r.prepare({rate,(juce::uint32)block,2});juce::AudioBuffer<float> out(2,block);std::vector<float> y;
    for(int k=0;k<1500;++k){
        if(k==375)h.set("roomMicDistance",5);if(k==750)h.set("roomWalls",7);if(k==1125)h.set("roomSpace",0);
        auto& bus=r.bus(2,block);for(int n=0;n<block;++n){const float x=float(.2*std::sin(2*pi*220*(k*block+n)/rate));bus.setSample(0,n,x);bus.setSample(1,n,x);}
        out.clear();countAllocations=true;r.process(&bus,out);countAllocations=false;for(int n=0;n<block;++n)y.push_back(out.getSample(0,n));
    }
    const auto worst=[&](size_t from,size_t to){double m=0;for(size_t n=std::max<size_t>(from,2);n<to;++n)m=std::max(m,std::abs(double(y[n])-2*y[n-1]+y[n-2]));return m;};
    const double settled=std::max({worst(30000,48000),worst(78000,96000),worst(126000,144000),worst(174000,192000)});
    const double moves=std::max({worst(48000,60000),worst(96000,108000),worst(144000,156000)});
    std::cout<<"MEASURE per change: mic "<<worst(48000,60000)<<", walls "<<worst(96000,108000)<<", Space off "<<worst(144000,156000)<<"; settled "<<worst(30000,48000)<<" / "<<worst(78000,96000)<<" / "<<worst(126000,144000)<<" / "<<worst(174000,192000)<<"\n";
    std::cout<<"MEASURE transitions (mic 2.45 -> 5 m, walls -> curtain, Space off): largest second difference "<<moves<<" against "<<settled<<" settled; allocations "<<allocations<<"\n";
    require(moves<settled*1.5 && allocations==0,"A spatial change steps the return or allocates");
    std::cout<<"PASS spatial changes crossfade without a step and without allocation\n";
}

void controls() {
    const auto p=std::make_unique<ChimeraProcessor>();
    spectralforge::cabRoom::RoomControls panel(*p);panel.setSize(680,214);
    const auto find=[&](const juce::String& id){for(auto* c:panel.getChildren())if(c->getComponentID()==id)return c;return (juce::Component*)nullptr;};
    const auto raw=[&](const char* id){return p->parameters().getRawParameterValue(id)->load();};
    auto* space=dynamic_cast<juce::Button*>(find("cabRoom.room.space"));auto* size=dynamic_cast<juce::ComboBox*>(find("cabRoom.room.size"));
    auto* length=dynamic_cast<juce::Slider*>(find("cabRoom.room.length"));auto* floor=dynamic_cast<juce::ComboBox*>(find("cabRoom.room.floor"));
    auto* mic=dynamic_cast<juce::Slider*>(find("cabRoom.room.micDistance"));
    require(space && size && length && floor && mic,"Spatial room control missing");
    require(size->isEnabled() && !length->isEnabled(),"Space off must leave Size active and the space controls inactive");
    space->setToggleState(true,juce::sendNotificationSync);panel.refreshSpace();
    require(raw("roomSpace")>.5f && !size->isEnabled() && length->isEnabled() && floor->isEnabled(),"SPACE does not switch to the declared room");
    length->setValue(12,juce::sendNotificationSync);floor->setSelectedId(1,juce::sendNotificationSync);mic->setValue(4,juce::sendNotificationSync);
    require(std::abs(raw("roomSpaceLength")-12)<.01f && juce::roundToInt(raw("roomFloor"))==0 && std::abs(raw("roomMicDistance")-4)<.01f,"Space controls are not attached");
    for(auto* c:panel.getChildren())if(c->isVisible())require(panel.getLocalBounds().contains(c->getBounds()),"A room control lies outside the callout: "+c->getComponentID().toStdString());
    std::cout<<"PASS SPACE switch, attached dimension / surface / mic controls, Size disabled while the space is on\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {spaceOffIsRoomCore();imageSources();decayAndLevel();transitions();controls();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
