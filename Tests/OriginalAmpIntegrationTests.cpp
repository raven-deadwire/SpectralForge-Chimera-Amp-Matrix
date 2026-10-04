#include "OriginalAmpProcessor.h"
#include "OriginalAmpPanel.h"
#include <iostream>
#include <stdexcept>
#include <set>
#include <cstdlib>
#include <new>

namespace { bool watch=false;std::size_t allocations=0; }
void* operator new(std::size_t n){if(watch)++allocations;if(auto* p=std::malloc(std::max(n,std::size_t{1})))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

using namespace spectralforge::original;
void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
struct Host : juce::AudioProcessor {
    using juce::AudioProcessor::processBlock;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
        juce::AudioProcessorValueTreeState::ParameterLayout p;
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"old",1},"old",0,1,.33f));
        appendParameters(p);return p;
    }
    juce::AudioProcessorValueTreeState state{*this,nullptr,"LAB",layout()};
    const juce::String getName() const override{return "OriginalAmp development test";}
    void prepareToPlay(double,int) override{}void releaseResources() override{}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override{}
    double getTailLengthSeconds() const override{return 0;}
    bool acceptsMidi() const override{return false;}bool producesMidi() const override{return false;}
    juce::AudioProcessorEditor* createEditor() override{return nullptr;}bool hasEditor() const override{return false;}
    int getNumPrograms() override{return 1;}int getCurrentProgram() override{return 0;}
    void setCurrentProgram(int) override{}const juce::String getProgramName(int) override{return {};}
    void changeProgramName(int,const juce::String&) override{}
    void getStateInformation(juce::MemoryBlock& data) override{juce::MemoryOutputStream stream(data,false);state.copyState().writeToStream(stream);}
    void setStateInformation(const void* data,int size) override{const auto tree=juce::ValueTree::readFromData(data,std::size_t(size));if(tree.hasType("LAB"))state.replaceState(tree);}
};
void stateTests(bool ui,const char* imagePath) {
    Host a,b;
    const float oldValue=a.state.getRawParameterValue("old")->load();
    require(a.getParameters().size()==1+contextCount*int(controlCount+1),"Append-only lab parameter count");
    require(a.state.getParameter("old")->getParameterIndex()==0,"Appended parameters moved existing ID");
    std::set<juce::String> ids;
    Banks banks;
    for(int context=0;context<contextCount;++context) {
        banks[std::size_t(context)].enabled=(context%2)==0;
        for(std::size_t c=0;c<controlCount;++c) {
            const auto id=parameterID(context,c);require(ids.insert(id).second,"Duplicate parameter identity");
            auto* p=a.state.getParameter(id);require(p!=nullptr,"Missing new parameter");
            require(p->getVersionHint()==3,"New parameter version hint");
            const float normal=.07f+float((context*13+int(c))%80)*.01f;
            p->setValueNotifyingHost(normal);banks[std::size_t(context)].state.values[c]=p->convertFrom0to1(normal);
        }
    }
    juce::MemoryBlock data;a.getStateInformation(data);b.setStateInformation(data.getData(),int(data.getSize()));
    for(const auto& id:ids)require(std::abs(a.state.getRawParameterValue(id)->load()-b.state.getRawParameterValue(id)->load())<.001f,"APVTS binary state roundtrip");
    require(a.state.getRawParameterValue("old")->load()==oldValue,"Original bank changed unrelated parameter");
    auto tree=save(banks);Banks restored;require(restore(tree,restored),"Six-context codec restore");
    for(int i=0;i<contextCount;++i)require(banks[std::size_t(i)].state==restored[std::size_t(i)].state&&banks[std::size_t(i)].enabled==restored[std::size_t(i)].enabled,"Context state leaked");
    tree.setProperty("version",2,nullptr);require(!restore(tree,restored),"Unknown state schema accepted");
    require(restored[2].state==banks[2].state,"Rejected state partially mutated destination");
    tree=save(banks);tree.getChild(1).setProperty("index",0,nullptr);require(!restore(tree,restored),"Duplicate context accepted");
    require(restore({},restored)&&!restored[0].enabled&&restored[0].state==State{},"Old session did not get disabled default bank");
    if(ui) {
        OriginalAmpPanel panel(a.state,4);panel.setSize(720,390);panel.selectPreset(2);
        auto* gain=dynamic_cast<juce::Slider*>(panel.findChildWithID(parameterID(4,0)));
        require(gain&&gain->getTextFromValue(.72)=="7.2"&&std::abs(gain->getValueFromText("7.2")-.72)<1.e-6,"Displayed gain scale differs from entered scale");
        for(std::size_t c=0;c<controlCount;++c) {
            require(std::abs(a.state.getRawParameterValue(parameterID(4,c))->load()-presets[2].state.values[c])<.002f,"Panel preset does not reach parameter");
            require(std::abs(a.state.getRawParameterValue(parameterID(3,c))->load()-banks[3].state.values[c])<.002f,"Panel preset modified another bank");
        }
        if(imagePath) {
            const auto image=panel.createComponentSnapshot(panel.getLocalBounds());
            juce::File file(juce::String::fromUTF8(imagePath));file.deleteFile();auto stream=file.createOutputStream();
            require(stream!=nullptr&&juce::PNGImageFormat().writeImageToStream(image,*stream),"Panel screenshot write");
        }
        std::cout<<"PASS actual JUCE panel preset attachment and inactive-bank isolation\n";
    }
    std::cout<<"PASS 84 appended development parameters and six-context state\n";
}
juce::AudioBuffer<float> render(double rate,unsigned os,int block) {
    OriginalAmpProcessor amp;amp.prepare(rate,64,os);State s;amp.set(s);amp.reset();
    require(std::isfinite(amp.latency())&&amp.latency()>=0&&amp.latency()<64&&amp.latency()==std::floor(amp.latency()),"Oversampler latency contract");
    juce::AudioBuffer<float> out(2,4096),temp(2,block);out.clear();
    for(int offset=0;offset<out.getNumSamples();offset+=block) {
        const int count=std::min(block,out.getNumSamples()-offset);temp.setSize(2,count,false,false,true);temp.clear();
        for(int n=0;n<count;++n)temp.setSample(0,n,float(.035*std::sin(2*detail::pi*82.4*(offset+n)/rate)));
        watch=true;amp.set(s);amp.process(temp);watch=false;
        for(int c=0;c<2;++c)out.copyFrom(c,offset,temp,c,0,count);
    }
    return out;
}
int main(int argc,char** argv) {
    try {
        juce::ScopedJuceInitialiser_GUI init;stateTests(argc>1,argc>2?argv[2]:nullptr);
        double difference=0;float peak=0;
        for(double rate:{44100.,48000.,96000.})for(unsigned os:{1u,2u,4u,8u}) {
            const auto a=render(rate,os,64),b=render(rate,os,257);
            require(a.getRMSLevel(0,0,a.getNumSamples())>.001,"Silent oversampling route");
            for(int n=0;n<a.getNumSamples();++n) {
                const float x=a.getSample(0,n);require(std::isfinite(x)&&a.getSample(1,n)==0,"Stereo crosstalk/nonfinite output");
                difference=std::max(difference,std::abs(double(x)-b.getSample(0,n)));peak=std::max(peak,std::abs(x));
            }
        }
        require(difference<1.e-6&&peak<4,"Block partition or peak regression");
        require(allocations==0,"Oversampling adapter allocated while processing");
        std::cout<<"PASS 12 JUCE oversampling routes; partition residual="<<difference<<" peak="<<peak<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
