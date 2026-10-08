#include "CabPanel.h"
#include <iostream>
#include <stdexcept>
#include <vector>
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
void set(ChimeraProcessor& p,const juce::String& id,float v){auto* a=p.parameters().getParameter(id);check(a!=nullptr,"parameter missing");a->setValueNotifyingHost(a->convertTo0to1(v));}
const std::array<const char*,10> cabIds{"cabblend","cabAgain","cabAdelay","cabAinvert","cabBgain","cabBdelay","cabBinvert","cabBlow","cabBhigh","cabBtype"};
void dirtyCab(ChimeraProcessor& p) {
    for(int lane=1;lane<=3;++lane)for(const auto* prefix:cabIds) {
        const auto id=juce::String(prefix)+juce::String(lane);
        auto* parameter=p.parameters().getParameter(id);
        parameter->setValueNotifyingHost(parameter->getDefaultValue()==1.f ? 0.f : 1.f);
    }
}
void neutralCab(ChimeraProcessor& p) {
    const std::array<float,10> expected{0,0,0,0,0,0,0,70,9000,0};
    const auto tree=p.parameters().copyState();
    for(int lane=1;lane<=3;++lane)for(size_t i=0;i<cabIds.size();++i) {
        const auto id=juce::String(cabIds[i])+juce::String(lane);
        const float actual=p.parameters().getRawParameterValue(id)->load();
        if(actual!=expected[i])std::cerr<<id<<" expected "<<expected[i]<<" actual "<<actual<<'\n';
        check(actual==expected[i],"legacy CAB exact default");
        check(float(tree.getChildWithProperty("id",id).getProperty("value"))==expected[i],"legacy CAB serialized default");
        auto* parameter=p.parameters().getParameter(id);
        check(parameter->convertFrom0to1(parameter->getValue())==expected[i],"legacy CAB host default");
        check(p.micName(lane-1,1).isEmpty(),"legacy absent B asset");
    }
}
void fixture(const juce::File& f,int offset){juce::WavAudioFormat format;auto stream=f.createOutputStream();auto w=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.release(),48000,1,24,{},0));check(w!=nullptr,"writer");juce::AudioBuffer<float> b(1,512);b.clear();b.setSample(0,offset,.5f);b.setSample(0,offset+7,.15f);check(w->writeFromAudioSampleBuffer(b,0,512),"write");}
std::vector<float> render(ChimeraProcessor& p){p.prepareToPlay(48000,128);juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;for(int i=0;i<64;++i){b.clear();p.processBlock(b,midi);}std::vector<float> out;for(int block=0;block<64;++block){for(int c=0;c<2;++c)for(int n=0;n<128;++n)b.setSample(c,n,.05f*std::sin(float(block*128+n)*(.073f+c*.027f)));p.processBlock(b,midi);for(int n=0;n<128;++n)for(int c=0;c<2;++c){check(std::isfinite(b.getSample(c,n)),"finite production output");out.push_back(b.getSample(c,n));}}p.releaseResources();check(p.backgroundResourcesReleased(),"A/B worker and kernel teardown");return out;}
void equal(const std::vector<float>& a,const std::vector<float>& b){float d=0;double energy=0;check(a.size()==b.size(),"length");for(size_t i=0;i<a.size();++i){d=std::max(d,std::abs(a[i]-b[i]));energy+=a[i]*a[i];}check(energy>1e-8,"audible response");check(d<1e-6f,"production project audio restore");}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("cab-state-fixture",{},false);struct Cleanup{juce::File f;~Cleanup(){f.deleteRecursively();}} cleanup{folder};try{check(folder.createDirectory().wasOk(),"directory");auto a=folder.getChildFile("A.wav"),b=folder.getChildFile("B.wav");if(argc==4) {
        check(juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]).copyFileTo(a),"private A copy");
        check(juce::File::getCurrentWorkingDirectory().getChildFile(argv[3]).copyFileTo(b),"private B copy");
    } else {check(argc<=2,"usage: ChimeraCabPanelStateTests [snapshot.png [user-IR-A user-IR-B]]");fixture(a,0);fixture(b,18);}
    auto p=std::make_unique<ChimeraProcessor>();
    set(*p,"gateon",0);set(*p,"oversampling",0);set(*p,"lowampmix",1);
    for(int lane=0;lane<3;++lane){const auto n=juce::String(lane+1);set(*p,"ampon"+n,0);check(p->loadMicIR(lane,0,a).wasOk() && p->loadMicIR(lane,1,b).wasOk(),"production IR imports");set(*p,"cabblend"+n,.31f+float(lane)*.12f);set(*p,"cabAgain"+n,-3);set(*p,"cabBgain"+n,-5);set(*p,"cabAdelay"+n,.7f);set(*p,"cabBdelay"+n,1.3f);set(*p,"cabBinvert"+n,1);set(*p,"cabBlow"+n,120);set(*p,"cabBhigh"+n,6000);}
    juce::MemoryBlock saved;p->getStateInformation(saved);check(a.deleteFile() && b.deleteFile(),"delete sources");
    for(int mode=0;mode<3;++mode){for(int dual=0;dual<(mode==1?2:1);++dual){set(*p,"mode",float(mode));set(*p,"dualtype",float(dual));p->getStateInformation(saved);auto original=render(*p);auto restored=std::make_unique<ChimeraProcessor>();dirtyCab(*restored);restored->setStateInformation(saved.getData(),int(saved.getSize()));equal(original,render(*restored));check(restored->micName(2,1)=="B.wav","Mic B filename restore");std::cout<<"PASS routing "<<mode<<" dual "<<dual<<" embedded project\n";}}
    p->copyComparison();set(*p,"cabblend1",.9f);p->selectComparison(1);check(std::abs(p->parameters().getRawParameterValue("cabblend1")->load()-.31f)<1e-6f,"A/B snapshot parameter restore");check(p->micName(0,1)=="B.wav","A/B embedded Mic B");
    if(argc>1){p->prepareToPlay(48000,128);CabPanel panel(*p,0);
        auto* polarity=dynamic_cast<juce::TextButton*>(panel.findChildWithID("cabBinvert1"));
        check(polarity && polarity->getClickingTogglesState(),"polarity click interaction");
        polarity->setToggleState(false,juce::sendNotification);check(p->parameters().getRawParameterValue("cabBinvert1")->load()==0,"polarity UI binding");
        polarity->setToggleState(true,juce::sendNotification);
        auto* blend=dynamic_cast<juce::Slider*>(panel.findChildWithID("cabblend1"));check(blend && blend->getTextFromValue(.31).contains("31.0%"),"blend percentage display");
        auto image=panel.createComponentSnapshot(panel.getLocalBounds());check(image.isValid(),"panel snapshot");auto out=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).createOutputStream();check(out!=nullptr,"snapshot stream");check(out->setPosition(0) && out->truncate().wasOk(),"snapshot overwrite");juce::PNGImageFormat png;check(png.writeImageToStream(image,*out),"snapshot write");p->releaseResources();}
    // Old project children have no slot tag and no new controls. Migration must
    // reset dirty current values rather than inheriting a previous CAB edit.
    p->selectComparison(0);
    for(int lane=0;lane<3;++lane) {
        const auto n=juce::String(lane+1);
        set(*p,"cabblend"+n,0);set(*p,"cabAgain"+n,0);set(*p,"cabAdelay"+n,0);set(*p,"cabAinvert"+n,0);
    }
    p->getStateInformation(saved);auto legacy=juce::ValueTree::fromXml(*juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize())));
    legacy.removeChild(legacy.getChildWithName("COMPARISONS"),nullptr);
    for(int i=legacy.getNumChildren();--i>=0;) {
        auto child=legacy.getChild(i);const auto id=child.getProperty("id").toString();
        if(id.startsWith("cabA") || id.startsWith("cabB") || id.startsWith("cabblend"))legacy.removeChild(i,nullptr);
    }
    auto irs=legacy.getChildWithName("USER_IRS");
    for(int i=irs.getNumChildren();--i>=0;) {
        auto child=irs.getChild(i);
        if(int(child.getProperty("slot",0))==1)irs.removeChild(i,nullptr);else child.removeProperty("slot",nullptr);
    }
    legacy.setProperty("schemaVersion",9,nullptr);
    auto baseline=render(*p);
    dirtyCab(*p);
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),saved);p->setStateInformation(saved.getData(),int(saved.getSize()));
    check(p->parameters().getRawParameterValue("cabblend1")->load()==0,"legacy blend default");
    check(p->parameters().getRawParameterValue("cabAgain1")->load()==0,"legacy level default");
    neutralCab(*p);
    check(p->micName(0,1).isEmpty(),"legacy absent B asset");equal(baseline,render(*p));
    std::cout<<"PASS legacy A-only project migration\n";
    // Repeat dirty-session migration in every production routing mode.
    for(int mode=0;mode<3;++mode)for(int dual=0;dual<(mode==1?2:1);++dual) {
        legacy.getChildWithProperty("id","mode").setProperty("value",mode,nullptr);
        legacy.getChildWithProperty("id","dualtype").setProperty("value",dual,nullptr);
        juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),saved);
        p->setStateInformation(saved.getData(),int(saved.getSize()));auto clean=render(*p);
        dirtyCab(*p);p->setStateInformation(saved.getData(),int(saved.getSize()));
        neutralCab(*p);equal(clean,render(*p));
        // Legacy comparison slots use the same migration, including reloads.
        auto withSlots=legacy.createCopy();juce::ValueTree slots("COMPARISONS");
        for(int slot=0;slot<2;++slot){auto core=legacy.createCopy();core.setProperty("slot",slot,nullptr);slots.appendChild(core,nullptr);}
        withSlots.appendChild(slots,nullptr);
        juce::AudioProcessor::copyXmlToBinary(*withSlots.createXml(),saved);
        p->setStateInformation(saved.getData(),int(saved.getSize()));dirtyCab(*p);p->selectComparison(1);
        neutralCab(*p);equal(clean,render(*p));
        p->getStateInformation(saved);auto reloaded=std::make_unique<ChimeraProcessor>();dirtyCab(*reloaded);
        reloaded->setStateInformation(saved.getData(),int(saved.getSize()));neutralCab(*reloaded);equal(clean,render(*reloaded));
    }
    // Non-neutral gains keep the original normalized automation mapping.
    for(int lane=1;lane<=3;++lane)for(const auto* prefix:{"cabAgain","cabBgain"}) {
        auto* parameter=p->parameters().getParameter(juce::String(prefix)+juce::String(lane));
        for(float value:{-24.f,-20.f,-3.f,0.f,6.f,12.f}) {
            const juce::NormalisableRange<float> legacyRange{-24.f,12.f,.01f};
            check(parameter->getNormalisableRange().interval==legacyRange.interval,"CAB gain grid compatibility");
            const float expectedNormalised=value==0 ? 24.f/36.f : legacyRange.convertTo0to1(legacyRange.snapToLegalValue(value));
            check(parameter->convertTo0to1(value)==expectedNormalised,"CAB gain automation compatibility");
            if(value==0) {
                const float unity=parameter->convertTo0to1(value);
                check(parameter->convertFrom0to1(unity)==0,"exact unity mapping");
                const float adjacent=parameter->convertTo0to1(.01f);
                check(parameter->convertFrom0to1(adjacent)>0,"adjacent gain automation retained");
                std::cout<<"legacy FMA unity residual "<<std::fma(36.f,unity,-24.f)<<" dB; canonical unity 0 dB\n";
            }
        }
    }
    std::cout<<"PASS dirty legacy routes, comparison slots, re-save and automation mapping\n";

    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
