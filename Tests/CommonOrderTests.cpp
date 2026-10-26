// 1.4 signal order, common pre-POST block (docs/SIGNAL_ORDER_1_4.md): the
// order really changes the audio graph and each stage's capture tap follows
// its stage, reorders are click-safe, state/A-B/presets/old projects restore
// the right order, undo/redo work, moves write no parameter and the callback
// stays allocation-free.
#include "PluginProcessor.h"
#include "SignalPathView.h"
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
using namespace spectralforge::commonOrder;
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* parameter=p.parameters().getParameter(id);require(parameter!=nullptr,"Unknown order test parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
constexpr double rate=48000;constexpr int block=128;
std::unique_ptr<ChimeraProcessor> make() {
    auto p=std::make_unique<ChimeraProcessor>();
    set(*p,"gateon",0);set(*p,"input",0);set(*p,"output",0);set(*p,"oversampling",0);
    p->setRateAndBufferSizeDetails(rate,block);p->prepareToPlay(rate,block);
    return p;
}
// Tone EQ +12 dB at 110 Hz feeding a hard Clean Comp: EQ > Comp and
// Comp > EQ differ by about the EQ gain.
void voice(ChimeraProcessor& p) {
    set(p,"toneEQ_bypass",0);set(p,"toneEQ_b1_enabled",1);set(p,"toneEQ_b1_type",0);set(p,"toneEQ_b1_frequency",110);set(p,"toneEQ_b1_gain",12);
    set(p,"cleanCompOn",1);set(p,"cleanCompThreshold",-40);set(p,"cleanCompRatio",20);set(p,"cleanCompMakeup",0);set(p,"cleanCompMix",100);
}
std::vector<float> render(ChimeraProcessor& p,int blocks,double hz,int& clock) {
    std::vector<float> out;juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;
    for(int b=0;b<blocks;++b) {
        for(int n=0;n<block;++n){const float x=.2f*float(std::sin(juce::MathConstants<double>::twoPi*hz*double(clock+n)/rate));audio.setSample(0,n,x);audio.setSample(1,n,x);}
        clock+=block;p.processBlock(audio,midi);
        for(int n=0;n<block;++n)out.push_back(audio.getSample(0,n));
    }
    return out;
}
float rmsDb(const std::vector<float>& x) {double e=0;for(float v:x)e+=double(v)*v;return float(10*std::log10(e/double(x.size())+1e-30));}
float compressorTapDb(ChimeraProcessor& p) {
    std::vector<spectralforge::autoset::Window> windows;p.autoSetCapture().drain(spectralforge::autoset::Tap::compressor,windows);
    require(!windows.empty(),"The compressor tap captured nothing");
    float peak=-120;for(size_t i=windows.size()/2;i<windows.size();++i)peak=std::max(peak,windows[i].peakDb);return peak;
}
std::vector<float> snapshot(ChimeraProcessor& p) {std::vector<float> v;for(auto* q:p.getParameters())v.push_back(q->getValue());return v;}
juce::MemoryBlock save(ChimeraProcessor& p) {juce::MemoryBlock data;p.getStateInformation(data);return data;}

void helpers() {
    require(toString(defaultOrder)=="toneEQ,cleanComp,multiband,deEsser","The released order is not Tone EQ > Clean Comp > Multiband > De-esser");
    const Order reversed{Stage::deesser,Stage::multiband,Stage::compressor,Stage::toneEQ};
    require(fromString(toString(reversed))==reversed && unpack(pack(reversed))==reversed,"Order text or code does not round-trip");
    for(const auto* bad:{"","toneEQ,cleanComp,multiband","toneEQ,toneEQ,multiband,deEsser","toneEQ,cleanComp,multiband,reverb","toneEQ,cleanComp,multiband,deEsser,toneEQ"})
        require(fromString(bad)==defaultOrder,"A damaged order does not fall back to the released order");
    require(moved(defaultOrder,Stage::toneEQ,-1)==defaultOrder && moved(defaultOrder,Stage::deesser,1)==defaultOrder,"A move leaves the region");
    require(moved(defaultOrder,Stage::multiband,-1)==Order{Stage::toneEQ,Stage::multiband,Stage::compressor,Stage::deesser},"A move does not swap with its neighbour");
    std::cout<<"PASS order helpers: released order, round-trip, damaged text falls back, moves stay in the region\n";
}
void orderChangesTheGraph() {
    auto p=make();voice(*p);int clock=0;
    p->autoSetCapture().listen({spectralforge::autoset::Tap::compressor});
    render(*p,300,110.,clock);const float eqFirst=rmsDb(render(*p,100,110.,clock)),tapEqFirst=compressorTapDb(*p);
    require(p->moveCommonStage(Stage::compressor,-1),"The compressor cannot move before the Tone EQ");
    require(toString(p->commonOrder())=="cleanComp,toneEQ,multiband,deEsser","The move did not publish the new order");
    render(*p,300,110.,clock);const float compFirst=rmsDb(render(*p,100,110.,clock)),tapCompFirst=compressorTapDb(*p);
    std::cout<<"MEASURE EQ > Comp "<<eqFirst<<" dB, Comp > EQ "<<compFirst<<" dB; compressor input "<<tapEqFirst<<" / "<<tapCompFirst<<" dBFS peak\n";
    require(compFirst-eqFirst>8.f,"Moving the compressor before the EQ does not change the audio graph");
    require(tapEqFirst-tapCompFirst>8.f,"The compressor's capture tap does not follow the compressor");
    std::cout<<"PASS order changes the audio graph and the compressor tap follows its stage\n";
}
void reorderIsClickSafe() {
    // Held tone through an active EQ and compressor: no edge (second
    // difference) beyond twice the larger steady edge of the two orders.
    auto p=make();voice(*p);int clock=0;
    const auto edge=[&](int blocks){const auto x=render(*p,blocks,110.,clock);float e=0;
        for(size_t i=2;i<x.size();++i)e=std::max(e,std::abs(x[i]-2.f*x[i-1]+x[i-2]));return e;};
    render(*p,300,110.,clock);float before=edge(120),worst=0;
    for(const auto& [stage,direction]:std::vector<std::pair<Stage,int>>{{Stage::compressor,-1},{Stage::multiband,-1},{Stage::toneEQ,1},{Stage::deesser,-1}}) {
        require(p->moveCommonStage(stage,direction),"Reorder step could not move");
        const float during=edge(60);edge(240);const float after=edge(120);
        const float x=during/std::max(before,after);worst=std::max(worst,x);before=after;
        std::cout<<"MEASURE move "<<stageId(stage)<<(direction<0?" earlier":" later")<<": edge "<<x<<" x steady\n";
    }
    require(worst<2.f,"A reorder produces an edge beyond the orders' own");
    std::cout<<"PASS reorders are click-safe (fade out, swap, settle, fade in)\n";
}
void stateAndRecall() {
    const Order custom{Stage::multiband,Stage::toneEQ,Stage::deesser,Stage::compressor};
    auto p=make();for(const auto& [s,d]:std::vector<std::pair<Stage,int>>{{Stage::multiband,-1},{Stage::multiband,-1},{Stage::compressor,1},{Stage::compressor,1}})p->moveCommonStage(s,d);
    require(p->commonOrder()==custom,"Test order was not reached");
    const auto data=save(*p);
    auto restored=make();restored->setStateInformation(data.getData(),int(data.getSize()));
    require(restored->commonOrder()==custom,"The order does not survive a project save and restore");
    require(!restored->canUndoCommonOrder(),"A restored project kept another session's order undo");
    // An old project has no order property: it plays the released order, also
    // into a processor whose order was changed.
    auto xml=juce::AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()));require(xml!=nullptr,"State is not binary XML");
    xml->removeAttribute(propertyName);
    if(auto* slots=xml->getChildByName("COMPARISONS"))for(auto* slot:slots->getChildIterator())slot->removeAttribute(propertyName);
    juce::MemoryBlock legacy;juce::AudioProcessor::copyXmlToBinary(*xml,legacy);
    restored->setStateInformation(legacy.getData(),int(legacy.getSize()));
    require(restored->commonOrder()==defaultOrder,"A project without an order does not open in the released order");
    xml->setAttribute(propertyName,"toneEQ,toneEQ,reverb");juce::MemoryBlock damaged;juce::AudioProcessor::copyXmlToBinary(*xml,damaged);
    restored->setStateInformation(damaged.getData(),int(damaged.getSize()));
    require(restored->commonOrder()==defaultOrder,"A damaged order does not open in the released order");
    // A/B slots keep their own order.
    auto ab=make();ab->moveCommonStage(Stage::deesser,-1);const auto a=ab->commonOrder();
    ab->selectComparison(1);ab->moveCommonStage(Stage::toneEQ,1);const auto b=ab->commonOrder();
    require(!(a==b),"Test A/B orders are equal");
    ab->selectComparison(0);require(ab->commonOrder()==a,"A/B slot A lost its order");
    ab->selectComparison(1);require(ab->commonOrder()==b,"A/B slot B lost its order");
    // A factory preset plays its authored (released) order.
    ab->loadFactoryPreset(0);require(ab->commonOrder()==defaultOrder,"A factory preset keeps a custom order");
    require(!ab->canUndoCommonOrder(),"A factory preset kept the order undo");
    std::cout<<"PASS order state: save/restore, old and damaged projects open in the released order, A/B slots keep theirs, presets reset it\n";
}
void undoAndParameters() {
    auto p=make();const auto parameters=snapshot(*p);
    require(!p->moveCommonStage(Stage::toneEQ,-1) && !p->canUndoCommonOrder(),"An impossible move was recorded");
    p->moveCommonStage(Stage::compressor,-1);const auto first=p->commonOrder();
    p->moveCommonStage(Stage::deesser,-1);const auto second=p->commonOrder();
    require(snapshot(*p)==parameters,"A reorder wrote a parameter");
    require(p->undoCommonOrder() && p->commonOrder()==first,"Undo does not restore the previous order");
    require(p->undoCommonOrder() && p->commonOrder()==defaultOrder,"Second undo does not restore the released order");
    require(!p->undoCommonOrder(),"Undo ran past the history");
    require(p->undoCommonOrder(true) && p->commonOrder()==first && p->undoCommonOrder(true) && p->commonOrder()==second,"Redo does not reapply the moves");
    p->undoCommonOrder();p->moveCommonStage(Stage::multiband,1);
    require(!p->canUndoCommonOrder(true),"A new move did not clear the redo history");
    require(snapshot(*p)==parameters,"Undo or redo wrote a parameter");
    std::cout<<"PASS order undo/redo; moves, undo and redo write no parameter\n";
}
void signalPathFollowsOrder() {
    // The routing graph shows the common block in its saved order, chained
    // in that order, and its Alt+Left/Right command moves the stage.
    auto p=make();SignalPathView view(*p,false);
    const auto x=[&](const char* id){const auto* n=view.snapshot().find(id);require(n!=nullptr,"Common block node missing");return n->bounds.getX();};
    const auto chained=[&](const char* a,const char* b){for(const auto& e:view.snapshot().edges)if(e.from==a&&e.to==b&&!e.tap)return true;return false;};
    require(x("tone.eq")<x("common.comp") && x("common.comp")<x("common.multiband") && x("common.multiband")<x("common.deesser"),"The graph does not show the released order");
    p->moveCommonStage(Stage::deesser,-1);p->moveCommonStage(Stage::deesser,-1);view.refresh();
    require(x("tone.eq")<x("common.deesser") && x("common.deesser")<x("common.comp"),"The graph does not follow a reorder");
    require(chained("tone.eq","common.deesser") && chained("common.deesser","common.comp") && chained("common.multiband","post.section0"),"The graph's connections do not follow the order");
    juce::TextButton* comp=nullptr;for(auto* c:view.getChildren())if(c->getComponentID()=="path.common.comp")comp=dynamic_cast<juce::TextButton*>(c);
    auto* orderable=dynamic_cast<SignalPathNavigationButton*>(comp);require(orderable && orderable->onOrderKey && orderable->onContextMenu,"Common block button has no order commands");
    const auto parameters=snapshot(*p);
    require(orderable->onOrderKey(juce::KeyPress(juce::KeyPress::leftKey,juce::ModifierKeys::altModifier,0)),"Alt+Left is not handled");
    require(toString(p->commonOrder())=="toneEQ,cleanComp,deEsser,multiband","Alt+Left did not move Clean Comp earlier");
    require(!orderable->onOrderKey(juce::KeyPress(juce::KeyPress::leftKey)),"A plain arrow key moved a stage");
    require(snapshot(*p)==parameters,"The graph's order command wrote a parameter");
    for(auto* c:view.getChildren())if(c->getComponentID()=="path.output")
        require(!dynamic_cast<SignalPathNavigationButton*>(c)->onOrderKey,"A block outside the common region offers order commands");
    std::cout<<"PASS signal path graph shows and chains the saved order; Alt+Left/Right moves only common-block stages\n";
}
void noAllocation() {
    auto p=make();voice(*p);int clock=0;render(*p,40,110.,clock);
    juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;
    for(int step=0;step<4;++step) {
        p->moveCommonStage(step%2?Stage::compressor:Stage::multiband,step<2?-1:1);
        allocations=0;countAllocations=true;
        for(int b=0;b<30;++b){audio.clear();p->processBlock(audio,midi);}
        countAllocations=false;require(allocations==0,"A reorder allocates in the audio callback");
    }
    std::cout<<"PASS reorders: no allocation in the callback\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {helpers();orderChangesTheGraph();reorderIsClickSafe();stateAndRecall();undoAndParameters();signalPathFollowsOrder();noAllocation();return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
