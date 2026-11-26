// 1.9 CREATIVE slot (docs/CREATIVE_1_9.md): each of the nine models measured
// on its documented law, plus the slot contract (off is sample-exact and
// idle, no allocation, bounded output, model crossfade) and the processor and
// editor wiring.
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "CreativeFX.h"
#include "CreativePanel.h"
#include "CreativePage.h"
#include "SignalPath.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <new>
#include <set>
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
constexpr double rate=48000,pi=3.14159265358979323846;constexpr int block=256;
void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
double db(double v){return 20*std::log10(std::max(v,1.e-30));}
struct Host final : juce::AudioProcessor {
    juce::AudioProcessorValueTreeState state;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout(){juce::AudioProcessorValueTreeState::ParameterLayout l;appendCreativeParameters(l);return l;}
    Host():state(*this,nullptr,"CREATIVE",layout()){}
    void set(const char* id,float v){auto* p=state.getParameter(id);require(p!=nullptr,std::string("no parameter ")+id);p->setValueNotifyingHost(p->convertTo0to1(v));}
    const juce::String getName() const override {return "creative";}
    void prepareToPlay(double,int) override {}void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override {return 0;}bool acceptsMidi() const override {return false;}bool producesMidi() const override {return false;}
    juce::AudioProcessorEditor* createEditor() override {return nullptr;}bool hasEditor() const override {return false;}
    int getNumPrograms() override {return 1;}int getCurrentProgram() override {return 0;}void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return {};}void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}void setStateInformation(const void*,int) override {}
};
struct Stereo {std::vector<float> l,r;};
struct Setup {int model{};float a{50},b{50},c{50},mix{100};int sync{3};double tempo{120};};
// Renders `samples` frames of input(n, channel) through a CREATIVE slot; the
// slot is switched on before the first block, so its 20 ms mix ramp is at the start.
template<class Input> Stereo render(const Setup& s,int samples,Input input,CreativeFX* keep=nullptr) {
    Host h;h.set("creativeOn",1);h.set("creativeModel",float(s.model));h.set("creativeA",s.a);h.set("creativeB",s.b);h.set("creativeC",s.c);
    h.set("creativeMix",s.mix);h.set("creativeSync",float(s.sync));
    CreativeFX local;CreativeFX& fx=keep?*keep:local;fx.bind(h.state);fx.prepare({rate,(juce::uint32)block,2});
    Stereo out{std::vector<float>((size_t)samples),std::vector<float>((size_t)samples)};juce::AudioBuffer<float> b(2,block);
    for(int offset=0;offset<samples;offset+=block){
        for(int n=0;n<block;++n){b.setSample(0,n,input(offset+n,0));b.setSample(1,n,input(offset+n,1));}
        countAllocations=true;fx.process(b,s.tempo);countAllocations=false;
        for(int n=0;n<block && offset+n<samples;++n){out.l[size_t(offset+n)]=b.getSample(0,n);out.r[size_t(offset+n)]=b.getSample(1,n);}
    }
    return out;
}
auto sine(double hz,double amplitude){return [=](int n,int){return float(amplitude*std::sin(2*pi*hz*n/rate));};}
float noise(int n,int c){std::uint32_t x=std::uint32_t(n)*2654435761u^std::uint32_t(c*40503+12345);x^=x>>13;x*=0x5bd1e995u;x^=x>>15;return float(x&0xffff)/32768.f-1.f;}
double rms(const std::vector<float>& x,int from,int to){double e=0;for(int n=from;n<to;++n)e+=double(x[size_t(n)])*x[size_t(n)];return std::sqrt(e/std::max(1,to-from));}
// Frequency from rising zero crossings (linear interpolation) over [from, to).
double frequency(const std::vector<float>& x,int from,int to){
    double first=-1,last=-1;int count=0;
    for(int n=from+1;n<to;++n)if(x[size_t(n-1)]<0 && x[size_t(n)]>=0){const double t=n-1+x[size_t(n-1)]/(x[size_t(n-1)]-x[size_t(n)]);if(first<0)first=t;last=t;++count;}
    return count>1?(count-1)*rate/(last-first):0;
}
double magnitude(const std::vector<float>& x,int from,int to,double hz){
    std::complex<double> sum;for(int n=from;n<to;++n){const double w=.5-.5*std::cos(2*pi*(n-from)/(to-from));sum+=w*double(x[size_t(n)])*std::polar(1.0,-2*pi*hz*n/rate);}
    return 2*std::abs(sum)/((to-from)*.5);
}
// Strongest component within +-12 % of `near` (0.5 Hz grid), in Hz.
double peakNear(const std::vector<float>& x,int from,int to,double near){double best=0,at=near;for(double hz=near*.88;hz<=near*1.12;hz+=.5){const double m=magnitude(x,from,to,hz);if(m>best){best=m;at=hz;}}return at;}
// Free-rate control value for a target rate on an exponential lo..hi law.
float freeA(double hz,double lo,double hi){return float(100*std::log(hz/lo)/std::log(hi/lo));}

void offAndMix() {
    // Off: sample-exact and idle. On with Mix 0: still sample-exact.
    Host h;CreativeFX fx;fx.bind(h.state);fx.prepare({rate,(juce::uint32)block,2});juce::AudioBuffer<float> b(2,block);
    for(int k=0;k<50;++k){for(int n=0;n<block;++n)for(int c=0;c<2;++c)b.setSample(c,n,.3f*noise(k*block+n,c));juce::AudioBuffer<float> copy(b);fx.process(b,120);
        for(int c=0;c<2;++c)for(int n=0;n<block;++n)require(b.getSample(c,n)==copy.getSample(c,n),"CREATIVE off changed the signal");}
    require(!fx.isActive(),"CREATIVE off is not idle");
    for(int model=0;model<creativeModelCount;++model){
        Setup s;s.model=model;s.mix=0;const auto x=[](int n,int c){return .3f*noise(n,c);};const auto out=render(s,24000,x);
        for(int n=0;n<24000;++n)require(out.l[size_t(n)]==x(n,0) && out.r[size_t(n)]==x(n,1),std::string("Mix 0 changed the signal in ")+creativeModels[size_t(model)].name);
    }
    std::cout<<"PASS off is sample-exact and idle; Mix 0 is sample-exact for all nine models\n";
}

void gateSlicer() {
    // 1/8 at 120 BPM = 4 Hz; depth 100 %, duty 50 %.
    Setup s;s.model=0;s.b=100;s.c=50;const auto out=render(s,96000,sine(1000,.5));
    std::vector<double> env;for(int n=4800;n+48<=96000;n+=48){double p=0;for(int k=0;k<48;++k)p=std::max(p,double(std::abs(out.l[size_t(n+k)])));env.push_back(p);}
    int rises=0,open=0,first=-1,last=-1;for(size_t i=1;i<env.size();++i){if(env[i-1]<.25 && env[i]>=.25){++rises;if(first<0)first=int(i);last=int(i);}open+=env[i]>=.25;}
    const double hz=(rises-1)/((last-first)*48/rate),duty=double(open)/env.size();
    double closed=1;for(const double e:env)closed=std::min(closed,e);
    Setup half=s;half.b=50;const auto shallow=render(half,96000,sine(1000,.5));double floor=1;for(int n=24000;n<96000;n+=48){double p=0;for(int k=0;k<48;++k)p=std::max(p,double(std::abs(shallow.l[size_t(n+k)])));floor=std::min(floor,p);}
    std::cout<<"MEASURE Gate Slicer 1/8 @ 120 BPM: "<<hz<<" Hz gate (4 expected), duty "<<duty<<", closed "<<db(closed/.5)<<" dB; depth 50 %: floor "<<db(floor/.5)<<" dB\n";
    require(std::abs(hz-4)<.3 && std::abs(duty-.5)<.05 && db(closed/.5)<-40 && std::abs(db(floor/.5)+6.02)<.5,"Gate Slicer rate, duty or depth wrong");
}

void stepHarmonizer() {
    // 1/4 at 120 BPM = 0.5 s steps; pattern B 30 % = major 0 / 4 / 7 / 12 st.
    Setup s;s.model=1;s.b=30;s.c=0;s.sync=2;const auto out=render(s,int(rate*2.2),sine(220,.5));
    // The first step boundary is after one full step: steps start at 0.5 s.
    const std::array<int,4> interval{0,4,7,12};
    std::string line;bool ok=true;
    for(int k=0;k<4;++k){
        const int step=(k+1)%4;const int from=int(rate*(.5*k+.15)),to=int(rate*(.5*k+.45));
        const double expected=220*std::pow(2.0,interval[size_t(k)]/12.0),f=peakNear(out.l,from,to,expected);
        line+=" "+juce::String(f,1).toStdString()+" ("+juce::String(expected,1).toStdString()+")";ok=ok&&std::abs(f/expected-1)<.015;(void)step;
    }
    std::cout<<"MEASURE Step Harmonizer major pattern on 220 Hz, Hz (expected):"<<line<<"\n";
    require(ok,"Step Harmonizer steps are off pitch");
}

void timeBend() {
    // 0.5 Hz sine bend of +-12 st on 220 Hz: 110 to 440 Hz.
    Setup s;s.model=2;s.a=freeA(.5,.05,4);s.b=100;s.c=0;const auto out=render(s,int(rate*4.2),sine(220,.5));
    double lo=1e9,hi=0;for(int n=int(rate*.2);n+960<int(rate*4.2);n+=480){const double f=frequency(out.l,n,n+960);if(f>0){lo=std::min(lo,f);hi=std::max(hi,f);}}
    std::cout<<"MEASURE Time Bend +-12 st on 220 Hz: "<<lo<<" - "<<hi<<" Hz (110 - 440)\n";
    require(std::abs(lo/110-1)<.06 && std::abs(hi/440-1)<.06,"Time Bend range wrong");
}

double centroid(const std::vector<float>& x,int from,int to){
    double num=0,den=0;for(double hz=100;hz<12000;hz*=1.12){const double m=magnitude(x,from,to,hz);num+=hz*m*m;den+=m*m;}
    return num/std::max(den,1e-30);
}
void zapSweep() {
    // Noise starting at 0 is the onset; sweep 8 kHz -> 150 Hz over 20 + 580 x 50 % = 310 ms.
    Setup s;s.model=3;s.a=50;s.b=50;s.c=50;const auto x=[](int n,int c){return n<int(rate*.7)||n>int(rate*1.2)?.25f*noise(n,c):0.f;};
    const auto out=render(s,int(rate*1.6),x);
    const double early=centroid(out.l,int(rate*.02),int(rate*.05)),mid=centroid(out.l,int(rate*.14),int(rate*.17)),late=centroid(out.l,int(rate*.45),int(rate*.6));
    const double again=centroid(out.l,int(rate*1.22),int(rate*1.25));
    std::cout<<"MEASURE Laser Zap filter sweep centroid: "<<early<<" Hz at 20-50 ms, "<<mid<<" Hz at 140-170 ms, "<<late<<" Hz after the sweep; "<<again<<" Hz after a new onset\n";
    require(early>2500 && mid<early*.7 && late<700 && again>2500,"Zap Sweep does not sweep down from each onset");
}

void freqShift() {
    // +100 Hz on 1 kHz: 1100 Hz out, the 900 Hz image and the 1 kHz input suppressed.
    const float a=float(50*(1+std::cbrt(.1)));
    Setup s;s.model=4;s.a=a;s.b=0;s.c=0;const auto out=render(s,48000,sine(1000,.5));
    const double up=magnitude(out.l,9600,48000,1100),image=magnitude(out.l,9600,48000,900),carrier=magnitude(out.l,9600,48000,1000);
    Setup spread=s;spread.c=100;const auto wide=render(spread,48000,sine(1000,.5));
    const double right=magnitude(wide.r,9600,48000,900),rightUp=magnitude(wide.r,9600,48000,1100);
    std::cout<<"MEASURE Freq Shift +100 Hz on 1 kHz: 1100 Hz "<<db(up/.5)<<" dB, image 900 Hz "<<db(image/up)<<" dB, 1 kHz "<<db(carrier/up)<<" dB; spread 100 %: right at 900 Hz "<<db(right/.5)<<" dB ("<<db(rightUp/right)<<" dB at 1100)\n";
    require(std::abs(db(up/.5))<.5 && db(image/up)<-35 && db(carrier/up)<-35 && std::abs(db(right/.5))<.5 && db(rightUp/right)<-35,"Freq Shift is not single-sideband");
}

void freezeHold() {
    // 0.5 s tone then silence: the captured grain sustains and decays by 60 dB per hold (3.87 s at A 50 %).
    Setup s;s.model=5;s.a=50;s.b=50;s.c=50;const auto x=[](int n,int){return n<int(rate*.5)?float(.3*std::sin(2*pi*440*n/rate)):0.f;};
    const auto out=render(s,int(rate*3),x);
    const double at1=rms(out.l,int(rate*1.0),int(rate*1.2)),at2=rms(out.l,int(rate*2.0),int(rate*2.2));
    const double hold=.5*std::pow(60.0,.5),expected=60.0/hold;
    std::cout<<"MEASURE Freeze Hold after the input stops: "<<db(at1/.3*std::sqrt(2.0))<<" dB at 1 s, decay "<<db(at1/at2)<<" dB/s (60 / "<<hold<<" s = "<<expected<<")\n";
    require(at1>.3/std::sqrt(2.0)*.1 && std::abs(db(at1/at2)-expected)<1.5,"Freeze Hold does not sustain and decay as declared");
}

void glitchStutter() {
    // Chance 100 %: every 1/16 step (6000 samples at 120 BPM) repeats the previous one.
    const auto x=[](int n,int c){return .3f*noise(n,c);};
    Setup s;s.model=6;s.b=100;s.c=0;s.sync=4;const auto out=render(s,72000,x);
    const int L=6000,fade=int(rate*.003)+1;int compared=0;double worst=0;
    for(int n=2*L;n<72000;++n){const int phase=(n+1)%L;if(phase<fade||phase>L-fade)continue;worst=std::max(worst,double(std::abs(out.l[size_t(n)]-x(n-L,0))));++compared;}
    Setup r=s;r.c=100;const auto back=render(r,72000,x);double worstBack=0;
    for(int n=2*L;n<72000;++n){const int phase=(n+1)%L;if(phase<fade||phase>L-fade)continue;const int start=n-phase;worstBack=std::max(worstBack,double(std::abs(back.l[size_t(n)]-x(start-1-phase,0))));}
    Setup never=s;never.b=0;const auto plain=render(never,24000,x);bool same=true;for(int n=0;n<24000;++n)same=same&&plain.l[size_t(n)]==x(n,0);
    std::cout<<"MEASURE Glitch Stutter: repeat error "<<worst<<" over "<<compared<<" samples, reversed "<<worstBack<<"; chance 0 % passes the input "<<(same?"exactly":"inexactly")<<"\n";
    require(worst<1e-6 && worstBack<1e-6 && same,"Glitch Stutter repeats the wrong audio");
}

void bitCrush() {
    const auto levels=[](const std::vector<float>& x){std::set<float> v(x.begin()+4800,x.end());return v.size();};
    Setup s;s.model=7;s.a=100;s.b=0;s.c=0;const auto two=levels(render(s,24000,sine(110,.9)).l);
    Setup eight=s;eight.a=float(100*8/14.0);const auto many=levels(render(eight,24000,sine(110,.9)).l);
    Setup slow=s;slow.a=0;slow.b=100;const auto held=render(slow,24000,[](int n,int c){return .5f*noise(n,c);}).l;   // noise: every hold takes a new value
    int runs=0;for(size_t n=4801;n<held.size();++n)runs+=held[n]!=held[n-1];const double run=(held.size()-4801)/double(std::max(1,runs));
    Setup open=s;open.a=0;const auto bright=render(open,24000,sine(5000,.5));Setup dark=open;dark.c=100;const auto dull=render(dark,24000,sine(5000,.5));
    const double tone=db(rms(dull.l,4800,24000)/rms(bright.l,4800,24000));
    std::cout<<"MEASURE Bit Crush: 2 bits "<<two<<" levels, 8 bits "<<many<<" levels; rate 100 %: held "<<run<<" samples (96 at 500 Hz); tone 100 % at 5 kHz "<<tone<<" dB\n";
    require(two<=5 && many<=257 && many>two && std::abs(run-96)<2 && tone<-12,"Bit Crush word length, rate or tone wrong");
}

void grainCloud() {
    Setup s;s.model=8;s.a=60;s.b=0;s.c=50;const auto still=render(s,96000,sine(440,.5));
    const double on=magnitude(still.l,24000,96000,440),off=std::max(magnitude(still.l,24000,96000,330),magnitude(still.l,24000,96000,587));
    Setup scatter=s;scatter.b=100;const auto spread=render(scatter,96000,sine(440,.5));
    const double share=std::pow(magnitude(spread.l,24000,96000,440),2)/(2*std::pow(rms(spread.l,24000,96000),2));
    Setup sparse=s;sparse.a=20;const double low=rms(render(sparse,96000,sine(440,.5)).l,24000,96000);Setup dense=s;dense.a=100;const double high=rms(render(dense,96000,sine(440,.5)).l,24000,96000);
    const double level=db(rms(still.l,24000,96000)/(.5/std::sqrt(2.0)));
    std::cout<<"MEASURE Grain Cloud: scatter 0 % level "<<level<<" dB (design -3), 440 Hz neighbours "<<db(off/on)<<" dB; scatter 100 %: 440 Hz carries "<<share*100<<" % of the power; level density 20 % vs 100 %: "<<db(low/high)<<" dB\n";
    require(db(off/on)<-20 && share<.5 && std::abs(level+3)<2 && std::abs(db(low/high))<2.5,"Grain Cloud pitch or level law wrong");
}

void contract() {
    // Model changes crossfade, extremes stay bounded, the callback never allocates.
    Host h;h.set("creativeOn",1);h.set("creativeMix",100);CreativeFX fx;fx.bind(h.state);fx.prepare({rate,(juce::uint32)block,2});juce::AudioBuffer<float> b(2,block);
    double peak=0;bool finite=true;allocations=0;
    for(int k=0;k<9*120;++k){
        if(k%120==0){h.set("creativeModel",float(k/120));for(const char* id:{"creativeA","creativeB","creativeC"})h.set(id,(k/120)%2?100.f:0.f);}
        for(int n=0;n<block;++n)for(int c=0;c<2;++c)b.setSample(c,n,.5f*noise(k*block+n,c));
        countAllocations=true;fx.process(b,120);countAllocations=false;
        for(int c=0;c<2;++c)for(int n=0;n<block;++n){const float v=b.getSample(c,n);finite=finite&&std::isfinite(v);peak=std::max(peak,double(std::abs(v)));}
    }
    std::cout<<"MEASURE model sweep at extreme settings: peak "<<peak<<", allocations "<<allocations<<"\n";
    require(finite && peak<4 && allocations==0,"CREATIVE became unbounded or allocated");
    std::cout<<"PASS model crossfades, bounded extremes, allocation-free callback\n";
}

void processorAndEditor() {
    // In the processor the slot sits in the POST chain: Bit Crush at 2 bits changes the output; off restores it.
    const auto run=[](bool creative){auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(rate,block);p->prepareToPlay(rate,block);
        auto set=[&](const char* id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
        if(creative){set("creativeOn",1);set("creativeModel",7);set("creativeA",100);set("creativeB",0);set("creativeC",0);set("creativeMix",100);}
        juce::AudioBuffer<float> b(2,block);juce::MidiBuffer midi;std::vector<float> out;
        for(int k=0;k<60;++k){for(int n=0;n<block;++n)for(int c=0;c<2;++c)b.setSample(c,n,float(.3*std::sin(2*pi*220*(k*block+n)/rate)));p->processBlock(b,midi);for(int n=0;n<block;++n)out.push_back(b.getSample(0,n));}
        return out;};
    const auto plain=run(false),crushed=run(true);double diff=0;for(size_t n=0;n<plain.size();++n)diff=std::max(diff,double(std::abs(plain[n]-crushed[n])));
    std::cout<<"MEASURE processor: CREATIVE Bit Crush changes the output by up to "<<diff<<"\n";
    require(diff>1e-3,"The CREATIVE slot is not in the processor's signal chain");
    auto p=std::make_unique<ChimeraProcessor>();
    const auto raw=[&](const char* id){return p->parameters().getRawParameterValue(id)->load();};
    spectralforge::creativeUI::CreativeStrip strip(*p);strip.setSize(850,24);
    spectralforge::creativeUI::CreativePanel panel(*p);
    juce::Button* on=nullptr;for(auto* c:strip.getChildren())if(c->getComponentID()=="creativeStrip.on")on=dynamic_cast<juce::Button*>(c);
    require(on!=nullptr,"CREATIVE strip switch missing");
    on->setToggleState(true,juce::sendNotificationSync);strip.modelBox().setSelectedId(5,juce::sendNotificationSync);
    require(raw("creativeOn")>.5f && juce::roundToInt(raw("creativeModel"))==4,"CREATIVE strip is not attached");
    panel.refreshModel();require(panel.controlLabel(0)=="SHIFT" && panel.controlLabel(1)=="FEEDBACK" && panel.controlLabel(2)=="SPREAD","Panel labels do not follow the model");
    const auto path=spectralforge::signalPath::read(*p);const auto* node=path.find("post.creative");
    require(node!=nullptr && node->label=="Freq Shift" && node->active,"Signal path lacks the CREATIVE node");
    std::cout<<"PASS processor chain, strip and panel attached, labels per model, signal-path node\n";
}

// ---- CREATIVE page (docs/CREATIVE_PAGE.md): patterns, Laser pitch modes, placement ----
struct PageHost final : juce::AudioProcessor {
    juce::AudioProcessorValueTreeState state;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout(){juce::AudioProcessorValueTreeState::ParameterLayout l;appendCreativeParameters(l);appendCreativePageParameters(l);return l;}
    PageHost():state(*this,nullptr,"CREATIVE",layout()){}
    void set(const juce::String& id,float v){auto* p=state.getParameter(id);require(p!=nullptr,("no parameter "+id).toStdString());p->setValueNotifyingHost(p->convertTo0to1(v));}
    const juce::String getName() const override {return "creative page";}
    void prepareToPlay(double,int) override {}void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override {return 0;}bool acceptsMidi() const override {return false;}bool producesMidi() const override {return false;}
    juce::AudioProcessorEditor* createEditor() override {return nullptr;}bool hasEditor() const override {return false;}
    int getNumPrograms() override {return 1;}int getCurrentProgram() override {return 0;}void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return {};}void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}void setStateInformation(const void*,int) override {}
};
// Renders through a slot bound to the page layout; `ppq` >= 0 reports a playing host from that position.
template<class Input> Stereo renderPage(PageHost& h,int samples,Input input,double tempo=120,double ppq=-1) {
    CreativeFX fx;fx.bind(h.state);fx.prepare({rate,(juce::uint32)block,2});
    Stereo out{std::vector<float>((size_t)samples),std::vector<float>((size_t)samples)};juce::AudioBuffer<float> b(2,block);
    for(int offset=0;offset<samples;offset+=block){
        for(int n=0;n<block;++n){b.setSample(0,n,input(offset+n,0));b.setSample(1,n,input(offset+n,1));}
        if(ppq>=0)fx.setTransport(ppq+double(offset)*tempo/60.0/rate);
        countAllocations=true;fx.process(b,tempo);countAllocations=false;
        for(int n=0;n<block && offset+n<samples;++n){out.l[size_t(offset+n)]=b.getSample(0,n);out.r[size_t(offset+n)]=b.getSample(1,n);}
    }
    return out;
}
// Peak of a 1 kHz sine over the middle half of step `index` (steps of `stepSamples` from `origin`).
double stepPeak(const std::vector<float>& x,int origin,int stepSamples,int index){double p=0;for(int n=origin+index*stepSamples+stepSamples/4;n<origin+index*stepSamples+stepSamples*3/4;++n)p=std::max(p,double(std::abs(x[size_t(n)])));return p;}

void slicerPatterns() {
    // Straight with attack 0 is the released gate, sample for sample.
    {CreativeVoice voice;voice.prepare(rate);CreativeSettings s;s.on=true;s.model=0;s.a=.37f;s.b=.8f;s.c=.3f;s.sync=0;
     std::vector<float> l(48000),r(48000);for(int n=0;n<48000;++n){l[size_t(n)]=.4f*noise(n,0);r[size_t(n)]=.4f*noise(n,1);}
     std::array<std::vector<float>,2> wet{std::vector<float>(48000),std::vector<float>(48000)};const float* in[2]{l.data(),r.data()};
     voice.process(0,s,120,in,wet,48000);
     double phase=0,gate=1;const double hz=1.0*std::pow(20.0/1.0,double(s.a));
     for(int n=0;n<48000;++n){phase+=hz/rate;phase-=std::floor(phase);const double target=phase<.1+.8*s.c?1.0:1.0-s.b;gate+=(target-gate)*(1.0-std::exp(-1.0/(.002*rate)));
         require(wet[0][size_t(n)]==float(l[size_t(n)]*gate) && wet[1][size_t(n)]==float(r[size_t(n)]*gate),"Straight pattern is not the released gate");}}
    // 1/16 at 120 BPM: 6000-sample steps. Depth 100 %, duty 100 % (0.9 of the step).
    const int step=6000;const auto tone=sine(1000,.5);
    for(const int pattern:{1,2,5,8}){
        PageHost h;h.set("creativeOn",1);h.set("creativeModel",0);h.set("creativeB",100);h.set("creativeC",100);h.set("creativeMix",100);h.set("creativeSync",4);h.set("creativePattern",float(pattern));
        const auto out=renderPage(h,step*40,tone,120,0.0);
        for(int k=16;k<32;++k){const double want=.5*creativePatterns[size_t(pattern)].levels[size_t(k%16)],got=stepPeak(out.l,0,step,k);
            require(std::abs(got-want)<.01,std::string("Pattern ")+creativePatterns[size_t(pattern)].name+" step "+std::to_string(k%16+1)+" level "+std::to_string(got)+" != "+std::to_string(want));}
    }
    // Custom: 3 steps at 100 / 0 / 50 %; the fourth step is the first again.
    {PageHost h;h.set("creativeOn",1);h.set("creativeModel",0);h.set("creativeB",100);h.set("creativeC",100);h.set("creativeMix",100);h.set("creativeSync",4);
     h.set("creativePattern",float(creativeCustomPattern));h.set("creativeLength",3);h.set("creativeStep1",100);h.set("creativeStep2",0);h.set("creativeStep3",50);
     const auto out=renderPage(h,step*20,tone,120,0.0);const std::array<double,3> want{.5,0,.25};
     for(int k=6;k<18;++k)require(std::abs(stepPeak(out.l,0,step,k)-want[size_t(k%3)])<.01,"Custom pattern or its length is wrong");
     // Alternate: even steps left, odd steps right.
     h.set("creativeLength",2);h.set("creativeStep2",100);h.set("creativeStereo",1);const auto wide=renderPage(h,step*20,tone,120,0.0);
     for(int k=6;k<18;++k){const double l=stepPeak(wide.l,0,step,k),r=stepPeak(wide.r,0,step,k);
         require(k%2==0?(l>.49 && r<.01):(r>.49 && l<.01),"Alternate does not switch sides step by step");}}
    // Host position: starting 2.5 steps into the bar plays step 3 (0-based 2) at once, half-way through.
    {PageHost h;h.set("creativeOn",1);h.set("creativeModel",0);h.set("creativeB",100);h.set("creativeC",100);h.set("creativeMix",100);h.set("creativeSync",4);h.set("creativePattern",8);   // Ramp up
     const auto out=renderPage(h,step*8,tone,120,2.5*.25);
     // Step 3 (level 1.0) starts at -3000 samples; step 4 starts at 3000: .25, .5, .75, 1 follow.
     const std::array<double,4> want{.125,.25,.375,.5};for(int k=1;k<5;++k)require(std::abs(stepPeak(out.l,3000,step,k)-want[size_t((k-1)%4)])<.01,"The slicer does not follow the host position");}
    // Attack 100 %: the gate opens over ~42 ms instead of 2 ms.
    {const auto riseTime=[&](float attack){PageHost h;h.set("creativeOn",1);h.set("creativeModel",0);h.set("creativeB",100);h.set("creativeC",50);h.set("creativeMix",100);h.set("creativeSync",2);h.set("creativePattern",0);h.set("creativeAttack",attack);
         const auto out=renderPage(h,96000,tone,120,0.0);   // quarter notes: 24000-sample steps, opening at 48000
         int t10=-1,t90=-1;for(int n=48000;n<60000;++n){double p=0;for(int k=0;k<48;++k)p=std::max(p,double(std::abs(out.l[size_t(n+k)])));if(t10<0 && p>=.05)t10=n;if(t90<0 && p>=.45){t90=n;break;}}
         return (t90-t10)/rate*1000.0;};
     const double fast=riseTime(0),slow=riseTime(100);
     std::cout<<"MEASURE slicer attack 10-90 %: "<<fast<<" ms at 0 %, "<<slow<<" ms at 100 %\n";
     require(fast<8 && slow>60 && slow<130,"Slicer attack does not set the opening time");}
    std::cout<<"PASS slicer: Straight is the released gate sample for sample; built-in and custom patterns, length, alternate stereo, host position, attack\n";
}

void laserPitch() {
    // A 440 Hz note starting at 0.2 s. Dive, range 50 % (12 st), sweep 50 % (310 ms).
    const auto note=[](int n,int){return n<int(rate*.2)?0.f:float(.4*std::sin(2*pi*440*n/rate));};
    const auto run=[&](int mode){PageHost h;h.set("creativeOn",1);h.set("creativeModel",3);h.set("creativeA",50);h.set("creativeB",50);h.set("creativeC",50);h.set("creativeMix",100);h.set("creativeLaserMode",float(mode));
        return renderPage(h,int(rate*1.2),note);};
    const auto dive=run(laserPitchDive),rise=run(laserPitchRise);const int t0=int(rate*.2);
    // Expected at t after the onset: 440 x 2^(+-12 x (1 - t / 0.31)^2 / 12).
    const auto expected=[](double t,double sign){const double left=1-t/.31;return 440*std::pow(2.0,sign*12*left*left/12);};
    const double d1=frequency(dive.l,t0+int(rate*.05),t0+int(rate*.09)),d2=frequency(dive.l,t0+int(rate*.18),t0+int(rate*.22));
    const double r1=frequency(rise.l,t0+int(rate*.05),t0+int(rate*.09)),r2=frequency(rise.l,t0+int(rate*.18),t0+int(rate*.22));
    std::cout<<"MEASURE Laser dive: "<<d1<<" Hz at 50-90 ms ("<<expected(.07,1)<<" expected), "<<d2<<" Hz at 180-220 ms ("<<expected(.2,1)<<"); rise: "<<r1<<" Hz ("<<expected(.07,-1)<<"), "<<r2<<" Hz ("<<expected(.2,-1)<<")\n";
    require(std::abs(d1/expected(.07,1)-1)<.08 && std::abs(d2/expected(.2,1)-1)<.05 && d1>d2 && d2>440,"Laser dive does not fall onto the note");
    require(std::abs(r1/expected(.07,-1)-1)<.08 && std::abs(r2/expected(.2,-1)-1)<.05 && r1<r2 && r2<440,"Laser rise does not climb onto the note");
    // After the sweep (and its 10 ms blend) the note passes untouched.
    double late=0;for(int n=t0+int(rate*.45);n<int(rate*1.2);++n)late=std::max(late,double(std::abs(dive.l[size_t(n)]-note(n,0))));
    std::cout<<"MEASURE Laser after the sweep: largest difference from the input "<<late<<"\n";
    require(late<1e-4,"Laser does not return to the unshifted note");
    // Range 0: nothing is shifted.
    {PageHost h;h.set("creativeOn",1);h.set("creativeModel",3);h.set("creativeC",0);h.set("creativeMix",100);const auto flat=renderPage(h,int(rate*.6),note);
     const double hz=frequency(flat.l,t0+int(rate*.05),t0+int(rate*.25));require(std::abs(hz-440)<1,"Laser range 0 still shifts");}
    // The filter mode is the released sweep: the page layout's Filter sweep equals a slot without page parameters.
    {const auto x=[](int n,int c){return .25f*noise(n,c);};Setup s;s.model=3;const auto released=render(s,24000,x);
     PageHost h;h.set("creativeOn",1);h.set("creativeModel",3);h.set("creativeMix",100);h.set("creativeLaserMode",float(laserFilterSweep));const auto page=renderPage(h,24000,x);
     for(int n=0;n<24000;++n)require(page.l[size_t(n)]==released.l[size_t(n)] && page.r[size_t(n)]==released.r[size_t(n)],"Filter sweep mode is not the released Zap Sweep");}
    std::cout<<"PASS Laser: pitch dive and rise follow their law and land on the note; range 0 is unshifted; Filter sweep is the released model sample for sample\n";
}

void placement() {
    PageHost h;h.set("creativeOn",1);h.set("creativeModel",7);h.set("creativeA",100);h.set("creativeMix",100);   // Bit Crush, 2 bits: audible anywhere
    CreativeFX fx;fx.bind(h.state);fx.prepare({rate,(juce::uint32)block,2});juce::AudioBuffer<float> b(2,block);
    const auto fill=[&](int k){for(int n=0;n<block;++n)for(int c=0;c<2;++c)b.setSample(c,n,float(.3*std::sin(2*pi*220*(k*block+n)/rate)));};
    // Which of the four points changed the block, as a bit mask.
    const auto pass=[&](int k){int mask=0;for(int point=0;point<creativeInsertCount;++point){fill(k);juce::AudioBuffer<float> copy(b);fx.processAt(point,b,120);
        bool changed=false;for(int n=0;n<block;++n)changed|=b.getSample(0,n)!=copy.getSample(0,n);if(changed)mask|=1<<point;}return mask;};
    for(int k=0;k<20;++k)require(pass(k)==1<<creativeInPostRack,"A slot placed in the POST rack ran elsewhere");
    require(fx.placement()==creativeInPostRack,"Placement did not start in the POST rack");
    h.set("creativeInsert",float(creativeAtAmpInput));
    // The move: the effect fades out at the old point, then runs only at the new one.
    int k=20,blocksToMove=0;for(;k<60 && fx.placement()!=creativeAtAmpInput;++k,++blocksToMove)require((pass(k)&~(1<<creativeInPostRack))==0,"The slot ran at two points during a move");
    require(fx.placement()==creativeAtAmpInput && blocksToMove*block/rate<.06,"The slot did not move within 60 ms");
    for(;k<80;++k)require(pass(k)==1<<creativeAtAmpInput,"After the move the slot does not run at the amp input only");
    // No click: through a move the output never steps more than the effect itself does.
    {PageHost g;g.set("creativeOn",1);g.set("creativeModel",0);g.set("creativeB",0);g.set("creativeMix",100);   // slicer at depth 0 passes the signal
     CreativeFX one;one.bind(g.state);one.prepare({rate,(juce::uint32)block,2});double last=0,jump=0;
     for(int q=0;q<80;++q){if(q==30)g.set("creativeInsert",float(creativeAfterPost));fill(q);for(int point=0;point<creativeInsertCount;++point)one.processAt(point,b,120);
         for(int n=0;n<block;++n){const double v=b.getSample(0,n);if(q>0||n>0)jump=std::max(jump,std::abs(v-last));last=v;}}
     std::cout<<"MEASURE placement move: largest sample-to-sample step "<<jump<<" (a 220 Hz sine at 0.3 steps 0.0086)\n";
     require(jump<.012,"Moving the slot clicks");}
    // Off: every point is sample-exact, and an idle slot follows the placement at once.
    h.set("creativeOn",0);for(int q=0;q<40;++q)pass(100+q);
    require(!fx.isActive() && pass(200)==0,"CREATIVE off is not exact at every placement");
    h.set("creativeInsert",float(creativeBeforeTone));pass(201);require(fx.placement()==creativeBeforeTone,"An idle slot does not follow its placement");
    std::cout<<"PASS placement: one point at a time, a move fades out and in without a click, off is exact everywhere\n";
}

void pageInProcessor() {
    const auto make=[]{auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(rate,block);p->prepareToPlay(rate,block);return p;};
    const auto run=[&](int insert,bool on){auto p=make();
        auto set=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);require(q!=nullptr,("missing "+id).toStdString());q->setValueNotifyingHost(q->convertTo0to1(v));};
        if(on){set("creativeOn",1);set("creativeModel",7);set("creativeA",100);set("creativeB",0);set("creativeC",0);set("creativeMix",100);}
        set("creativeInsert",float(insert));
        juce::AudioBuffer<float> b(2,block);juce::MidiBuffer midi;std::vector<float> out;
        for(int k=0;k<80;++k){for(int n=0;n<block;++n)for(int c=0;c<2;++c)b.setSample(c,n,float(.3*std::sin(2*pi*220*(k*block+n)/rate)));p->processBlock(b,midi);for(int n=0;n<block;++n)out.push_back(b.getSample(0,n));}
        return out;};
    const auto differ=[](const std::vector<float>& x,const std::vector<float>& y){double d=0;for(size_t n=0;n<x.size();++n)d=std::max(d,double(std::abs(x[n]-y[n])));return d;};
    const auto plain=run(creativeInPostRack,false);std::array<std::vector<float>,creativeInsertCount> placed;
    for(int i=0;i<creativeInsertCount;++i){placed[size_t(i)]=run(i,true);
        require(differ(plain,run(i,false))==0,"Placement changes the sound of a slot that is off");
        require(differ(plain,placed[size_t(i)])>1e-3,std::string("The slot is not heard at ")+creativeInsertNames()[i].toStdString());}
    const double input=differ(placed[size_t(creativeAtAmpInput)],placed[size_t(creativeInPostRack)]);
    std::cout<<"MEASURE processor: Bit Crush at the amp input differs from the POST rack by up to "<<input<<"\n";
    require(input>1e-3,"Amp input placement sounds the same as the POST rack");
    // Parameters: both slots, version hint 36; the Laser defaults to the pitch dive in a new instance...
    auto p=make();int count=0;
    for(auto* parameter:p->getParameters())if(auto* with=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))
        if(with->paramID.contains("creativeInsert")||with->paramID.contains("creativePattern")||with->paramID.contains("creativeLength")||with->paramID.contains("creativeAttack")||with->paramID.contains("creativeStereo")||with->paramID.contains("creativeLaserMode")||with->paramID.contains("creativeStep")){
            // Slots 3 and 4 came with their whole bank at version 37 (docs/POST_FOUR_SLOTS.md).
            const bool late=with->paramID.startsWith("slot3_")||with->paramID.startsWith("slot4_");
            if(!late)++count;require(with->getVersionHint()==(late?postExtraSlotParameterVersion:creativePageParameterVersion),"CREATIVE page parameter has the wrong version hint");}
    require(count==2*creativePageParameterCount,"CREATIVE page parameter count is "+std::to_string(count));
    const auto raw=[&](const juce::String& id){return p->parameters().getRawParameterValue(id)->load();};
    require(juce::roundToInt(raw("creativeLaserMode"))==laserPitchDive && juce::roundToInt(raw("creativeInsert"))==creativeInPostRack && juce::roundToInt(raw("creativePattern"))==0,"New-instance defaults are wrong");
    // ...and a project saved before the page restores the released filter sweep, in both slots.
    {juce::MemoryBlock saved;p->getStateInformation(saved);
     auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));require(xml!=nullptr,"State is not XML");
     auto tree=juce::ValueTree::fromXml(*xml);int removed=0;
     for(int i=tree.getNumChildren();--i>=0;){const auto id=tree.getChild(i).getProperty("id").toString();
         if(id.contains("creativeInsert")||id.contains("creativePattern")||id.contains("creativeLength")||id.contains("creativeAttack")||id.contains("creativeStereo")||id.contains("creativeLaserMode")||id.contains("creativeStep")){tree.removeChild(i,nullptr);++removed;}}
     require(removed==postSlotCount*creativePageParameterCount,"Saved state does not hold every CREATIVE page parameter ("+std::to_string(removed)+")");
     juce::MemoryBlock old;if(auto oldXml=tree.createXml())juce::AudioProcessor::copyXmlToBinary(*oldXml,old);
     auto q=make();q->setStateInformation(old.getData(),int(old.getSize()));
     const auto value=[&](const juce::String& id){return juce::roundToInt(q->parameters().getRawParameterValue(id)->load());};
     require(value("creativeLaserMode")==laserFilterSweep && value(postSlotID(1,"creativeLaserMode"))==laserFilterSweep,"An older project does not keep the Laser's filter sweep");
     require(value("creativeInsert")==creativeInPostRack && value("creativePattern")==0 && value("creativeStereo")==0,"An older project does not restore the released CREATIVE behaviour");
     // A project saved now keeps what it chose.
     auto set=[&](const juce::String& id,float v){auto* r=p->parameters().getParameter(id);r->setValueNotifyingHost(r->convertTo0to1(v));};
     set("creativeLaserMode",float(laserPitchRise));set("creativeInsert",float(creativeAtAmpInput));set("creativePattern",float(creativeCustomPattern));set("creativeStep5",25);set(postSlotID(1,"creativeInsert"),float(creativeAfterPost));
     juce::MemoryBlock now;p->getStateInformation(now);auto r=make();r->setStateInformation(now.getData(),int(now.getSize()));
     const auto got=[&](const juce::String& id){return r->parameters().getRawParameterValue(id)->load();};
     require(juce::roundToInt(got("creativeLaserMode"))==laserPitchRise && juce::roundToInt(got("creativeInsert"))==creativeAtAmpInput && juce::roundToInt(got("creativePattern"))==creativeCustomPattern
         && std::abs(got("creativeStep5")-25)<.01 && juce::roundToInt(got(postSlotID(1,"creativeInsert")))==creativeAfterPost,"CREATIVE page settings do not survive save and reopen");}
    // The page: tiles select the model, names follow it, the grid edits the Custom pattern.
    {auto q=make();spectralforge::creativeUI::CreativePage page(*q);page.setSize(1140,438);
     const auto value=[&](const juce::String& id){return q->parameters().getRawParameterValue(id)->load();};
     auto* tile=dynamic_cast<juce::Button*>(page.control("creativeTile3"));require(tile!=nullptr,"Laser tile missing");tile->onClick();
     require(juce::roundToInt(value("creativeModel"))==3 && page.view(0).controlName(2)=="RANGE","The Laser tile does not select the model or name its controls");
     auto* q3=q->parameters().getParameter("creativeLaserMode");q3->setValueNotifyingHost(q3->convertTo0to1(float(laserFilterSweep)));
     require(page.view(0).controlName(2)=="RESONANCE","Filter sweep mode does not name control C RESONANCE");
     dynamic_cast<juce::Button*>(page.control("creativeTile0"))->onClick();
     auto* pattern=q->parameters().getParameter("creativePattern");pattern->setValueNotifyingHost(pattern->convertTo0to1(2));   // Gallop
     auto& grid=page.view(0).stepGrid();require(grid.isVisible() && grid.getWidth()>300,"The step grid is not shown for the slicer");
     const auto cell=grid.cell(1).toNearestInt();   // Gallop step 2 is closed: drag it half open
     const auto event=[&](juce::Point<int> at){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at.toFloat(),{},juce::MouseInputSource::defaultPressure,0,0,0,0,&grid,&grid,juce::Time::getCurrentTime(),at.toFloat(),juce::Time::getCurrentTime(),1,false);};
     const juce::Point<int> at{cell.getCentreX(),grid.getHeight()/2};grid.mouseDown(event(at));grid.mouseUp(event(at));
     require(juce::roundToInt(value("creativePattern"))==creativeCustomPattern,"Editing a built-in pattern does not switch to Custom");
     require(std::abs(value("creativeStep1")-100)<.01 && std::abs(value("creativeStep2")-50)<3 && std::abs(value("creativeStep3")-100)<.01 && std::abs(value("creativeStep4")-100)<.01,"Custom did not start from the pattern that was showing");
     // Slot 2 has its own controls; showing it changes no parameter.
     juce::MemoryBlock before,after;q->getStateInformation(before);page.showSlot(1);q->getStateInformation(after);
     require(before==after && page.shownSlot()==1 && page.control(postSlotID(1,"creativeOn"))!=nullptr && page.view(1).isVisible() && !page.view(0).isVisible(),"Slot 2 view is wrong or changed the sound");}
    std::cout<<"PASS processor: four placements heard, off exact, "<<count<<" page parameters at hint 36, older projects keep the filter sweep, save / reopen, page tiles, names and step grid\n";
}

// The real editor: the stage strip's CREATIVE button and a POST strip's ALL open the page.
// CHIMERA_CREATIVE_SNAPSHOTS=<dir> also writes the page as PNG files (2x).
void pageInEditor() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(rate,block);p->prepareToPlay(rate,block);
    auto set=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());require(editor!=nullptr,"No editor");
    if(auto* full=dynamic_cast<ChimeraEditor*>(editor.get()))full->setEasyMode(false,false);   // the full editor, whatever this machine remembers
    editor->setVisible(true);editor->addToDesktop(0);
    const auto pump=[]{juce::MessageManager::getInstance()->runDispatchLoopUntil(250);};pump();
    const std::function<juce::Component*(juce::Component&,const juce::String&)> find=[&](juce::Component& root,const juce::String& id)->juce::Component*{
        if(root.getComponentID()==id)return &root;for(auto* c:root.getChildren())if(auto* hit=find(*c,id))return hit;return nullptr;};
    auto* page=find(*editor,"creativePage");require(page!=nullptr && !page->isVisible(),"CREATIVE page missing or shown at start");
    auto* stage=dynamic_cast<juce::Button*>(find(*editor,"path.creative"));require(stage!=nullptr && stage->isShowing(),"The stage strip has no CREATIVE button");
    juce::MemoryBlock before,after;p->getStateInformation(before);
    stage->onClick();pump();p->getStateInformation(after);
    require(page->isShowing() && before==after,"The CREATIVE stage button does not open the page, or opening it changed the sound");
    require(find(*editor,"synthPage")!=nullptr && !find(*editor,"synthPage")->isVisible(),"SYNTH page shows together with CREATIVE");
    // Back to POST, then ALL on the second slot's strip: the page opens on slot 2.
    auto* post=dynamic_cast<juce::Button*>(find(*editor,"path.post"));require(post!=nullptr,"POST stage button missing");post->onClick();pump();
    require(!page->isVisible(),"CREATIVE page stays over the POST page");
    auto* all=dynamic_cast<juce::Button*>(find(*editor,"creativeStrip.all"));require(all!=nullptr && all->isShowing(),"POST strip ALL missing");
    all->onClick();pump();
    auto* creative=dynamic_cast<spectralforge::creativeUI::CreativePage*>(page);
    require(page->isShowing() && creative!=nullptr && creative->shownSlot()==0 && stage->getToggleState(),"ALL does not open the CREATIVE page on its slot");
    if(const auto directory=juce::SystemStats::getEnvironmentVariable("CHIMERA_CREATIVE_SNAPSHOTS",{});directory.isNotEmpty()){
        const auto save=[&](const juce::String& name){pump();const auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,2.f);
            juce::File file=juce::File(directory).getChildFile(name);file.getParentDirectory().createDirectory();file.deleteFile();juce::FileOutputStream out(file);juce::PNGImageFormat png;require(out.openedOk() && png.writeImageToStream(image,out),"Snapshot not written");};
        set("creativeOn",1);set("creativeModel",0);set("creativePattern",2);set("creativeB",100);set("creativeSync",4);creative->view(0).refresh();save("creative-slicer.png");
        set("creativeModel",3);set("creativeInsert",float(creativeAtAmpInput));creative->view(0).refresh();save("creative-laser.png");
        set("creativeModel",6);creative->view(0).refresh();save("creative-glitch.png");
    }
    editor->removeFromDesktop();editor.reset();
    std::cout<<"PASS editor: CREATIVE stage button and POST strip ALL open the page; opening changes no parameter\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {offAndMix();gateSlicer();stepHarmonizer();timeBend();zapSweep();freqShift();freezeHold();glitchStutter();bitCrush();grainCloud();contract();processorAndEditor();slicerPatterns();laserPitch();placement();pageInProcessor();pageInEditor();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
