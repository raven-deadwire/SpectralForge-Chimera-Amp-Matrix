#include "CabPanel.h"
#include "IRStateAssets.h"
#include "CabMicrophoneUITests.h"
#include "CabIntegratedUITests.h"
#include "CabSceneUITests.h"
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
#include "CabExpansionStateTests.h"
void originalStateContracts() {
    auto p=std::make_unique<ChimeraProcessor>();
    set(*p,"gateon",0);set(*p,"oversampling",0);set(*p,"lowampmix",1);
    for(int lane=0;lane<3;++lane) {
        set(*p,"ampon"+juce::String(lane+1),0);
        auto setModel=[&](const char* suffix,float value){set(*p,spectralforge::originalCabID(lane,suffix),value);};
        setModel("design",1);setModel("rear",1);setModel("tweeter",.25f);
        setModel("Aon",1);setModel("Bon",1);setModel("Bmic",2);setModel("Bunit",1);
        setModel("Aposition",.7f);setModel("Bdistance",23.f);set(*p,"cabblend"+juce::String(lane+1),.4f);
    }
    for(int mode=0;mode<3;++mode)for(int dual=0;dual<(mode==1?2:1);++dual) {
        set(*p,"mode",float(mode));set(*p,"dualtype",float(dual));
        juce::MemoryBlock data;p->getStateInformation(data);
        auto restored=std::make_unique<ChimeraProcessor>();restored->setStateInformation(data.getData(),int(data.getSize()));
        equal(render(*p),render(*restored));
        check(restored->parameters().getRawParameterValue("ocab1_Bunit")->load()==1,"independent unit restoration");
        std::cout<<"PASS original CAB production state mode="<<mode<<" dual="<<dual<<'\n';
    }
    set(*p,"mode",2);
    for(int os=0;os<4;++os)for(float lowMix:{0.f,.5f,1.f}) {
        set(*p,"oversampling",float(os));set(*p,"lowampmix",lowMix);
        juce::MemoryBlock data;p->getStateInformation(data);auto restored=std::make_unique<ChimeraProcessor>();restored->setStateInformation(data.getData(),int(data.getSize()));equal(render(*p),render(*restored));
    }
    std::cout<<"PASS original Matrix low DI 0/50/100 and 1x/2x/4x/8x restoration\n";
    set(*p,"oversampling",0);set(*p,"lowampmix",1);
    auto a=render(*p);p->copyComparison();set(*p,"ocab1_Adistance",55);p->selectComparison(1);equal(a,render(*p));
    // Actual production path, not only generator coefficients.
    set(*p,"mode",0);set(*p,"cabblend1",0);const auto before=render(*p);
    set(*p,"ocab1_Aposition",0);const auto after=render(*p);float diff=0;
    for(size_t n=0;n<before.size();++n)diff=std::max(diff,std::abs(before[n]-after[n]));check(diff>1e-5,"position reaches production output");
    // Each shared/slot control must affect production audio, not just serialize
    // or update a label. A-only isolates the independently selected microphone.
    // Off-centre avoids the physical mirror symmetry of identical cone centres.
    set(*p,"ocab1_Aposition",.63f);
    for(const auto& change:std::array<std::pair<const char*,float>,6>{{
            {"design",0},{"rear",0},{"tweeter",.9f},{"Amic",2},{"Aunit",3},{"Adistance",42}}}) {
        const auto id=spectralforge::originalCabID(0,change.first);
        const float old=p->parameters().getRawParameterValue(id)->load();
        const auto baseline=render(*p);set(*p,id,change.second);const auto changed=render(*p);
        float maximum=0;for(size_t n=0;n<baseline.size();++n)maximum=std::max(maximum,std::abs(baseline[n]-changed[n]));
        if(maximum<=1e-5f)std::cerr<<"Inaudible control "<<id<<" difference="<<maximum<<'\n';
        check(maximum>1e-5f,"original control does not reach production audio");set(*p,id,old);
    }
    CabPanel panel(*p,0);
    // Nested controls remain host-bound and legible in the real CAB panel.
    std::function<juce::Component*(juce::Component&,const juce::String&)> find=[&](juce::Component& root,const juce::String& id)->juce::Component* {
        if(root.getComponentID()==id)return &root;for(auto* child:root.getChildren())if(auto* found=find(*child,id))return found;return nullptr;
    };
    auto* distance=dynamic_cast<juce::Slider*>(find(panel,"ocab1_Adistance"));check(distance!=nullptr,"modeled distance UI");
    distance->setValue(33,juce::sendNotificationSync);check(std::abs(p->parameters().getRawParameterValue("ocab1_Adistance")->load()-33)<.11f,"distance UI binding");
    auto preview=panel.createComponentSnapshot(panel.getLocalBounds());
    auto output=juce::File::getCurrentWorkingDirectory().getChildFile("original-cab-panel.png").createOutputStream();
    check(output!=nullptr && output->setPosition(0) && output->truncate().wasOk(),"original screenshot output");
    juce::PNGImageFormat png;check(png.writeImageToStream(preview,*output),"original screenshot");
    const auto personal=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("original-cab-user-fixture",".wav",false);
    fixture(personal,7);check(p->loadMicIR(0,0,personal).wasOk(),"mixed User IR / original import");check(personal.deleteFile(),"mixed source deletion");
    check(p->parameters().getRawParameterValue("ocab1_Aon")->load()==0 && p->parameters().getRawParameterValue("ocab1_Bon")->load()==1,"import switches only its own slot");
    set(*p,"cabblend1",.5f);
    for(int mode=0;mode<3;++mode)for(int dual=0;dual<(mode==1?2:1);++dual) {
        set(*p,"mode",float(mode));set(*p,"dualtype",float(dual));juce::MemoryBlock mixed;p->getStateInformation(mixed);
        auto mixedRecall=std::make_unique<ChimeraProcessor>();mixedRecall->setStateInformation(mixed.getData(),int(mixed.getSize()));equal(render(*p),render(*mixedRecall));
    }
    // Comparison recall crosses the actual capture/model boundary with its
    // original source file already absent.
    const int mixedSlot=1-p->comparisonSlot();p->copyComparison();const auto mixedAudio=render(*p);set(*p,"ocab1_Aon",1);p->selectComparison(mixedSlot);
    equal(mixedAudio,render(*p));check(p->parameters().getRawParameterValue("ocab1_Aon")->load()==0,"comparison lost captured source mode");
    // Legacy reset is checked from a deliberately dirty modeled session.
    set(*p,"ocab1_Aon",1);
    auto legacy=p->parameters().copyState();
    for(int n=legacy.getNumChildren();--n>=0;)if(legacy.getChild(n).getProperty("id").toString().startsWith("ocab"))legacy.removeChild(n,nullptr);
    juce::MemoryBlock old;juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),old);
    p->setStateInformation(old.getData(),int(old.getSize()));
    for(int lane=0;lane<3;++lane)for(const char* suffix:{"Aon","Bon"})check(p->parameters().getRawParameterValue(spectralforge::originalCabID(lane,suffix))->load()==0,"legacy must disable dirty modeled slots");
    std::cout<<"PASS original A/B comparison, position audio, UI and legacy disable\n";
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("cab-state-fixture",{},false);struct Cleanup{juce::File f;~Cleanup(){f.deleteRecursively();}} cleanup{folder};try{if(argc==2 && juce::String(argv[1])=="--expansion-only") {expansionStateContracts(juce::File::getCurrentWorkingDirectory());return 0;}originalStateContracts();check(folder.createDirectory().wasOk(),"directory");auto a=folder.getChildFile("A.wav"),b=folder.getChildFile("B.wav");if(argc==4) {
        check(juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]).copyFileTo(a),"private A copy");
        check(juce::File::getCurrentWorkingDirectory().getChildFile(argv[3]).copyFileTo(b),"private B copy");
    } else {check(argc<=2,"usage: ChimeraCabPanelStateTests [snapshot.png [user-IR-A user-IR-B]]");fixture(a,0);fixture(b,18);}
    cabMicrophoneUITests::run(folder.getChildFile("microphone-ui"),argc>1
        ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).getParentDirectory() : juce::File{});
    cabIntegratedUITests::run(folder.getChildFile("integrated-ui"),argc>1
        ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).getParentDirectory() : juce::File{});
    cabSceneUITests::run(folder.getChildFile("scene-ui"),argc>1
        ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).getParentDirectory() : juce::File{});
    check(!juce::SharedResourcePointer<spectralforge::cabArt::Bank>::getSharedObjectWithoutCreating(),
        "CAB artwork survives the final panel owner");
    std::cout<<"PASS CAB artwork lifetime: every image released after the last panel closes\n";
    auto p=std::make_unique<ChimeraProcessor>();
    set(*p,"gateon",0);set(*p,"oversampling",0);set(*p,"lowampmix",1);
    for(int lane=0;lane<3;++lane){const auto n=juce::String(lane+1);set(*p,"ampon"+n,0);check(p->loadMicIR(lane,0,a).wasOk() && p->loadMicIR(lane,1,b).wasOk(),"production IR imports");set(*p,"cabblend"+n,.31f+float(lane)*.12f);set(*p,"cabAgain"+n,-3);set(*p,"cabBgain"+n,-5);set(*p,"cabAdelay"+n,.7f);set(*p,"cabBdelay"+n,1.3f);set(*p,"cabBinvert"+n,1);set(*p,"cabBlow"+n,120);set(*p,"cabBhigh"+n,6000);}
    juce::MemoryBlock saved;p->getStateInformation(saved);
    auto pooled=juce::ValueTree::fromXml(*juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize())));
    check(pooled.getChildWithName("IR_ASSETS").getNumChildren()==2,"same files pooled across six slots and active comparison");
    check(!pooled.getChildWithName("USER_IRS").getChild(0).hasProperty("data"),"IR reference has no inline payload");
    check(a.deleteFile() && b.deleteFile(),"delete sources");
    for(int mode=0;mode<3;++mode){for(int dual=0;dual<(mode==1?2:1);++dual){set(*p,"mode",float(mode));set(*p,"dualtype",float(dual));p->getStateInformation(saved);auto original=render(*p);auto restored=std::make_unique<ChimeraProcessor>();dirtyCab(*restored);restored->setStateInformation(saved.getData(),int(saved.getSize()));equal(original,render(*restored));check(restored->micName(2,1)=="B.wav","Mic B filename restore");std::cout<<"PASS routing "<<mode<<" dual "<<dual<<" embedded project\n";}}
    p->copyComparison();set(*p,"cabblend1",.9f);p->selectComparison(1);check(std::abs(p->parameters().getRawParameterValue("cabblend1")->load()-.31f)<1e-6f,"A/B snapshot parameter restore");check(p->micName(0,1)=="B.wav","A/B embedded Mic B");
    if(argc>1){p->prepareToPlay(48000,128);CabPanel panel(*p,0);
        panel.setView(CabPanel::View::irLoader);
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
    check(spectralforge::irState::unpack(legacy),"expand fixture to actual legacy inline format");
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

    // Six distinct original stereo WAVs close to the unchanged 4 MiB import cap.
    auto large=std::make_unique<ChimeraProcessor>();
    for(int i=0;i<6;++i) {
        auto f=folder.getChildFile("large"+juce::String(i)+".wav");
        juce::WavAudioFormat format;auto stream=f.createOutputStream();
        auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.release(),384000,2,32,{},0));
        check(writer!=nullptr,"large writer");juce::AudioBuffer<float> buffer(2,384000);buffer.clear();
        buffer.setSample(0,i+1,.5f);buffer.setSample(1,i+20,.25f);
        check(writer->writeFromAudioSampleBuffer(buffer,0,buffer.getNumSamples()),"large write");writer.reset();
        // Valid ancillary JUNK padding exercises file-byte limits without
        // relaxing the existing <=1-second, <=384-kHz decoder contract.
        auto padding=f.createOutputStream();check(padding!=nullptr,"padding stream");
        const auto originalSize=f.getSize();const int padBytes=int(4*1024*1024-64-originalSize-8);
        check(padBytes>0 && padding->setPosition(originalSize),"padding position");
        padding->write("JUNK",4);padding->writeInt(padBytes);
        juce::MemoryBlock zeros(size_t(padBytes),true);padding->write(zeros.getData(),zeros.getSize());
        check(padding->setPosition(4),"RIFF length position");padding->writeInt(4*1024*1024-64-8);padding.reset();
        check(f.getSize()>4000000 && f.getSize()<=4*1024*1024,"near-limit stereo fixture");
        check(large->loadMicIR(i%3,i/3,f).wasOk(),"unchanged import cap accepts large IR");
        check(f.deleteFile(),"large source deletion");
    }
    large->copyComparison();large->selectComparison(1);large->copyComparison();
    juce::MemoryBlock big;check(large->tryGetStateInformation(big),"six distinct assets fit reader budget");
    auto bigTree=juce::ValueTree::fromXml(*juce::AudioProcessor::getXmlFromBinary(big.getData(),int(big.getSize())));
    check(bigTree.getChildWithName("IR_ASSETS").getNumChildren()==6,"six hashes across both comparisons");
    check(big.getSize()<spectralforge::irState::maxStateBytes,"exact serialized budget");
    auto largeRestored=std::make_unique<ChimeraProcessor>();largeRestored->setStateInformation(big.getData(),int(big.getSize()));
    for(int slot=0;slot<2;++slot) {largeRestored->selectComparison(slot);for(int i=0;i<6;++i)check(largeRestored->micName(i%3,i/3)=="large"+juce::String(i)+".wav","large assets restore without files in either comparison");}
    // Corrupt hashes and dangling references reject the whole project before mutation.
    auto bad=bigTree.createCopy();bad.getChildWithName("IR_ASSETS").getChild(0).setProperty("sha256",juce::String::repeatedString("0",64),nullptr);
    juce::MemoryBlock malformed;juce::AudioProcessor::copyXmlToBinary(*bad.createXml(),malformed);
    set(*largeRestored,"cabblend1",.73f);largeRestored->setStateInformation(malformed.getData(),int(malformed.getSize()));
    check(std::abs(largeRestored->parameters().getRawParameterValue("cabblend1")->load()-.73f)<1e-6f,"hash rejection preserves session");
    bad=bigTree.createCopy();bad.getChildWithName("IR_ASSETS").removeChild(0,nullptr);
    juce::AudioProcessor::copyXmlToBinary(*bad.createXml(),malformed);
    largeRestored->setStateInformation(malformed.getData(),int(malformed.getSize()));
    check(std::abs(largeRestored->parameters().getRawParameterValue("cabblend1")->load()-.73f)<1e-6f,"dangling reference preserves session");
    // Force an actual UTF-8 serialized overflow, including multibyte text.
    check(spectralforge::irState::unpack(bigTree),"expand large test tree");
    bigTree.setProperty("oversize",juce::String::repeatedString("界",24*1024*1024),nullptr);
    juce::MemoryBlock retained("keep",4);check(!spectralforge::irState::serialize(bigTree,retained),"oversize save refused");
    check(retained.getSize()==4 && std::memcmp(retained.getData(),"keep",4)==0,"failed save preserves destination");
    auto before=largeRestored->parameters().copyState();
    before.setProperty("oversize",juce::String::repeatedString("x",65*1024*1024),nullptr);
    largeRestored->parameters().replaceState(before);
    const auto selected=largeRestored->comparisonSlot();
    check(!largeRestored->tryGetStateInformation(retained),"processor overflow save refused");
    check(retained.getSize()==4 && std::memcmp(retained.getData(),"keep",4)==0,"processor destination retained");
    check(largeRestored->comparisonSlot()==selected,"failed save keeps active comparison");
    before.removeProperty("oversize",nullptr);largeRestored->parameters().replaceState(before);
    largeRestored->selectComparison(1-selected);
    check(largeRestored->micName(0,0)=="large0.wav","failed save keeps inactive comparison assets");
    std::cout<<"PASS shared hashes, six near-limit stereo files, deleted-file comparisons, hash rejection and exact UTF-8 overflow; bytes="<<big.getSize()<<'\n';

    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
