// External sidechain and SYNTH bus (docs/SIDECHAIN_SYNTH_BUS.md): the bus
// layout contract, each keyed detector (clean compressor, the gates before
// and after the rig, multiband, POST Console VCA), the fallback without a
// sidechain, the SYNTH output choice, shared sidechain / SYNTH bus channels,
// bypass, and allocation in the callback.
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "PostNativeDSP.h"
#include <array>
#include <cmath>
#include <complex>
#include <functional>
#include <cstdlib>
#include <iostream>
#include <limits>
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
constexpr double rate=48000,pi=3.14159265358979323846;constexpr int block=256;
void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
double db(double v){return 20*std::log10(std::max(v,1.e-30));}
using Signal=std::function<float(int)>;
Signal sine(double amplitude,double hz){return [=](int n){return float(amplitude*std::sin(2*pi*hz*n/rate));};}
Signal silent(){return [](int){return 0.f;};}

struct Rig {
    std::unique_ptr<ChimeraProcessor> p;
    Rig(bool sidechain,bool synthBus,bool monoSidechain=false):p(std::make_unique<ChimeraProcessor>()) {
        auto layout=p->getBusesLayout();
        require(layout.inputBuses.size()==2 && layout.outputBuses.size()==2,"Sidechain or SYNTH bus missing");
        layout.inputBuses.set(1,sidechain?(monoSidechain?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo()):juce::AudioChannelSet::disabled());
        layout.outputBuses.set(1,synthBus?juce::AudioChannelSet::stereo():juce::AudioChannelSet::disabled());
        require(p->setBusesLayout(layout),"Layout rejected");
        p->setRateAndBufferSizeDetails(rate,block);p->prepareToPlay(rate,block);
    }
    void set(const char* id,float v){auto* q=p->parameters().getParameter(id);require(q!=nullptr,std::string("no parameter ")+id);q->setValueNotifyingHost(q->convertTo0to1(v));}
    // Runs `seconds`; returns the main left output and the SYNTH bus left output.
    struct Out {std::vector<float> main,bus;};
    Out run(double seconds,const Signal& input,const Signal& key,std::function<void(int,juce::MidiBuffer&)> midi={},bool bypassed=false) {
        const int channels=std::max(p->getTotalNumInputChannels(),p->getTotalNumOutputChannels());
        juce::AudioBuffer<float> b(channels,block);Out out;
        const bool hasKey=p->sidechainEnabled(),hasBus=p->synthBusEnabled();
        const int keyFirst=hasKey?p->getChannelIndexInProcessBlockBuffer(true,1,0):-1,keyCount=hasKey?p->getChannelCountOfBus(true,1):0;
        const int busFirst=hasBus?p->getChannelIndexInProcessBlockBuffer(false,1,0):-1;
        for(int k=0;k*block<int(seconds*rate);++k){
            b.clear();
            for(int n=0;n<block;++n){const float x=input(k*block+n);b.setSample(0,n,x);b.setSample(1,n,x);
                for(int c=0;c<keyCount;++c)b.setSample(keyFirst+c,n,key(k*block+n));}
            juce::MidiBuffer m;if(midi)midi(k,m);
            if(bypassed)p->processBlockBypassed(b,m);else {countAllocations=true;p->processBlock(b,m);countAllocations=false;}
            for(int n=0;n<block;++n){out.main.push_back(b.getSample(0,n));out.bus.push_back(hasBus?b.getSample(busFirst,n):0.f);}
        }
        return out;
    }
};
double rms(const std::vector<float>& x,double from,double to){double e=0;const int a=int(from*rate),z=std::min(int(x.size()),int(to*rate));for(int n=a;n<z;++n)e+=double(x[size_t(n)])*x[size_t(n)];return std::sqrt(e/std::max(1,z-a));}

void layouts() {
    auto owner=std::make_unique<ChimeraProcessor>();auto& p=*owner;   // heap: larger than a Windows stack
    const auto mono=juce::AudioChannelSet::mono(),stereo=juce::AudioChannelSet::stereo(),off=juce::AudioChannelSet::disabled();
    auto layout=p.getBusesLayout();
    require(layout.getChannelSet(true,1)==off && layout.getChannelSet(false,1)==off,"The optional buses must start disabled");
    const auto test=[&](juce::AudioChannelSet in,juce::AudioChannelSet out,juce::AudioChannelSet side,juce::AudioChannelSet synth){
        auto l=p.getBusesLayout();l.inputBuses.set(0,in);l.outputBuses.set(0,out);l.inputBuses.set(1,side);l.outputBuses.set(1,synth);return p.isBusesLayoutSupported(l);};
    int accepted=0;
    for(const auto& main:{mono,stereo})for(const auto& side:{off,mono,stereo})for(const auto& synth:{off,stereo}){require(test(main,main,side,synth),"A supported layout was rejected");++accepted;}
    require(!test(stereo,stereo,off,mono),"Mono SYNTH bus accepted");
    require(!test(stereo,mono,stereo,stereo) && !test(mono,stereo,off,off),"Asymmetric main bus accepted");
    std::cout<<"MEASURE layouts: "<<accepted<<" accepted (main mono / stereo x sidechain off / mono / stereo x SYNTH bus off / stereo); mono SYNTH bus and asymmetric main rejected\n";
    std::cout<<"PASS bus layouts: both optional buses start off, the accepted set and the rejections\n";
}

void compressorKey() {
    // Main -30 dBFS at 220 Hz under a threshold of 0 dBFS (ratio 20); the key is +12 dBFS.
    const auto measure=[](bool sidechain,bool keyOn){
        Rig r(sidechain,false);r.set("cleanCompOn",1);r.set("cleanCompThreshold",0);r.set("cleanCompRatio",20);r.set("cleanCompAttack",1);r.set("cleanCompKnee",0);r.set("scKeyComp",keyOn?1.f:0.f);
        return rms(r.run(1.5,sine(.0316,220),sine(4,220)).main,.5,1.5);};
    const double none=measure(false,false),off=measure(true,false),keyed=measure(true,true),fallback=measure(false,true);
    std::cout<<"MEASURE clean compressor, main -30 dBFS, key +12 dBFS: no sidechain "<<db(none)<<" dBFS; key off "<<db(off)<<"; EXT KEY "<<db(keyed)<<" ("<<db(keyed/none)<<" dB); EXT KEY without a sidechain "<<db(fallback)<<"\n";
    require(std::abs(db(off/none))<.01 && std::abs(db(fallback/none))<.01 && db(keyed/none)<-8,"Clean compressor key wrong");
    std::cout<<"PASS clean compressor: the key compresses, the switch alone or the sidechain alone changes nothing\n";
}

void gateKeys() {
    // Main -30 dBFS opens a -65 dBFS gate; a silent key closes it; a loud key opens it for a silent-feeling input.
    std::string line;
    for(const bool afterRig:{false,true}){
        const auto measure=[&](bool keyOn,const Signal& key){
            Rig r(true,false);r.set("gateAfterRig",afterRig?1.f:0.f);r.set("scKeyGates",keyOn?1.f:0.f);
            return rms(r.run(1.5,sine(.0316,220),key).main,.75,1.5);};
        const double open=measure(false,silent()),closed=measure(true,silent()),held=measure(true,sine(.3,220));
        line+=std::string(afterRig?" after the rig":" before the rig")+": open "+juce::String(db(open),1).toStdString()+", silent key "+juce::String(db(closed),1).toStdString()+", -10 dBFS key "+juce::String(db(held),1).toStdString()+" dBFS;";
        require(db(closed/open)<-40 && std::abs(db(held/open))<.5,"Gate key wrong");
    }
    // The Expander detects from the key too. It is placed after the rig: before
    // it, the amp's own compression would hide its 24 dB range.
    Rig e(true,false);e.set("gateon",0);e.set("expOn",1);e.set("expAfterRig",1);e.set("expThreshold",-40);e.set("scKeyGates",1);
    const double expanded=rms(e.run(1.5,sine(.0316,220),silent()).main,.75,1.5);
    Rig o(true,false);o.set("gateon",0);o.set("expOn",1);o.set("expAfterRig",1);o.set("expThreshold",-40);
    const double expanderOpen=rms(o.run(1.5,sine(.0316,220),silent()).main,.75,1.5);
    std::cout<<"MEASURE gates, main -30 dBFS:"<<line<<" Expander after the rig (-40 dBFS, range 24 dB) "<<db(expanderOpen)<<" -> "<<db(expanded)<<" dBFS with a silent key\n";
    require(db(expanded/expanderOpen)<-20,"Expander does not follow the key");
    std::cout<<"PASS gate before and after the rig and the Expander follow the key\n";
}

void multibandKey() {
    const auto measure=[](bool keyOn){
        Rig r(true,false);r.set("mbOn",1);r.set("scKeyMultiband",keyOn?1.f:0.f);
        for(int band=0;band<4;++band){r.set(spectralforge::multibandID(band,"threshold").toRawUTF8(),-10);r.set(spectralforge::multibandID(band,"ratio").toRawUTF8(),20);r.set(spectralforge::multibandID(band,"range").toRawUTF8(),36);}
        // Key: +12 dBFS at 1.5 kHz (band 3 only); main: 1.5 kHz and 220 Hz at -30 dBFS each.
        const auto out=r.run(1.5,[](int n){return float(.0316*(std::sin(2*pi*1500*n/rate)+std::sin(2*pi*220*n/rate)));},sine(4,1500)).main;
        const auto band=[&](double hz){std::complex<double> s;for(int n=int(.5*rate);n<int(1.5*rate);++n)s+=double(out[size_t(n)])*std::polar(1.0,-2*pi*hz*n/rate);return std::abs(s);};
        return std::pair<double,double>{band(1500),band(220)};};
    const auto plain=measure(false),keyed=measure(true);
    const double high=db(keyed.first/plain.first),low=db(keyed.second/plain.second);
    std::cout<<"MEASURE multiband, key +12 dBFS at 1.5 kHz: 1.5 kHz "<<high<<" dB, 220 Hz "<<low<<" dB\n";
    require(high<-10 && std::abs(low)<1,"Multiband key does not act on its own band only");
    std::cout<<"PASS multiband: each band follows the same band of the key\n";
}

void consoleVcaKey() {
    // POST Console VCA with EXTERNAL SIDE-CHAIN: -40 dBFS signal under its threshold, a 0 dBFS key.
    const auto measure=[](bool external,bool key){
        spectralforge::PostNativeDSP dsp;dsp.prepare({rate,block,2});auto state=spectralforge::defaultPostNativeState();
        auto& section=state.sections[0];section.selected=0;section.banks[0].bypass=false;section.banks[0].values[6]=external?1.f:0.f;
        juce::AudioBuffer<float> b(2,block),k(2,block);double e=0;
        for(int i=0;i<int(rate)/block;++i){
            for(int n=0;n<block;++n){const float x=float(.01*std::sin(2*pi*220*(i*block+n)/rate));for(int c=0;c<2;++c){b.setSample(c,n,x);k.setSample(c,n,float(std::sin(2*pi*220*(i*block+n)/rate)));}}
            dsp.setKey(key?&k:nullptr);dsp.processSection(b,0,section);
            if(i>int(rate)/block/2)for(int n=0;n<block;++n)e+=double(b.getSample(0,n))*b.getSample(0,n);
        }
        return std::sqrt(e);};
    const double internal=measure(false,true),keyed=measure(true,true),noKey=measure(true,false);
    std::cout<<"MEASURE Console VCA, -40 dBFS signal, 0 dBFS key: EXTERNAL SIDE-CHAIN "<<db(keyed/internal)<<" dB re internal; switch on without a key "<<db(noKey/internal)<<" dB\n";
    require(db(keyed/internal)<-6 && std::abs(db(noKey/internal))<.01,"Console VCA external sidechain wrong");
    std::cout<<"PASS POST Console VCA EXTERNAL SIDE-CHAIN listens to the key, and without one to its input\n";
}

void synthBus() {
    const auto a3=[](int k,juce::MidiBuffer& m){if(k==2)m.addEvent(juce::MidiMessage::noteOn(1,57,1.f),0);};
    std::string line;std::array<std::pair<double,double>,3> levels{};
    for(int output=0;output<3;++output){
        Rig r(false,true);r.set("synthOn",1);r.set("synthSource",2);r.set("synthOutput",float(output));
        const auto out=r.run(1.0,silent(),silent(),a3);levels[size_t(output)]={rms(out.main,.3,1),rms(out.bus,.3,1)};
        line+=" "+spectralforge::synthOutputNames()[output].toStdString()+": main "+juce::String(db(levels[size_t(output)].first),1).toStdString()+" / bus "+juce::String(db(levels[size_t(output)].second),1).toStdString()+" dBFS;";
    }
    // Without a SYNTH bus, "Synth bus" stays in the mix.
    Rig f(false,false);f.set("synthOn",1);f.set("synthSource",2);f.set("synthOutput",1);const double fallback=rms(f.run(1.0,silent(),silent(),a3).main,.3,1);
    // Both buses on: the SYNTH bus carries the synth, never the key sharing its channels.
    Rig both(true,true);both.set("synthOutput",1);const auto shared=both.run(1.0,silent(),sine(.5,440));
    double leaked=0;for(float x:shared.bus)leaked=std::max(leaked,double(std::abs(x)));
    // Bypassed: the SYNTH bus is silent.
    Rig b(false,true);b.set("synthOn",1);b.set("synthSource",2);b.set("synthOutput",2);b.run(.5,silent(),silent(),a3);
    const auto bypassed=b.run(.3,silent(),silent(),{},true);double bypassPeak=0;for(float x:bypassed.bus)bypassPeak=std::max(bypassPeak,double(std::abs(x)));
    std::cout<<"MEASURE SYNTH output, silent guitar + MIDI A3:"<<line<<" no SYNTH bus + Synth bus: main "<<db(fallback)<<" dBFS; key on the shared channels, synth off: bus peak "<<leaked<<"; bypassed bus peak "<<bypassPeak<<"\n";
    require(levels[0].first>1e-3 && levels[0].second==0,"Main mix: the synth must stay off its bus");
    require(levels[1].first<1e-5 && levels[1].second>1e-3,"Synth bus: the synth must leave the mix");
    require(levels[2].first>1e-3 && levels[2].second>1e-3,"Main + synth bus: both");
    require(fallback>1e-3 && leaked==0 && bypassPeak==0,"Fallback, shared channels or bypass wrong");
    std::cout<<"PASS SYNTH output choice, fallback without a bus, shared channels, bypass\n";
}

// Review fixes (10/30): a non-finite key sample does not break the keyed
// compressor; the SYNTH bus is delayed by the reported latency like the
// main output.
void reviewFixes() {
    Rig r(true,false);r.set("cleanCompOn",1);r.set("cleanCompThreshold",0);r.set("cleanCompRatio",20);r.set("cleanCompAttack",1);r.set("cleanCompKnee",0);r.set("scKeyComp",1);
    const int bad=int(.2*rate);
    const auto key=[bad](int n){return n==bad?std::numeric_limits<float>::quiet_NaN():n==bad+1?std::numeric_limits<float>::infinity():float(4*std::sin(2*pi*220*n/rate));};
    const auto out=r.run(1.5,sine(.0316,220),key).main;
    const double keyed=rms(out,.6,1.5);bool finite=true;for(float x:out)finite=finite&&std::isfinite(x);
    Rig clean(true,false);for(const auto* id:{"cleanCompOn","scKeyComp"})clean.set(id,1);clean.set("cleanCompThreshold",0);clean.set("cleanCompRatio",20);clean.set("cleanCompAttack",1);clean.set("cleanCompKnee",0);
    const double reference=rms(clean.run(1.5,sine(.0316,220),sine(4,220)).main,.6,1.5);
    Rig b(false,true);b.set("synthOn",1);b.set("synthSource",2);b.set("synthOutput",2);
    const auto played=b.run(.5,silent(),silent(),[](int k,juce::MidiBuffer& m){if(k==2)m.addEvent(juce::MidiMessage::noteOn(1,57,1.f),0);});
    int onset=-1;for(size_t n=0;n<played.bus.size();++n)if(std::abs(played.bus[n])>1e-7f){onset=int(n);break;}
    const int latency=b.p->getLatencySamples();
    std::cout<<"MEASURE review fixes: keyed compressor after a NaN and an Inf key sample "<<db(keyed)<<" dBFS (clean key "<<db(reference)<<"); SYNTH bus onset "<<onset-2*block<<" samples after the note, latency "<<latency<<"\n";
    require(finite && std::abs(db(keyed/reference))<.1,"A non-finite key sample broke the keyed compressor");
    require(latency>0 && std::abs(onset-2*block-latency)<=1,"The SYNTH bus is not delayed by the reported latency");
    std::cout<<"PASS non-finite key samples are ignored; the SYNTH bus follows the reported latency\n";
}

void callback() {
    // Every key switch and both buses on, with a key and MIDI: no allocations after the first block.
    Rig r(true,true);for(const auto* id:{"scKeyComp","scKeyGates","scKeyMultiband","cleanCompOn","mbOn","synthOn"})r.set(id,1);r.set("synthOutput",2);r.set("synthSource",0);
    r.run(.2,sine(.1,110),sine(.5,220));allocations=0;
    const auto out=r.run(1.0,sine(.1,110),sine(.5,220),[](int k,juce::MidiBuffer& m){if(k%20==0)m.addEvent(juce::MidiMessage::noteOn(1,60+k%12,1.f),0);});
    bool finite=true;for(size_t n=0;n<out.main.size();++n)finite=finite&&std::isfinite(out.main[n])&&std::isfinite(out.bus[n]);
    // A mono sidechain keys as well.
    Rig m(true,false,true);m.set("scKeyGates",1);const double monoClosed=rms(m.run(1.5,sine(.0316,220),silent()).main,.75,1.5);
    std::cout<<"MEASURE callback with every key and both buses: "<<allocations<<" allocations; mono sidechain, silent key: "<<db(monoClosed)<<" dBFS\n";
    require(allocations==0 && finite && db(monoClosed)<-60,"Callback allocates, is not finite, or a mono sidechain does not key");
    std::cout<<"PASS allocation-free callback with both buses; mono sidechain\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {layouts();compressorKey();gateKeys();multibandKey();consoleVcaKey();synthBus();reviewFixes();callback();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
