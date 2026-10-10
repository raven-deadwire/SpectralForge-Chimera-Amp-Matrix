#pragma once
#include "NativeUITests.h"
#include "CabRoom.h"
#include "GuitarSignaturePresets.h"
namespace signalPathTests {
using namespace nativeUITests;
using namespace spectralforge::signalPath;
inline bool edge(const Snapshot& s,const juce::String& a,const juce::String& b) {for(const auto& e:s.edges)if(e.from==a&&e.to==b)return true;return false;}
inline std::vector<float> values(ChimeraProcessor& p){std::vector<float> result;for(auto* parameter:p.getParameters())result.push_back(parameter->getValue());return result;}
struct Writes final:juce::AudioProcessorListener {
    int changes{},gestures{};
    void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {++changes;}
    void audioProcessorChanged(juce::AudioProcessor*,const juce::AudioProcessorListener::ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int) override {++gestures;}
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int) override {++gestures;}
};
inline void topology(ChimeraProcessor& p) {
    for(int mode=0;mode<3;++mode)for(int dual=0;dual<2;++dual)for(int gate=0;gate<2;++gate){set(p,"mode",float(mode));set(p,"dualtype",float(dual));set(p,"gateAfterRig",float(gate));auto s=read(p);
        int amps=0,cabs=0;for(const auto& n:s.nodes){amps+=n.id.startsWith("amp.context");cabs+=n.target==Target::cab;}
        require(amps==mode+1&&cabs==mode+1,"Path lane count differs from DSP");
        require(edge(s,"post.section0","post.section1")&&edge(s,"post.section4","post.section5")&&edge(s,"post.section5","utilities")&&edge(s,"utilities","final.eq")&&edge(s,"final.eq","output"),"POST / Final EQ order differs from DSP");
        require(edge(s,gate?"gate":"merge","tone.eq")&&edge(s,"tone.eq","post.section0")&&!edge(s,"merge","post.section0")&&!edge(s,"utilities","output"),"Tone / Final EQ must occupy the actual global DSP positions");
        require(s.find("tone.eq")->target==Target::toneEQ && s.find("final.eq")->target==Target::finalEQ,"EQ editor target mismatch");
        for(size_t i=0;i<s.nodes.size();++i)for(size_t j=i+1;j<s.nodes.size();++j)require(!s.nodes[i].bounds.intersects(s.nodes[j].bounds),"Expanded path blocks overlap");
        if(mode==2){require(edge(s,"low.comp","low.di")&&edge(s,"low.comp","amp.context3")&&edge(s,"amp.context3","cab.context3")&&edge(s,"cab.context3","low.blend")&&edge(s,"low.di","low.blend")&&edge(s,"low.blend","level.context3"),"Matrix LOW branch/blend order differs from DSP");require(!edge(s,"split","amp.context3"),"Wet PRE falsely feeds LOW");}
    }
    set(p,"mode",2);set(p,"boardLowTap",0);auto s=read(p);require(edge(s,"transpose","clean"),"Tap zero is not before PRE");
    const auto board=p.pedalBoardState();set(p,"boardLowTap",5);s=read(p);require(edge(s,"pre.owner"+juce::String(board.order[4]),"clean"),"Tap five is not after PRE");
    set(p,"boardEnabled",0);set(p,"preorder",0);set(p,"gainorder",1);s=read(p);require(edge(s,"pre.legacy3","pre.legacy4")&&edge(s,"pre.legacy4","clean")&&edge(s,"pre.legacy0","pre.legacy6"),"Legacy PRE order/tap differs from DSP");
    set(p,"gateAfterRig",1);s=read(p);require(edge(s,"merge","gate")&&edge(s,"gate","tone.eq")&&edge(s,"input","gate"),"Post gate/detector displayed at wrong point");
    set(p,"ampon1",0);set(p,"lowampmix",.83f);s=read(p);require(s.find("low.blend")->status=="100 : 0"&&s.find("amp.context3")->active,"LOW AMP switch must control blend, not bypass its internal processor");
    set(p,"solo2",1);s=read(p);require(!s.find("level.context3")->active&&s.find("level.context4")->active,"Solo audibility is stale");
    p.loadFactoryPreset(0);
}
inline void click(juce::Button& button) {
    // Send pointer events to the actual native component (not an onClick call).
    button.getTopLevelComponent()->toFront(true);
    const auto centre=button.getLocalBounds().getCentre().toFloat();juce::Desktop::setMousePosition(button.localPointToGlobal(centre).roundToInt());settle(40);
    const auto time=juce::Time::getCurrentTime();
    const auto event=[&](bool down){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),centre,juce::ModifierKeys(down?juce::ModifierKeys::leftButtonModifier:0),1,0,0,0,0,&button,&button,time,centre,time,1,false);};
    static_cast<juce::Component&>(button).mouseDown(event(true));static_cast<juce::Component&>(button).mouseUp(event(false));settle(80);
}
inline void capture(juce::Component& root,const juce::File& folder,const juce::String& name) {
    // FileOutputStream opens existing files at EOF; never append a new PNG to
    // an earlier candidate's image or leave the first PNG as stale evidence.
    const auto file=folder.getChildFile(name+".png");
    require(!file.existsAsFile() || file.deleteFile(),"Cannot replace Signal Path snapshot");
    snapshot(root,folder,name.toRawUTF8());
}
inline void run(const juce::File& folder) {
    auto p=std::make_unique<ChimeraProcessor>();topology(*p);
    Writes writes;p->addListener(&writes);
    auto editor=std::make_unique<ChimeraEditor>(*p);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);
    auto* strip=find<SignalPathView>(*editor,"signalPathStrip");require(strip!=nullptr,"Persistent path strip absent");
    for(const auto scenario:std::array<std::array<int,2>,4>{{{0,0},{1,0},{1,1},{2,0}}}) {
        std::cout<<"Signal Path scenario "<<scenario[0]<<" / "<<scenario[1]<<std::endl;
        const int mode=scenario[0];set(*p,"mode",float(mode));set(*p,"dualtype",float(scenario[1]));settle(100);
        auto* expand=find<juce::TextButton>(*editor,"signalPathExpand");require(expand,"Path expansion missing");
        const auto before=values(*p);writes.changes=writes.gestures=0;click(*expand);
        auto graphWindow=juce::Component::SafePointer<juce::DialogWindow>(dialog("signalPathGraph"));require(graphWindow,"Path expansion did not open native graph");
        auto* graph=dynamic_cast<SignalPathView*>(graphWindow->getContentComponent());require(graph,"Graph content missing");
        capture(*graph,folder,("signal-path-mode-"+juce::String(mode)+"-dual-"+juce::String(scenario[1])).toRawUTF8());
        const auto nodes=graph->snapshot().nodes;
        for(const auto& node:nodes){auto* b=find<juce::TextButton>(*graph,"path."+node.id);require(b,"Navigation block missing");click(*b);if(!b->getToggleState())throw std::runtime_error("Native pointer click did not select block "+node.id.toStdString());
            if(node.target==Target::cab)require(dialog("cabWorkspace"),"CAB click did not open the existing editor");
            if(node.target==Target::toneEQ || node.target==Target::finalEQ){
                const bool tone=node.target==Target::toneEQ;
                auto* selected=find<spectralforge::GraphicalEQPanel>(*editor,tone?"toneEQ_panel":"finalEQ_panel");
                auto* other=find<spectralforge::GraphicalEQPanel>(*editor,tone?"finalEQ_panel":"toneEQ_panel");
                require(selected && other && selected->isShowing() && selected->isNavigationSelected() && !other->isNavigationSelected(),"Expanded EQ click did not open/highlight the requested bank");
            }
            if(node.target==Target::amp && node.lane>=0)require(dialog("ampNativePanel"+juce::String(node.lane+1)),"AMP click did not open the existing lane editor");
            // Amp and CAB dialogs are independently owned and closed here.
            if(auto* w=dialog("signalPathTuner"))close(w);
            if(auto* w=dialog("ampNativePanel"+juce::String(node.lane+1)))close(w);
            if(auto* w=dialog("cabWorkspace")){auto* workspace=dynamic_cast<CabWorkspace*>(w->getContentComponent());require(workspace && workspace->focusedRig()==node.lane,"CAB navigation opened wrong rig");close(w);}
        }
        for(float scale:{.75f,1.f,1.25f,1.5f}){
            editor->setSize(juce::roundToInt(1180*scale),juce::roundToInt(780*scale));
            for(const auto* id:{"pre","post","rigs","tone.eq","final.eq"}){
                auto* button=find<juce::TextButton>(*strip,"path."+juce::String(id));require(button,"Compact navigation missing");click(*button);
                const auto rect=editor->getLocalArea(strip,strip->getLocalBounds());require(editor->getLocalBounds().contains(rect),"Strip clipped at UI scale");
                for(auto* sibling:strip->getParentComponent()->getChildren())if(sibling!=strip && sibling->isVisible())require(!rect.intersects(editor->getLocalArea(sibling,sibling->getLocalBounds())),"Strip overlaps visible editor content");
                for(auto* child:strip->getChildren())inside(*strip,*child,"Compact path button clipped");
                for(int e=0;e<2;++e){auto* panel=find<spectralforge::GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");
                    if(panel->isShowing()){inside(*editor,*panel,"EQ panel clipped at UI scale");for(int band=0;band<12;++band)require(panel->getLocalBounds().toFloat().contains(panel->nodePosition(band)),"EQ node clipped");}
                }
            }
            capture(*editor,folder,"integrated-eq-mode-"+juce::String(mode)+"-dual-"+juce::String(scenario[1])+"-scale-"+juce::String(juce::roundToInt(scale*100)));
        }
        auto* pre=find<juce::TextButton>(*graph,"path."+nodes[3].id);
        require(pre && pre->getWantsKeyboardFocus(),"Path keyboard target absent");
        pre->grabKeyboardFocus();require(static_cast<juce::Component&>(*pre).keyPressed(juce::KeyPress(juce::KeyPress::returnKey)),"Path Return key was not handled");settle(100);
        require(before==values(*p)&&writes.changes==0&&writes.gestures==0,"Navigation/open/resize emitted parameter writes or host gestures");
        capture(*editor,folder,("signal-path-editor-"+juce::String(mode)+"-dual-"+juce::String(scenario[1])).toRawUTF8());graphWindow->closeButtonPressed();settle(80);require(!graphWindow,"Non-modal graph did not close");
    }
    for(int preset=0;preset<spectralforge::selectablePresetCount;++preset){
        std::cout<<"Signal Path preset "<<preset<<std::endl;
        p->loadFactoryPreset(preset);settle(50);const auto expected=values(*p);writes.changes=writes.gestures=0;
        auto* pre=find<juce::TextButton>(*strip,"path.pre");click(*pre);
        require(expected==values(*p)&&writes.changes==0&&writes.gestures==0,"Preset navigation changed audio content");
        require(strip->snapshot().mode==int(p->parameters().getRawParameterValue("mode")->load()),"Preset mode/path failed to converge");
        require(!strip->snapshot().find("tone.eq")->active && !strip->snapshot().find("final.eq")->active,"Factory recall left EQ path enabled");
    }
    // Host changes converge through a presentation read, including inactive tabs.
    set(*p,"mode",2);set(*p,"cab2",0);set(*p,"gateAfterRig",1);set(*p,"transposeon",1);set(*p,"transpose",-2);settle(120);
    require(!strip->snapshot().find("cab.context4")->active && strip->snapshot().find("transpose")->active&&edge(strip->snapshot(),"merge","gate"),"Host automation did not refresh path/bypass");
    {SignalPathView automated(*p,false);capture(automated,folder,"signal-path-matrix-post-gate");}
    set(*p,"toneEQ_bypass",0);set(*p,"toneEQ_b1_enabled",1);set(*p,"finalEQ_bypass",1);settle(100);
    require(strip->snapshot().find("tone.eq")->active && strip->snapshot().find("tone.eq")->status=="ACTIVE / 1 BANDS" && !strip->snapshot().find("final.eq")->active,"Automated EQ bypass/band status did not converge");
    p->copyComparison();set(*p,"cab2",1);set(*p,"toneEQ_bypass",1);set(*p,"finalEQ_bypass",0);p->selectComparison(1);settle(100);require(!strip->snapshot().find("cab.context4")->active && strip->snapshot().find("tone.eq")->active && !strip->snapshot().find("final.eq")->active,"A/B did not restore graph CAB/EQ bypass");
    juce::MemoryBlock saved;p->getStateInformation(saved);p->loadFactoryPreset(0);p->setStateInformation(saved.getData(),int(saved.getSize()));settle(120);require(strip->snapshot().mode==2&&!strip->snapshot().find("cab.context4")->active && strip->snapshot().find("tone.eq")->active && !strip->snapshot().find("final.eq")->active,"Project restore left stale graph");
    auto expected=values(*p);writes.changes=writes.gestures=0;editor->removeFromDesktop();editor.reset();editor=std::make_unique<ChimeraEditor>(*p);settle(80);require(expected==values(*p)&&writes.changes==0&&writes.gestures==0,"Editor recreation changed sound");
    p->removeListener(&writes);editor.reset();
    std::cout<<"PASS Signal Path: Classic / Dual Blend & Crossover / Matrix LOW topology; native pointer navigation; host-write/gesture isolation; scales; automation; preset/project/A-B restore; editor recreation\n";
}
}
