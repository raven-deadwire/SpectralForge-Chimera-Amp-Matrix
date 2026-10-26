// Para EQ (PRE model 40, docs/PRE_EXPANSION_A_1_4.md): catalog and menu
// entry, parameters appended after the released list at version 16, measured
// response equal to the analytic peaking EQ and boost, band ranges, neutral
// transparency, board integration, old-project defaults, no allocation.
#include "PluginProcessor.h"
#include "EffectSelectionCatalog.h"
#include <cmath>
#include <cstdlib>
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

namespace {
using namespace spectralforge;
constexpr int paraEQ=40;
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
using Controls=std::array<float,12>;
Controls neutral() {Controls c{};const auto i=defaultPedalInstance(paraEQ);for(size_t k=0;k<c.size();++k)c[k]=i.controls[k];return c;}

void catalogAndMenu() {
    const auto& m=pedalModel(paraEQ);
    require(juce::String(m.id)=="pre.paraeq" && juce::String(m.name)=="Para EQ" && m.implemented && m.controlCount==11,"Para EQ catalog entry");
    require(releasedPedalModelCount==40 && pedalModelCount==41,"Model counts");
    require(juce::String(pedalMenuName(paraEQ))=="Para EQ" && pedalMenuEntries[paraEQ].kind==PedalMenuKind::equalizer,"Para EQ is not in the Equalizer menu");
    const auto n=neutral();
    require(n[1]==0 && n[4]==0 && n[7]==0 && juce::roundToInt(n[2])==1 && n[10]<.5f,"Para EQ does not start flat with medium Q and boost off");
    std::cout<<"PASS Para EQ catalog: model 40, 11 controls, Equalizer menu, starts flat\n";
}
void parametersAppend() {
    auto p=std::make_unique<ChimeraProcessor>();const int total=p->getParameters().size(),first=total-pedalExpansionParameterCount;
    require(pedalExpansionParameterCount==5*12,"Para EQ adds 5 x (bypass + 11 controls)");
    int seen=0;
    for(auto* raw:p->getParameters()) {
        const auto* parameter=dynamic_cast<juce::AudioProcessorParameterWithID*>(raw);require(parameter!=nullptr,"Parameter without ID");
        const bool expansion=parameter->paramID.startsWith("board") && parameter->paramID.contains("_m40_");
        if(expansion){require(parameter->getParameterIndex()>=first && parameter->getVersionHint()==pedalExpansionParameterVersion,"A Para EQ parameter is not appended at version 16");++seen;}
        else require(parameter->getParameterIndex()<first,"A released parameter moved behind the Para EQ banks");
    }
    require(seen==pedalExpansionParameterCount,"Para EQ banks incomplete");
    for(int owner=0;owner<5;++owner)require(p->parameters().getParameter(pedalBypassID(owner,paraEQ))!=nullptr && p->parameters().getParameter(pedalControlID(owner,paraEQ,9))!=nullptr,"Para EQ bank missing for an owner");
    std::cout<<"PASS Para EQ parameters: 60 appended after every released parameter at version 16\n";
}
// Steady-state gain of a sine through the EQ, in dB.
double measuredDb(PedalParaEQ& eq,const Controls& c,double rate,double hz) {
    eq.reset();juce::AudioBuffer<float> b(2,256);double in=0,out=0;long t=0;
    for(int block=0;block<int(rate*.6/256);++block) {
        for(int n=0;n<256;++n,++t){const float x=.1f*float(std::sin(juce::MathConstants<double>::twoPi*hz*double(t)/rate));b.setSample(0,n,x);b.setSample(1,n,-x);}
        juce::AudioBuffer<float> dry;dry.makeCopyOf(b);eq.process(b,c);
        if(block>int(rate*.3/256))for(int n=0;n<256;++n){in+=double(dry.getSample(0,n))*dry.getSample(0,n);out+=double(b.getSample(0,n))*b.getSample(0,n);}
    }
    return 10*std::log10(out/in);
}
void response() {
    for(const double rate:{44100.,48000.,96000.}) {
        PedalParaEQ eq;eq.prepare(rate);
        // Band ranges and the Nyquist guard.
        require(std::abs(PedalParaEQ::frequency(0,0)-35)<1e-3f && std::abs(PedalParaEQ::frequency(0,1)-500)<1e-2f && std::abs(PedalParaEQ::frequency(1,0)-250)<1e-2f
                && std::abs(PedalParaEQ::frequency(1,1)-5000)<.1f && std::abs(PedalParaEQ::frequency(2,0)-1000)<.1f && std::abs(PedalParaEQ::frequency(2,1)-20000)<1,"Band ranges");
        // Neutral: exactly transparent.
        {const auto c=neutral();juce::AudioBuffer<float> b(2,512),d(2,512);
         for(int n=0;n<512;++n){const float x=.3f*float(std::sin(.05*n));b.setSample(0,n,x);b.setSample(1,n,.5f*x);}d.makeCopyOf(b);eq.reset();eq.process(b,c);
         float error=0;for(int ch=0;ch<2;++ch)for(int n=0;n<512;++n)error=std::max(error,std::abs(b.getSample(ch,n)-d.getSample(ch,n)));
         require(error<1e-6f,"Neutral Para EQ is not transparent");}
        // Measured gain equals the analytic peaking EQ and boost.
        double worst=0;
        const std::vector<Controls> settings{
            [&]{auto c=neutral();c[0]=0;c[1]=15;c[2]=0;return c;}(),                    // low 35 Hz +15 wide
            [&]{auto c=neutral();c[3]=.5f;c[4]=-15;c[5]=2;return c;}(),                 // mid ~1.1 kHz -15 narrow
            [&]{auto c=neutral();c[6]=1;c[7]=10;c[8]=1;c[9]=30;c[10]=1;return c;}(),    // high 20 kHz (clamped) +10, boost 30
            [&]{auto c=neutral();c[1]=-6;c[4]=9;c[7]=-12;c[9]=12;c[10]=1;return c;}()}; // all bands, boost 12
        for(const auto& c:settings)for(const double hz:{35.,120.,500.,1118.,3000.,8000.,15000.}) {
            if(hz>rate*.4)continue;
            const double expected=eq.magnitudeDb(c,hz),measured=measuredDb(eq,c,rate,hz);
            worst=std::max(worst,std::abs(expected-measured));
        }
        std::cout<<"MEASURE Para EQ "<<rate<<" Hz: largest measured-vs-analytic gain error "<<worst<<" dB\n";
        require(worst<.05,"Measured Para EQ gain differs from its analytic response");
        // The bands do what they say: +15 dB at the low band's centre, -15 at the mid's.
        {auto c=neutral();c[0]=.5f;c[1]=15;require(std::abs(eq.magnitudeDb(c,PedalParaEQ::frequency(0,.5f))-15)<.01,"Low band centre gain");}
        {auto c=neutral();c[3]=.25f;c[4]=-15;c[5]=2;require(std::abs(eq.magnitudeDb(c,PedalParaEQ::frequency(1,.25f))+15)<.01,"Mid band centre gain");}
        {auto c=neutral();c[9]=30;c[10]=1;require(std::abs(eq.magnitudeDb(c,1000)-30)<1e-6,"Boost is not 30 dB");}
        // Narrower Q affects less of the neighbourhood.
        {auto w=neutral(),n=neutral();w[3]=n[3]=.5f;w[4]=n[4]=12;w[5]=0;n[5]=2;const double f=PedalParaEQ::frequency(1,.5f)*2;
         require(eq.magnitudeDb(w,f)>eq.magnitudeDb(n,f)+3,"Q choices do not change the bandwidth");}
    }
    std::cout<<"PASS Para EQ response: band ranges, neutral transparency, measured = analytic at 44.1/48/96 kHz, Q and boost\n";
}
void boardIntegration() {
    // Through the PRE board DSP in owner 2 at position 0: the model processes,
    // a bypassed slot does not.
    PedalBoardDSP board;board.prepare({48000,256,2});
    PedalBoardState state;state.enabled=true;state.instances[2]=defaultPedalInstance(paraEQ);state.order={2,0,1,3,4};
    state.instances[2].controls[9]=12;state.instances[2].controls[10]=1;   // boost 12 dB, EQ flat
    juce::AudioBuffer<float> b(2,256);double in=0,out=0;long t=0;
    for(int block=0;block<200;++block){
        for(int n=0;n<256;++n,++t){const float x=.05f*float(std::sin(juce::MathConstants<double>::twoPi*440.*double(t)/48000.));b.setSample(0,n,x);b.setSample(1,n,x);if(block>100)in+=double(x)*x;}
        board.process(b,state);if(block>100)for(int n=0;n<256;++n)out+=double(b.getSample(0,n))*b.getSample(0,n);
    }
    const double gain=10*std::log10(out/in);std::cout<<"MEASURE Para EQ boost through the PRE board: "<<gain<<" dB\n";
    require(std::abs(gain-12)<.05,"Para EQ in the PRE board does not apply its boost");
    // The processor's parameter cache delivers the model and its controls.
    auto p=std::make_unique<ChimeraProcessor>();p->setPedalModel(1,paraEQ);
    auto* mid=p->parameters().getParameter(pedalControlID(1,paraEQ,4));mid->setValueNotifyingHost(mid->convertTo0to1(-7.5f));
    const auto read=p->pedalBoardState();
    require(read.instances[1].model==paraEQ && std::abs(read.instances[1].controls[4]+7.5f)<.01f,"The processor does not read the Para EQ bank");
    std::cout<<"PASS Para EQ in the PRE board: boost measured through the board; processor reads its bank\n";
}
void oldProjects() {
    // A project saved without the Para EQ banks restores their defaults, also
    // into a processor whose Para EQ was edited.
    auto source=std::make_unique<ChimeraProcessor>();juce::MemoryBlock data;source->getStateInformation(data);
    auto xml=juce::AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()));require(xml!=nullptr,"State is not binary XML");
    std::vector<juce::XmlElement*> remove;for(auto* child:xml->getChildIterator())if(child->getStringAttribute("id").contains("_m40_"))remove.push_back(child);
    require(remove.size()==size_t(pedalExpansionParameterCount),"Fixture did not hold the Para EQ banks");
    for(auto* child:remove)xml->removeChildElement(child,true);
    juce::MemoryBlock legacy;juce::AudioProcessor::copyXmlToBinary(*xml,legacy);
    auto target=std::make_unique<ChimeraProcessor>();auto* gain=target->parameters().getParameter(pedalControlID(0,paraEQ,1));gain->setValueNotifyingHost(gain->convertTo0to1(9.f));
    target->setStateInformation(legacy.getData(),int(legacy.getSize()));
    require(std::abs(gain->convertFrom0to1(gain->getValue()))<.01f,"An old project kept a Para EQ value from the session");
    std::cout<<"PASS old projects restore Para EQ defaults\n";
}
void noAllocation() {
    PedalParaEQ eq;eq.prepare(48000);juce::AudioBuffer<float> b(2,128);auto c=neutral();
    allocations=0;countAllocations=true;
    for(int i=0;i<200;++i){c[4]=-15.f+30.f*float(i%20)/19.f;c[3]=float(i%7)/6.f;c[10]=float(i%2);eq.process(b,c);}
    countAllocations=false;require(allocations==0,"Para EQ allocates while processing");
    std::cout<<"PASS Para EQ: no allocation while automating\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {catalogAndMenu();parametersAppend();response();boardIntegration();oldProjects();noAllocation();return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
