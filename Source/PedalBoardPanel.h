#pragma once
#include "PluginProcessor.h"
#include "HardwareArtwork.h"

// This view edits the same APVTS values consumed by the audio engine. No preview
// JSON is imported and the legacy sound engine stays available independently.
class PedalBoardPanel final : public juce::Component, private juce::Timer {
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    struct Control {
        juce::Label label;juce::Slider slider;juce::TextButton midi{"MIDI"};
        std::unique_ptr<SA> attachment;
    };
    struct Card {
        juce::ComboBox model;juce::TextButton bypass{"BYPASS"},left{"<"},right{">"},copy{"COPY"},remove{"X"},detail{"CONTROLS"};
        juce::Label title;std::array<Control,3> controls;std::unique_ptr<BA> attachment;
        int owner{-1},modelId{-1};
    };
    ChimeraProcessor& processor;
    std::array<Card,5> cards;
    std::array<Control,spectralforge::pedalMaxControls> details;
    juce::TextButton close{"BACK TO 5 PEDALS"},undo{"UNDO"},redo{"REDO"};
    juce::Label notice,detailTitle,tapLabel;
    juce::Slider tap;
    std::unique_ptr<SA> tapAttachment;
    int detailedOwner{-1},detailedModel{-1};
    bool syncing{};
    void replaceModel(int owner,int model) {
        // Host automation is outside A/B/state history: disclose bank reuse even after loading an older snapshot.
        if(model==0) {processor.setPedalModel(owner,model);refresh();return;}
        const juce::Component::SafePointer<PedalBoardPanel> safe(this);
        juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon).withTitle("Use this parameter bank?")
            .withMessage("Each slot/model uses a fixed host parameter bank. Any DAW automation previously recorded for this slot and model will apply, including after A/B or project restore. Review those lanes before proceeding. MIDI assignments for this slot will be cleared.")
            .withButton("USE BANK").withButton("CANCEL"),[safe,owner,model](int result){if(!safe)return;if(result==1)safe->processor.setPedalModel(owner,model);safe->refresh();});
    }
    void duplicate(int owner) {
        const auto state=processor.pedalBoardState();int target=-1;
        for(int i:state.order)if(!state.instances[(size_t)i].model){target=i;break;}
        if(target<0) {notice.setText("All five slots are occupied. Delete a pedal or explicitly replace a slot.",juce::dontSendNotification);return;}
        const juce::Component::SafePointer<PedalBoardPanel> safe(this);
        juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon).withTitle("Use destination parameter bank?")
            .withMessage("The copy has independent controls in the empty destination slot. Any DAW automation previously recorded for this model at that destination can control the copy, including after A/B or project restore. Review those lanes before proceeding.")
            .withButton("USE BANK").withButton("CANCEL"),[safe,owner](int result){if(!safe)return;if(result==1)safe->processor.duplicatePedal(owner);safe->refresh();});
    }
    void addControl(Control& c) {
        addAndMakeVisible(c.label);addAndMakeVisible(c.slider);addAndMakeVisible(c.midi);
        c.label.setJustificationType(juce::Justification::centred);c.label.setFont(juce::FontOptions(11.f));
        c.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        c.slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,78,20);
    }
    void bind(Control& c,int owner,int model,int index,bool showMidi) {
        c.attachment.reset();c.slider.textFromValueFunction=nullptr;c.slider.valueFromTextFunction=nullptr;
        const auto& m=spectralforge::pedalModel(model);const bool exists=index<m.controlCount;
        c.label.setVisible(exists);c.slider.setVisible(exists);c.midi.setVisible(exists&&showMidi);
        if(!exists)return;
        const auto& spec=m.controls[(size_t)index];
        c.label.setText(spec.label,juce::dontSendNotification);c.slider.setName(spec.label);
        c.slider.setRange(spec.minimum,spec.maximum,spec.interval);
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
        if(owner>=0) {
            detailTitle.setText(spectralforge::pedalModel(detailedModel).name,juce::dontSendNotification);
            for(int i=0;i<spectralforge::pedalMaxControls;++i)bind(details[(size_t)i],owner,detailedModel,i,true);
        }
        refresh();resized();repaint();
    }
    void timerCallback() override {refresh();}
    void refresh() {
        if(!isVisible())return;
        const auto state=processor.pedalBoardState();syncing=true;
        if(detailedOwner>=0 && state.instances[(size_t)detailedOwner].model!=detailedModel) {syncing=false;showDetails(-1);return;}
        int count=0;for(const auto& e:state.instances)if(e.model)++count;
        const bool matrix=processor.parameters().getRawParameterValue("mode")->load()==2;
        tap.setEnabled(matrix);tapLabel.setText(matrix ? "LOW TAP after slot" : "LOW TAP / Matrix only",juce::dontSendNotification);
        for(int position=0;position<5;++position) {
            auto& card=cards[(size_t)position];const int owner=state.order[(size_t)position],model=state.instances[(size_t)owner].model;
            if(card.owner!=owner || card.modelId!=model) {
                card.owner=owner;card.modelId=model;card.model.setSelectedId(model+1,juce::dontSendNotification);
                card.attachment.reset();
                if(model)card.attachment=std::make_unique<BA>(processor.parameters(),spectralforge::pedalBypassID(owner,model),card.bypass);
                for(int c=0;c<3;++c)bind(card.controls[(size_t)c],owner,model,c,false);
            }
            card.title.setText("SLOT "+juce::String(position+1)+"  /  INSTANCE "+juce::String(owner+1),juce::dontSendNotification);
            const bool visible=detailedOwner<0;
            for(juce::Component* c:std::initializer_list<juce::Component*>{&card.title,&card.model,&card.bypass,&card.left,&card.right,&card.copy,&card.remove,&card.detail})c->setVisible(visible);
            card.left.setEnabled(position>0);card.right.setEnabled(position<4);card.copy.setEnabled(count<5&&model!=0);card.detail.setEnabled(model!=0);
            card.remove.setEnabled(model!=0);card.bypass.setEnabled(model!=0);
            for(int c=0;c<3;++c) {const bool show=visible&&c<spectralforge::pedalModel(model).controlCount;card.controls[(size_t)c].label.setVisible(show);card.controls[(size_t)c].slider.setVisible(show);}
        }
        close.setVisible(detailedOwner>=0);detailTitle.setVisible(detailedOwner>=0);
        for(int c=0;c<spectralforge::pedalMaxControls;++c) {
            const bool show=detailedOwner>=0&&c<spectralforge::pedalModel(juce::jmax(0,detailedModel)).controlCount;
            details[(size_t)c].label.setVisible(show);details[(size_t)c].slider.setVisible(show);details[(size_t)c].midi.setVisible(show);
        }
        syncing=false;repaint();
    }
public:
    explicit PedalBoardPanel(ChimeraProcessor& p):processor(p) {
        setComponentID("universalPedalBoard");
        for(int position=0;position<5;++position) {
            auto& card=cards[(size_t)position];
            for(juce::Component* c:std::initializer_list<juce::Component*>{&card.title,&card.model,&card.bypass,&card.left,&card.right,&card.copy,&card.remove,&card.detail})addAndMakeVisible(c);
            card.model.setComponentID("boardModelAt"+juce::String(position));
            card.model.addItem("Empty / +",1);
            for(int model=1;model<spectralforge::pedalModelCount;++model) {
                const auto& m=spectralforge::pedalModel(model);
                card.model.getRootMenu()->addItem(model+1,juce::String(m.category)+" / "+m.name,m.implemented,false);
            }
            card.model.onChange=[this,position]{auto& c=cards[(size_t)position];if(!syncing&&c.owner>=0)replaceModel(c.owner,c.model.getSelectedId()-1);};
            card.left.onClick=[this,position]{processor.movePedal(cards[(size_t)position].owner,-1);notice.setText("Order changed. Moving across the LOW TAP changes the LOW input.",juce::dontSendNotification);refresh();};
            card.right.onClick=[this,position]{processor.movePedal(cards[(size_t)position].owner,1);notice.setText("Order changed. Moving across the LOW TAP changes the LOW input.",juce::dontSendNotification);refresh();};
            card.copy.onClick=[this,position]{duplicate(cards[(size_t)position].owner);};
            card.remove.onClick=[this,position]{processor.setPedalModel(cards[(size_t)position].owner,0);refresh();};
            card.detail.onClick=[this,position]{showDetails(cards[(size_t)position].owner);};
            for(auto& c:card.controls)addControl(c);
        }
        for(auto& c:details)addControl(c);
        for(juce::Component* c:std::initializer_list<juce::Component*>{&close,&undo,&redo,&notice,&detailTitle,&tapLabel,&tap})addAndMakeVisible(c);
        close.onClick=[this]{showDetails(-1);};undo.onClick=[this]{processor.undoPedalEdit();refresh();};redo.onClick=[this]{processor.undoPedalEdit(true);refresh();};
        tap.setRange(0,5,1);tap.setSliderStyle(juce::Slider::LinearHorizontal);tap.setTextBoxStyle(juce::Slider::TextBoxRight,false,28,22);
        tapAttachment=std::make_unique<SA>(processor.parameters(),"boardLowTap",tap);
        notice.setFont(juce::FontOptions(11.f));notice.setText("Experimental DSP. Fixed instance MIDI/automation follows moves. Stop playback before changing the board structure.",juce::dontSendNotification);
        startTimerHz(10);refresh();
    }
    ~PedalBoardPanel() override {stopTimer();}
    void visibilityChanged() override {if(isVisible())refresh();}
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171a19));
        if(detailedOwner<0)for(int i=0;i<5;++i) {
            const auto& card=cards[(size_t)i];const int x=i*230;
            int family=-1,variant=0;
            if(card.modelId>=1&&card.modelId<=5){family=0;variant=card.modelId-1;}
            if(card.modelId>=6&&card.modelId<=10){family=3;variant=card.modelId-6;}
            if(card.modelId>=11&&card.modelId<=15){family=4;variant=card.modelId-11;}
            if(card.modelId>=16&&card.modelId<=20){family=5;variant=card.modelId-16;}
            if(card.modelId>=21&&card.modelId<=25){family=6;variant=card.modelId-21;}
            if(family>=0)spectralforge::art::pedal(g,{float(x),46.f,220.f,352.f},family,variant);
            else {g.setColour(juce::Colour(card.modelId==0?0xff252a27:0xff354942));g.fillRoundedRectangle(float(x),46,220,352,9);}
            g.setColour(juce::Colour(0xff111513).withAlpha(.9f));g.fillRoundedRectangle(float(x+7),51,206,66,4);
        }
        g.setColour(juce::Colour(0xffc6d7bc));g.setFont(juce::FontOptions(11.f));
        g.drawText("GR "+juce::String(processor.pedalReduction(),1)+" dB",840,7,118,25,juce::Justification::centredRight);
    }
    void resized() override {
        undo.setBounds(0,5,61,25);redo.setBounds(67,5,61,25);tapLabel.setBounds(145,5,168,25);tap.setBounds(313,5,200,25);notice.setBounds(0,398,1140,17);
        close.setBounds(937,5,203,27);detailTitle.setBounds(14,47,1090,35);
        for(int position=0;position<5;++position) {
            auto& c=cards[(size_t)position];const int x=position*230;
            c.title.setBounds(x+9,52,202,19);c.model.setBounds(x+9,79,202,29);
            for(int k=0;k<3;++k) {
                const int left=x+(k==2 ? 64 : 7+k*107),top=k==2 ? 218 : 119;
                c.controls[(size_t)k].label.setBounds(left,top,98,18);c.controls[(size_t)k].slider.setBounds(left,top+18,98,81);
            }
            c.left.setBounds(x+9,320,31,25);c.right.setBounds(x+44,320,31,25);c.copy.setBounds(x+80,320,62,25);c.remove.setBounds(x+147,320,64,25);
            c.detail.setBounds(x+9,354,107,31);c.bypass.setBounds(x+122,354,89,31);
        }
        for(int k=0;k<spectralforge::pedalMaxControls;++k) {
            auto& c=details[(size_t)k];const int x=16+(k%4)*282,y=87+(k/4)*103;
            c.label.setBounds(x,y,190,19);c.slider.setBounds(x+34,y+20,123,79);c.midi.setBounds(x+194,y+42,61,25);
        }
    }
};
