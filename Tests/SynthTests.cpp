// 1.10 SYNTH layer (docs/SYNTH_1_10.md): pitch-tracker accuracy, latency and
// octave errors on guitar-range notes (low B to E5, including a weak
// fundamental), MIDI pitch, the three layers' declared behaviour, the off
// contract, allocation, and the processor / editor wiring.
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "SynthLayer.h"
#include "SynthPanel.h"
#include "SignalPath.h"
#include <algorithm>
#include <cmath>
#include <complex>
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
constexpr double rate=48000,pi=3.14159265358979323846;constexpr int block=256;
void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
double db(double v){return 20*std::log10(std::max(v,1.e-30));}
struct Host final : juce::AudioProcessor {
    juce::AudioProcessorValueTreeState state;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout(){juce::AudioProcessorValueTreeState::ParameterLayout l;appendSynthParameters(l);return l;}
    Host():state(*this,nullptr,"SYNTH",layout()){}
    void set(const char* id,float v){auto* p=state.getParameter(id);require(p!=nullptr,std::string("no parameter ")+id);p->setValueNotifyingHost(p->convertTo0to1(v));}
    const juce::String getName() const override {return "synth";}
    void prepareToPlay(double,int) override {}void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override {return 0;}bool acceptsMidi() const override {return true;}bool producesMidi() const override {return false;}
    juce::AudioProcessorEditor* createEditor() override {return nullptr;}bool hasEditor() const override {return false;}
    int getNumPrograms() override {return 1;}int getCurrentProgram() override {return 0;}void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return {};}void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}void setStateInformation(const void*,int) override {}
};
// A plucked-string stand-in with an exact pitch: ten harmonics at 1/k, each
// decaying faster than the last; `fundamental` scales harmonic 1 (a weak
// fundamental is common on low strings).
float pluck(double hz,int n,double fundamental=1.0){
    const double t=n/rate;double v=0;
    for(int k=1;k<=10;++k){if(k*hz>rate*.45)break;v+=(k==1?fundamental:1.0)/k*std::exp(-t*(1.0+.6*k))*std::sin(2*pi*k*hz*t);}
    return float(.3*v);
}
double magnitude(const std::vector<float>& x,int from,int to,double hz){
    std::complex<double> sum;for(int n=from;n<to;++n){const double w=.5-.5*std::cos(2*pi*(n-from)/(to-from));sum+=w*double(x[size_t(n)])*std::polar(1.0,-2*pi*hz*n/rate);}
    return 2*std::abs(sum)/((to-from)*.5);
}
double peakNear(const std::vector<float>& x,int from,int to,double near){double best=0,at=near;for(double hz=near*.9;hz<=near*1.1;hz+=.1){const double m=magnitude(x,from,to,hz);if(m>best){best=m;at=hz;}}return at;}
double rms(const std::vector<float>& x,int from,int to){double e=0;for(int n=from;n<to;++n)e+=double(x[size_t(n)])*x[size_t(n)];return std::sqrt(e/std::max(1,to-from));}

struct Track {double latencyMs{-1},medianCents{},octaveShare{};};
template<class Signal> Track track(double hz,Signal signal) {
    PitchTracker t;t.prepare(rate);std::vector<float> in(block);std::vector<double> cents;Track r;int frames=0,octaves=0;
    for(int k=0;k*block<int(rate*1.2);++k){
        for(int n=0;n<block;++n)in[size_t(n)]=signal(k*block+n);
        t.push(in.data(),nullptr,block);
        const double time=(k+1)*block/rate;
        if(t.hz()>0 && t.clarity()>.8){
            const double c=1200*std::log2(t.hz()/hz);
            if(r.latencyMs<0 && std::abs(c)<20)r.latencyMs=1000*time;
            if(time>.2){++frames;octaves+=std::abs(c)>50;cents.push_back(std::abs(c));}
        }
    }
    std::sort(cents.begin(),cents.end());r.medianCents=cents.empty()?1e9:cents[cents.size()/2];r.octaveShare=frames?double(octaves)/frames:1;
    return r;
}
void tracker() {
    struct Note{const char* name;double hz;};
    double worstLatency=0,worstCents=0,worstOctave=0;std::string lines;
    for(const Note& note:{Note{"B1",61.735},Note{"E2",82.407},Note{"A2",110.0},Note{"G3",196.0},Note{"E4",329.63},Note{"A4",440.0},Note{"E5",659.26}}){
        for(int kind=0;kind<3;++kind){
            const auto r=kind==0?track(note.hz,[&](int n){return float(.3*std::sin(2*pi*note.hz*n/rate));})
                        :track(note.hz,[&](int n){return pluck(note.hz,n,kind==1?1.0:.2);});
            lines+=std::string(" ")+note.name+(kind==0?" sine":kind==1?" pluck":" weak-fund")+" "+juce::String(r.latencyMs,0).toStdString()+" ms/"+juce::String(r.medianCents,2).toStdString()+" ct/"+juce::String(r.octaveShare*100,1).toStdString()+"%;";
            require(r.latencyMs>0,std::string("Tracker never locked on ")+note.name);
            worstLatency=std::max(worstLatency,r.latencyMs);worstCents=std::max(worstCents,r.medianCents);worstOctave=std::max(worstOctave,r.octaveShare);
        }
    }
    std::cout<<"MEASURE tracker (lock latency / median |error| / octave-error frames):"<<lines<<"\n";
    std::cout<<"MEASURE tracker worst: lock "<<worstLatency<<" ms, median error "<<worstCents<<" cents, octave errors "<<worstOctave*100<<" %\n";
    require(worstCents<5 && worstOctave<.02 && worstLatency<100,"Tracker accuracy, octave robustness or lock time out of bounds");
    std::cout<<"PASS pitch tracker on sines, plucks and weak fundamentals from low B to E5\n";
}

struct Run {std::vector<float> l;SynthLayer* layer{};};
// Renders the synth alone: `guitar(n)` is the tracked input, `midi` the notes per block.
template<class Guitar,class Midi> std::vector<float> render(Host& h,int samples,Guitar guitar,Midi midi,SynthLayer* keep=nullptr) {
    SynthLayer local;SynthLayer& s=keep?*keep:local;s.bind(h.state);s.prepare(rate,block);
    std::vector<float> out;juce::AudioBuffer<float> in(2,block),o(2,block);juce::MidiBuffer m;
    for(int k=0;k*block<samples;++k){
        for(int n=0;n<block;++n){const float x=guitar(k*block+n);in.setSample(0,n,x);in.setSample(1,n,x);}
        m.clear();midi(k,m);o.clear();
        countAllocations=true;s.handleMidi(m);s.analyse(in);s.process(o);countAllocations=false;
        for(int n=0;n<block;++n)out.push_back(o.getSample(0,n));
    }
    return out;
}
auto silence(){return [](int){return 0.f;};}
auto noMidi(){return [](int,juce::MidiBuffer&){};}
auto holdNote(int note,float velocity=1.f){return [=](int k,juce::MidiBuffer& m){if(k==0)m.addEvent(juce::MidiMessage::noteOn(1,note,velocity),0);};}

void gateAndMidi() {
    // Tracking threshold: -50 dBFS under a -40 dBFS threshold stays silent; -20 dBFS plays.
    const auto quiet=[](int n){return float(.00316*std::sin(2*pi*110*n/rate));};
    const auto loud=[](int n){return float(.1*std::sin(2*pi*110*n/rate));};
    Host h;h.set("synthOn",1);h.set("synthSource",1);
    const double under=rms(render(h,24000,quiet,noMidi()),12000,24000),over=rms(render(h,24000,loud,noMidi()),12000,24000);
    // MIDI A3 = 220 Hz; octave -1 halves it; pitch bend +2 st.
    Host m;m.set("synthOn",1);m.set("synthSource",2);m.set("synthCharacter",0);m.set("synthTone",100);
    const auto a3=render(m,48000,silence(),holdNote(57));const double f=peakNear(a3,12000,48000,220);
    m.set("synthOctave",-1);const double low=peakNear(render(m,48000,silence(),holdNote(57)),12000,48000,110);
    m.set("synthOctave",0);const double bent=peakNear(render(m,48000,silence(),[](int k,juce::MidiBuffer& b){if(k==0){b.addEvent(juce::MidiMessage::noteOn(1,57,1.f),0);b.addEvent(juce::MidiMessage::pitchWheel(1,16383),0);}}),12000,48000,247);
    std::cout<<"MEASURE threshold -40 dBFS: input -50 dBFS gives "<<db(under)<<" dBFS, -20 dBFS gives "<<db(over)<<" dBFS; MIDI A3 "<<f<<" Hz, octave -1 "<<low<<" Hz, bend +2 st "<<bent<<" Hz (246.9)\n";
    require(under==0 && over>1e-3 && std::abs(f-220)<.3 && std::abs(low-110)<.3 && std::abs(bent-246.94)<.5,"Tracking threshold or MIDI pitch wrong");
    std::cout<<"PASS tracking threshold, MIDI pitch, octave and bend\n";
}

void dvergr() {
    Host h;h.set("synthOn",1);h.set("synthSource",2);h.set("synthModel",0);h.set("synthCharacter",0);h.set("synthTone",100);
    const auto open=render(h,48000,silence(),holdNote(57));
    const double fundamental=magnitude(open,12000,48000,220),subOctave=magnitude(open,12000,48000,110);
    h.set("synthTone",0);const auto dark=render(h,48000,silence(),holdNote(57));
    double hiOpen=0,hiDark=0;for(double hz=3080;hz<9000;hz+=220){hiOpen+=std::pow(magnitude(open,12000,48000,hz),2);hiDark+=std::pow(magnitude(dark,12000,48000,hz),2);}
    std::cout<<"MEASURE Dvergr Analog: sub octave "<<db(subOctave/fundamental)<<" dB re the note; harmonics 3-9 kHz, Cutoff 0 vs 100 %: "<<10*std::log10(hiDark/hiOpen)<<" dB\n";
    require(db(subOctave/fundamental)>-12 && 10*std::log10(hiDark/hiOpen)<-30,"Dvergr Analog sub octave or cutoff wrong");
}

void seidr() {
    // Played by the guitar: louder playing opens the filter and raises the level.
    Host h;h.set("synthOn",1);h.set("synthSource",1);h.set("synthModel",1);h.set("synthCharacter",100);h.set("synthTone",20);h.set("synthAttack",5);
    const auto soft=render(h,48000,[](int n){return float(.03*std::sin(2*pi*110*n/rate));},noMidi());
    const auto hard=render(h,48000,[](int n){return float(.25*std::sin(2*pi*110*n/rate));},noMidi());
    const auto centroid=[](const std::vector<float>& x){double num=0,den=0;for(int k=1;k<=40;++k){const double m=magnitude(x,24000,48000,110.0*k);num+=110.0*k*m*m;den+=m*m;}return num/std::max(den,1e-30);};
    const double cs=centroid(soft),ch=centroid(hard),ls=rms(soft,24000,48000),lh=rms(hard,24000,48000);
    std::cout<<"MEASURE Seidr Dynamic, input -30 vs -12 dBFS: centroid "<<cs<<" -> "<<ch<<" Hz, level "<<db(ls)<<" -> "<<db(lh)<<" dBFS\n";
    require(ch>cs*1.5 && lh>ls*2,"Seidr Dynamic does not follow the playing");
}

void mimir() {
    // Release 500 ms: the strike decays 60 dB in 0.5 s, held or not; the FM index decays faster.
    Host h;h.set("synthOn",1);h.set("synthSource",2);h.set("synthModel",2);h.set("synthRelease",500);h.set("synthAttack",1);h.set("synthTone",60);h.set("synthCharacter",30);
    const auto out=render(h,int(rate*1.2),silence(),holdNote(57));
    std::vector<double> level;for(int n=0;n+480<=int(out.size());n+=480)level.push_back(db(rms(out,n,n+480)));
    const double top=*std::max_element(level.begin(),level.end());int a=-1,b=-1;
    for(size_t i=0;i<level.size();++i){if(a<0&&level[i]<top-10)a=int(i);if(b<0&&level[i]<top-40){b=int(i);break;}}
    require(a>0 && b>a,"Mimir Impact decay not measurable");
    const double t60=60.0*(b-a)*.01/30.0;
    const double earlySide=magnitude(out,240,1680,660)/magnitude(out,240,1680,220),lateSide=magnitude(out,int(rate*.15),int(rate*.25),660)/magnitude(out,int(rate*.15),int(rate*.25),220);
    std::cout<<"MEASURE Mimir Impact: T60 "<<t60<<" s (Release 0.5 s); 660 Hz sideband re 220 Hz: "<<db(earlySide)<<" dB early, "<<db(lateSide)<<" dB at 150-250 ms\n";
    require(std::abs(t60/.5-1)<.15 && db(earlySide)-db(lateSide)>20,"Mimir Impact decay or FM index envelope wrong");
}

void contract() {
    // Off adds nothing and runs nothing; switching off releases, then idles.
    Host h;SynthLayer s;s.bind(h.state);s.prepare(rate,block);juce::AudioBuffer<float> in(2,block),o(2,block);juce::MidiBuffer m;m.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
    for(int n=0;n<block;++n){in.setSample(0,n,.3f);in.setSample(1,n,.3f);o.setSample(0,n,.1f);o.setSample(1,n,.1f);}
    s.handleMidi(m);s.analyse(in);s.process(o);bool same=true;for(int n=0;n<block;++n)same=same&&o.getSample(0,n)==.1f&&o.getSample(1,n)==.1f;
    require(same && !s.sounding(),"SYNTH off changed the signal");
    h.set("synthOn",1);h.set("synthSource",2);h.set("synthRelease",100);int k=0;
    for(;k<40;++k){o.clear();s.process(o);}
    require(s.sounding(),"SYNTH does not sound on a held MIDI note");
    h.set("synthOn",0);int idle=-1;for(int j=0;j<200;++j){o.clear();s.process(o);if(!s.sounding()){idle=j;break;}}
    std::cout<<"MEASURE switched off while sounding: idle after "<<idle*block/rate*1000<<" ms (release 100 ms)\n";
    require(idle>0,"SYNTH does not go idle after it is switched off");
    // Extremes: every layer, octave +1, fast attack, max tone/character, loud guitar; bounded and allocation-free.
    double peak=0;bool finite=true;allocations=0;
    for(int model=0;model<3;++model){Host x;x.set("synthOn",1);x.set("synthModel",float(model));x.set("synthOctave",1);x.set("synthTone",100);x.set("synthCharacter",100);x.set("synthAttack",1);x.set("synthLevel",6);
        const auto out=render(x,48000,[](int n){return pluck(82.407,n%24000);},holdNote(96));for(float v:out){finite=finite&&std::isfinite(v);peak=std::max(peak,double(std::abs(v)));}}
    std::cout<<"MEASURE extremes: peak "<<peak<<", allocations "<<allocations<<"\n";
    require(finite && peak<8 && allocations==0,"SYNTH unbounded or allocating");
    std::cout<<"PASS off contract, release to idle, bounded extremes, allocation-free callback\n";
}

void processorAndEditor() {
    const auto run=[](bool synthOn){auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(rate,block);p->prepareToPlay(rate,block);
        auto set=[&](const char* id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
        if(synthOn){set("synthOn",1);set("synthSource",2);}
        juce::AudioBuffer<float> b(2,block);std::vector<float> out;
        for(int k=0;k<80;++k){juce::MidiBuffer midi;if(k==2)midi.addEvent(juce::MidiMessage::noteOn(1,57,1.f),0);b.clear();p->processBlock(b,midi);for(int n=0;n<block;++n)out.push_back(b.getSample(0,n));}
        return out;};
    const auto off=run(false),on=run(true);
    const double quiet=rms(off,8000,20480),played=rms(on,8000,20480),pitch=peakNear(on,8000,20480,220);
    std::cout<<"MEASURE processor, silent input + MIDI A3: off "<<db(quiet)<<" dBFS, on "<<db(played)<<" dBFS at "<<pitch<<" Hz\n";
    require(played>quiet*100 && played>1e-3 && std::abs(pitch-220)<1,"SYNTH is not in the processor's signal chain");
    auto p=std::make_unique<ChimeraProcessor>();const auto raw=[&](const char* id){return p->parameters().getRawParameterValue(id)->load();};
    spectralforge::synthUI::SynthStrip strip(*p);strip.setSize(850,24);spectralforge::synthUI::SynthPanel panel(*p);
    juce::Button* on2=nullptr;for(auto* c:strip.getChildren())if(c->getComponentID()=="synthStrip.on")on2=dynamic_cast<juce::Button*>(c);
    require(on2!=nullptr,"SYNTH strip switch missing");
    on2->setToggleState(true,juce::sendNotificationSync);strip.modelBox().setSelectedId(3,juce::sendNotificationSync);
    require(raw("synthOn")>.5f && juce::roundToInt(raw("synthModel"))==2,"SYNTH strip is not attached");
    panel.refreshModel();require(panel.controlLabel(3)=="STRIKE" && panel.controlLabel(4)=="RATIO","Panel labels do not follow the layer");
    const auto path=spectralforge::signalPath::read(*p);const auto* node=path.find("synth");
    bool edge=false;for(const auto& e:path.edges)edge|=e.from=="synth"&&e.to=="tone.eq";
    require(node!=nullptr && node->label=="Mimir Impact" && node->active && edge,"Signal path lacks the SYNTH node before Tone EQ");
    std::cout<<"PASS processor chain, strip and panel attached, labels per layer, signal-path node into Tone EQ\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {tracker();gateAndMidi();dvergr();seidr();mimir();contract();processorAndEditor();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
