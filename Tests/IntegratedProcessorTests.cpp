#include "PluginProcessor.h"
#include "GuitarSignaturePresets.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* parameter=p.parameters().getParameter(id);require(parameter!=nullptr,"Unknown preset/test parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
std::vector<float> render(ChimeraProcessor& p) {
    p.prepareToPlay(48000,128);juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;std::vector<float> result;
    for(int block=0;block<120;++block) {
        for(int n=0;n<128;++n) {
            const double t=double(block*128+n)/48000;
            const float envelope=(block%24)<18?1.f:.05f;
            const float x=envelope*float(.10*std::sin(juce::MathConstants<double>::twoPi*110*t)+.04*std::sin(juce::MathConstants<double>::twoPi*165*t));
            b.setSample(0,n,x);b.setSample(1,n,-.7f*x);
        }
        p.processBlock(b,midi);
        for(int c=0;c<2;++c)for(int n=0;n<128;++n) {const auto x=b.getSample(c,n);require(std::isfinite(x) && std::abs(x)<4,"Signature output invalid/unbounded");result.push_back(x);}
    }
    p.releaseResources();return result;
}
void signatures(const juce::File& directory) {
    require(directory.createDirectory().wasOk(),"Cannot create signature evidence directory");
    for(int song=0;song<4;++song) {
        const auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
        // Dirty every parameter bank before recall, including unrelated models.
        for(auto* raw:a->getParameters())raw->setValueNotifyingHost(.81f);
        a->loadFactoryPreset(spectralforge::factoryPresetCount+song);
        b->loadFactoryPreset(spectralforge::factoryPresetCount+song);
        for(int i=0;i<a->getParameters().size();++i) {
            const auto av=a->getParameters()[i]->getValue(),bv=b->getParameters()[i]->getValue();
            if(std::abs(av-bv)>1e-6f) {std::cerr<<"DIRTY "<<a->getParameters()[i]->getName(100)<<" a="<<av<<" b="<<bv<<'\n';require(false,"Signature inherited a dirty parameter bank");}
        }
        const auto snapshot=spectralforge::guitarSignatureSnapshot(a->parameters(),song);
        int params=0;for(auto child:snapshot)if(child.hasProperty("id") && child.hasProperty("value"))++params;
        require(params==a->getParameters().size(),"Full signature snapshot omitted APVTS parameters");
        require(snapshot.getChildWithName("GUITAR_SIGNATURE").getProperty("id").toString()==spectralforge::guitarSignatures[size_t(song)].id,"Signature identity missing");
        auto xml=snapshot.createXml();require(xml->writeTo(directory.getChildFile(juce::String(song)+".xml")),"Snapshot export failed");
        juce::MemoryBlock state;a->getStateInformation(state);b->setStateInformation(state.getData(),int(state.getSize()));
        for(int i=0;i<a->getParameters().size();++i)require(std::abs(a->getParameters()[i]->getValue()-b->getParameters()[i]->getValue())<1e-6f,"Signature binary project restore differs");
        const auto first=render(*a),restored=render(*b);double energy=0;float delta=0;
        for(size_t i=0;i<first.size();++i) {delta=std::max(delta,std::abs(first[i]-restored[i]));energy+=double(first[i])*first[i];}
        require(delta<1e-6f && energy>1e-5,"Signature saved-state synthetic audio roundtrip failed");
        a->copyComparison();a->selectComparison(1);a->selectComparison(0);
        require(a->parameters().state.getChildWithName("GUITAR_SIGNATURE").getProperty("id").toString()==spectralforge::guitarSignatures[size_t(song)].id,"Signature metadata lost in A/B");
        std::cout<<"PASS full-state "<<spectralforge::guitarSignatures[size_t(song)].name<<" parameters="<<params<<" audio_delta="<<delta<<" energy="<<energy<<" synthetic only\n";
    }
}
void gainAndGR() {
    for(bool board:{false,true})for(int mode:{0,1,2}) {
        const auto p=std::make_unique<ChimeraProcessor>();
        set(*p,"gateon",0);set(*p,"input",0);set(*p,"output",0);set(*p,"mode",float(mode));set(*p,"boardEnabled",board?1.f:0.f);set(*p,"oversampling",0);set(*p,"lowampmix",1);
        for(int lane=1;lane<=3;++lane){set(*p,"ampon"+juce::String(lane),0);set(*p,"cab"+juce::String(lane),0);}
        p->prepareToPlay(48000,128);juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;double input=0,output=0;
        for(int block=0;block<300;++block) {
            for(int n=0;n<128;++n) {const float x=.2f*std::sin(float(block*128+n)*.07f);audio.setSample(0,n,x);audio.setSample(1,n,-.7f*x);if(block>100)input+=double(x)*x;}
            p->processBlock(audio,midi);if(block>100)for(int n=0;n<128;++n)output+=std::pow(audio.getSample(0,n),2);
        }
        const auto db=10*std::log10(output/input);require(std::abs(db)<.05,"Unity routing unexpectedly loses level");
        require(p->getLatencySamples()<p->pitchLatency(),"Zero-shift/bypass retains pitch-window latency");p->releaseResources();
        std::cout<<"MEASURE unity mode="<<mode<<" board="<<board<<" delta_db="<<db<<" zero_shift_reported="<<p->getLatencySamples()<<'\n';
    }
    for(int model=6;model<=10;++model) {
        const auto p=std::make_unique<ChimeraProcessor>();set(*p,"gateon",0);set(*p,"output",0);p->setPedalModel(0,model);
        for(int c=0;c<spectralforge::pedalModel(model).controlCount;++c)set(*p,spectralforge::pedalControlID(0,model,c),spectralforge::pedalModel(model).controls[size_t(c)].maximum);
        p->prepareToPlay(48000,128);juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
        for(int block=0;block<100;++block){for(int c=0;c<2;++c)for(int n=0;n<128;++n)audio.setSample(c,n,.5f);p->processBlock(audio,midi);}
        require(p->pedalReduction()>.1f && std::isfinite(p->pedalReduction()),"Compressor DSP GR did not reach processor meter");
        set(*p,spectralforge::pedalBypassID(0,model),1);for(int i=0;i<20;++i){audio.clear();p->processBlock(audio,midi);}
        require(p->pedalReduction()==0,"Bypassed compressor retains stale GR");p->releaseResources();
        std::cout<<"PASS processor pedal GR model="<<model<<" active and bypass\n";
    }
}
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    try {signatures(juce::File(argc>1?argv[1]:"/tmp/chimera-signatures"));gainAndGR();}
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
