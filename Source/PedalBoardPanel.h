#pragma once
#include "PluginProcessor.h"
#include "HardwareArtwork.h"
#include "EffectSelectionCatalog.h"

// The five slots edit the real APVTS model banks. Selection and copying happen
// immediately; bank ownership, MIDI state and undo remain processor operations.
class PedalBoardPanel final : public juce::Component, private juce::Timer {
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    // APVTS notifications update the control without activating another audio
    // path. Only deliberate mouse, keyboard or textbox edits activate the board.
    class BoardSlider final : public juce::Slider, private juce::Label::Listener,
                              private juce::TextEditor::Listener {
        juce::String editorText;
        void labelTextChanged(juce::Label*) override {}
        void editorShown(juce::Label*,juce::TextEditor& editor) override {
            editorText=editor.getText();editor.addListener(this);
        }
        void textEditorTextChanged(juce::TextEditor& editor) override {
            if(editor.getText()!=editorText)activate();
        }
        void activate() {if(isEnabled() && onUserEdit)onUserEdit();}
    public:
        std::function<void()> onUserEdit;
        void watchTextBox() {
            for(auto* child:getChildren())if(auto* label=dynamic_cast<juce::Label*>(child))label->addListener(this);
        }
        void mouseDown(const juce::MouseEvent& event) override {
            if(!event.mods.isPopupMenu())activate();juce::Slider::mouseDown(event);
        }
        void mouseWheelMove(const juce::MouseEvent& event,const juce::MouseWheelDetails& wheel) override {
            if(wheel.deltaX!=0 || wheel.deltaY!=0)activate();juce::Slider::mouseWheelMove(event,wheel);
        }
        bool keyPressed(const juce::KeyPress& key) override {
            const bool handled=juce::Slider::keyPressed(key);if(handled)activate();return handled;
        }
    };
    struct Control {
        juce::Label label;BoardSlider slider;juce::TextButton midi{"MIDI"};
        std::unique_ptr<SA> attachment;
    };
    struct Card {
        PedalSelector model;juce::TextButton bypass{"ON"},left{"<"},right{">"},copy{"COPY"},remove{"X"},detail{"CONTROLS"};
        juce::Label title;std::array<Control,6> controls;std::unique_ptr<BA> attachment;
        int owner{-1},modelId{-1};
    };
    ChimeraProcessor& processor;
    std::array<Card,5> cards;
    std::array<Control,spectralforge::pedalMaxControls> details;
    juce::TextButton close{"BACK TO 5 PEDALS"},undo{"UNDO"},redo{"REDO"},detailBypass{"ON"};
    juce::Label notice,detailTitle,detailReference,tapLabel;
    BoardSlider tap;
    std::unique_ptr<SA> tapAttachment;
    std::unique_ptr<BA> detailBypassAttachment;
    int detailedOwner{-1},detailedModel{-1};
    bool syncing{};
    static void stylePowerButton(juce::TextButton& button,bool modelPresent,bool bypassed) {
        const bool active=modelPresent&&!bypassed;
        button.getProperties().set("pedalPower",true);
        button.setButtonText(active?"ON":"OFF");
        button.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff6e9c59));
        button.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff323837));
        button.setColour(juce::TextButton::textColourOffId,juce::Colour(0xff111b13));
        button.setColour(juce::TextButton::textColourOnId,juce::Colour(0xffd2d6ce));
        button.setTooltip(active?"Effect is ON. Click to bypass.":"Effect is OFF. Click to enable.");
    }
    static int cardControl(int model,int index) {
        if(model==4) {constexpr int map[]{6,0,1,2,3,4};return map[index];}
        if(model==10) {constexpr int map[]{0,2,3,4,5,6};return map[index];}
        if(model==29) {constexpr int map[]{0,2,3,5,6,7};return map[index];}
        return index;
    }
    void replaceModel(int owner,int model) {
        processor.setPedalModel(owner,model);refresh();
    }
    void duplicate(int owner) {
        const auto state=processor.pedalBoardState();int target=-1;
        for(int i:state.order)if(!state.instances[(size_t)i].model){target=i;break;}
        if(target<0) {notice.setText("All five slots are occupied. Delete a pedal or explicitly replace a slot.",juce::dontSendNotification);return;}
        processor.duplicatePedal(owner);refresh();
    }
    void addControl(Control& c) {
        addAndMakeVisible(c.label);addAndMakeVisible(c.slider);addAndMakeVisible(c.midi);
        c.label.setJustificationType(juce::Justification::centred);c.label.setFont(juce::FontOptions(11.f));
        c.label.setColour(juce::Label::textColourId,juce::Colour(0xffdce8d2));
        c.label.setColour(juce::Label::backgroundColourId,juce::Colour(0xff151c18).withAlpha(.94f));
        c.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        c.slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,78,20);
        c.slider.onUserEdit=[this]{processor.setPedalBoardEnabled(true);};c.slider.watchTextBox();
        c.slider.setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xffdce8d2));
        c.slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff151c18));
    }
    void bind(Control& c,int owner,int model,int index,bool showMidi) {
        c.attachment.reset();c.slider.textFromValueFunction=nullptr;c.slider.valueFromTextFunction=nullptr;
        const auto& m=spectralforge::pedalModel(model);const bool exists=index<m.controlCount;
        c.label.setVisible(exists);c.slider.setVisible(exists);c.midi.setVisible(exists&&showMidi);
        if(!exists)return;
        const auto& spec=m.controls[(size_t)index];
        c.label.setText(spec.label,juce::dontSendNotification);c.slider.setName(spec.label);
        c.label.setTooltip(spec.label);
        c.slider.setRange(spec.minimum,spec.maximum,spec.interval);
        const auto artwork=spectralforge::art::boardPedalStyle(model);
        c.slider.getProperties().set("knobStyle",artwork.knobStyle);
        c.slider.getProperties().set("brightFace",showMidi?false:artwork.brightFace);
        c.slider.setEnabled(spec.connected);c.midi.setEnabled(spec.connected);
        c.slider.setTooltip(spec.connected ? "Experimental DSP curve. Hardware taper has not been calibrated." : "Not connected to DSP; control is disabled.");
        const auto names=juce::StringArray::fromTokens(spec.choices,"|",{});
        if(juce::String(spec.choices).isNotEmpty()) c.slider.textFromValueFunction=[names](double value){return names[juce::jlimit(0,names.size()-1,juce::roundToInt(value))];};
        const auto id=spectralforge::pedalControlID(owner,model,index);c.slider.setComponentID(id);
        c.attachment=std::make_unique<SA>(processor.parameters(),id,c.slider);
        c.midi.onClick=[this,id]{processor.learnMidi(id);notice.setText("Move a MIDI controller to bind this instance/control.",juce::dontSendNotification);};
    }
    void showDetails(int owner) {
        const auto state=processor.pedalBoardState();detailedOwner=owner;detailedModel=owner<0 ? -1 : state.instances[(size_t)owner].model;
        detailBypassAttachment.reset();
        if(owner>=0) {
            detailTitle.setText(spectralforge::pedalMenuName(detailedModel),juce::dontSendNotification);
            detailReference.setText(spectralforge::pedalModel(detailedModel).name,juce::dontSendNotification);
            for(int i=0;i<spectralforge::pedalMaxControls;++i)bind(details[(size_t)i],owner,detailedModel,i,true);
            detailBypassAttachment=std::make_unique<BA>(processor.parameters(),spectralforge::pedalBypassID(owner,detailedModel),detailBypass);
        }
        refresh();resized();repaint();
    }
    void timerCallback() override {refresh();}
    void refresh() {
        if(!isVisible())return;
        const auto state=processor.pedalBoardState();syncing=true;
        if(detailedOwner>=0 && state.instances[(size_t)detailedOwner].model!=detailedModel) {syncing=false;showDetails(-1);return;}
        bool layoutChanged=false;int count=0;for(const auto& e:state.instances)if(e.model)++count;
        const bool matrix=processor.parameters().getRawParameterValue("mode")->load()==2;
        tap.setEnabled(matrix);tapLabel.setText(matrix ? "LOW TAP after slot" : "LOW TAP / Matrix only",juce::dontSendNotification);
        for(int position=0;position<5;++position) {
            auto& card=cards[(size_t)position];const int owner=state.order[(size_t)position],model=state.instances[(size_t)owner].model;
            if(card.owner!=owner || card.modelId!=model) {
                layoutChanged=true;
                const bool ownerChanged=card.owner!=owner;
                card.owner=owner;card.modelId=model;
                if(ownerChanged)card.model.resetSyncExplicit(model+1);else card.model.syncSelectedId(model+1);
                card.attachment.reset();
                if(model)card.attachment=std::make_unique<BA>(processor.parameters(),spectralforge::pedalBypassID(owner,model),card.bypass);
                for(int c=0;c<6;++c)bind(card.controls[(size_t)c],owner,model,cardControl(model,c),false);
                card.model.setTooltip(model==39?"Spectral octaver: about 43-46 ms processing delay. Dense low chords can lose fundamentals.":model==38?"Monophonic octave divider. Use single notes; chord tracking is not supported.":"Choose a pedal by type. Each slot keeps its own settings.");
            }
            card.title.setText("SLOT "+juce::String(position+1)+"  /  "+(model?juce::String(spectralforge::pedalModel(model).controlCount)+" CONTROLS":"ADD A PEDAL"),juce::dontSendNotification);
            const bool visible=detailedOwner<0;
            for(juce::Component* c:std::initializer_list<juce::Component*>{&card.title,&card.model,&card.bypass,&card.left,&card.right,&card.copy,&card.remove,&card.detail})c->setVisible(visible);
            card.model.setEnabled(true);
            card.left.setEnabled(position>0);card.right.setEnabled(position<4);card.copy.setEnabled(count<5&&model!=0);card.detail.setEnabled(model!=0);
            card.detail.setButtonText(spectralforge::pedalModel(model).controlCount>6?"ALL CONTROLS":"DETAIL / MIDI");
            card.remove.setEnabled(model!=0);card.bypass.setEnabled(model!=0);
            stylePowerButton(card.bypass,model!=0,state.instances[(size_t)owner].bypass);
            for(int c=0;c<6;++c) {
                const int index=cardControl(model,c);const bool show=visible&&index<spectralforge::pedalModel(model).controlCount;
                card.controls[(size_t)c].label.setVisible(show);card.controls[(size_t)c].slider.setVisible(show);
                if(show)card.controls[(size_t)c].slider.setEnabled(spectralforge::pedalModel(model).controls[(size_t)index].connected);
            }
        }
        close.setVisible(detailedOwner>=0);detailTitle.setVisible(detailedOwner>=0);detailReference.setVisible(detailedOwner>=0);
        detailBypass.setVisible(detailedOwner>=0);
        if(detailedOwner>=0)stylePowerButton(detailBypass,true,state.instances[(size_t)detailedOwner].bypass);
        for(int c=0;c<spectralforge::pedalMaxControls;++c) {
            const bool show=detailedOwner>=0&&c<spectralforge::pedalModel(juce::jmax(0,detailedModel)).controlCount;
            details[(size_t)c].label.setVisible(show);details[(size_t)c].slider.setVisible(show);details[(size_t)c].midi.setVisible(show);
            if(show) {const bool enabled=spectralforge::pedalModel(detailedModel).controls[(size_t)c].connected;details[(size_t)c].slider.setEnabled(enabled);details[(size_t)c].midi.setEnabled(enabled);}
        }
        tap.setEnabled(matrix);undo.setEnabled(true);redo.setEnabled(true);
        syncing=false;if(layoutChanged)resized();repaint();
    }
public:
    explicit PedalBoardPanel(ChimeraProcessor& p):processor(p) {
        setComponentID("universalPedalBoard");
        for(int position=0;position<5;++position) {
            auto& card=cards[(size_t)position];
            for(juce::Component* c:std::initializer_list<juce::Component*>{&card.title,&card.model,&card.bypass,&card.left,&card.right,&card.copy,&card.remove,&card.detail})addAndMakeVisible(c);
            card.model.setComponentID("boardModelAt"+juce::String(position));
            card.detail.setComponentID("boardDetailAt"+juce::String(position));
            card.copy.setComponentID("boardCopyAt"+juce::String(position));
            card.remove.setComponentID("boardRemoveAt"+juce::String(position));
            card.bypass.setComponentID("boardBypassAt"+juce::String(position));
            card.left.setComponentID("boardLeftAt"+juce::String(position));
            card.right.setComponentID("boardRightAt"+juce::String(position));
            card.model.onChange=[this,position]{auto& c=cards[(size_t)position];c.model.acceptSelection();if(!syncing&&c.owner>=0)replaceModel(c.owner,c.model.getSelectedId()-1);};
            card.left.onClick=[this,position]{processor.movePedal(cards[(size_t)position].owner,-1);notice.setText("Order changed. Moving across the LOW TAP changes the LOW input.",juce::dontSendNotification);refresh();};
            card.right.onClick=[this,position]{processor.movePedal(cards[(size_t)position].owner,1);notice.setText("Order changed. Moving across the LOW TAP changes the LOW input.",juce::dontSendNotification);refresh();};
            card.copy.onClick=[this,position]{duplicate(cards[(size_t)position].owner);};
            card.remove.onClick=[this,position]{processor.setPedalModel(cards[(size_t)position].owner,0);refresh();};
            card.detail.onClick=[this,position]{showDetails(cards[(size_t)position].owner);};
            card.bypass.onClick=[this]{processor.setPedalBoardEnabled(true);refresh();};
            for(auto& c:card.controls)addControl(c);
        }
        for(auto& c:details)addControl(c);
        for(juce::Component* c:std::initializer_list<juce::Component*>{&close,&undo,&redo,&notice,&detailTitle,&detailReference,&detailBypass,&tapLabel,&tap})addAndMakeVisible(c);
        close.setComponentID("boardDetailClose");detailTitle.setComponentID("boardDetailTitle");detailReference.setComponentID("boardDetailReference");
        detailBypass.setComponentID("boardDetailBypass");detailBypass.onClick=[this]{processor.setPedalBoardEnabled(true);refresh();};
        undo.setComponentID("boardUndo");redo.setComponentID("boardRedo");tap.setComponentID("boardLowTapControl");notice.setComponentID("boardNotice");
        detailTitle.setFont(juce::FontOptions(19.f,juce::Font::bold));detailReference.setFont(juce::FontOptions(11.f));
        detailReference.setColour(juce::Label::textColourId,juce::Colour(0xffaeb8ab));
        close.onClick=[this]{showDetails(-1);};undo.onClick=[this]{processor.undoPedalEdit();refresh();};redo.onClick=[this]{processor.undoPedalEdit(true);refresh();};
        tap.setRange(0,5,1);tap.setSliderStyle(juce::Slider::LinearHorizontal);tap.setTextBoxStyle(juce::Slider::TextBoxRight,false,28,22);
        tap.onUserEdit=[this]{processor.setPedalBoardEnabled(true);};tap.watchTextBox();
        tapAttachment=std::make_unique<SA>(processor.parameters(),"boardLowTap",tap);
        notice.setFont(juce::FontOptions(11.f));notice.setText("Choose a pedal by type. Move, copy or replace it in any of the five slots.",juce::dontSendNotification);
        startTimerHz(10);refresh();
    }
    ~PedalBoardPanel() override {stopTimer();}
    void visibilityChanged() override {if(isVisible())refresh();}
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171a19));
        if(detailedOwner<0)for(int i=0;i<5;++i) {
            const auto& card=cards[(size_t)i];const int x=i*230;
            if(card.modelId>0)spectralforge::art::boardPedal(g,{float(x),46.f,220.f,352.f},card.modelId);
            else {g.setColour(juce::Colour(0xff252a27));g.fillRoundedRectangle(float(x),46,220,352,9);}
            g.setColour(juce::Colour(0xff111513).withAlpha(.9f));g.fillRoundedRectangle(float(x+7),51,206,66,4);
            if(card.modelId==0) {g.setColour(juce::Colour(0xff8ea38f));g.setFont(juce::FontOptions(36.f));g.drawText("+",x+30,143,160,62,juce::Justification::centred);g.setFont(juce::FontOptions(12.f));g.drawText("Choose any pedal above",x+12,211,196,22,juce::Justification::centred);}
        }
        g.setColour(juce::Colour(0xffc6d7bc));g.setFont(juce::FontOptions(11.f));
        g.drawText("GR "+juce::String(processor.pedalReduction(),1)+" dB",842,7,86,25,juce::Justification::centredRight);
        const auto state=processor.pedalBoardState();int poly=0;for(const auto& instance:state.instances)if(instance.model==39)++poly;
        if(poly>0 && detailedOwner<0) {const double sampleRate=processor.getSampleRate()>0?processor.getSampleRate():48000;g.setColour(juce::Colour(0xffe6bb7b));g.drawText("PRE DELAY "+juce::String(1000.0*processor.pedalBoardLatencySamples()/sampleRate,1)+" ms / spectral octave",527,7,312,25,juce::Justification::centredLeft);}
    }
    void resized() override {
        undo.setBounds(0,5,61,25);redo.setBounds(67,5,61,25);tapLabel.setBounds(145,5,168,25);tap.setBounds(313,5,200,25);notice.setBounds(0,398,1140,17);
        close.setBounds(937,5,203,27);detailBypass.setBounds(741,5,87,27);detailTitle.setBounds(14,44,1090,24);detailReference.setBounds(14,68,1090,18);
        for(int position=0;position<5;++position) {
            auto& c=cards[(size_t)position];const int x=position*230;
            c.title.setBounds(x+9,52,202,19);c.model.setBounds(x+9,79,202,29);
            for(int k=0;k<6;++k) {
                const int count=spectralforge::pedalModel(juce::jmax(0,c.modelId)).controlCount;
                const bool compact=count>3;
                const int left=count==4?x+17+(k%2)*107:compact?x+7+(k%3)*70:x+(count==1 || k==2?71:17+k*107);
                const int top=count==4?121+(k/2)*96:compact?121+(k/3)*96:count<3?145:k==2?215:119;
                const int width=count==4?78:compact?66:78,height=compact?74:76;
                c.controls[(size_t)k].label.setBounds(left,top,width,18);c.controls[(size_t)k].slider.setBounds(left,top+18,width,height);
                c.controls[(size_t)k].slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,compact?65:78,18);
                c.controls[(size_t)k].slider.watchTextBox();
            }
            c.left.setBounds(x+9,320,31,25);c.right.setBounds(x+44,320,31,25);c.copy.setBounds(x+80,320,62,25);c.remove.setBounds(x+147,320,64,25);
            c.detail.setBounds(x+9,354,107,31);c.bypass.setBounds(x+122,354,89,31);
        }
        for(int k=0;k<spectralforge::pedalMaxControls;++k) {
            const bool four=detailedModel>=0&&spectralforge::pedalModel(detailedModel).controlCount==4;
            auto& c=details[(size_t)k];const int x=four?248+(k%2)*370:16+(k%4)*282,y=four?94+(k/2)*139:87+(k/4)*103;
            c.label.setBounds(x,y,190,19);c.slider.setBounds(x+34,y+20,123,79);c.midi.setBounds(x+194,y+42,61,25);
        }
    }
};
