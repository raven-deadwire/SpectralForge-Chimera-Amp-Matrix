// 1.4 constrained signal-order editor (docs/SIGNAL_ORDER_1_4.md): the order
// model, the audio graph in both regions, the click-safe change, state /
// A-B / old-project recall, undo / redo, automation identity, stale input,
// the signal-path display and its commands.
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "SignalPath.h"
#include "SignalPathView.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace spectralforge;
constexpr double rate=48000,pi=3.14159265358979323846;constexpr int block=256;
void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
double db(double v){return 20*std::log10(std::max(v,1.e-30));}
void set(ChimeraProcessor& p,const juce::String& id,float v){auto* q=p.parameters().getParameter(id);require(q!=nullptr,"no parameter "+id.toStdString());q->setValueNotifyingHost(q->convertTo0to1(v));}
float get(ChimeraProcessor& p,const juce::String& id){return p.parameters().getRawParameterValue(id)->load();}
std::unique_ptr<ChimeraProcessor> make(){auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(rate,block);p->prepareToPlay(rate,block);return p;}
std::vector<float> run(ChimeraProcessor& p,int blocks,double amplitude=.0316,double hz=1000,int offset=0){
    std::vector<float> out;juce::AudioBuffer<float> b(2,block);juce::MidiBuffer m;
    for(int k=0;k<blocks;++k){for(int n=0;n<block;++n){const float x=float(amplitude*std::sin(2*pi*hz*((offset+k)*block+n)/rate));b.setSample(0,n,x);b.setSample(1,n,x);}
        p.processBlock(b,m);for(int n=0;n<block;++n)out.push_back(b.getSample(0,n));}
    return out;
}
double magnitude(const std::vector<float>& x,int from,int to,double hz){std::complex<double> s;for(int n=from;n<to;++n)s+=double(x[size_t(n)])*std::polar(1.0,-2*pi*hz*n/rate);return 2*std::abs(s)/(to-from);}

void model() {
    const auto start=ToneOrder::initial();require(start.valid() && start.isInitial(),"Initial order invalid");
    const auto moved=start.placed(toneEQModule,3);
    require(moved.slots==std::array<int,6>{compressorModule,multibandModule,saturationModule,toneEQModule,filterModule,deesserModule} && moved.valid(),"place() wrong");
    require(moved.placed(toneEQModule,0)==start && start.placed(deesserModule,99).position(deesserModule)==5,"place() round trip or clamping wrong");
    ToneOrder twice=start;twice.slots[1]=0;require(!twice.valid(),"Duplicate accepted");
    // Parameters: 13 slots, not automatable, defaults are the released order.
    auto p=make();int count=0;
    for(auto* raw:p->getParameters())if(auto* q=dynamic_cast<juce::RangedAudioParameter*>(raw))if(q->getParameterID().startsWith("order")){
        ++count;require(!q->isAutomatable() && q->getVersionHint()==signalOrderParameterVersion,"Order slot automatable or wrong version");}
    require(count==signalOrderParameterCount && p->requestedOrder()==SignalOrder{},"Order bank size or default wrong");
    std::cout<<"MEASURE model: "<<count<<" slot parameters (6 tone / dynamics, 7 POST), version "<<signalOrderParameterVersion<<", not automatable\n";
    std::cout<<"PASS order model: permutations, placement, defaults\n";
}

void toneRegion() {
    // Tone EQ +12 dB at 1 kHz and a hard clean compressor (ratio 20): EQ first
    // is squashed by the compressor; compressor first lets the boost through,
    // so the levels differ by close to the boost.
    const auto level=[](bool compressorFirst){
        auto p=make();set(*p,"mode",0);set(*p,"ampon1",0);set(*p,"cab1",0);   // a clean rig: the sine reaches the block as a sine
        set(*p,eqID(0,-1,"bypass"),0);set(*p,eqID(0,0,"enabled"),1);set(*p,eqID(0,0,"type"),0);set(*p,eqID(0,0,"frequency"),1000);set(*p,eqID(0,0,"gain"),12);set(*p,eqID(0,0,"q"),1);
        set(*p,"cleanCompOn",1);set(*p,"cleanCompThreshold",-50);set(*p,"cleanCompRatio",20);set(*p,"cleanCompAttack",1);set(*p,"cleanCompKnee",0);
        if(compressorFirst)require(p->placeModule(OrderRegion::tone,compressorModule,0),"Move refused");
        const auto out=run(*p,200);return magnitude(out,48000,51200,1000);};
    const double eqFirst=level(false),compFirst=level(true);
    std::cout<<"MEASURE tone / dynamics order, Tone EQ +12 dB at 1 kHz with a hard compressor: EQ > comp "<<db(eqFirst)<<" dBFS, comp > EQ "<<db(compFirst)<<" dBFS ("<<db(compFirst/eqFirst)<<" dB)\n";
    require(db(compFirst/eqFirst)>9,"The tone / dynamics block does not run in its saved order");
    std::cout<<"PASS tone / dynamics block runs in its saved order\n";
}

void postRegion() {
    // POST EQ +12 dB at 1 kHz and a hard bus compressor, run directly.
    const auto level=[](const PostOrder& order){
        PostFXChain chain;chain.prepare({rate,(juce::uint32)block,2});FXState s;
        s.eqOn=true;s.eqMid=12;s.eqMidHz=1000;s.eqQ=1;s.busCompOn=true;s.busThreshold=-50;s.busRatio=20;s.busAttack=1;s.busRelease=100;
        juce::AudioBuffer<float> b(2,block);std::vector<float> out;
        for(int k=0;k<200;++k){for(int n=0;n<block;++n){const float x=float(.0316*std::sin(2*pi*1000*(k*block+n)/rate));b.setSample(0,n,x);b.setSample(1,n,x);}
            chain.process(b,s,order);for(int n=0;n<block;++n)out.push_back(b.getSample(0,n));}
        return magnitude(out,48000,51200,1000);};
    const auto released=PostOrder::initial(),eqFirst=released.placed(postEQModule,0);
    const double compFirst=level(released),eqBefore=level(eqFirst);
    // Every module still runs exactly once: a reversed rack is a valid permutation.
    PostOrder reversed;for(int i=0;i<postModuleCount;++i)reversed.slots[size_t(i)]=postModuleCount-1-i;
    std::cout<<"MEASURE POST order, EQ +12 dB at 1 kHz with a hard bus compressor: comp > EQ (released) "<<db(compFirst)<<" dBFS, EQ > comp "<<db(eqBefore)<<" dBFS ("<<db(compFirst/eqBefore)<<" dB)\n";
    require(db(compFirst/eqBefore)>6 && reversed.valid(),"The POST rack does not run in its saved order");
    std::cout<<"PASS POST rack runs in its saved order\n";
}

void clickSafe() {
    // A clean rig (amp and cabinet off) carries a 220 Hz sine, so any click
    // shows as broadband energy. The second difference x[n] - 2x[n-1] + x[n-2]
    // of a sine is tiny; a step in level or waveform makes it jump by about the
    // step. The 5 ms raised-cosine fades add a smooth share of the same size
    // as the sine's own, so the bound is twice the steady value.
    const auto curvature=[](const std::vector<float>& x,size_t from,size_t to){double m=0;for(size_t n=std::max<size_t>(2,from);n<to;++n)m=std::max(m,double(std::abs(x[n]-2*x[n-1]+x[n-2])));return m;};
    const auto clean=[](){auto p=make();set(*p,"mode",0);set(*p,"ampon1",0);set(*p,"cab1",0);return p;};
    // 1. A move with no audible effect (the de-esser is off): only the dip remains.
    auto p=clean();const auto before=run(*p,200,.1,220);
    p->placeModule(OrderRegion::tone,deesserModule,0);const auto after=run(*p,40,.1,220,200);
    // Dip: one-period (218-sample) peak envelope below half the steady peak.
    const double peak=*std::max_element(before.begin()+24000,before.end());
    int dip=0;for(size_t n=0;n+218<after.size();++n){float m=0;for(size_t k=n;k<n+218;++k)m=std::max(m,std::abs(after[k]));if(m<peak*.5)++dip;}
    const double steady=curvature(before,24000,before.size()),neutral=curvature(after,0,6000);
    // 2. An audible move: Tone EQ +12 dB at 220 Hz goes after a hard compressor.
    auto q=clean();set(*q,"cleanCompOn",1);set(*q,"cleanCompThreshold",-40);set(*q,"cleanCompRatio",20);set(*q,"cleanCompAttack",1);
    set(*q,eqID(0,-1,"bypass"),0);set(*q,eqID(0,0,"enabled"),1);set(*q,eqID(0,0,"type"),0);set(*q,eqID(0,0,"frequency"),220);set(*q,eqID(0,0,"gain"),12);set(*q,eqID(0,0,"q"),1);
    const auto loudBefore=run(*q,200,.1,220);q->placeModule(OrderRegion::tone,compressorModule,0);const auto loudAfter=run(*q,200,.1,220,200);
    const double steadyLoud=std::max(curvature(loudBefore,24000,loudBefore.size()),curvature(loudAfter,30000,loudAfter.size())),audible=curvature(loudAfter,0,6000);
    // Switched raw, the level change would be a step of about the peak difference.
    const double rawStep=std::abs(double(*std::max_element(loudAfter.begin()+30000,loudAfter.end()))-double(*std::max_element(loudBefore.begin()+24000,loudBefore.end())));
    std::cout<<"MEASURE click-safe change, 220 Hz on a clean rig: neutral move: envelope below half its level for "<<dip/rate*1000<<" ms; second difference "<<neutral<<" during the switch vs "<<steady
             <<" steady; audible move (EQ after a hard compressor) "<<audible<<" vs "<<steadyLoud<<" steady (a raw switch would step by about "<<rawStep<<")\n";
    require(dip>0 && dip/rate<.016 && neutral<=steady*2 && audible<=steadyLoud*2 && rawStep>steadyLoud*100,"Order change is not click-safe or the dip is too long");
    std::cout<<"PASS order changes fade out and back in: no click above the steady signal, dip under 16 ms\n";
}

void recallAndUndo() {
    auto p=make();
    const auto snapshot=[](ChimeraProcessor& q){std::map<juce::String,float> v;for(auto* raw:q.getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(raw))if(!r->getParameterID().startsWith("order"))v[r->getParameterID()]=r->getValue();return v;};
    const auto others=snapshot(*p);
    struct Gestures final : juce::AudioProcessorListener {
        int begin{},end{};std::vector<int> changed;
        void audioProcessorParameterChanged(juce::AudioProcessor*,int index,float) override {changed.push_back(index);}
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
        void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int) override {++begin;}
        void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int) override {++end;}
    } g;p->addListener(&g);
    require(p->moveModule(OrderRegion::tone,toneEQModule,2) && !p->moveModule(OrderRegion::tone,deesserModule,1),"Move or edge refusal wrong");
    p->removeListener(&g);
    bool onlyOrder=true;for(const int i:g.changed)if(!dynamic_cast<juce::RangedAudioParameter*>(p->getParameters()[i])->getParameterID().startsWith("order"))onlyOrder=false;
    require(onlyOrder && g.begin==g.end && g.begin==int(g.changed.size()) && snapshot(*p)==others,"A move wrote another parameter, or not as complete gestures");
    const auto moved=p->requestedOrder();
    require(moved.tone.position(toneEQModule)==2,"Move did not land");
    // Undo / redo / reset.
    require(p->undoOrder() && p->requestedOrder()==SignalOrder{} && p->redoOrder() && p->requestedOrder()==moved,"Undo / redo wrong");
    require(p->resetOrder(OrderRegion::tone) && p->requestedOrder()==SignalOrder{} && p->undoOrder() && p->requestedOrder()==moved,"Reset / undo wrong");
    p->placeModule(OrderRegion::post,postReverbModule,0);const auto both=p->requestedOrder();
    // Project state round trip, and the audio follows it.
    juce::MemoryBlock saved;p->getStateInformation(saved);
    auto q=make();q->setStateInformation(saved.getData(),int(saved.getSize()));
    require(q->requestedOrder()==both && !q->canUndoOrder(),"Order not restored, or undo history survived a recall");
    const auto a=run(*p,40),b=run(*q,40);double diff=0;for(size_t n=0;n<a.size();++n)diff=std::max(diff,double(std::abs(a[n]-b[n])));
    // A/B: each slot keeps its own order.
    p->copyComparison();p->resetOrder(OrderRegion::tone);p->resetOrder(OrderRegion::post);
    p->selectComparison(1);const auto slotB=p->requestedOrder();p->selectComparison(0);const auto slotA=p->requestedOrder();
    // An old project without order parameters loads the released order.
    juce::ValueTree tree;{auto stateXml=p->getXmlFromBinary(saved.getData(),int(saved.getSize()));tree=juce::ValueTree::fromXml(*stateXml);}
    for(int i=tree.getNumChildren()-1;i>=0;--i)if(tree.getChild(i).getProperty("id").toString().startsWith("order"))tree.removeChild(i,nullptr);
    juce::MemoryBlock old;{auto oldXml=tree.createXml();juce::AudioProcessor::copyXmlToBinary(*oldXml,old);}
    auto r=make();r->placeModule(OrderRegion::tone,deesserModule,0);r->setStateInformation(old.getData(),int(old.getSize()));
    std::cout<<"MEASURE recall: restored order "<<(q->requestedOrder()==both?"equal":"different")<<", audio difference after restore "<<diff<<"; A/B slots "<<(slotB==both?"keep":"lose")<<" / "<<(slotA==SignalOrder{}?"keep":"lose")
             <<" their orders; old project -> "<<(r->requestedOrder()==SignalOrder{}?"released order":"other")<<"\n";
    require(diff==0 && slotB==both && slotA==SignalOrder{} && r->requestedOrder()==SignalOrder{},"Recall, A/B or old-project order wrong");
    std::cout<<"PASS moves write only the order (complete gestures), undo / redo / reset, project, A/B and old-project recall\n";
}

void staleInput() {
    // A duplicate in the slots (a half-written edit, or a damaged session)
    // keeps the previous valid order rather than jumping to another one.
    auto p=make();p->placeModule(OrderRegion::tone,deesserModule,0);run(*p,4);
    const auto valid=p->requestedOrder();
    set(*p,orderSlotID(OrderRegion::tone,1),float(deesserModule));   // two slots now hold the de-esser
    SignalOrderParameters reader;reader.bind(p->parameters());
    const auto kept=reader.read(valid);
    require(kept.tone==valid.tone && p->requestedOrder().tone==ToneOrder::initial(),"Stale order not handled");
    run(*p,4);   // processes without crashing on the invalid slots
    std::cout<<"PASS an invalid slot set keeps the last valid order in the audio thread\n";
}

void pathAndCommands() {
    auto p=make();
    p->placeModule(OrderRegion::tone,saturationModule,0);p->placeModule(OrderRegion::post,postReverbModule,0);
    auto s=signalPath::read(*p);
    const auto edge=[&](const char* a,const char* b){for(const auto& e:s.edges)if(e.from==a&&e.to==b)return true;return false;};
    require(edge("merge","common.saturation") && edge("common.saturation","tone.eq") && edge("common.deesser","post.section5") && edge("post.section5","post.section0")
            && edge("post.section4","post.creative") && edge("post.creative","utilities"),"Signal path does not follow the saved order");
    for(size_t i=0;i<s.nodes.size();++i)for(size_t j=i+1;j<s.nodes.size();++j)require(!s.nodes[i].bounds.intersects(s.nodes[j].bounds),"Path blocks overlap after a move");
    set(*p,"synthInsert",1);s=signalPath::read(*p);require(edge("synth","common.saturation"),"SYNTH 'before tone / dynamics' does not join the first block");
    // The routing graph: commands and keys move, navigation does not.
    SignalPathView view(*p,false);view.setSize(1140,650);
    const auto before=p->requestedOrder();
    view.navigate=[](const signalPath::Node&){};
    if(auto* b=view.buttonFor("common.comp"))b->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    require(p->requestedOrder()==before,"Navigation changed the order");
    require(view.orderCommand("common.comp",SignalPathView::OrderCommand::first) && p->requestedOrder().tone.position(compressorModule)==0,"Placement command failed");
    auto* button=dynamic_cast<SignalPathButton*>(view.buttonFor("common.comp"));require(button!=nullptr && button->onPlacementMenu!=nullptr,"Movable block has no placement menu");
    require(button->keyPressed(juce::KeyPress(juce::KeyPress::rightKey,juce::ModifierKeys::altModifier,0)) && p->requestedOrder().tone.position(compressorModule)==1,"Alt + Right did not move the block");
    require(button->keyPressed(juce::KeyPress('z',juce::ModifierKeys::commandModifier,0)) && p->requestedOrder().tone.position(compressorModule)==0,"Ctrl + Z did not undo");
    require(view.buttonFor("final.eq")!=nullptr && dynamic_cast<SignalPathButton*>(view.buttonFor("final.eq"))->onPlacementMenu==nullptr,"A fixed block offers a placement menu");
    std::cout<<"PASS signal path follows the order; placement commands, Alt + arrows and Ctrl + Z; navigation writes nothing; fixed blocks stay fixed\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {model();toneRegion();postRegion();clickSafe();recallAndUndo();staleInput();pathAndCommands();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
