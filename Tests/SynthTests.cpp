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
#include "SynthWavetableIO.h"
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
#include <thread>
#include <atomic>
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
    static juce::AudioProcessorValueTreeState::ParameterLayout layout(){juce::AudioProcessorValueTreeState::ParameterLayout l;appendSynthParameters(l);appendSynthWavetableParameters(l,synthOscCount);appendSynthFiltersParameters(l);appendSynthSequencerParameters(l);appendSynthEffectsParameters(l);appendSynthTableImportParameters(l,synthOscCount);return l;}
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

void filtersAndDistortion() {
    // docs/SYNTH_FILTERS.md. Off (the defaults), whatever else is set: the released voice, sample for sample.
    {Host a;a.set("synthOn",1);a.set("synthSource",2);a.set("synthDrive",40);a.set("synthFilterEnv",50);
     Host b;b.set("synthOn",1);b.set("synthSource",2);b.set("synthDrive",40);b.set("synthFilterEnv",50);
     for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{{"synthFilter2Type",6},{"synthFilter2Cutoff",900},{"synthFilter2Resonance",80},{"synthFilter2KeyTrack",100},{"synthFilter2Env",-60},
         {"synthFilterRouting",1},{"synthFilterBalance",10},{"synthDistType",2},{"synthDistPlace",2},{"synthDistDrive",100},{"synthDistMix",40}})b.set(id,v);
     const auto x=render(a,48000,silence(),hold({45,57})).l,y=render(b,48000,silence(),hold({45,57})).l;
     double diff=0;for(size_t n=0;n<x.size();++n)diff=std::max(diff,double(std::abs(x[n]-y[n])));
     std::cout<<"MEASURE filter 2 and distortion off, their settings changed: largest sample difference "<<diff<<"\n";
     require(diff==0,"The voice changed with filter 2 and the distortion off");}
    // Filter 2 alone: each type against the curve the page draws (a 55 Hz saw at 96 kHz worth of samples).
    Host plain;clean(plain,2);const auto dry=render(plain,96000,silence(),hold({33})).l;
    const auto relative=[&](const std::vector<float>& wet,double f){return db(magnitude(wet,24000,96000,f)/magnitude(dry,24000,96000,f));};
    const auto with=[&](std::initializer_list<std::pair<const char*,float>> values){Host h;clean(h,2);for(const auto& [id,v]:values)h.set(id,v);return render(h,96000,silence(),hold({33})).l;};
    {double worst=0;std::string line;
     for(int type=0;type<synthFilter2TypeCount;++type){
        const float cutoff=type>=synthFilterCombPlus?480.f:1000.f;   // 480 Hz: a comb of exactly 100 samples
        const auto wet=with({{"synthFilter2On",1},{"synthFilter2Type",float(type)},{"synthFilter2Cutoff",cutoff},{"synthFilter2Resonance",30}});double typeWorst=0;
        for(int k:{2,5,9,18,36,72}){const double f=55.0*k,expected=db(synthFilter2Magnitude(type,cutoff,.3,f,rate));if(expected>-40)typeWorst=std::max(typeWorst,std::abs(relative(wet,f)-expected));}
        worst=std::max(worst,typeWorst);line+=" "+synthFilter2TypeNames()[type].toStdString()+" "+juce::String(typeWorst,3).toStdString()+" dB;";}
     std::cout<<"MEASURE filter 2 vs analytic response, resonance 30 %, largest error at harmonics 110 Hz-4 kHz:"<<line<<"\n";
     require(worst<.3,"Filter 2 differs from the curve the page draws");}
    // Serial: the two responses multiply. Parallel: the balance ends are one filter alone.
    {const auto serial=with({{"synthFilterOn",1},{"synthFilterType",0},{"synthCutoff",2000},{"synthResonance",30},{"synthFilter2On",1},{"synthFilter2Type",2},{"synthFilter2Cutoff",300},{"synthFilter2Resonance",30}});
     double worst=0;for(int k:{2,5,9,18,36,72}){const double f=55.0*k;worst=std::max(worst,std::abs(relative(serial,f)-db(synthFilterMagnitude(0,2000,.3,f,rate))-db(synthFilter2Magnitude(2,300,.3,f,rate))));}
     const auto one=with({{"synthFilterOn",1},{"synthFilterType",0},{"synthCutoff",2000}}),two=with({{"synthFilter2On",1},{"synthFilter2Type",2},{"synthFilter2Cutoff",300}});
     const auto left=with({{"synthFilterOn",1},{"synthFilterType",0},{"synthCutoff",2000},{"synthFilter2On",1},{"synthFilter2Type",2},{"synthFilter2Cutoff",300},{"synthFilterRouting",1},{"synthFilterBalance",0}});
     const auto right=with({{"synthFilterOn",1},{"synthFilterType",0},{"synthCutoff",2000},{"synthFilter2On",1},{"synthFilter2Type",2},{"synthFilter2Cutoff",300},{"synthFilterRouting",1},{"synthFilterBalance",100}});
     const auto centre=with({{"synthFilterOn",1},{"synthFilterType",0},{"synthCutoff",2000},{"synthFilter2On",1},{"synthFilter2Type",2},{"synthFilter2Cutoff",300},{"synthFilterRouting",1}});
     double d1=0,d2=0,d3=0;for(size_t n=0;n<one.size();++n){d1=std::max(d1,double(std::abs(left[n]-one[n])));d2=std::max(d2,double(std::abs(right[n]-two[n])));d3=std::max(d3,double(std::abs(centre[n]-(one[n]+two[n]))));}
     std::cout<<"MEASURE routing: serial vs the product of both curves "<<worst<<" dB; parallel balance 0 vs filter 1 alone "<<d1<<", 100 vs filter 2 alone "<<d2<<", 50 vs their sum "<<d3<<"\n";
     require(worst<.3 && d1<1e-6 && d2<1e-6 && d3<1e-5,"Filter routing wrong");}
    // Comb with KEY 100 %: tuned at C4, it follows the note (A3: peaks at 220 Hz and its multiples).
    {const auto comb=[&](float key){Host h;clean(h,2);h.set("synthNoiseOn",1);h.set("synthNoiseLevel",100);h.set("synthNoiseColor",100);h.set("synthOscAOn",0);
        h.set("synthFilter2On",1);h.set("synthFilter2Type",6);h.set("synthFilter2Cutoff",262);h.set("synthFilter2Resonance",90);h.set("synthFilter2KeyTrack",key);return render(h,96000,silence(),hold({57})).l;};
     const auto fixed=comb(0),tracked=comb(100);
     const auto band=[&](const std::vector<float>& x,double f){double e=0;for(double d=-6;d<=6;d+=1.5)e+=std::pow(magnitude(x,24000,96000,f+d),2);return 10*std::log10(e);};
     const double trackedPeak=band(tracked,220)-band(tracked,330),fixedPeak=band(fixed,262)-band(fixed,393);
     std::cout<<"MEASURE comb on noise, resonance 90 %: tuned 262 Hz, peak over the dip a fifth above "<<fixedPeak<<" dB; with KEY 100 % at A3, 220 Hz over 330 Hz "<<trackedPeak<<" dB\n";
     require(fixedPeak>15 && trackedPeak>15,"The comb does not ring at its cutoff, or does not follow the key");}
    // Distortion. Every shape stays inside +-1 (Bit crush: inside the input's peak plus half a step).
    {double bound=0;for(int type=0;type<synthDistTypeCount;++type)for(const double drive:{0.0,.3,1.0})for(double x=-4;x<=4;x+=.001){
        const double y=synthDistort(type,synthDistGain(drive),synthDistSteps(drive),x);require(std::isfinite(y),"Distortion output not finite");
        bound=std::max(bound,type==5?std::abs(y)-std::abs(x)-.5/synthDistSteps(drive):std::abs(y)-1.0);}
     require(bound<1e-9,"A distortion shape leaves its bound");}
    {const auto sine=[&](std::initializer_list<std::pair<const char*,float>> values,std::initializer_list<int> notes={57}){Host h;clean(h,0);for(const auto& [id,v]:values)h.set(id,v);return render(h,48000,silence(),hold(notes)).l;};
     const auto off=sine({}),unity=sine({{"synthDistOn",1},{"synthDistType",1},{"synthDistDrive",0}}),dry=sine({{"synthDistOn",1},{"synthDistType",2},{"synthDistDrive",100},{"synthDistMix",0}});
     double d1=0,d2=0;for(size_t n=0;n<off.size();++n){d1=std::max(d1,double(std::abs(off[n]-unity[n])));d2=std::max(d2,double(std::abs(off[n]-dry[n])));}
     std::string line;
     for(int type=0;type<synthDistTypeCount;++type){const auto wet=sine({{"synthDistOn",1},{"synthDistType",float(type)},{"synthDistDrive",70}});
        line+=" "+synthDistTypeNames()[type].toStdString()+" h2 "+juce::String(harmonicDb(wet,220,2),1).toStdString()+" h3 "+juce::String(harmonicDb(wet,220,3),1).toStdString()+";";
        require(std::max(harmonicDb(wet,220,2),harmonicDb(wet,220,3))>-40 || type==5,std::string("A distortion type adds no harmonics: ")+synthDistTypeNames()[type].toStdString());}
     const auto mean=[](const std::vector<float>& x){double m=0;for(int n=24000;n<48000;++n)m+=x[size_t(n)];return m/24000;};
     const auto tube=sine({{"synthDistOn",1},{"synthDistType",3},{"synthDistDrive",70}}),crush=sine({{"synthDistOn",1},{"synthDistType",5},{"synthDistDrive",100}});
     // Per voice: a fifth through the hard clipper has no 2 f1 - f2 product (110.4 Hz), which a clipper after the mix would add.
     const double f1=hz(57),f2=hz(64);const auto chord=sine({{"synthDistOn",1},{"synthDistType",1},{"synthDistDrive",70},{"synthVoices",4}},{57,64});
     const double intermod=db(magnitude(chord,12000,48000,2*f1-f2)/magnitude(chord,12000,48000,f1));
     // Position: before a low-pass the filter removes the new harmonics; after it they stay.
     const auto before=sine({{"synthDistOn",1},{"synthDistType",1},{"synthDistDrive",70},{"synthDistPlace",0},{"synthFilterOn",1},{"synthFilterType",1},{"synthCutoff",400},{"synthResonance",0}});
     const auto after=sine({{"synthDistOn",1},{"synthDistType",1},{"synthDistDrive",70},{"synthDistPlace",2},{"synthFilterOn",1},{"synthFilterType",1},{"synthCutoff",400},{"synthResonance",0}});
     std::cout<<"MEASURE distortion on a sine at A3, drive 70 %, harmonics 2 / 3 (dB re 1):"<<line<<"\n";
     std::cout<<"MEASURE distortion: hard clip at drive 0 % differs from off by "<<d1<<", mix 0 % by "<<d2<<"; Tube harmonic 2 "<<harmonicDb(tube,220,2)<<" dB, DC "<<mean(tube)<<"; Bit crush at 100 % harmonic 3 "<<harmonicDb(crush,220,3)
              <<" dB; A3 + E4 hard clipped, 2 f1 - f2 "<<intermod<<" dB re f1; harmonic 9 before / after a 400 Hz low-pass "<<harmonicDb(before,220,9)<<" / "<<harmonicDb(after,220,9)<<" dB\n";
     require(d1==0 && d2==0,"Distortion at unity or mix 0 is not clean");
     require(harmonicDb(tube,220,2)>-20 && std::abs(mean(tube))<1e-3 && harmonicDb(crush,220,3)>-40,"Tube adds no even harmonic or leaves DC, or Bit crush does nothing");
     require(intermod<-60,"The distortion is not per voice");
     require(harmonicDb(after,220,9)>harmonicDb(before,220,9)+30,"The distortion position does not change the order");}
    std::cout<<"PASS filter 2 (eight types), serial / parallel routing, comb key tracking, per-voice distortion\n";
}

void sequencerAndMacros() {
    // docs/SYNTH_SEQUENCER.md. Steps and macros set but connected to nothing: the voice does not change.
    const auto largest=[](const std::vector<float>& a,const std::vector<float>& b){double d=0;for(size_t n=0;n<a.size();++n)d=std::max(d,double(std::abs(a[n]-b[n])));return d;};
    {Host a;clean(a,2);Host b;clean(b,2);for(int k=0;k<synthSeqMaxSteps;++k)b.set(synthSeqStepID(k).toRawUTF8(),float(k%2?-80:60));
     for(int k=0;k<synthMacroCount;++k)b.set(synthMacroID(k).toRawUTF8(),70);b.set("synthSeqSmooth",50);b.set("synthSeqSteps",5);b.set("synthSeqRate",2);
     const double d=largest(render(a,48000,silence(),hold({57})).l,render(b,48000,silence(),hold({57})).l);
     std::cout<<"MEASURE sequencer and macros set, no row connected: largest sample difference "<<d<<"\n";require(d==0,"An unconnected sequencer or macro changed the voice");}
    // Step seq -> Amp at 100 %: step 1 is 0 (full level), step 2 is -100 % (silence). 1/16 at 120 BPM is 6000 samples a step.
    const auto gate=[&](std::initializer_list<std::pair<const char*,float>> values,double tempo=120,int noteBlock=0){
        Host h;clean(h,0);h.set("synthModB1Source",float(modSeq));h.set("synthModB1Target",float(targetAmp));h.set("synthModB1Amount",100);h.set("synthSeqStep2",-100);h.set("synthSeqSteps",2);
        for(const auto& [id,v]:values)h.set(id,v);
        return render(h,48000,silence(),[noteBlock](int k,juce::MidiBuffer& m){if(k==noteBlock)m.addEvent(juce::MidiMessage::noteOn(1,69,1.f),0);},nullptr,tempo).l;};
    {const auto x=gate({});const double on1=rms(x,1000,5000),off=rms(x,7000,11000),on2=rms(x,13000,17000);
     const auto slow=gate({},60);const double slowOn=rms(slow,7000,11000),slowOff=rms(slow,13000,23000);
     const auto three=gate({{"synthSeqSteps",3},{"synthSeqStep3",-100}});const double t1=rms(three,13000,17000),t2=rms(three,19000,23000);
     const auto fast=gate({{"synthSeqRate",5}});const double f1=rms(fast,3500,5500),f2=rms(fast,6500,8500);
     std::cout<<"MEASURE step seq to Amp (steps 0 / -100 %), 1/16 at 120 BPM: step 1 "<<db(on1)<<" dBFS, step 2 "<<db(off)<<", step 1 again "<<db(on2)<<"; at 60 BPM 7000-11000 "<<db(slowOn)<<", 13000-23000 "<<db(slowOff)
              <<"; three steps (0 / -100 / -100): step 3 "<<db(t1)<<", step 1 "<<db(t2)<<"; 1/32: step 2 "<<db(f1)<<", step 1 "<<db(f2)<<"\n";
     require(on1>.05 && off<1e-6 && std::abs(on2/on1-1)<.01,"The sequencer does not step at 1/16 of the tempo");
     require(slowOn>.05 && slowOff<1e-6,"The sequencer does not follow the tempo");
     require(t1<1e-6 && t2>.05 && f1<1e-6 && f2>.05,"The step count or the step length is wrong");}
    // SMOOTH: at 100 % the level glides instead of jumping. RETRIG off: a late note joins the running pattern.
    {const auto smooth=gate({{"synthSeqSmooth",100}});const double early=rms(smooth,6100,6600),late=rms(smooth,11000,11900),full=rms(smooth,1000,5000);
     const auto retrig=gate({},120,30),running=gate({{"synthSeqRetrigger",0}},120,30);   // the note starts 7680 samples in, inside step 2
     const double r=rms(retrig,8200,11200),free=rms(running,8200,11200),freeLater=rms(running,12500,17500);
     std::cout<<"MEASURE smooth 100 %: just after the step change "<<db(early/full)<<" dB re step 1, at the end of step 2 "<<db(late/full)<<" dB; a note 160 ms in: RETRIG on "<<db(r)<<" dBFS, off "<<db(free)<<" then "<<db(freeLater)<<"\n";
     require(early/full>.5 && late/full<.1 && late>1e-5,"SMOOTH does not glide between steps");
     require(r>.05 && free<1e-6 && freeLater>.05,"RETRIG does not decide whether a new phrase restarts the pattern");}
    // Macros and the newer targets: a macro at 100 % with an amount equals the control set there by hand.
    {const auto run=[&](std::initializer_list<std::pair<const char*,float>> values){Host h;clean(h,2);for(const auto& [id,v]:values)h.set(id,v);return render(h,48000,silence(),hold({45})).l;};
     const auto row=[](int target,float amount){return std::vector<std::pair<const char*,float>>{{"synthMacro1",100},{"synthModB2Source",float(modMacro1)},{"synthModB2Target",float(target)},{"synthModB2Amount",amount}};};
     const auto both=[&](std::initializer_list<std::pair<const char*,float>> base,std::initializer_list<std::pair<const char*,float>> moved,int target,float amount,std::initializer_list<std::pair<const char*,float>> manual){
        Host a;clean(a,2);for(const auto& [id,v]:base)a.set(id,v);for(const auto& [id,v]:moved)a.set(id,v);for(const auto& [id,v]:row(target,amount))a.set(id,v);
        Host b;clean(b,2);for(const auto& [id,v]:base)b.set(id,v);for(const auto& [id,v]:manual)b.set(id,v);
        return largest(render(a,48000,silence(),hold({45})).l,render(b,48000,silence(),hold({45})).l);};
     const double cutoff2=both({{"synthFilter2On",1},{"synthFilter2Type",1}},{{"synthFilter2Cutoff",300}},targetCutoff2,50,{{"synthFilter2Cutoff",2400}});   // 6 octaves x 50 % = 3 octaves
     const double res2=both({{"synthFilter2On",1},{"synthFilter2Type",1},{"synthFilter2Cutoff",800}},{{"synthFilter2Resonance",10}},targetResonance2,60,{{"synthFilter2Resonance",70}});
     const double drive=both({{"synthDistOn",1},{"synthDistType",2}},{{"synthDistDrive",10}},targetDistDrive,70,{{"synthDistDrive",80}});
     const double mix=both({{"synthDistOn",1},{"synthDistType",1},{"synthDistDrive",80}},{{"synthDistMix",0}},targetDistMix,100,{{"synthDistMix",100}});
     const double warp=both({{"synthOscATable",1},{"synthOscAWarp",float(synthWarpSync)}},{{"synthOscAWarpAmount",0}},targetWarpA,100,{{"synthOscAWarpAmount",100}});
     const double balance=both({{"synthFilterOn",1},{"synthFilter2On",1},{"synthFilterRouting",1}},{{"synthFilterBalance",50}},targetFilterBalance,50,{{"synthFilterBalance",100}});
     (void)run;
     std::cout<<"MEASURE Macro 1 at 100 % against the control set by hand, largest sample difference: Cutoff 2 "<<cutoff2<<", Resonance 2 "<<res2<<", Dist drive "<<drive<<", Dist mix "<<mix<<", Warp A "<<warp<<", Filter balance "<<balance<<"\n";
     require(cutoff2<1e-4 && res2<1e-4 && drive<1e-4 && mix<1e-6 && warp<1e-6 && balance<1e-6,"A macro does not move a newer target by its amount");
     // A released source on a second-bank row is the same as on a released row.
     Host a;clean(a,2);a.set("synthFilterOn",1);a.set("synthCutoff",300);a.set("synthMod3Source",float(modEnv2));a.set("synthMod3Target",float(targetCutoff));a.set("synthMod3Amount",80);
     Host b;clean(b,2);b.set("synthFilterOn",1);b.set("synthCutoff",300);b.set("synthModB5Source",float(modEnv2));b.set("synthModB5Target",float(targetCutoff));b.set("synthModB5Amount",80);
     const double same=largest(render(a,48000,silence(),hold({45})).l,render(b,48000,silence(),hold({45})).l);
     std::cout<<"MEASURE Env 2 -> Cutoff on row 3 against row 13: largest sample difference "<<same<<"\n";require(same==0,"The two row banks differ");}
    // A macro moved while a note sounds is smoothed: no step larger than the wave's own.
    {Host h;clean(h,0);h.set("synthModB1Source",float(modMacro1));h.set("synthModB1Target",float(targetAmp));h.set("synthModB1Amount",-100);
     SynthLayer s;s.bind(h.state);s.prepare(rate,block);std::vector<float> out;juce::AudioBuffer<float> in(2,block),o(2,block);in.clear();
     for(int k=0;k<40;++k){juce::MidiBuffer m;if(k==0)m.addEvent(juce::MidiMessage::noteOn(1,57,1.f),0);if(k==20)h.set("synthMacro1",100);o.clear();s.handleMidi(m);s.analyse(in);s.render(block,120);s.addAt(SynthInsert::beforePost,o);for(int n=0;n<block;++n)out.push_back(o.getSample(0,n));}
     double jump=0;for(size_t n=1;n<out.size();++n)jump=std::max(jump,double(std::abs(out[n]-out[n-1])));
     std::cout<<"MEASURE Macro 1 -> Amp -100 %, macro jumped from 0 to 100 % in a note: largest sample step "<<jump<<", level after "<<db(rms(out,9000,10000))<<" dBFS\n";
     require(jump<.03 && rms(out,9000,10000)<1e-4,"A macro change clicks or does not arrive");}
    std::cout<<"PASS step sequencer (tempo, length, smooth, retrigger), macros, second matrix bank and its targets\n";
}

std::vector<double> envelope(const std::vector<float>& x,int frame){std::vector<double> e;for(size_t n=0;n+size_t(frame)<=x.size();n+=size_t(frame))e.push_back(rms(x,int(n),int(n)+frame)*std::sqrt(2.0));return e;}
void effects() {
    // docs/SYNTH_EFFECTS.md. Every effect set but off: the rack is not run and the output is the released one.
    const auto largest=[](const std::vector<float>& a,const std::vector<float>& b){double d=0;for(size_t n=0;n<a.size();++n)d=std::max(d,double(std::abs(a[n]-b[n])));return d;};
    using Values=std::vector<std::pair<const char*,float>>;
    // A sine note of `blocks` blocks (0: held), 5 ms release.
    const auto play=[](const Values& values,int note=57,int blocks=0,int samples=48000,double tempo=120,std::function<void(Host&,int)> change={}){
        Host h;clean(h,0);h.set("synthAmpRelease",5);for(const auto& [id,v]:values)h.set(id,v);
        return render(h,samples,silence(),[&,note,blocks](int k,juce::MidiBuffer& m){if(change)change(h,k);if(k==0)m.addEvent(juce::MidiMessage::noteOn(1,note,1.f),0);if(blocks>0 && k==blocks)m.addEvent(juce::MidiMessage::noteOff(1,note),0);},nullptr,tempo);};
    {const auto dry=play({});
     const auto set=play({{"synthFxChorusRate",2},{"synthFxChorusDepth",100},{"synthFxChorusMix",100},{"synthFxPhaserFeedback",90},{"synthFxDelayTime",100},{"synthFxDelayFeedback",90},{"synthFxDelayMix",100},{"synthFxDelayPingPong",1},
                          {"synthFxReverbDecay",8},{"synthFxReverbMix",100},{"synthFxEqLow",12},{"synthFxEqHigh",-12}});
     const double d=std::max(largest(dry.l,set.l),largest(dry.r,set.r));
     // On at 0 % mix (the EQ: on at 0 dB) the effect runs and still passes the sound unchanged.
     const auto zero=play({{"synthFxChorusOn",1},{"synthFxChorusMix",0},{"synthFxPhaserOn",1},{"synthFxPhaserMix",0},{"synthFxDelayOn",1},{"synthFxDelayMix",0},{"synthFxReverbOn",1},{"synthFxReverbMix",0}});
     const auto flat=play({{"synthFxEqOn",1}});
     const double z=largest(dry.l,zero.l),e=largest(dry.l,flat.l);
     std::cout<<"MEASURE effects set but off: largest sample difference "<<d<<"; chorus, phaser, delay, reverb on at 0 % mix "<<z<<"; EQ on at 0 dB "<<e<<"\n";
     require(d==0,"An effect that is off changed the synth");require(z==0 && e<1e-6,"An effect at 0 % mix or a flat EQ changed the synth");}
    // Delay. A 21 ms note; Free 250 ms is 12000 samples. Mix 50 %: the first repeat is as loud as the note, each later one lower by the feedback.
    {const int len=4;const auto burst=[](const std::vector<float>& x,int at){return rms(x,at+520,at+1000);};   // after the 10 ms the mix takes to arrive at the start
     const auto x=play({{"synthFxDelayOn",1},{"synthFxDelaySync",0},{"synthFxDelayTime",250},{"synthFxDelayFeedback",50},{"synthFxDelayMix",50}},57,len,48000);
     const double note=burst(x.l,0),first=burst(x.l,12000),second=burst(x.l,24000),third=burst(x.l,36000),between=rms(x.l,6000,11000);
     const auto synced=play({{"synthFxDelayOn",1},{"synthFxDelaySync",4},{"synthFxDelayFeedback",0},{"synthFxDelayMix",50}},57,len,48000,120);
     const auto slow=play({{"synthFxDelayOn",1},{"synthFxDelaySync",4},{"synthFxDelayFeedback",0},{"synthFxDelayMix",50}},57,len,48000,60);
     const auto dotted=play({{"synthFxDelayOn",1},{"synthFxDelaySync",5},{"synthFxDelayFeedback",0},{"synthFxDelayMix",50}},57,len,48000,120);
     const auto capped=play({{"synthFxDelayOn",1},{"synthFxDelaySync",1},{"synthFxDelayFeedback",0},{"synthFxDelayMix",50}},57,len,120000,30);   // 1/2 at 30 BPM is 4 s: held at 2 s
     std::cout<<"MEASURE delay Free 250 ms, feedback 50 %, mix 50 %: note "<<db(note)<<" dBFS, repeats "<<db(first/note)<<" / "<<db(second/note)<<" / "<<db(third/note)<<" dB re the note, between "<<db(between)<<" dBFS; 1/8 at 120 BPM repeat at 12000 "
              <<db(burst(synced.l,12000)/note)<<" dB, at 60 BPM at 24000 "<<db(burst(slow.l,24000)/note)<<" dB (at 12000 "<<db(burst(slow.l,12000))<<" dBFS); 1/8 D at 18000 "<<db(burst(dotted.l,18000)/note)<<" dB; 1/2 at 30 BPM at 96000 "<<db(burst(capped.l,96000)/note)<<" dB\n";
     require(note>.02 && std::abs(db(first/note))<.5 && std::abs(db(second/first)+6.02)<.5 && std::abs(db(third/second)+6.02)<.5 && between<1e-5,"The delay's repeats are at the wrong time or level");
     require(std::abs(db(burst(synced.l,12000)/note))<.5 && std::abs(db(burst(slow.l,24000)/note))<.5 && burst(slow.l,12000)<1e-5 && std::abs(db(burst(dotted.l,18000)/note))<.5,"The delay does not follow the tempo");
     require(std::abs(db(burst(capped.l,96000)/note))<.5,"The delay is not held at 2 s");
     // Ping-pong: the first repeat on the left only, the second on the right only.
     const auto pp=play({{"synthFxDelayOn",1},{"synthFxDelaySync",0},{"synthFxDelayTime",250},{"synthFxDelayFeedback",50},{"synthFxDelayMix",50},{"synthFxDelayPingPong",1}},57,len,48000);
     std::cout<<"MEASURE ping-pong: repeat 1 left "<<db(burst(pp.l,12000)/note)<<" dB, right "<<db(burst(pp.r,12000))<<" dBFS; repeat 2 left "<<db(burst(pp.l,24000))<<" dBFS, right "<<db(burst(pp.r,24000)/note)<<" dB\n";
     require(std::abs(db(burst(pp.l,12000)/note))<.5 && burst(pp.r,12000)<1e-5 && burst(pp.l,24000)<1e-5 && std::abs(db(burst(pp.r,24000)/note)+6.02)<.5,"Ping-pong does not alternate sides");
     // Switched on inside a held note: no click (a 110 Hz sine's own largest step is its level x 0.0144).
     const auto on=play({{"synthFxDelaySync",0},{"synthFxDelayTime",100},{"synthFxDelayMix",50}},45,0,48000,120,[](Host& h,int k){if(k==60)h.set("synthFxDelayOn",1);});
     double step=0,own=0;for(int n=4000;n<12000;++n)own=std::max(own,double(std::abs(on.l[size_t(n)]-on.l[size_t(n-1)])));for(int n=15000;n<17000;++n)step=std::max(step,double(std::abs(on.l[size_t(n)]-on.l[size_t(n-1)])));
     // Switched off and on again later: nothing of the old repeats comes back.
     const auto again=play({{"synthFxDelayOn",1},{"synthFxDelaySync",0},{"synthFxDelayTime",500},{"synthFxDelayFeedback",90},{"synthFxDelayMix",50}},57,len,96000,120,[](Host& h,int k){if(k==20)h.set("synthFxDelayOn",0);if(k==60)h.set("synthFxDelayOn",1);});
     std::cout<<"MEASURE delay switched on inside a note: largest step between samples "<<step<<" (the note's own "<<own<<"); off at 0.1 s and on at 0.3 s after a note with 500 ms repeats: later output "<<db(rms(again.l,16000,96000))<<" dBFS\n";
     require(step<1.2*own,"Switching the delay on clicks");require(rms(again.l,16000,96000)<1e-6,"An effect switched on again replays its old memory");}
    // Reverb, mix 100 %. A held note: the tail's level is near the note's. After the note: it falls 60 dB in about DECAY.
    {const auto dryHeld=play({},57,0,96000);
     const auto held=play({{"synthFxReverbOn",1},{"synthFxReverbDecay",2},{"synthFxReverbDamp",0},{"synthFxReverbMix",100}},57,0,96000);
     const double level=db(rms(held.l,48000,96000)/rms(dryHeld.l,48000,96000));
     double levelLow=1e9,levelHigh=-1e9,levelMean=0;int levels=0;
     for(int note=33;note<=96;note+=3){const double v=db(rms(play({{"synthFxReverbOn",1},{"synthFxReverbDecay",2},{"synthFxReverbDamp",0},{"synthFxReverbMix",100}},note,0,96000).l,48000,96000)/rms(play({},note,0,96000).l,48000,96000));
         levelLow=std::min(levelLow,v);levelHigh=std::max(levelHigh,v);levelMean+=v;++levels;}
     levelMean/=levels;
     std::cout<<"MEASURE reverb level, held notes A1 to C7 in minor thirds, mix 100 %, re the dry note: mean "<<levelMean<<" dB, lowest "<<levelLow<<" dB, highest "<<levelHigh<<" dB\n";
     const auto slope=[&](float decay,float damp,int note){const auto x=play({{"synthFxReverbOn",1},{"synthFxReverbDecay",decay},{"synthFxReverbDamp",damp},{"synthFxReverbMix",100}},note,40,int(48000*(decay+.5)));
         const int a=int(10240+.15*decay*48000),b=int(10240+.65*decay*48000);return 2.0*(db(rms(x.l,a,a+4800))-db(rms(x.l,b,b+4800)));};   // dB lost over one DECAY
     const double one=slope(1,0,57),four=slope(4,0,57),bright=slope(2,0,93),damped=slope(2,100,93);
     const auto wide=play({{"synthFxReverbOn",1},{"synthFxReverbDecay",2},{"synthFxReverbMix",100}},57,40,48000);
     double same=0,power=0;for(int n=12000;n<48000;++n){same+=double(wide.l[size_t(n)])*wide.r[size_t(n)];power+=.5*(double(wide.l[size_t(n)])*wide.l[size_t(n)]+double(wide.r[size_t(n)])*wide.r[size_t(n)]);}
     std::cout<<"MEASURE reverb, mix 100 %: held A3, tail level "<<level<<" dB re the dry note; fall over one DECAY: 1 s "<<one<<" dB, 4 s "<<four<<" dB; A6 at 2 s, damping 0 % "<<bright<<" dB, 100 % "<<damped<<" dB; left-right correlation of the tail "<<same/power<<"\n";
     require(std::abs(levelMean)<4 && levelHigh<10,"The reverb's level is far from the note's");
     require(std::abs(one-60)<12 && std::abs(four-60)<12,"The reverb does not decay in the time set");
     require(damped>bright+20,"Damping does not shorten the treble");require(std::abs(same/power)<.5,"The reverb's tail is not stereo");}
    // Chorus, mix 100 %, depth 100 %, 1 Hz: the delay sweeps 6 ms each way, so the pitch moves by 3.8 % (65 cents) each way.
    {const auto x=play({{"synthFxChorusOn",1},{"synthFxChorusRate",1},{"synthFxChorusDepth",100},{"synthFxChorusMix",100}},69,0,96000);
     const double down=peakNear(x.l,48000-2400,48000+2400,440),up=peakNear(x.l,72000-2400,72000+2400,440),right=peakNear(x.r,48000-2400,48000+2400,440);
     const auto still=play({{"synthFxChorusOn",1},{"synthFxChorusDepth",0},{"synthFxChorusMix",100}},69,0,48000);const auto dry=play({},69,0,48000);
     double shifted=0;for(int n=2000;n<48000;++n)shifted=std::max(shifted,double(std::abs(still.l[size_t(n)]-dry.l[size_t(n-576)])));
     std::cout<<"MEASURE chorus 1 Hz, depth 100 %, mix 100 %, A4: left at 1.0 s "<<down<<" Hz, at 1.5 s "<<up<<" Hz; right at 1.0 s "<<right<<" Hz; depth 0 %: against the dry note 12 ms earlier, largest difference "<<shifted<<"\n";
     require(std::abs(down-423.7)<3 && std::abs(up-456.3)<3,"The chorus sweep is not 6 ms at the rate set");require(std::abs(right-440)<3,"The chorus's right channel is not a quarter cycle behind");
     require(shifted<1e-4,"The chorus's centre delay is not 12 ms");}
    // Phaser, depth 0 %, no feedback, mix 50 %: six stages centred on 800 Hz cancel there and pass at 440 Hz.
    {const auto level=[&](int note,bool on){const auto x=play(on?Values{{"synthFxPhaserOn",1},{"synthFxPhaserDepth",0},{"synthFxPhaserFeedback",0},{"synthFxPhaserMix",50}}:Values{},note,0,48000);return rms(x.l,24000,48000);};
     const double notch=db(level(79,true)/level(79,false)),pass=db(level(69,true)/level(69,false));
     const auto moving=play({{"synthFxPhaserOn",1},{"synthFxPhaserRate",1},{"synthFxPhaserDepth",100},{"synthFxPhaserFeedback",0},{"synthFxPhaserMix",50}},79,0,96000);
     double lo=1e9,hi=0;for(int n=48000;n+2400<=96000;n+=2400){const double v=rms(moving.l,n,n+2400);lo=std::min(lo,v);hi=std::max(hi,v);}
     std::cout<<"MEASURE phaser at rest (depth 0 %): G5 784 Hz "<<notch<<" dB, A4 440 Hz "<<pass<<" dB; swept at 1 Hz, G5 level over a second: "<<db(hi/lo)<<" dB between lowest and highest\n";
     require(notch<-18 && std::abs(pass)<.5,"The phaser's notches are not where they should be");require(db(hi/lo)>12,"The phaser does not sweep");}
    // EQ against its own curve.
    {double worst=0;
     for(const auto& [low,high]:{std::pair<float,float>{12,0},{-12,0},{0,12},{0,-12},{6,-6}})for(const int note:{33,57,81,105}){
         const auto on=play({{"synthLevel",-24},{"synthFxEqOn",1},{"synthFxEqLow",low},{"synthFxEqHigh",high}},note,0,48000),off=play({{"synthLevel",-24}},note,0,48000);   // low enough to stay under the safety stage at +12 dB
         const double measured=db(magnitude(on.l,24000,48000,hz(note))/magnitude(off.l,24000,48000,hz(note))),expected=db(synthFxEqMagnitude(low,high,hz(note),rate));worst=std::max(worst,std::abs(measured-expected));}
     std::cout<<"MEASURE EQ, five settings x A1 / A3 / A5 / A7: largest error against the shelf curves "<<worst<<" dB; the curve at +12 / +12 dB: 55 Hz "<<db(synthFxEqMagnitude(12,12,55,rate))<<" dB, 880 Hz "<<db(synthFxEqMagnitude(12,12,880,rate))<<" dB, 12 kHz "<<db(synthFxEqMagnitude(12,12,12000,rate))<<" dB\n";
     require(worst<.1,"The EQ does not follow its curve");require(std::abs(db(synthFxEqMagnitude(12,12,55,rate))-12)<1 && std::abs(db(synthFxEqMagnitude(12,12,12000,rate))-12)<1,"The shelves do not reach their gain");}
    // Everything on at once, full load: bounded, finite, no allocation on the audio thread.
    {Host h;clean(h,2);for(const auto& [id,v]:Values{{"synthVoices",8},{"synthFxChorusOn",1},{"synthFxPhaserOn",1},{"synthFxPhaserFeedback",90},{"synthFxDelayOn",1},{"synthFxDelayFeedback",95},{"synthFxDelayMix",100},{"synthFxDelayPingPong",1},
            {"synthFxReverbOn",1},{"synthFxReverbDecay",12},{"synthFxReverbMix",100},{"synthFxEqOn",1},{"synthFxEqLow",12},{"synthFxEqHigh",12},{"synthLevel",6}})h.set(id,v);
     allocations=0;const auto x=render(h,96000,silence(),hold({40,47,52,56,59,64,68,71}));
     double peak=0;bool finite=true;for(const float v:x.l){finite&=std::isfinite(v);peak=std::max(peak,double(std::abs(v)));}
     std::cout<<"MEASURE every effect on, eight voices, +6 dB: peak "<<peak<<", allocations "<<allocations<<"\n";
     require(finite && peak<=2.0 && allocations==0,"The effect rack is unbounded or allocates");}
}
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

// Review fixes (10/30): leaving Sample mode mid-note does not read a frame
// position as a wave phase; a sample that arrives after the attack starts at
// START; peaks above full scale survive the 24-bit FLAC; the FLAC is cached.
void reviewFixes() {
    // Sample -> Wave while a note sounds (saw, filter on).
    double spike=0,steady=0;
    {
        Host h;clean(h,1);h.set("synthFilterOn",1);h.set("synthOscAMode",1);h.set("synthOscARoot",60);
        SynthLayer s;s.bind(h.state);s.prepare(rate,block);s.swapSample(0,tone(261.63,1.0,44100));
        juce::AudioBuffer<float> in(2,block),o(2,block);in.clear();juce::MidiBuffer m;
        for(int k=0;k<200;++k){
            m.clear();if(k==0)m.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            if(k==100)h.set("synthOscAMode",0);
            o.clear();s.handleMidi(m);s.analyse(in);s.render(block,120);s.addAt(SynthInsert::beforePost,o);
            const double peak=o.getMagnitude(0,0,block);
            if(k>=100)spike=std::max(spike,peak);else if(k>=20)steady=std::max(steady,peak);
        }
    }
    // A sample loaded 0.1 s into a held note, START 50 %, one-shot at its root: it ends 0.5 s later.
    double ended=0;
    {
        Host h;clean(h,0);h.set("synthOscAMode",1);h.set("synthOscARoot",60);h.set("synthOscALoop",0);h.set("synthOscAStart",50);
        SynthLayer s;s.bind(h.state);s.prepare(rate,block);
        juce::AudioBuffer<float> in(2,block),o(2,block);in.clear();juce::MidiBuffer m;
        const int loadAt=int(.1*rate/block);int last=0;
        for(int k=0;k*block<int(rate*1.2);++k){
            m.clear();if(k==0)m.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            if(k==loadAt)s.swapSample(0,tone(261.63,1.0,48000));
            o.clear();s.handleMidi(m);s.analyse(in);s.render(block,120);s.addAt(SynthInsert::beforePost,o);
            for(int n=0;n<block;++n)if(std::abs(o.getSample(0,n))>1e-6f)last=k*block+n;
        }
        ended=last/rate-loadAt*block/rate;
    }
    // A float sample peaking at 2.0 round-trips unclipped; saving twice reuses the encoding.
    double clipError=0;bool cached=false;
    {
        juce::AudioBuffer<float> b(1,24000);for(int n=0;n<24000;++n)b.setSample(0,n,float(2.0*std::sin(2*pi*220.0*n/48000)));
        SynthLayer s;s.swapSample(0,synthSample::make(b,48000,"hot"));
        const auto first=synthSample::save(s);const void* data=s.sample(0)->encoded.getData();
        const auto second=synthSample::save(s);cached=s.sample(0)->encoded.getData()==data && first.isEquivalentTo(second);
        auto loaded=synthSample::restore(second);
        require(loaded[0]!=nullptr,"Hot sample not restored");
        for(int n=0;n<24000;++n)clipError=std::max(clipError,double(std::abs(loaded[0]->data[0][size_t(n)]-s.sample(0)->data[0][size_t(n)])));
    }
    std::cout<<"MEASURE review fixes: Sample -> Wave peak "<<spike<<" (playing "<<steady<<"); late sample from 50 % ends after "<<ended<<" s (0.5); peak-2.0 round trip error "<<clipError<<"\n";
    require(spike<2*steady+.1,"Leaving Sample mode mid-note spiked");
    require(std::abs(ended-.5)<.02,"A late sample did not start at START");
    require(clipError<1e-5 && cached,"Sample above full scale clipped, or its encoding not cached");
    // A reference held while drawing survives a swap from another thread, and
    // saving on one thread while another swaps samples is safe.
    {
        SynthLayer s;s.swapSample(0,tone(220,.2,48000));
        const auto held=s.sharedSample(0);s.swapSample(0,nullptr);
        require(held!=nullptr && held->length==int(.2*48000) && s.sample(0)==nullptr,"A held sample did not survive its replacement");
        std::atomic<bool> done{false};std::atomic<int> saves{0};
        std::thread saver([&]{while(!done.load()){const auto tree=synthSample::save(s);(void)tree;++saves;}});
        for(int k=0;k<60;++k)s.swapSample(k%3,k%4==3?nullptr:tone(110+k,.05,48000));
        done=true;saver.join();
        require(saves.load()>0,"The saving thread did not run");
    }
    std::cout<<"PASS Sample -> Wave without a spike, late sample from START, unclipped hot samples, cached encoding, samples shared safely across threads\n";
}

struct Writes final : juce::AudioProcessorListener {
    std::map<int,int> changes;int gestures{};
    void audioProcessorParameterChanged(juce::AudioProcessor*,int index,float) override {++changes[index];}
    void audioProcessorChanged(juce::AudioProcessor*,const juce::AudioProcessorListener::ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int) override {++gestures;}
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int) override {++gestures;}
};
// Wavetable oscillators (docs/SYNTH_WAVETABLE.md).
void wavetables() {
    const auto run=[](int table,float wave,int note=57,int warp=0,float amount=0){Host h;clean(h,wave);h.set("synthOscATable",float(table));h.set("synthOscAWarp",float(warp));h.set("synthOscAWarpAmount",amount);return render(h,48000,silence(),hold({note})).l;};
    // Classic shapes ignore the warp: the released oscillator, sample for sample.
    {Host plain;clean(plain,2.5f);const auto a=render(plain,48000,silence(),hold({57})).l,b=run(0,2.5f,57,synthWarpSync,100);
     double diff=0;for(size_t n=0;n<a.size();++n)diff=std::max(diff,double(std::abs(a[n]-b[n])));
     std::cout<<"MEASURE classic shapes with a warp set: largest sample difference "<<diff<<"\n";require(diff==0,"Classic shapes changed with the wavetable parameters");}
    // Every frame of every table: full scale, no DC, and it sounds.
    {const auto& t=SynthWavetables::get();std::vector<float> cycle;
     for(int table=1;table<synthTableCount;++table)for(int f=0;f<synthTableFrames;++f){
        t.draw(table,double(f)/(synthTableFrames-1),cycle,2048);float peak=0;double mean=0;for(float x:cycle){peak=std::max(peak,std::abs(x));mean+=x;}mean/=cycle.size();
        require(peak>.9f && peak<1.01f && std::abs(mean)<.01,std::string("Frame not normalised or has DC: ")+synthTables[size_t(table)].name+" frame "+std::to_string(f));}}
    // Harmonics: a sine at the start, a saw at the end. Pulse starts as a square.
    const auto sweep0=run(1,0),sweep4=run(1,4),pulse0=run(2,0),middle=run(1,2);
    std::cout<<"MEASURE Harmonics harmonics 2 / 3 (dB re 1): start "<<harmonicDb(sweep0,220,2)<<" / "<<harmonicDb(sweep0,220,3)<<"; end "<<harmonicDb(sweep4,220,2)<<" / "<<harmonicDb(sweep4,220,3)
             <<" (-6.0 / -9.5); middle "<<harmonicDb(middle,220,2)<<" / "<<harmonicDb(middle,220,20)<<" (harmonic 20); Pulse start "<<harmonicDb(pulse0,220,2)<<" / "<<harmonicDb(pulse0,220,3)<<" (-inf / -9.5)\n";
    require(harmonicDb(sweep0,220,2)<-60 && harmonicDb(sweep0,220,3)<-60,"Harmonics does not start as a sine");
    require(std::abs(harmonicDb(sweep4,220,2)+6.02)<.3 && std::abs(harmonicDb(sweep4,220,3)+9.54)<.3,"Harmonics does not end as a saw");
    require(harmonicDb(middle,220,2)>-8 && harmonicDb(middle,220,20)<harmonicDb(sweep4,220,20)-20,"The wave position does not move through the frames");
    require(harmonicDb(pulse0,220,2)<-40 && std::abs(harmonicDb(pulse0,220,3)+9.54)<.3,"Pulse does not start as a square");
    // Band limit: C8 (4186 Hz) as a full saw. Harmonic 6 (25.1 kHz) would fold to 22.9 kHz, harmonic 7 to 18.7 kHz.
    {const double f=hz(108);const auto high=run(1,4,108);
     const double fold6=db(magnitude(high,12000,48000,rate-6*f)/magnitude(high,12000,48000,f)),fold7=db(magnitude(high,12000,48000,rate-7*f)/magnitude(high,12000,48000,f)),h4=harmonicDb(high,f,4);
     std::cout<<"MEASURE band limit at C8: harmonic 4 (16.7 kHz) "<<h4<<" dB (-12.0 for a saw), folded harmonic 6 "<<fold6<<" dB, folded harmonic 7 "<<fold7<<" dB re the fundamental\n";
     require(fold6<-70 && fold7<-70,"A wavetable aliases at C8");
     require(std::abs(h4+12.04)<.5,"A wavetable loses harmonics well below half the sample rate at C8");
     // A3 keeps the saw's harmonics to 16 kHz and beyond (harmonic 72 is 15.8 kHz, -37.1 dB for a saw).
     require(std::abs(harmonicDb(sweep4,220,60)-db(1.0/60))<.5,"A wavetable is dull at A3");}
    // Warp: 0 % is no warp; Sync at 100 % restarts a sine eight times per cycle.
    {const auto off=run(1,0),zero=run(1,0,57,synthWarpSync,0),sync=run(1,0,57,synthWarpSync,100);
     double diff=0;for(size_t n=0;n<off.size();++n)diff=std::max(diff,double(std::abs(off[n]-zero[n])));
     std::cout<<"MEASURE warp: 0 % differs from Off by "<<diff<<"; Sync 100 % on a sine, harmonic 8 "<<harmonicDb(sync,220,8)<<" dB re 1\n";
     require(diff==0 && harmonicDb(sync,220,8)>20,"Warp amount 0 is not neutral, or Sync does not multiply the cycle");
     for(int warp=1;warp<synthWarpCount;++warp)for(double ph=0;ph<1;ph+=.01){const double w=synthWarpPhase(warp,1.0,ph);require(w>=0 && w<=1.0 && synthWarpPhase(warp,0.0,ph)==ph,"A warp leaves the 0-1 phase range");}}
    // The matrix's Wave target moves the position, as it morphs the classic shapes.
    {Host h;clean(h,0);h.set("synthOscATable",1);h.set("synthMod1Source",4);h.set("synthMod1Target",4);h.set("synthMod1Amount",100);   // Velocity -> Wave A
     const auto moved=render(h,48000,silence(),hold({57})).l;std::cout<<"MEASURE position moved by the matrix (velocity to Wave A): harmonic 2 "<<harmonicDb(moved,220,2)<<" dB (unmodulated "<<harmonicDb(sweep0,220,2)<<")\n";
     require(harmonicDb(moved,220,2)>-10,"The matrix's Wave target does not move a wavetable's position");}
    std::cout<<"PASS wavetables: classic unchanged, frames normalised, position, band limit, warp\n";
}

// Wavetable import (docs/SYNTH_WAVETABLE_IMPORT.md): cutting a file into
// frames, playing the table, its band limit, refusals and the project state.
void wavetableImport() {
    namespace io=synthTableIO;
    const auto writeWav=[](const juce::File& file,const std::vector<float>& x,int bits=32){
        juce::AudioBuffer<float> b(1,int(x.size()));for(size_t n=0;n<x.size();++n)b.setSample(0,int(n),x[n]);
        juce::WavAudioFormat wav;auto stream=file.createOutputStream();require(stream!=nullptr,"Cannot write the test WAV");
        std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(stream.get(),48000,1,bits,{},0));require(w!=nullptr,"No WAV writer");stream.release();w->writeFromAudioSampleBuffer(b,0,b.getNumSamples());};
    // `frames` frames of `size` samples: frame k is harmonic k + 1 at half scale.
    const auto ladder=[](int frames,int size){std::vector<float> x(size_t(frames)*size_t(size));for(int k=0;k<frames;++k)for(int n=0;n<size;++n)x[size_t(k)*size_t(size)+size_t(n)]=float(.5*std::sin(2*pi*(k+1)*n/size));return x;};
    const auto play=[](std::shared_ptr<const SynthUserTable> table,float wave,int note,bool imported=true,int warp=0,float amount=0){
        Host h;clean(h,wave);h.set("synthOscAImported",imported?1.f:0.f);h.set("synthOscAWarp",float(warp));h.set("synthOscAWarpAmount",amount);
        SynthLayer s;s.swapTable(0,std::move(table));return render(h,48000,silence(),hold({note}),&s).l;};
    // Frames and position: four frames holding harmonics 1 to 4.
    std::unique_ptr<SynthUserTable> built;juce::String note;
    require(io::make(ladder(4,2048),2048,"Ladder",built,&note).wasOk() && built && built->frames==4 && built->sourceFrames==4 && built->sourceFrameSize==2048 && note.isEmpty(),"A four-frame table was not built");
    {float peak=0;double mean=0;for(int n=0;n<synthTableSize;++n){peak=std::max(peak,std::abs(built->frame(0,0)[n]));mean+=built->frame(0,0)[n];}
     require(std::abs(peak-1.f)<1e-3f && std::abs(mean/synthTableSize)<1e-4,"An imported table is not normalised or has DC");}
    const std::shared_ptr<const SynthUserTable> four(std::move(built));
    const auto first=play(four,0,57),last=play(four,4,57),between=play(four,2,57);
    const double h1First=db(magnitude(first,12000,48000,220)),h4First=db(magnitude(first,12000,48000,880)/magnitude(first,12000,48000,220)),h1Last=db(magnitude(last,12000,48000,220)/magnitude(last,12000,48000,880));
    const double mid2=db(magnitude(between,12000,48000,440)),mid3=db(magnitude(between,12000,48000,660)),mid1=db(magnitude(between,12000,48000,220));
    std::cout<<"MEASURE imported table, A3: first frame 220 Hz "<<h1First<<" dBFS, 880 Hz "<<h4First<<" dB re it; last frame 220 Hz "<<h1Last<<" dB re 880 Hz; position 50 %: 440 Hz "<<mid2<<", 660 Hz "<<mid3<<", 220 Hz "<<mid1<<" dBFS\n";
    require(h4First<-80 && h1Last<-80,"The first or last imported frame is not played alone at the ends of POSITION");
    require(std::abs(mid2-mid3)<.2 && mid1<mid2-80,"POSITION does not blend neighbouring imported frames");
    // The switch off, or on without a table: the oscillator as it was.
    {Host a;clean(a,2.5f);a.set("synthOscATable",3);const auto plain=render(a,24000,silence(),hold({57})).l;
     Host b;clean(b,2.5f);b.set("synthOscATable",3);b.set("synthOscAImported",1);const auto none=render(b,24000,silence(),hold({57})).l;
     Host c;clean(c,2.5f);c.set("synthOscATable",3);SynthLayer s;s.swapTable(0,four);const auto off=render(c,24000,silence(),hold({57}),&s).l;
     double d1=0,d2=0;for(size_t n=0;n<plain.size();++n){d1=std::max(d1,double(std::abs(plain[n]-none[n])));d2=std::max(d2,double(std::abs(plain[n]-off[n])));}
     std::cout<<"MEASURE imported switch on without a table: largest difference "<<d1<<"; table present, switch off: "<<d2<<"\n";
     require(d1==0 && d2==0,"The import switch changes an oscillator that has no table, or a table plays with the switch off");}
    // Band limit: a full saw in one 2048-sample frame, at C8.
    {std::vector<float> saw(2048);for(int n=0;n<2048;++n)saw[size_t(n)]=float(1.0-2.0*(n+.5)/2048);
     std::unique_ptr<SynthUserTable> t;require(io::make(saw,2048,"Saw",t).wasOk() && t->frames==1,"A one-frame table was not built");
     const std::shared_ptr<const SynthUserTable> one(std::move(t));const double f=hz(108);const auto high=play(one,2,108),low=play(one,0,57);
     const double fold6=db(magnitude(high,12000,48000,rate-6*f)/magnitude(high,12000,48000,f)),fold7=db(magnitude(high,12000,48000,rate-7*f)/magnitude(high,12000,48000,f));
     std::cout<<"MEASURE imported saw: at C8 harmonic 4 "<<harmonicDb(high,f,4)<<" dB (-12.0), folded harmonic 6 "<<fold6<<" dB, 7 "<<fold7<<" dB; at A3 harmonic 2 "<<harmonicDb(low,220,2)<<" (-6.0), 60 "<<harmonicDb(low,220,60)<<" dB (-35.6)\n";
     require(fold6<-70 && fold7<-70 && std::abs(harmonicDb(high,f,4)+12.04)<.5,"An imported table aliases or is dull at C8");
     require(std::abs(harmonicDb(low,220,2)+6.02)<.3 && std::abs(harmonicDb(low,220,60)-db(1.0/60))<.5,"An imported saw is not a saw at A3");
     // The warp applies to an imported table: Sync at 100 % on a one-harmonic frame.
     const auto sync=play(four,0,57,true,synthWarpSync,100);require(harmonicDb(sync,220,8)>20,"The warp does not apply to an imported table");}
    // Other frame sizes keep the harmonics: 256 (a power of two) and 600 (not).
    {double worst256=0,worst600=0;std::vector<float> out(2048);
     std::vector<float> a(256);for(int n=0;n<256;++n)a[size_t(n)]=float(.6*std::sin(2*pi*n/256)+.3*std::cos(2*pi*5*n/256));
     io::resampleCycle(a.data(),256,out.data());for(int n=0;n<2048;++n)worst256=std::max(worst256,std::abs(double(out[size_t(n)])-(.6*std::sin(2*pi*n/2048)+.3*std::cos(2*pi*5*n/2048))));
     std::vector<float> b(600);for(int n=0;n<600;++n)b[size_t(n)]=float(.6*std::sin(2*pi*n/600)+.3*std::cos(2*pi*7*n/600));
     io::resampleCycle(b.data(),600,out.data());for(int n=0;n<2048;++n)worst600=std::max(worst600,std::abs(double(out[size_t(n)])-(.6*std::sin(2*pi*n/2048)+.3*std::cos(2*pi*7*n/2048))));
     std::cout<<"MEASURE a cycle of 256 samples as 2048: largest error "<<worst256<<"; of 600 samples: "<<worst600<<"\n";
     require(worst256<1e-5 && worst600<1e-5,"A cycle of another length does not keep its harmonics");}
    // Files. Auto: a multiple of 2048 is cut at 2048; a remainder is left out and said; a short file is one cycle.
    const auto dir=juce::File::createTempFile("synthTables");dir.deleteRecursively();require(dir.createDirectory().wasOk(),"No test folder");
    const auto load=[&](const juce::String& name,int size,std::unique_ptr<SynthUserTable>& t,juce::String& said){return io::load(dir.getChildFile(name),size,t,&said);};
    std::unique_ptr<SynthUserTable> t;juce::String said;
    writeWav(dir.getChildFile("eight.wav"),ladder(8,2048));
    require(load("eight.wav",io::frameAuto,t,said).wasOk() && t->frames==8 && t->sourceFrameSize==2048 && said.isEmpty() && t->name=="eight","A file of eight 2048-sample frames was not read as eight frames");
    {auto x=ladder(3,2048);x.resize(x.size()+10000,.1f);writeWav(dir.getChildFile("ragged.wav"),x);}
    require(load("ragged.wav",io::frameAuto,t,said).wasOk() && t->frames==7 && said.contains("1808 samples"),"A remainder after the last whole frame was not left out and reported: "+said.toStdString());
    writeWav(dir.getChildFile("cycle.wav"),ladder(1,600));
    require(load("cycle.wav",io::frameAuto,t,said).wasOk() && t->frames==1 && t->sourceFrameSize==600 && said.isEmpty(),"A short file was not read as one cycle");
    // A chosen frame size.
    writeWav(dir.getChildFile("small.wav"),ladder(16,256));
    require(load("small.wav",io::frame256,t,said).wasOk() && t->frames==16 && t->sourceFrameSize==256,"Frame size 256 was not used");
    require(load("small.wav",io::frameOneCycle,t,said).wasOk() && t->frames==1 && t->sourceFrameSize==4096,"One cycle did not take the whole file");
    require(load("small.wav",io::frameAuto,t,said).wasOk() && t->frames==2 && t->sourceFrameSize==2048,"Auto did not cut a 4096-sample file at 2048");
    // More frames than a table holds: 64 evenly spaced, the first and last kept; beyond 256 the file is not read.
    writeWav(dir.getChildFile("hundred.wav"),ladder(100,2048),16);
    require(load("hundred.wav",io::frameAuto,t,said).wasOk() && t->frames==64 && t->sourceFrames==100 && said.contains("64 of 100"),"A file with 100 frames was not reduced to 64: "+said.toStdString());
    {const std::shared_ptr<const SynthUserTable> big(std::move(t));const auto end=play(big,4,45);   // A2: the last frame is harmonic 100, 11 kHz
     require(db(magnitude(end,12000,48000,11000)/magnitude(end,12000,48000,110))>60,"The last of 100 frames is not the table's last frame");}
    {const double begun=juce::Time::getMillisecondCounterHiRes();writeWav(dir.getChildFile("long.wav"),ladder(300,2048),16);const double written=juce::Time::getMillisecondCounterHiRes();
     require(load("long.wav",io::frameAuto,t,said).wasOk() && t->frames==64 && t->sourceFrames==256 && said.contains("first 256"),"A file beyond 256 frames was not cut there: "+said.toStdString());
     std::cout<<"MEASURE importing 256 frames of 2048 samples (64 kept): "<<juce::Time::getMillisecondCounterHiRes()-written<<" ms (one run, this machine; writing the file took "<<written-begun<<" ms)\n";}
    // A Serum-style `clm ` chunk names the frame size: 16-bit, four frames of 1024.
    {const auto x=ladder(4,1024);juce::MemoryOutputStream o;const char clm[]="<!>1024 00000000 wavetable (test)";const int clmSize=int(sizeof(clm)-1);const int dataSize=int(x.size())*2;
     o.write("RIFF",4);o.writeInt(4+8+16+8+clmSize+(clmSize&1)+8+dataSize);o.write("WAVE",4);o.write("fmt ",4);o.writeInt(16);o.writeShort(1);o.writeShort(1);o.writeInt(48000);o.writeInt(96000);o.writeShort(2);o.writeShort(16);
     o.write("clm ",4);o.writeInt(clmSize);o.write(clm,size_t(clmSize));if(clmSize&1)o.writeByte(0);
     o.write("data",4);o.writeInt(dataSize);for(float v:x)o.writeShort(short(juce::roundToInt(v*32767.f)));
     require(dir.getChildFile("clm.wav").replaceWithData(o.getData(),o.getDataSize()),"Cannot write the clm test file");
     require(io::clmFrameSize(dir.getChildFile("clm.wav"))==1024 && io::clmFrameSize(dir.getChildFile("eight.wav"))==0,"The clm chunk's frame size was not read");
     require(load("clm.wav",io::frameAuto,t,said).wasOk() && t->frames==4 && t->sourceFrameSize==1024,"The clm chunk's frame size was not used");}
    // Refused, each with a message.
    {std::unique_ptr<SynthUserTable> none;
     writeWav(dir.getChildFile("silent.wav"),std::vector<float>(4096,0.f));writeWav(dir.getChildFile("tiny.wav"),std::vector<float>(10,.5f));writeWav(dir.getChildFile("toolong.wav"),std::vector<float>(20000,.5f));
     dir.getChildFile("text.wav").replaceWithText("not audio");
     const auto refused=[&](const juce::String& name,int size){juce::String ignored;const auto r=io::load(dir.getChildFile(name),size,none,&ignored);return r.failed() && r.getErrorMessage().isNotEmpty() && none==nullptr;};
     require(refused("silent.wav",io::frameAuto) && refused("tiny.wav",io::frameAuto) && refused("toolong.wav",io::frameOneCycle) && refused("text.wav",io::frameAuto) && refused("missing.wav",io::frameAuto) && refused("cycle.wav",io::frame1024),
             "A silent, too short, too long, non-audio or missing file was not refused");
     std::vector<float> bad(2048,.1f);bad[7]=std::numeric_limits<float>::quiet_NaN();require(io::make(bad,2048,"bad",none).failed() && io::make(bad,8,"bad",none).failed() && none==nullptr,"A value that is not a number or a frame size out of range was not refused");}
    // Through the processor: the oscillator switches to its table; the project keeps it exactly.
    {auto p=std::make_unique<ChimeraProcessor>();const auto raw=[](ChimeraProcessor& q,const char* id){return q.parameters().getRawParameterValue(id)->load();};
     {auto* mode=p->parameters().getParameter("synthOscBMode");mode->setValueNotifyingHost(1.f);}
     juce::String cut;require(p->loadSynthTable(1,dir.getChildFile("hundred.wav"),io::frameAuto,&cut).wasOk() && cut.contains("64 of 100"),"The processor did not import a table");
     require(raw(*p,"synthOscBOn")>.5f && raw(*p,"synthOscBMode")<.5f && raw(*p,"synthOscBImported")>.5f && raw(*p,"synthOscAImported")<.5f,"Importing did not switch osc B to Wave mode and its table");
     require(p->loadSynthTable(1,dir.getChildFile("silent.wav")).failed() && p->synthEngine().sharedTable(1)!=nullptr && p->synthEngine().sharedTable(1)->name=="hundred","A refused file replaced the table");
     juce::MemoryBlock saved;p->getStateInformation(saved);
     auto q=std::make_unique<ChimeraProcessor>();q->setStateInformation(saved.getData(),int(saved.getSize()));
     const auto a=p->synthEngine().sharedTable(1),b=q->synthEngine().sharedTable(1);
     require(a && b && b->frames==64 && b->sourceFrames==100 && b->sourceFrameSize==2048 && b->name=="hundred" && a->data.size()==b->data.size() && raw(*q,"synthOscBImported")>.5f,"The imported table was not restored");
     double worst=0;for(size_t n=0;n<a->data.size();++n)worst=std::max(worst,double(std::abs(a->data[n]-b->data[n])));
     auto fresh=std::make_unique<ChimeraProcessor>();juce::MemoryBlock none;fresh->getStateInformation(none);
     std::cout<<"MEASURE table state: "<<(saved.getSize()-none.getSize())/1024<<" KiB more with one 64-frame table, largest difference after save and restore "<<worst<<"\n";
     q->setStateInformation(none.getData(),int(none.getSize()));
     require(worst==0 && q->synthEngine().sharedTable(1)==nullptr && raw(*q,"synthOscBImported")<.5f,"The table round trip is inexact, or a table survived a project without one");
     // A damaged entry is ignored; the rest of the project loads.
     {juce::ValueTree tree("SYNTH_TABLES");juce::ValueTree item("TABLE");item.setProperty("osc",0,nullptr);item.setProperty("frames",2,nullptr);item.setProperty("cycles",juce::MemoryBlock(1000),nullptr);tree.appendChild(item,nullptr);
      juce::ValueTree wrong("TABLE");wrong.setProperty("osc",7,nullptr);wrong.setProperty("frames",1,nullptr);wrong.setProperty("cycles",juce::MemoryBlock(size_t(synthTableSize)*4),nullptr);tree.appendChild(wrong,nullptr);
      const auto restored=io::restore(tree);require(!restored[0] && !restored[1] && !restored[2],"A damaged table entry was restored");}
     p->clearSynthTable(1);require(p->synthEngine().sharedTable(1)==nullptr && raw(*p,"synthOscBImported")<.5f,"Remove did not clear the table and its switch");}
    dir.deleteRecursively();
    std::cout<<"PASS wavetable import: frames, position, band limit, frame sizes, clm chunk, refusals, project state\n";
}

// Preset files (docs/SYNTH_PRESETS.md): names, the round trip, refusals, the list.
void presetFiles() {
    namespace sp=spectralforge::synthPresets;
    const auto dir=juce::File::createTempFile("synthPresets");dir.deleteRecursively();require(dir.createDirectory(),"No temporary preset folder");
    struct Cleanup{juce::File d;~Cleanup(){d.deleteRecursively();}} cleanup{dir};
    // Names
    require(sp::cleanName("  My   Bass  ")=="My Bass" && sp::cleanName("a/b\\c:d*e?f\"g<h>i|j")=="abcdefghij" && sp::cleanName("..hidden")=="hidden" && sp::cleanName("end. . ")=="end","Preset names are not cleaned");
    require(sp::cleanName("").isEmpty() && sp::cleanName(" ./\\ ").isEmpty() && sp::cleanName("con").isEmpty() && sp::cleanName("LPT1.x").isEmpty() && sp::cleanName("Console")=="Console","Empty or reserved preset names are accepted");
    require(sp::cleanName(juce::String::repeatedString("x",200)).length()==sp::maxNameLength && sp::cleanName(juce::CharPointer_UTF8("\xec\x8b\xa0\xec\x8a\xa4 \xeb\xb2\xa0\xec\x9d\xb4\xec\x8a\xa4"))==juce::String(juce::CharPointer_UTF8("\xec\x8b\xa0\xec\x8a\xa4 \xeb\xb2\xa0\xec\x9d\xb4\xec\x8a\xa4")),"Long or non-Latin preset names are wrong");
    require(sp::cleanCategory("pad")=="Pad" && sp::cleanCategory("nonsense")=="Other" && sp::categories().size()==7,"Preset types are wrong");
    // Round trip: every carried parameter at a random legal value.
    Host h;juce::Random random(41);int carried=0,kept=0;
    for(auto* raw:h.getParameters()){auto* q=dynamic_cast<juce::RangedAudioParameter*>(raw);q->setValueNotifyingHost(random.nextFloat());if(sp::carries(q->getParameterID()))++carried;else ++kept;}
    const auto captured=sp::capture(h,"Round / Trip","Lead");
    require(captured.name=="Round Trip" && captured.category=="Lead" && int(captured.values.size())==carried && kept==8,"A capture does not hold every carried parameter");
    juce::File file;require(sp::write(dir,captured,&file).wasOk() && file==dir.getChildFile("Round Trip.chimerasynth") && file.existsAsFile(),"A preset was not written under its name");
    spectralforge::SynthPreset loaded;require(sp::read(file,loaded).wasOk() && loaded.name=="Round Trip" && loaded.category=="Lead" && loaded.user && loaded.values.size()==captured.values.size(),"A written preset does not read back");
    double worst=0;for(const auto& [id,v]:captured.values){bool found=false;for(const auto& [other,w]:loaded.values)if(other==id){found=true;worst=std::max(worst,double(std::abs(v-w)));}require(found,"A parameter is missing from the read preset");}
    for(const auto& [id,v]:loaded.values)require(!synthPatchKeeps(id),"A preset file holds a parameter presets must not change");
    std::cout<<"MEASURE preset file: "<<carried<<" parameters carried, "<<kept<<" kept out, "<<file.getSize()<<" bytes; largest difference after write and read "<<worst<<"\n";
    require(worst==0,"Preset values change in the file");
    // Refusals. None may change the preset passed in.
    const auto put=[&](const char* name,const juce::String& text){const auto f=dir.getChildFile(name);f.replaceWithText(text);return f;};
    const auto refused=[&](const juce::File& f){spectralforge::SynthPreset q;q.name="untouched";const auto r=sp::read(f,q);return r.failed() && r.getErrorMessage().isNotEmpty() && q.name=="untouched";};
    require(refused(dir.getChildFile("missing.chimerasynth")),"A missing preset file reads");
    require(refused(put("a.chimerasynth","not json at all {")) && refused(put("a.chimerasynth","[1,2,3]")) && refused(put("a.chimerasynth","{\"format\":\"other\",\"version\":1,\"parameters\":{}}")),"A file that is not a preset reads");
    require(refused(put("a.chimerasynth","{\"format\":\"chimera-synth-preset\",\"version\":2,\"parameters\":{}}")) && refused(put("a.chimerasynth","{\"format\":\"chimera-synth-preset\",\"version\":\"1\",\"parameters\":{}}"))
         && refused(put("a.chimerasynth","{\"format\":\"chimera-synth-preset\",\"version\":0,\"parameters\":{}}")) && refused(put("a.chimerasynth","{\"format\":\"chimera-synth-preset\",\"version\":1}")),"A preset of another version or without parameters reads");
    require(refused(put("a.chimerasynth","{\"format\":\"chimera-synth-preset\",\"version\":1,\"parameters\":{\"synthCutoff\":\"high\"}}")) && refused(put("a.chimerasynth","{\"format\":\"chimera-synth-preset\",\"version\":1,\"parameters\":{\"synthCutoff\":true}}"))
         && refused(put("a.chimerasynth","{\"format\":\"chimera-synth-preset\",\"version\":1,\"parameters\":{\"synthCutoff\":1e300}}")) && refused(put("a.chimerasynth","{\"format\":\"chimera-synth-preset\",\"version\":1,\"parameters\":{\"synthCutoff\":[1]}}")),"A preset with a value that is not a usable number reads");
    require(refused(put("a.chimerasynth","{\"format\":\"chimera-synth-preset\",\"version\":1,\"parameters\":{},\"pad\":\""+juce::String::repeatedString("x",sp::maxFileBytes)+"\"}")),"A preset file over the size limit reads");
    // Ids a preset may not change, and ids that are not the synth's, are dropped; an unknown type becomes Other.
    spectralforge::SynthPreset partial;
    require(sp::read(put("a.chimerasynth","{\"format\":\"chimera-synth-preset\",\"version\":1,\"category\":\"Chiptune\",\"parameters\":{\"synthOn\":1,\"synthLevel\":6,\"ampGain\":3,\"synthCutoff\":432,\"synthFutureThing\":2}}"),partial).wasOk(),"A sparse preset does not read");
    require(partial.category=="Other" && partial.values.size()==2 && partial.name=="a","A preset keeps ids it must not carry");
    dir.getChildFile("a.chimerasynth").deleteFile();
    // The list: factory first, then the folder by name; unreadable files counted.
    for(const char* name:{"zeta","Alpha 10","alpha 2","Beta"}){auto q=captured;q.name=name;require(sp::write(dir,q).wasOk(),"Preset not written");}
    put("broken.chimerasynth","{");put("notes.txt","not a preset");
    int unreadable=-1;const auto all=sp::list(dir,&unreadable);const auto factoryCount=synthPatches().size();
    require(all.size()==factoryCount+5 && unreadable==1 && !all[0].user && all[0].name=="Init" && all[1].category=="Bass" && all[2].category=="Pad" && all[3].category=="Pluck","The preset list is wrong");
    std::string order;for(size_t i=factoryCount;i<all.size();++i){require(all[i].user,"A file is listed as built in");order+=all[i].name.toStdString()+"|";}
    require(order=="alpha 2|Alpha 10|Beta|Round Trip|zeta|","User presets are not in name order: "+order);
    require(sp::list(dir.getChildFile("nowhere")).size()==factoryCount,"A missing folder does not list the factory presets alone");
    // Saving a name that differs only in case replaces that preset; deleting is for user presets in the folder only.
    {auto q=captured;q.name="BETA";q.category="Keys";require(sp::write(dir,q).wasOk(),"Preset not rewritten");const auto again=sp::list(dir);int betas=0;for(const auto& x:again)if(x.name.equalsIgnoreCase("beta")){++betas;require(x.name=="BETA" && x.category=="Keys","The replaced preset is the old one");}
     require(betas==1 && again.size()==all.size(),"A name differing in case made a second preset");}
    {auto q=captured;q.name="  ";require(sp::write(dir,q).failed(),"A preset without a name was written");}
    require(sp::remove(dir,all[0]).failed(),"A factory preset can be deleted");
    {auto outside=all.back();outside.file=dir.getParentDirectory().getChildFile("elsewhere.chimerasynth");require(sp::remove(dir,outside).failed(),"A file outside the preset folder can be deleted");
     auto other=all.back();other.file=dir.getChildFile("notes.txt");require(sp::remove(dir,other).failed() && dir.getChildFile("notes.txt").existsAsFile(),"A file that is not a preset can be deleted");}
    require(sp::remove(dir,all.back()).wasOk() && !all.back().file.existsAsFile(),"A user preset was not deleted");
    // Many presets: the time to read the folder.
    for(int i=0;i<300;++i){auto q=captured;q.name="Many "+juce::String(i);require(sp::write(dir,q).wasOk(),"Preset not written");}
    const auto start=juce::Time::getMillisecondCounterHiRes();const auto many=sp::list(dir);const double ms=juce::Time::getMillisecondCounterHiRes()-start;
    std::cout<<"MEASURE preset folder with "<<many.size()-factoryCount<<" presets: read in "<<juce::String(ms,1)<<" ms\n";
    require(many.size()>=factoryCount+300,"Not every preset was listed");
    std::cout<<"PASS preset names, file round trip, refusals, list order, replace and delete\n";
}
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
    // Wavetable controls (docs/SYNTH_WAVETABLE.md): nine parameters at version hint 38, appended last.
    {int count=0;const int total=p->getParameters().size()-synthOscCount;   // the import switches (hint 42) follow
     for(auto* parameter:p->getParameters())if(auto* with=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))if(with->getVersionHint()==synthWavetableParameterVersion){
        require(with->getParameterIndex()>=total-(synthOscCount*synthWavetableParametersPerOsc+synthFiltersParameterCount+synthSequencerParameterCount+synthEffectsParameterCount) && with->paramID.startsWith("synthOsc"),"A wavetable parameter is not in the last bank");++count;}
     require(count==synthOscCount*synthWavetableParametersPerOsc && raw("synthOscATable")==0 && raw("synthOscAWarp")==0 && raw("synthOscAWarpAmount")==0,"Wavetable bank size or defaults wrong");
     static_cast<juce::Button*>(page.control("synthOscBMode0"))->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(20);page.updateModes();
     require(page.control("synthOscBTable")->isVisible() && !page.control("synthOscBWarp")->isVisible() && page.control("synthOscBShapes")->isVisible(),"Classic shapes do not show the shape strip and the table choice");
     static_cast<juce::ComboBox*>(page.control("synthOscBTable"))->setSelectedId(4,juce::sendNotificationSync);page.updateModes();
     require(juce::roundToInt(raw("synthOscBTable"))==3 && page.control("synthOscBWarp")->isVisible() && page.control("synthOscBWarpAmount")->isVisible() && !page.control("synthOscBShapes")->isVisible(),"Choosing a table does not show its warp controls");
     auto& tableDisplay=page.waveDisplay(1);drag(tableDisplay,{20,20},{float(tableDisplay.getWidth())*.5f,20});
     require(std::abs(raw("synthOscBWave")-2.f)<.08f,"Dragging across a wavetable does not set its position");
     static_cast<juce::ComboBox*>(page.control("synthOscBTable"))->setSelectedId(1,juce::sendNotificationSync);page.updateModes();}
    // Wavetable import on the page (docs/SYNTH_WAVETABLE_IMPORT.md): the menu lists the imported table and plays it.
    {auto* menu=static_cast<juce::ComboBox*>(page.control("synthOscBTable"));using Menu=spectralforge::synthUI::TableMenu;
     require(menu->indexOfItemId(Menu::importedId)<0 && menu->indexOfItemId(Menu::importId)>=0 && menu->indexOfItemId(Menu::removeId)<0,"The table menu offers an imported table before one exists, or no import");
     const auto wav=juce::File::createTempFile(".wav");
     {juce::AudioBuffer<float> b(1,4*2048);for(int k=0;k<4;++k)for(int n=0;n<2048;++n)b.setSample(0,k*2048+n,float(.5*std::sin(2*pi*(k+1)*n/2048)));
      juce::WavAudioFormat format;auto stream=wav.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> w(format.createWriterFor(stream.get(),48000,1,24,{},0));stream.release();w->writeFromAudioSampleBuffer(b,0,b.getNumSamples());}
     juce::String said;require(page.importTable(1,wav,0,&said).wasOk() && said.isEmpty(),"The page did not import a table");
     require(menu->getSelectedId()==Menu::importedId && menu->getText()==wav.getFileNameWithoutExtension()+" (imported)" && menu->indexOfItemId(Menu::removeId)>=0,"The menu does not show the imported table");
     require(raw("synthOscBImported")>.5f && page.control("synthOscBWarp")->isVisible() && !page.control("synthOscBShapes")->isVisible(),"An imported table does not show the warp in place of the shape strip");
     menu->setSelectedId(2,juce::sendNotificationSync);page.updateModes();
     require(raw("synthOscBImported")<.5f && juce::roundToInt(raw("synthOscBTable"))==1 && menu->getSelectedId()==2 && menu->indexOfItemId(Menu::importedId)>=0,"Choosing a built-in table does not leave the imported one, or forgets it");
     menu->setSelectedId(Menu::importedId,juce::sendNotificationSync);page.updateModes();
     require(raw("synthOscBImported")>.5f && juce::roundToInt(raw("synthOscBTable"))==1 && menu->getSelectedId()==Menu::importedId,"Choosing the imported table again does not play it");
     require(page.importTable(1,juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("no-such-table.wav")).failed() && menu->getSelectedId()==Menu::importedId,"A refused import changed the oscillator");
     menu->setSelectedId(Menu::removeId,juce::sendNotificationSync);page.updateModes();
     require(p->synthEngine().sharedTable(1)==nullptr && raw("synthOscBImported")<.5f && menu->getSelectedId()==2 && menu->indexOfItemId(Menu::importedId)<0,"Remove did not clear the imported table from the oscillator and the menu");
     menu->setSelectedId(1,juce::sendNotificationSync);page.updateModes();wav.deleteFile();}
    // Modulation matrix: only rows in use, + adds one, a knob's menu connects a source, x removes it.
    // Sixteen rows: 0-7 are the released parameters, 8-15 the second bank, which new connections take first.
    {using Page=spectralforge::synthUI::SynthPage;const auto pumpNow=[]{juce::MessageManager::getInstance()->runDispatchLoopUntil(20);};
     const auto id=[](int row,const char* field){return Page::modRowID(row,field);};const auto value=[&](int row,const char* field){return raw(id(row,field).toRawUTF8());};
     page.applyPatch(0);pumpNow();
     for(int m=0;m<Page::modRowCount;++m)page.clearModulation(m);
     const auto shown=[&]{int n=0;for(int m=0;m<Page::modRowCount;++m)n+=page.control(id(m,"Source"))->isVisible();return n;};
     require(Page::modRowCount==16 && id(0,"Source")=="synthMod1Source" && id(8,"Source")=="synthModB1Source","Matrix row IDs wrong");
     require(shown()==1 && page.control(id(8,"Source"))->isVisible() && page.control("synthModAdd")->isVisible(),"An empty matrix does not show one row and the + button");
     static_cast<juce::Button*>(page.control("synthModAdd"))->triggerClick();pumpNow();
     require(shown()==2,"+ does not add a row");
     require(Page::modTargetFor("synthCutoff")==targetCutoff && Page::modTargetFor("synthOscBFine")==targetPitchB && Page::modTargetFor("synthOscCWave")==targetWaveC && Page::modTargetFor("synthGlide")==0
             && Page::modTargetFor("synthFilter2Cutoff")==targetCutoff2 && Page::modTargetFor("synthFilter2Resonance")==targetResonance2,"Knob to matrix target mapping wrong");
     writes.gestures=0;
     const int row=page.assignModulation(2,targetCutoff);   // LFO 1 -> Cutoff
     require(row==8 && juce::roundToInt(value(8,"Source"))==2 && juce::roundToInt(value(8,"Target"))==targetCutoff && value(8,"Amount")==50 && writes.gestures==6,
             "Connecting a source to a knob does not fill the first free row in one gesture per parameter");
     require(page.assignModulation(2,targetCutoff)==8 && page.assignModulation(1,targetCutoff)==9,"A repeated connection made a second row, or a new one reused the row");
     auto* cutoff=dynamic_cast<juce::Slider*>(page.control("synthCutoff"));auto* glide=dynamic_cast<juce::Slider*>(page.control("synthGlide"));
     require(cutoff!=nullptr && std::abs(double(cutoff->getProperties().getWithDefault("mod",0.0))-1.0)<1e-6 && (glide==nullptr || double(glide->getProperties().getWithDefault("mod",0.0))==0),"A modulated knob is not marked");
     static_cast<juce::Button*>(page.control(id(9,"Clear")))->triggerClick();static_cast<juce::Button*>(page.control(id(8,"Clear")))->triggerClick();pumpNow();
     require(juce::roundToInt(value(8,"Source"))==0 && value(8,"Amount")==0 && shown()==1,"x does not clear a row");
     auto* amount=dynamic_cast<juce::Slider*>(page.control(id(8,"Amount")));
     require(amount!=nullptr && amount->getSliderStyle()==juce::Slider::LinearBar && amount->getTextFromValue(-35.0)=="-35 %" && amount->getTextFromValue(20.0)=="+20 %","The amount is not a signed bar");
     // A released row in use (an older project, a factory patch) is on screen too.
     {auto put=[&](const juce::String& pid,float v){auto* q=p->parameters().getParameter(pid);q->setValueNotifyingHost(q->convertTo0to1(v));};
      put(id(0,"Source"),float(modLfo1));put(id(0,"Target"),float(targetPitchA));page.refreshModulation();
      require(page.control(id(0,"Source"))->isVisible() && shown()==1,"A released matrix row in use is not shown");
      // The sequencer, the macros and the newer targets live in rows 9-16 only: eight of them, then the matrix is full for those.
      for(int k=0;k<synthModSlotsB;++k)require(page.assignModulation(modSeq+k%5,targetCutoff2+k)>=synthModSlots,"A sequencer / macro connection went to a released row");
      require(page.assignModulation(modMacro4,targetPitchA)==-1,"A ninth sequencer / macro connection found a row");
      require(page.assignModulation(modLfo2,targetAmp)==1,"With rows 9-16 full a released connection does not use a released row");
      // Sixteen rows do not fit: the list scrolls, and every row on screen sits inside it.
      for(int m=2;m<synthModSlots;++m)page.assignModulation(modLfo2,targetPitchA+m);page.refreshModulation();
      auto* list=page.control(id(15,"Source"))->getParentComponent();auto* view=dynamic_cast<juce::Viewport*>(list->getParentComponent()->getParentComponent());
      require(shown()==16 && !page.control("synthModAdd")->isVisible() && view!=nullptr && list->getHeight()>view->getHeight() && view->getHeight()>=60,"Sixteen matrix rows do not scroll");
      for(int m=0;m<Page::modRowCount;++m)require(list->getLocalBounds().contains(page.control(id(m,"Source"))->getBounds()) && list->getLocalBounds().contains(page.control(id(m,"Clear"))->getBounds()),"A matrix row leaves its list");
      std::cout<<"MEASURE matrix: sixteen rows, list "<<list->getWidth()<<" x "<<list->getHeight()<<" px in a view "<<view->getWidth()<<" x "<<view->getHeight()<<" px\n";
      for(int m=0;m<Page::modRowCount;++m)page.clearModulation(m);require(shown()==1,"Clearing every row does not leave one empty row");}}
    // Sequencer and macros share the LFO panel: LFO / SEQ / MACRO.
    {const auto pumpNow=[]{juce::MessageManager::getInstance()->runDispatchLoopUntil(20);};
     for(const auto* cid:{"synthModTabLFO","synthModTabSEQ","synthModTabMACRO","synthSeqGraph","synthSeqSteps","synthSeqRate","synthSeqSmooth","synthSeqRetrigger","synthMacro1","synthMacro4"})require(page.control(cid)!=nullptr,std::string("SYNTH page control missing: ")+cid);
     require(page.control("synthLfo1Shape")->isVisible() && !page.control("synthSeqGraph")->isVisible() && !page.control("synthMacro1")->isVisible(),"The LFO view is not the first one");
     static_cast<juce::Button*>(page.control("synthModTabSEQ"))->triggerClick();pumpNow();
     require(!page.control("synthLfo1Shape")->isVisible() && page.control("synthSeqGraph")->isVisible() && page.control("synthSeqRate")->isVisible() && !page.control("synthMacro1")->isVisible(),"SEQ does not show the sequencer");
     auto& steps=page.stepGraph();const auto c3=steps.column(3).getCentre(),c6=steps.column(6).getCentre();
     require(steps.getWidth()>=100 && steps.getHeight()>=40,"The step graph is too small");
     writes.gestures=0;writes.changes.clear();drag(steps,{c3.x,float(steps.getHeight())*.25f},{c6.x,float(steps.getHeight())*.75f});
     std::cout<<"MEASURE step graph "<<steps.getWidth()<<" x "<<steps.getHeight()<<" px; drag from step 4 (upper quarter) to step 7 (lower quarter): steps 4-7 "<<raw("synthSeqStep4")<<" / "<<raw("synthSeqStep5")<<" / "<<raw("synthSeqStep6")<<" / "<<raw("synthSeqStep7")<<" %, gestures "<<writes.gestures<<"\n";
     require(std::abs(raw("synthSeqStep4")-50)<=4 && std::abs(raw("synthSeqStep7")+50)<=4 && std::abs(raw("synthSeqStep5")-17)<=4 && std::abs(raw("synthSeqStep6")+17)<=4 && writes.gestures==8 && writes.changes.size()==4,"Drawing steps is not one gesture per step, misplaced, or skips the steps passed over");
     {const auto source=juce::Desktop::getInstance().getMainMouseSource();const auto now=juce::Time::getCurrentTime();
      steps.mouseDoubleClick(juce::MouseEvent(source,c3,juce::ModifierKeys(),1,0,0,0,0,&steps,&steps,now,c3,now,2,false));require(raw("synthSeqStep4")==0,"Double-click does not reset a step");}
     static_cast<juce::Button*>(page.control("synthModTabMACRO"))->triggerClick();pumpNow();
     require(page.control("synthMacro1")->isVisible() && page.control("synthMacro4")->isVisible() && !page.control("synthSeqGraph")->isVisible(),"MACRO does not show the macros");
     auto* macro=dynamic_cast<juce::Slider*>(page.control("synthMacro2"));require(macro!=nullptr && macro->getTextFromValue(35.0)=="MACRO 2   35 %" && macro->getHeight()>=16,"A macro bar is unnamed or too small");
     static_cast<juce::Button*>(page.control("synthModTabLFO"))->triggerClick();pumpNow();require(page.control("synthLfo2Sync")->isVisible(),"LFO does not come back");
     int count=0;const int total=p->getParameters().size()-synthOscCount;   // the import switches (hint 42) follow
     for(auto* parameter:p->getParameters())if(auto* with=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))if(with->getVersionHint()==synthSequencerParameterVersion){require(with->getParameterIndex()>=total-synthSequencerParameterCount-synthEffectsParameterCount && with->getParameterIndex()<total-synthEffectsParameterCount,"A sequencer parameter is not in its bank");++count;}
     require(count==synthSequencerParameterCount && synthSequencerParameterCount==48,"Sequencer bank size wrong");
     auto put=[&](const juce::String& pid,float v){auto* q=p->parameters().getParameter(pid);q->setValueNotifyingHost(q->convertTo0to1(v));};for(int k=0;k<synthSeqMaxSteps;++k)put(synthSeqStepID(k),0);}
    // Effect rack (docs/SYNTH_EFFECTS.md): FX swaps the lower row for five effect panels; twenty-two parameters at version hint 41, appended last.
    {const auto shown=[&](const juce::String& id){auto* c=page.control(id);if(c==nullptr)return false;for(;c!=nullptr && c!=&page;c=c->getParentComponent())if(!c->isVisible())return false;return true;};
     const auto value=[&](const char* id){return p->parameters().getRawParameterValue(id)->load();};
     const auto put=[&](const char* id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
     const auto ids=SynthEffects::parameterIDs();const auto pumpNow=[]{juce::MessageManager::getInstance()->runDispatchLoopUntil(20);};
     require(!page.effectsShown() && shown("synthAmpAttack") && shown("synthSubOn") && shown("synthModAdd"),"The page does not open on the envelopes and matrix");
     for(const auto& id:ids)require(page.control(id)!=nullptr && !shown(id),("An effect control is missing or shown outside the rack: "+id).toStdString());
     auto* view=dynamic_cast<juce::Button*>(page.control("synthFxView"));require(view!=nullptr && view->getWidth()>=30 && page.getLocalBounds().contains(view->getBounds()),"No FX button in the top bar");
     view->triggerClick();pumpNow();
     require(page.effectsShown() && !shown("synthAmpAttack") && !shown("synthModEnvDecay") && !shown("synthSubOn") && !shown("synthNoiseLevel") && !shown("synthModAdd") && !shown("synthModTabLFO") && !shown("synthLfo1Rate") && !shown("synthSeqGraph") && !shown("synthMacro1"),
             "FX leaves envelope, LFO, matrix or sub / noise controls on screen");
     require(shown("synthLevel") && shown("synthCutoff") && shown("synthOutput") && shown("synthOscALevel"),"FX hides the oscillators, filters or master");
     std::vector<std::pair<juce::String,juce::Rectangle<int>>> areas;const auto lower=page.getLocalBounds().withTrimmedTop(page.getHeight()/2);int smallest=1000;
     for(const auto& id:ids){auto* c=page.control(id);require(shown(id),("An effect control is not shown in the rack: "+id).toStdString());
         const auto area=page.getLocalArea(c->getParentComponent(),c->getBounds());require(lower.contains(area) && area.getWidth()>=36 && area.getHeight()>=16,("An effect control is outside the lower row or too small: "+id).toStdString());
         for(const auto& [other,taken]:areas)require(!taken.intersects(area),("Effect controls overlap: "+id+" and "+other).toStdString());
         if(dynamic_cast<juce::Slider*>(c)!=nullptr)smallest=std::min(smallest,std::min(area.getWidth(),area.getHeight()));
         areas.push_back({id,area});}
     require(!areas.back().second.intersects(page.getLocalArea(page.control("synthLevel")->getParentComponent(),page.control("synthLevel")->getBounds())),"The rack covers the master controls");
     std::cout<<"MEASURE effect rack on the page ("<<page.getWidth()<<" x "<<page.getHeight()<<"): "<<ids.size()<<" controls in the lower row, none overlapping; smallest knob "<<smallest<<" px\n";
     static_cast<juce::Button*>(page.control("synthFxDelayOn"))->triggerClick();pumpNow();require(value("synthFxDelayOn")>.5f,"The DELAY switch does not switch the delay");put("synthFxDelayOn",0);
     auto* time=page.control("synthFxDelayTime")->getParentComponent();
     put("synthFxDelaySync",0);page.updateModes();require(time->isEnabled(),"TIME is not usable in Free");
     put("synthFxDelaySync",4);page.updateModes();require(!time->isEnabled(),"TIME is not dimmed while the delay follows the tempo");
     view->triggerClick();pumpNow();
     require(!page.effectsShown() && shown("synthAmpAttack") && shown("synthSubOn") && shown("synthModAdd") && shown("synthLfo1Rate") && !shown("synthSeqGraph") && !shown("synthFxChorusOn") && !shown("synthFxEqHigh"),"FX off does not bring the envelopes, LFOs and matrix back");
     int count=0;const int total=p->getParameters().size()-synthOscCount;   // the import switches (hint 42) follow
     for(auto* parameter:p->getParameters())if(auto* with=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))if(with->getVersionHint()==synthEffectsParameterVersion){
        require(with->getParameterIndex()==total-synthEffectsParameterCount+count && with->paramID==ids[count] && parameter->isAutomatable(),"An effect parameter is not in the last bank, in order");++count;}
     require(count==synthEffectsParameterCount && synthEffectsParameterCount==22,"Effect bank size wrong");
     for(const char* id:{"synthFxChorusOn","synthFxPhaserOn","synthFxDelayOn","synthFxReverbOn","synthFxEqOn"}){auto* q=p->parameters().getParameter(id);require(q->getDefaultValue()<.5f,"An effect is on by default");}}
    // Preset browser (docs/SYNTH_PRESETS.md): previous / name / next in the top bar, the browser in the lower row, user presets as files.
    {namespace sp=spectralforge::synthPresets;
     const auto dir=juce::File::createTempFile("synthPagePresets");dir.deleteRecursively();
     struct Cleanup{juce::File d;~Cleanup(){d.deleteRecursively();}} cleanup{dir};
     const auto pumpNow=[]{juce::MessageManager::getInstance()->runDispatchLoopUntil(20);};
     const auto shown=[&](const juce::String& id){auto* c=page.control(id);if(c==nullptr)return false;for(;c!=nullptr && c!=&page;c=c->getParentComponent())if(!c->isVisible())return false;return true;};
     const auto put=[&](const char* id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
     const auto click=[&](const char* id){auto* b=dynamic_cast<juce::Button*>(page.control(id));require(b!=nullptr,std::string("No button ")+id);b->triggerClick();pumpNow();};
     const auto status=[&]{return static_cast<juce::Label*>(page.control("synthPresetStatus"))->getText();};
     page.showEffects(false);page.applyPatch(0);
     page.setPresetDirectory(dir);require(!dir.exists(),"Opening the page or pointing it at a folder created the preset folder");
     // Top bar: nothing overlaps, everything is on the page.
     {std::vector<std::pair<std::string,juce::Rectangle<int>>> bar;
      for(const char* id:{"synthOn","synthType","synthCharacter","synthSource","synthInsert","synthPresetPrev","synthPreset","synthPresetNext","synthFxView","synthVoices","synthOctave"}){
         auto* c=page.control(id);require(c!=nullptr && c->isVisible(),std::string("Top-bar control missing: ")+id);auto* top=c;while(top->getParentComponent()!=&page)top=top->getParentComponent();
         require(page.getLocalBounds().contains(top->getBounds()) && top->getBottom()<=28,std::string("Top-bar control off the bar: ")+id);
         for(const auto& [other,taken]:bar)require(!taken.intersects(top->getBounds()),std::string("Top-bar controls overlap: ")+id+" and "+other);
         bar.push_back({id,top->getBounds()});}
      require(page.control("synthPreset")->getWidth()>=100 && page.control("synthPresetPrev")->getWidth()>=12 && page.control("synthPresetPrev")->getHeight()>=12,"The preset name or its arrows are too small");}
     const juce::StringArray browser{"synthPresetList","synthPresetBank","synthPresetType","synthPresetSearch","synthPresetName","synthPresetSaveType","synthPresetSave","synthPresetDelete","synthPresetFolder","synthPresetStatus","synthPresetCount"};
     for(const auto& id:browser)require(page.control(id)!=nullptr && !shown(id),("A browser control is missing or shown before the browser opens: "+id).toStdString());
     auto* name=dynamic_cast<juce::TextButton*>(page.control("synthPreset"));auto* list=dynamic_cast<juce::ListBox*>(page.control("synthPresetList"));
     auto* nameEditor=dynamic_cast<juce::TextEditor*>(page.control("synthPresetName"));auto* search=dynamic_cast<juce::TextEditor*>(page.control("synthPresetSearch"));
     auto* bank=dynamic_cast<juce::ComboBox*>(page.control("synthPresetBank"));auto* kind=dynamic_cast<juce::ComboBox*>(page.control("synthPresetType"));auto* saveKind=dynamic_cast<juce::ComboBox*>(page.control("synthPresetSaveType"));
     require(name && list && nameEditor && search && bank && kind && saveKind,"A browser control has the wrong type");
     require(page.currentPresetName()=="Init" && name->getButtonText()=="Init" && !page.currentPresetEdited(),"The loaded patch's name is not shown");
     writes.gestures=0;writes.changes.clear();
     click("synthPreset");
     require(page.presetsShown() && !page.effectsShown() && !shown("synthAmpAttack") && !shown("synthSubOn") && !shown("synthModAdd") && !shown("synthLfo1Rate") && !shown("synthFxChorusOn"),"The browser leaves envelope, LFO, matrix or effect controls on screen");
     require(shown("synthLevel") && shown("synthCutoff") && shown("synthOscALevel") && shown("synthOutput"),"The browser hides the oscillators, filters or master");
     {std::vector<std::pair<juce::String,juce::Rectangle<int>>> areas;const auto lower=page.getLocalBounds().withTrimmedTop(page.getHeight()/2);
      for(const auto& id:browser){auto* c=page.control(id);require(shown(id),("A browser control is not shown: "+id).toStdString());const auto area=page.getLocalArea(c->getParentComponent(),c->getBounds());
         require(lower.contains(area) && area.getWidth()>=60 && area.getHeight()>=16,("A browser control is outside the lower row or too small: "+id).toStdString());
         for(const auto& [other,taken]:areas)require(!taken.intersects(area),("Browser controls overlap: "+id+" and "+other).toStdString());areas.push_back({id,area});}
      require(!list->getBounds().intersects(page.getLocalArea(page.control("synthLevel")->getParentComponent(),page.control("synthLevel")->getBounds())),"The browser covers the master controls");
      std::cout<<"MEASURE preset browser on the page ("<<page.getWidth()<<" x "<<page.getHeight()<<"): list "<<list->getWidth()<<" x "<<list->getHeight()<<" px, "<<list->getHeight()/list->getRowHeight()<<" rows without scrolling\n";
      require(list->getHeight()/list->getRowHeight()>=6,"The preset list shows too few rows");}
     const int factoryCount=int(synthPatches().size());
     require(page.presetsListed()==factoryCount && page.presetNameAt(0)=="Init" && list->getSelectedRow()==0 && writes.changes.empty(),"The browser does not open on the factory presets with the loaded one marked, or opening it wrote a parameter");
     // Save: the settings as they are, nothing written to a parameter; kept parameters are not in the file.
     put("synthCutoff",777);put("synthFxDelayOn",1);put("synthMacro2",40);put("synthLevel",-10);put("synthOn",1);
     require(page.currentPresetEdited() && name->getButtonText()=="Init *","A changed preset is not marked");
     writes.gestures=0;writes.changes.clear();
     click("synthPresetSave");require(status().containsIgnoreCase("name") && !dir.exists(),"Saving without a name did something");
     nameEditor->setText("  My / Bass ",true);pumpNow();saveKind->setSelectedId(1,juce::sendNotificationSync);click("synthPresetSave");
     const auto file=dir.getChildFile("My Bass.chimerasynth");
     require(file.existsAsFile() && writes.changes.empty() && page.currentPresetName()=="My Bass" && !page.currentPresetEdited() && name->getButtonText()=="My Bass" && status().contains("Saved"),"SAVE did not write the preset and show it as loaded, or wrote a parameter");
     require(page.presetsListed()==factoryCount+1 && page.presetNameAt(factoryCount)=="My Bass" && list->getSelectedRow()==factoryCount,"The saved preset is not in the list, selected");
     {spectralforge::SynthPreset saved;require(sp::read(file,saved).wasOk() && saved.category=="Bass","The saved preset does not read or has the wrong type");
      for(const auto& [id,v]:saved.values)require(id!="synthOn" && id!="synthLevel","A kept parameter was saved");}
     // Loading: the Init patch, then the saved preset from the list. Routing, level and the on switch stay.
     list->selectRow(0);pumpNow();
     require(page.currentPresetName()=="Init" && std::abs(raw("synthCutoff")-6000)<1 && raw("synthFxDelayOn")<.5f && raw("synthMacro2")<.5f && std::abs(raw("synthLevel")+10)<.01f && raw("synthOn")>.5f,"Choosing Init in the list did not load it, or changed a kept parameter");
     put("synthLevel",-3);writes.gestures=0;writes.changes.clear();
     list->selectRow(factoryCount);pumpNow();
     require(page.currentPresetName()=="My Bass" && std::abs(raw("synthCutoff")-777)<.5f && raw("synthFxDelayOn")>.5f && std::abs(raw("synthMacro2")-40)<.01f && std::abs(raw("synthLevel")+3)<.01f && !page.currentPresetEdited(),"Choosing the saved preset did not restore it");
     require(writes.changes.size()==3 && writes.gestures==6 && writes.changes.count(index("synthLevel"))==0,"Loading a preset is not one gesture per changed parameter");
     // Replace needs a second press.
     put("synthCutoff",1234);click("synthPresetSave");
     {spectralforge::SynthPreset saved;sp::read(file,saved);float cutoff=0;for(const auto& [id,v]:saved.values)if(id=="synthCutoff")cutoff=v;
      require(std::abs(cutoff-777)<.5f && static_cast<juce::Button*>(page.control("synthPresetSave"))->getButtonText()=="REPLACE?" && page.currentPresetEdited(),"SAVE replaced a preset without asking");}
     click("synthPresetSave");
     {spectralforge::SynthPreset saved;sp::read(file,saved);float cutoff=0;for(const auto& [id,v]:saved.values)if(id=="synthCutoff")cutoff=v;
      require(std::abs(cutoff-1234)<.5f && static_cast<juce::Button*>(page.control("synthPresetSave"))->getButtonText()=="SAVE" && !page.currentPresetEdited() && page.presetsListed()==factoryCount+1,"The second press did not replace the preset");}
     // A second preset; filters, search, previous / next.
     nameEditor->setText("Glass Pad",true);pumpNow();saveKind->setSelectedId(3,juce::sendNotificationSync);click("synthPresetSave");
     require(page.presetsListed()==factoryCount+2 && page.currentPresetName()=="Glass Pad","The second preset was not saved");
     bank->setSelectedId(3,juce::sendNotificationSync);require(page.presetsListed()==2 && page.presetNameAt(0)=="Glass Pad" && page.presetNameAt(1)=="My Bass" && list->getSelectedRow()==0,"USER does not list the two saved presets");
     bank->setSelectedId(2,juce::sendNotificationSync);require(page.presetsListed()==factoryCount && list->getSelectedRow()<0,"FACTORY lists a user preset or marks one");
     bank->setSelectedId(1,juce::sendNotificationSync);kind->setSelectedId(4,juce::sendNotificationSync);
     require(page.presetsListed()==2 && page.presetNameAt(0)=="Seidr Dynamic" && page.presetNameAt(1)=="Glass Pad","The type filter does not list the pads");
     kind->setSelectedId(1,juce::sendNotificationSync);search->setText("BAS",true);pumpNow();require(page.presetsListed()==1 && page.presetNameAt(0)=="My Bass","Search does not find the preset by part of its name");
     search->setText("zzz",true);pumpNow();require(page.presetsListed()==0,"Search lists presets that do not match");
     writes.changes.clear();click("synthPresetNext");require(writes.changes.empty() && page.currentPresetName()=="Glass Pad","Next loaded something from an empty list");
     search->setText("",true);pumpNow();require(page.presetsListed()==factoryCount+2 && writes.changes.empty(),"Clearing the search does not list everything, or filtering loaded a preset");
     click("synthPresetNext");require(page.currentPresetName()=="My Bass" && std::abs(raw("synthCutoff")-1234)<.5f,"Next does not load the next preset");
     click("synthPresetNext");require(page.currentPresetName()=="Init" && std::abs(raw("synthCutoff")-6000)<1,"Next does not wrap to the first preset");
     click("synthPresetPrev");click("synthPresetPrev");require(page.currentPresetName()=="Glass Pad","Previous does not step back");
     bank->setSelectedId(3,juce::sendNotificationSync);click("synthPresetNext");click("synthPresetNext");require(page.currentPresetName()=="Glass Pad","Previous / next do not stay inside the filtered list");
     bank->setSelectedId(1,juce::sendNotificationSync);
     // The name travels with the project.
     {juce::MemoryBlock saved;p->getStateInformation(saved);
      auto q=std::make_unique<ChimeraProcessor>();q->setStateInformation(saved.getData(),int(saved.getSize()));
      spectralforge::synthUI::SynthPage second(*q);second.setSize(1140,438);second.setPresetDirectory(dir);
      require(second.currentPresetName()=="Glass Pad" && !second.currentPresetEdited() && static_cast<juce::Button*>(second.control("synthPreset"))->getButtonText()=="Glass Pad","A reloaded project does not show its preset");
      auto* cutoff=q->parameters().getParameter("synthCutoff");cutoff->setValueNotifyingHost(.2f);require(second.currentPresetEdited(),"A change after a reload is not marked");}
     // A hand-written file: values beyond a control's range are held at its limit; parameters it does not name take their defaults.
     dir.getChildFile("Wild.chimerasynth").replaceWithText("{\"format\":\"chimera-synth-preset\",\"version\":1,\"category\":\"FX\",\"parameters\":{\"synthCutoff\":1e8,\"synthResonance\":-50,\"synthFilterType\":99,\"synthOn\":0,\"synthNotThere\":1}}");
     dir.getChildFile("Broken.chimerasynth").replaceWithText("{");
     page.refreshPresets();
     {int row=-1;for(int i=0;i<page.presetsListed();++i)if(page.presetNameAt(i)=="Wild")row=i;require(row>=0 && page.presetsListed()==factoryCount+3,"A hand-written preset is not listed, or a broken file is");
      require(static_cast<juce::Label*>(page.control("synthPresetCount"))->getText().contains("1 unreadable"),"A file that cannot be read is not reported");
      list->selectRow(row);pumpNow();
      auto* cutoff=p->parameters().getParameter("synthCutoff");auto* type=p->parameters().getParameter("synthFilterType");
      require(page.currentPresetName()=="Wild" && raw("synthCutoff")==cutoff->convertFrom0to1(1.f) && raw("synthResonance")==0 && type->getValue()==1.f && raw("synthOn")>.5f && raw("synthFxDelayOn")<.5f && raw("synthMacro2")<.5f && !page.currentPresetEdited(),
              "Out-of-range values are not held at the limits, a kept parameter was written, or an unnamed parameter kept its value");}
     // Delete: a second press; the sound stays; built-in presets cannot be deleted.
     click("synthPresetPrev");require(page.currentPresetName()=="My Bass","Previous from Wild is not My Bass");
     writes.changes.clear();click("synthPresetDelete");
     require(file.existsAsFile() && static_cast<juce::Button*>(page.control("synthPresetDelete"))->getButtonText()=="SURE?","DELETE removed a preset without asking");
     click("synthPresetDelete");
     require(!file.existsAsFile() && page.presetsListed()==factoryCount+2 && page.currentPresetName().isEmpty() && name->getButtonText()=="PRESETS" && writes.changes.empty() && std::abs(raw("synthCutoff")-1234)<.5f,"The second press did not delete the preset, or changed the sound");
     page.applyPatch(1);click("synthPresetDelete");click("synthPresetDelete");
     require(page.presetsListed()==factoryCount+2 && page.currentPresetName()=="Dvergr Analog" && status().isNotEmpty(),"A built-in preset was deleted");
     // Closing the browser brings the envelopes back.
     click("synthPreset");require(!page.presetsShown() && shown("synthAmpAttack") && shown("synthModAdd") && !shown("synthPresetList"),"Closing the browser does not bring the envelopes and matrix back");
     click("synthFxView");click("synthPreset");require(page.presetsShown() && !page.effectsShown() && !shown("synthFxChorusOn") && !static_cast<juce::Button*>(page.control("synthFxView"))->getToggleState(),"The browser and the effect rack show together");
     click("synthFxView");require(page.effectsShown() && !page.presetsShown() && !shown("synthPresetList") && !name->getToggleState(),"FX does not replace the browser");
     page.showEffects(false);page.applyPatch(0);put("synthLevel",0);put("synthOn",0);}
    // Optional evidence: SYNTH_PAGE_SNAPSHOT=<directory> writes the page, with a sample in osc B.
    if(const char* dir=std::getenv("SYNTH_PAGE_SNAPSHOT")){
        const auto wav=juce::File::createTempFile(".wav");
        {juce::AudioBuffer<float> b(1,48000);for(int n=0;n<48000;++n)b.setSample(0,n,float(.6*std::exp(-n/12000.0)*std::sin(2*pi*196.0*n/48000)*(1+.3*std::sin(2*pi*3.0*n/48000))));
         juce::WavAudioFormat format;auto stream=wav.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> w(format.createWriterFor(stream.get(),48000,1,24,{},0));stream.release();w->writeFromAudioSampleBuffer(b,0,48000);}
        require(p->loadSynthSample(1,wav).wasOk(),"Snapshot sample did not load");wav.deleteFile();
        {auto put=[&](const char* id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
         put("synthOscATable",9);put("synthOscAWave",1.6f);put("synthOscAWarp",float(synthWarpBendUp));put("synthOscAWarpAmount",35);
         put("synthOscCOn",1);put("synthOscCTable",3);put("synthOscCWave",2.4f);
         put("synthFilter2On",1);put("synthFilter2Type",6);put("synthFilter2Cutoff",440);put("synthFilter2Resonance",60);put("synthFilterRouting",1);put("synthFilterBalance",35);
         put("synthDistOn",1);put("synthDistType",2);put("synthDistPlace",1);put("synthDistDrive",45);put("synthDistMix",80);}
        page.assignModulation(2,targetWaveA);page.assignModulation(1,targetCutoff);page.assignModulation(modSeq,targetCutoff2);page.assignModulation(modMacro1,targetDistDrive);
        {auto put=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};put(synthModBID(1,"Amount"),-40);put(synthModBID(2,"Amount"),70);
         static const int pattern[]{100,0,-60,0,80,40,0,-100,60,0,0,30,-30,90,0,-50};for(int k=0;k<synthSeqMaxSteps;++k)put(synthSeqStepID(k),float(pattern[k]));put("synthSeqSteps",12);put("synthSeqSmooth",20);put("synthMacro1",65);}
        if(const char* tab=std::getenv("SYNTH_PAGE_TAB"))page.showModTab(std::atoi(tab));else page.showModTab(1);
        if(std::getenv("SYNTH_PAGE_FX")){
            auto put=[&](const char* id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
            for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{{"synthFxChorusOn",1},{"synthFxDelayOn",1},{"synthFxDelaySync",5},{"synthFxDelayFeedback",45},{"synthFxDelayPingPong",1},{"synthFxReverbOn",1},{"synthFxReverbDecay",3.5f},
                    {"synthFxEqOn",1},{"synthFxEqLow",3},{"synthFxEqHigh",-2}})put(id,v);
            page.showEffects(true);}
        if(std::getenv("SYNTH_PAGE_IMPORT")){   // an imported table in osc A: 32 frames from a sine to a folded sine
            const auto table=juce::File::createTempFile(".wav");
            {juce::AudioBuffer<float> b(1,32*2048);for(int k=0;k<32;++k)for(int n=0;n<2048;++n)b.setSample(0,k*2048+n,float(std::sin((1.0+k*.35)*std::sin(2*pi*n/2048)*pi*.5)));
             juce::WavAudioFormat format;auto stream=table.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> w(format.createWriterFor(stream.get(),48000,1,24,{},0));stream.release();w->writeFromAudioSampleBuffer(b,0,b.getNumSamples());}
            const auto named=table.getSiblingFile("Fold Sweep.wav");table.moveFileTo(named);
            require(page.importTable(0,named).wasOk(),"Snapshot table not imported");named.deleteFile();}
        juce::File presetDir;
        if(std::getenv("SYNTH_PAGE_PRESETS")){
            presetDir=juce::File::createTempFile("synthSnapshotPresets");presetDir.deleteRecursively();
            static const std::pair<const char*,const char*> names[]{{"Glass Pad","Pad"},{"Iron Bass","Bass"},{"Night Lead","Lead"},{"Rune Pluck","Pluck"},{"Shimmer Keys","Keys"},{"Storm Sweep","FX"}};
            for(const auto& [name,kind]:names)require(spectralforge::synthPresets::write(presetDir,spectralforge::synthPresets::capture(*p,name,kind)).wasOk(),"Snapshot preset not written");
            page.setPresetDirectory(presetDir);page.showPresets(true);
            {int row=-1;for(int i=0;i<page.presetsListed();++i)if(page.presetNameAt(i)=="Iron Bass")row=i;static_cast<juce::ListBox*>(page.control("synthPresetList"))->selectRow(row);}
            {auto* q=p->parameters().getParameter("synthResonance");q->setValueNotifyingHost(.4f);}(void)page.currentPresetEdited();}
        page.refreshModulation();
        page.updateModes();page.resized();
        for(const float scale:{1.f,1.5f}){
            juce::Image image(juce::Image::ARGB,int(page.getWidth()*scale),int(page.getHeight()*scale),true);
            {juce::Graphics g(image);g.fillAll(juce::Colour(0xff111616));g.drawImageAt(page.createComponentSnapshot(page.getLocalBounds(),true,scale),0,0);}   // on the editor's dark background
            auto out=juce::File(dir).getChildFile("synth_page_"+juce::String(int(scale*100))+".png");out.deleteFile();
            auto stream=out.createOutputStream();require(stream && juce::PNGImageFormat().writeImageToStream(image,*stream),"Cannot write the SYNTH page snapshot");
        }
        if(presetDir!=juce::File()){page.setPresetDirectory(juce::File::createTempFile("none"));presetDir.deleteRecursively();}
    }
    // Filters and distortion (docs/SYNTH_FILTERS.md): thirteen parameters at version hint 39, appended last.
    {int count=0;const int total=p->getParameters().size()-synthOscCount;   // the import switches (hint 42) follow
     for(auto* parameter:p->getParameters())if(auto* with=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))if(with->getVersionHint()==synthFiltersParameterVersion){
        require(with->getParameterIndex()>=total-synthFiltersParameterCount-synthSequencerParameterCount-synthEffectsParameterCount && with->getParameterIndex()<total-synthSequencerParameterCount-synthEffectsParameterCount && (with->paramID.startsWith("synthFilter")||with->paramID.startsWith("synthDist")),"A filter 2 / distortion parameter is not in the last bank");++count;}
     const auto initial=[&](const char* id){auto* q=p->parameters().getParameter(id);return q->convertFrom0to1(q->getDefaultValue());};
     require(count==synthFiltersParameterCount && initial("synthFilter2On")==0 && initial("synthDistOn")==0 && initial("synthFilterRouting")==0 && initial("synthDistMix")==100,"Filter 2 / distortion bank size or defaults wrong");
     const std::vector<const char*> ids{"synthFilterOn","synthFilterType","synthCutoff","synthResonance","synthDrive","synthKeyTrack","synthFilterEnv","synthFilter2On","synthFilter2Type","synthFilter2Cutoff",
         "synthFilter2Resonance","synthFilter2KeyTrack","synthFilter2Env","synthFilterRouting","synthFilterBalance","synthDistOn","synthDistType","synthDistPlace","synthDistDrive","synthDistMix"};
     const auto put=[&](const char* id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
     put("synthFilterRouting",0);page.updateModes();require(page.control("synthFilterBalance")!=nullptr && !page.control("synthFilterBalance")->isVisible(),"The parallel balance shows in Serial");
     put("synthFilterRouting",1);page.updateModes();require(page.control("synthFilterBalance")->isVisible(),"The parallel balance is hidden in Parallel");
     const auto area=[&](juce::Component* c){auto* top=c;while(top->getParentComponent()!=&page)top=top->getParentComponent();return top->getBounds();};   // a knob's slider sits in its Knob
     std::vector<juce::Rectangle<int>> boxes{curve.getBounds()};
     for(const auto* id:ids){auto* c=page.control(id);require(c!=nullptr && c->isVisible(),std::string("SYNTH page control missing: ")+id);boxes.push_back(area(c));}
     for(size_t i=0;i<boxes.size();++i){require(page.getLocalBounds().contains(boxes[i]) && boxes[i].getWidth()>=30 && boxes[i].getHeight()>=16,"A filter-panel control is off the page or too small");
        for(size_t j=i+1;j<boxes.size();++j)require(!boxes[i].intersects(boxes[j]),std::string("Filter-panel controls overlap: ")+(i?ids[i-1]:"curve")+" / "+ids[j-1]);}
     require(curve.getHeight()>=56,"The filter curve is too small");
     // The curve: with filter 2 on, a press near its handle moves filter 2 and leaves filter 1.
     put("synthFilter2On",1);put("synthCutoff",6000);put("synthResonance",20);put("synthFilter2Cutoff",200);put("synthFilter2Resonance",20);
     const auto h2=curve.handle(1);writes.gestures=0;writes.changes.clear();drag(curve,h2,{h2.x+40,h2.y});
     std::cout<<"MEASURE filter panel: curve "<<curve.getWidth()<<" x "<<curve.getHeight()<<" px; dragging filter 2's handle: cutoff 2 "<<raw("synthFilter2Cutoff")<<" Hz, cutoff 1 "<<raw("synthCutoff")<<" Hz, gestures "<<writes.gestures<<"\n";
     require(raw("synthFilter2Cutoff")>250 && std::abs(raw("synthCutoff")-6000)<1 && writes.gestures==4 && writes.changes.count(index("synthCutoff"))==0,"Dragging filter 2's handle is wrong");
     put("synthFilter2On",0);put("synthFilterRouting",0);put("synthFilter2Cutoff",200);put("synthFilter2Resonance",20);page.updateModes();}
    std::cout<<"MEASURE page: square tile -> wave "<<squareValue<<", display drag to the saw tile -> "<<morphed<<"\n";
    p->removeListener(&writes);
    std::cout<<"PASS inserts in the processor and the path, SYNTH page controls, graph drags as host gestures, patches\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {tracker();tuningTest();waveMorph();unisonSubNoise();wavetables();wavetableImport();filterResponse();filtersAndDistortion();envelopes();lfoAndMatrix();sequencerAndMacros();effects();fmAndVoices();guitarAndMidi();oscillatorC();engineTypes();sampleOscillator();sampleState();reviewFixes();contractAndPatches();presetFiles();processorInsertsAndPage();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
