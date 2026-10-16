#pragma once
#include "NativeUITests.h"
#include "CabRoom.h"
#include "GuitarSignaturePresets.h"
#include "CabLowBlendAudioTests.h"
namespace signalPathTests {
using namespace nativeUITests;
using namespace spectralforge::signalPath;
inline bool edge(const Snapshot& s,const juce::String& a,const juce::String& b) {for(const auto& e:s.edges)if(e.from==a&&e.to==b)return true;return false;}
// A stale presentation remains a test failure; report its state before dereferencing.
inline const Node& requiredNode(const Snapshot& snapshot,const char* id,const char* phase) {
    const auto* node=snapshot.find(id);
    if(node==nullptr)
        throw std::runtime_error((juce::String("Signal Path ")+phase+": missing "+id
            +" (snapshot mode="+juce::String(snapshot.mode)+", nodes="+juce::String(int(snapshot.nodes.size()))+")").toStdString());
    return *node;
}
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
        require(edge(s,gate?"gate":"merge","tone.eq")&&edge(s,"tone.eq","common.comp")&&edge(s,"common.comp","post.section0")&&edge(s,"output","output.limiter")&&!edge(s,"merge","post.section0")&&!edge(s,"utilities","output"),"Tone / common dynamics / Final EQ and last limiter must occupy the actual global DSP positions");
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
    // JUCE's PNG callback does not propagate file-stream write failures. Encode
    // in memory, then verify a staged file before replacing earlier evidence.
    const auto file=folder.getChildFile(name+".png");
    const auto pixels=root.createComponentSnapshot(root.getLocalBounds());
    juce::MemoryOutputStream encoded;juce::PNGImageFormat png;
    require(pixels.isValid() && png.writeImageToStream(pixels,encoded) && encoded.getDataSize()>0,
            "Signal Path screenshot encoding failed");
    const auto verify=[&](const juce::File& candidate) {
        juce::MemoryBlock bytes;
        require(candidate.loadFileAsData(bytes) && bytes.matches(encoded.getData(),encoded.getDataSize()),
                "Signal Path screenshot file differs from its complete encoded image");
        const auto decoded=juce::ImageFileFormat::loadFrom(bytes.getData(),bytes.getSize());
        require(decoded.isValid() && decoded.getWidth()==pixels.getWidth() && decoded.getHeight()==pixels.getHeight(),
                "Signal Path screenshot is not a complete decodable image");
    };
    juce::TemporaryFile staged(file,juce::TemporaryFile::useHiddenFile);
    require(staged.getFile().replaceWithData(encoded.getData(),encoded.getDataSize()),
            "Signal Path screenshot staging failed");
    verify(staged.getFile());
    require(staged.overwriteTargetFileWithTemporary(),"Cannot replace Signal Path snapshot");
    verify(file);
}
inline constexpr std::array<const char*,6> compactStages{"pre","head","cab","tone.eq","post","final.eq"};
inline juce::TextButton& pathButton(SignalPathView& view,const juce::String& id) {
    auto* button=find<juce::TextButton>(view,"path."+id);
    require(button,"Signal path navigation button missing");return *button;
}
inline void accessibleButton(juce::TextButton& button,const juce::String& label) {
    auto* handler=button.getAccessibilityHandler();
    require(button.isAccessible() && handler && handler->getRole()==juce::AccessibilityRole::button,
            "Signal path button has no accessible button role");
    require(handler->getTitle().containsIgnoreCase(label) && handler->getActions().contains(juce::AccessibilityActionType::press),
            "Signal path accessible label or press action is absent");
}
inline void compactContract(SignalPathView& strip,int mode) {
    int count=0;for(auto* child:strip.getChildren())if(child->getComponentID().startsWith("path."))++count;
    require(count==int(compactStages.size()),"Compact workflow must contain exactly six editing stages");
    require(strip.getHeight()>=44,"Primary navigation hit area is too small");
    int previousRight=0;
    for(size_t i=0;i<compactStages.size();++i) {
        auto& button=pathButton(strip,compactStages[i]);
        require(button.isVisible() && button.isEnabled() && button.getWantsKeyboardFocus(),"Compact editing stage is inaccessible");
        require(button.getExplicitFocusOrder()==int(i)+1 && button.getX()>=previousRight,
                "Compact workflow visual or keyboard order changed");
        previousRight=button.getRight();inside(strip,button,"Compact workflow stage is clipped");
        accessibleButton(button,button.getButtonText());
    }
    for(const auto* id:{"input","gate","transpose","tuner","utilities","output","rigs"})
        require(find(strip,"path."+juce::String(id))==nullptr,"Utility or aggregate rig block duplicated in compact workflow");
    require(pathButton(strip,"head").getButtonText()=="HEAD" && pathButton(strip,"cab").getButtonText()=="CAB"
            && strip.snapshot().mode==mode,"Separate HEAD/CAB stages did not retain the restored routing mode");
    for(const auto* id:{"head","cab"})
        require(pathButton(strip,id).getAccessibilityHandler()->getTitle().contains(mode==0?"CLASSIC":mode==1?"DUAL":"MATRIX"),
                "HEAD/CAB accessible names lost the current routing-mode context");
    auto* expand=find<juce::TextButton>(strip,"signalPathExpand");
    require(expand && expand->getButtonText()=="ROUTING" && expand->getExplicitFocusOrder()==7
            && expand->isVisible() && expand->isEnabled() && expand->getWantsKeyboardFocus(),
            "Actual routing inspection is not the final accessible workflow action");
    inside(strip,*expand,"Routing inspection button is clipped");accessibleButton(*expand,"routing");
}
template<class T> inline T* named(juce::Component& root,const juce::String& name) {
    if(auto* typed=dynamic_cast<T*>(&root);typed && typed->getName()==name)return typed;
    for(auto* child:root.getChildren())if(auto* result=named<T>(*child,name))return result;
    return nullptr;
}
inline void globalUtilities(juce::Component& editor,SignalPathView& strip) {
    // Existing global controls are located outside the workflow, including the
    // persistent tuner action in the footer. Merely inspecting them cannot mute.
    for(const auto* text:{"GATE","TRANSPOSE","TUNER"}) {
        int count=0;
        const auto visit=[&](auto&& self,juce::Component& root)->void {
            if(auto* button=dynamic_cast<juce::TextButton*>(&root);button && (button->getButtonText()==text || (juce::String(text)=="TUNER" && button->getComponentID()=="tunerOpen"))) {
                require(button->isShowing() && button->isEnabled() && !strip.isParentOf(button),
                        "Existing global utility control is hidden or duplicated in workflow");++count;
            }
            for(auto* child:root.getChildren())self(self,*child);
        };
        visit(visit,editor);require(count==1,"Existing global utility action was removed or duplicated");
    }
    for(const auto* name:{"Gate threshold","Gate release","Gate hold","Gate range","Transpose semitones"}) {
        auto* slider=named<juce::Slider>(editor,name);
        require(slider && slider->isShowing() && slider->isEnabled(),"Existing global utility slider is unavailable");
    }
}
inline void key(juce::Component& owner,const juce::KeyPress& press) {
    auto* peer=owner.getPeer();require(peer,"Signal path keyboard test has no native peer");
    require(peer->handleKeyPress(press),"Native signal path key was not handled");settle(20);
}
inline void modifiedSpaceIgnored(juce::TextButton& button,juce::Component& other) {
    struct Clicks final:juce::Button::Listener {
        explicit Clicks(juce::Button& target):button(target){button.addListener(this);}
        ~Clicks() override {button.removeListener(this);}
        void buttonClicked(juce::Button*) override {++count;}
        juce::Button& button;int count{};
    } clicks(button);
    button.grabKeyboardFocus();
    // Dispatch directly to the button: modified Space belongs to the host or
    // enclosing editor, so this check must not invoke those unrelated actions.
    for(const int modifier:{juce::ModifierKeys::shiftModifier,juce::ModifierKeys::ctrlModifier,juce::ModifierKeys::altModifier,juce::ModifierKeys::commandModifier})
        require(!static_cast<juce::Component&>(button).keyPressed(juce::KeyPress(juce::KeyPress::spaceKey,modifier,0)),
                "Navigation consumed a modified Space shortcut");
    other.grabKeyboardFocus();
    require(!static_cast<juce::Component&>(button).keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)),
            "An unfocused navigation button consumed Space");
    settle(35);require(clicks.count==0,"Modified or unfocused Space activated a navigation action");
}
inline void keyboardOrder(SignalPathView& view,const std::vector<juce::Component*>& expected) {
    require(!expected.empty(),"No workflow keyboard targets");
    view.getTopLevelComponent()->toFront(true);expected.front()->grabKeyboardFocus();settle(35);
    require(expected.front()->hasKeyboardFocus(false),"Cannot focus the first workflow action");
    for(size_t i=1;i<expected.size();++i) {
        key(view,juce::KeyPress(juce::KeyPress::tabKey));
        require(expected[i]->hasKeyboardFocus(false),"Tab does not follow visible signal path order");
    }
    for(size_t i=expected.size()-1;i>0;--i) {
        key(view,juce::KeyPress(juce::KeyPress::tabKey,juce::ModifierKeys::shiftModifier,0));
        require(expected[i-1]->hasKeyboardFocus(false),"Shift-Tab does not reverse signal path order");
    }
}
inline void compactDestination(juce::Component& editor,SignalPathView& strip,const juce::String& id,const char* phase="navigation") {
    if(!pathButton(strip,id).getToggleState()) {
        juce::String selected;
        for(const auto* stage:compactStages)if(pathButton(strip,stage).getToggleState())selected+=juce::String(stage)+" ";
        auto* room=find(editor,"cabRoomOverview");auto* amp=find(editor,"ampSelect1");
        auto* toggle=find<juce::TextButton>(editor,"cabRigControls");
        const auto presentation=juce::String(", roomShowing=")+juce::String(int(room && room->isShowing()))
            +", ampShowing="+juce::String(int(amp && amp->isShowing()))
            +", toggleText="+(toggle?toggle->getButtonText():"missing")
            +", toggleBounds="+(toggle?toggle->getScreenBounds().toString():"missing")
            +", toggleVisible="+juce::String(int(toggle && toggle->isShowing()))
            +", toggleEnabled="+juce::String(int(toggle && toggle->isEnabled()))
            +", toggleBlocked="+juce::String(int(toggle && toggle->isCurrentlyBlockedByAnotherModalComponent()))
            +", toggleMouseOver="+juce::String(int(toggle && toggle->isMouseOver()));
        throw std::runtime_error((juce::String("Compact workflow action was not selected: phase=")+phase
            +", requested="+id+", mode="+juce::String(strip.snapshot().mode)+", selected=["+selected+"]"
            +", button="+pathButton(strip,id).getScreenBounds().toString()+", editor="+editor.getScreenBounds().toString()
            +", pointer="+juce::Desktop::getMousePosition().toString()+presentation).toStdString());
    }
    const auto visible=[&](const char* componentID){auto* c=find(editor,componentID);return c && c->isShowing();};
    if(id=="pre")require(visible("universalPedalBoard") && !visible("postNativePanel0") && !visible("toneEQ_panel"),
                         "PRE workflow action did not open the pedalboard");
    else if(id=="head" || id=="cab") {
        const bool heads=id=="head";const int count=strip.snapshot().mode+1;
        require(find(editor,"cabRigControls")==nullptr && !visible("universalPedalBoard") && !visible("postNativePanel0") && !visible("toneEQ_panel"),
                "HEAD/CAB workflow action did not open the current mode's rig editor");
        auto* room=find<CabRoomOverview>(editor,"cabRoomOverview");
        require(room && room->isShowing()!=heads && room->activeRigCount()==count,
                "HEAD/CAB workflow did not select the correct current-mode overview");
        for(int lane=1;lane<=3;++lane) {
            const auto suffix=juce::String(lane);
            auto* amp=find(editor,"ampSelect"+suffix);auto* cabinet=find(*room,"cabRoomRig"+suffix);
            require(amp && cabinet && amp->isShowing()==(heads && lane<=count)
                    && cabinet->isShowing()==(!heads && lane<=count),
                    "HEAD/CAB navigation displayed the wrong mode's lane controls");
            require(!dialog("ampNativePanel"+suffix),"Aggregate HEAD navigation opened an arbitrary single-lane dialog");
        }
        require(!dialog("cabWorkspace"),"Aggregate CAB navigation opened an arbitrary single-lane workspace");
        require(!pathButton(strip,heads?"cab":"head").getToggleState(),"HEAD and CAB were selected together");
    }
    else if(id=="post")require(visible("postNativePanel0") && !visible("universalPedalBoard") && !visible("toneEQ_panel"),
                               "POST workflow action did not open the rack");
    else {
        const bool tone=id=="tone.eq";
        auto* selected=find<spectralforge::GraphicalEQPanel>(editor,tone?"toneEQ_panel":"finalEQ_panel");
        auto* other=find<spectralforge::GraphicalEQPanel>(editor,tone?"finalEQ_panel":"toneEQ_panel");
        require(selected && other && selected->isShowing() && !other->isShowing() && selected->isNavigationSelected() && !other->isNavigationSelected(),
                "Compact EQ action did not open and select the requested bank");
        auto* frequency=find<juce::Slider>(*selected,spectralforge::eqID(tone?0:1,selected->selectedBand(),"frequency"));
        require(selected->selectedBand()<0 ? selected->hasKeyboardFocus(false) : frequency && frequency->hasKeyboardFocus(true),"Compact EQ action did not focus its existing node or empty workspace");
    }
    globalUtilities(editor,strip);
}
inline void compactKeyboardAndAccessibility(juce::Component& editor,SignalPathView& strip) {
    std::vector<juce::Component*> order;
    for(const auto* id:compactStages)order.push_back(&pathButton(strip,id));
    order.push_back(find(strip,"signalPathExpand"));keyboardOrder(strip,order);
    for(const auto* id:compactStages) {
        auto& button=pathButton(strip,id);
        strip.select({});button.grabKeyboardFocus();key(strip,juce::KeyPress(juce::KeyPress::returnKey));
        compactDestination(editor,strip,id);
        strip.select({});button.grabKeyboardFocus();key(strip,juce::KeyPress(juce::KeyPress::spaceKey));
        compactDestination(editor,strip,id);
        strip.select({});
        auto* handler=button.getAccessibilityHandler();
        require(handler && handler->getActions().invoke(juce::AccessibilityActionType::press),"Accessible workflow press was not handled");
        settle(80);compactDestination(editor,strip,id);
        modifiedSpaceIgnored(button,*order.back());
    }
    modifiedSpaceIgnored(*dynamic_cast<juce::TextButton*>(order.back()),*order.front());
}
inline void routingTrace(const char* phase, ChimeraProcessor& p, juce::Component& editor, SignalPathView& strip) {
#if CHIMERA_ROUTING_DIAGNOSTICS
    const auto actual=read(p); const auto& probe=dynamic_cast<ChimeraEditor&>(editor).routingProbe();
    std::cerr << "ROUTING TRACE phase=" << phase << " now=" << juce::Time::getMillisecondCounterHiRes()
        << " rawGate=" << p.parameters().getRawParameterValue("gateAfterRig")->load()
        << " dspGate=" << p.routingGateObserved.load() << " dspBlocks=" << p.routingBlocks.load()
        << " directPost=" << edge(actual,"merge","gate") << " cachedPost=" << edge(strip.snapshot(),"merge","gate")
        << " cachedInput=" << edge(strip.snapshot(),"input","gate")
        << " ticks=" << probe.ticks << " skipped=" << probe.skipped << " refreshed=" << probe.refreshed
        << " entered=" << probe.entered << " finished=" << probe.finished << " visible=" << probe.visible
        << " messageThread=" << probe.messageThread << " viewCalls=" << strip.refreshCalls << " viewTime=" << strip.refreshTime
        << " snapshotEqual=" << (actual==strip.snapshot()) << std::endl;
#endif
}
inline void automationFocus(ChimeraProcessor& processor,juce::Component& editor,SignalPathView& strip) {
    const auto before=values(processor);
    std::array<float,4> previous{};const std::array<const char*,4> ids{"gateAfterRig","transposeon","transpose","tuneron"};
    for(size_t i=0;i<ids.size();++i)previous[i]=processor.parameters().getRawParameterValue(ids[i])->load();
    for(const auto* stage:{"head","cab"}) {
        juce::Component::SafePointer<juce::TextButton> focused=&pathButton(strip,stage);
        editor.toFront(true);focused->grabKeyboardFocus();settle(35);
        for(const auto gate:{0,1}) {
            set(processor,"gateAfterRig",float(gate));set(processor,"transposeon",1);set(processor,"transpose",-2);set(processor,"tuneron",1);
            routingTrace("parameters-set",processor,editor,strip);
            juce::AudioBuffer<float> diagnosticAudio(2,64); diagnosticAudio.clear(); juce::MidiBuffer diagnosticMidi;
            processor.processBlock(diagnosticAudio,diagnosticMidi);
            routingTrace("audio-processed",processor,editor,strip);
            settle(100);
            routingTrace("messages-settled",processor,editor,strip);
            require(focused && focused.getComponent()==find<juce::TextButton>(strip,"path."+juce::String(stage)) && focused->hasKeyboardFocus(false),
                    "Utility automation rebuilt or stole focus from the compact stage");
            compactContract(strip,strip.snapshot().mode);globalUtilities(editor,strip);
            require(edge(strip.snapshot(),"input","tuner") && edge(strip.snapshot(),gate?"merge":"input","gate"),
                    "Compact presentation hid the actual automated routing snapshot");
        }
    }
    for(size_t i=0;i<ids.size();++i)set(processor,ids[i],previous[i]);settle(100);
    require(before==values(processor),"Automation focus fixture did not restore its initial parameters");
}
inline void rigPresentation(ChimeraProcessor& processor,juce::Component& editor,SignalPathView& strip,Writes& writes) {
    const auto before=values(processor);const int mode=strip.snapshot().mode;
    const int initialChanges=writes.changes,initialGestures=writes.gestures;
    const auto dual=processor.parameters().getRawParameterValue("dualtype")->load();
    auto* canvas=find(editor,"surface");
    require(canvas && find(editor,"cabRigControls")==nullptr,"Duplicate rig navigation remains");
    click(pathButton(strip,"head"));compactDestination(editor,strip,"head","initial HEAD pointer click");
    tab(*canvas,"PRE");tab(*canvas,"RIGS");
    compactDestination(editor,strip,"head","HEAD stage return");
    click(pathButton(strip,"cab"));compactDestination(editor,strip,"cab","CAB navigation");
    click(pathButton(strip,"head"));compactDestination(editor,strip,"head","HEAD navigation");
    if(mode==1) {
        set(processor,"dualtype",dual>.5f?0.f:1.f);settle(100);
        compactContract(strip,mode);compactDestination(editor,strip,"head","Dual topology change");
        set(processor,"dualtype",dual);settle(100);
        compactDestination(editor,strip,"head","Dual topology restore");
    }
    // Routing-mode changes keep the existing room-first policy, while changing
    // the Dual topology alone retains its current HEAD presentation.
    set(processor,"mode",float((mode+1)%3));settle(100);
    compactContract(strip,(mode+1)%3);compactDestination(editor,strip,"cab","HEAD mode change");
    set(processor,"mode",float(mode));settle(100);
    compactContract(strip,mode);compactDestination(editor,strip,"cab","CAB mode restore");
    for(const auto* stage:{"pre","post"}) {
        click(pathButton(strip,stage));compactDestination(editor,strip,stage,"global stage pointer click");
        set(processor,"mode",float((mode+1)%3));settle(100);
        compactContract(strip,(mode+1)%3);compactDestination(editor,strip,stage,"global stage mode change");
        set(processor,"mode",float(mode));settle(100);
        compactContract(strip,mode);compactDestination(editor,strip,stage,"global stage mode restore");
    }
    require(before==values(processor),"Rig presentation fixture changed audio state");
    require(writes.changes-initialChanges==6+(mode==1?2:0) && writes.gestures==initialGestures,
            "Rig/global-page navigation wrote parameters beyond the intentional mode/topology fixture changes");
}
inline void restoredRigDestinations(ChimeraProcessor& processor,juce::Component& editor,SignalPathView& strip,Writes& writes) {
    const auto before=values(processor);writes.changes=writes.gestures=0;
    for(const auto* id:{"head","cab"}) {click(pathButton(strip,id));compactDestination(editor,strip,id);}
    require(before==values(processor) && writes.changes==0 && writes.gestures==0,
            "Restored HEAD/CAB navigation changed parameters or host gestures");
}
inline void preSelectionRestore(ChimeraProcessor& processor) {
    const auto before=values(processor);const auto previous=processor.parameters().getRawParameterValue("boardEnabled")->load();
    set(processor,"boardEnabled",1);
    SignalPathView restored(processor,true);restored.setSize(1140,44);
    restored.addToDesktop(juce::ComponentPeer::windowIsTemporary);restored.setVisible(true);
    const auto selectFirst=[&](const char* prefix) {
        const auto& nodes=restored.snapshot().nodes;
        auto selected=std::find_if(nodes.begin(),nodes.end(),[&](const auto& node){return node.id.startsWith(prefix);});
        require(selected!=nodes.end(),"PRE restore fixture cannot find the active PRE engine");
        restored.select(selected->id);return selected->id;
    };
    const auto boardOwner=selectFirst("pre.owner");
    set(processor,"boardEnabled",0);restored.refresh();
    require(!restored.snapshot().find(boardOwner) && pathButton(restored,"pre").getToggleState(),
            "Saved pedalboard selection lost PRE highlight when legacy PRE was restored");
    compactContract(restored,restored.snapshot().mode);
    const auto legacyOwner=selectFirst("pre.legacy");
    set(processor,"boardEnabled",1);restored.refresh();
    require(!restored.snapshot().find(legacyOwner) && pathButton(restored,"pre").getToggleState(),
            "Saved legacy PRE selection lost PRE highlight when the pedalboard was restored");
    compactContract(restored,restored.snapshot().mode);
    set(processor,"boardEnabled",previous);settle(80);
    require(before==values(processor),"PRE restore fixture did not preserve its original audio state");
}
inline void rigSelectionRestore(ChimeraProcessor& processor) {
    const auto before=values(processor);const auto previous=processor.parameters().getRawParameterValue("mode")->load();
    SignalPathView restored(processor,true);restored.setSize(1140,44);
    restored.addToDesktop(juce::ComponentPeer::windowIsTemporary);restored.setVisible(true);
    for(const auto* stage:{"head","cab"}) {
        set(processor,"mode",2);restored.refresh();
        const bool head=juce::String(stage)=="head";const auto id=juce::String(head?"amp.context5":"cab.context5");
        require(restored.snapshot().find(id),"Rig restore fixture is missing its Matrix HIGH context");
        restored.select(id);set(processor,"mode",0);restored.refresh();
        require(!restored.snapshot().find(id) && pathButton(restored,stage).getToggleState()
                && !pathButton(restored,head?"cab":"head").getToggleState(),
                "Stale rig context lost the distinction between HEAD and CAB selection");
        compactContract(restored,0);
    }
    set(processor,"mode",previous);settle(80);
    require(before==values(processor),"Rig selection restore fixture did not preserve audio state");
}
inline void run(const juce::File& folder) {
    cabLowBlendAudioTests::run();
    auto p=std::make_unique<ChimeraProcessor>();topology(*p);p->prepareToPlay(48000,64);
    Writes writes;p->addListener(&writes);
    auto editor=std::make_unique<ChimeraEditor>(*p);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);
    auto* strip=find<SignalPathView>(*editor,"signalPathStrip");require(strip!=nullptr,"Persistent path strip absent");
    compactDestination(*editor,*strip,"cab");
    for(const auto scenario:std::array<std::array<int,2>,4>{{{0,0},{1,0},{1,1},{2,0}}}) {
        std::cout<<"Signal Path scenario "<<scenario[0]<<" / "<<scenario[1]<<std::endl;
        const int mode=scenario[0];
        // October 23 starts with empty persistent-slot banks. Exercise both
        // legitimate focus targets without weakening the occupied-band check.
        const bool occupiedEQ=mode==1;
        for(int bank=0;bank<2;++bank) {
            auto& edits=p->eqEditing();
            if(occupiedEQ && !edits.isOccupied(bank,0))
                require(edits.addBand(bank,1000.f,0.f)==0,"Navigation occupied-bank fixture failed");
            if(!occupiedEQ && edits.isOccupied(bank,0))
                require(edits.removeBand(bank,0),"Navigation empty-bank fixture failed");
            require(edits.occupiedMask(bank)==(occupiedEQ?1:0),"Navigation fixture has unexpected occupancy");
        }
        set(*p,"mode",float(mode));set(*p,"dualtype",float(scenario[1]));settle(100);
        compactContract(*strip,mode);globalUtilities(*editor,*strip);automationFocus(*p,*editor,*strip);
        rigPresentation(*p,*editor,*strip,writes);
        // Capture the ordinary rig workflow before opening the detailed graph
        // or visiting individual editing controls. Keep every supported scale.
        auto* canvas=find(*editor,"surface");require(canvas,"Signal workflow editor canvas missing");tab(*canvas,"RIGS");
        for(float scale:{.75f,1.f,1.25f,1.5f}) {
            editor->setSize(juce::roundToInt(1180*scale),juce::roundToInt(780*scale));settle(30);
            compactContract(*strip,mode);globalUtilities(*editor,*strip);
            capture(*editor,folder,"signal-workflow-mode-"+juce::String(mode)+"-dual-"+juce::String(scenario[1])+"-scale-"+juce::String(juce::roundToInt(scale*100)));
        }
        editor->setSize(1180,780);settle(40);
        click(pathButton(*strip,"head"));compactDestination(*editor,*strip,"head");
        capture(*editor,folder,"signal-workflow-head-mode-"+juce::String(mode)+"-dual-"+juce::String(scenario[1])+"-scale-100");
        click(pathButton(*strip,"cab"));compactDestination(*editor,*strip,"cab");
        auto* expand=find<juce::TextButton>(*editor,"signalPathExpand");require(expand,"Path expansion missing");
        const auto before=values(*p);writes.changes=writes.gestures=0;
        compactKeyboardAndAccessibility(*editor,*strip);
        // ROUTING remains available from the same keyboard sequence as editors.
        expand->grabKeyboardFocus();key(*strip,juce::KeyPress(juce::KeyPress::returnKey));
        auto returnGraph=juce::Component::SafePointer<juce::DialogWindow>(dialog("signalPathGraph"));
        require(returnGraph,"Return did not open routing inspection");returnGraph->closeButtonPressed();settle(80);
        require(!returnGraph,"Return-opened routing inspection did not close");
        expand->grabKeyboardFocus();key(*strip,juce::KeyPress(juce::KeyPress::spaceKey));
        auto graphWindow=juce::Component::SafePointer<juce::DialogWindow>(dialog("signalPathGraph"));require(graphWindow,"Path expansion did not open native graph");
        auto* graph=dynamic_cast<SignalPathView*>(graphWindow->getContentComponent());require(graph,"Graph content missing");
        require(graph->snapshot()==read(*p),"Expanded graph does not retain the complete current routing snapshot");
        for(const auto* id:{"input","gate","transpose","tuner","utilities","output"})
            require(find<juce::TextButton>(*graph,"path."+juce::String(id)),"Detailed routing lost a hidden global utility");
        const auto nativeGraphSize=graphWindow->getBounds();
        graphWindow->setSize(855,540);settle(80);
        for(auto* child:graph->getChildren())inside(*graph,*child,"Expanded path block clipped on a small native display");
        capture(*graph,folder,"signal-path-small-mode-"+juce::String(mode)+"-dual-"+juce::String(scenario[1]));
        graphWindow->setSize(nativeGraphSize.getWidth(),nativeGraphSize.getHeight());settle(80);
        for(auto* child:graph->getChildren())inside(*graph,*child,"Expanded path block clipped by native window bounds");
        capture(*graph,folder,("signal-path-mode-"+juce::String(mode)+"-dual-"+juce::String(scenario[1])).toRawUTF8());
        const auto nodes=graph->snapshot().nodes;
        std::vector<juce::Component*> detailOrder;
        for(const auto& node:nodes)detailOrder.push_back(&pathButton(*graph,node.id));
        keyboardOrder(*graph,detailOrder);
        for(const auto& node:nodes){auto* b=find<juce::TextButton>(*graph,"path."+node.id);require(b,"Navigation block missing");click(*b);if(!b->getToggleState())throw std::runtime_error("Native pointer click did not select block "+node.id.toStdString());
            accessibleButton(*b,node.label);
            if(node.target==Target::cab)require(dialog("cabWorkspace"),"CAB click did not open the existing editor");
            if(node.target==Target::toneEQ || node.target==Target::finalEQ){
                const bool tone=node.target==Target::toneEQ;
                auto* selected=find<spectralforge::GraphicalEQPanel>(*editor,tone?"toneEQ_panel":"finalEQ_panel");
                auto* other=find<spectralforge::GraphicalEQPanel>(*editor,tone?"finalEQ_panel":"toneEQ_panel");
                require(selected && other && selected->isShowing() && !other->isShowing() && selected->isNavigationSelected() && !other->isNavigationSelected(),"Expanded EQ click did not open/highlight the requested bank");
                if(occupiedEQ) {
                    require(selected->selectedBand()==0,"EQ navigation lost the occupied persistent slot");
                    auto* frequency=find<juce::Slider>(*selected,spectralforge::eqID(tone?0:1,selected->selectedBand(),"frequency"));
                    require(frequency && frequency->hasKeyboardFocus(true),"EQ navigation did not focus the requested bank's frequency control");
                } else {
                    require(selected->selectedBand()==-1 && selected->hasKeyboardFocus(false),
                            "Empty EQ navigation did not focus the requested bank's graph");
                }
            }
            if(node.target==Target::dynamics)require(find(*editor,"commonDynamics.panel")->isShowing(),"Detailed compressor/limiter navigation did not open dynamics");
            if(node.target==Target::amp && node.lane>=0)require(dialog("ampNativePanel"+juce::String(node.lane+1)),"AMP click did not open the existing lane editor");
            if(node.target==Target::pre || node.target==Target::post || node.target==Target::rigs || node.target==Target::amp || node.target==Target::cab || node.target==Target::low) {
                const auto stage=node.target==Target::pre?"pre":node.target==Target::post?"post":node.target==Target::cab?"cab":"head";
                require(pathButton(*strip,stage).getToggleState(),"Detailed route selection lost its compact editing-stage highlight");
                if(node.target==Target::amp || node.target==Target::cab || node.target==Target::low || node.target==Target::rigs)
                    require(!pathButton(*strip,node.target==Target::cab?"head":"cab").getToggleState(),
                            "Detailed route selection confused amplifier and cabinet editing stages");
            }
            // Amp and CAB dialogs are independently owned and closed here.
            if(auto* w=dialog("signalPathTuner"))close(w);
            if(auto* w=dialog("ampNativePanel"+juce::String(node.lane+1)))close(w);
            if(auto* w=dialog("cabWorkspace")){auto* workspace=dynamic_cast<CabWorkspace*>(w->getContentComponent());require(workspace && workspace->focusedRig()==node.lane,"CAB navigation opened wrong rig");close(w);}
        }
        for(float scale:{.75f,1.f,1.25f,1.5f}){
            editor->setSize(juce::roundToInt(1180*scale),juce::roundToInt(780*scale));
            for(const auto* id:compactStages){
                auto* button=find<juce::TextButton>(*strip,"path."+juce::String(id));require(button,"Compact navigation missing");click(*button);
                compactContract(*strip,mode);compactDestination(*editor,*strip,id);
                if(juce::String(id)=="head" || juce::String(id)=="cab")
                    for(const auto& node:nodes) {
                        const bool actual=pathButton(*graph,node.id).getToggleState();
                        const bool expected=node.id.startsWith(juce::String(id)=="head"?"amp.context":"cab.context");
                        if(actual!=expected)
                            throw std::runtime_error((juce::String("Aggregate HEAD/CAB selection did not highlight every matching detailed branch: requested=")
                                +id+", node="+node.id+", actual="+juce::String(int(actual))+", expected="+juce::String(int(expected))
                                +", graphMode="+juce::String(graph->snapshot().mode)+", scale="+juce::String(scale)).toStdString());
                    }
                const auto rect=editor->getLocalArea(strip,strip->getLocalBounds());require(editor->getLocalBounds().contains(rect),"Strip clipped at UI scale");
                for(auto* sibling:strip->getParentComponent()->getChildren())if(sibling!=strip && sibling->isVisible())require(!rect.intersects(editor->getLocalArea(sibling,sibling->getLocalBounds())),"Strip overlaps visible editor content");
                for(auto* child:strip->getChildren())inside(*strip,*child,"Compact path button clipped");
                for(int e=0;e<2;++e){auto* panel=find<spectralforge::GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");
                    if(panel->isShowing()){inside(*editor,*panel,"EQ panel clipped at UI scale");for(int band=0;band<12;++band)require(panel->getLocalBounds().toFloat().contains(panel->nodePosition(band)),"EQ node clipped");}
                }
            }
            capture(*editor,folder,"integrated-eq-mode-"+juce::String(mode)+"-dual-"+juce::String(scenario[1])+"-scale-"+juce::String(juce::roundToInt(scale*100)));
        }
        auto preNode=std::find_if(nodes.begin(),nodes.end(),[](const auto& node){return node.target==Target::pre;});
        require(preNode!=nodes.end(),"Expanded PRE keyboard target absent");
        auto& pre=pathButton(*graph,preNode->id);graphWindow->toFront(true);pre.grabKeyboardFocus();
        key(*graph,juce::KeyPress(juce::KeyPress::returnKey));settle(60);
        require(pre.getToggleState() && pathButton(*strip,"pre").getToggleState(),"Expanded PRE Return did not preserve workflow selection");
        graphWindow->toFront(true);pre.grabKeyboardFocus();key(*graph,juce::KeyPress(juce::KeyPress::spaceKey));settle(60);
        require(pre.getToggleState() && pathButton(*strip,"pre").getToggleState(),"Expanded PRE Space did not preserve workflow selection");
        graphWindow->toFront(true);modifiedSpaceIgnored(pre,pathButton(*graph,"input"));
        require(before==values(*p)&&writes.changes==0&&writes.gestures==0,"Navigation/open/resize emitted parameter writes or host gestures");
        capture(*editor,folder,("signal-path-editor-"+juce::String(mode)+"-dual-"+juce::String(scenario[1])).toRawUTF8());graphWindow->closeButtonPressed();settle(80);require(!graphWindow,"Non-modal graph did not close");
    }
    preSelectionRestore(*p);rigSelectionRestore(*p);
    for(int preset=0;preset<spectralforge::selectablePresetCount;++preset){
        std::cout<<"Signal Path preset "<<preset<<std::endl;
        p->loadFactoryPreset(preset);settle(50);const auto expected=values(*p);writes.changes=writes.gestures=0;
        auto* pre=find<juce::TextButton>(*strip,"path.pre");click(*pre);
        compactContract(*strip,int(p->parameters().getRawParameterValue("mode")->load()));compactDestination(*editor,*strip,"pre");
        require(expected==values(*p)&&writes.changes==0&&writes.gestures==0,"Preset navigation changed audio content");
        require(strip->snapshot().mode==int(p->parameters().getRawParameterValue("mode")->load()),"Preset mode/path failed to converge");
        require(!strip->snapshot().find("tone.eq")->active && !strip->snapshot().find("final.eq")->active,"Factory recall left EQ path enabled");
    }
    std::cout<<"Signal Path final automation / A-B restore"<<std::endl;
    // Host changes converge through a presentation read, including inactive tabs.
    set(*p,"mode",2);set(*p,"cab2",0);set(*p,"gateAfterRig",1);set(*p,"transposeon",1);set(*p,"transpose",-2);settle(120);
    compactContract(*strip,2);
    require(!requiredNode(strip->snapshot(),"cab.context4","host automation").active && requiredNode(strip->snapshot(),"transpose","host automation").active&&edge(strip->snapshot(),"merge","gate"),"Host automation did not refresh path/bypass");
    {SignalPathView automated(*p,false);capture(automated,folder,"signal-path-matrix-post-gate");}
    set(*p,"toneEQ_bypass",0);set(*p,"toneEQ_b1_enabled",1);set(*p,"finalEQ_bypass",1);settle(100);
    require(requiredNode(strip->snapshot(),"tone.eq","EQ automation").active && requiredNode(strip->snapshot(),"tone.eq","EQ automation").status=="ACTIVE / 1 BANDS" && !requiredNode(strip->snapshot(),"final.eq","EQ automation").active,"Automated EQ bypass/band status did not converge");
    p->copyComparison();set(*p,"cab2",1);set(*p,"toneEQ_bypass",1);set(*p,"finalEQ_bypass",0);p->selectComparison(1);settle(100);require(!requiredNode(strip->snapshot(),"cab.context4","A/B restore").active && requiredNode(strip->snapshot(),"tone.eq","A/B restore").active && !requiredNode(strip->snapshot(),"final.eq","A/B restore").active,"A/B did not restore graph CAB/EQ bypass");compactContract(*strip,2);
    restoredRigDestinations(*p,*editor,*strip,writes);
    std::cout<<"Signal Path project restore"<<std::endl;
    juce::MemoryBlock saved;p->getStateInformation(saved);p->loadFactoryPreset(0);p->setStateInformation(saved.getData(),int(saved.getSize()));settle(120);require(strip->snapshot().mode==2&&!requiredNode(strip->snapshot(),"cab.context4","project restore").active && requiredNode(strip->snapshot(),"tone.eq","project restore").active && !requiredNode(strip->snapshot(),"final.eq","project restore").active,"Project restore left stale graph");compactContract(*strip,2);
    restoredRigDestinations(*p,*editor,*strip,writes);
    std::cout<<"Signal Path editor recreation"<<std::endl;
    auto expected=values(*p);writes.changes=writes.gestures=0;editor->removeFromDesktop();editor.reset();editor=std::make_unique<ChimeraEditor>(*p);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(80);
    strip=find<SignalPathView>(*editor,"signalPathStrip");require(strip,"Restored editor lost the compact workflow");compactContract(*strip,2);globalUtilities(*editor,*strip);
    require(expected==values(*p)&&writes.changes==0&&writes.gestures==0,"Editor recreation changed sound");
    compactDestination(*editor,*strip,"cab");restoredRigDestinations(*p,*editor,*strip,writes);
    p->removeListener(&writes);editor.reset();
    std::cout<<"PASS Signal Path: six-stage HEAD/CAB workflow / global utility retention / full routing inspection; Classic / Dual Blend & Crossover / Matrix LOW topology; native pointer, Tab/Shift-Tab/Return/Space and accessibility navigation; modified/unfocused Space isolation; host-write/gesture isolation; scales; automation/focus; preset/project/A-B/PRE engine restore; editor recreation\n";
}
}
