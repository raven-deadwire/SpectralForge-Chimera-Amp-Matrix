// Real beta.2 bytes are never manufactured by this regression. Synthetic and
// corrupt-metadata coverage remains in GraphicalEQEditingTests.cpp.
#include "PluginProcessor.h"
#include "EQParameterAssertions.h"
#include "EQNormalizationContract.h"
#include "IRStateAssets.h"
#include <array>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
using namespace spectralforge;
namespace {
void check(bool b,const juce::String& message) {if(!b)throw std::runtime_error(message.toStdString());}
using Values=std::array<float,4967>;
int valueFailures=0;
Values values(ChimeraProcessor& p) {
    check(p.getParameters().size()==4967,"released parameter count"); Values v{};
    for(int i=0;i<4967;++i) {
        auto* parameter=dynamic_cast<juce::AudioProcessorParameterWithID*>(p.getParameters()[i]);
        check(parameter!=nullptr,"identified parameter");
        v[size_t(i)]=p.parameters().getRawParameterValue(parameter->paramID)->load();
    }
    return v;
}
void unchanged(const Values& before,const Values& after,const std::set<int>& allowed={}) {
    for(int i=0;i<4967;++i) if(!allowed.contains(i))
        check(chimeraTest::sameFloatBits(before[size_t(i)],after[size_t(i)]),"unexpected parameter change index="+juce::String(i)+" before="+juce::String(before[size_t(i)],10)+" after="+juce::String(after[size_t(i)],10));
}
void restored(const Values& before,ChimeraProcessor& p) {
    const auto after=values(p);int canonicalChanges=0;
    for(int i=0;i<4967;++i) if(!chimeraTest::sameFloatBits(before[size_t(i)],after[size_t(i)])) {
        auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(p.getParameters()[i]);
        check(parameter!=nullptr,"restore parameter type");
        const float canonical=parameter->convertFrom0to1(parameter->convertTo0to1(before[size_t(i)]));
        if(!eqNormalization::restoredRepresentation(*parameter,before[size_t(i)],after[size_t(i)])) {
            ++valueFailures;
            std::cout<<"FAIL_RESTORE id="<<parameter->paramID<<" index="<<i
                     <<std::setprecision(10)<<" before="<<before[size_t(i)]<<" canonical="<<canonical
                     <<" after="<<after[size_t(i)]<<" normalized_before="<<parameter->convertTo0to1(before[size_t(i)])
                     <<" normalized_after="<<parameter->convertTo0to1(after[size_t(i)])<<'\n';
        }
        if(eqNormalization::retainedConstructor(*parameter,before[size_t(i)],after[size_t(i)]))
            std::cout<<"RETAINED_CONSTRUCTOR id="<<parameter->paramID<<std::setprecision(10)
                     <<" saved="<<before[size_t(i)]<<" raw="<<after[size_t(i)]<<"\n";
        ++canonicalChanges;
    }
    std::cout<<"RESTORE canonical_changes="<<canonicalChanges<<'\n';
}
juce::MemoryBlock read(const juce::File& f) {juce::MemoryBlock b;check(f.loadFileAsData(b)&&b.getSize()>8,"missing state "+f.getFullPathName());return b;}
juce::ValueTree decode(const juce::MemoryBlock& b) {
    auto xml=juce::AudioProcessor::getXmlFromBinary(b.getData(),int(b.getSize()));
    check(xml!=nullptr,"invalid real fixture binary XML");auto t=juce::ValueTree::fromXml(*xml);
    check(irState::unpack(t),"invalid fixture IR assets");return t;
}
void load(ChimeraProcessor& p,const juce::MemoryBlock& b) {p.setStateInformation(b.getData(),int(b.getSize()));}
juce::MemoryBlock save(ChimeraProcessor& p,const juce::File& path) {
    juce::MemoryBlock b;check(p.tryGetStateInformation(b),"serialize");
    check(path.replaceWithData(b.getData(),b.getSize()),"disk save");return b;
}
void compareFixture(ChimeraProcessor& p,const juce::ValueTree& tree) {
    int count=0, storedBits=0, canonicalBits=0;
    for(auto c:tree) if(c.hasType("PARAM")) {
        const auto id=c.getProperty("id").toString();auto* param=p.parameters().getParameter(id);
        check(param!=nullptr,"fixture ID missing "+id);
        const auto stored=float(c.getProperty("value"));
        const auto actual=p.parameters().getRawParameterValue(id)->load();
        const auto canonical=param->convertFrom0to1(param->convertTo0to1(stored));
        // State recall may preserve the exact incoming raw value without an
        // additional host-normalization write. That is stronger preservation,
        // not a failure against the value produced by a redundant roundtrip.
        const bool exactStored=chimeraTest::sameFloatBits(actual,stored);
        const bool exactCanonical=chimeraTest::sameFloatBits(actual,canonical);
        if(exactStored) ++storedBits; else if(exactCanonical) ++canonicalBits;
        check(std::isfinite(actual)&&(exactStored||exactCanonical),"legacy stored/canonical value mismatch "+id);
        check(std::abs(actual-stored)<=std::max(.0001f,std::abs(stored)*.000002f),"legacy value intent mismatch "+id);
        ++count;
    }
    check(count==4967,"fixture must contain every released parameter");
    std::cout<<"VALUES stored_bits="<<storedBits<<" canonical_bits="<<canonicalBits<<'\n';
}
void masks(ChimeraProcessor& p,uint16_t tone,uint16_t final) {
    check(p.eqEditing().occupiedMask(0)==tone && p.eqEditing().occupiedMask(1)==final,"occupancy masks");
}
void auxiliary(const juce::ValueTree& a,const juce::ValueTree& b) {
    for(const auto* tag:{"USER_IRS","MIDI_MAP"})
        check(a.getChildWithName(tag).isEquivalentTo(b.getChildWithName(tag)),juce::String("auxiliary state changed ")+tag);
}
int index(ChimeraProcessor& p,int bank,int band,const char* field) {return p.parameters().getParameter(eqID(bank,band,field))->getParameterIndex();}
void exercise(const juce::File& fixture,const juce::File& out,bool hasB) {
    const auto failureBaseline=valueFailures;
    const auto original=read(fixture);const auto tree=decode(original);
    check(!tree.getChildWithName("EQ_EDIT_STATE").isValid(),"fixture must be pre-occupancy");
    auto p=std::make_unique<ChimeraProcessor>();load(*p,original);compareFixture(*p,tree);masks(*p,4095,4095);
    auto& edits=p->eqEditing();check(!edits.canUndo()&&!edits.canRedo(),"load history boundary");
    // Disabled legacy slots remain occupied; a thirteenth band cannot steal one.
    const auto initial=values(*p);check(edits.addBand(0,600,2)==-1,"legacy occupied disabled slot was reused");unchanged(initial,values(*p));
    std::array<Values,2> snapshots{};snapshots[0]=initial;
    if(hasB) {
        p->selectComparison(1);compareFixture(*p,tree.getChildWithName("COMPARISONS").getChildWithProperty("slot",1));
        masks(*p,4095,4095);snapshots[1]=values(*p);check(snapshots[0]!=snapshots[1],"A/B fixture must be distinct");
        p->selectComparison(0);restored(initial,*p);
    }
    std::cout<<"STAGE delete/undo/redo\n";
    const auto beforeDelete=values(*p);
    constexpr std::array<int,2> slots{3,7};
    std::set<int> deletedFields;
    for(int bank=0;bank<2;++bank) {
        const auto before=values(*p);check(edits.removeBand(bank,slots[size_t(bank)]),"delete legacy slot");
        const int enabled=index(*p,bank,slots[size_t(bank)],"enabled");deletedFields.insert(enabled);
        unchanged(before,values(*p),{enabled});check(edits.undo(),"delete undo");unchanged(before,values(*p));
        check(edits.redo(),"delete redo");
    }
    unchanged(beforeDelete,values(*p),deletedFields);masks(*p,4087,3967);
    std::cout<<"STAGE deleted save/reopen\n";
    const auto deleted=values(*p);save(*p,out.getChildFile("deleted.chimera"));
    p.reset();p=std::make_unique<ChimeraProcessor>();load(*p,read(out.getChildFile("deleted.chimera")));
    restored(deleted,*p);masks(*p,4087,3967);check(!p->eqEditing().canUndo(),"history persisted on disk");
    auxiliary(tree,decode(save(*p,out.getChildFile("deleted-reopened.chimera"))));
    if(hasB) {
        p->selectComparison(1);restored(snapshots[1],*p);masks(*p,4095,4095);
        p->selectComparison(0);restored(deleted,*p);masks(*p,4087,3967);
    }
    std::cout<<"STAGE deleted automation\n";
    // Existing host tuning automation must not resurrect a deleted band.
    for(int bank=0;bank<2;++bank) {
        const auto frequency=eqID(bank,slots[size_t(bank)],"frequency");
        p->parameters().getParameter(frequency)->setValueNotifyingHost(.61f);
    }
    masks(*p,4087,3967);
    for(int bank=0;bank<2;++bank)
        p->parameters().getParameter(eqID(bank,slots[size_t(bank)],"enabled"))->setValueNotifyingHost(1.f);
    masks(*p,4095,4095);check(!p->eqEditing().canUndo(),"host enable entered local history");
    load(*p,read(out.getChildFile("deleted.chimera")));restored(deleted,*p);masks(*p,4087,3967);
    std::cout<<"STAGE reuse fixed slots\n";
    const auto beforeReuse=values(*p);
    std::set<int> reusedFields;
    for(int bank=0;bank<2;++bank) {
        const int slot=slots[size_t(bank)];
        check(p->eqEditing().addBand(bank,bank==0?1234.f:4321.f,bank==0?6.f:-8.f)==slot,"slot compressed or wrong slot reused");
        for(const auto* f:{"enabled","frequency","gain","q","type"})reusedFields.insert(index(*p,bank,slot,f));
    }
    unchanged(beforeReuse,values(*p),reusedFields);masks(*p,4095,4095);
    std::cout<<"STAGE reused save/reopen\n";
    const auto reused=values(*p);save(*p,out.getChildFile("reused.chimera"));
    p.reset();p=std::make_unique<ChimeraProcessor>();load(*p,read(out.getChildFile("reused.chimera")));
    restored(reused,*p);masks(*p,4095,4095);
    std::cout<<"STAGE reused automation\n";
    // Simulate old host lane writes by stable APVTS ID/index. The independent
    // Linux ABI host separately replays the actual old VST3 numeric ParamIDs.
    const auto beforeAutomation=values(*p);
    std::set<int> laneFields;
    for(int bank=0;bank<2;++bank) for(const auto* field:{"frequency","gain"}) {
        const auto id=eqID(bank,slots[size_t(bank)],field);auto* param=p->parameters().getParameter(id);
        laneFields.insert(param->getParameterIndex());param->setValueNotifyingHost(field[0]=='f'?.61f:.31f);
        const auto expected=param->convertFrom0to1(field[0]=='f'?.61f:.31f);
        check(chimeraTest::sameFloatBits(p->parameters().getRawParameterValue(id)->load(),expected),"saved automation lane target");
    }
    unchanged(beforeAutomation,values(*p),laneFields);check(!p->eqEditing().canUndo(),"external automation history boundary");
    masks(*p,4095,4095);const auto automated=values(*p);
    std::cout<<"STAGE automated save/reopen\n";
    save(*p,out.getChildFile("automated.chimera"));load(*p,read(out.getChildFile("automated.chimera")));restored(automated,*p);
    if(hasB) {
        p->selectComparison(1);restored(snapshots[1],*p);masks(*p,4095,4095);
        p->selectComparison(0);restored(automated,*p);
    }
    auxiliary(tree,decode(save(*p,out.getChildFile("final.chimera"))));
    std::cout<<(valueFailures==failureBaseline?"PASS":"FAIL")<<" real beta.2 fixture "<<fixture.getFileName()<<": 4967 values / 24 fixed slots / delete-undo-redo / disk reopen / reuse / legacy lane IDs / "<<(hasB?"distinct A-B":"single A")<<'\n';
}
}
int main(int argc,char** argv) {
    try {
        check(argc==3,"usage: fixtures-dir output-dir");juce::ScopedJuceInitialiser_GUI init;
        const juce::File fixtures(argv[1]),output(argv[2]);
        for(const auto* name:{"default","pattern","ab-seeded"}) {
            const auto out=output.getChildFile(name);check(out.createDirectory().wasOk(),"output directory");
            exercise(fixtures.getChildFile(juce::String(name)+".chimera"),out,juce::String(name)=="ab-seeded");
        }
        std::cout<<(valueFailures==0?"PASS":"FAIL")<<" legacy state contracts; value_failures="<<valueFailures<<"; actual DAW modes and DAW projects NOT_ASSESSED\n";return valueFailures==0?0:1;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
