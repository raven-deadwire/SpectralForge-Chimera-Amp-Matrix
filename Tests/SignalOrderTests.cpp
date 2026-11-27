// 1.4 constrained signal-order editor (docs/SIGNAL_ORDER_1_4.md): the order
// model, the audio graph in both regions, the click-safe change, state /
// A-B / old-project recall, undo / redo, automation identity, stale input,
// the signal-path display and its commands.
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "SignalPath.h"
#include "SignalPathView.h"
#include "PluginEditor.h"
#include "PostRackDrag.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
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
    // Parameters: 13 released slots at version 29 and POST positions 8-14 at
    // version 35 (docs/POST_SECOND_SLOTS.md), not automatable, defaults are the
    // released order followed by the second slots.
    // POST positions 15-28 came with slots 3 and 4 at version 37 (docs/POST_FOUR_SLOTS.md).
    auto p=make();int count=0,second=0,extra=0;
    for(auto* raw:p->getParameters())if(auto* q=dynamic_cast<juce::RangedAudioParameter*>(raw))if(q->getParameterID().startsWith("order")){
        const int position=q->getParameterID().startsWith("orderPost")?q->getParameterID().substring(9).getIntValue():0;
        const bool appended=position>postModuleCount,late=position>postSecondSlotBlockCount;
        ++count;second+=appended && !late;extra+=late;
        require(!q->isAutomatable() && q->getVersionHint()==(late?postExtraSlotParameterVersion:appended?postSecondSlotParameterVersion:signalOrderParameterVersion),"Order slot automatable or wrong version");}
    require(count==signalOrderParameterCount+postSecondSlotOrderParameterCount+postExtraSlotOrderParameterCount && second==postSecondSlotBlockCount-postModuleCount && extra==postBlockCount-postSecondSlotBlockCount && p->requestedOrder()==SignalOrder{},"Order bank size or default wrong");
    require(PostOrder::initial().slots[size_t(postModuleCount)]==postBlock(postDynamicsModule,1) && PostOrder::initial().position(postBlock(postReverbModule,1))==postSecondSlotBlockCount-1
            && PostOrder::initial().position(postBlock(postReverbModule,postSlotCount-1))==postBlockCount-1,"Slots 2-4 do not follow the released order");
    std::cout<<"MEASURE model: "<<count<<" slot parameters (6 tone / dynamics, "<<postBlockCount<<" POST), versions "<<signalOrderParameterVersion<<" / "<<postSecondSlotParameterVersion<<" / "<<postExtraSlotParameterVersion<<", not automatable\n";
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
    PostOrder reversed;for(int i=0;i<postBlockCount;++i)reversed.slots[size_t(i)]=postBlockCount-1-i;
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
    // Every intermediate state of a multi-slot write is invalid (or the target):
    // reset from a swapped order passes through no third valid order.
    {
        auto w=make();
        w->placeModule(OrderRegion::tone,1,0);w->placeModule(OrderRegion::tone,3,2);w->placeModule(OrderRegion::tone,5,4);
        const auto swapped=w->requestedOrder();require((swapped.tone.slots==std::array<int,toneModuleCount>{1,0,3,2,5,4}),"Swap not applied");
        struct Watch final : juce::AudioProcessorListener {
            ChimeraProcessor* q{};int thirdValid{};SignalOrder from,to;
            void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {
                std::array<int,toneModuleCount> slots{};for(int i=0;i<toneModuleCount;++i)slots[size_t(i)]=juce::roundToInt(q->parameters().getRawParameterValue(orderSlotID(OrderRegion::tone,i))->load());
                ToneOrder t;t.slots=slots;if(t.valid() && t!=from.tone && t!=to.tone)++thirdValid;}
            void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
        } watch;watch.q=w.get();watch.from=swapped;watch.to=SignalOrder{};
        w->addListener(&watch);require(w->resetOrder(OrderRegion::tone),"Reset refused");w->removeListener(&watch);
        require(watch.thirdValid==0 && w->requestedOrder()==SignalOrder{},"A reset passed through a third valid order");
        // A stored order that is not a permutation loads as the default order.
        juce::MemoryBlock data;w->getStateInformation(data);
        juce::ValueTree t;{auto x=w->getXmlFromBinary(data.getData(),int(data.getSize()));t=juce::ValueTree::fromXml(*x);}
        for(auto child:t)if(child.getProperty("id").toString()=="orderTone2")child.setProperty("value",0,nullptr);   // duplicate of slot 1
        juce::MemoryBlock bad;{auto x=t.createXml();juce::AudioProcessor::copyXmlToBinary(*x,bad);}
        auto v=make();v->placeModule(OrderRegion::tone,deesserModule,0);v->setStateInformation(bad.getData(),int(bad.getSize()));
        require(v->requestedOrder()==SignalOrder{} && juce::roundToInt(v->parameters().getRawParameterValue("orderTone2")->load())==1,"An invalid stored order was not replaced by the default");
    }
    std::cout<<"PASS moves write only the order (complete gestures), undo / redo / reset, project, A/B and old-project recall; no third order mid-write; invalid stored order -> default\n";
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
void dragging() {
    // 1.5 drag in the ROUTING graph: preview while hovering, one command on drop.
    auto p=make();SignalPathView view(*p,false);view.setSize(1140,650);
    int navigated=0;view.navigate=[&](const signalPath::Node&){++navigated;};
    struct Writes final : juce::AudioProcessorListener {
        int changes{};void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {++changes;}
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
    } writes;
    auto* button=dynamic_cast<SignalPathButton*>(view.buttonFor("common.comp"));require(button!=nullptr,"Clean compressor block missing");
    const auto source=juce::Desktop::getInstance().getMainMouseSource();
    const auto event=[&](juce::Point<int> inView,juce::Point<int> downInView,bool down){
        const auto local=button->getLocalPoint(&view,inView).toFloat(),start=button->getLocalPoint(&view,downInView).toFloat();const auto now=juce::Time::getCurrentTime();
        return juce::MouseEvent(source,local,juce::ModifierKeys(down?juce::ModifierKeys::leftButtonModifier:0),1,0,0,0,0,button,button,now,start,now,1,local.getDistanceFrom(start)>6);};
    const auto centre=[&](const juce::String& id){return view.buttonFor(id)->getBoundsInParent().getCentre();};
    const auto targets=view.dropTargets(OrderRegion::tone);require(targets.size()==6,"Six drop targets expected");
    // Drag the compressor (position 2) over position 5 and drop.
    const auto start=centre("common.comp");
    p->addListener(&writes);
    button->mouseDown(event(start,start,true));
    button->mouseDrag(event(targets[3].getCentre(),start,true));
    const int hover=view.dropPosition();
    button->mouseDrag(event(targets[4].getCentre(),start,true));
    const int hover2=view.dropPosition();const int writesWhileDragging=writes.changes;
    // Optional evidence: SIGNAL_ORDER_SNAPSHOT=<directory> writes the graph mid-drag.
    if(const char* dir=std::getenv("SIGNAL_ORDER_SNAPSHOT")){
        juce::Image image(juce::Image::ARGB,1140,650,true);{juce::Graphics g(image);g.fillAll(juce::Colour(0xff171e20));g.drawImageAt(view.createComponentSnapshot(view.getLocalBounds(),true,1.f),0,0);}
        auto out=juce::File(dir).getChildFile("signal_order_drag.png");out.deleteFile();auto stream=out.createOutputStream();
        require(stream && juce::PNGImageFormat().writeImageToStream(image,*stream),"Cannot write the drag snapshot");
    }
    button->mouseUp(event(targets[4].getCentre(),start,false));
    p->removeListener(&writes);
    const int landed=p->requestedOrder().tone.position(compressorModule);
    require(hover==3 && hover2==4 && writesWhileDragging==0 && landed==4 && navigated==0,"Drag preview, write-free hover, drop or click suppression wrong");
    require(p->undoOrder() && p->requestedOrder()==SignalOrder{} && !p->canUndoOrder(),"A drop is not exactly one undo step");
    // Escape cancels, and the release after it does nothing.
    button=dynamic_cast<SignalPathButton*>(view.buttonFor("common.comp"));
    button->mouseDown(event(start,start,true));button->mouseDrag(event(targets[5].getCentre(),start,true));
    require(button->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && view.draggingBlock().isEmpty(),"Escape did not cancel");
    button->mouseDrag(event(targets[4].getCentre(),start,true));   // moving on after Escape does not restart the drag
    require(view.draggingBlock().isEmpty(),"A drag restarted after Escape");
    button->mouseUp(event(targets[4].getCentre(),start,false));
    // Releasing over a fixed block (FINAL EQ) or far away cancels too.
    button->mouseDown(event(start,start,true));button->mouseDrag(event(centre("final.eq"),start,true));const int overFixed=view.dropPosition();button->mouseUp(event(centre("final.eq"),start,false));
    button->mouseDown(event(start,start,true));button->mouseDrag(event({1100,40},start,true));button->mouseUp(event({1100,40},start,false));
    require(overFixed==-1 && p->requestedOrder()==SignalOrder{} && navigated==0,"A cancelled drag changed the order or navigated");
    // A POST block only targets POST positions.
    auto* reverb=dynamic_cast<SignalPathButton*>(view.buttonFor("post.section5"));
    require(reverb!=nullptr && view.dropTargets(OrderRegion::post).size()==size_t(postBlockCount),"POST drop targets wrong");
    // A plain click (under the drag threshold) still opens the editor.
    button->mouseDown(event(start,start,true));button->mouseUp(event(start+juce::Point<int>(2,1),start,false));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    std::cout<<"MEASURE drag: hover previews position "<<hover+1<<" then "<<hover2+1<<", "<<writesWhileDragging<<" parameter writes while hovering, dropped at position "<<landed+1
             <<"; over FINAL EQ the target is "<<overFixed<<"; a 2-pixel press navigates "<<navigated<<" time\n";
    require(navigated==1,"A click no longer opens the editor");
    std::cout<<"PASS drag: preview only while hovering, one undoable command on drop, Escape / off-target cancel, clicks still navigate\n";
}

// Review (10/30): keyed detectors placed after a module with latency read
// the key delayed by it.
void keyDelay() {
    KeyDelay d;d.prepare(64,32);
    juce::AudioBuffer<float> key(2,32);std::vector<float> got;int n0=0;
    for(int chunk=0;chunk<6;++chunk){
        const int n=chunk%2?32:17;key.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i){key.setSample(0,i,float(n0+i+1));key.setSample(1,i,-float(n0+i+1));}
        const auto* out=d.process(&key,5);require(out!=nullptr && out!=&key && out->getNumSamples()==n,"Delayed key missing");
        for(int i=0;i<n;++i)got.push_back(out->getSample(0,i));n0+=n;
    }
    bool exact=true;for(size_t i=0;i<got.size();++i)exact=exact&&got[i]==(i<5?0.f:float(i-5+1));
    require(exact,"KeyDelay is not an exact 5-sample delay across ragged chunks");
    require(d.process(&key,0)==&key && d.process(nullptr,5)==nullptr,"KeyDelay pass-through or no-key case wrong");
    key.setSize(2,8,false,false,true);for(int i=0;i<8;++i){key.setSample(0,i,9);key.setSample(1,i,9);}
    const auto* back=d.process(&key,5);require(back->getSample(0,0)==0.f && back->getSample(0,5)==9.f,"A returning key replayed old audio");
    std::cout<<"PASS key delay: exact across ragged chunks, pass-through at 0, silence after the key returns\n";
}
}

// POST page drag (docs/SIGNAL_ORDER_1_4.md, "Drag on the POST page"): the
// shipped editor's rack grips reorder with one command; hover writes nothing;
// Escape, off-target release and a plain click change nothing.
juce::Component* findID(juce::Component& root,const juce::String& id){
    if(root.getComponentID()==id)return &root;
    for(auto* child:root.getChildren())if(auto* found=findID(*child,id))return found;
    return nullptr;
}
template<class T> T* findType(juce::Component& root){
    if(auto* t=dynamic_cast<T*>(&root))return t;
    for(auto* child:root.getChildren())if(auto* found=findType<T>(*child))return found;
    return nullptr;
}
void postPageDrag() {
    auto p=make();auto editor=std::make_unique<ChimeraEditor>(*p);editor->setSize(1180,780);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    // Open the POST page through the compact signal path, as a user would.
    auto* path=findType<SignalPathView>(*editor);require(path!=nullptr && path->buttonFor("post")!=nullptr,"Compact signal path or its POST block missing");
    path->buttonFor("post")->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto* overlay=dynamic_cast<postRack::Overlay*>(findID(*editor,"postRackDrag"));
    auto* dynamicsRow=findID(*editor,"postNativePanel0");auto* eqRow=findID(*editor,"postNativePanel2");
    require(overlay!=nullptr && overlay->isVisible() && dynamicsRow!=nullptr && eqRow!=nullptr,"POST page drag layer or rack rows missing");
    require(dynamicsRow->getY()==postRack::rowY(0) && eqRow->getY()==postRack::rowY(2),"Released order does not keep the released rack layout");
    auto* canvas=overlay->getParentComponent();
    struct Writes final : juce::AudioProcessorListener {
        int changes{};void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {++changes;}
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
    } writes;
    const auto source=juce::Desktop::getInstance().getMainMouseSource();
    // A mouse event on `grip` at a canvas point, pressed at `downAt`.
    const auto event=[&](postRack::Grip& grip,juce::Point<int> at,juce::Point<int> downAt,bool down){
        const auto local=grip.getLocalPoint(canvas,at).toFloat(),start=grip.getLocalPoint(canvas,downAt).toFloat();const auto now=juce::Time::getCurrentTime();
        return juce::MouseEvent(source,local,juce::ModifierKeys(down?juce::ModifierKeys::leftButtonModifier:0),1,0,0,0,0,&grip,&grip,now,start,now,1,local.getDistanceFrom(start)>6);};
    const auto gripCentre=[&](postRack::Grip& grip){return canvas->getLocalPoint(&grip,grip.getLocalBounds().getCentre());};
    // 1. Drag POST dynamics over the reverb row, then the delay row, and drop.
    auto& dynamics=overlay->grip(postDynamicsModule);const auto start=gripCentre(dynamics);
    require(postRack::rowBounds(0).contains(start),"The dynamics grip is not on its row");
    p->addListener(&writes);
    dynamics.mouseDown(event(dynamics,start,start,true));
    dynamics.mouseDrag(event(dynamics,postRack::rowBounds(5).getCentre(),start,true));
    const int hoverReverb=overlay->dropModule(),hoverReverbPosition=overlay->dropPosition();
    dynamics.mouseDrag(event(dynamics,postRack::rowBounds(4).getCentre(),start,true));
    const int hoverDelay=overlay->dropModule();const auto caption=overlay->caption();const int writesWhileHovering=writes.changes;
    if(const char* dir=std::getenv("SIGNAL_ORDER_SNAPSHOT")){
        auto out=juce::File(dir).getChildFile("post_page_drag.png");out.deleteFile();auto stream=out.createOutputStream();
        require(stream && juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds(),true,1.f),*stream),"Cannot write the POST page drag snapshot");
    }
    dynamics.mouseUp(event(dynamics,postRack::rowBounds(4).getCentre(),start,false));
    p->removeListener(&writes);
    const auto moved=p->requestedOrder().post;
    require(hoverReverb==postReverbModule && hoverReverbPosition==6 && hoverDelay==postDelayModule && caption=="POST DYNAMICS > POSITION 5" && writesWhileHovering==0,"POST page hover preview wrong, or hovering wrote a parameter");
    require(moved==PostOrder::initial().placed(postDynamicsModule,4),"The drop did not place POST dynamics at position 5");
    // The rows follow the order: dynamics now sits below the delay row.
    require(dynamicsRow->getY()==postRack::rowY(4) && eqRow->getY()==postRack::rowY(1),"Rack rows do not follow the new order");
    require(p->undoOrder() && p->requestedOrder()==SignalOrder{} && !p->canUndoOrder(),"A POST page drop is not exactly one undo step");
    juce::MessageManager::getInstance()->runDispatchLoopUntil(150);   // the editor timer lays the rows out again
    require(dynamicsRow->getY()==postRack::rowY(0),"Rows did not return after undo");
    // 2. Escape cancels; moving on and releasing afterwards does nothing.
    p->addListener(&writes);writes.changes=0;
    dynamics.mouseDown(event(dynamics,start,start,true));dynamics.mouseDrag(event(dynamics,postRack::rowBounds(3).getCentre(),start,true));
    const bool wasDragging=overlay->isDragging();
    require(wasDragging && dynamics.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !overlay->isDragging(),"Escape did not cancel the POST page drag");
    dynamics.mouseDrag(event(dynamics,postRack::rowBounds(5).getCentre(),start,true));require(!overlay->isDragging(),"The drag restarted after Escape");
    dynamics.mouseUp(event(dynamics,postRack::rowBounds(5).getCentre(),start,false));
    // 3. A click (2 pixels) does not reorder; nor does a release outside the rack or over the own row.
    dynamics.mouseDown(event(dynamics,start,start,true));dynamics.mouseUp(event(dynamics,start+juce::Point<int>(2,1),start,false));
    dynamics.mouseDown(event(dynamics,start,start,true));dynamics.mouseDrag(event(dynamics,{600,120},start,true));const int outside=overlay->dropModule();dynamics.mouseUp(event(dynamics,{600,120},start,false));
    dynamics.mouseDown(event(dynamics,start,start,true));dynamics.mouseDrag(event(dynamics,start+juce::Point<int>(300,10),start,true));dynamics.mouseUp(event(dynamics,start+juce::Point<int>(300,10),start,false));
    p->removeListener(&writes);
    require(outside==-1 && p->requestedOrder()==SignalOrder{} && writes.changes==0 && !p->canUndoOrder(),"A cancelled drag or a click changed the order");
    // 4. CREATIVE's strip is a grip and a target too.
    auto& creative=overlay->grip(postCreativeModule);const auto creativeStart=gripCentre(creative);
    require(postRack::creativeStripBounds.contains(creativeStart),"The CREATIVE grip is not on its strip");
    creative.mouseDown(event(creative,creativeStart,creativeStart,true));creative.mouseDrag(event(creative,postRack::rowBounds(3).getCentre(),creativeStart,true));creative.mouseUp(event(creative,postRack::rowBounds(3).getCentre(),creativeStart,false));
    require(p->requestedOrder().post.position(postCreativeModule)==3 && postRack::creativePlacement(p->requestedOrder().post)=="AFTER POST EQ / BEFORE MODULATION","CREATIVE drop wrong");
    auto& reverb=overlay->grip(postReverbModule);const auto reverbStart=gripCentre(reverb);
    reverb.mouseDown(event(reverb,reverbStart,reverbStart,true));reverb.mouseDrag(event(reverb,postRack::creativeStripBounds.getCentre(),reverbStart,true));reverb.mouseUp(event(reverb,postRack::creativeStripBounds.getCentre(),reverbStart,false));
    require(p->requestedOrder().post.position(postReverbModule)==3 && p->requestedOrder().post.position(postCreativeModule)==4,"Dropping on the CREATIVE strip did not take its position");
    std::cout<<"MEASURE POST page drag: hover over REVERB previews position "<<hoverReverbPosition+1<<", caption '"<<caption<<"', "<<writesWhileHovering<<" writes while hovering; dropped at position "<<moved.position(postDynamicsModule)+1<<"; outside target "<<outside<<"\n";
    std::cout<<"PASS POST page drag: rows follow the order, one undoable command on drop, Escape / outside / own row / click change nothing, CREATIVE strip drags and takes drops\n";
}

// POST second slots (docs/POST_SECOND_SLOTS.md).
std::vector<float> renderInput(ChimeraProcessor& p,const std::vector<float>& input){
    std::vector<float> out;juce::AudioBuffer<float> b(2,block);juce::MidiBuffer m;
    for(size_t start=0;start<input.size();start+=block){for(int n=0;n<block;++n){const float x=start+size_t(n)<input.size()?input[start+size_t(n)]:0.f;b.setSample(0,n,x);b.setSample(1,n,x);}
        p.processBlock(b,m);for(int n=0;n<block;++n)out.push_back(b.getSample(0,n));}
    return out;
}
std::unique_ptr<ChimeraProcessor> cleanRig(){auto p=make();set(*p,"mode",0);set(*p,"ampon1",0);set(*p,"cab1",0);return p;}
juce::String s2(const juce::String& id){return postSlotID(1,id);}
// A parameter of POST slot 2, 3 or 4 ("slot2_", "slot3_", "slot4_": the same prefix length).
bool extraSlotID(const juce::String& id){for(int slot=1;slot<postSlotCount;++slot)if(id.startsWith(postSlotPrefix(slot)))return true;return false;}
std::map<juce::String,float> secondSlotValues(ChimeraProcessor& p){
    std::map<juce::String,float> v;
    for(auto* raw:p.getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(raw))if(extraSlotID(r->getParameterID()))v[r->getParameterID()]=p.parameters().getRawParameterValue(r->getParameterID())->load();
    return v;
}
bool secondSlotsAtDefault(ChimeraProcessor& p,bool oldProject=false){
    // A project from before the CREATIVE page keeps the Laser's filter sweep (docs/CREATIVE_PAGE.md).
    for(auto* raw:p.getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(raw))if(extraSlotID(r->getParameterID())){
        const float expected=oldProject && r->getParameterID()=="slot2_creativeLaserMode"?1.f:r->getDefaultValue();
        if(std::abs(r->getValue()-expected)>1.e-6f)return false;}
    return true;
}
void secondSlotsBank() {
    auto p=make();
    // Every second slot starts OFF / bypassed with its section's default model.
    const auto fx2=PostFXChain::engaged;FXState defaults;defaults.postNative=p->postNativeState(1);
    bool anyOn=false;for(int m=0;m<postModuleCount;++m)if(m!=postCreativeModule)anyOn|=fx2(defaults,m);
    require(!anyOn && get(*p,s2("creativeOn"))==0 && get(*p,s2("delayon"))==0 && get(*p,s2("pn_bus_native"))==get(*p,"pn_bus_native") && get(*p,s2("pn_bus_m0_bypass"))==1,"A second slot does not start off");
    // The same definitions as slot 1: range, default and automation flag.
    int copies=0;
    for(auto* raw:p->getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(raw))if(extraSlotID(r->getParameterID())){
        auto* original=p->parameters().getParameter(r->getParameterID().substring(6));require(original!=nullptr,"Second-slot parameter without a slot 1 original: "+r->getParameterID().toStdString());
        // The CREATIVE page's slot-2 copies came later, at their own version (docs/CREATIVE_PAGE.md).
        // Slots 3 and 4 came later still, all at version 37 (docs/POST_FOUR_SLOTS.md).
        // The reverb controls of every slot came together, at version 43 (docs/POST_REVERB_CONTROLS.md).
        const bool reverbControl=isPostReverbAddedVersion(r->getVersionHint());   // with their tempo shape (44) and decay (45)
        const bool late=!r->getParameterID().startsWith("slot2_"),page=late || reverbControl || r->getVersionHint()==creativePageParameterVersion;
        require(r->getVersionHint()==(reverbControl?r->getVersionHint():late?postExtraSlotParameterVersion:page?creativePageParameterVersion:postSecondSlotParameterVersion) && r->getDefaultValue()==original->getDefaultValue() && r->isAutomatable()==original->isAutomatable()
                && r->getNormalisableRange().start==original->getNormalisableRange().start && r->getNormalisableRange().end==original->getNormalisableRange().end,"Second-slot definition differs from slot 1: "+r->getParameterID().toStdString());
        if(!page)++copies;}
    require(copies+postSecondSlotOrderParameterCount==postSecondSlotParameterCount(),"Second-slot bank size");
    // Automation of released parameters still targets slot 1: same IDs, same indices.
    auto* released=p->parameters().getParameter("delaytime");require(released!=nullptr && released->getParameterIndex()<p->getParameters().size()-postSecondSlotParameterCount()-creativePageTotalParameterCount-postExtraSlotParameterCount()-(synthOscCount*synthWavetableParametersPerOsc+synthFiltersParameterCount+synthSequencerParameterCount+(synthEffectsParameterCount+(synthTableImportParameterCount+postReverbControlsParameterCount+(postReverbShapeParameterCount+postReverbAfterShapeParameterCount)))),"A released ID moved");
    std::cout<<"MEASURE second-slot bank: "<<postSecondSlotParameterCount()<<" parameters at version "<<postSecondSlotParameterVersion<<" ("<<copies<<" slot2_ copies + "<<postSecondSlotOrderParameterCount<<" order slots), all off by default\n";
    std::cout<<"PASS second slots: appended bank, slot 1 definitions, off by default, released IDs unchanged\n";
}
void secondSlotsOldState() {
    // A project saved before the second slots: no slot2_ values, no orderPost8-14.
    auto source=make();set(*source,"delayon",1);set(*source,"reverbon",1);source->placeModule(OrderRegion::post,postReverbModule,0);
    juce::MemoryBlock saved;source->getStateInformation(saved);
    juce::ValueTree tree;{auto xml=source->getXmlFromBinary(saved.getData(),int(saved.getSize()));tree=juce::ValueTree::fromXml(*xml);}
    int removed=0;
    for(int i=tree.getNumChildren()-1;i>=0;--i){const auto id=tree.getChild(i).getProperty("id").toString();
        if(extraSlotID(id) || (id.startsWith("orderPost") && id.substring(9).getIntValue()>postModuleCount)){tree.removeChild(i,nullptr);++removed;}}
    // Released order slots store only blocks 0-6, as 1.4 wrote them.
    for(int i=0;i<postModuleCount;++i)require(int(tree.getChildWithProperty("id",orderSlotID(OrderRegion::post,i)).getProperty("value"))<postModuleCount,"Fixture holds a second-slot block");
    juce::MemoryBlock old;{auto xml=tree.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,old);}
    // Load it into a session whose second slots are on and moved.
    auto p=make();set(*p,s2("delayon"),1);set(*p,s2("delaytime"),90);set(*p,s2("creativeOn"),1);p->placeModule(OrderRegion::post,postBlock(postDelayModule,1),0);
    p->setStateInformation(old.getData(),int(old.getSize()));
    PostOrder expected=PostOrder::initial().placed(postReverbModule,0);
    require(secondSlotsAtDefault(*p,true) && p->requestedOrder().post==expected,"An old project does not load with slot 2 off and the released order (second slots after it)");
    // ... and sounds bit-identical to the same project with no second-slot data touched.
    auto reference=make();set(*reference,"delayon",1);set(*reference,"reverbon",1);reference->placeModule(OrderRegion::post,postReverbModule,0);
    const auto a=run(*p,120,.1,330),b=run(*reference,120,.1,330);double diff=0;for(size_t n=0;n<a.size();++n)diff=std::max(diff,double(std::abs(a[n]-b[n])));
    std::cout<<"MEASURE old project: removed "<<removed<<" second-slot values; order "<<(p->requestedOrder().post==expected?"released (+ slot 2 after it)":"other")<<"; largest sample difference vs. the untouched project "<<diff<<"\n";
    require(diff==0,"An old project does not sound bit-identical");
    std::cout<<"PASS old project: slot 2 off, released order with the second slots after REVERB, bit-identical audio\n";
}
void secondSlotsIndependent() {
    // Two delays: slot 1 at 300 ms, slot 2 at 120 ms, no feedback, in series.
    const auto echoes=[](bool second){
        auto p=cleanRig();set(*p,"delayon",1);set(*p,"delaytime",300);set(*p,"delayfeedback",0);set(*p,"delaymix",.5f);
        if(second){set(*p,s2("delayon"),1);set(*p,s2("delaytime"),120);set(*p,s2("delayfeedback"),0);set(*p,s2("delaymix"),.5f);}
        run(*p,40,0.0);   // settle the switches
        std::vector<float> in(size_t(rate*.8),0.f);in[size_t(rate*.1)]=.5f;
        const auto out=renderInput(*p,in);
        const auto energy=[&](double ms){double e=0;const int at=int(rate*(.1+ms*.001));for(int n=at-96;n<at+96;++n)e+=double(out[size_t(n)])*out[size_t(n)];return e;};
        return std::array<double,4>{energy(0),energy(120),energy(300),energy(420)};};
    const auto one=echoes(false),two=echoes(true);
    std::cout<<"MEASURE two delays (300 ms slot 1, 120 ms slot 2), echo energy at 0 / 120 / 300 / 420 ms: slot 1 only "<<one[0]<<" / "<<one[1]<<" / "<<one[2]<<" / "<<one[3]
             <<"; both "<<two[0]<<" / "<<two[1]<<" / "<<two[2]<<" / "<<two[3]<<"\n";
    require(one[2]>one[1]*1000 && one[2]>one[3]*1000 && two[1]>two[0]*.05 && two[3]>two[0]*.01 && two[2]>two[0]*.05,"The second delay does not run as its own instance");
    // Changing slot 2 leaves slot 1's parameters alone, and the other way round.
    auto p=make();const auto before=get(*p,"delaytime");set(*p,s2("delaytime"),77);set(*p,"reverbsize",.9f);
    require(get(*p,"delaytime")==before && get(*p,s2("reverbsize"))==.35f,"Slot 1 and slot 2 share a parameter");
    std::cout<<"PASS slot 2 runs its own processor: a second delay adds its own echoes; no shared parameters\n";
}
void secondSlotsBypass() {
    // A bypassed second slot is a no-op wherever it sits and whatever its settings.
    const auto base=[]{auto p=make();set(*p,"delayon",1);set(*p,"reverbon",1);set(*p,"choruson",1);return p;};
    auto x=base(),y=base();
    for(int m=0;m<postModuleCount;++m)y->placeModule(OrderRegion::post,postBlock(m,1),y->requestedOrder().post.position(m)+1);   // each slot 2 right after its slot 1
    set(*y,s2("delaytime"),500);set(*y,s2("reverbmix"),.6f);set(*y,s2("pn_eq_modelExt"),2);set(*y,s2("pn_bus_m0_threshold"),-40);set(*y,s2("creativeModel"),4);
    const auto a=run(*x,160,.1,440),b=run(*y,160,.1,440);double diff=0;for(size_t n=0;n<a.size();++n)diff=std::max(diff,double(std::abs(a[n]-b[n])));
    require(y->getLatencySamples()==x->getLatencySamples(),"A bypassed second slot changed the latency");
    // CPU: the whole processBlock with every second slot off, then all on.
    const auto timed=[](ChimeraProcessor& p){juce::AudioBuffer<float> b(2,block);juce::MidiBuffer m;b.clear();const auto t0=juce::Time::getHighResolutionTicks();for(int k=0;k<400;++k)p.processBlock(b,m);return juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks()-t0)/400.0*1.e6;};
    auto z=base();for(const char* id:{"delayon","reverbon","choruson","creativeOn"})set(*z,s2(id),1);
    for(const char* id:{"pn_bus_m0_bypass","pn_eq_m0_bypass"})set(*z,s2(id),0);
    run(*z,8);run(*y,8);
    run(*x,8);timed(*x);timed(*y);timed(*z);   // warm up
    const double released=timed(*x),off=timed(*y),on=timed(*z);
    std::cout<<"MEASURE bypassed second slots, interleaved and with changed settings: largest sample difference "<<diff<<"; latency "<<y->getLatencySamples()<<" = "<<x->getLatencySamples()
             <<"; processBlock (256 samples) "<<released<<" us before any second-slot edit, "<<off<<" us with every second slot off (interleaved), "<<on<<" us with six of them on\n";
    require(diff==0,"A bypassed second slot changed the sound");
    std::cout<<"PASS bypassed second slots are a sample-exact no-op and add no latency\n";
}
void secondSlotsLatency() {
    // The second preamp is the only POST block with latency: it reports it while on.
    auto p=make();const int base=p->getLatencySamples();
    set(*p,s2("pn_preamp_m0_bypass"),0);run(*p,8,.1,220);const int on=p->getLatencySamples();const bool active=p->postSecondPreampActive();
    set(*p,s2("pn_preamp_m0_bypass"),1);run(*p,8,.1,220);const int off=p->getLatencySamples();
    std::cout<<"MEASURE second POST preamp latency: base "<<base<<", on "<<on<<", off again "<<off<<" samples\n";
    require(active && on>base && off==base && !p->postSecondPreampActive(),"Second preamp latency not reported or not released");
    // The reported latency changes in the same block as the audio switch,
    // never one block later: checked after every block of both transitions.
    for(const float bypass:{0.f,1.f}){
        set(*p,s2("pn_preamp_m0_bypass"),bypass);
        for(int k=0;k<40;++k){run(*p,1,.1,220,k);const bool running=p->postSecondPreampActive();const int now=p->getLatencySamples();
            require(running ? now==on : now==base,"Reported latency lags the second preamp switch at block "+std::to_string(k));}
        require(p->postSecondPreampActive()==(bypass<.5f),"Second preamp did not switch within 40 blocks");}
    std::cout<<"PASS the second preamp reports its latency only while it runs, from the block it switches in\n";
}
void secondSlotsOrder() {
    auto p=make();
    // 14 movable blocks: DELAY 2 to the front, then one later, undo / redo.
    const int delay2=postBlock(postDelayModule,1);
    struct Watch final : juce::AudioProcessorListener {
        ChimeraProcessor* q{};int thirdValid{};PostOrder from,to;
        void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {
            PostOrder o;for(int i=0;i<postBlockCount;++i)o.slots[size_t(i)]=juce::roundToInt(q->parameters().getRawParameterValue(orderSlotID(OrderRegion::post,i))->load());
            if(o.valid() && o!=from && o!=to)++thirdValid;}
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
    } watch;watch.q=p.get();watch.from=PostOrder::initial();watch.to=PostOrder::initial().placed(delay2,0);
    p->addListener(&watch);require(p->placeModule(OrderRegion::post,delay2,0),"DELAY 2 move refused");p->removeListener(&watch);
    require(p->requestedOrder().post.slots[0]==delay2 && watch.thirdValid==0,"DELAY 2 did not land first, or a third order was written");
    require(p->moveModule(OrderRegion::post,delay2,1) && p->requestedOrder().post.position(delay2)==1 && p->undoOrder() && p->requestedOrder().post.position(delay2)==0 && p->redoOrder() && p->requestedOrder().post.position(delay2)==1,"Move / undo / redo with 14 blocks");
    require(!p->moveModule(OrderRegion::post,postBlock(postReverbModule,postSlotCount-1),1) && p->placeModule(OrderRegion::post,postDynamicsModule,postBlockCount-1) && p->requestedOrder().post.slots[size_t(postBlockCount-1)]==postDynamicsModule,"Edges of the 14-block order");
    // Stale slots: a duplicate keeps the last valid order.
    const auto valid=p->requestedOrder();run(*p,2);
    set(*p,orderSlotID(OrderRegion::post,postBlockCount-1),float(delay2));
    SignalOrderParameters reader;reader.bind(p->parameters());require(reader.read(valid).post==valid.post && p->requestedOrder().post==PostOrder::initial(),"Stale 14-block order not handled");
    run(*p,2);
    // Presets order the sections; the second slots follow in the same order.
    auto q=make();require(q->applyOrderPreset(OrderRegion::post,1) && q->requestedOrder().post.slots[0]==postEQModule && q->requestedOrder().post.slots[size_t(postModuleCount)]==postBlock(postEQModule,1),"Order preset with second slots");
    // The routing graph: 14 POST blocks, the second ones labelled.
    // An unused slot 2-4 is not drawn (docs/POST_FOUR_SLOTS.md); a slot that is on is.
    auto s=signalPath::read(*q);
    int idleNodes=0;for(const auto& n:s.nodes)idleNodes+=n.target==signalPath::Target::post;
    require(s.find("post2.section4")==nullptr && idleNodes==postModuleCount,"An unused slot is drawn in the routing graph");
    set(*q,s2("delayon"),1);s=signalPath::read(*q);const auto* node=s.find("post2.section4");
    require(node!=nullptr && node->label=="DELAY 2" && node->status!="BYPASSED" && node->slot==1 && node->active,"DELAY 2 missing from the routing graph");
    set(*q,postSlotID(3,"reverbon"),1);s=signalPath::read(*q);
    require(s.find("post4.section5")!=nullptr && s.find("post4.section5")->label=="REVERB 4" && s.find("post4.section5")->slot==3,"REVERB 4 missing from the routing graph");
    int postNodes=0;for(const auto& n:s.nodes)postNodes+=n.target==signalPath::Target::post;
    for(size_t i=0;i<s.nodes.size();++i)for(size_t j=i+1;j<s.nodes.size();++j)require(!s.nodes[i].bounds.intersects(s.nodes[j].bounds),"Path blocks overlap with 14 POST blocks");
    for(const auto& n:s.nodes)require(n.bounds.getBottom()<=700,"A block falls outside the routing graph");
    {   // Every block on: 28 POST blocks still fit three rows without overlap.
        auto full=make();for(int block=postModuleCount;block<postBlockCount;++block)full->setPostBlockOn(block,true);
        const auto all=signalPath::read(*full);int n=0;for(const auto& node:all.nodes)n+=node.target==signalPath::Target::post;
        require(n==postBlockCount,"Not every active POST block is drawn");
        for(size_t i=0;i<all.nodes.size();++i){require(all.nodes[i].bounds.getBottom()<=700 && all.nodes[i].bounds.getRight()<=1140,"A block falls outside the full routing graph");
            for(size_t j=i+1;j<all.nodes.size();++j)require(!all.nodes[i].bounds.intersects(all.nodes[j].bounds),"Path blocks overlap with 28 POST blocks");}
    }
    SignalPathView view(*q,false);view.setSize(1140,700);
    require(view.dropTargets(OrderRegion::post).size()==size_t(postBlockCount) && view.orderCommand("post2.section4",SignalPathView::OrderCommand::first) && q->requestedOrder().post.slots[0]==delay2,"Routing graph cannot move a second slot");
    std::cout<<"MEASURE routing graph: "<<postNodes<<" POST blocks; DELAY 2 at "<<s.find("post2.section4")->bounds.toString()<<"\n";
    std::cout<<"PASS 14-block order: moves, undo / redo, edges, stale slots, presets, routing graph labels and commands\n";
}
void secondSlotsState() {
    auto p=make();
    set(*p,s2("delayon"),1);set(*p,s2("delaytime"),123);set(*p,s2("pn_eq_m0_bypass"),0);set(*p,s2("pn_eq_modelExt"),1);set(*p,s2("creativeOn"),1);set(*p,s2("creativeModel"),3);set(*p,s2(effectExtensionID(10)),2);
    p->placeModule(OrderRegion::post,postBlock(postDelayModule,1),2);
    const auto values=secondSlotValues(*p);const auto order=p->requestedOrder();
    // Project round trip.
    juce::MemoryBlock saved;p->getStateInformation(saved);
    auto q=make();q->setStateInformation(saved.getData(),int(saved.getSize()));
    require(secondSlotValues(*q)==values && q->requestedOrder()==order,"Project state lost slot 2");
    // A recall restarts the processors (CREATIVE 2 then starts at its mix at
    // once), so the original reloads its own state before the comparison.
    p->setStateInformation(saved.getData(),int(saved.getSize()));
    const auto a=run(*p,60,.1,330),b=run(*q,60,.1,330);double diff=0;for(size_t n=0;n<a.size();++n)diff=std::max(diff,double(std::abs(a[n]-b[n])));
    // A/B: COPY duplicates slot 2; each comparison keeps its own.
    p->copyComparison();set(*p,s2("delaytime"),400);p->placeModule(OrderRegion::post,postBlock(postDelayModule,1),0);
    const auto changed=secondSlotValues(*p);const auto changedOrder=p->requestedOrder();
    p->selectComparison(1);const bool bKeeps=secondSlotValues(*p)==values && p->requestedOrder()==order;
    p->selectComparison(0);const bool aKeeps=secondSlotValues(*p)==changed && p->requestedOrder()==changedOrder;
    // Factory presets start every second slot off, in the released order.
    p->loadFactoryPreset(0);
    std::cout<<"MEASURE second-slot state: "<<values.size()<<" values; project round trip "<<(diff==0?"sample-exact":"different")<<" ("<<diff<<"); A/B "<<(aKeeps&&bKeeps?"keep":"lose")<<" their own slot 2\n";
    require(diff==0 && aKeeps && bKeeps,"A/B or audio round trip of slot 2 wrong");
    require(secondSlotsAtDefault(*p) && p->requestedOrder().post==PostOrder::initial(),"A factory preset kept a second slot");
    std::cout<<"PASS project save / load, A/B copy and factory presets round-trip slot 2\n";
}
// Slots 3 and 4 (docs/POST_FOUR_SLOTS.md).
void extraSlotsBank() {
    auto p=make();int bank=0,order=0;std::array<int,postSlotCount> copies{};
    const int total=p->getParameters().size();
    for(auto* raw:p->getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(raw))if(r->getVersionHint()==postExtraSlotParameterVersion){
        require(r->getParameterIndex()>=total-postExtraSlotParameterCount()-(synthOscCount*synthWavetableParametersPerOsc+synthFiltersParameterCount+synthSequencerParameterCount+(synthEffectsParameterCount+(synthTableImportParameterCount+postReverbControlsParameterCount+(postReverbShapeParameterCount+postReverbAfterShapeParameterCount)))),"A slot 3 / 4 parameter is not in the last bank");++bank;
        if(r->getParameterID().startsWith("orderPost")){++order;continue;}
        const int slot=r->getParameterID().startsWith("slot3_")?2:r->getParameterID().startsWith("slot4_")?3:-1;require(slot>0,"Unexpected ID in the slot 3 / 4 bank: "+r->getParameterID().toStdString());
        ++copies[size_t(slot)];
        // The same definition as slot 2, which equals slot 1.
        auto* twin=p->parameters().getParameter(postSlotID(1,r->getParameterID().substring(6)));require(twin!=nullptr,"No slot 2 twin: "+r->getParameterID().toStdString());
        require(r->getDefaultValue()==twin->getDefaultValue() && r->isAutomatable()==twin->isAutomatable() && r->getNormalisableRange().start==twin->getNormalisableRange().start && r->getNormalisableRange().end==twin->getNormalisableRange().end
                && r->getName(64).endsWith("(slot "+juce::String(slot+1)+")"),"Slot definition differs from slot 2: "+r->getParameterID().toStdString());}
    // Slot 2's reverb controls are not part of what slots 3 and 4 copied: theirs sit in the reverb controls bank (version 43).
    int slot2=0;for(auto* raw:p->getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(raw))slot2+=r->getParameterID().startsWith("slot2_") && !isPostReverbAddedVersion(r->getVersionHint());
    require(bank==postExtraSlotParameterCount() && order==postExtraSlotOrderParameterCount && copies[2]==slot2 && copies[3]==slot2,"Slot 3 / 4 bank size");
    for(int block=postModuleCount;block<postBlockCount;++block)require(!p->postBlockOn(block) && !p->postBlockShown(block),"A slot 2-4 block starts on");
    for(int block=0;block<postModuleCount;++block)require(p->postBlockShown(block),"A slot 1 block is hidden");
    std::cout<<"MEASURE slot 3 / 4 bank: "<<bank<<" parameters at version "<<postExtraSlotParameterVersion<<" ("<<order<<" order slots, "<<copies[2]<<" per slot); "<<total<<" parameters in all\n";
    std::cout<<"PASS slots 3 and 4: appended bank, slot 2's definitions, off by default\n";
}
void extraSlotsAudio() {
    // Four delays in series, each with its own time: every slot adds its own echo.
    const std::array<float,postSlotCount> times{300,120,200,60};
    const auto echoes=[&](int slots){
        auto p=cleanRig();
        for(int slot=0;slot<slots;++slot){set(*p,postSlotID(slot,"delayon"),1);set(*p,postSlotID(slot,"delaytime"),times[size_t(slot)]);set(*p,postSlotID(slot,"delayfeedback"),0);set(*p,postSlotID(slot,"delaymix"),.5f);}
        run(*p,40,0.0);
        std::vector<float> in(size_t(rate*.9),0.f);in[size_t(rate*.1)]=.5f;const auto out=renderInput(*p,in);
        const auto energy=[&](double ms){double e=0;const int at=int(rate*(.1+ms*.001));for(int n=at-96;n<at+96;++n)e+=double(out[size_t(n)])*out[size_t(n)];return e;};
        return std::array<double,4>{energy(0),energy(60),energy(200),energy(300)};};
    const auto two=echoes(2),four=echoes(4);
    std::cout<<"MEASURE four delays (300 / 120 / 200 / 60 ms), echo energy at 0 / 60 / 200 / 300 ms: slots 1-2 "<<two[0]<<" / "<<two[1]<<" / "<<two[2]<<" / "<<two[3]<<"; slots 1-4 "<<four[0]<<" / "<<four[1]<<" / "<<four[2]<<" / "<<four[3]<<"\n";
    require(two[3]>two[1]*1000 && two[3]>two[2]*1000 && four[1]>four[0]*.05 && four[2]>four[0]*.05,"Delay 3 or 4 does not run as its own instance");
    // Off: sample-exact no-op wherever the blocks sit and whatever their settings.
    const auto base=[]{auto p=make();set(*p,"delayon",1);set(*p,"reverbon",1);return p;};
    auto x=base(),y=base();
    for(int slot=2;slot<postSlotCount;++slot){set(*y,postSlotID(slot,"delaytime"),90);set(*y,postSlotID(slot,"reverbmix"),1);set(*y,postSlotID(slot,"creativeModel"),3);
        y->placeModule(OrderRegion::post,postBlock(postDelayModule,slot),0);y->placeModule(OrderRegion::post,postBlock(postReverbModule,slot),3);}
    const auto a=run(*x,120,.1,330),b=run(*y,120,.1,330);double diff=0;for(size_t n=0;n<a.size();++n)diff=std::max(diff,double(std::abs(a[n]-b[n])));
    require(diff==0 && x->getLatencySamples()==y->getLatencySamples(),"Unused slots 3 / 4 change the sound or the latency");
    // Latency: each extra preamp adds its own, only while it runs.
    auto p=make();run(*p,8,.1,220);const int none=p->getLatencySamples();std::array<int,postSlotCount> steps{};
    for(int slot=1;slot<postSlotCount;++slot){set(*p,postSlotID(slot,"pn_preamp_m0_bypass"),0);run(*p,12,.1,220);steps[size_t(slot)]=p->getLatencySamples();require(p->postPreampActive(slot),"Preamp "+std::to_string(slot+1)+" did not start");}
    const int each=steps[1]-none;
    require(each>0 && steps[2]-steps[1]==each && steps[3]-steps[2]==each,"Extra preamps do not each add their latency");
    for(int slot=1;slot<postSlotCount;++slot)set(*p,postSlotID(slot,"pn_preamp_m0_bypass"),1);
    run(*p,24,.1,220);require(p->getLatencySamples()==none,"Extra preamp latency not released");
    std::cout<<"MEASURE latency: none "<<none<<", +1 preamp "<<steps[1]<<", +2 "<<steps[2]<<", +3 "<<steps[3]<<" samples; unused slots: largest sample difference "<<diff<<"\n";
    std::cout<<"PASS slots 3 and 4 run their own processors, are a sample-exact no-op when off, and report preamp latency per slot\n";
}
void extraSlotsOldState() {
    // A 1.5 project: slots 1 and 2 only, 14 order positions with DELAY 2 placed first and on.
    auto source=make();set(*source,"delayon",1);set(*source,s2("delayon"),1);set(*source,s2("delaytime"),150);source->placeModule(OrderRegion::post,postBlock(postDelayModule,1),0);
    juce::MemoryBlock saved;source->getStateInformation(saved);
    juce::ValueTree tree;{auto xml=source->getXmlFromBinary(saved.getData(),int(saved.getSize()));tree=juce::ValueTree::fromXml(*xml);}
    int removed=0;
    for(int i=tree.getNumChildren()-1;i>=0;--i){const auto id=tree.getChild(i).getProperty("id").toString();
        if(id.startsWith("slot3_") || id.startsWith("slot4_") || (id.startsWith("orderPost") && id.substring(9).getIntValue()>postSecondSlotBlockCount)){tree.removeChild(i,nullptr);++removed;}}
    for(int i=0;i<postSecondSlotBlockCount;++i)require(int(tree.getChildWithProperty("id",orderSlotID(OrderRegion::post,i)).getProperty("value"))<postSecondSlotBlockCount,"Fixture holds a slot 3 / 4 block");
    juce::MemoryBlock old;{auto xml=tree.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,old);}
    // Load it into a session whose slots 3 and 4 are on and moved.
    auto p=make();set(*p,postSlotID(2,"delayon"),1);set(*p,postSlotID(3,"reverbon"),1);p->placeModule(OrderRegion::post,postBlock(postReverbModule,3),0);
    p->setStateInformation(old.getData(),int(old.getSize()));
    const PostOrder expected=PostOrder::initial().placed(postBlock(postDelayModule,1),0);
    bool late=false;for(auto* raw:p->getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(raw))if((r->getParameterID().startsWith("slot3_") || r->getParameterID().startsWith("slot4_")) && std::abs(r->getValue()-r->getDefaultValue())>1.e-6f)late=true;
    require(!late && p->requestedOrder().post==expected && get(*p,s2("delayon"))==1 && get(*p,s2("delaytime"))==150,"A 1.5 project does not load with slots 3 / 4 off and its order");
    auto reference=make();set(*reference,"delayon",1);set(*reference,s2("delayon"),1);set(*reference,s2("delaytime"),150);reference->placeModule(OrderRegion::post,postBlock(postDelayModule,1),0);
    const auto a=run(*p,120,.1,330),b=run(*reference,120,.1,330);double diff=0;for(size_t n=0;n<a.size();++n)diff=std::max(diff,double(std::abs(a[n]-b[n])));
    std::cout<<"MEASURE 1.5 project: removed "<<removed<<" slot 3 / 4 values; largest sample difference vs. the untouched project "<<diff<<"\n";
    require(diff==0,"A 1.5 project does not sound bit-identical");
    std::cout<<"PASS 1.5 project: slots 3 / 4 off, saved order kept, bit-identical audio\n";
}
void extraSlotsAdd() {
    auto p=make();
    // Add a delay after POST EQ: slot 2 switches on, right after POST EQ.
    const int first=p->addPostBlock(postDelayModule,postEQModule);
    require(first==postBlock(postDelayModule,1) && p->postBlockOn(first) && p->requestedOrder().post.position(first)==p->requestedOrder().post.position(postEQModule)+1,"Added delay is not slot 2 right after POST EQ");
    // Two more: slots 3 and 4; the fifth is refused. Without an anchor a block goes after the last block in use.
    const int second=p->addPostBlock(postDelayModule),third=p->addPostBlock(postDelayModule);
    require(second==postBlock(postDelayModule,2) && third==postBlock(postDelayModule,3) && p->addPostBlock(postDelayModule)==-1,"Delay slots 3 / 4 not added, or a fifth accepted");
    {const auto order=p->requestedOrder().post;int lastShown=-1;for(int i=0;i<postBlockCount;++i)if(p->postBlockShown(order.slots[size_t(i)]))lastShown=order.slots[size_t(i)];
     require(lastShown==third && order.position(second)==order.position(third)-1,"A block added without an anchor is not last in the chain");}
    // Every section: dynamics, preamp and EQ go through their native bank's bypass.
    for(int section=0;section<postModuleCount;++section)if(section!=postDelayModule){const int block=p->addPostBlock(section);require(block==postBlock(section,1) && p->postBlockOn(block),"Could not add section "+std::to_string(section));}
    // One undo step takes back the placement; switching off hides the block again.
    p->setPostBlockOn(first,false);require(!p->postBlockOn(first) && !p->postBlockShown(first),"Removing a block did not switch it off");
    // The graph: add from the view, after a chosen block; earlier / later step over hidden blocks.
    auto q=make();SignalPathView view(*q,false);view.setSize(1140,700);
    require(view.addPostBlock(postReverbModule,"post.section0") && q->postBlockOn(postBlock(postReverbModule,1)),"The routing graph cannot add a block");
    auto s=signalPath::read(*q);const auto edge=[&](const char* a,const char* b){for(const auto& e:s.edges)if(e.from==a&&e.to==b)return true;return false;};
    require(edge("post.section0","post2.section5") && edge("post2.section5","post.section1"),"The added block is not drawn right after the chosen block");
    require(view.orderCommand("post.section5",SignalPathView::OrderCommand::earlier),"Move earlier refused");s=signalPath::read(*q);
    require(edge("post.section5","post.creative") && edge("post.section4","post.section5"),"Move earlier did not step by one drawn block");
    require(view.orderCommand("post.section5",SignalPathView::OrderCommand::later) && view.orderCommand("post.section5",SignalPathView::OrderCommand::later)==false,"Move later past the last drawn block");
    const auto targets=view.dropTargets(OrderRegion::post);int shown=0;for(const auto& r:targets)shown+=!r.isEmpty();
    require(targets.size()==size_t(postBlockCount) && shown==postModuleCount+1,"Drop targets do not match the drawn blocks");
    std::cout<<"PASS add / remove: next free slot, placed after the chosen block, four per section, graph commands skip unused slots\n";
}
void secondSlotsPage() {
    auto p=make();auto editor=std::make_unique<ChimeraEditor>(*p);editor->setSize(1180,780);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    findType<SignalPathView>(*editor)->buttonFor("post")->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto* selector=dynamic_cast<postSlotUI::SlotSelector*>(findID(*editor,"postSlot4"));auto* row2=findID(*editor,"postSlotRow1_2");auto* delay1=findID(*editor,"delaymodel");
    auto* overlay=dynamic_cast<postRack::Overlay*>(findID(*editor,"postRackDrag"));
    require(selector!=nullptr && row2!=nullptr && delay1!=nullptr && overlay!=nullptr,"Slot switch, DELAY 2 row or drag layer missing");
    require(selector->isVisible() && !row2->isVisible() && delay1->isVisible() && delay1->getBounds()==juce::Rectangle<int>(48,postRack::rowY(4)+23,182,24),"Slot 1 not shown by default");
    struct Writes final : juce::AudioProcessorListener {
        int changes{};void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {++changes;}
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
    } writes;p->addListener(&writes);
    selector->button(1).triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    p->removeListener(&writes);
    require(writes.changes==0 && row2->isVisible() && !delay1->isVisible() && row2->getY()==postRack::rowY(4),"The slot switch wrote a parameter or did not show DELAY 2");
    // Edit DELAY 2 from its row: its ON writes slot 2 only.
    auto* on2=dynamic_cast<juce::Button*>(findID(*row2,s2("delayon")));require(on2!=nullptr,"DELAY 2 ON missing");
    on2->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    require(get(*p,s2("delayon"))==1 && get(*p,"delayon")==0,"DELAY 2 ON did not write slot 2 only");
    // Drag DELAY 2 by its grip onto the POST EQ row.
    auto& grip=overlay->grip(postBlock(postDelayModule,1));require(grip.isVisible() && !overlay->grip(postDelayModule).isVisible(),"Grip does not follow the shown slot");
    auto* canvas=overlay->getParentComponent();const auto source=juce::Desktop::getInstance().getMainMouseSource();
    const auto event=[&](juce::Point<int> at,juce::Point<int> downAt,bool down){const auto local=grip.getLocalPoint(canvas,at).toFloat(),start=grip.getLocalPoint(canvas,downAt).toFloat();const auto now=juce::Time::getCurrentTime();
        return juce::MouseEvent(source,local,juce::ModifierKeys(down?juce::ModifierKeys::leftButtonModifier:0),1,0,0,0,0,&grip,&grip,now,start,now,1,local.getDistanceFrom(start)>6);};
    const auto start=canvas->getLocalPoint(&grip,grip.getLocalBounds().getCentre());
    grip.mouseDown(event(start,start,true));grip.mouseDrag(event(postRack::rowBounds(2).getCentre(),start,true));const auto caption=overlay->caption();
    grip.mouseUp(event(postRack::rowBounds(2).getCentre(),start,false));
    require(caption=="DELAY 2 > POSITION 3" && p->requestedOrder().post.slots[2]==postBlock(postDelayModule,1) && p->undoOrder() && p->requestedOrder()==SignalOrder{},"DELAY 2 drag on the POST page wrong");
    // CREATIVE 2 and the native POST rows switch the same way.
    dynamic_cast<postSlotUI::SlotSelector*>(findID(*editor,"postSlot5"))->button(1).triggerClick();
    dynamic_cast<postSlotUI::SlotSelector*>(findID(*editor,"postSlot0"))->button(1).triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    require(findID(*editor,"creativeStrip_2")->isVisible() && !findID(*editor,"creativeStrip")->isVisible() && findID(*editor,"postNativePanel0_2")->isVisible() && !findID(*editor,"postNativePanel0")->isVisible(),"CREATIVE 2 or POST dynamics 2 not shown");
    // Navigation from the routing graph opens the second slot it names.
    selector->button(0).triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(20);require(!row2->isVisible(),"Back to slot 1 failed");
    // The page at the tested editor scales: slot switches stay clear of the row controls.
    const auto clear=[&](const juce::String& id){auto* c=findID(*editor,id);return c==nullptr || !c->isVisible() || !c->getBoundsInParent().intersects(selector->getBoundsInParent());};
    for(const float scale:{.75f,1.f,1.25f,1.5f}){
        editor->setViewSize({int(1180*scale),int(780*scale)},scale);juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        for(int section=0;section<postModuleCount;++section){auto* sel=findID(*editor,"postSlot"+juce::String(section));require(sel!=nullptr && sel->isVisible() && sel->getWidth()>=40 && sel->getHeight()>=17,"Slot switch missing at scale");}
        require(clear("delaymodel") && clear("delayon") && clear("postExpand4"),"Slot switch overlaps a row control");
        selector->button(1).triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(20);   // the snapshot shows DELAY 2
        const auto clear2=[&](const juce::String& id){auto* c=findID(*row2,id);return c!=nullptr && c->isVisible() && !(c->getBoundsInParent()+row2->getPosition()).intersects(selector->getBoundsInParent());};
        require(row2->isVisible() && clear2(s2("delaymodel")) && clear2(s2("delayon")) && clear2("postExpand4_2"),"Slot switch overlaps a DELAY 2 control");
        if(const char* dir=std::getenv("SIGNAL_ORDER_SNAPSHOT")){
            auto out=juce::File(dir).getChildFile("post_second_slots_"+juce::String(juce::roundToInt(scale*100))+".png");out.deleteFile();auto stream=out.createOutputStream();
            require(stream && juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds(),true,1.f),*stream),"Cannot write the second-slot snapshot");
        }
        selector->button(0).triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }
    std::cout<<"PASS POST page: slot 1 / 2 switches show each second slot without writing, DELAY 2 edits slot 2, drags as DELAY 2, scales 75-150 %\n";
}

// The POST page with four slots. CHIMERA_POST_SNAPSHOTS=<dir> also writes the
// page and the routing graph as PNG files (2x).
void extraSlotsPage() {
    auto p=make();auto editor=std::make_unique<ChimeraEditor>(*p);editor->setEasyMode(false,false);editor->setSize(1180,780);
    const auto pump=[]{juce::MessageManager::getInstance()->runDispatchLoopUntil(60);};pump();
    findType<SignalPathView>(*editor)->buttonFor("post")->triggerClick();pump();
    auto* selector=dynamic_cast<postSlotUI::SlotSelector*>(findID(*editor,"postSlot4"));auto* add=dynamic_cast<juce::Button*>(findID(*editor,"postAdd"));
    require(selector!=nullptr && add!=nullptr && add->isVisible(),"Slot switch or + ADD missing");
    for(int slot=0;slot<postSlotCount;++slot)require(selector->button(slot).isVisible() && selector->button(slot).getWidth()>=14,"Slot switch button "+std::to_string(slot+1)+" missing or too small");
    // Each slot's rows exist and show when chosen; choosing writes nothing.
    juce::MemoryBlock before,after;p->getStateInformation(before);
    for(int slot=1;slot<postSlotCount;++slot){
        selector->button(slot).triggerClick();pump();
        auto* row=findID(*editor,"postSlotRow1"+postSlotSuffix(slot));
        require(row!=nullptr && row->isVisible() && row->getY()==postRack::rowY(4),"DELAY "+std::to_string(slot+1)+" row not shown");
        for(int other=1;other<postSlotCount;++other)if(other!=slot)require(!findID(*editor,"postSlotRow1"+postSlotSuffix(other))->isVisible(),"Two DELAY rows shown at once");
    }
    for(int section=0;section<3;++section)for(int slot=1;slot<postSlotCount;++slot)require(findID(*editor,"postNativePanel"+juce::String(section)+postSlotSuffix(slot))!=nullptr,"A native POST panel of slot 2-4 is missing");
    for(int slot=1;slot<postSlotCount;++slot)require(findID(*editor,"creativeStrip"+postSlotSuffix(slot))!=nullptr,"A CREATIVE strip of slot 2-4 is missing");
    p->getStateInformation(after);require(before==after,"Choosing a slot changed the project");
    // The switch lights the slots that are on.
    set(*p,postSlotID(2,"delayon"),1);juce::MessageManager::getInstance()->runDispatchLoopUntil(250);
    require(selector->slotShownActive(2) && !selector->slotShownActive(1) && !selector->slotShownActive(3),"The slot switch does not light slot 3 alone");
    // The switch stays clear of the row's controls.
    {auto* row=findID(*editor,"postSlotRow1_3");selector->button(2).triggerClick();pump();
     const auto box=selector->getBounds();
     for(auto* child:row->getChildren())if(child->isVisible() && dynamic_cast<juce::Label*>(child)==nullptr)require(!child->getBounds().translated(row->getX(),row->getY()).intersects(box),"Slot switch overlaps a DELAY 3 control: "+child->getComponentID().toStdString());}
    if(const auto directory=juce::SystemStats::getEnvironmentVariable("CHIMERA_POST_SNAPSHOTS",{});directory.isNotEmpty()){
        const auto save=[&](juce::Component& c,const juce::String& name){juce::MessageManager::getInstance()->runDispatchLoopUntil(300);const auto image=c.createComponentSnapshot(c.getLocalBounds(),true,2.f);
            juce::File file=juce::File(directory).getChildFile(name);file.getParentDirectory().createDirectory();file.deleteFile();juce::FileOutputStream out(file);juce::PNGImageFormat png;require(out.openedOk() && png.writeImageToStream(image,out),"Snapshot not written");};
        set(*p,"delayon",1);set(*p,"reverbon",1);set(*p,postSlotID(2,"delaytime"),180);
        p->addPostBlock(postReverbModule,postDynamicsModule);p->addPostBlock(postModulationModule);
        save(*editor,"post-page-delay3.png");
        SignalPathView graph(*p,false);graph.setSize(1140,700);save(graph,"routing-added.png");
        for(int block=postModuleCount;block<postBlockCount;++block)p->setPostBlockOn(block,true);
        graph.refresh();save(graph,"routing-all-28.png");
    }
    std::cout<<"PASS POST page: four-slot switches, slot 2-4 rows, lights, + ADD; choosing a slot writes nothing\n";
}
// POST reverb controls (docs/POST_REVERB_CONTROLS.md): the bank, the audio of a slot, and state.
void reverbControlsBank() {
    auto p=make();const int total=p->getParameters().size();int count=0;
    for(auto* parameter:p->getParameters())if(auto* with=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))if(with->getVersionHint()==postReverbControlsParameterVersion) {
        const int slot=count/postReverbControlsPerSlot,control=count%postReverbControlsPerSlot;const auto& spec=reverbControlSpecs[size_t(control)];
        require(with->getParameterIndex()==total-postReverbControlsParameterCount-(postReverbShapeParameterCount+postReverbAfterShapeParameterCount)+count && with->paramID==reverbControlID(slot,control) && parameter->isAutomatable(),"A reverb control is not in the last bank, slot 1 first");
        require(with->getName(64)==postSlotName(slot,spec.name),"A reverb control has the wrong host name: "+with->getName(64).toStdString());
        require(std::abs(get(*p,with->paramID)-spec.initial)<1e-6f,"A reverb control does not start at its default: "+with->paramID.toStdString());++count;}
    require(count==postReverbControlsParameterCount && count==20,"The reverb controls bank is not five parameters for each of four slots");
    // The defaults ask nothing of the reverb.
    const ReverbControls plain;
    require(get(*p,"reverbpredelay")==plain.preDelayMs && get(*p,"reverblowcut")==plain.lowCutHz && get(*p,"reverbtone")==plain.tone && get(*p,"reverbwidth")==plain.width && get(*p,"reverbfreeze")<.5f,
            "The parameter defaults are not the reverb's plain controls");
    // Each slot has its own.
    set(*p,"slot2_reverbpredelay",120);set(*p,"slot4_reverbfreeze",1);
    require(get(*p,"reverbpredelay")==0 && get(*p,"slot3_reverbpredelay")==0 && get(*p,"reverbfreeze")<.5f && get(*p,"slot3_reverbfreeze")<.5f,"Slots share a reverb control");
    std::cout<<"PASS reverb controls bank: 20 parameters at version 43, last, five for each slot, at defaults that leave the reverb as it was\n";
}
void reverbControlsAudio() {
    // 60 Hz through the rig with the reverb on: LOW CUT takes it out of the reverb, so the output changes; 4 kHz does not.
    const auto tone=[](double hz){std::vector<float> x(size_t(rate*2));for(size_t n=0;n<x.size();++n)x[n]=float(.05*std::sin(2*pi*hz*double(n)/rate));return x;};
    const auto with=[&](const std::function<void(ChimeraProcessor&)>& edit,const std::vector<float>& in){auto p=make();set(*p,"reverbon",1);set(*p,"reverbmix",.5f);if(edit)edit(*p);return renderInput(*p,in);};
    // The level of the tone itself in the output, over its last second.
    const auto level=[&](const std::vector<float>& x,double hz){return db(magnitude(x,int(rate),int(x.size()),hz));};
    const auto low=tone(60),high=tone(4000);
    const auto plainLow=with({},low),cutLow=with([](ChimeraProcessor& p){set(p,"reverblowcut",500);},low);
    const auto plainHigh=with({},high),cutHigh=with([](ChimeraProcessor& p){set(p,"reverblowcut",500);},high);
    const double lowChange=level(cutLow,60)-level(plainLow,60),highChange=level(cutHigh,4000)-level(plainHigh,4000);
    // 4 kHz is reported, not checked: the low cut turns the reverb's phase a little there, which moves the sum with the dry
    // sound. That the cut leaves 4 kHz in the reverb itself is checked on the module (ChimeraReverbControlsDSPTests).
    require(lowChange<-2 && cutLow!=plainLow,"LOW CUT does not reach the reverb of slot 1 through the processor (60 Hz changed by "+std::to_string(lowChange)+" dB in the output, 4 kHz by "+std::to_string(highChange)+" dB)");
    // Another slot's controls do nothing to this one: bit-identical with slot 2's reverb off and its controls moved, and the other way round.
    const auto moved=with([](ChimeraProcessor& p){set(p,"slot2_reverbpredelay",150);set(p,"slot2_reverblowcut",400);set(p,"slot2_reverbtone",-1);set(p,"slot2_reverbwidth",0);set(p,"slot2_reverbfreeze",1);},low);
    require(moved==plainLow,"A switched-off slot's reverb controls changed the sound");
    // Slot 2's reverb has its own: with slot 1 off, slot 2's LOW CUT works and slot 1's does nothing.
    const auto second=[&](const std::function<void(ChimeraProcessor&)>& edit){auto p=make();set(*p,"slot2_reverbon",1);set(*p,"slot2_reverbmix",.5f);if(edit)edit(*p);return renderInput(*p,low);};
    const auto secondPlain=second({}),secondCut=second([](ChimeraProcessor& p){set(p,"slot2_reverblowcut",500);}),secondOther=second([](ChimeraProcessor& p){set(p,"reverblowcut",500);set(p,"slot3_reverblowcut",500);});
    require(level(secondCut,60)-level(secondPlain,60)<-2 && secondOther==secondPlain,"Slot 2's reverb does not have its own controls");
    // FREEZE through the processor: the reverb is still sounding five seconds after the playing stopped.
    const auto tail=[&](bool freeze){auto p=make();set(*p,"reverbon",1);set(*p,"reverbmix",.5f);std::vector<float> in(size_t(rate*7),0.f);const auto burst=tone(330);std::copy(burst.begin(),burst.begin()+long(rate),in.begin());
        const auto head=renderInput(*p,std::vector<float>(in.begin(),in.begin()+long(rate*.9)));if(freeze)set(*p,"reverbfreeze",1);
        const auto rest=renderInput(*p,std::vector<float>(in.begin()+long(rate*.9),in.end()));double e=0;for(size_t n=rest.size()-size_t(rate);n<rest.size();++n)e+=double(rest[n])*rest[n];return db(std::sqrt(e/rate));};
    const double open=tail(false),held=tail(true);
    require(held>open+30,"FREEZE does not hold the tail through the processor (frozen "+std::to_string(held)+" dB, open "+std::to_string(open)+" dB after five seconds)");
    std::cout<<"MEASURE reverb controls through the processor: LOW CUT at 500 Hz changes the tone's level in the output by "<<lowChange<<" dB (60 Hz) and "<<highChange<<" dB (4 kHz); five seconds on, the tail is at "<<held<<" dB frozen and "<<open<<" dB open\n";
    std::cout<<"PASS reverb controls audio: slot 1 and slot 2 each have their own; a switched-off slot's controls change nothing (bit-identical); freeze holds\n";
}
void reverbControlsState() {
    // Saved and loaded: every value comes back.
    auto source=make();
    const std::array<float,postReverbControlsPerSlot> values{85.f,240.f,-.4f,1.5f,1.f};
    for(int slot=0;slot<postSlotCount;++slot)for(int control=0;control<postReverbControlsPerSlot;++control)set(*source,reverbControlID(slot,control),control==4?float((slot+1)%2):values[size_t(control)]+float(slot));
    juce::MemoryBlock saved;source->getStateInformation(saved);
    auto loaded=make();loaded->setStateInformation(saved.getData(),int(saved.getSize()));
    for(int slot=0;slot<postSlotCount;++slot)for(int control=0;control<postReverbControlsPerSlot;++control)
        require(std::abs(get(*loaded,reverbControlID(slot,control))-get(*source,reverbControlID(slot,control)))<1e-4f,"A reverb control did not come back from a saved project: "+reverbControlID(slot,control).toStdString());
    // A project saved before the controls existed loads them at their defaults, over a session that had moved them.
    juce::ValueTree tree;{auto xml=source->getXmlFromBinary(saved.getData(),int(saved.getSize()));tree=juce::ValueTree::fromXml(*xml);}
    int removed=0;
    for(int i=tree.getNumChildren()-1;i>=0;--i){const auto id=tree.getChild(i).getProperty("id").toString();
        for(int slot=0;slot<postSlotCount;++slot)for(int control=0;control<postReverbControlsPerSlot;++control)if(id==reverbControlID(slot,control)){tree.removeChild(i,nullptr);++removed;slot=postSlotCount;break;}}
    require(removed==postReverbControlsParameterCount,"The saved project does not hold every reverb control");
    juce::MemoryBlock old;{auto xml=tree.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,old);}
    loaded->setStateInformation(old.getData(),int(old.getSize()));
    for(int slot=0;slot<postSlotCount;++slot)for(int control=0;control<postReverbControlsPerSlot;++control)
        require(std::abs(get(*loaded,reverbControlID(slot,control))-reverbControlSpecs[size_t(control)].initial)<1e-6f,"A project saved before the reverb controls did not load them at their defaults: "+reverbControlID(slot,control).toStdString());
    std::cout<<"PASS reverb controls state: all 20 values are saved and loaded; an older project loads them at their defaults\n";
}
// POST reverb tempo shape (docs/POST_REVERB_SHAPE.md): the bank, the audio through the processor, and state.
void reverbShape() {
    auto p=make();const int total=p->getParameters().size();int count=0;
    for(auto* parameter:p->getParameters())if(auto* with=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))if(with->getVersionHint()==postReverbShapeParameterVersion) {
        const int slot=count/postReverbShapePerSlot,control=count%postReverbShapePerSlot;
        require(with->getParameterIndex()==total-(postReverbShapeParameterCount+postReverbAfterShapeParameterCount)+count && with->paramID==reverbShapeID(slot,control) && parameter->isAutomatable() && with->getName(64)==postSlotName(slot,reverbShapeNames[size_t(control)]),"A reverb shape parameter is not in the last bank, slot 1 first, under its name");++count;}
    require(count==postReverbShapeParameterCount && count==12,"The reverb shape bank is not three parameters for each of four slots");
    for(int slot=0;slot<postSlotCount;++slot)require(get(*p,reverbShapeID(slot,0))==0 && get(*p,reverbShapeID(slot,1))==1 && juce::roundToInt(get(*p,reverbShapeID(slot,2)))==reverbShapeDefaultLength,"A reverb shape does not start Off, at full depth and one bar");
    // Through the processor at 120 BPM, one bar: the Gate lets the reverb through for a second and closes it for a second.
    const auto render=[](const std::function<void(ChimeraProcessor&)>& edit){auto q=make();set(*q,"reverbon",1);set(*q,"reverbmix",.5f);set(*q,"reverbsize",.2f);if(edit)edit(*q);
        std::vector<float> in(size_t(rate*4));for(size_t n=0;n<in.size();++n)in[n]=float(.05*std::sin(2*pi*440*double(n)/rate));return renderInput(*q,in);};
    const auto plain=render({}),gated=render([](ChimeraProcessor& q){set(q,"reverbshape",float(reverbShapeGate));});
    const auto apart=[&](double from,double to){double d=0,e=0;for(size_t n=size_t(from*rate);n<size_t(to*rate);++n){d+=double(gated[n]-plain[n])*(gated[n]-plain[n]);e+=double(plain[n])*plain[n];}return db(std::sqrt(d/std::max(e,1e-30)));};
    const double open=apart(2.2,2.8),closed=apart(3.2,3.8);
    require(open<-40 && closed>-20,"The Gate shape does not open and close the reverb through the processor (difference from the plain reverb "+std::to_string(open)+" dB in the open half, "+std::to_string(closed)+" dB in the closed half)");
    // Each slot has its own, and a switched-off slot's shape changes nothing.
    const auto other=render([](ChimeraProcessor& q){set(q,"slot2_reverbshape",float(reverbShapeGate));set(q,"slot3_reverbshapedepth",.3f);set(q,"slot4_reverbshapelength",0);});
    require(other==plain,"A switched-off slot's reverb shape changed the sound");
    // Depth 0 is the plain reverb, to within the follower's rounding.
    const auto none=render([](ChimeraProcessor& q){set(q,"reverbshape",float(reverbShapeGate));set(q,"reverbshapedepth",0);});
    {double d=0,e=0;for(size_t n=0;n<plain.size();++n){d=std::max(d,double(std::abs(none[n]-plain[n])));e=std::max(e,double(std::abs(plain[n])));}require(d<e*1e-4,"A shape at no depth changed the sound");}
    // Saved and loaded; an older project loads Off.
    auto source=make();for(int slot=0;slot<postSlotCount;++slot){set(*source,reverbShapeID(slot,0),float(1+slot));set(*source,reverbShapeID(slot,1),.2f+.1f*float(slot));set(*source,reverbShapeID(slot,2),float(slot));}
    juce::MemoryBlock saved;source->getStateInformation(saved);auto loaded=make();loaded->setStateInformation(saved.getData(),int(saved.getSize()));
    for(int slot=0;slot<postSlotCount;++slot)for(int control=0;control<postReverbShapePerSlot;++control)require(std::abs(get(*loaded,reverbShapeID(slot,control))-get(*source,reverbShapeID(slot,control)))<1e-4f,"A reverb shape value did not come back from a saved project");
    juce::ValueTree tree;{auto xml=source->getXmlFromBinary(saved.getData(),int(saved.getSize()));tree=juce::ValueTree::fromXml(*xml);}
    int removed=0;for(int i=tree.getNumChildren()-1;i>=0;--i){const auto id=tree.getChild(i).getProperty("id").toString();if(id.endsWith("reverbshape") || id.endsWith("reverbshapedepth") || id.endsWith("reverbshapelength")){tree.removeChild(i,nullptr);++removed;}}
    require(removed==postReverbShapeParameterCount,"The saved project does not hold every reverb shape value");
    juce::MemoryBlock old;{auto xml=tree.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,old);}
    loaded->setStateInformation(old.getData(),int(old.getSize()));
    for(int slot=0;slot<postSlotCount;++slot)require(get(*loaded,reverbShapeID(slot,0))==0 && get(*loaded,reverbShapeID(slot,1))==1 && juce::roundToInt(get(*loaded,reverbShapeID(slot,2)))==reverbShapeDefaultLength,"A project saved before the reverb shape did not load it Off");
    std::cout<<"MEASURE reverb shape through the processor (Gate, 120 BPM, one bar; difference from the plain reverb): "<<open<<" dB in the open half, "<<closed<<" dB in the closed half\n";
    std::cout<<"PASS reverb shape: 12 parameters at version 44, last; the Gate opens and closes the reverb through the processor; each slot its own; saved, loaded, and Off in an older project\n";
}
// More of the reverb shape (docs/POST_REVERB_SHAPE_MORE.md): sixteen parameters at version 46, last; through the processor; saved and loaded.
void reverbShapeMore() {
    auto p=make();const int total=p->getParameters().size();int count=0;
    for(auto* parameter:p->getParameters())if(auto* with=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))if(with->getVersionHint()==postReverbShapeMoreParameterVersion) {
        const int slot=count/postReverbShapeMorePerSlot,control=count%postReverbShapeMorePerSlot;
        require(with->getParameterIndex()==total-postReverbShapeMoreParameterCount+count && with->paramID==reverbShapeMoreID(slot,control) && parameter->isAutomatable() && with->getName(64)==postSlotName(slot,reverbShapeMoreNames[size_t(control)]),"One more reverb shape parameter is not in the last bank, slot 1 first, under its name");++count;}
    require(count==postReverbShapeMoreParameterCount && count==16,"More of the reverb shape is not four parameters for each of four slots");
    for(int slot=0;slot<postSlotCount;++slot)require(get(*p,reverbShapeMoreID(slot,0))==0 && get(*p,reverbShapeMoreID(slot,1))==1 && get(*p,reverbShapeMoreID(slot,2))==0 && get(*p,reverbShapeMoreID(slot,3))==0,"The reverb shape does not start in bars, one second, as drawn and without morph");
    // Through the processor: a Gate two seconds long, in seconds, opens and closes the reverb as the one-bar Gate at 120 BPM does.
    const auto render=[](const std::function<void(ChimeraProcessor&)>& edit){auto q=make();set(*q,"reverbon",1);set(*q,"reverbmix",.5f);set(*q,"reverbsize",.2f);if(edit)edit(*q);
        std::vector<float> in(size_t(rate*4));for(size_t n=0;n<in.size();++n)in[n]=float(.05*std::sin(2*pi*440*double(n)/rate));return renderInput(*q,in);};
    const auto plain=render({}),bars=render([](ChimeraProcessor& q){set(q,"reverbshape",float(reverbShapeGate));}),
        seconds=render([](ChimeraProcessor& q){set(q,"reverbshape",float(reverbShapeGate));set(q,"reverbshapeunit",1);set(q,"reverbshapeseconds",2);set(q,"reverbshapelength",0);});
    require(seconds==bars,"A shape two seconds long is not the one-bar shape at 120 BPM through the processor");
    const auto shorter=render([](ChimeraProcessor& q){set(q,"reverbshape",float(reverbShapeGate));set(q,"reverbshapeunit",1);set(q,"reverbshapeseconds",.5f);});
    require(shorter!=bars,"The shape's length in seconds does nothing through the processor");
    const auto curved=render([](ChimeraProcessor& q){set(q,"reverbshape",float(reverbShapeSwell));set(q,"reverbshapecurve",1);}),drawn=render([](ChimeraProcessor& q){set(q,"reverbshape",float(reverbShapeSwell));}),
        morphed=render([](ChimeraProcessor& q){set(q,"reverbshape",float(reverbShapeSwell));set(q,"reverbshapemorph",1);});
    require(curved!=drawn && morphed!=drawn,"CURVE or MORPH does nothing through the processor");
    // With the shape Off, and in a switched-off slot, they change nothing.
    const auto off=render([](ChimeraProcessor& q){set(q,"reverbshapeunit",1);set(q,"reverbshapeseconds",.3f);set(q,"reverbshapecurve",1);set(q,"reverbshapemorph",-1);
        set(q,"slot2_reverbshapeunit",1);set(q,"slot3_reverbshapecurve",-1);set(q,"slot4_reverbshapemorph",1);});
    require(off==plain,"More of the reverb shape changed the sound with the shape Off");
    // Saved and loaded; an older project loads the defaults.
    auto source=make();for(int slot=0;slot<postSlotCount;++slot){set(*source,reverbShapeMoreID(slot,0),float(slot%2));set(*source,reverbShapeMoreID(slot,1),.5f+float(slot));set(*source,reverbShapeMoreID(slot,2),-.5f+.3f*float(slot));set(*source,reverbShapeMoreID(slot,3),.8f-.4f*float(slot));}
    juce::MemoryBlock saved;source->getStateInformation(saved);auto loaded=make();loaded->setStateInformation(saved.getData(),int(saved.getSize()));
    for(int slot=0;slot<postSlotCount;++slot)for(int control=0;control<postReverbShapeMorePerSlot;++control)require(std::abs(get(*loaded,reverbShapeMoreID(slot,control))-get(*source,reverbShapeMoreID(slot,control)))<1e-3f,"One more reverb shape value did not come back from a saved project");
    juce::ValueTree tree;{auto xml=source->getXmlFromBinary(saved.getData(),int(saved.getSize()));tree=juce::ValueTree::fromXml(*xml);}
    int removed=0;for(int i=tree.getNumChildren()-1;i>=0;--i){const auto id=tree.getChild(i).getProperty("id").toString();if(id.contains("reverbshapeunit") || id.contains("reverbshapeseconds") || id.contains("reverbshapecurve") || id.contains("reverbshapemorph")){tree.removeChild(i,nullptr);++removed;}}
    require(removed==postReverbShapeMoreParameterCount,"The saved project does not hold every one of the added reverb shape values");
    juce::MemoryBlock old;{auto xml=tree.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,old);}
    loaded->setStateInformation(old.getData(),int(old.getSize()));
    for(int slot=0;slot<postSlotCount;++slot)require(get(*loaded,reverbShapeMoreID(slot,0))==0 && get(*loaded,reverbShapeMoreID(slot,1))==1 && get(*loaded,reverbShapeMoreID(slot,2))==0 && get(*loaded,reverbShapeMoreID(slot,3))==0,"A project saved before these controls did not load them at their defaults");
    std::cout<<"PASS more of the reverb shape: 16 parameters at version 46, last; seconds, CURVE and MORPH act through the processor and not with the shape Off; each slot its own; saved, loaded, and defaults in an older project\n";
}
// DECAY (docs/POST_REVERB_DECAY.md): four parameters at version 45, after the shape; through the processor; saved and loaded.
void reverbDecay() {
    auto p=make();const int total=p->getParameters().size();int count=0;
    for(auto* parameter:p->getParameters())if(auto* with=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))if(with->getVersionHint()==postReverbDecayParameterVersion) {
        require(with->getParameterIndex()==total-postReverbAfterShapeParameterCount+count && with->paramID==reverbDecayID(count) && parameter->isAutomatable() && with->getName(64)==postSlotName(count,"Reverb decay"),"A reverb decay parameter is not first after the reverb shape, slot 1 first, under its name");++count;}
    require(count==postReverbDecayParameterCount && count==4,"The reverb decay bank is not one parameter for each of four slots");
    for(int slot=0;slot<postSlotCount;++slot)require(get(*p,reverbDecayID(slot))==0,"A reverb decay does not start with SIZE setting it");
    {auto* q=p->parameters().getParameter("reverbdecay");require(q->getText(q->convertTo0to1(0),32)=="SIZE" && q->getText(q->convertTo0to1(2.5f),32)=="2.50 s" && std::abs(q->convertFrom0to1(q->getValueForText("SIZE")))<1e-6f && std::abs(q->convertFrom0to1(q->getValueForText("3.5 s"))-3.5f)<1e-3f,"The reverb decay does not read as SIZE or as seconds");}
    // Through the processor: a burst of tone, then silence. A long DECAY leaves more a second later than SIZE alone; a short one less.
    const auto render=[](const std::function<void(ChimeraProcessor&)>& edit){auto q=make();set(*q,"reverbon",1);set(*q,"reverbmix",.5f);set(*q,"reverbsize",.5f);if(edit)edit(*q);
        std::vector<float> in(size_t(rate*4),0.f);for(size_t n=size_t(rate);n<size_t(rate*1.5);++n)in[n]=float(.05*std::sin(2*pi*220*double(n)/rate));return renderInput(*q,in);};
    const auto level=[](const std::vector<float>& x){double e=0;for(size_t n=size_t(rate*2.5);n<size_t(rate*3);++n)e+=double(x[n])*x[n];return db(std::sqrt(e/(rate*.5)));};
    const auto plain=render({}),longer=render([](ChimeraProcessor& q){set(q,"reverbdecay",12);}),shorter=render([](ChimeraProcessor& q){set(q,"reverbdecay",.3f);});
    require(level(longer)>level(plain)+6 && level(shorter)<level(plain)-6,"DECAY does not lengthen and shorten the reverb through the processor (a second after the note: SIZE alone "+std::to_string(level(plain))+" dB, 12 s "+std::to_string(level(longer))+" dB, 0.3 s "+std::to_string(level(shorter))+" dB)");
    // Each slot has its own, and a switched-off slot's decay changes nothing.
    const auto other=render([](ChimeraProcessor& q){set(q,"slot2_reverbdecay",5);set(q,"slot3_reverbdecay",.5f);set(q,"slot4_reverbdecay",20);});
    require(other==plain,"A switched-off slot's reverb decay changed the sound");
    // Saved and loaded; an older project loads with SIZE setting the decay.
    auto source=make();for(int slot=0;slot<postSlotCount;++slot)set(*source,reverbDecayID(slot),1.5f+float(slot));
    juce::MemoryBlock saved;source->getStateInformation(saved);auto loaded=make();loaded->setStateInformation(saved.getData(),int(saved.getSize()));
    for(int slot=0;slot<postSlotCount;++slot)require(std::abs(get(*loaded,reverbDecayID(slot))-get(*source,reverbDecayID(slot)))<1e-3f,"A reverb decay did not come back from a saved project");
    juce::ValueTree tree;{auto xml=source->getXmlFromBinary(saved.getData(),int(saved.getSize()));tree=juce::ValueTree::fromXml(*xml);}
    int removed=0;for(int i=tree.getNumChildren()-1;i>=0;--i){const auto id=tree.getChild(i).getProperty("id").toString();if(id.contains("reverbdecay")){tree.removeChild(i,nullptr);++removed;}}
    require(removed==postReverbDecayParameterCount,"The saved project does not hold every reverb decay");
    juce::MemoryBlock old;{auto xml=tree.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,old);}
    loaded->setStateInformation(old.getData(),int(old.getSize()));
    for(int slot=0;slot<postSlotCount;++slot)require(get(*loaded,reverbDecayID(slot))==0,"A project saved before the reverb decay did not load with SIZE setting it");
    std::cout<<"MEASURE reverb decay through the processor (a second after the note): SIZE alone "<<level(plain)<<" dB, 12 s "<<level(longer)<<" dB, 0.3 s "<<level(shorter)<<" dB\n";
    std::cout<<"PASS reverb decay: 4 parameters at version 45, after the shape; it lengthens and shortens the reverb through the processor; each slot its own; saved, loaded, and SIZE in an older project\n";
}
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {model();toneRegion();postRegion();clickSafe();recallAndUndo();staleInput();pathAndCommands();dragging();postPageDrag();keyDelay();
        secondSlotsBank();secondSlotsOldState();secondSlotsIndependent();secondSlotsBypass();secondSlotsLatency();secondSlotsOrder();secondSlotsState();secondSlotsPage();
        extraSlotsBank();extraSlotsAudio();extraSlotsOldState();extraSlotsAdd();extraSlotsPage();
        reverbControlsBank();reverbControlsAudio();reverbControlsState();reverbShape();reverbDecay();reverbShapeMore();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
