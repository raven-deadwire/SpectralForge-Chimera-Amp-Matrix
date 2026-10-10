#include "GuitarSignaturePresets.h"
#include "PluginProcessor.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <stdexcept>
#include <cstdlib>

// Allocation instrumentation is thread-local and enabled only around EQ callbacks.
static thread_local bool countAllocations=false;
static thread_local size_t allocations=0;
void* operator new(std::size_t n) {if(countAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
using namespace spectralforge;
namespace {
void check(bool v,const char* m) {if(!v)throw std::runtime_error(m);}
void set(ChimeraProcessor& p,const juce::String& id,float v) {auto* q=p.parameters().getParameter(id);check(q!=nullptr,"Missing parameter");q->setValueNotifyingHost(q->convertTo0to1(v));}
float get(ChimeraProcessor& p,const juce::String& id) {return p.parameters().getRawParameterValue(id)->load();}
void configure(ChimeraProcessor& p,int e,int b,int type,float f,float g,float q=.70710678f) {
    set(p,eqID(e,-1,"bypass"),0);set(p,eqID(e,b,"enabled"),1);set(p,eqID(e,b,"frequency"),f);set(p,eqID(e,b,"gain"),g);set(p,eqID(e,b,"q"),q);set(p,eqID(e,b,"type"),float(type));
}
void fill(juce::AudioBuffer<float>& b,int offset,double rate,double hz=1000,double amplitude=.05) {
    for(int n=0;n<b.getNumSamples();++n)for(int c=0;c<b.getNumChannels();++c)b.setSample(c,n,float(amplitude*std::sin(2*juce::MathConstants<double>::pi*hz*(offset+n)/rate))*(c==1?-.7f:1.f));
}
void fifoContract() {
    EQAnalyzerFIFO f;EQAnalyzerFrame x;
    for(uint32_t n=0;n<EQAnalyzerFIFO::capacity+17;++n)f.push({float(n),float(n+1),float(n+2),float(n+3)});
    check(f.dropped.load()==17,"Analyzer overflow did not drop newest");
    for(uint32_t n=0;n<EQAnalyzerFIFO::capacity;++n){check(f.pop(x),"FIFO lost sample");check(x.inputL==float(n) && x.outputR==float(n+3),"FIFO changed paired stereo ordering");}
    check(!f.pop(x),"FIFO failed empty detection");
    std::atomic<bool> done{false};std::thread producer([&]{for(int n=0;n<100000;++n)f.push({float(n),float(n),float(n),float(n)});done.store(true);});
    float previous=-1;while(!done.load())if(f.pop(x)){check(x.inputL>previous && x.outputR==x.inputL,"SPSC ordering");previous=x.inputL;}
    producer.join();while(f.pop(x)){check(x.inputL>previous,"SPSC drain ordering");previous=x.inputL;}
    std::cout<<"PASS paired stereo FIFO capacity / overflow / concurrent producer-consumer\n";
}
void filters(ChimeraProcessor& p) {
    auto& eq=p.graphicalEQ(0);
    for(double rate:{44100.,48000.,96000.,192000.})for(int channels:{1,2}) {
        configure(p,0,0,0,1000,12);eq.prepare(rate);juce::AudioBuffer<float> b(channels,128);double input=0,output=0;
        for(int block=0;block<600;++block){fill(b,block*128,rate);if(block>=300)for(int n=0;n<128;++n)input+=b.getSample(0,n)*b.getSample(0,n);countAllocations=true;eq.process(b);countAllocations=false;if(block>=300)for(int n=0;n<128;++n)output+=b.getSample(0,n)*b.getSample(0,n);}
        check(std::abs(10*std::log10(output/input)-12)<.05,"Bell did not apply measured 12 dB");check(allocations==0,"EQ allocated in callback");
        set(p,eqID(0,-1,"bypass"),1);for(int n=0;n<40;++n){fill(b,n*128,rate);eq.process(b);}fill(b,0,rate);juce::AudioBuffer<float> dry;dry.makeCopyOf(b);eq.process(b);
        for(int c=0;c<channels;++c)for(int n=0;n<128;++n)check(b.getSample(c,n)==dry.getSample(c,n),"Settled bypass not exact dry");
    }
    // Independent numerical expectations at the center of HP / LP / notch / BP.
    for(int type=3;type<7;++type){configure(p,0,0,type,1000,0);eq.prepare(48000);juce::AudioBuffer<float>b(2,128);double sum=0;
        for(int k=0;k<600;++k){fill(b,k*128,48000);eq.process(b);if(k>=300)for(int n=0;n<128;++n)sum+=b.getSample(0,n)*b.getSample(0,n);}
        const double db=10*std::log10(std::max(1.e-30,sum/(300*128*.05*.05*.5)));
        check(type==5 ? db<-55 : std::abs(db-(type==6?0:-3.0103))<.06,"Cut/notch/pass measured response");
    }
    for(int type:{1,2}){configure(p,0,0,type,1000,12);eq.prepare(48000);juce::AudioBuffer<float>b(2,128);double sum=0;
        const double hz=type==1?50:15000;for(int k=0;k<600;++k){fill(b,k*128,48000,hz);eq.process(b);if(k>=300)for(int n=0;n<128;++n)sum+=b.getSample(0,n)*b.getSample(0,n);}
        check(std::abs(10*std::log10(sum/(300*128*.05*.05*.5))-12)<.2,"Shelf plateau response");}
    configure(p,0,0,0,20,24,18);check(eq.tailSeconds()>1 && eq.tailSeconds()<120,"EQ tail poles were omitted or sign reversed");
    // Automate all 24 nodes, all filter types and extremes during real processing.
    for(int e=0;e<2;++e){set(p,eqID(e,-1,"bypass"),0);for(int n=0;n<12;++n)configure(p,e,n,n%7,graphicalEQFrequencies[(size_t)n],0);p.graphicalEQ(e).prepare(48000);}
    juce::AudioBuffer<float>b(2,64);for(int k=0;k<1600;++k){const int e=k%2,n=k%12;
        configure(p,e,n,k%7,k%3==0?20.f:k%3==1?20000.f:float(100+k%17000),k%2?24.f:-24.f,k%2?18.f:.1f);
        fill(b,k*64,48000,997,.001);countAllocations=true;p.graphicalEQ(0).process(b);p.graphicalEQ(1).process(b);countAllocations=false;
        for(int c=0;c<2;++c)for(int j=0;j<64;++j)check(std::isfinite(b.getSample(c,j)),"Nonfinite under rapid automation");
    }
    check(allocations==0,"Rapid automation allocated");std::cout<<"PASS measured 7 filters, 4 rates, mono/stereo, exact bypass, 24-node automation, no EQ allocations\n";
}
void stateContract(ChimeraProcessor& p) {
    constexpr std::array<const char*,5> fields{"enabled","frequency","gain","q","type"};
    auto copy=std::make_unique<ChimeraProcessor>();
    for(int e=0;e<2;++e)for(int b=0;b<12;++b)configure(p,e,b,b%7,117.f+float(e*31+b*971),float(e*2+b-11),.3f+float(b));
    juce::MemoryBlock saved;p.getStateInformation(saved);copy->setStateInformation(saved.getData(),int(saved.getSize()));
    int checked=0;for(int e=0;e<2;++e){check(get(*copy,eqID(e,-1,"bypass"))==get(p,eqID(e,-1,"bypass")),"Bypass restore");++checked;
        for(int b=0;b<12;++b)for(const auto* f:fields){const auto id=eqID(e,b,f);const float v=get(p,id);const auto* parameter=p.parameters().getParameter(id);
            check(parameter->isAutomatable() && parameter->getVersionHint()==10 && parameter->getParameterIndex()>=4845,"EQ ordinal/hint/automation");
            const float tolerance=std::string(f)=="frequency" || std::string(f)=="q" || std::string(f)=="gain" ? 4*std::numeric_limits<float>::epsilon()*std::max(1.f,std::abs(v)) : 0;
            check(std::abs(get(*copy,id)-v)<=tolerance,"Project parameter restore");++checked;}}
    check(checked==graphicalEQParameterCount,"EQ bank coverage");
    p.copyComparison();p.selectComparison(1);set(p,eqID(0,0,"gain"),-9);p.selectComparison(0);check(get(p,eqID(0,0,"gain"))==-11,"A recall");p.selectComparison(1);check(get(p,eqID(0,0,"gain"))==-9,"B recall");
    auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));auto legacy=juce::ValueTree::fromXml(*xml);
    auto strip=[](juce::ValueTree t){for(int i=t.getNumChildren()-1;i>=0;--i){auto id=t.getChild(i).getProperty("id").toString();if(id.startsWith("toneEQ_") || id.startsWith("finalEQ_"))t.removeChild(i,nullptr);}};
    strip(legacy);for(auto slot:legacy.getChildWithName("COMPARISONS"))strip(slot);juce::MemoryBlock old;juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),old);
    copy->setStateInformation(old.getData(),int(old.getSize()));check(copy->graphicalEQ(0).isBypassed() && copy->graphicalEQ(1).isBypassed(),"Legacy missing EQ inherited dirty state");
    for(int index=0;index<selectablePresetCount;++index){copy->loadFactoryPreset(index);check(copy->graphicalEQ(0).isBypassed() && copy->graphicalEQ(1).isBypassed(),"Factory preset enabled a new EQ");for(int e=0;e<2;++e)for(int b=0;b<12;++b)check(!copy->graphicalEQ(e).values(b).enabled,"Factory preset inherited dirty node");}
    std::cout<<"PASS 122 host parameters / project / A-B / legacy migration / "<<selectablePresetCount<<" factory defaults\n";
}
void audioRestore() {
    auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
    configure(*a,0,0,0,700,4);configure(*a,1,0,2,3000,-3);set(*a,"output",-6);
    a->copyComparison();a->selectComparison(1);set(*a,eqID(0,0,"gain"),7);a->selectComparison(0);
    juce::MemoryBlock bytes;a->getStateInformation(bytes);b->setStateInformation(bytes.getData(),int(bytes.getSize()));
    const auto compare=[&](bool exact) {
        a->prepareToPlay(48000,128);b->prepareToPlay(48000,128);check(a->getLatencySamples()==b->getLatencySamples(),"State latency changed");
        juce::AudioBuffer<float>x(2,128),y(2,128);juce::MidiBuffer midi;double error=0,energy=0;
        for(int k=0;k<200;++k){fill(x,k*128,48000,997,.02);y.makeCopyOf(x,true);a->processBlock(x,midi);b->processBlock(y,midi);
            for(int c=0;c<2;++c)for(int n=0;n<128;++n){error=std::max(error,double(std::abs(x.getSample(c,n)-y.getSample(c,n))));energy+=x.getSample(c,n)*x.getSample(c,n);}}
        check(error<=(exact?0.:1.e-6),"Restored project/A-B audio changed");check(energy>1.e-8,"Restore audio fixture is silent");a->releaseResources();b->releaseResources();
    };
    compare(false);a->selectComparison(1);b->selectComparison(1);compare(false);
    // Removing both banks from an old project must yield the same audio as fresh bypass defaults.
    set(*a,eqID(0,-1,"bypass"),1);set(*a,eqID(1,-1,"bypass"),1);a->getStateInformation(bytes);
    auto xml=juce::AudioProcessor::getXmlFromBinary(bytes.getData(),int(bytes.getSize()));auto legacy=juce::ValueTree::fromXml(*xml);
    for(int i=legacy.getNumChildren()-1;i>=0;--i){const auto id=legacy.getChild(i).getProperty("id").toString();if(id.startsWith("toneEQ_") || id.startsWith("finalEQ_"))legacy.removeChild(i,nullptr);}
    juce::MemoryBlock old;juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),old);a->setStateInformation(bytes.getData(),int(bytes.getSize()));b->setStateInformation(old.getData(),int(old.getSize()));compare(true);
    std::cout<<"PASS actual project and A/B audio <= 1e-6 / legacy missing EQ audio exact / unchanged latency\n";
}
void placement() {
    for(int route=0;route<4;++route) {
        auto p=std::make_unique<ChimeraProcessor>();set(*p,"mode",route==0?0:route==3?2:1);set(*p,"dualtype",route==2?1:0);
        for(int lane=1;lane<=3;++lane){set(*p,"ampon"+juce::String(lane),0);set(*p,"cab"+juce::String(lane),0);}
        set(*p,"boardEnabled",0);set(*p,"gateon",0);set(*p,"output",0);set(*p,"buscompon",1);set(*p,"busthreshold",-24);set(*p,"busratio",4);set(*p,"busattack",1);set(*p,"busrelease",50);set(*p,"busmakeup",0);
        for(int section=0;section<3;++section)set(*p,postNativeModeID(section),0);
        set(*p,"doubleron",1);set(*p,"doublertime",6);set(*p,"metronome",0);
        configure(*p,0,0,0,997,12);configure(*p,1,0,3,200,0);p->prepareToPlay(48000,128);
        std::array<std::atomic<float>*,fxSpecs.size()> raw{};for(size_t i=0;i<raw.size();++i)raw[i]=p->parameters().getRawParameterValue(fxSpecs[i].id);
        auto fx=readFX(raw);for(size_t i=0;i<modelFamilies.size();++i)fx.models[i]=int(get(*p,modelFamilies[i].parameter));
        fx.postNative=defaultPostNativeState(false);
        PostFXChain post;PerformanceUtilities utility;const juce::dsp::ProcessSpec spec{48000,128,2};post.prepare(spec);utility.prepare(spec);
        auto ref=std::make_unique<ChimeraProcessor>();configure(*ref,0,0,0,997,12);configure(*ref,1,0,3,200,0);ref->graphicalEQ(0).prepare(48000);ref->graphicalEQ(1).prepare(48000);
        p->graphicalEQ(0).analyzer.enabled.store(true);p->graphicalEQ(1).analyzer.enabled.store(true);
        juce::AudioBuffer<float>b(2,128),toneInput(2,128),toneOutput(2,128),finalInput(2,128),finalOutput(2,128);juce::MidiBuffer midi;
        for(int k=0;k<400;++k){fill(b,k*128,48000,997,.15);p->processBlock(b,midi);
            for(int n=0;n<128;++n){EQAnalyzerFrame t,f;check(p->graphicalEQ(0).analyzer.pop(t) && p->graphicalEQ(1).analyzer.pop(f),"Placement analyzer missing frame");toneInput.setSample(0,n,t.inputL);toneInput.setSample(1,n,t.inputR);toneOutput.setSample(0,n,t.outputL);toneOutput.setSample(1,n,t.outputR);finalInput.setSample(0,n,f.inputL);finalInput.setSample(1,n,f.inputR);finalOutput.setSample(0,n,f.outputL);finalOutput.setSample(1,n,f.outputR);}
            ref->graphicalEQ(0).process(toneInput);
            for(int c=0;c<2;++c)for(int n=0;n<128;++n)check(std::abs(toneInput.getSample(c,n)-toneOutput.getSample(c,n))<1.e-6,"Tone EQ transfer mismatch");
            post.process(toneOutput,fx);utility.process(toneOutput,120,true,6,false,false);
            for(int c=0;c<2;++c)for(int n=0;n<128;++n)check(std::abs(toneOutput.getSample(c,n)-finalInput.getSample(c,n))<1.e-6,"Tone / POST compressor / width / Final placement mismatch");
            ref->graphicalEQ(1).process(finalInput);
            for(int c=0;c<2;++c)for(int n=0;n<128;++n){
                check(std::abs(finalInput.getSample(c,n)-finalOutput.getSample(c,n))<1.e-6,"Final EQ transfer mismatch");check(b.getSample(c,n)==finalOutput.getSample(c,n),"Final EQ is not before master unity trim");}
        }
        p->releaseResources();std::cout<<"PASS route "<<route<<" analyzer node transfers and final output\n";
    }
}
void benchmark(ChimeraProcessor& p,const char* path) {
    std::ofstream out(path);out<<"rate,block,channels,analyzer,p50_us,p99_us,budget_us\n";
    for(int e=0;e<2;++e)for(int b=0;b<12;++b)configure(p,e,b,b%3,graphicalEQFrequencies[(size_t)b],b%2?2.f:-2.f,1);
    for(double rate:{48000.,96000.})for(int block:{64,128,512})for(int channels:{1,2})for(bool analyzer:{false,true}) {
        for(int e=0;e<2;++e){p.graphicalEQ(e).prepare(rate);p.graphicalEQ(e).analyzer.enabled.store(analyzer);}juce::AudioBuffer<float>b(channels,block);std::vector<double> times;times.reserve(600);
        for(int k=0;k<640;++k){fill(b,k*block,rate,997,.001);const auto start=std::chrono::steady_clock::now();countAllocations=true;p.graphicalEQ(0).process(b);p.graphicalEQ(1).process(b);countAllocations=false;
            const auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();if(k>=40)times.push_back(us);EQAnalyzerFrame f;for(int e=0;e<2;++e)while(p.graphicalEQ(e).analyzer.pop(f)){} }
        std::sort(times.begin(),times.end());const double budget=block/rate*1.e6;out<<rate<<','<<block<<','<<channels<<','<<analyzer<<','<<times[300]<<','<<times[594]<<','<<budget<<'\n';
        std::cout<<"CPU pair24 "<<rate<<" Hz "<<block<<" / "<<channels<<"ch analyzer_capture="<<analyzer<<" p50="<<times[300]<<" p99="<<times[594]<<" us / "<<budget<<" us\n";
    }
    check(allocations==0,"Benchmark callback allocated");
}
}
int main(int argc,char** argv) {
    try {juce::ScopedJuceInitialiser_GUI init;fifoContract();auto p=std::make_unique<ChimeraProcessor>();filters(*p);stateContract(*p);audioRestore();placement();benchmark(*p,argc>1?argv[1]:"eq-cpu.csv");return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
