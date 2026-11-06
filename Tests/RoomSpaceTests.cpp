// Room Advanced 1.8 spatial stage (docs/ROOM_SPACE_1_8.md): a declared room
// with surface materials, cabinet placement and a room-mic pair, measured on
// the room's wet return against the room-acoustics formulas it declares.
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "RoomControls.h"
#include "CabRoom.h"
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

// Room Space materials on the room scenery (view only, RoomSpaceScenery.h).
void sceneryMaterials() {
    using namespace spectralforge::cabRoom;
    // A lit synthetic photo: a horizontal light pool on a dark gradient.
    juce::Image photo(juce::Image::RGB,600,220,true);
    for(int y=0;y<photo.getHeight();++y)for(int x=0;x<photo.getWidth();++x) {
        const float pool=std::exp(-std::pow((float(x)-300.f)/90.f,2.f)),v=.06f+.3f*pool+.1f*float(y)/220.f;
        photo.setPixelAt(x,y,juce::Colour::fromFloatRGBA(v*1.1f,v*.8f,v*.55f,1.f));
    }
    const juce::Rectangle<float> full=photo.getBounds().toFloat();
    const auto copy=[&]{return photo.createCopy();};
    const auto meanColour=[](const juce::Image& image,juce::Rectangle<int> area){
        double r=0,g=0,b=0;int n=0;
        for(int y=area.getY();y<area.getBottom();++y)for(int x=area.getX();x<area.getRight();++x){const auto c=image.getPixelAt(x,y);r+=c.getFloatRed();g+=c.getFloatGreen();b+=c.getFloatBlue();++n;}
        return std::array<double,3>{r/n,g/n,b/n};
    };
    const juce::Rectangle<int> floorArea{0,int(220*.7f),600,40},ledge{0,int(220*.615f),600,4},wallArea{0,int(220*.2f),600,40};
    {auto off=copy();applySpaceMaterials(off,full,{});
     for(int y=0;y<220;y+=7)for(int x=0;x<600;x+=7)require(off.getPixelAt(x,y)==photo.getPixelAt(x,y),"Space off changed the scenery");}
    std::vector<std::array<double,3>> floors;
    for(int m=0;m<int(spectralforge::roomMaterials.size());++m) {
        auto image=copy();applySpaceMaterials(image,full,{true,m,m,m});
        for(int y=ledge.getY();y<ledge.getBottom();++y)for(int x=0;x<600;x+=5)require(image.getPixelAt(x,y)==photo.getPixelAt(x,y),"The skirting ledge was recoloured");
        floors.push_back(meanColour(image,floorArea));
        // The light pool stays brighter than the dark side on every material.
        const auto lit=meanColour(image,{280,floorArea.getY(),40,floorArea.getHeight()}),dark=meanColour(image,{0,floorArea.getY(),40,floorArea.getHeight()});
        require(lit[0]+lit[1]+lit[2]>dark[0]+dark[1]+dark[2],"Material "+std::to_string(m)+" lost the photo's light pool");
    }
    for(size_t a=0;a<floors.size();++a)for(size_t b=a+1;b<floors.size();++b) {
        const double d=std::abs(floors[a][0]-floors[b][0])+std::abs(floors[a][1]-floors[b][1])+std::abs(floors[a][2]-floors[b][2]);
        require(d>.01,"Floor materials "+std::to_string(a)+" and "+std::to_string(b)+" look the same");
    }
    // Only the chosen surface changes.
    auto wallsOnly=copy();applySpaceMaterials(wallsOnly,full,{true,-1,1,-1});
    const auto wallMean=meanColour(wallsOnly,wallArea),photoWall=meanColour(photo,wallArea),floorMean=meanColour(wallsOnly,floorArea),photoFloor=meanColour(photo,floorArea);
    require(std::abs(wallMean[0]-photoWall[0])>.02 && std::abs(floorMean[0]-photoFloor[0])<1e-6,"Wall material changed the floor or left the walls");
    // The room overview follows the parameters; Space off draws the plain photo.
    auto p=std::make_unique<ChimeraProcessor>();
    const auto set=[&](const char* id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    CabRoomOverview view(*p);view.setSize(1140,412);view.refreshState();
    require(!view.shownSpaceSurfaces().on,"Space materials shown with Space off");
    const auto plain=view.createComponentSnapshot(view.getLocalBounds(),true,1.f);
    set("roomOn",1);set("roomSpace",1);set("roomFloor",0);set("roomWalls",1);set("roomCeiling",8);view.refreshState();
    const auto shown=view.shownSpaceSurfaces();
    require(shown.on && shown.floor==0 && shown.walls==1 && shown.ceiling==8,"Room overview did not follow the Space materials");
    const auto concrete=view.createComponentSnapshot(view.getLocalBounds(),true,1.f);
    set("roomFloor",6);view.refreshState();require(view.shownSpaceSurfaces().floor==6,"Floor material change not followed");
    const auto carpet=view.createComponentSnapshot(view.getLocalBounds(),true,1.f);
    const juce::Rectangle<int> viewFloor{0,380,1140,20};
    require(meanColour(concrete,viewFloor)!=meanColour(carpet,viewFloor),"Changing the floor material did not change the drawn floor");
    // The raven relief keeps its photographed look: found on the photo, near
    // the top centre, and drawn unchanged under a brick wall.
    {
        juce::SharedResourcePointer<ArtworkBank> bank;
        require(bank->emblem.isValid() && bank->emblem.getBounds()==bank->room.getBounds(),"Emblem mask missing");
        double sum=0,sx=0,sy=0;
        for(int y=0;y<bank->emblem.getHeight();++y)for(int x=0;x<bank->emblem.getWidth();++x){const double a=bank->emblem.getPixelAt(x,y).getAlpha()/255.;sum+=a;sx+=a*x;sy+=a*y;}
        const double area=sum/double(bank->emblem.getWidth()*bank->emblem.getHeight());
        const double cx=sx/sum/bank->emblem.getWidth(),cy=sy/sum/bank->emblem.getHeight();
        require(area>.005 && area<.04 && std::abs(cx-.5)<.03 && cy>.12 && cy<.3,
                "Emblem mask misplaced: area="+std::to_string(area)+" centre="+std::to_string(cx)+","+std::to_string(cy));
        const float scale=std::max(1140.f/float(bank->room.getWidth()),412.f/float(bank->room.getHeight()));
        const float ox=(1140.f-float(bank->room.getWidth())*scale)*.5f,oy=412.f-float(bank->room.getHeight())*scale;
        const juce::Rectangle<int> relief{int(ox+float(cx*bank->room.getWidth())*scale)-3,int(oy+float(cy*bank->room.getHeight())*scale)-3,6,6};
        const juce::Rectangle<int> wallBeside{int(ox+.3f*float(bank->room.getWidth())*scale),relief.getY(),6,6};
        set("roomWalls",1);view.refreshState();
        const auto brick=view.createComponentSnapshot(view.getLocalBounds(),true,1.f);
        const auto a=meanColour(brick,relief),b=meanColour(plain,relief);
        require(std::abs(a[0]-b[0])+std::abs(a[1]-b[1])+std::abs(a[2]-b[2])<.02,"The raven relief took the wall material");
        const auto c=meanColour(brick,wallBeside),d=meanColour(plain,wallBeside);
        require(std::abs(c[0]-d[0])+std::abs(c[1]-d[1])+std::abs(c[2]-d[2])>.05,"The wall beside the relief kept the photo");
    }
    set("roomSpace",0);view.refreshState();require(!view.shownSpaceSurfaces().on,"Space off still shown");
    const auto offAgain=view.createComponentSnapshot(view.getLocalBounds(),true,1.f);
    require(meanColour(offAgain,viewFloor)==meanColour(plain,viewFloor),"Space off did not return to the plain scenery");
    std::cout<<"PASS Room Space scenery: off unchanged, 10 distinct floor materials, light pool kept, ledge untouched, per-surface, raven relief kept, overview follows parameters\n";
}
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {spaceOffIsRoomCore();imageSources();decayAndLevel();transitions();controls();sceneryMaterials();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
