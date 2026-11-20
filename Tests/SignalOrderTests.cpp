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
    auto p=make();int count=0,second=0;
    for(auto* raw:p->getParameters())if(auto* q=dynamic_cast<juce::RangedAudioParameter*>(raw))if(q->getParameterID().startsWith("order")){
        const bool appended=q->getParameterID().startsWith("orderPost") && q->getParameterID().substring(9).getIntValue()>postModuleCount;
        ++count;second+=appended;
        require(!q->isAutomatable() && q->getVersionHint()==(appended?postSecondSlotParameterVersion:signalOrderParameterVersion),"Order slot automatable or wrong version");}
    require(count==signalOrderParameterCount+postSecondSlotOrderParameterCount && second==postBlockCount-postModuleCount && p->requestedOrder()==SignalOrder{},"Order bank size or default wrong");
    require(PostOrder::initial().slots[size_t(postModuleCount)]==postBlock(postDynamicsModule,1) && PostOrder::initial().position(postBlock(postReverbModule,1))==postBlockCount-1,"Second slots do not follow the released order");
    std::cout<<"MEASURE model: "<<count<<" slot parameters (6 tone / dynamics, 14 POST), versions "<<signalOrderParameterVersion<<" / "<<postSecondSlotParameterVersion<<", not automatable\n";
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
            && edge("post.section4","post.creative") && edge("post.creative","post2.section0") && edge("post2.section5","utilities"),"Signal path does not follow the saved order");
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
std::map<juce::String,float> secondSlotValues(ChimeraProcessor& p){
    std::map<juce::String,float> v;
    for(auto* raw:p.getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(raw))if(r->getParameterID().startsWith("slot2_"))v[r->getParameterID()]=p.parameters().getRawParameterValue(r->getParameterID())->load();
    return v;
}
bool secondSlotsAtDefault(ChimeraProcessor& p){
    for(auto* raw:p.getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(raw))if(r->getParameterID().startsWith("slot2_") && std::abs(r->getValue()-r->getDefaultValue())>1.e-6f)return false;
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
    for(auto* raw:p->getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(raw))if(r->getParameterID().startsWith("slot2_")){
        auto* original=p->parameters().getParameter(r->getParameterID().substring(6));require(original!=nullptr,"Second-slot parameter without a slot 1 original: "+r->getParameterID().toStdString());
        require(r->getVersionHint()==postSecondSlotParameterVersion && r->getDefaultValue()==original->getDefaultValue() && r->isAutomatable()==original->isAutomatable()
                && r->getNormalisableRange().start==original->getNormalisableRange().start && r->getNormalisableRange().end==original->getNormalisableRange().end,"Second-slot definition differs from slot 1: "+r->getParameterID().toStdString());
        ++copies;}
    require(copies+postSecondSlotOrderParameterCount==postSecondSlotParameterCount(),"Second-slot bank size");
    // Automation of released parameters still targets slot 1: same IDs, same indices.
    auto* released=p->parameters().getParameter("delaytime");require(released!=nullptr && released->getParameterIndex()<p->getParameters().size()-postSecondSlotParameterCount(),"A released ID moved");
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
        if(id.startsWith("slot2_") || (id.startsWith("orderPost") && id.substring(9).getIntValue()>postModuleCount)){tree.removeChild(i,nullptr);++removed;}}
    // Released order slots store only blocks 0-6, as 1.4 wrote them.
    for(int i=0;i<postModuleCount;++i)require(int(tree.getChildWithProperty("id",orderSlotID(OrderRegion::post,i)).getProperty("value"))<postModuleCount,"Fixture holds a second-slot block");
    juce::MemoryBlock old;{auto xml=tree.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,old);}
    // Load it into a session whose second slots are on and moved.
    auto p=make();set(*p,s2("delayon"),1);set(*p,s2("delaytime"),90);set(*p,s2("creativeOn"),1);p->placeModule(OrderRegion::post,postBlock(postDelayModule,1),0);
    p->setStateInformation(old.getData(),int(old.getSize()));
    PostOrder expected=PostOrder::initial().placed(postReverbModule,0);
    require(secondSlotsAtDefault(*p) && p->requestedOrder().post==expected,"An old project does not load with slot 2 off and the released order (second slots after it)");
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
    require(!p->moveModule(OrderRegion::post,postBlock(postReverbModule,1),1) && p->placeModule(OrderRegion::post,postDynamicsModule,postBlockCount-1) && p->requestedOrder().post.slots[size_t(postBlockCount-1)]==postDynamicsModule,"Edges of the 14-block order");
    // Stale slots: a duplicate keeps the last valid order.
    const auto valid=p->requestedOrder();run(*p,2);
    set(*p,orderSlotID(OrderRegion::post,postBlockCount-1),float(delay2));
    SignalOrderParameters reader;reader.bind(p->parameters());require(reader.read(valid).post==valid.post && p->requestedOrder().post==PostOrder::initial(),"Stale 14-block order not handled");
    run(*p,2);
    // Presets order the sections; the second slots follow in the same order.
    auto q=make();require(q->applyOrderPreset(OrderRegion::post,1) && q->requestedOrder().post.slots[0]==postEQModule && q->requestedOrder().post.slots[size_t(postModuleCount)]==postBlock(postEQModule,1),"Order preset with second slots");
    // The routing graph: 14 POST blocks, the second ones labelled.
    auto s=signalPath::read(*q);const auto* node=s.find("post2.section4");
    require(node!=nullptr && node->label=="DELAY 2" && node->status=="BYPASSED" && node->slot==1 && !node->active,"DELAY 2 missing from the routing graph");
    int postNodes=0;for(const auto& n:s.nodes)postNodes+=n.target==signalPath::Target::post;
    for(size_t i=0;i<s.nodes.size();++i)for(size_t j=i+1;j<s.nodes.size();++j)require(!s.nodes[i].bounds.intersects(s.nodes[j].bounds),"Path blocks overlap with 14 POST blocks");
    for(const auto& n:s.nodes)require(n.bounds.getBottom()<=700,"A block falls outside the routing graph");
    set(*q,s2("delayon"),1);s=signalPath::read(*q);require(s.find("post2.section4")->active && s.find("post2.section4")->status!="BYPASSED","DELAY 2 status does not follow its switch");
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

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {model();toneRegion();postRegion();clickSafe();recallAndUndo();staleInput();pathAndCommands();dragging();postPageDrag();keyDelay();
        secondSlotsBank();secondSlotsOldState();secondSlotsIndependent();secondSlotsBypass();secondSlotsLatency();secondSlotsOrder();secondSlotsState();secondSlotsPage();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}
