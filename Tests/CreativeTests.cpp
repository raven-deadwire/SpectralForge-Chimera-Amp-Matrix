// 1.9 CREATIVE slot (docs/CREATIVE_1_9.md): each of the nine models measured
// on its documented law, plus the slot contract (off is sample-exact and
// idle, no allocation, bounded output, model crossfade) and the processor and
// editor wiring.
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "CreativeFX.h"
#include "CreativePanel.h"
#include "SignalPath.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdlib>
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
    std::cout<<"MEASURE Zap Sweep centroid: "<<early<<" Hz at 20-50 ms, "<<mid<<" Hz at 140-170 ms, "<<late<<" Hz after the sweep; "<<again<<" Hz after a new onset\n";
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
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {offAndMix();gateSlicer();stepHarmonizer();timeBend();zapSweep();freqShift();freezeHold();glitchStutter();bitCrush();grainCloud();contract();processorAndEditor();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
