#pragma once
#include "AmpHeadLayout.h"
#include "AmpHeadKnobLook.h"
#include "AmpNativePanel.h"
#include "CabHeadView.h"

// No parameter ownership: every face control attaches to the existing bank.
class AmpHeadControlView final : public juce::Component {
    ChimeraProcessor& processor;
    const int lane;
    const bool enlarged;
    juce::SharedResourcePointer<spectralforge::cabHead::Bank> artwork;
    std::vector<std::unique_ptr<NativeControlView>> controls,optionControls;
    std::vector<spectralforge::ampHead::Knob> anchors,optionAnchors;
    juce::Image cleanControlField;
    int cleanModel{-1};
    class ChannelButton : public juce::TextButton {
        float faceWidth{28.f},faceHeight{18.f};
    public:
        void setFaceSize(float w,float h) {faceWidth=w;faceHeight=h;}
        void paintButton(juce::Graphics& g,bool over,bool down) override {
            auto face=getLocalBounds().toFloat().withSizeKeepingCentre(faceWidth,faceHeight);
            const auto colour=juce::Colour(getToggleState()?0xff507d99:0xff171d20).brighter(over||down?.15f:0.f);
            g.setGradientFill(juce::ColourGradient(colour.brighter(.12f),face.getTopLeft(),colour.darker(.3f),face.getBottomLeft(),false));g.fillRoundedRectangle(face,2.f);
            g.setColour(juce::Colour(hasKeyboardFocus(true)?0xffb3e1ff:0xffa99878));g.drawRoundedRectangle(face,2.f,1.f);
            g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(12.f,juce::Font::bold));g.drawText(getButtonText(),face.toNearestInt(),juce::Justification::centred);
        }
    };
    std::array<std::unique_ptr<ChannelButton>,5> channels;
    std::unique_ptr<StableAmpComboBox> input;
    juce::Label inputLabel;
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
        spectralforge::ampHeadKnob::Renderer knobRenderer;
        juce::Font getLabelFont(juce::Label& label) override {return label.getFont();}
        void drawLabel(juce::Graphics& g,juce::Label& label) override {
            if(!bool(label.getProperties()["headFaceLabel"])) {spectralforge::art::DialogLook::drawLabel(g,label);return;}
            const auto ink=label.findColour(juce::Label::textColourId);
            g.setFont(label.getFont());
            g.setColour((ink.getPerceivedBrightness()>.5f?juce::Colours::black:juce::Colours::white).withAlpha(.48f));
            g.drawText(label.getText(),label.getLocalBounds().translated(0,1),juce::Justification::centred,false);
            g.setColour(ink);g.drawText(label.getText(),label.getLocalBounds(),juce::Justification::centred,false);
        }
        juce::Label* createSliderTextBox(juce::Slider& slider) override {
            auto* value=spectralforge::art::DialogLook::createSliderTextBox(slider);
            value->setBorderSize(juce::BorderSize<int>(0));
            value->setFont(juce::FontOptions(float(slider.getProperties().getWithDefault("headFontSize",12.f)),juce::Font::bold));
            // The brass face uses dark idle ink, but JUCE's editor defaults can
            // combine a pale face background with white selected text. Give
            // numeric entry an explicit, high-contrast dark readout on both heads.
            value->setColour(juce::Label::textWhenEditingColourId,juce::Colour(0xfff1ece0));
            value->setColour(juce::Label::backgroundWhenEditingColourId,juce::Colour(0xff101718));
            value->setColour(juce::Label::outlineWhenEditingColourId,juce::Colour(0xffbfa377));
            value->setColour(juce::TextEditor::highlightColourId,juce::Colour(0xff4d6572));
            value->setColour(juce::TextEditor::highlightedTextColourId,juce::Colour(0xfffff5df));
            if(auto* head=dynamic_cast<NativeControlSlider*>(&slider)) {
                value->onEditorShow=[head,value]{head->readoutEditorShown();if(auto* editor=value->getCurrentTextEditor()){editor->setBorder(juce::BorderSize<int>(0));editor->setIndents(2,0);editor->setJustification(juce::Justification::centred);}};
                value->onEditorHide=[head]{head->readoutEditorHidden();};
            }
            return value;
        }
        juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override {
            if(!bool(slider.getProperties()["headControl"]))return spectralforge::art::DialogLook::getSliderLayout(slider);
            juce::Slider::SliderLayout layout;
            const int labelHeight=slider.getTextBoxHeight();
            const int diameter=juce::jmin(slider.getWidth(),slider.getHeight()-labelHeight);
            layout.textBoxBounds={0,0,slider.getWidth(),labelHeight};
            layout.sliderBounds={(slider.getWidth()-diameter)/2,labelHeight,diameter,diameter};
            return layout;
        }
        void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float value,float start,float end,juce::Slider& s) override {
            const auto bounds=juce::Rectangle<float>(float(x),float(y),float(w),float(h))
                .withSizeKeepingCentre(float(juce::jmin(w,h)-2),float(juce::jmin(w,h)-2));
            knobRenderer.draw(g,bounds,value,start,end,int(s.getProperties()["headModel"]),s.hasKeyboardFocus(true));
        }
    } look;
    void rebuildInput() {
        // Destroy the previous ComboBox and its SafePointer-backed popup/
        // pending change notification before attaching a different bank.
        input=std::make_unique<StableAmpComboBox>();addChildComponent(*input);
        input->setComponentID("ampHeadInput"+juce::String(lane+1));input->setName("Amplifier input route");
        input->onChange=[this]{if(!bindingCurrent())return;finishInteraction();input->acceptSelection();processor.setAmpNativeRoute(lane,input->getSelectedId()-1);};
    }
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
        addChildComponent(inputLabel);inputLabel.setText("INPUT ROUTE",juce::dontSendNotification);
        inputLabel.setFont(juce::FontOptions(12.f,juce::Font::bold));inputLabel.setJustificationType(juce::Justification::centred);
        inputLabel.setColour(juce::Label::textColourId,juce::Colour(0xff191b18));


        refresh();
    }
    ~AmpHeadControlView() override {finishInteraction();controls.clear();optionControls.clear();setLookAndFeel(nullptr);}
    void finishInteraction() {for(auto& c:controls)c->finishInteraction();for(auto& c:optionControls)c->finishInteraction();if(input)input->hidePopup();}
    void visibilityChanged() override {if(!isShowing())finishInteraction();}
    int displayedContext() const {return context;}
    int displayedModel() const {return model;}
    int displayedChannel() const {return channel;}
    uint64_t displayedRecallRevision() const {return revision;}
    void selectChannel(int selected) {
        if(!bindingCurrent() || selected<0 || selected>=int(spectralforge::ampNativePanel(model).channels.size()))return;
        finishInteraction();processor.setAmpChannel(lane,selected);refresh();
    }
    size_t controlCount() const {return controls.size()+optionControls.size();}
    bool routeOnFace() const {return model==int(spectralforge::AmpModel::britEdge);}
    int auxiliaryColumns() const {return juce::jmax(1,(getWidth()-24)/180);}
    int auxiliarySlots() const {return int(optionControls.size())+((input && input->isVisible()&&!routeOnFace())?1:0);}
    int auxiliaryHeight() const {return enlarged && auxiliarySlots()>0 ? ((auxiliarySlots()+auxiliaryColumns()-1)/auxiliaryColumns())*56+8 : 0;}
    juce::Rectangle<float> headBounds() const {
        auto area=getLocalBounds().toFloat().reduced(enlarged?12.f:0.f);
        if(enlarged)area.removeFromBottom(float(auxiliaryHeight()));
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
            finishInteraction();controls.clear();optionControls.clear();mode=nextMode;model=nextModel;channel=nextChannel;revision=nextRevision;active=nextActive;
            context=spectralforge::ampNativeContext(mode,lane);anchors=spectralforge::ampHead::knobs(model,channel);optionAnchors=spectralforge::ampHead::switches(model,channel);
            rebuildInput();input->clear(juce::dontSendNotification);const auto& panel=spectralforge::ampNativePanel(model);
            for(size_t i=0;i<panel.routes.size();++i)input->addItem(juce::String::fromUTF8(panel.routes[i]),int(i)+1);
            input->resetSyncExplicit(processor.selectedAmpNativeRoute(lane)+1);
            if(enlarged && active)for(const auto& anchor:anchors) {
                const int index=spectralforge::ampHead::controlIndex(model,anchor.key);jassert(index>=0);if(index<0)continue;
                const auto& spec=panel.controls[size_t(index)];auto control=std::make_unique<NativeControlView>();
                control->bind(processor.parameters(),spectralforge::ampNativeControlID(context,model,index,channel),anchor.label,"AMP FACE",0,{},spec.minimum,spec.maximum,.001,spectralforge::art::headStyle(model).knobStyle);
                control->group.setVisible(false);control->label.setFont(juce::FontOptions(12.f,juce::Font::bold));control->label.setBorderSize(juce::BorderSize<int>(0));
                const auto ink=juce::Colour(spectralforge::ampHead::faceStyle(model).ink);
                control->label.setColour(juce::Label::textColourId,ink);control->label.getProperties().set("headFaceLabel",true);
                control->slider.getProperties().set("headControl",true);
                control->slider.getProperties().set("headModel",model);
                control->slider.setTextBoxStyle(juce::Slider::TextBoxAbove,false,84,15);
                control->slider.onReadoutVisibility=[view=control.get()](bool visible){view->label.setVisible(!visible);};
                control->slider.setInteractionReadout(true);
                control->slider.setTooltip(juce::String(anchor.label)+" — Drag to adjust; Shift for fine adjustment; double-click or press Enter to type a value.");
                control->slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(ink.getPerceivedBrightness()<.5f?0xffead4a4:0xff101718));
                control->slider.setColour(juce::Slider::textBoxTextColourId,ink);
                control->slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
                control->slider.setScrollWheelEnabled(false);control->slider.setWantsKeyboardFocus(true);
                control->slider.setVelocityModeParameters(.2,1,0.,true,juce::ModifierKeys::shiftModifier);
                if(model==spectralforge::firstOriginalAmpModel && index==int(spectralforge::original::Control::midFrequency)) {
                    control->slider.setSkewFactorFromMidPoint(850);control->slider.setTextValueSuffix(" Hz");control->slider.setNumDecimalPlacesToDisplay(0);
                } else if(model==spectralforge::niflheimrAmpModel && index==int(spectralforge::niflheimr::Control::midFrequency)) {
                    control->slider.setSkewFactorFromMidPoint(650);control->slider.setTextValueSuffix(" Hz");control->slider.setNumDecimalPlacesToDisplay(0);
                } else if(model==spectralforge::niflheimrAmpModel && index==int(spectralforge::niflheimr::Control::blend)) {
                    control->slider.textFromValueFunction=[](double v){return juce::String(juce::roundToInt(v*100))+"%";};
                    control->slider.valueFromTextFunction=[](const juce::String& v){return v.getDoubleValue()/100.;};control->slider.updateText();
                } else control->showHardwarePosition();
                control->slider.setDescription(contextName()+" / "+juce::String::fromUTF8(panel.channels[size_t(channel)])+" / "+anchor.label);
                // Captured identities reject any late pointer event before the
                // next UI tick, including same-model preset/A-B restoration.
                const auto boundRevision=revision;const int boundContext=context,boundModel=model,boundChannel=channel;
                control->canEdit=[this,boundRevision,boundContext,boundModel,boundChannel]{return bindingCurrent() && revision==boundRevision && context==boundContext && model==boundModel && channel==boundChannel;};
                control->activate=[this]{processor.activateNativeAmp(lane);};
                addAndMakeVisible(*control);controls.push_back(std::move(control));
            }
            if(enlarged && active)for(const auto& anchor:optionAnchors) {
                const int index=spectralforge::ampHead::controlIndex(model,anchor.key);jassert(index>=0);if(index<0)continue;
                const auto& spec=panel.controls[size_t(index)];auto control=std::make_unique<NativeControlView>();
                juce::StringArray values;for(const auto* value:spec.options)values.add(juce::String::fromUTF8(value));
                control->bind(processor.parameters(),spectralforge::ampNativeControlID(context,model,index,channel),
                    juce::String::fromUTF8(anchor.label),juce::String::fromUTF8(spec.group),
                    spec.kind==spectralforge::AmpNativeControlKind::choice?1:2,values,spec.minimum,spec.maximum,1.,
                    spectralforge::art::headStyle(model).knobStyle);
                control->setCompact(true);control->group.setVisible(false);
                control->label.setFont(juce::FontOptions(12.f,juce::Font::bold));
                const auto hint=juce::String::fromUTF8(spec.group)+" / "+juce::String::fromUTF8(spec.label);
                control->label.setTooltip(hint);control->choice.setTooltip(hint);control->toggle.setTooltip(hint);
                const auto boundRevision=revision;const int boundContext=context,boundModel=model,boundChannel=channel;
                control->canEdit=[this,boundRevision,boundContext,boundModel,boundChannel]{return bindingCurrent() && revision==boundRevision && context==boundContext && model==boundModel && channel==boundChannel;};
                control->activate=[this]{processor.activateNativeAmp(lane);};
                addAndMakeVisible(*control);optionControls.push_back(std::move(control));
            }
            for(int c=0;c<5;++c){
                channels[size_t(c)]=std::make_unique<ChannelButton>();auto& button=*channels[size_t(c)];addChildComponent(button);
                button.setComponentID("ampHeadChannel"+juce::String(lane+1)+"_"+juce::String(c));
                button.onClick=[this,c]{selectChannel(c);};button.setVisible(enlarged && active && (model==spectralforge::niflheimrAmpModel || model==spectralforge::firstOriginalAmpModel));
                button.setButtonText(juce::String(c+1));button.setToggleState(c==channel,juce::dontSendNotification);
                if(c<int(panel.channels.size())){const auto name=juce::String::fromUTF8(panel.channels[size_t(c)]);button.setName(name);button.setTooltip(name);}}
            input->setVisible(enlarged && active && panel.routes.size()>1);inputLabel.setVisible(input->isVisible());
            inputLabel.setColour(juce::Label::textColourId,juce::Colour(routeOnFace()?spectralforge::ampHead::faceStyle(model).ink:0xfff1ece0));
            focus.setVisible(!enlarged);resized();repaint();
        }
        const int nextRoute=processor.selectedAmpNativeRoute(lane);
        if(route!=nextRoute){route=nextRoute;input->syncSelectedId(route+1);}
    }
    juce::String contextName() const {return mode==0?"CLASSIC":mode==1?"DUAL / RIG "+juce::String(lane+1):"MATRIX / "+juce::String(lane==0?"LOW":lane==1?"MID":"HIGH");}
    juce::String caption() const {return contextName()+" / "+juce::String::fromUTF8(spectralforge::ampInfo(model).name)+" / "+juce::String::fromUTF8(spectralforge::ampNativePanel(model).channels[size_t(channel)]);}
    void resized() override {
        focus.setBounds(headBounds().toNearestInt());
        const auto face=spectralforge::cabHead::geometry(headBounds(),juce::jmax(0,model)).front;
        const auto style=spectralforge::ampHead::faceStyle(model);
        for(size_t i=0;i<controls.size();++i) {
            const auto& anchor=anchors[i];auto& c=*controls[i];
            const float fontSize=juce::jlimit(12.f,15.f,face.getWidth()*.0126f);
            const int labelHeight=juce::roundToInt(fontSize)+2;
            const int diameter=juce::jmax(32,juce::roundToInt(face.getWidth()*style.knobDiameter));
            const int cellWidth=juce::roundToInt(face.getWidth()*style.cellWidth);
            c.label.setFont(juce::FontOptions(fontSize,juce::Font::bold));
            c.slider.getProperties().set("headFontSize",fontSize);
            c.slider.setTextBoxStyle(juce::Slider::TextBoxAbove,false,cellWidth,labelHeight);
            for(auto* child:c.slider.getChildren())if(auto* value=dynamic_cast<juce::Label*>(child))value->setFont(c.label.getFont());
            c.setBounds(juce::roundToInt(face.getX()+face.getWidth()*anchor.x)-cellWidth/2,juce::roundToInt(face.getY()+face.getHeight()*anchor.y)-diameter/2-labelHeight,cellWidth,diameter+labelHeight);
            c.group.setVisible(false);c.label.setBounds(0,0,cellWidth,labelHeight);c.slider.setBounds(c.getLocalBounds());
        }
        const auto scaleRect=[&](juce::Rectangle<float> r){return juce::Rectangle<float>{face.getX()+face.getWidth()*r.getX(),face.getY()+face.getHeight()*r.getY(),face.getWidth()*r.getWidth(),face.getHeight()*r.getHeight()};};
        if(!style.channelRegion.isEmpty()) {
            const auto band=scaleRect(style.channelRegion);const auto width=band.getWidth()/5.f;
            for(int c=0;c<5;++c) {
                auto& button=*channels[size_t(c)];const float hitHeight=juce::jmax(32.f,band.getHeight()+4.f);
                button.setFaceSize(width*.70f,juce::jmax(16.f,band.getHeight()));
                button.setBounds(juce::Rectangle<float>{band.getX()+width*c,band.getCentreY()-hitHeight/2.f,width,hitHeight}.toNearestInt());
            }
        }
        const int columns=auxiliaryColumns(),cell=(getWidth()-24)/columns,top=getHeight()-auxiliaryHeight()+4;
        for(size_t i=0;i<optionControls.size();++i) {
            auto& control=*optionControls[i];control.setBounds(12+(int(i)%columns)*cell,top+(int(i)/columns)*56,cell-8,52);
            control.group.setVisible(false);control.label.setBounds(2,0,control.getWidth()-4,18);
            control.choice.setBounds(3,20,control.getWidth()-6,28);control.toggle.setBounds(3,20,control.getWidth()-6,28);
        }
        if(routeOnFace()) {
            const auto routeArea=scaleRect(style.routeRegion).toNearestInt();
            input->setBounds(routeArea.withHeight(30));inputLabel.setBounds(input->getX(),input->getY()-18,input->getWidth(),16);
        } else {
            const int i=int(optionControls.size());const int x=12+(i%columns)*cell,y=top+(i/columns)*56;
            inputLabel.setBounds(x+2,y,cell-12,18);input->setBounds(x+3,y+20,cell-14,28);
        }
    }
    void paint(juce::Graphics& g) override {
        spectralforge::cabHead::paint(g,headBounds(),juce::jmax(0,model));
        const auto face=spectralforge::cabHead::geometry(headBounds(),juce::jmax(0,model)).front;
        if(enlarged && active && (model==spectralforge::niflheimrAmpModel || model==spectralforge::firstOriginalAmpModel)) {
            // Replace the complete baked control field (including disk shadows),
            // using clean metal from the same vertical band. Per-disk elliptical
            // masks left crescents and bottom-row fragments at smaller sizes.
            // Keep the original jack, power lamp, switch and ornate perimeter.
            const auto& image=artwork->faces[size_t(model)].front;
            constexpr float left=.17f,top=.618f,width=.703f,height=.283f;
            if(image.isValid() && (!cleanControlField.isValid() || cleanModel!=model)) {
                cleanModel=model;
                // Cache one softly blended metal field. The outside edges stay
                // identical to the original fascia; old knob pixels lie fully
                // inside the opaque part, including their upper chrome rims.
                const int w=juce::roundToInt(image.getWidth()*width),h=juce::roundToInt(image.getHeight()*height);
                cleanControlField=juce::Image(juce::Image::RGB,w,h,false);
                const juce::Image::BitmapData source(image,juce::Image::BitmapData::readOnly);
                juce::Image::BitmapData target(cleanControlField,juce::Image::BitmapData::writeOnly);
                for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
                    const float u=float(x)/float(w-1),v=float(y)/float(h-1);
                    const float blend=juce::jlimit(0.f,1.f,juce::jmin(juce::jmin(u,1.f-u)/.085f,juce::jmin(v,1.f-v)/.008f));
                    const int sy=juce::roundToInt(image.getHeight()*top)+y;
                    const auto old=source.getPixelColour(juce::roundToInt(image.getWidth()*left)+x,sy);
                    const auto metal=source.getPixelColour(juce::roundToInt(image.getWidth()*(model==spectralforge::niflheimrAmpModel?.735f+u*.125f:.802f+u*.047f)),sy);
                    target.setPixelColour(x,y,old.interpolatedWith(metal,blend));
                }
            }
            if(cleanControlField.isValid())g.drawImage(cleanControlField,{face.getX()+face.getWidth()*left,face.getY()+face.getHeight()*top,face.getWidth()*width,face.getHeight()*height});
        }

    }
};

class AmpHeadWorkspace final : public juce::Component, private juce::Timer {
    spectralforge::art::DialogLook look;
    ChimeraProcessor& processor;int lane;
    AmpHeadControlView head;
    AmpNativePanel details;
    juce::Label title,hint;
    juce::TextButton back{"< BACK"},detailToggle{"DETAILS"},compareA{"A"},compareB{"B"};
    std::array<std::unique_ptr<juce::TextButton>,5> namedChannels;
    int channelModel{-1},channelContext{-1},channelSelection{-1};
    uint64_t channelRevision{};
    bool showDetails{};
    void timerCallback() override {refresh();}
public:
    AmpHeadWorkspace(ChimeraProcessor& p,int index):processor(p),lane(index),head(p,index,true),details(p,index,true) {
        setLookAndFeel(&look);setComponentID("ampHeadWorkspace");
        for(auto* c:std::initializer_list<juce::Component*>{&head,&details,&title,&hint,&back,&detailToggle,&compareA,&compareB})addAndMakeVisible(*c);

        back.setComponentID("ampHeadBack");back.onClick=[this]{head.finishInteraction();if(auto* w=findParentComponentOfClass<juce::DialogWindow>())w->exitModalState(0);};
        detailToggle.setComponentID("ampHeadDetails");detailToggle.onClick=[this]{head.finishInteraction();showDetails=!showDetails;refresh();};
        compareA.setComponentID("ampHeadCompareA");compareB.setComponentID("ampHeadCompareB");
        compareA.setTooltip("Recall the existing sound A comparison");compareB.setTooltip("Recall the existing sound B comparison");
        compareA.onClick=[this]{head.finishInteraction();processor.selectComparison(0);refresh();};
        compareB.onClick=[this]{head.finishInteraction();processor.selectComparison(1);refresh();};
        title.setFont(juce::FontOptions(15.f,juce::Font::bold));hint.setFont(juce::FontOptions(12.f));
        setSize(1200,650);refresh();startTimerHz(25);
    }
    ~AmpHeadWorkspace() override {stopTimer();head.finishInteraction();setLookAndFeel(nullptr);}
    void refresh() {
        compareA.setToggleState(processor.comparisonSlot()==0,juce::dontSendNotification);compareB.setToggleState(processor.comparisonSlot()==1,juce::dontSendNotification);
        head.refresh();const bool active=lane<=int(processor.parameters().getRawParameterValue("mode")->load());
        const bool supported=spectralforge::ampHead::supported(head.displayedModel());
        details.setVisible(active && (showDetails||!supported));head.setVisible(active && !details.isVisible());
        detailToggle.setVisible(active && supported);detailToggle.setButtonText(showDetails?"HEAD":"DETAILS");
        if(details.isVisible())details.refreshIfNeeded();
        if(channelModel!=head.displayedModel() || channelContext!=head.displayedContext()
            || channelSelection!=head.displayedChannel() || channelRevision!=head.displayedRecallRevision()) {
            channelModel=head.displayedModel();channelContext=head.displayedContext();
            channelSelection=head.displayedChannel();channelRevision=head.displayedRecallRevision();
            for(int c=0;c<5;++c) {
                namedChannels[size_t(c)]=std::make_unique<juce::TextButton>();auto& button=*namedChannels[size_t(c)];addChildComponent(button);
                button.setComponentID("ampHeadNamedChannel"+juce::String(lane+1)+"_"+juce::String(c));
                button.onClick=[this,c]{head.selectChannel(c);refresh();};
            }
        }
        for(int c=0;c<5;++c) {auto& button=*namedChannels[size_t(c)];
            const auto& names=spectralforge::ampNativePanel(head.displayedModel()).channels;
            button.setVisible(head.isVisible() && names.size()>1 && c<int(names.size()));
            if(c<int(names.size()))button.setButtonText(juce::String(c+1)+"  "+juce::String::fromUTF8(names[size_t(c)]));
            button.setToggleState(c==processor.selectedAmpChannel(lane),juce::dontSendNotification);
        }
        resized();
        title.setText(active?head.caption():"This rig is inactive in the current routing mode",juce::dontSendNotification);
        hint.setText(details.isVisible()?"Full amplifier controls":supported?"Drag to adjust | Shift: fine | Double-click / Enter: type value":"",juce::dontSendNotification);
    }
    void resized() override {back.setBounds(16,12,86,30);detailToggle.setBounds(getWidth()-112,12,96,30);compareA.setBounds(getWidth()-218,12,44,30);compareB.setBounds(getWidth()-168,12,44,30);title.setBounds(116,12,getWidth()-348,30);hint.setBounds(20,getHeight()-31,getWidth()-40,22);const bool names=namedChannels[0] && namedChannels[0]->isVisible();const int top=names?90:52;
        head.setBounds(12,top,getWidth()-24,getHeight()-top-40);
        const int count=juce::jmax(1,int(spectralforge::ampNativePanel(head.displayedModel()).channels.size()));
        const int channelWidth=juce::jmin(184,(getWidth()-48)/count);
        for(int c=0;c<5;++c)if(namedChannels[size_t(c)])namedChannels[size_t(c)]->setBounds((getWidth()-channelWidth*count)/2+c*channelWidth,52,channelWidth-6,30);details.setBounds(24,62,getWidth()-48,getHeight()-108);}
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff111617));}
};
