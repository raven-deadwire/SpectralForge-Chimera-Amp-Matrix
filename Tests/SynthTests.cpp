// SYNTH (docs/SYNTH_1_10.md, docs/SYNTH_PAGE.md): pitch tracker accuracy and
// latency, the engine (tuning, wave morph, unison, sub / noise, filter against
// its analytic response, envelopes, LFOs, the modulation matrix, FM,
// polyphony, glide, guitar / MIDI precedence), the four insert points, the
// off contract, allocation, patches and the SYNTH page.
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "SynthLayer.h"
#include "SynthPage.h"
#include "SynthSampleIO.h"
#include "SignalPath.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <map>
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


// Renders the engine alone at the default insert (Before POST): guitar(n) is
// the tracked input, midi(k, buffer) the events of block k.
struct Out {std::vector<float> l,r;};
template<class Guitar,class Midi> Out render(Host& h,int samples,Guitar guitar,Midi midi,SynthLayer* keep=nullptr,double tempo=120) {
    SynthLayer local;SynthLayer& s=keep?*keep:local;s.bind(h.state);s.prepare(rate,block);
    Out out;juce::AudioBuffer<float> in(2,block),o(2,block);juce::MidiBuffer m;
    for(int k=0;k*block<samples;++k){
        for(int n=0;n<block;++n){const float x=guitar(k*block+n);in.setSample(0,n,x);in.setSample(1,n,x);}
        m.clear();midi(k,m);o.clear();
        countAllocations=true;s.handleMidi(m);s.analyse(in);s.render(block,tempo);s.addAt(SynthInsert::beforePost,o);countAllocations=false;
        for(int n=0;n<block;++n){out.l.push_back(o.getSample(0,n));out.r.push_back(o.getSample(1,n));}
    }
    return out;
}
auto silence(){return [](int){return 0.f;};}
auto noMidi(){return [](int,juce::MidiBuffer&){};}
auto hold(std::initializer_list<int> notes,float velocity=1.f){std::vector<int> v(notes);return [v,velocity](int k,juce::MidiBuffer& m){if(k==0)for(int n:v)m.addEvent(juce::MidiMessage::noteOn(1,n,velocity),0);};}
// A clean test voice: osc A only, filter off, instant envelope, MIDI only.
Host& clean(Host& h,float wave){
    for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{{"synthOn",1},{"synthSource",2},{"synthCharacter",0},{"synthFilterOn",0},{"synthOscAWave",wave},{"synthOscADetune",0},
        {"synthAmpAttack",.5f},{"synthAmpSustain",100},{"synthWidth",0}})h.set(id,v);
    return h;
}
double hz(double note){return 440*std::pow(2.0,(note-69)/12);}
double harmonicDb(const std::vector<float>& x,double f,int k){return db(magnitude(x,12000,48000,f*k)/magnitude(x,12000,48000,f));}

void tuningTest() {
    struct Case{const char* name;std::vector<std::pair<const char*,float>> set;double expected;};
    std::string line;bool ok=true;
    for(const Case& c:{Case{"A3",{},220.0},Case{"osc octave +1",{{"synthOscAOctave",1}},440.0},Case{"semi +7",{{"synthOscASemi",7}},hz(64)},
                        Case{"fine +50 ct",{{"synthOscAFine",50}},220*std::pow(2.0,.5/12)},Case{"global octave -1",{{"synthOctave",-1}},110.0}}){
        Host h;clean(h,0);for(const auto& [id,v]:c.set)h.set(id,v);
        const double f=peakNear(render(h,48000,silence(),hold({57})).l,12000,48000,c.expected);
        line+=std::string(" ")+c.name+" "+juce::String(f,2).toStdString()+" Hz;";ok=ok&&std::abs(f/c.expected-1)<.0005;
    }
    Host b;clean(b,0);const double bent=peakNear(render(b,48000,silence(),[](int k,juce::MidiBuffer& m){if(k==0){m.addEvent(juce::MidiMessage::noteOn(1,57,1.f),0);m.addEvent(juce::MidiMessage::pitchWheel(1,16383),0);}}).l,12000,48000,247);
    std::cout<<"MEASURE tuning:"<<line<<" bend +2 st "<<bent<<" Hz (246.94)\n";
    require(ok && std::abs(bent-246.94)<.3,"Oscillator tuning wrong");
    std::cout<<"PASS oscillator octave / semitone / fine / global octave / pitch bend\n";
}

void waveMorph() {
    // Harmonic levels re the fundamental at 220 Hz: saw 1/k, square odd 1/k, triangle odd 1/k^2, sine none.
    const auto run=[](float wave){Host h;clean(h,wave);return render(h,48000,silence(),hold({57})).l;};
    const auto sine=run(0),tri=run(1),saw=run(2),square=run(3),mid=run(2.5f);
    std::cout<<"MEASURE harmonics 2 / 3 (dB re 1): sine "<<harmonicDb(sine,220,2)<<" / "<<harmonicDb(sine,220,3)<<"; triangle "<<harmonicDb(tri,220,2)<<" / "<<harmonicDb(tri,220,3)
             <<" (-inf / -19.1); saw "<<harmonicDb(saw,220,2)<<" / "<<harmonicDb(saw,220,3)<<" (-6.0 / -9.5); square "<<harmonicDb(square,220,2)<<" / "<<harmonicDb(square,220,3)
             <<" (-inf / -9.5); saw-square 50 % "<<harmonicDb(mid,220,2)<<" / "<<harmonicDb(mid,220,3)<<"\n";
    require(harmonicDb(sine,220,2)<-60 && harmonicDb(sine,220,3)<-60,"Sine is not pure");
    require(harmonicDb(tri,220,2)<-40 && std::abs(harmonicDb(tri,220,3)+19.08)<.3,"Triangle harmonics wrong");
    require(std::abs(harmonicDb(saw,220,2)+6.02)<.2 && std::abs(harmonicDb(saw,220,3)+9.54)<.2,"Saw harmonics wrong");
    require(harmonicDb(square,220,2)<-40 && std::abs(harmonicDb(square,220,3)+9.54)<.2,"Square harmonics wrong");
    require(harmonicDb(mid,220,2)<harmonicDb(saw,220,2)-4,"The wave position does not morph saw towards square");
    // The display draws the same shapes the engine morphs.
    double worst=0;for(double p=.01;p<1;p+=.05)worst=std::max(worst,std::abs(synthWaveDisplay(2.0,p)-(1-2*p)));
    require(worst<1e-12,"Wave display differs from the saw it shows");
    std::cout<<"PASS wave morph: sine, triangle, saw, square and the positions in between\n";
}

void unisonSubNoise() {
    Host h;clean(h,2);h.set("synthOscAUnison",5);h.set("synthOscADetune",100);h.set("synthWidth",100);
    const auto wide=render(h,48000,silence(),hold({57}));
    double side=0,mid=0;for(size_t n=12000;n<48000;++n){side+=std::pow(wide.l[n]-wide.r[n],2);mid+=std::pow(wide.l[n]+wide.r[n],2);}
    h.set("synthWidth",0);const auto narrow=render(h,48000,silence(),hold({57}));double same=0;for(size_t n=0;n<narrow.l.size();++n)same=std::max(same,double(std::abs(narrow.l[n]-narrow.r[n])));
    Host s;clean(s,0);s.set("synthSubOn",1);s.set("synthSubLevel",100);const auto sub=render(s,48000,silence(),hold({57})).l;
    const double subDb=db(magnitude(sub,12000,48000,110)/magnitude(sub,12000,48000,220));
    Host n;clean(n,0);n.set("synthOscALevel",0);n.set("synthNoiseOn",1);n.set("synthNoiseLevel",100);const double noise=rms(render(n,48000,silence(),hold({57})).l,12000,48000);
    std::cout<<"MEASURE unison 5 x 100 %: side/mid energy "<<side/mid<<" at width 100 %, largest L-R "<<same<<" at width 0 %; sub -1 oct "<<subDb<<" dB re the note; noise alone "<<db(noise)<<" dBFS\n";
    require(side/mid>.05 && same==0 && std::abs(subDb-db(1.0/.8))<.5 && noise>1e-3,"Unison width, sub octave or noise wrong");
    std::cout<<"PASS unison spread and width, sub oscillator, noise\n";
}

void filterResponse() {
    // A 55 Hz saw through each filter type, relative to the same saw unfiltered, at its harmonics.
    Host plain;clean(plain,2);const auto dry=render(plain,96000,silence(),hold({33})).l;
    double worst=0;std::string line;
    for(int type=0;type<6;++type){
        Host h;clean(h,2);h.set("synthFilterOn",1);h.set("synthFilterType",float(type));h.set("synthCutoff",1000);h.set("synthResonance",30);
        const auto wet=render(h,96000,silence(),hold({33})).l;double typeWorst=0;
        for(int k:{2,5,9,18,36,72}){
            const double f=55.0*k;const double measured=db(magnitude(wet,24000,96000,f)/magnitude(dry,24000,96000,f));
            const double expected=db(synthFilterMagnitude(type,1000,.3,f,rate));
            if(expected>-40)typeWorst=std::max(typeWorst,std::abs(measured-expected));
        }
        worst=std::max(worst,typeWorst);line+=" "+synthFilterTypeNames()[type].toStdString()+" "+juce::String(typeWorst,3).toStdString()+" dB;";
    }
    std::cout<<"MEASURE filter vs analytic response, cutoff 1 kHz / resonance 30 %, largest error at harmonics 110 Hz-4 kHz:"<<line<<"\n";
    require(worst<.2,"Filter response differs from the curve the page draws");
    std::cout<<"PASS six filter types match the analytic response shown on the page\n";
}

std::vector<double> envelope(const std::vector<float>& x,int frame){std::vector<double> e;for(size_t n=0;n+size_t(frame)<=x.size();n+=size_t(frame))e.push_back(rms(x,int(n),int(n)+frame)*std::sqrt(2.0));return e;}
void envelopes() {
    // Amp A 100 ms, D 200 ms, S 50 %, R 300 ms; note off at 1 s; 2 ms frames of a 2 kHz sine.
    Host h;clean(h,0);h.set("synthAmpAttack",100);h.set("synthAmpDecay",200);h.set("synthAmpSustain",50);h.set("synthAmpRelease",300);
    const auto out=render(h,int(rate*1.6),silence(),[](int k,juce::MidiBuffer& m){if(k==0)m.addEvent(juce::MidiMessage::noteOn(1,95,1.f),0);if(k==int(rate/block))m.addEvent(juce::MidiMessage::noteOff(1,95),0);}).l;
    const auto e=envelope(out,96);const double peak=*std::max_element(e.begin(),e.end());
    int reach=-1;for(size_t i=0;i<e.size();++i)if(e[i]>=.98*peak){reach=int(i);break;}
    const double attack=(reach+.5)*2;
    const SynthEnvelopeSettings s{.1,.2,.5,.3};
    const double at250=e[125]/peak,expected250=synthEnvelopeLevel(s,.25,1.0),sustain=e[480]/peak;
    const int off=int((double(int(rate/block)*block)/rate)*500);int gone=-1;for(size_t i=size_t(off);i<e.size();++i)if(e[i]<sustain*peak*1e-3){gone=int(i);break;}
    const double release=(gone-off)*2.0;
    std::cout<<"MEASURE amp envelope: attack to 98 % "<<attack<<" ms (98), level at 250 ms "<<db(at250)<<" dB (analytic "<<db(expected250)<<"), sustain "<<db(sustain)<<" dB (-6.02), release to -60 dB "<<release<<" ms (300)\n";
    require(std::abs(attack-98)<5 && std::abs(db(at250)-db(expected250))<.3 && std::abs(db(sustain)+6.02)<.1 && std::abs(release-300)<15,"Amp envelope timing wrong");
    std::cout<<"PASS ADSR attack, decay, sustain and release against the graph's formula\n";
}

void lfoAndMatrix() {
    // LFO 1 (sine, 5 Hz) -> Pitch A at 10 % = +-2.4 st, on 880 Hz so every 10 ms window holds several cycles.
    Host h;clean(h,0);h.set("synthLfo1Rate",5);h.set("synthMod1Source",float(modLfo1));h.set("synthMod1Target",float(targetPitchA));h.set("synthMod1Amount",10);
    const auto out=render(h,96000,silence(),hold({81})).l;
    double lo=1e9,hi=0;for(int n=4800;n+480<96000;n+=240){double first=-1,last=-1;int c=0;for(int i=n+1;i<n+480;++i)if(out[size_t(i-1)]<0&&out[size_t(i)]>=0){const double t=i-1+out[size_t(i-1)]/(out[size_t(i-1)]-out[size_t(i)]);if(first<0)first=t;last=t;++c;}if(c>2){const double f=(c-1)*rate/(last-first);lo=std::min(lo,f);hi=std::max(hi,f);}}
    const double depth=12*std::log2(hi/lo)/2;
    // LFO 2 synced to 1/4 at 120 BPM (2 Hz) -> Amp at 100 %.
    Host a;clean(a,0);a.set("synthLfo2Sync",3);a.set("synthLfo2Shape",3);a.set("synthMod2Source",float(modLfo2));a.set("synthMod2Target",float(targetAmp));a.set("synthMod2Amount",100);
    const auto am=envelope(render(a,96000,silence(),hold({81})).l,48);int rises=0,first=-1,last=-1;for(size_t i=1;i<am.size();++i)if(am[i-1]<.5&&am[i]>=.5){++rises;if(first<0)first=int(i);last=int(i);}
    const double syncHz=rises>1?(rises-1)/((last-first)*.001):0;
    // Env 2 -> Cutoff: the saw darkens as Env 2 decays.
    Host f;clean(f,2);f.set("synthFilterOn",1);f.set("synthCutoff",300);f.set("synthModEnvDecay",300);f.set("synthMod3Source",float(modEnv2));f.set("synthMod3Target",float(targetCutoff));f.set("synthMod3Amount",80);
    const auto sweep=render(f,48000,silence(),hold({45})).l;
    const double early=magnitude(sweep,480,2400,1760)/magnitude(sweep,480,2400,110),late=magnitude(sweep,36000,48000,1760)/magnitude(sweep,36000,48000,110);
    std::cout<<"MEASURE LFO 1 -> Pitch A 10 %: +-"<<depth<<" st (2.4); LFO 2 1/4 @ 120 BPM -> Amp: "<<syncHz<<" Hz (2); Env 2 -> Cutoff: 1760 Hz re 110 Hz "<<db(early)<<" dB early, "<<db(late)<<" dB late\n";
    require(std::abs(depth-2.4)<.15 && std::abs(syncHz-2)<.05 && db(early)-db(late)>20,"LFO or modulation matrix wrong");
    std::cout<<"PASS LFO rate and tempo sync, matrix routing to pitch, amp and cutoff\n";
}

void fmAndVoices() {
    // FM: B (sine, octave +1, level 0) into A (sine): sidebands at 220 +- 440 Hz.
    Host h;clean(h,0);h.set("synthOscBOn",1);h.set("synthOscBWave",0);h.set("synthOscBOctave",1);h.set("synthOscBLevel",0);
    const auto none=render(h,48000,silence(),hold({57})).l;h.set("synthFM",25);const auto fm=render(h,48000,silence(),hold({57})).l;
    const double before=db(magnitude(none,12000,48000,660)/magnitude(none,12000,48000,220)),after=db(magnitude(fm,12000,48000,660)/magnitude(fm,12000,48000,220));
    // Polyphony: three notes, three peaks; five notes into four voices steal the oldest.
    Host p;clean(p,0);p.set("synthVoices",4);
    SynthLayer chord;const auto three=render(p,48000,silence(),hold({57,61,64}),&chord).l;
    const double m1=magnitude(three,12000,48000,220),m2=magnitude(three,12000,48000,hz(61)),m3=magnitude(three,12000,48000,hz(64));
    SynthLayer stolen;render(p,12000,silence(),hold({57,59,61,62,64}),&stolen);
    // Mono legato glide (time constant 100 ms) from A3 to A4.
    Host g;clean(g,0);g.set("synthVoices",1);g.set("synthGlide",100);
    const auto glide=render(g,96000,silence(),[](int k,juce::MidiBuffer& m){if(k==0)m.addEvent(juce::MidiMessage::noteOn(1,57,1.f),0);if(k==94)m.addEvent(juce::MidiMessage::noteOn(1,69,1.f),0);}).l;
    const int t=94*block;const double soon=peakNear(glide,t+240,t+1200,300),settled=peakNear(glide,t+48000,t+67200,440);   // glide is a time constant: 1 s is 10 of them
    std::cout<<"MEASURE FM 25 %: 660 Hz sideband "<<before<<" -> "<<after<<" dB re the carrier; chord peaks "<<db(m1)<<" / "<<db(m2)<<" / "<<db(m3)<<" dBFS; 5 notes into 4 voices: "<<stolen.soundingVoices()
             <<" sounding; glide 100 ms: "<<soon<<" Hz in the first 25 ms, "<<settled<<" Hz settled\n";
    require(before<-60 && after>-25 && std::abs(db(m1)-db(m2))<.5 && std::abs(db(m1)-db(m3))<.5 && stolen.soundingVoices()==4 && soon>225 && soon<420 && std::abs(settled-440)<.5,"FM, polyphony or glide wrong");
    std::cout<<"PASS FM B to A, polyphony and voice stealing, mono legato glide\n";
}

void guitarAndMidi() {
    const auto quiet=[](int n){return float(.00316*std::sin(2*pi*110*n/rate));};
    const auto loud=[](int n){return float(.1*std::sin(2*pi*110*n/rate));};
    Host h;clean(h,0);h.set("synthSource",1);
    const double under=rms(render(h,24000,quiet,noMidi()).l,12000,24000);const auto played=render(h,48000,loud,noMidi()).l;
    const double over=rms(played,12000,24000),guitarPitch=peakNear(played,24000,48000,110);
    h.set("synthSource",0);const auto both=render(h,48000,loud,hold({69})).l;const double midiWins=peakNear(both,24000,48000,440),guitarAbsent=magnitude(both,24000,48000,110);
    std::cout<<"MEASURE guitar: -50 dBFS under a -40 dBFS threshold "<<db(under)<<" dBFS, -20 dBFS plays "<<db(over)<<" dBFS at "<<guitarPitch<<" Hz; guitar + held MIDI A4: "<<midiWins<<" Hz, 110 Hz at "<<db(guitarAbsent)<<" dBFS\n";
    require(under==0 && over>1e-3 && std::abs(guitarPitch-110)<.5 && std::abs(midiWins-440)<.5 && db(guitarAbsent)<-60,"Guitar tracking threshold or MIDI precedence wrong");
    std::cout<<"PASS guitar tracking threshold and pitch, a held MIDI note wins\n";
}

void contractAndPatches() {
    Host h;SynthLayer s;s.bind(h.state);s.prepare(rate,block);juce::AudioBuffer<float> in(2,block),o(2,block);juce::MidiBuffer m;m.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
    for(int n=0;n<block;++n)for(int c=0;c<2;++c){in.setSample(c,n,.3f);o.setSample(c,n,.1f);}
    s.handleMidi(m);s.analyse(in);s.render(block,120);for(int i=0;i<4;++i)s.addAt(SynthInsert(i),o);
    bool same=true;for(int n=0;n<block;++n)same=same&&o.getSample(0,n)==.1f&&o.getSample(1,n)==.1f;
    require(same && !s.sounding(),"SYNTH off changed the signal");
    // Everything at once: eight voices, unison 7 on all three oscillators, all eight slots, FM, character, drive.
    double peak=0;bool finite=true;allocations=0;
    const auto patches=synthPatches();   // one copy: the loops below refer into it
    for(int patch=0;patch<int(patches.size());++patch){
        Host x;for(const auto& [id,v]:patches[size_t(patch)].values)x.set(id.toRawUTF8(),v);
        x.set("synthOn",1);x.set("synthSource",0);x.set("synthVoices",8);x.set("synthOscAUnison",7);x.set("synthOscBOn",1);x.set("synthOscBUnison",7);x.set("synthOscCOn",1);x.set("synthOscCUnison",7);x.set("synthCharacter",100);x.set("synthDrive",100);x.set("synthResonance",100);
        for(int slot=0;slot<synthModSlots;++slot){x.set(synthModID(slot,"Source").toRawUTF8(),float(1+slot%7));x.set(synthModID(slot,"Target").toRawUTF8(),float(1+(slot*5)%(synthModTargetCount-1)));x.set(synthModID(slot,"Amount").toRawUTF8(),slot%2?100.f:-100.f);}
        const auto out=render(x,48000,[](int n){return pluck(82.407,n%24000);},hold({48,52,55,60,64,67,72,76}));
        for(size_t n=0;n<out.l.size();++n){finite=finite&&std::isfinite(out.l[n])&&std::isfinite(out.r[n]);peak=std::max({peak,double(std::abs(out.l[n])),double(std::abs(out.r[n]))});}
    }
    std::cout<<"MEASURE every patch at full load (8 voices x 3 x 7 unison, 8 slots, character, drive, resonance 100 %): peak "<<peak<<", allocations "<<allocations<<"\n";
    require(finite && peak<=2.0 && allocations==0,"SYNTH unbounded or allocating at full load");
    // The factory patches sound.
    std::string line;
    for(int patch=1;patch<int(patches.size());++patch){Host x;for(const auto& [id,v]:patches[size_t(patch)].values)x.set(id.toRawUTF8(),v);x.set("synthOn",1);x.set("synthSource",2);
        const double level=rms(render(x,24000,silence(),hold({57})).l,2400,12000);line+=std::string(" ")+patches[size_t(patch)].name+" "+juce::String(db(level),1).toStdString()+" dBFS;";require(level>1e-3,"A factory patch is silent");}
    std::cout<<"MEASURE factory patches, A3:"<<line<<"\n";
    std::cout<<"PASS off contract, full load bounded and allocation-free, factory patches sound\n";
}

// Frequency of the strongest component near `near`, per window, from zero crossings of a pure tone.
std::vector<double> crossingHz(const std::vector<float>& x,int from,int to,int window){
    std::vector<double> result;
    for(int n=from;n+window<=to;n+=window){double first=-1,last=-1;int c=0;
        for(int i=n+1;i<n+window;++i)if(x[size_t(i-1)]<0&&x[size_t(i)]>=0){const double t=i-1+x[size_t(i-1)]/(x[size_t(i-1)]-x[size_t(i)]);if(first<0)first=t;last=t;++c;}
        if(c>2)result.push_back((c-1)*rate/(last-first));}
    return result;
}
void oscillatorC() {
    // C alone at +7 semitones; then A + B + C at three pitches, each level its own.
    Host h;clean(h,0);h.set("synthOscALevel",0);h.set("synthOscCOn",1);h.set("synthOscCWave",0);h.set("synthOscCSemi",7);h.set("synthOscCLevel",100);
    const double c=peakNear(render(h,48000,silence(),hold({57})).l,12000,48000,hz(64));
    Host m;clean(m,0);m.set("synthOscALevel",100);m.set("synthOscBOn",1);m.set("synthOscBWave",0);m.set("synthOscBOctave",1);m.set("synthOscBLevel",50);
    m.set("synthOscCOn",1);m.set("synthOscCWave",0);m.set("synthOscCOctave",2);m.set("synthOscCLevel",25);
    const auto mix=render(m,48000,silence(),hold({57})).l;
    const double a=magnitude(mix,12000,48000,220),b=magnitude(mix,12000,48000,440),cc=magnitude(mix,12000,48000,880);
    // Level C through the matrix: the wheel is 0, so only a fixed source works; use velocity 1 at -100 %.
    m.set("synthMod1Source",float(modVelocity));m.set("synthMod1Target",float(targetLevelC));m.set("synthMod1Amount",-100);
    const double muted=magnitude(render(m,48000,silence(),hold({57})).l,12000,48000,880);
    std::cout<<"MEASURE osc C +7 st: "<<c<<" Hz ("<<hz(64)<<"); A / B / C at 100 / 50 / 25 %: "<<db(a)<<" / "<<db(b)<<" / "<<db(cc)<<" dBFS; velocity -> Level C -100 %: C at "<<db(muted)<<" dBFS\n";
    require(std::abs(c/hz(64)-1)<.0005 && std::abs(db(b/a)-db(.5))<.1 && std::abs(db(cc/a)-db(.25))<.1 && db(muted)<-100,"Oscillator C tuning, mixing or modulation wrong");
    std::cout<<"PASS third oscillator: tuning, mixing with A and B, its matrix target\n";
}

void engineTypes() {
    // Character 0: the three types are the same plain voice, sample for sample.
    std::array<std::vector<float>,3> plain;
    for(int t=0;t<3;++t){Host h;clean(h,2);h.set("synthFilterOn",1);h.set("synthType",float(t));plain[size_t(t)]=render(h,24000,silence(),hold({57},.6f)).l;}
    require(plain[0]==plain[1] && plain[0]==plain[2],"Engine types differ at character 0");
    // Dvergr Analog: pitch drift of a sine over 4 s in 50 ms windows; a quiet voice keeps its level.
    Host a;clean(a,0);a.set("synthType",float(synthAnalog));a.set("synthCharacter",100);
    const auto drifting=render(a,int(rate*4),silence(),hold({81})).l;const auto f=crossingHz(drifting,4800,int(rate*4),2400);
    double mean=0;for(double v:f)mean+=v;mean/=double(f.size());double spread=0;for(double v:f)spread+=std::pow(1200*std::log2(v/mean),2);spread=std::sqrt(spread/double(f.size()));
    Host q0,q1;clean(q0,2);clean(q1,2);for(Host* q:{&q0,&q1})q->set("synthOscALevel",5);q1.set("synthType",float(synthAnalog));q1.set("synthCharacter",100);
    const double quietChange=db(rms(render(q1,24000,silence(),hold({57})).l,4800,24000)/rms(render(q0,24000,silence(),hold({57})).l,4800,24000));
    // Seidr Dynamic: velocity 0.2 against 1.0 into a 600 Hz low-pass. The waves move towards
    // square, so brightness is read at an odd harmonic.
    const auto dynamicAt=[](float velocity){Host h;clean(h,2);h.set("synthFilterOn",1);h.set("synthFilterType",0);h.set("synthCutoff",600);h.set("synthType",float(synthDynamic));h.set("synthCharacter",100);
        const auto out=render(h,24000,silence(),hold({45},velocity)).l;return std::pair<double,double>{rms(out,4800,24000),magnitude(out,4800,24000,1650)/magnitude(out,4800,24000,110)};};
    const auto soft=dynamicAt(.2f),hard=dynamicAt(1.f);
    // The guitar's level does the same: -30 against -12 dBFS input.
    const auto guitarAt=[](double amplitude){Host h;clean(h,2);h.set("synthSource",1);h.set("synthFilterOn",1);h.set("synthFilterType",0);h.set("synthCutoff",600);h.set("synthType",float(synthDynamic));h.set("synthCharacter",100);
        const auto out=render(h,48000,[amplitude](int n){return float(amplitude*std::sin(2*pi*110*n/rate));},noMidi()).l;return magnitude(out,24000,48000,1650)/magnitude(out,24000,48000,110);};
    const double guitarSoft=guitarAt(.0316),guitarHard=guitarAt(.25);
    // Mimir Impact: osc A sine alone; the internal modulator at twice A puts a 660 Hz sideband on each attack.
    Host m;clean(m,0);m.set("synthType",float(synthImpact));m.set("synthCharacter",100);
    const auto struck=render(m,48000,silence(),hold({57})).l;
    const double early=db(magnitude(struck,0,960,660)/magnitude(struck,0,960,220)),late=db(magnitude(struck,24000,48000,660)/magnitude(struck,24000,48000,220));
    Host k;clean(k,0);k.set("synthOscALevel",0);k.set("synthType",float(synthImpact));k.set("synthCharacter",100);
    const auto click=render(k,24000,silence(),hold({57})).l;const double clickEarly=rms(click,0,240),clickLate=rms(click,4800,24000);
    std::cout<<"MEASURE Dvergr Analog 100 %: drift "<<spread<<" cents RMS over 4 s, level at osc 5 % "<<quietChange<<" dB; Seidr Dynamic 100 %: velocity 0.2 -> 1.0 level "<<db(soft.first)<<" -> "<<db(hard.first)
             <<" dBFS, 1650 Hz (odd harmonic 15) re 110 Hz "<<db(soft.second)<<" -> "<<db(hard.second)<<" dB; guitar -30 -> -12 dBFS: "<<db(guitarSoft)<<" -> "<<db(guitarHard)<<" dB; Mimir Impact 100 %: 660 Hz sideband "<<early<<" dB in the first 20 ms, "
             <<late<<" dB after 0.5 s; click "<<db(clickEarly)<<" dBFS in 5 ms, "<<db(clickLate)<<" dBFS after 100 ms\n";
    require(spread>2 && spread<10 && std::abs(quietChange)<.1,"Dvergr Analog drift or small-signal level wrong");
    require(hard.first>soft.first*2 && db(hard.second)-db(soft.second)>10 && db(guitarHard)-db(guitarSoft)>10,"Seidr Dynamic does not follow the playing level");
    require(early>-15 && late<-50 && clickEarly>1e-2 && clickLate<1e-6,"Mimir Impact strike or click wrong");
    std::cout<<"PASS three engine types: plain at character 0, analog drift, dynamic response, struck FM and click\n";
}

// A test sample: harmonics 1-6 at 1/k of `hz`, `seconds` long, at `sampleRate`.
std::unique_ptr<SynthSample> tone(double hz,double seconds,double sampleRate,double rightHz=0){
    juce::AudioBuffer<float> b(rightHz>0?2:1,int(seconds*sampleRate));
    for(int c=0;c<b.getNumChannels();++c)for(int n=0;n<b.getNumSamples();++n){double v=0;const double f=c==0?hz:rightHz;for(int k=1;k<=(c==0?6:1);++k)v+=std::sin(2*pi*k*f*n/sampleRate)/k;b.setSample(c,n,float(.4*v));}
    return synthSample::make(b,sampleRate,"tone");
}
void sampleOscillator() {
    // Root detection from the audio.
    const auto c4=tone(261.63,1.0,44100),g3=tone(196.0,1.0,44100);
    require(c4 && g3,"Sample not built");
    const int rootC=synthSample::rootNote(*c4),rootG=synthSample::rootNote(*g3);
    // Playback: root C4 at 44.1 kHz, A4 played: 440 Hz.
    const auto play=[&](std::unique_ptr<SynthSample> sample,std::vector<std::pair<const char*,float>> set,int note,int samples){
        Host h;clean(h,0);h.set("synthOscAMode",1);h.set("synthOscARoot",60);for(const auto& [id,v]:set)h.set(id,v);
        SynthLayer s;s.swapSample(0,std::move(sample));return render(h,samples,silence(),hold({note}),&s);};
    const auto a4=play(tone(261.63,1.0,44100),{},69,int(rate*2)).l;
    const double played=peakNear(a4,24000,48000,440),looped=rms(a4,int(rate*1.5),int(rate*2));
    // One-shot, an octave up: a 1 s sample ends after 0.5 s. Start 50 %: after 0.25 s.
    const auto once=play(tone(261.63,1.0,44100),{{"synthOscALoop",0}},72,int(rate)).l;
    const auto half=play(tone(261.63,1.0,44100),{{"synthOscALoop",0},{"synthOscAStart",50}},72,int(rate)).l;
    const auto lastSound=[](const std::vector<float>& x){int last=0;for(size_t n=0;n<x.size();++n)if(std::abs(x[n])>1e-6f)last=int(n);return last/rate;};
    // Stereo: left 440 Hz, right 660 Hz, played at the root.
    const auto stereo=play(tone(440,1.0,48000,660),{},60,48000);
    const double leftOnly=db(magnitude(stereo.l,12000,48000,660)/magnitude(stereo.l,12000,48000,440)),rightOnly=db(magnitude(stereo.r,12000,48000,440)/magnitude(stereo.r,12000,48000,660));
    // No sample loaded in Sample mode: silent.
    Host e;clean(e,0);e.set("synthOscAMode",1);const double empty=rms(render(e,12000,silence(),hold({57})).l,0,12000);
    std::cout<<"MEASURE sample: detected roots "<<synthNoteName(rootC)<<" ("<<c4->detectedHz<<" Hz), "<<synthNoteName(rootG)<<" ("<<g3->detectedHz<<" Hz); C4 sample at A4 "<<played<<" Hz (440), looped at 1.5-2 s "<<db(looped)
             <<" dBFS; one-shot +1 oct ends at "<<lastSound(once)<<" s (0.5), from 50 % at "<<lastSound(half)<<" s (0.25); stereo L 660 Hz "<<leftOnly<<" dB, R 440 Hz "<<rightOnly<<" dB; empty slot "<<db(empty)<<" dBFS\n";
    require(rootC==60 && rootG==55 && std::abs(played-440)<.5 && looped>1e-2,"Sample root detection, tuning or loop wrong");
    require(std::abs(lastSound(once)-.5)<.01 && std::abs(lastSound(half)-.25)<.01,"Sample one-shot length or start wrong");
    require(leftOnly<-60 && rightOnly<-60 && empty==0,"Stereo sample channels mixed, or an empty slot sounds");
    std::cout<<"PASS sample oscillator: root detection, pitch, loop, one-shot, start, stereo, empty slot\n";
}

void sampleState() {
    // Load from a file through the processor, save, restore into a new one.
    const auto file=juce::File::createTempFile(".wav");
    {juce::AudioBuffer<float> b(2,24000);for(int n=0;n<24000;++n){b.setSample(0,n,float(.5*std::sin(2*pi*196.0*n/48000)));b.setSample(1,n,float(.25*std::sin(2*pi*196.0*n/48000)));}
     juce::WavAudioFormat wav;auto stream=file.createOutputStream();require(stream!=nullptr,"Cannot write the test WAV");
     std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(stream.get(),48000,2,24,{},0));require(w!=nullptr,"No WAV writer");stream.release();w->writeFromAudioSampleBuffer(b,0,24000);}
    auto p=std::make_unique<ChimeraProcessor>();
    require(p->loadSynthSample(1,file).wasOk(),"Sample did not load");
    const auto raw=[](ChimeraProcessor& q,const char* id){return q.parameters().getRawParameterValue(id)->load();};
    require(raw(*p,"synthOscBOn")>.5f && raw(*p,"synthOscBMode")>.5f && juce::roundToInt(raw(*p,"synthOscBRoot"))==55,"Loading did not switch osc B to its sample at the detected root");
    juce::MemoryBlock saved;p->getStateInformation(saved);
    auto q=std::make_unique<ChimeraProcessor>();q->setStateInformation(saved.getData(),int(saved.getSize()));
    const auto* a=p->synthEngine().sample(1);const auto* b=q->synthEngine().sample(1);
    require(a && b && a->length==b->length && b->stereo() && b->name==file.getFileNameWithoutExtension(),"Sample not restored");
    double worst=0;for(int c=0;c<2;++c)for(int n=0;n<a->length;++n)worst=std::max(worst,double(std::abs(a->data[size_t(c)][size_t(n)]-b->data[size_t(c)][size_t(n)])));
    // A project without samples clears them.
    auto r=std::make_unique<ChimeraProcessor>();juce::MemoryBlock none;r->getStateInformation(none);q->setStateInformation(none.getData(),int(none.getSize()));
    std::cout<<"MEASURE sample state: "<<saved.getSize()/1024<<" KiB with a 0.5 s stereo sample, largest round-trip error "<<db(worst)<<" dBFS (24-bit FLAC)\n";
    require(worst<1e-6 && q->synthEngine().sample(1)==nullptr && juce::roundToInt(raw(*q,"synthOscBRoot"))==60,"Sample round trip inexact, or a sample survived a project without one");
    p->clearSynthSample(1);require(p->synthEngine().sample(1)==nullptr && raw(*p,"synthOscBMode")<.5f,"Clear did not remove the sample");
    file.deleteFile();
    std::cout<<"PASS samples load through the processor, persist in the project (24-bit FLAC) and clear\n";
}

struct Writes final : juce::AudioProcessorListener {
    std::map<int,int> changes;int gestures{};
    void audioProcessorParameterChanged(juce::AudioProcessor*,int index,float) override {++changes[index];}
    void audioProcessorChanged(juce::AudioProcessor*,const juce::AudioProcessorListener::ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int) override {++gestures;}
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int) override {++gestures;}
};
void processorInsertsAndPage() {
    // Each insert point: silent input + MIDI A3 -> the synth reaches the output.
    std::array<std::vector<float>,4> outputs;std::string line;
    for(int insert=0;insert<4;++insert){
        auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(rate,block);p->prepareToPlay(rate,block);
        auto set=[&](const char* id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
        set("synthOn",1);set("synthSource",2);set("synthInsert",float(insert));
        juce::AudioBuffer<float> b(2,block);
        for(int k=0;k<80;++k){juce::MidiBuffer midi;if(k==2)midi.addEvent(juce::MidiMessage::noteOn(1,57,1.f),0);b.clear();p->processBlock(b,midi);for(int n=0;n<block;++n)outputs[size_t(insert)].push_back(b.getSample(0,n));}
        const auto path=spectralforge::signalPath::read(*p);static constexpr std::array<const char*,4> joins{"split","tone.eq","post.section0","utilities"};
        bool edge=false;for(const auto& e:path.edges)edge|=e.from=="synth"&&e.to==joins[size_t(insert)];
        require(edge,"The signal path does not show the synth at its insert point");
        line+=" "+synthInsertNames()[insert].toStdString()+" "+juce::String(db(rms(outputs[size_t(insert)],8000,20480)),1).toStdString()+" dBFS (latency "+std::to_string(p->getLatencySamples())+");";
        require(rms(outputs[size_t(insert)],8000,20480)>1e-3,"The synth does not reach the output from an insert point");
    }
    double ampDiffers=0;for(size_t n=8000;n<20480;++n)ampDiffers=std::max(ampDiffers,double(std::abs(outputs[0][n]-outputs[2][n])));
    std::cout<<"MEASURE inserts, silent input + MIDI A3:"<<line<<" amp input vs before POST differ by up to "<<ampDiffers<<"\n";
    require(ampDiffers>1e-3,"Amp input does not pass the synth through the amps");
    // The page: attached controls, graph drags as single gestures, patches.
    auto p=std::make_unique<ChimeraProcessor>();Writes writes;p->addListener(&writes);
    spectralforge::synthUI::SynthPage page(*p);page.setSize(1140,438);
    const auto index=[&](const char* id){return p->parameters().getParameter(id)->getParameterIndex();};
    const auto raw=[&](const char* id){return p->parameters().getRawParameterValue(id)->load();};
    for(const auto* id:{"synthOn","synthSource","synthInsert","synthOscAWave","synthCutoff","synthAmpAttack","synthLfo1Shape","synthMod1Source","synthMod1Amount","synthLevel"})
        require(page.control(id)!=nullptr,std::string("SYNTH page control missing: ")+id);
    require(writes.changes.empty(),"Opening the SYNTH page wrote a parameter");
    auto& curve=page.filterCurve();
    const auto drag=[&](juce::Component& c,juce::Point<float> from,juce::Point<float> to){
        const auto source=juce::Desktop::getInstance().getMainMouseSource();const auto now=juce::Time::getCurrentTime();
        const auto event=[&](juce::Point<float> at,bool down){return juce::MouseEvent(source,at,juce::ModifierKeys(down?juce::ModifierKeys::leftButtonModifier:0),1,0,0,0,0,&c,&c,now,from,now,1,false);};
        c.mouseDown(event(from,true));c.mouseDrag(event(to,true));c.mouseUp(event(to,false));};
    writes.gestures=0;writes.changes.clear();
    drag(curve,{100,40},{float(curve.getWidth())*.75f,10});
    const double expectedCutoff=spectralforge::synthUI::FilterCurve::hzForX(float(curve.getWidth())*.75f,float(curve.getWidth()));
    std::cout<<"MEASURE filter-curve drag: cutoff "<<raw("synthCutoff")<<" Hz (pointer "<<expectedCutoff<<" Hz), resonance "<<raw("synthResonance")<<" %, gestures "<<writes.gestures<<"\n";
    require(std::abs(raw("synthCutoff")/expectedCutoff-1)<.02 && raw("synthResonance")>80 && writes.gestures==4 && writes.changes.size()==2,"Filter-curve drag is not one gesture per parameter or misplaced");
    writes.gestures=0;writes.changes.clear();
    auto& env=page.envelopeGraph(0);const auto a=env.attackPoint();drag(env,a,{a.x+30,a.y});
    require(raw("synthAmpAttack")>5 && writes.changes.size()==1 && writes.changes.count(index("synthAmpAttack")) && writes.gestures==2,"Envelope attack drag wrong");
    writes.changes.clear();page.applyPatch(1);
    require(juce::roundToInt(raw("synthSubOn"))==1 && std::abs(raw("synthCutoff")-1200)<1 && raw("synthOn")<.5f,"Patch did not apply (or switched the synth on)");
    page.applyPatch(0);require(std::abs(raw("synthCutoff")-6000)<1 && raw("synthSubOn")<.5f,"Init patch did not restore the defaults");
    // Three oscillators, engine type buttons, wave picking, Wave / Sample controls.
    for(const auto* id:{"synthOscCWave","synthOscCMode1","synthOscCLoad","synthOscCRoot","synthType2","synthCharacter","synthOscBShapes","synthOscCDisplay"})
        require(page.control(id)!=nullptr,std::string("SYNTH page control missing: ")+id);
    writes.gestures=0;writes.changes.clear();
    static_cast<juce::Button*>(page.control("synthType2"))->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    require(juce::roundToInt(raw("synthType"))==2 && writes.gestures==2 && writes.changes.size()==1,"Type button is not one gesture");
    auto& shapes=page.waveShapes(1);const auto squareTile=shapes.tile(3).getCentre().toFloat();
    writes.gestures=0;writes.changes.clear();drag(shapes,squareTile,squareTile);
    const float squareValue=raw("synthOscBWave");
    require(squareValue==3.f && writes.gestures==2 && writes.changes.size()==1,"Shape tile did not select square in one gesture");
    auto& display=page.waveDisplay(1);
    writes.gestures=0;writes.changes.clear();
    drag(display,{float(display.getWidth())*.3f,10.f},{float(display.getWidth())*.5f,10.f});   // above the triangle tile, then the saw tile
    const float morphed=raw("synthOscBWave");
    require(std::abs(morphed-2.f)<.02f && writes.gestures==2,"Display drag does not morph to the shape below the pointer in one gesture");
    require(page.control("synthOscBShapes")->isVisible() && !page.control("synthOscBLoad")->isVisible(),"Wave mode shows sample controls");
    static_cast<juce::Button*>(page.control("synthOscBMode1"))->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(20);page.updateModes();
    require(!page.control("synthOscBShapes")->isVisible() && page.control("synthOscBLoad")->isVisible() && page.control("synthOscBStart")->isVisible(),"Sample mode does not show its controls");
    writes.changes.clear();page.applyPatch(3);
    require(juce::roundToInt(raw("synthType"))==synthImpact && raw("synthOscBMode")<.5f && writes.changes.count(index("synthOscBRoot"))==0,"Mimir Impact patch: type, mode or kept root wrong");
    // Optional evidence: SYNTH_PAGE_SNAPSHOT=<directory> writes the page, with a sample in osc B.
    if(const char* dir=std::getenv("SYNTH_PAGE_SNAPSHOT")){
        const auto wav=juce::File::createTempFile(".wav");
        {juce::AudioBuffer<float> b(1,48000);for(int n=0;n<48000;++n)b.setSample(0,n,float(.6*std::exp(-n/12000.0)*std::sin(2*pi*196.0*n/48000)*(1+.3*std::sin(2*pi*3.0*n/48000))));
         juce::WavAudioFormat format;auto stream=wav.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> w(format.createWriterFor(stream.get(),48000,1,24,{},0));stream.release();w->writeFromAudioSampleBuffer(b,0,48000);}
        require(p->loadSynthSample(1,wav).wasOk(),"Snapshot sample did not load");wav.deleteFile();
        page.updateModes();page.resized();
        for(const float scale:{1.f,1.5f}){
            juce::Image image(juce::Image::ARGB,int(page.getWidth()*scale),int(page.getHeight()*scale),true);
            {juce::Graphics g(image);g.fillAll(juce::Colour(0xff111616));g.drawImageAt(page.createComponentSnapshot(page.getLocalBounds(),true,scale),0,0);}   // on the editor's dark background
            auto out=juce::File(dir).getChildFile("synth_page_"+juce::String(int(scale*100))+".png");out.deleteFile();
            auto stream=out.createOutputStream();require(stream && juce::PNGImageFormat().writeImageToStream(image,*stream),"Cannot write the SYNTH page snapshot");
        }
    }
    std::cout<<"MEASURE page: square tile -> wave "<<squareValue<<", display drag to the saw tile -> "<<morphed<<"\n";
    p->removeListener(&writes);
    std::cout<<"PASS inserts in the processor and the path, SYNTH page controls, graph drags as host gestures, patches\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {tracker();tuningTest();waveMorph();unisonSubNoise();filterResponse();envelopes();lfoAndMatrix();fmAndVoices();guitarAndMidi();oscillatorC();engineTypes();sampleOscillator();sampleState();contractAndPatches();processorInsertsAndPage();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
