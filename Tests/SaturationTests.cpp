// Three-band saturation (docs/SATURATION_1_6.md): exact Off / Mix 0 / Clean,
// small-signal transparency, clean low band, type harmonics, band isolation,
// click- and dip-free switching, Studio 4x aliasing and latency, processor
// parameters and no allocation.
#include "PluginProcessor.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
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

namespace {
using namespace spectralforge;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
constexpr double rate=48000.;
// A host-free parameter state for the processor class.
class Host : public juce::AudioProcessor {
public:
    Host():juce::AudioProcessor(BusesProperties().withInput("In",juce::AudioChannelSet::stereo()).withOutput("Out",juce::AudioChannelSet::stereo())),state(*this,nullptr,"S",layout()) {saturation.bind(state);}
    static juce::AudioProcessorValueTreeState::ParameterLayout layout(){juce::AudioProcessorValueTreeState::ParameterLayout l;appendSaturationParameters(l);return l;}
    void set(const juce::String& id,float v){auto* p=state.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(v));}
    const juce::String getName() const override {return "S";}
    void prepareToPlay(double,int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override {return 0;}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    juce::AudioProcessorEditor* createEditor() override {return nullptr;}
    bool hasEditor() const override {return false;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return {};}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*,int) override {}
    juce::AudioProcessorValueTreeState state;
    ThreeBandSaturation saturation;
};
std::vector<std::vector<float>> tones(const std::vector<std::pair<double,double>>& parts,double seconds) {   // (hz, dBFS)
    const size_t n=size_t(seconds*rate);std::vector<std::vector<float>> x(2,std::vector<float>(n));
    for(size_t i=0;i<n;++i){double v=0;for(auto [hz,db]:parts)v+=std::pow(10.,db/20.)*std::sin(juce::MathConstants<double>::twoPi*hz*double(i)/rate);x[0][i]=x[1][i]=float(v);}
    return x;
}
std::vector<std::vector<float>> run(Host& h,std::vector<std::vector<float>> x,int block=256) {
    h.saturation.prepare(rate,block);juce::AudioBuffer<float> b(2,block);
    for(size_t off=0;off<x[0].size();off+=size_t(block)){
        const int n=int(std::min<size_t>(size_t(block),x[0].size()-off));juce::AudioBuffer<float> part(2,n);
        for(int c=0;c<2;++c)part.copyFrom(c,0,x[size_t(c)].data()+off,n);
        countAllocations=true;h.saturation.process(part);countAllocations=false;
        for(int c=0;c<2;++c)std::copy(part.getReadPointer(c),part.getReadPointer(c)+n,x[size_t(c)].begin()+long(off));
    }
    return x;
}
double toneDb(const std::vector<float>& x,double hz,size_t start) {   // whole 0.1 s windows; tones are multiples of 10 Hz
    const size_t window=size_t(rate/10),length=(x.size()-start)/window*window;double re=0,im=0;
    for(size_t i=start;i<start+length;++i){const double w=juce::MathConstants<double>::twoPi*hz*double(i)/rate;re+=x[i]*std::cos(w);im+=x[i]*std::sin(w);}
    return 20*std::log10(2*std::sqrt(re*re+im*im)/double(length)+1e-15);
}
void exactWhenNeutral() {
    const auto input=tones({{100,-6},{1000,-6},{6000,-12}},.5);
    {Host h;require(run(h,input)==input,"Off is not sample-exact");}
    // On, Mix 0 and all-Clean keep every level; only the phase turns.
    double worst=0;
    for(int variant=0;variant<2;++variant){Host h;h.set("satOn",1);
        if(variant==0)h.set("satMix",0);else for(int b=0;b<3;++b)h.set(saturationID(b,"type"),0);
        const auto out=run(h,input);for(const double hz:{100.,1000.,6000.})worst=std::max(worst,std::abs(toneDb(out[0],hz,12000)-toneDb(input[0],hz,12000)));}
    std::cout<<"MEASURE on with Mix 0 / all bands Clean: largest level change "<<worst<<" dB (phase rotated around the crossovers)\n";
    require(worst<.01,"Mix 0 or Clean bands change a level");
    std::cout<<"PASS Off is sample-exact; Mix 0 and all-Clean keep every level\n";
}
void smallSignalAndCleanLow() {
    // Unity slope: a quiet signal passes almost unchanged at full drive.
    {Host h;h.set("satOn",1);for(int b=0;b<3;++b){h.set(saturationID(b,"type"),float(1+b%3));h.set(saturationID(b,"drive"),36);}
     const auto input=tones({{100,-80},{1000,-80},{6000,-80}},.5);const auto out=run(h,input);
     double worst=0;for(const double hz:{100.,1000.,6000.})worst=std::max(worst,std::abs(toneDb(out[0],hz,24000)-toneDb(input[0],hz,24000)));
     std::cout<<"MEASURE saturation at -80 dBFS, 36 dB drive, every band saturating: largest level change "<<worst<<" dB\n";
     require(worst<.05,"Saturation is not transparent to small signals");}
    // Clean low band: a loud 60 Hz under a driven mid keeps its level exactly.
    {Host h;h.set("satOn",1);h.set(saturationID(0,"type"),0);h.set(saturationID(1,"type"),2);h.set(saturationID(1,"drive"),30);h.set(saturationID(2,"type"),0);
     const auto input=tones({{60,-6},{1000,-6}},1.);const auto out=run(h,input);
     const double low=toneDb(out[0],60,24000)-toneDb(input[0],60,24000),mid=toneDb(out[0],1000,24000)-toneDb(input[0],1000,24000),second=toneDb(out[0],2000,24000)-toneDb(out[0],1000,24000);
     std::cout<<"MEASURE Clean low with a Tube mid at 30 dB drive: 60 Hz "<<low<<" dB, 1 kHz "<<mid<<" dB, its 2nd harmonic "<<second<<" dB re 1 kHz\n";
     // 60 Hz leaks into the mid band at -42 dB (LR4 at 200 Hz); compressing the
     // mid removes that share, about 0.07 dB.
     require(std::abs(low)<.1 && mid<-10 && second>-45,"The clean low band is not preserved, or the mid is not saturated");}
    std::cout<<"PASS small-signal transparency and clean low band\n";
}
void typesAndDrive() {
    // Harmonics re fundamental at 1 kHz, mid band only.
    const auto measure=[](int type,float drive,double& even,double& odd){
        Host h;h.set("satOn",1);h.set(saturationID(0,"type"),0);h.set(saturationID(2,"type"),0);h.set(saturationID(1,"type"),float(type));h.set(saturationID(1,"drive"),drive);
        h.set("satCrossover2",12000);const auto out=run(h,tones({{1000,-12}},.6));
        const double f=toneDb(out[0],1000,9600);even=toneDb(out[0],2000,9600)-f;odd=toneDb(out[0],3000,9600)-f;};
    double e[4][2],o[4][2];
    for(int type=1;type<=3;++type)for(int d=0;d<2;++d)measure(type,d?30.f:6.f,e[type][d],o[type][d]);
    std::cout<<"MEASURE harmonics re 1 kHz (-12 dBFS) at 6 / 30 dB drive: Tape 2nd "<<e[1][0]<<"/"<<e[1][1]<<", 3rd "<<o[1][0]<<"/"<<o[1][1]
             <<"; Tube 2nd "<<e[2][0]<<"/"<<e[2][1]<<", 3rd "<<o[2][0]<<"/"<<o[2][1]<<"; Clip 2nd "<<e[3][0]<<"/"<<e[3][1]<<", 3rd "<<o[3][0]<<"/"<<o[3][1]<<" dB\n";
    for(int type=1;type<=3;++type)require(o[type][1]>o[type][0]+10,"Drive does not add harmonics");
    require(e[2][1]>e[1][1]+30 && e[2][1]>e[3][1]+30 && e[2][1]>-40,"Only Tube should add even harmonics");
    std::cout<<"PASS drive adds harmonics; Tube is the asymmetric (even-harmonic) type\n";
}
void isolationAndSwitching() {
    // A 6 kHz tone in a Clean high band is untouched by a heavily driven mid.
    {Host h;h.set("satOn",1);h.set(saturationID(0,"type"),0);h.set(saturationID(2,"type"),0);h.set(saturationID(1,"type"),3);h.set(saturationID(1,"drive"),36);
     const auto input=tones({{6000,-6}},.5);const auto out=run(h,input);
     const double change=toneDb(out[0],6000,12000)-toneDb(input[0],6000,12000);
     std::cout<<"MEASURE 6 kHz in a Clean high band, mid Clip at 36 dB drive: "<<change<<" dB, 18 kHz "<<toneDb(out[0],18000,12000)-toneDb(input[0],6000,12000)<<" dB re tone\n";
     require(std::abs(change)<.1,"A Clean band is changed by the neighbour's saturation");}
    // Switching on and off with all bands Clean on a 2.5 kHz tone, where the
    // band sum is in antiphase with the input: a plain crossfade would cancel
    // it; the all-pass morph keeps the level. Then with a driven band.
    const auto switching=[&](bool driven,double& low,double& high,double& before,double& after){
        Host h;for(int b=0;b<3;++b)h.set(saturationID(b,"type"),driven&&b==1?3.f:0.f);h.set(saturationID(1,"drive"),36);
        auto x=tones({{2500,-6}},1.);h.saturation.prepare(rate,256);std::vector<float> out;
        for(size_t off=0;off<x[0].size();off+=256){h.set("satOn",off>=size_t(.3*rate)&&off<size_t(.65*rate)?1.f:0.f);
            juce::AudioBuffer<float> part(2,256);for(int c=0;c<2;++c)part.copyFrom(c,0,x[size_t(c)].data()+off,256);
            h.saturation.process(part);out.insert(out.end(),part.getReadPointer(0),part.getReadPointer(0)+256);}
        const auto rms=[&](size_t a,size_t b){double e=0;for(size_t i=a;i<b;++i)e+=double(out[i])*out[i];return std::sqrt(e/double(b-a));};
        before=rms(size_t(.2*rate),size_t(.3*rate));after=rms(size_t(.5*rate),size_t(.6*rate));low=1e9;high=0;
        for(const size_t start:{size_t(.3*rate),size_t(.65*rate)})for(size_t w=start;w<start+size_t(.04*rate);w+=96){low=std::min(low,rms(w,w+96));high=std::max(high,rms(w,w+96));}};
    {double low,high,before,after;switching(false,low,high,before,after);
     std::cout<<"MEASURE switching on/off, Clean bands, 2.5 kHz at the crossover: RMS off "<<before<<", on "<<after<<", 2 ms windows during both changes "<<low<<" to "<<high<<"\n";
     require(low>=before*.94 && high<=before*1.06 && std::abs(after/before-1)<.01,"Switching dips at the crossover");}
    {double low,high,before,after;switching(true,low,high,before,after);
     std::cout<<"MEASURE switching on/off, Clip 36 dB mid: RMS off "<<before<<", on "<<after<<", during the changes "<<low<<" to "<<high<<"\n";
     require(low>=std::min(before,after)*.94 && high<=std::max(before,after)*1.06,"Switching dips or overshoots");}
    std::cout<<"PASS band isolation; switching on moves between the states without a dip\n";
}
void studio() {
    // A 7 kHz tone through Clip at full drive: the 5th harmonic (35 kHz) folds
    // to 13 kHz at the host rate; Studio's 4x keeps it out.
    const auto alias=[](int quality,int& latency){Host h;h.set("satOn",1);h.set("satQuality",float(quality));
        for(int b=0;b<3;++b){h.set(saturationID(b,"type"),3);h.set(saturationID(b,"drive"),24);}
        const auto out=run(h,tones({{7000,-6}},.6));latency=h.saturation.latency();
        return toneDb(out[0],13000,9600)-toneDb(out[0],7000,9600);};
    int liveLatency=0,studioLatency=0;const double live=alias(0,liveLatency),studio=alias(1,studioLatency);
    std::cout<<"MEASURE Clip 24 dB on 7 kHz: 13 kHz alias of the 5th harmonic "<<live<<" dB (Live) / "<<studio<<" dB (Studio 4x) re tone; latency "<<liveLatency<<" / "<<studioLatency<<" samples\n";
    require(liveLatency==0 && studioLatency>0 && studio<live-20,"Studio does not suppress aliasing or report its latency");
    std::cout<<"PASS Studio 4x: aliasing down, latency reported; Live: zero latency\n";
}
void processorParameters() {
    ChimeraProcessor p;int seen=0;const int total=p.getParameters().size();
    const int later=total-spectralforge::referenceAmpParameterCount()-spectralforge::filterModulationParameterCount-spectralforge::postExpansionParameterCount()-spectralforge::effectExtensionParameterCount-spectralforge::creativeParameterCount-(spectralforge::synthParameterCount+spectralforge::sidechainParameterCount+spectralforge::signalOrderParameterCount+spectralforge::filterModulationExtensionParameterCount+spectralforge::multibandSixBandParameterCount)-spectralforge::roomAdvancedParameterCount-spectralforge::roomSpaceParameterCount;   // banks appended after saturation
    for(auto* raw:p.getParameters())if(auto* id=dynamic_cast<juce::AudioProcessorParameterWithID*>(raw);id && id->paramID.startsWith("sat")){
        require(raw->getVersionHint()==saturationParameterVersion && raw->getParameterIndex()>=later-saturationParameterCount && raw->getParameterIndex()<later,"Saturation parameters are not appended at their version");++seen;}
    require(seen==saturationParameterCount,"Saturation parameter bank incomplete");
    std::cout<<"PASS "<<seen<<" saturation parameters appended at version "<<saturationParameterVersion<<"\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {exactWhenNeutral();smallSignalAndCleanLow();typesAndDrive();isolationAndSwitching();studio();processorParameters();
         require(allocations==0,"Saturation allocates while processing");std::cout<<"PASS no allocation while processing\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
