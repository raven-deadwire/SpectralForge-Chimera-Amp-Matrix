// OVERSAMPLING Auto (docs/OVERSAMPLING_AUTO.md): a new instance follows the
// sample rate (4x at 44.1/48 kHz, 2x at 88.2/96, 1x from 176.4) and sounds
// bit for bit like the fixed choice it picks; a session saved before Auto
// existed restores Auto off with its stored choice; a saved Auto restores on;
// a change to the released "oversampling" parameter (host, automation) turns
// Auto off, while a restore or an A/B switch does not; a factory preset resets
// it to Auto (it used to reset it to 4x);
// the host latency is the same either way.
#include "PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok,const juce::String& message){if(!ok)throw std::runtime_error(message.toStdString());}
void set(ChimeraProcessor& p,const char* id,float value){auto* q=p.parameters().getParameter(id);require(q!=nullptr,juce::String("missing ")+id);q->setValueNotifyingHost(q->convertTo0to1(value));}
float raw(ChimeraProcessor& p,const char* id){return p.parameters().getRawParameterValue(id)->load();}
bool automatic(ChimeraProcessor& p){return raw(p,"oversamplingAuto")>.5f;}

std::vector<float> render(ChimeraProcessor& p,double rate,int block=128,double seconds=.6){
    p.prepareToPlay(rate,block);juce::AudioBuffer<float> b(2,block);juce::MidiBuffer midi;std::vector<float> out;
    for(long t=0;t<long(seconds*rate);t+=block){
        for(int n=0;n<block;++n){const double time=double(t+n)/rate,note=std::fmod(time,.2);
            float x=0;const double e=std::min(1.,note/.003)*std::exp(-note*10);for(int h=1;h<=6;++h)x+=float(e*.25/h*std::sin(juce::MathConstants<double>::twoPi*110.*h*note));
            b.setSample(0,n,x);b.setSample(1,n,x);}
        p.processBlock(b,midi);for(int c=0;c<2;++c)for(int n=0;n<block;++n)out.push_back(b.getSample(c,n));
    }
    return out;
}

void followsTheRate(){
    struct Case{double rate;int choice;const char* name;};
    for(const auto& c:{Case{44100,2,"4x"},Case{48000,2,"4x"},Case{88200,1,"2x"},Case{96000,1,"2x"},Case{176400,0,"1x"},Case{192000,0,"1x"}}){
        auto a=std::make_unique<ChimeraProcessor>();a->prepareToPlay(c.rate,128);
        require(automatic(*a),"a new instance does not start with OVERSAMPLING Auto");
        require(a->effectiveOversampling()==c.choice,"Auto picks the wrong factor");
        // Bit for bit the fixed choice Auto picks; same host latency.
        auto b=std::make_unique<ChimeraProcessor>();set(*b,"oversamplingAuto",0);set(*b,"oversampling",float(c.choice));
        set(*a,"mode",2);set(*b,"mode",2);
        const auto ya=render(*a,c.rate),yb=render(*b,c.rate);
        require(ya==yb,"Auto does not sound like the fixed factor it picks");
        require(a->getLatencySamples()==b->getLatencySamples(),"Auto changes the host latency");
        std::cout<<"MEASURE "<<c.rate/1000.<<" kHz: Auto runs "<<c.name<<", identical to fixed "<<c.name<<", latency "<<a->getLatencySamples()<<"\n";
    }
    // At 48 kHz a new instance is bit for bit the released default (fixed 4x).
    auto fresh=std::make_unique<ChimeraProcessor>(),released=std::make_unique<ChimeraProcessor>();set(*released,"oversamplingAuto",0);
    require(raw(*released,"oversampling")==2,"the released default is not 4x");
    require(render(*fresh,48000)==render(*released,48000),"a new instance at 48 kHz differs from the released default");
    std::cout<<"PASS Auto follows the rate (4x / 2x / 1x) and matches the fixed factor bit for bit; at 48 kHz it is the released default\n";
}

juce::MemoryBlock save(ChimeraProcessor& p){juce::MemoryBlock m;p.getStateInformation(m);return m;}
void restore(ChimeraProcessor& p,const juce::MemoryBlock& m){p.setStateInformation(m.getData(),int(m.getSize()));}

void sessions(){
    // A session saved before Auto existed: no oversamplingAuto entry.
    for(float stored:{0.f,1.f,2.f,3.f}){
        auto source=std::make_unique<ChimeraProcessor>();set(*source,"oversampling",stored);
        auto bytes=save(*source);auto xml=juce::AudioProcessor::getXmlFromBinary(bytes.getData(),int(bytes.getSize()));require(xml!=nullptr,"state is not XML");
        for(auto* child=xml->getFirstChildElement();child!=nullptr;)
            {auto* next=child->getNextElement();if(child->getStringAttribute("id")=="oversamplingAuto")xml->removeChildElement(child,true);child=next;}
        juce::MemoryBlock legacy;juce::AudioProcessor::copyXmlToBinary(*xml,legacy);
        auto target=std::make_unique<ChimeraProcessor>();target->prepareToPlay(96000,128);restore(*target,legacy);
        require(!automatic(*target) && raw(*target,"oversampling")==stored,"a legacy session does not keep its stored OVERSAMPLING");
        require(target->effectiveOversampling()==int(stored),"a legacy session runs another factor");
    }
    std::cout<<"PASS sessions saved before Auto restore with Auto off and their stored 1x-8x, at 96 kHz too\n";
    // A saved Auto and a saved manual choice both come back as saved.
    {auto a=std::make_unique<ChimeraProcessor>();auto b=std::make_unique<ChimeraProcessor>();b->prepareToPlay(96000,128);set(*b,"oversampling",3);
     restore(*b,save(*a));require(automatic(*b) && b->effectiveOversampling()==1,"a saved Auto does not restore on");}
    {auto a=std::make_unique<ChimeraProcessor>();set(*a,"oversampling",3);require(!automatic(*a),"a host change of OVERSAMPLING leaves Auto on");
     auto b=std::make_unique<ChimeraProcessor>();restore(*b,save(*a));require(!automatic(*b) && raw(*b,"oversampling")==3,"a saved manual choice does not restore");}
    std::cout<<"PASS a saved Auto restores on; a host/automation change turns Auto off and is saved\n";
    // A/B and factory presets keep the Auto choice.
    {auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(96000,128);
     p->copyComparison();p->selectComparison(1);set(*p,"oversampling",0);require(!automatic(*p),"slot B manual choice");
     p->selectComparison(0);require(automatic(*p),"A/B switching lost Auto in slot A");
     p->selectComparison(1);require(!automatic(*p) && raw(*p,"oversampling")==0,"A/B switching lost slot B's manual 1x");
     p->selectComparison(0);
     for(int preset:{0,5,12}){p->loadFactoryPreset(preset);require(automatic(*p),"a factory preset turned Auto off");}
     // A factory preset initialises the whole sound, OVERSAMPLING included. It
     // used to write 4x; it now writes the new-instance default, Auto (4x at
     // 48 kHz, so the preset sounds as before there).
     set(*p,"oversampling",1);require(!automatic(*p),"manual 2x");p->loadFactoryPreset(3);
     require(automatic(*p),"a factory preset does not reset OVERSAMPLING to Auto");}
    std::cout<<"PASS A/B slots keep their own Auto choice; a factory preset resets OVERSAMPLING to Auto, as it used to reset it to 4x\n";
}
}

int main(){
    juce::ScopedJuceInitialiser_GUI init;
    try{followsTheRate();sessions();return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
