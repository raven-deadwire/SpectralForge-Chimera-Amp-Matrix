#pragma once
#include "AmpHeadLayout.h"
#include "AmpNativePanel.h"
#include "CabHeadView.h"

// No parameter ownership: every face control attaches to the existing bank.
class AmpHeadControlView final : public juce::Component {
    ChimeraProcessor& processor;
    const int lane;
    const bool enlarged;
    juce::SharedResourcePointer<spectralforge::cabHead::Bank> artwork;
    std::vector<std::unique_ptr<NativeControlView>> controls;
    std::vector<spectralforge::ampHead::Knob> anchors;
    class ChannelButton : public juce::TextButton {
    public:
        void paintButton(juce::Graphics& g,bool over,bool down) override {
            auto face=getLocalBounds().toFloat().withSizeKeepingCentre(float(getWidth()-2),18.f);
            g.setColour(juce::Colour(getToggleState()?0xff507d99:0xff171d20).brighter(over||down?.15f:0.f));g.fillRoundedRectangle(face,2.f);
            g.setColour(juce::Colour(hasKeyboardFocus(true)?0xffb3e1ff:0xffa99878));g.drawRoundedRectangle(face,2.f,1.f);
            g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(10.f));g.drawText(getButtonText(),face.toNearestInt(),juce::Justification::centred);
        }
    };
    std::array<ChannelButton,5> channels;
    StableAmpComboBox input;
    class OpenButton : public juce::TextButton {
    public:
        OpenButton():juce::TextButton("OPEN AMP") {}
        void paintButton(juce::Graphics& g,bool over,bool) override {
            if(over || hasKeyboardFocus(true)) {
                g.setColour(juce::Colour(0xffead4a4));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.f),3.f,2.f);
                g.setFont(juce::FontOptions(12.f,juce::Font::bold));g.drawText("OPEN AMP",getLocalBounds().withHeight(22),juce::Justification::centred);
            }
        }
    } focus;
    int context{-1},model{-1},channel{-1},mode{-1},route{-1};
    uint64_t revision{};
    bool active{true};
    struct FaceLook : spectralforge::art::DialogLook {
        juce::Font getLabelFont(juce::Label&) override {return juce::FontOptions(10.f);}
        void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float value,float start,float end,juce::Slider& s) override {
            const auto b=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).withSizeKeepingCentre(float(juce::jmin(w,h)-2),float(juce::jmin(w,h)-2));
            const bool gold=int(s.getProperties()["knobStyle"])==3;
            g.setColour(juce::Colour(0xff060708));g.fillEllipse(b);
            auto disk=b.reduced(3.f);
            g.setGradientFill({juce::Colour(gold?0xfff4d38b:0xffe4e8e8),disk.getTopLeft(),juce::Colour(gold?0xff755525:0xff565e64),disk.getBottomRight(),false});g.fillEllipse(disk);
            g.setColour(juce::Colour(0xff101316));g.drawEllipse(disk,1.f);
            const float angle=start+value*(end-start),radius=disk.getWidth()*.42f;
            const auto c=disk.getCentre();
            g.setColour(juce::Colours::black);g.drawLine(c.x+std::sin(angle)*radius*.28f,c.y-std::cos(angle)*radius*.28f,c.x+std::sin(angle)*radius,c.y-std::cos(angle)*radius,2.3f);
            if(s.hasKeyboardFocus(true)){g.setColour(juce::Colour(0xff94d4f0));g.drawEllipse(b,1.5f);}
        }
    } look;
    bool bindingCurrent() const {
        const int now=int(processor.parameters().getRawParameterValue("mode")->load());
        return isShowing() && active && now==mode && processor.selectedAmpModel(lane)==model
            && processor.selectedAmpChannel(lane)==channel && processor.ampHeadRecallRevision()==revision;
    }
public:
    std::function<void()> onOpen;
    AmpHeadControlView(ChimeraProcessor& p,int index,bool full):processor(p),lane(index),enlarged(full) {
        setComponentID("ampHead"+juce::String(lane+1));setLookAndFeel(&look);
        addChildComponent(focus);focus.setComponentID("ampHeadOpen"+juce::String(lane+1));
        focus.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        focus.setTooltip("Enlarge the amplifier and operate its faceplate controls.");
        focus.onClick=[this]{finishInteraction();if(onOpen)onOpen();};
        addChildComponent(input);input.setComponentID("ampHeadInput"+juce::String(lane+1));input.setName("Amplifier input route");
        input.onChange=[this]{if(!bindingCurrent())return;finishInteraction();input.acceptSelection();processor.setAmpNativeRoute(lane,input.getSelectedId()-1);};
        for(int c=0;c<5;++c){auto& button=channels[size_t(c)];addChildComponent(button);button.setComponentID("ampHeadChannel"+juce::String(lane+1)+"_"+juce::String(c));
            button.onClick=[this,c]{if(!bindingCurrent())return;finishInteraction();processor.setAmpChannel(lane,c);refresh();};}
        refresh();
    }
    ~AmpHeadControlView() override {finishInteraction();controls.clear();setLookAndFeel(nullptr);}
    void finishInteraction() {for(auto& c:controls)c->finishInteraction();}
    void visibilityChanged() override {if(!isShowing())finishInteraction();}
    int displayedContext() const {return context;}
    int displayedModel() const {return model;}
    size_t controlCount() const {return controls.size();}
    juce::Rectangle<float> headBounds() const {
        auto area=getLocalBounds().toFloat().reduced(enlarged?12.f:0.f);
        const auto& d=spectralforge::cabPhysical::head(juce::jmax(0,model));
        const float ratio=spectralforge::cabHead::projectedHeight(juce::jmax(0,model))/d.width;
        const float width=juce::jmin(area.getWidth(),area.getHeight()/ratio);
        return area.withSizeKeepingCentre(width,width*ratio);
    }
    void refresh() {
        const int nextMode=int(processor.parameters().getRawParameterValue("mode")->load());
        const int nextModel=processor.selectedAmpModel(lane),nextChannel=processor.selectedAmpChannel(lane);
        const auto nextRevision=processor.ampHeadRecallRevision();
        const bool nextActive=lane<=nextMode;
        if(mode!=nextMode || model!=nextModel || channel!=nextChannel || revision!=nextRevision || active!=nextActive) {
            finishInteraction();controls.clear();mode=nextMode;model=nextModel;channel=nextChannel;revision=nextRevision;active=nextActive;
            context=spectralforge::ampNativeContext(mode,lane);anchors=spectralforge::ampHead::knobs(model);
            input.clear(juce::dontSendNotification);const auto& panel=spectralforge::ampNativePanel(model);
            for(size_t i=0;i<panel.routes.size();++i)input.addItem(panel.routes[i],int(i)+1);
            input.resetSyncExplicit(processor.selectedAmpNativeRoute(lane)+1);
            if(enlarged && active)for(const auto& anchor:anchors) {
                const int index=spectralforge::ampHead::controlIndex(model,anchor.key);jassert(index>=0);if(index<0)continue;
                const auto& spec=panel.controls[size_t(index)];auto control=std::make_unique<NativeControlView>();
                control->bind(processor.parameters(),spectralforge::ampNativeControlID(context,model,index,channel),anchor.label,"AMP FACE",0,{},spec.minimum,spec.maximum,.001,spectralforge::art::headStyle(model).knobStyle);
                control->group.setVisible(false);control->label.setFont(juce::FontOptions(9.f));
                const auto ink=juce::Colour(model==1?0xff191b18:0xfff1ece0);
                control->label.setColour(juce::Label::textColourId,ink);
                control->slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,64,13);
                control->slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
                control->slider.setColour(juce::Slider::textBoxTextColourId,ink);
                control->slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
                control->slider.setScrollWheelEnabled(false);control->slider.setWantsKeyboardFocus(true);
                control->slider.setVelocityModeParameters(.2,1,0.,true,juce::ModifierKeys::shiftModifier);
                if(model==spectralforge::niflheimrAmpModel && index==int(spectralforge::niflheimr::Control::midFrequency)) {
                    control->slider.setSkewFactorFromMidPoint(650);control->slider.setTextValueSuffix(" Hz");control->slider.setNumDecimalPlacesToDisplay(0);
                } else if(model==spectralforge::niflheimrAmpModel && index==int(spectralforge::niflheimr::Control::blend)) {
                    control->slider.textFromValueFunction=[](double v){return juce::String(juce::roundToInt(v*100))+"%";};
                    control->slider.valueFromTextFunction=[](const juce::String& v){return v.getDoubleValue()/100.;};control->slider.updateText();
                } else control->showHardwarePosition();
                control->slider.setDescription(contextName()+" / "+panel.channels[size_t(channel)]+" / "+anchor.label);
                // Captured identities reject any late pointer event before the
                // next UI tick, including same-model preset/A-B restoration.
                const auto boundRevision=revision;const int boundContext=context,boundModel=model,boundChannel=channel;
                control->canEdit=[this,boundRevision,boundContext,boundModel,boundChannel]{return bindingCurrent() && revision==boundRevision && context==boundContext && model==boundModel && channel==boundChannel;};
                control->activate=[this]{processor.activateNativeAmp(lane);};
                addAndMakeVisible(*control);controls.push_back(std::move(control));
            }
            for(int c=0;c<5;++c){auto& button=channels[size_t(c)];button.setVisible(enlarged && active && model==spectralforge::niflheimrAmpModel);
                button.setButtonText(juce::String(c+1));button.setToggleState(c==channel,juce::dontSendNotification);
                if(model==spectralforge::niflheimrAmpModel){button.setName(panel.channels[size_t(c)]);button.setTooltip(panel.channels[size_t(c)]);}}
            input.setVisible(enlarged && active && model==1);
            focus.setVisible(!enlarged);resized();repaint();
        }
        const int nextRoute=processor.selectedAmpNativeRoute(lane);
        if(route!=nextRoute){route=nextRoute;input.syncSelectedId(route+1);}
    }
    juce::String contextName() const {return mode==0?"CLASSIC":mode==1?"DUAL / RIG "+juce::String(lane+1):"MATRIX / "+juce::String(lane==0?"LOW":lane==1?"MID":"HIGH");}
    juce::String caption() const {return contextName()+" / "+juce::String::fromUTF8(spectralforge::ampInfo(model).name)+" / "+spectralforge::ampNativePanel(model).channels[size_t(channel)];}
    void resized() override {
        focus.setBounds(headBounds().toNearestInt());
        const auto face=spectralforge::cabHead::geometry(headBounds(),juce::jmax(0,model)).front;
        for(size_t i=0;i<controls.size();++i) {
            const auto& anchor=anchors[i];auto& c=*controls[i];
            const int diameter=juce::jmax(32,juce::roundToInt(face.getWidth()*(model==1?.046f:.029f)));
            const int cellWidth=juce::roundToInt(face.getWidth()*(model==1?.10f:.067f));
            const int h=diameter+25;
            c.slider.setTextBoxStyle(model==1?juce::Slider::TextBoxBelow:juce::Slider::TextBoxRight,false,model==1?64:juce::jmax(29,cellWidth-diameter-2),model==1?13:18);
            c.setBounds(juce::roundToInt(face.getX()+face.getWidth()*anchor.x)-(model==1?cellWidth/2:diameter/2),juce::roundToInt(face.getY()+face.getHeight()*anchor.y)-diameter/2-11,cellWidth,h);
            c.group.setVisible(false);c.label.setBounds(0,0,cellWidth,11);c.slider.setBounds(0,11,cellWidth,diameter+(model==1?14:0));
        }
        for(int c=0;c<5;++c)channels[size_t(c)].setBounds(juce::roundToInt(face.getX()+face.getWidth()*(.409f+c*.0385f)),juce::roundToInt(face.getY()+face.getHeight()*.588f)-16,juce::jmax(32,juce::roundToInt(face.getWidth()*.03f)),32);
        input.setBounds(juce::roundToInt(face.getX()+face.getWidth()*.80f),juce::roundToInt(face.getY()+face.getHeight()*.69f),juce::roundToInt(face.getWidth()*.17f),28);
    }
    void paint(juce::Graphics& g) override {
        spectralforge::cabHead::paint(g,headBounds(),juce::jmax(0,model));
        const auto face=spectralforge::cabHead::geometry(headBounds(),juce::jmax(0,model)).front;
        if(enlarged && active && model==spectralforge::niflheimrAmpModel) {
            // Replace only the baked knob disks with adjacent clean metal from
            // the SAME row of the original fascia. Keep all ornate trim intact.
            const auto& image=artwork->faces[size_t(model)].front;
            for(const auto& a:anchors){const float w=face.getWidth()*.046f,h=face.getHeight()*.139f;
                const auto patch=juce::Rectangle<float>(face.getX()+face.getWidth()*a.x-w/2,face.getY()+face.getHeight()*a.y-h/2,w,h);
                const juce::Graphics::ScopedSaveState saved(g);juce::Path mask;mask.addEllipse(patch);g.reduceClipRegion(mask);
                if(image.isValid())g.drawImage(image,patch.getX(),patch.getY(),patch.getWidth(),patch.getHeight(),int(image.getWidth()*.73f),int(image.getHeight()*(a.y-.0695f)),int(image.getWidth()*.046f),int(image.getHeight()*.139f));}
        }

    }
};

class AmpHeadWorkspace final : public juce::Component, private juce::Timer {
    spectralforge::art::DialogLook look;
    ChimeraProcessor& processor;int lane;
    AmpHeadControlView head;
    AmpNativePanel details;
    juce::Label title,hint;
    juce::TextButton back{"< BACK"},detailToggle{"DETAILS"};
    bool showDetails{};
    void timerCallback() override {refresh();}
public:
    AmpHeadWorkspace(ChimeraProcessor& p,int index):processor(p),lane(index),head(p,index,true),details(p,index,true) {
        setLookAndFeel(&look);setComponentID("ampHeadWorkspace");
        for(auto* c:std::initializer_list<juce::Component*>{&head,&details,&title,&hint,&back,&detailToggle})addAndMakeVisible(*c);
        back.setComponentID("ampHeadBack");back.onClick=[this]{head.finishInteraction();if(auto* w=findParentComponentOfClass<juce::DialogWindow>())w->exitModalState(0);};
        detailToggle.setComponentID("ampHeadDetails");detailToggle.onClick=[this]{head.finishInteraction();showDetails=!showDetails;refresh();};
        title.setFont(juce::FontOptions(15.f,juce::Font::bold));hint.setFont(juce::FontOptions(12.f));
        setSize(1200,650);refresh();startTimerHz(25);
    }
    ~AmpHeadWorkspace() override {stopTimer();head.finishInteraction();setLookAndFeel(nullptr);}
    void refresh() {
        head.refresh();const bool active=lane<=int(processor.parameters().getRawParameterValue("mode")->load());
        const bool supported=spectralforge::ampHead::supported(head.displayedModel());
        details.setVisible(active && (showDetails||!supported));head.setVisible(active && !details.isVisible());
        detailToggle.setVisible(active && supported);detailToggle.setButtonText(showDetails?"HEAD":"DETAILS");
        if(details.isVisible())details.refreshIfNeeded();
        title.setText(active?head.caption():"This rig is inactive in the current routing mode",juce::dontSendNotification);
        hint.setText(details.isVisible()?"Full amplifier controls":supported?"Drag a knob | Shift for fine adjustment | Click a value for precise entry":"",juce::dontSendNotification);
    }
    void resized() override {back.setBounds(16,12,86,30);detailToggle.setBounds(getWidth()-112,12,96,30);title.setBounds(116,12,getWidth()-240,30);hint.setBounds(20,getHeight()-31,getWidth()-40,22);head.setBounds(12,52,getWidth()-24,getHeight()-92);details.setBounds(24,62,getWidth()-48,getHeight()-108);}
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff111617));}
};
