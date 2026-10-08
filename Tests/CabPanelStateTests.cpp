#include "CabPanel.h"
#include <iostream>
#include <stdexcept>
#include <vector>
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
void set(ChimeraProcessor& p,const juce::String& id,float v){auto* a=p.parameters().getParameter(id);check(a!=nullptr,"parameter missing");a->setValueNotifyingHost(a->convertTo0to1(v));}
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
    for(int mode=0;mode<3;++mode){for(int dual=0;dual<(mode==1?2:1);++dual){set(*p,"mode",float(mode));set(*p,"dualtype",float(dual));p->getStateInformation(saved);auto original=render(*p);auto restored=std::make_unique<ChimeraProcessor>();restored->setStateInformation(saved.getData(),int(saved.getSize()));equal(original,render(*restored));check(restored->micName(2,1)=="B.wav","Mic B filename restore");std::cout<<"PASS routing "<<mode<<" dual "<<dual<<" embedded project\n";}}
    p->copyComparison();set(*p,"cabblend1",.9f);p->selectComparison(1);check(std::abs(p->parameters().getRawParameterValue("cabblend1")->load()-.31f)<1e-6f,"A/B snapshot parameter restore");check(p->micName(0,1)=="B.wav","A/B embedded Mic B");
    if(argc>1){CabPanel panel(*p,0);auto image=panel.createComponentSnapshot(panel.getLocalBounds());check(image.isValid(),"panel snapshot");auto out=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).createOutputStream();check(out!=nullptr,"snapshot stream");juce::PNGImageFormat png;check(png.writeImageToStream(image,*out),"snapshot write");}
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
    set(*p,"cabblend1",1);set(*p,"cabAgain1",-20);set(*p,"cabAdelay1",15);
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),saved);p->setStateInformation(saved.getData(),int(saved.getSize()));
    check(p->parameters().getRawParameterValue("cabblend1")->load()==0,"legacy blend default");
    check(p->parameters().getRawParameterValue("cabAgain1")->load()==0,"legacy level default");
    check(p->micName(0,1).isEmpty(),"legacy absent B asset");equal(baseline,render(*p));
    std::cout<<"PASS legacy A-only project migration\n";

    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
