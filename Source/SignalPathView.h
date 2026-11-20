#pragma once
#include "SignalPath.h"
#include "UIRefresh.h"
#include <optional>

// JUCE buttons activate on Return. Accept unmodified Space only while this
// navigation button owns focus, without registering a host-wide shortcut.
class SignalPathNavigationButton : public juce::TextButton {
public:
    using juce::TextButton::TextButton;
    bool keyPressed(const juce::KeyPress& key) override {
        if(isEnabled() && hasKeyboardFocus(false) && key.isKeyCode(juce::KeyPress::spaceKey)
            && !key.getModifiers().isAnyModifierKeyDown()){
            triggerClick();return true;
        }
        return juce::TextButton::keyPressed(key);
    }
};
// These navigation buttons have no APVTS attachments or audio-state writes.
// A movable block (1.4, docs/SIGNAL_ORDER_1_4.md) also takes order commands:
// right-click for the placement menu, Alt + arrows / Home / End to move it,
// Ctrl + Z / Ctrl + Shift + Z to undo / redo order changes. 1.5: drag it to
// another position in its block; Escape or a release away from its block
// cancels. A drag writes nothing until the drop.
class SignalPathButton final : public SignalPathNavigationButton {
public:
    bool compact{},active{};juce::String label,status;
    spectralforge::signalPath::Target target{};
    bool drag{},swallowRelease{},cancelled{};
    std::function<void()> onPlacementMenu;
    std::function<bool(const juce::KeyPress&)> onOrderKey;
    std::function<void(juce::Point<int>)> onDragMove;   // pointer in the parent view's coordinates
    std::function<void(bool commit)> onDragEnd;
    bool dragging() const noexcept {return drag;}
    void mouseDown(const juce::MouseEvent& e) override {
        swallowRelease=false;cancelled=false;
        if(e.mods.isPopupMenu() && onPlacementMenu){onPlacementMenu();return;}
        SignalPathNavigationButton::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        // After Escape the gesture stays cancelled until the next press.
        if(onDragMove && !e.mods.isPopupMenu() && !cancelled && (drag || e.getDistanceFromDragStart()>6)) {
            if(!drag){drag=true;setMouseCursor(juce::MouseCursor::DraggingHandCursor);}
            onDragMove(e.getEventRelativeTo(getParentComponent()).getPosition());return;
        }
        SignalPathNavigationButton::mouseDrag(e);
    }
    void mouseUp(const juce::MouseEvent& e) override {
        if(e.mods.isPopupMenu() && onPlacementMenu)return;
        if(drag){finishDrag(true);return;}   // a drag is not a click: it does not navigate
        if(swallowRelease){swallowRelease=false;setState(juce::Button::buttonNormal);return;}   // released after Escape
        SignalPathNavigationButton::mouseUp(e);
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if(drag && key.isKeyCode(juce::KeyPress::escapeKey)){finishDrag(false);swallowRelease=true;cancelled=true;return true;}
        if(onOrderKey && onOrderKey(key))return true;
        return SignalPathNavigationButton::keyPressed(key);
    }
    void finishDrag(bool commit){drag=false;setMouseCursor(juce::MouseCursor::NormalCursor);setState(juce::Button::buttonNormal);if(onDragEnd)onDragEnd(commit);}
    void paintButton(juce::Graphics& g,bool hover,bool down) override {
        const auto area=getLocalBounds().toFloat().reduced(.5f);
        g.setColour(getToggleState()?juce::Colour(0xff416953):juce::Colour(hover||down?0xff2f4540:0xff222c30));g.fillRoundedRectangle(area,3);
        g.setColour(hasKeyboardFocus(true)?juce::Colours::white:juce::Colour(getToggleState()?0xff9bc0a8:0xff53675c));g.drawRoundedRectangle(area,3,1);
        g.setColour(active?juce::Colour(0xffe8e8dc):juce::Colour(0xffacb6b2));g.setFont(juce::FontOptions(compact?12.f:11.f,juce::Font::bold));
        auto labelBounds=compact?getLocalBounds().reduced(6,2):getLocalBounds().reduced(4).withHeight(22);
        if(compact){
            // Original vector pictograms remain crisp at every UI scale.
            juce::Graphics::ScopedSaveState saved(g);g.addTransform(juce::AffineTransform::translation(12.f,float(getHeight()-24)/2));
            using T=spectralforge::signalPath::Target;
            if(target==T::amp){g.drawRoundedRectangle(1,5,28,18,2,1.5f);g.drawLine(8,5,8,1,1.5f);g.drawLine(8,1,22,1,1.5f);g.drawLine(22,1,22,5,1.5f);for(int i=0;i<3;++i)g.drawEllipse(5.f+i*8,15,3,3,1.2f);}
            else if(target==T::cab){g.drawRoundedRectangle(4,0,22,24,2,1.5f);g.drawEllipse(10,3,10,8,1.5f);g.drawEllipse(10,13,10,8,1.5f);}
            else if(target==T::toneEQ||target==T::finalEQ){for(int i=0;i<3;++i){const float x=5.f+i*10;g.drawLine(x,1,x,23,1.5f);g.fillRoundedRectangle(x-3,float(4+i*6),6,4,1);}}
            else if(target==T::pre){g.drawRoundedRectangle(6,0,18,24,3,1.5f);g.drawEllipse(11,3,8,8,1.5f);g.fillEllipse(13,17,4,4);}
            else {g.drawRoundedRectangle(0,1,30,22,2,1.5f);g.drawLine(1,12,29,12,1.5f);g.drawEllipse(4,5,3,3,1);g.drawEllipse(4,16,3,3,1);g.drawLine(13,7,25,7,1);g.drawLine(13,18,25,18,1);}
        }
        if(compact)labelBounds.removeFromLeft(43);
        g.drawFittedText(label,labelBounds,juce::Justification::centred,1);
        if(!compact){g.setFont(juce::FontOptions(9.f));g.drawFittedText(status,4,25,getWidth()-8,17,juce::Justification::centred,1);}
    }
};
class SignalPathView final : public juce::Component,
                             private juce::Timer,
                             private juce::AudioProcessorValueTreeState::Listener,
                             private juce::AsyncUpdater {
public:
    SignalPathView(ChimeraProcessor& p,bool compactView):processor(p),compact(compactView) {
        setComponentID(compact?"signalPathStrip":"signalPathGraph");
        setName(compact?"Signal path editing stages":"Actual signal routing");
        setTitle(getName());
        setDescription(compact?"PRE, amplifier heads, cabinets, Tone EQ, POST and Final EQ. Open Routing for the complete signal path, including global utilities.":"Complete signal path with utility positions, taps, parallel branches and rig summing. Select a block to open its controls.");
        setFocusContainerType(juce::Component::FocusContainerType::focusContainer);
        for(const auto* id:routingParameters)processor.parameters().addParameterListener(id,this);
        if(!compact){setSize(1140,650);startTimerHz(25);}refresh();
    }
    ~SignalPathView() override {
        stopTimer();cancelPendingUpdate();
        for(const auto* id:routingParameters)processor.parameters().removeParameterListener(id,this);
    }
    std::function<void(const spectralforge::signalPath::Node&)> navigate;
    std::function<void()> expand;
    std::function<juce::String()> selection;
    const spectralforge::signalPath::Snapshot& snapshot() const {return current;}
    void select(const juce::String& id){if(selected!=id){selected=id;updateSelection();}}
    // Order commands for a movable node (docs/SIGNAL_ORDER_1_4.md). Each one is
    // a processor edit with its own undo entry; navigation never calls them.
    enum class OrderCommand {earlier,later,first,last,position,undo,redo,reset,preset};
    struct Placement {spectralforge::OrderRegion region;int module;};
    static std::optional<Placement> movable(const juce::String& id) {
        using spectralforge::OrderRegion;
        for(const auto region:{OrderRegion::tone,OrderRegion::post})
            for(int m=0;m<spectralforge::moduleCount(region);++m)if(spectralforge::moduleNodeID(region,m)==id)return Placement{region,m};
        return std::nullopt;
    }
    bool orderCommand(const juce::String& id,OrderCommand command,int position=-1) {
        const auto place=movable(id);if(!place)return false;
        const int count=spectralforge::moduleCount(place->region);bool changed=false;
        switch(command){
            case OrderCommand::earlier:changed=processor.moveModule(place->region,place->module,-1);break;
            case OrderCommand::later:changed=processor.moveModule(place->region,place->module,1);break;
            case OrderCommand::first:changed=processor.placeModule(place->region,place->module,0);break;
            case OrderCommand::last:changed=processor.placeModule(place->region,place->module,count-1);break;
            case OrderCommand::position:changed=processor.placeModule(place->region,place->module,position);break;
            case OrderCommand::undo:changed=processor.undoOrder();break;
            case OrderCommand::redo:changed=processor.redoOrder();break;
            case OrderCommand::reset:changed=processor.resetOrder(place->region);break;
            case OrderCommand::preset:changed=processor.applyOrderPreset(place->region,position);break;
        }
        if(changed){refresh();if(auto* b=buttonFor(id))b->grabKeyboardFocus();}
        return changed;
    }
    // 1.5 drag (docs/SIGNAL_ORDER_1_4.md): the valid drop targets are the
    // positions of the dragged block's own region. Hovering only updates the
    // preview; the drop is one placement command (one undo entry).
    void dragTo(const juce::String& id,juce::Point<int> at) {
        const auto place=movable(id);if(!place)return;
        const auto targets=dropTargets(place->region);
        // A target counts only under the pointer (with a small margin), so a
        // release over another block or empty space cancels.
        const int margin=juce::roundToInt(14.f*graphScale());int best=-1;
        for(size_t i=0;i<targets.size();++i)if(targets[i].expanded(margin).contains(at)){best=int(i);break;}
        dragId=id;dragPoint=at;dropSlot=best;repaint();
    }
    void endDrag(const juce::String& id,bool commit) {
        const int slot=dropSlot;const bool wasDragging=dragId==id;
        dragId={};dropSlot=-1;repaint();
        if(!commit || !wasDragging || slot<0)return;
        const auto place=movable(id);if(!place)return;
        const auto order=processor.requestedOrder();
        const int from=place->region==spectralforge::OrderRegion::tone?order.tone.position(place->module):order.post.position(place->module);
        if(slot!=from)orderCommand(id,OrderCommand::position,slot);
    }
    // Scaled bounds of each position in a region, in its current order.
    std::vector<juce::Rectangle<int>> dropTargets(spectralforge::OrderRegion region) const {
        const auto order=processor.requestedOrder();std::vector<juce::Rectangle<int>> result;
        for(int slot=0;slot<spectralforge::moduleCount(region);++slot){
            const int module=region==spectralforge::OrderRegion::tone?order.tone.slots[(size_t)slot]:order.post.slots[(size_t)slot];
            if(const auto* n=current.find(spectralforge::moduleNodeID(region,module)))result.push_back(n->bounds.toFloat().transformedBy(juce::AffineTransform::scale(graphScale())).getSmallestIntegerContainer());
        }
        return result;
    }
    const juce::String& draggingBlock() const noexcept {return dragId;}
    int dropPosition() const noexcept {return dropSlot;}
    void paintOverChildren(juce::Graphics& g) override {
        if(compact || dragId.isEmpty())return;
        const auto place=movable(dragId);if(!place)return;
        const auto targets=dropTargets(place->region);
        g.setColour(juce::Colour(0x60d7a35a));for(const auto& r:targets)g.drawRoundedRectangle(r.toFloat().expanded(2),4,1);   // every valid target
        g.setFont(juce::FontOptions(11.f,juce::Font::bold));
        if(dropSlot>=0 && dropSlot<int(targets.size())){
            const auto r=targets[(size_t)dropSlot].toFloat().expanded(3);
            g.setColour(juce::Colour(0xffd7a35a));g.drawRoundedRectangle(r,4,2.5f);
            g.drawText(spectralforge::moduleName(place->region,place->module).toUpperCase()+" > POSITION "+juce::String(dropSlot+1),r.withY(r.getY()-18).withHeight(16).expanded(60,0),juce::Justification::centred);
        } else {
            g.setColour(juce::Colour(0xffacb6b2));
            g.drawText("RELEASE HERE TO CANCEL",juce::Rectangle<int>(dragPoint.x-90,dragPoint.y-26,180,16),juce::Justification::centred);
        }
    }
    juce::TextButton* buttonFor(const juce::String& id){for(auto& b:buttons)if(b->getComponentID()=="path."+id)return b.get();return nullptr;}
#if CHIMERA_ROUTING_DIAGNOSTICS
    uint64_t refreshCalls{}; double refreshTime{};
#endif
    void refresh() {
#if CHIMERA_ROUTING_DIAGNOSTICS
        ++refreshCalls; refreshTime=juce::Time::getMillisecondCounterHiRes();
#endif
        auto next=spectralforge::signalPath::read(processor);
        if(next==current)return;
        current=std::move(next);
        // Preserve existing buttons (and keyboard focus) across automation.
        std::vector<spectralforge::signalPath::Node> visible;
        if(compact){
            using namespace spectralforge::signalPath;
            const auto add=[&](const juce::String& id){if(const auto* n=current.find(id))visible.push_back(*n);};
            // This strip is an editing workflow. The unabridged routing model
            // remains in current and is inspected in the separate graph.
            bool preActive=false;for(const auto& n:current.nodes)if(n.id.startsWith("pre."))preActive|=n.active;
            visible.push_back({"pre","PRE",preActive?"ACTIVE":"BYPASSED","Open the PRE pedalboard.",Target::pre,-1,-1,preActive});
            const juce::String modeName=current.mode==0?"CLASSIC":current.mode==1?(current.dualCross?"DUAL CROSSOVER":"DUAL BLEND"):"MATRIX";
            bool headActive=false,cabActive=false;
            for(const auto& n:current.nodes){if(n.id.startsWith("amp.context"))headActive|=n.active;if(n.target==Target::cab)cabActive|=n.active;}
            visible.push_back({"head","HEAD",modeName+" / "+juce::String(current.mode+1)+" HEAD"+(current.mode==0?"":"S"),"Open the amplifier controls for every active rig in the current mode.",Target::amp,-1,-1,headActive});
            visible.push_back({"cab","CAB",modeName+" / "+juce::String(current.mode+1)+" CAB"+(current.mode==0?"":"S"),"Open the cabinet room for every active rig in the current mode.",Target::cab,-1,-1,cabActive});
            add("tone.eq");
            bool postActive=false;for(const auto& n:current.nodes)if(n.target==Target::post)postActive|=n.active;
            visible.push_back({"post","POST",postActive?"ACTIVE":"BYPASSED","Open the POST effects rack.",Target::post,-1,-1,postActive});
            add("final.eq");
            if(const auto* n=current.find("synth"))visible.push_back({"synth","SYNTH",n->status,"Open the SYNTH page: oscillators, filter, envelopes, LFOs and modulation.",Target::synth,-1,-1,n->active});
        }else visible=current.nodes;
        std::vector<std::unique_ptr<juce::TextButton>> nextButtons;
        for(const auto& node:visible) {
            std::unique_ptr<juce::TextButton> button;
            for(auto& old:buttons)if(old && old->getComponentID()=="path."+node.id){button=std::move(old);break;}
            if(!button){button=std::make_unique<SignalPathButton>();addAndMakeVisible(*button);}
            auto& painted=static_cast<SignalPathButton&>(*button);painted.compact=compact;painted.active=node.active;painted.label=node.label;painted.status=node.status;painted.target=node.target;
            button->setComponentID("path."+node.id);
            button->setButtonText(compact?node.label:node.label+"\n"+node.status);
            button->setName(node.label+" / "+node.status+" / open editor");
            button->setTitle(button->getName());
            button->setDescription(node.detail+(compact?" Open Routing to inspect all processing stages.":""));
            button->setTooltip(node.label+" | "+node.status+"\n"+node.detail+(compact?"\nROUTING shows the complete signal path.":""));
            button->setWantsKeyboardFocus(true);
            button->setColour(juce::TextButton::textColourOffId,node.active?juce::Colour(0xffe5e1d4):juce::Colour(0xff9caaa5));
            button->onClick=[this,node]{selected=node.id;updateSelection();if(navigate)navigate(node);};
            if(!compact){
                const auto place=movable(node.id);
                painted.onPlacementMenu=place?std::function<void()>([this,id=node.id]{showPlacementMenu(id);}):nullptr;
                painted.onOrderKey=place?std::function<bool(const juce::KeyPress&)>([this,id=node.id](const juce::KeyPress& key){return orderKey(id,key);}):nullptr;
                painted.onDragMove=place?std::function<void(juce::Point<int>)>([this,id=node.id](juce::Point<int> at){dragTo(id,at);}):nullptr;
                painted.onDragEnd=place?std::function<void(bool)>([this,id=node.id](bool commit){endDrag(id,commit);}):nullptr;
                if(place)button->setTooltip(button->getTooltip()+"\nDrag within its block to move it (Escape cancels). Right-click: placement menu. Alt + arrows: move. Ctrl + Z: undo.");
            }
            button->setExplicitFocusOrder(int(nextButtons.size())+1);
            nextButtons.push_back(std::move(button));
        }
        buttons=std::move(nextButtons);
        if(compact&&!expandButton){
            expandButton=std::make_unique<SignalPathNavigationButton>("ROUTING");
            expandButton->setComponentID("signalPathExpand");
            expandButton->setName("Routing / inspect complete signal path");
            expandButton->setTitle(expandButton->getName());
            expandButton->setDescription("Inspect Input, Gate, Transpose, Tuner tap, PRE order, amp and cabinet branches, EQ, POST, Doubler, Click and Output. Select a block to open its controls.");
            expandButton->setTooltip("View the complete signal path, including global utilities and rig branches.");
            expandButton->setWantsKeyboardFocus(true);
            expandButton->onClick=[this]{if(expand)expand();};addAndMakeVisible(*expandButton);
        }
        if(expandButton)expandButton->setExplicitFocusOrder(int(buttons.size())+1);
        resized();updateSelection();repaint();
    }
    void resized() override {
        if(compact){
            const int routingWidth=88,gap=10;
            const int available=juce::jmax(0,getWidth()-routingWidth-gap);
            const int count=juce::jmax(1,int(buttons.size()));
            for(size_t i=0;i<buttons.size();++i){
                const int left=int(i)*available/count,right=(int(i)+1)*available/count;
                buttons[i]->setBounds(left,0,juce::jmax(0,right-left-gap),getHeight());
            }
            if(expandButton)expandButton->setBounds(juce::jmax(0,getWidth()-routingWidth),0,juce::jmin(routingWidth,getWidth()),getHeight());
        }
        else for(auto& b:buttons)if(const auto* n=current.find(b->getComponentID().substring(5))){b->setBounds(n->bounds);b->setTransform(juce::AffineTransform::scale(graphScale()));}
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171e20));
        if(compact){g.setColour(juce::Colour(0xff8da99c));for(size_t i=0;i+1<buttons.size();++i){const auto r=buttons[i]->getBounds();g.drawText(">",r.getRight(),0,10,getHeight(),juce::Justification::centred);}return;}
        // Native window managers can constrain the dialog to a smaller display.
        // Fit both connections and hit targets to the same logical canvas.
        g.addTransform(juce::AffineTransform::scale(graphScale()));
        g.setColour(juce::Colour(0xffc8d7cc));g.setFont(juce::FontOptions(13.f));
        g.drawText(current.mode==0?"CLASSIC  /  ONE RIG":current.mode==1?(current.dualCross?"DUAL  /  CROSSOVER":"DUAL  /  PARALLEL BLEND"):"MATRIX  /  CLEAN LOW + WET MID / HIGH",16,5,700,24,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(10.f));g.drawText("Click a block to edit  |  Tab / Shift-Tab  |  Enter / Space",650,8,450,20,juce::Justification::centredRight);
        for(const auto& e:current.edges){
            const auto* a=current.find(e.from);const auto* b=current.find(e.to);if(!a||!b)continue;
            const auto from=a->bounds.toFloat(),to=b->bounds.toFloat();juce::Path path;
            auto start=from.getCentre(),end=to.getCentre();
            if(e.tap && e.to=="gate"){
                start={from.getX(),from.getCentreY()};end={to.getX(),to.getCentreY()};
                path.startNewSubPath(start);path.lineTo(2.f,start.y);path.lineTo(2.f,end.y);path.lineTo(end);
            }else if(e.from=="merge" && e.to=="gate"){
                start={from.getX(),from.getCentreY()};end={to.getRight(),to.getCentreY()};
                path.startNewSubPath(start);path.lineTo(end);
            }else if(e.from.startsWith("level.context")){
                start={from.getRight(),from.getCentreY()};end={to.getRight(),to.getCentreY()};
                path.startNewSubPath(start);path.lineTo(1110.f,start.y);path.lineTo(1110.f,end.y);path.lineTo(end);
            }else if(e.to=="split"){
                start={from.getRight(),from.getCentreY()};end={to.getX(),to.getCentreY()};path.startNewSubPath(start);
                path.lineTo(1128.f,start.y);path.lineTo(1128.f,280.f);path.lineTo(1116.f,280.f);path.startNewSubPath(1104.f,280.f);path.lineTo(6.f,280.f);path.lineTo(6.f,end.y);path.lineTo(end);
            }else if(to.getY()==from.getY() && to.getX()<from.getX()){
                start={from.getX(),from.getCentreY()};end={to.getRight(),to.getCentreY()};path.startNewSubPath(start);path.lineTo(end);
            }else if(to.getX()>from.getX()){start=juce::Point<float>(from.getRight(),from.getCentreY());end=juce::Point<float>(to.getX(),to.getCentreY());path.startNewSubPath(start);const float x=(start.x+end.x)*.5f;path.lineTo(x,start.y);path.lineTo(x,end.y);path.lineTo(end);}
            else {start=juce::Point<float>(from.getCentreX(),from.getBottom());end=juce::Point<float>(to.getCentreX(),to.getY());path.startNewSubPath(start);if(end.y<start.y)end={to.getCentreX(),to.getBottom()};const float y=end.y>start.y?(start.y+end.y)*.5f:juce::jmax(start.y,end.y)+13.f;path.lineTo(start.x,y);path.lineTo(end.x,y);path.lineTo(end);}
            g.setColour(e.tap?juce::Colour(0xff86b5ce):juce::Colour(0xff789480));
            if(e.tap){juce::Path dashed;const float pattern[]{4,4};juce::PathStrokeType(1.4f).createDashedStroke(dashed,path,pattern,2);g.fillPath(dashed);}else g.strokePath(path,juce::PathStrokeType(1.4f));
            const auto before=path.getPointAlongPath(juce::jmax(0.f,path.getLength()-8));g.drawArrow({before,end},1.4f,6,6);
        }
    }
private:
    void parameterChanged(const juce::String&,float) override {triggerAsyncUpdate();}
    void handleAsyncUpdate() override {
        refresh();
        if(selection)select(selection());
    }
    bool orderKey(const juce::String& id,const juce::KeyPress& key) {
        const auto mods=key.getModifiers();
        if(mods.isAltDown() && !mods.isCommandDown()){
            const int code=key.getKeyCode();
            if(code==juce::KeyPress::leftKey||code==juce::KeyPress::upKey){orderCommand(id,OrderCommand::earlier);return true;}
            if(code==juce::KeyPress::rightKey||code==juce::KeyPress::downKey){orderCommand(id,OrderCommand::later);return true;}
            if(code==juce::KeyPress::homeKey){orderCommand(id,OrderCommand::first);return true;}
            if(code==juce::KeyPress::endKey){orderCommand(id,OrderCommand::last);return true;}
        }
        // Nothing to undo or redo: the key goes on to the host and the editor.
        if(mods.isCommandDown() && !mods.isAltDown() && (key.getKeyCode()=='Z' || key.getKeyCode()=='z'))return orderCommand(id,mods.isShiftDown()?OrderCommand::redo:OrderCommand::undo);
        return false;
    }
    void showPlacementMenu(const juce::String& id) {
        const auto place=movable(id);if(!place)return;
        const auto order=processor.requestedOrder();
        const auto& slots=place->region==spectralforge::OrderRegion::tone?std::vector<int>(order.tone.slots.begin(),order.tone.slots.end()):std::vector<int>(order.post.slots.begin(),order.post.slots.end());
        int at=0;for(size_t i=0;i<slots.size();++i)if(slots[i]==place->module)at=int(i);
        const int count=int(slots.size());const auto region=place->region==spectralforge::OrderRegion::tone?juce::String("tone / dynamics block"):juce::String("POST rack");
        juce::PopupMenu menu,positions;
        menu.addSectionHeader(spectralforge::moduleName(place->region,place->module).toUpperCase()+"  ("+juce::String(at+1)+" of "+juce::String(count)+" in the "+region+")");
        menu.addItem(1,"Move earlier",at>0);menu.addItem(2,"Move later",at<count-1);
        menu.addItem(3,"Move to the start",at>0);menu.addItem(4,"Move to the end",at<count-1);
        for(int i=0;i<count;++i)positions.addItem(100+i,juce::String(i+1)+"  "+spectralforge::moduleName(place->region,slots[(size_t)i]),i!=at,i==at);
        menu.addSubMenu("Place at position",positions);
        juce::PopupMenu presets;
        {const auto& list=spectralforge::orderPresets(place->region);
         for(size_t i=0;i<list.size();++i){bool same=true;for(int k=0;k<count;++k)same=same&&slots[(size_t)k]==list[i].slots[(size_t)k];presets.addItem(200+int(i),list[i].name,!same,same);}}
        menu.addSubMenu("Order presets",presets);
        menu.addSeparator();
        menu.addItem(5,"Undo order change",processor.canUndoOrder());menu.addItem(6,"Redo order change",processor.canRedoOrder());
        menu.addItem(7,"Reset the "+region+" order");
        juce::Component::SafePointer<SignalPathView> safe(this);
        auto* target=buttonFor(id);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),[safe,id](int result){
            if(!safe||result==0)return;
            if(result>=200){safe->orderCommand(id,OrderCommand::preset,result-200);return;}
            if(result>=100){safe->orderCommand(id,OrderCommand::position,result-100);return;}
            static constexpr std::array<OrderCommand,7> commands{OrderCommand::earlier,OrderCommand::later,OrderCommand::first,OrderCommand::last,OrderCommand::undo,OrderCommand::redo,OrderCommand::reset};
            if(result>=1 && result<=7)safe->orderCommand(id,commands[(size_t)result-1]);
        });
    }
    float graphScale() const {return juce::jmin(float(juce::jmax(1,getWidth()))/1140.f,float(juce::jmax(1,getHeight()))/650.f);}
    void timerCallback() override {if(spectralforge::ui::visible(*this)){refresh();if(selection)select(selection());}}
    void updateSelection(){
        auto editingStage=selected;
        // A mode/board restore may remove the selected detail node. Retain its
        // editor family until a new selection arrives, as the old strip did.
        if(compact){
            if(selected.startsWith("pre."))editingStage="pre";
            else if(selected.startsWith("post."))editingStage="post";
            else if(selected.startsWith("cab."))editingStage="cab";
            else if(selected=="rigs"||selected.startsWith("amp.")||selected.startsWith("low.")||selected.startsWith("tone.context")||selected.startsWith("level.context"))editingStage="head";
        }
        if(compact)if(const auto* node=current.find(selected)){
            using Target=spectralforge::signalPath::Target;
            switch(node->target){
                case Target::pre:editingStage="pre";break;
                case Target::rigs:case Target::amp:case Target::low:editingStage="head";break;
                case Target::cab:editingStage="cab";break;
                case Target::post:editingStage="post";break;
                case Target::synth:editingStage="synth";break;
                case Target::dynamics:editingStage=selected=="output.limiter"?"final.eq":"tone.eq";break;
                default:break;
            }
        }
        for(auto& b:buttons){
            const auto id=b->getComponentID().substring(5);
            const bool grouped=!compact&&((selected=="rigs"&&id=="split")||(selected=="head"&&id.startsWith("amp.context"))||(selected=="cab"&&id.startsWith("cab.context"))||(selected=="pre"&&id.startsWith("pre."))||(selected=="post"&&id.startsWith("post.")));
            b->setToggleState(id==(compact?editingStage:selected)||grouped,juce::dontSendNotification);
        }
    }
    // Restore/automation notifications must refresh topology even before the
    // editor timer or ComboBox attachments deliver their presentation update.
    static constexpr std::array<const char*,4> routingParameters{{"gateAfterRig","expAfterRig","mode","dualtype"}};
    ChimeraProcessor& processor;bool compact;juce::String selected;
    juce::String dragId;juce::Point<int> dragPoint;int dropSlot{-1};
    spectralforge::signalPath::Snapshot current;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    std::unique_ptr<juce::TextButton> expandButton;
};

// Non-modal inspection: navigation leaves underlying editors immediately usable.
class SignalPathWindow final : public juce::DialogWindow {
public:
    explicit SignalPathWindow(SignalPathView* view,juce::Component* owner)
        :juce::DialogWindow("Chimera / Signal Path",juce::Colour(0xff171e20),true) {
        setUsingNativeTitleBar(true);setContentOwned(view,true);setResizable(false,false);
        centreAroundComponent(owner,getWidth(),getHeight());setVisible(true);
    }
    void closeButtonPressed() override {delete this;}
};

// Inspect the tuner without turning it on (which could mute the audible path).
class SignalPathTunerPanel final : public juce::Component,private juce::Timer {
public:
    explicit SignalPathTunerPanel(ChimeraProcessor& p):processor(p) {
        setComponentID("signalPathTuner");setSize(430,150);
        reference.setSliderStyle(juce::Slider::LinearHorizontal);reference.setTextBoxStyle(juce::Slider::TextBoxRight,false,70,24);reference.setTextValueSuffix(" Hz");reference.setName("Tuner A4 reference");
        enabled.setClickingTogglesState(true);mute.setClickingTogglesState(true);
        for(juce::Component* c:std::initializer_list<juce::Component*>{&reference,&enabled,&mute,&reading})addAndMakeVisible(c);
        ref=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters(),"tunerref",reference);
        on=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters(),"tuneron",enabled);
        silence=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters(),"tunermute",mute);
        reference.setBounds(20,20,390,30);enabled.setBounds(20,60,150,28);mute.setBounds(180,60,230,28);reading.setBounds(20,98,390,36);
        timerCallback();startTimerHz(25);
    }
    ~SignalPathTunerPanel() override {stopTimer();}
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff171e20));}
private:
    void timerCallback() override {if(isVisible())reading.setText(enabled.getToggleState()?juce::String(processor.tuningFrequency(),1)+" Hz / confidence "+juce::String(processor.tuningConfidence(),2):"Tuner is off. Viewing does not enable or mute.",juce::dontSendNotification);}
    ChimeraProcessor& processor;juce::Slider reference;juce::TextButton enabled{"TUNER ON"},mute{"MUTE WHILE TUNING"};juce::Label reading;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ref;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> on,silence;
};
