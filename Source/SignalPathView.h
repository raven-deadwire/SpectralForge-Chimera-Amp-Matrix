#pragma once
#include "SignalPath.h"
#include "UIRefresh.h"

// Native buttons provide focus/Return/Space navigation and accessibility names.
// This component has no APVTS attachments and cannot mutate audio state.
class SignalPathButton final : public juce::TextButton {
public:
    bool compact{},active{};juce::String label,status;
    void paintButton(juce::Graphics& g,bool hover,bool down) override {
        const auto area=getLocalBounds().toFloat().reduced(.5f);
        g.setColour(getToggleState()?juce::Colour(0xff416953):juce::Colour(hover||down?0xff2f4540:0xff222c30));g.fillRoundedRectangle(area,3);
        g.setColour(hasKeyboardFocus(true)?juce::Colours::white:juce::Colour(getToggleState()?0xff9bc0a8:0xff53675c));g.drawRoundedRectangle(area,3,1);
        g.setColour(active?juce::Colour(0xffe8e8dc):juce::Colour(0xffacb6b2));g.setFont(juce::FontOptions(compact?9.5f:11.f));
        g.drawFittedText(label,getLocalBounds().reduced(4).withHeight(compact?getHeight()-8:22),juce::Justification::centred,1);
        if(!compact){g.setFont(juce::FontOptions(9.f));g.drawFittedText(status,4,25,getWidth()-8,17,juce::Justification::centred,1);}
    }
};
class SignalPathView final : public juce::Component,private juce::Timer {
public:
    SignalPathView(ChimeraProcessor& p,bool compactView):processor(p),compact(compactView) {
        setComponentID(compact?"signalPathStrip":"signalPathGraph");
        if(!compact){setSize(1140,650);startTimerHz(25);}refresh();
    }
    ~SignalPathView() override {stopTimer();}
    std::function<void(const spectralforge::signalPath::Node&)> navigate;
    std::function<void()> expand;
    std::function<juce::String()> selection;
    const spectralforge::signalPath::Snapshot& snapshot() const {return current;}
    void select(const juce::String& id){if(selected!=id){selected=id;updateSelection();}}
    void refresh() {
        auto next=spectralforge::signalPath::read(processor);
        if(next==current)return;
        current=std::move(next);
        // Preserve existing buttons (and keyboard focus) across automation.
        std::vector<spectralforge::signalPath::Node> visible;
        if(compact){
            using namespace spectralforge::signalPath;
            const auto add=[&](const juce::String& id){if(const auto* n=current.find(id))visible.push_back(*n);};
            add("input");if(!processor.parameters().getRawParameterValue("gateAfterRig")->load())add("gate");add("transpose");
            bool preActive=false;for(const auto& n:current.nodes)if(n.id.startsWith("pre."))preActive|=n.active;
            visible.push_back({"pre","PRE",preActive?"ACTIVE":"BYPASSED",{},Target::pre,-1,-1,preActive});
            visible.push_back({"rigs",current.mode==0?"CLASSIC":current.mode==1?"DUAL":"MATRIX","",{},Target::rigs});
            if(processor.parameters().getRawParameterValue("gateAfterRig")->load()>.5f)add("gate");
            add("tone.eq");
            bool postActive=false;for(const auto& n:current.nodes)if(n.target==Target::post)postActive|=n.active;
            visible.push_back({"post","POST",postActive?"ACTIVE":"BYPASSED",{},Target::post,-1,-1,postActive});
            add("utilities");add("final.eq");add("output");
        }else visible=current.nodes;
        std::vector<std::unique_ptr<juce::TextButton>> nextButtons;
        for(const auto& node:visible) {
            std::unique_ptr<juce::TextButton> button;
            for(auto& old:buttons)if(old && old->getComponentID()=="path."+node.id){button=std::move(old);break;}
            if(!button){button=std::make_unique<SignalPathButton>();addAndMakeVisible(*button);}
            auto& painted=static_cast<SignalPathButton&>(*button);painted.compact=compact;painted.active=node.active;painted.label=node.label;painted.status=node.status;
            button->setComponentID("path."+node.id);
            button->setButtonText(compact?node.label:node.label+"\n"+node.status);
            button->setName(node.label+" / "+node.status+" / open editor");
            button->setTooltip(node.label+" | "+node.status+"\n"+node.detail+"\nNavigate only; sound is unchanged.");
            button->setWantsKeyboardFocus(true);
            button->setColour(juce::TextButton::textColourOffId,node.active?juce::Colour(0xffe5e1d4):juce::Colour(0xff9caaa5));
            button->onClick=[this,node]{selected=node.id;updateSelection();if(navigate)navigate(node);};
            button->setExplicitFocusOrder(int(nextButtons.size())+1);
            nextButtons.push_back(std::move(button));
        }
        buttons=std::move(nextButtons);
        if(compact&&!expandButton){expandButton=std::make_unique<juce::TextButton>("PATH +");expandButton->setComponentID("signalPathExpand");expandButton->setName("Expand actual signal path");expandButton->setWantsKeyboardFocus(true);expandButton->onClick=[this]{if(expand)expand();};addAndMakeVisible(*expandButton);}
        resized();updateSelection();repaint();
    }
    void resized() override {
        if(compact){const int width=(getWidth()-76)/juce::jmax(1,int(buttons.size()));for(size_t i=0;i<buttons.size();++i)buttons[i]->setBounds(int(i)*width,0,width-10,getHeight());if(expandButton)expandButton->setBounds(getWidth()-74,0,74,getHeight());}
        else for(auto& b:buttons)if(const auto* n=current.find(b->getComponentID().substring(5)))b->setBounds(n->bounds);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171e20));
        if(compact){g.setColour(juce::Colour(0xff8da99c));for(size_t i=0;i+1<buttons.size();++i){const auto r=buttons[i]->getBounds();g.drawText(">",r.getRight(),0,10,getHeight(),juce::Justification::centred);}return;}
        g.setColour(juce::Colour(0xffc8d7cc));g.setFont(juce::FontOptions(13.f));
        g.drawText(current.mode==0?"CLASSIC  /  ONE RIG":current.mode==1?(current.dualCross?"DUAL  /  CROSSOVER":"DUAL  /  PARALLEL BLEND"):"MATRIX  /  CLEAN LOW + WET MID / HIGH",16,5,700,24,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(10.f));g.drawText("Click a block to edit  |  Tab / Shift-Tab + Enter  |  Navigation only",650,8,450,20,juce::Justification::centredRight);
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
            }else if(to.getX()>from.getX()){start=juce::Point<float>(from.getRight(),from.getCentreY());end=juce::Point<float>(to.getX(),to.getCentreY());path.startNewSubPath(start);const float x=(start.x+end.x)*.5f;path.lineTo(x,start.y);path.lineTo(x,end.y);path.lineTo(end);}
            else {start=juce::Point<float>(from.getCentreX(),from.getBottom());end=juce::Point<float>(to.getCentreX(),to.getY());path.startNewSubPath(start);if(end.y<start.y)end={to.getCentreX(),to.getBottom()};const float y=end.y>start.y?(start.y+end.y)*.5f:juce::jmax(start.y,end.y)+13.f;path.lineTo(start.x,y);path.lineTo(end.x,y);path.lineTo(end);}
            g.setColour(e.tap?juce::Colour(0xff86b5ce):juce::Colour(0xff789480));
            if(e.tap){juce::Path dashed;const float pattern[]{4,4};juce::PathStrokeType(1.4f).createDashedStroke(dashed,path,pattern,2);g.fillPath(dashed);}else g.strokePath(path,juce::PathStrokeType(1.4f));
            const auto before=path.getPointAlongPath(juce::jmax(0.f,path.getLength()-8));g.drawArrow({before,end},1.4f,6,6);
        }
    }
private:
    void timerCallback() override {if(spectralforge::ui::visible(*this)){refresh();if(selection)select(selection());}}
    void updateSelection(){for(auto& b:buttons){const auto id=b->getComponentID().substring(5);b->setToggleState(id==selected || (!compact&&((selected=="rigs"&&id=="split")||(selected=="pre"&&id.startsWith("pre."))||(selected=="post"&&id.startsWith("post.")))) || (compact&&((id=="pre"&&selected.startsWith("pre."))||(id=="rigs"&&(selected.startsWith("amp.")||selected.startsWith("cab.")||selected.startsWith("low.")))||(id=="post"&&selected.startsWith("post.")))),juce::dontSendNotification);}}
    ChimeraProcessor& processor;bool compact;juce::String selected;
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
