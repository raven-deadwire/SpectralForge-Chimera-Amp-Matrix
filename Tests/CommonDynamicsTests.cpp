#include "PluginProcessor.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

static thread_local bool counting=false;
static thread_local size_t allocations=0;
void* operator new(std::size_t n){if(counting)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
using namespace spectralforge;
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void set(ChimeraProcessor& p,const juce::String& id,float value){auto* q=p.parameters().getParameter(id);check(q!=nullptr,"Missing dynamics parameter");q->setValueNotifyingHost(q->convertTo0to1(value));}
float get(ChimeraProcessor& p,const juce::String& id){return p.parameters().getRawParameterValue(id)->load();}
void fill(juce::AudioBuffer<float>& b,int offset,double rate,double amplitude=.5,double hz=1000){for(int n=0;n<b.getNumSamples();++n)for(int c=0;c<b.getNumChannels();++c)b.setSample(c,n,float(amplitude*std::sin(juce::MathConstants<double>::twoPi*hz*(offset+n)/rate))*(c==1?-.25f:1.f));}
void bypassAndLimiter(ChimeraProcessor& p){
    auto& d=p.commonDynamics();
    for(double rate:{44100.,48000.,96000.,192000.})for(int channels:{1,2}){
        d.prepare(rate);juce::AudioBuffer<float> b(channels,128),dry;fill(b,0,rate);dry.makeCopyOf(b);
        counting=true;d.compress(b);d.limit(b);counting=false;
        for(int c=0;c<channels;++c)for(int n=0;n<128;++n)check(b.getSample(c,n)==dry.getSample(c,n),"Default dynamics changed released audio");
        set(p,"liveLimiterOn",1);set(p,"liveLimiterCeiling",-6);d.reset();
        const double ceiling=std::pow(10.,-.3);
        for(int k=0;k<100;++k){fill(b,k*128,rate,4);b.setSample(0,0,4);if(channels==2)b.setSample(1,0,-1);
            counting=true;d.limit(b);counting=false;
            check(b.getMagnitude(0,128)<=ceiling+2e-7,"Limiter exceeded sample peak ceiling");
            check(b.getSample(0,0)>0,"Limiter inserted audio delay");
            if(channels==2)check(std::abs(b.getSample(0,0)*-.25-b.getSample(1,0))<1e-7,"Limiter stereo link changed image");
        }
        check(d.limitReduction.load()>17 && d.limitReduction.load()<19,"Limiter GR does not match applied gain");
        set(p,"liveLimiterOn",0);for(int k=0;k<200;++k){fill(b,k*128,rate);d.limit(b);}fill(b,0,rate);dry.makeCopyOf(b);d.limit(b);
        for(int c=0;c<channels;++c)for(int n=0;n<128;++n)check(b.getSample(c,n)==dry.getSample(c,n),"Limiter did not return to exact bypass");
    }
    check(allocations==0,"Dynamics allocated in callback");
    std::cout<<"PASS exact default bypass; limiter ceiling, first-sample response, stereo link, GR and bypass at four rates\n";
}
void compressor(ChimeraProcessor& p){
    check(CommonDynamics::compressionDB(-30,-24,4,0)==0,"Compressor below threshold");
    check(std::abs(CommonDynamics::compressionDB(-12,-24,4,0)-9)<1e-12,"Compressor 4:1 curve");
    check(std::abs(CommonDynamics::compressionDB(-24,-24,4,6)-.5625)<1e-12,"Compressor knee curve");
    auto& d=p.commonDynamics();set(p,"cleanCompOn",1);set(p,"cleanCompThreshold",-24);set(p,"cleanCompKnee",0);set(p,"cleanCompDetector",0);set(p,"cleanCompAttack",1);set(p,"cleanCompRelease",100);d.prepare(48000);
    juce::AudioBuffer<float> b(2,128);double input=0,output=0;
    for(int k=0;k<500;++k){fill(b,k*128,48000);if(k>300)for(int n=0;n<128;++n)input+=b.getSample(0,n)*b.getSample(0,n);
        counting=true;d.compress(b);counting=false;
        if(k>300)for(int n=0;n<128;++n){output+=b.getSample(0,n)*b.getSample(0,n);check(std::abs(b.getSample(1,n)+.25*b.getSample(0,n))<1e-7,"Compressor stereo link");}
    }
    const auto reduction=-10*std::log10(output/input);check(reduction>13 && reduction<14,"Measured compressor 4:1 reduction incorrect");
    check(std::abs(reduction-d.compReduction.load())<.1,"Compressor GR differs from measured gain");
    set(p,"cleanCompMix",0);for(int k=0;k<50;++k){fill(b,k*128,48000);d.compress(b);}fill(b,0,48000);juce::AudioBuffer<float> dry;dry.makeCopyOf(b);d.compress(b);
    for(int c=0;c<2;++c)for(int n=0;n<128;++n)check(b.getSample(c,n)==dry.getSample(c,n),"Parallel zero is not exact aligned dry");
    set(p,"cleanCompMix",1);set(p,"cleanCompDetector",1);d.reset();float early=0,late=0;
    for(int k=0;k<60;++k){fill(b,k*128,48000);d.compress(b);if(k==0)early=d.compReduction.load();late=d.compReduction.load();}
    check(late>early+3,"Compressor attack did not build reduction");
    for(int k=0;k<400;++k){b.clear();d.compress(b);}check(d.compReduction.load()<.05,"Compressor release did not recover");
    set(p,"cleanCompOn",0);d.reset();check(allocations==0,"Compressor allocated in callback");
    std::cout<<"PASS analytic hard/soft knee, measured compression and GR, stereo link, parallel alignment, attack and release\n";
}
void dynamicEQ(ChimeraProcessor& p){
    for(int s=0;s<4;++s){set(p,dynamicSlotID(s,"target"),float(1+s*6));set(p,dynamicSlotID(s,"range"),6);set(p,dynamicSlotID(s,"threshold"),-24);set(p,dynamicSlotID(s,"ratio"),4);}
    DynamicEQBindings bindings;bindings.bind(p.parameters());int assigned=0;for(int t=1;t<=24;++t)assigned+=bindings.forBand(t).enabled;check(assigned==4,"Dynamic EQ shared four-node cap failed");
    set(p,dynamicSlotID(1,"target"),1);set(p,dynamicSlotID(1,"range"),12);const auto duplicate=bindings.forBand(1);check(duplicate.enabled && std::abs(duplicate.range-6)<1e-5,"Duplicate disabled or replaced the first active node");set(p,dynamicSlotID(1,"target"),7);set(p,dynamicSlotID(1,"range"),6);
    for(int bank=0;bank<2;++bank){set(p,eqID(bank,-1,"bypass"),0);for(int band:{0,6}){set(p,eqID(bank,band,"enabled"),1);set(p,eqID(bank,band,"frequency"),1000);set(p,eqID(bank,band,"q"),4);}auto& eq=p.graphicalEQ(bank);eq.prepare(48000);
        juce::AudioBuffer<float> b(2,128);double in=0,out=0;
        for(int k=0;k<600;++k){fill(b,k*128,48000);if(k>300)for(int n=0;n<128;++n)in+=b.getSample(0,n)*b.getSample(0,n);
            counting=true;eq.process(b);counting=false;
            if(k>300)for(int n=0;n<128;++n)out+=b.getSample(0,n)*b.getSample(0,n);
        }
        check(std::abs(10*std::log10(out/in)+12)<.1,"Two dynamic nodes per bank did not apply the combined cut");for(int band:{0,6})check(std::abs(eq.dynamicReduction(band)-6)<.01,"Dynamic GR differs from applied cut");
        for(int k=0;k<1000;++k){fill(b,k*128,48000,.01,80);eq.process(b);}for(int band:{0,6})check(eq.dynamicReduction(band)<.01,"Frequency selective detector failed to release");
        set(p,eqID(bank,-1,"bypass"),1);for(int k=0;k<10;++k)eq.process(b);for(int band:{0,6})check(eq.dynamicReduction(band)==0,"Bypassed EQ still reports dynamic cut");
    }
    check(allocations==0,"Dynamic EQ allocated in callback");
    set(p,eqID(0,-1,"bypass"),0);set(p,eqID(0,0,"frequency"),20);set(p,eqID(0,0,"gain"),24);set(p,eqID(0,0,"q"),18);
    set(p,dynamicSlotID(0,"target"),0);const auto staticTail=p.graphicalEQ(0).tailSeconds();set(p,dynamicSlotID(0,"target"),1);
    check(staticTail>1 && p.graphicalEQ(0).tailSeconds()>=staticTail,"Dynamic mode under-reported the uncompressed filter tail");
    set(p,eqID(0,-1,"bypass"),1);
    std::cout<<"PASS four shared nodes, deterministic duplicate recovery, two-bank measured dynamic cut/GR, selective detector, release and bypass\n";
}
void expander(){
    juce::AudioBuffer<float> b(2,128);NoiseGate gate;gate.prepare(48000);gate.setMode(true,2,.5);
    for(int k=0;k<1000;++k){for(int c=0;c<2;++c)for(int n=0;n<128;++n)b.setSample(c,n,.001f);gate.process(b,true,-40,50,10,12);}
    check(std::abs(20*std::log10(gate.reduction())+12)<.01,"Expander did not respect range floor");
    gate.setMode(true,1,.5);for(int k=0;k<200;++k){for(int c=0;c<2;++c)for(int n=0;n<128;++n)b.setSample(c,n,.001f);gate.process(b,true,-40,50,10,96);}
    check(gate.reduction()>.999,"Expander ratio 1 is not unity");
    gate.setMode(true,2,.5);for(int k=0;k<1000;++k){for(int c=0;c<2;++c)for(int n=0;n<128;++n)b.setSample(c,n,.001f);gate.process(b,true,-40,50,10,96);}
    check(std::abs(20*std::log10(gate.reduction())+20)<.02,"Expander downward slope");
    std::cout<<"PASS DI expander ratio, unity and finite attenuation range\n";
}
void lastStage(){
    auto p=std::make_unique<ChimeraProcessor>();set(*p,"gateon",0);set(*p,"boardEnabled",0);set(*p,"preon",0);set(*p,"delayon",0);set(*p,"reverbon",0);
    for(int lane=1;lane<=3;++lane){set(*p,"cab"+juce::String(lane),0);set(*p,"ampon"+juce::String(lane),0);}
    set(*p,"output",6);set(*p,eqID(1,-1,"bypass"),0);set(*p,eqID(1,0,"enabled"),1);set(*p,eqID(1,0,"frequency"),1000);set(*p,eqID(1,0,"gain"),12);
    p->prepareToPlay(48000,64);
    juce::AudioBuffer<float> b(2,257);juce::MidiBuffer midi;float dryPeak=0;
    for(int i=0;i<120;++i){fill(b,i*257,48000,.5);p->processBlock(b,midi);if(i>60)dryPeak=std::max(dryPeak,b.getMagnitude(0,257));}
    check(dryPeak>.3f,"Limiter placement fixture does not exceed ceiling before limiting");
    const int latency=p->getLatencySamples();
    set(*p,"liveLimiterOn",1);set(*p,"liveLimiterCeiling",-12);
    for(int i=0;i<500;++i){fill(b,i*257,48000,.5);p->processBlock(b,midi);check(b.getMagnitude(0,257)<=std::pow(10.f,-.6f)+2e-7f,"Processor raised gain after final limiter");}
    check(p->getLatencySamples()==latency,"Common limiter changed host latency");
    check(p->outputMeter()<.252f,"Output meter did not observe limited output");p->releaseResources();
    std::cout<<"PASS production processor: Final EQ + output boost before limiter, oversized host blocks, output metering and unchanged latency\n";
}
void state(ChimeraProcessor& p){
    check(p.getParameters().size()==5026+multibandParameterCount+deEsserParameterCount+spectralforge::extraGateParameterCount+spectralforge::multibandStereoParameterCount+spectralforge::saturationParameterCount+spectralforge::referenceAmpParameterCount()+spectralforge::filterModulationParameterCount,"Unexpected complete appended host count");
    int ordinal=4986;for(int s=0;s<4;++s)for(const char* field:{"target","threshold","range","attack","release","ratio"}){
        auto* q=p.parameters().getParameter(dynamicSlotID(s,field));check(q->getParameterIndex()==ordinal++ && q->getVersionHint()==12,"Dynamic parameter order/version changed");check(q->isAutomatable()==(juce::String(field)!="target"),"Dynamic target automation contract");}
    for(const auto* id:{"cleanCompOn","cleanCompThreshold","cleanCompRatio","cleanCompAttack","cleanCompRelease","cleanCompKnee","cleanCompMakeup","cleanCompMix","cleanCompHP","cleanCompDetector","gateMode","gateRatio","gateAttack","liveLimiterOn","liveLimiterCeiling","liveLimiterRelease"}){
        auto* q=p.parameters().getParameter(id);check(q->getParameterIndex()==ordinal++ && q->getVersionHint()==12 && q->isAutomatable(),"Common dynamics append-only order/version changed");}
    check(ordinal==5026,"Missing dynamics parameter coverage");
    set(p,"cleanCompOn",1);set(p,"gateMode",1);set(p,"liveLimiterOn",1);set(p,"liveLimiterCeiling",-3);
    juce::MemoryBlock saved;p.getStateInformation(saved);auto copy=std::make_unique<ChimeraProcessor>();copy->setStateInformation(saved.getData(),int(saved.getSize()));
    for(int i=4986;i<5026;++i){auto* q=dynamic_cast<juce::RangedAudioParameter*>(p.getParameters()[i]);check(std::abs(get(*copy,q->paramID)-get(p,q->paramID))<.002,"Dynamics project restore failed");}
    p.copyComparison();p.selectComparison(1);set(p,"liveLimiterCeiling",-12);p.selectComparison(0);check(std::abs(get(p,"liveLimiterCeiling")+3)<1e-5,"Dynamics A recall");p.selectComparison(1);check(std::abs(get(p,"liveLimiterCeiling")+12)<1e-5,"Dynamics B recall");
    auto legacy=p.parameters().copyState();for(int i=legacy.getNumChildren();--i>=0;){const auto id=legacy.getChild(i).getProperty("id").toString();auto* q=p.parameters().getParameter(id);if(q && q->getParameterIndex()>=4986)legacy.removeChild(i,nullptr);}
    juce::MemoryBlock old;juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),old);copy->setStateInformation(old.getData(),int(old.getSize()));
    check(get(*copy,"cleanCompOn")==0 && get(*copy,"liveLimiterOn")==0 && get(*copy,"gateMode")==0,"Legacy state inherited enabled dynamics");for(int i=0;i<4;++i)check(get(*copy,dynamicSlotID(i,"target"))==0,"Legacy state inherited a dynamic node");
    std::cout<<"PASS 40 append-only parameters, fixed indices/version hints, automation, project/A-B recall and legacy defaults\n";
}
}
int main(){try{juce::ScopedJuceInitialiser_GUI gui;auto p=std::make_unique<ChimeraProcessor>();bypassAndLimiter(*p);compressor(*p);dynamicEQ(*p);expander();lastStage();state(*p);std::cout<<"PASS common dynamics contracts\n";return 0;}catch(const std::exception& e){counting=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
