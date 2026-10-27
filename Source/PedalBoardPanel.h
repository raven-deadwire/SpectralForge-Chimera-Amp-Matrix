#pragma once
#include "KnobInteraction.h"
#include "PluginProcessor.h"
#include "HardwareArtwork.h"
#include "EffectSelectionCatalog.h"
#include "UIRefresh.h"
#include "DynamicsSpectrumPanel.h"

// The five slots edit the real APVTS model banks. Selection and copying happen
// immediately; bank ownership, MIDI state and undo remain processor operations.
class PedalBoardPanel final : public juce::Component, private juce::Timer {
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA=juce::ParameterAttachment;
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
            if(isScrollWheelEnabled() && (wheel.deltaX!=0 || wheel.deltaY!=0))activate();juce::Slider::mouseWheelMove(event,wheel);
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
        PedalSelector model;juce::TextButton bypass{"ON"},left{"<"},right{">"},copy{"COPY"},remove{"X"},detail{"CONTROLS"},spectrumEdit{"SPECTRUM EDITOR"};
        juce::Label title,reference;std::array<Control,6> controls;std::unique_ptr<BA> attachment;
        int owner{-1},modelId{-1};
    };
    ChimeraProcessor& processor;
    std::array<Card,5> cards;
    std::array<Control,spectralforge::pedalMaxControls> details;
    juce::TextButton close{"BACK TO 5 PEDALS"},undo{"UNDO"},redo{"REDO"},detailBypass{"ON"};
    juce::Label notice,detailTitle,detailReference,tapLabel,compressorStatus;
    BoardSlider tap;
    std::unique_ptr<SA> tapAttachment;
    std::unique_ptr<BA> detailBypassAttachment;
    juce::Component detailContent;
    juce::Viewport detailViewport;
    std::unique_ptr<spectralforge::DynamicsSpectrumPanel> compressorSpectrum;
    int detailedOwner{-1},detailedModel{-1};
    bool syncing{};
    spectralforge::ui::Changed<std::array<int,19>> refreshKey;
    spectralforge::ui::Changed<int> compressorSourceKey;
    spectralforge::ui::Changed<int> reductionKey;
    static bool isCompressorModel(int model) noexcept {
        return juce::isPositiveAndBelow(model,spectralforge::pedalModelCount)
            && spectralforge::pedalMenuEntries[(size_t)model].kind==spectralforge::PedalMenuKind::compressor;
    }
    bool compressorDetailsShowing() const noexcept {return detailedOwner>=0 && isCompressorModel(detailedModel);}
    float displayedReduction() const noexcept {
        if(!compressorDetailsShowing())return processor.pedalReduction();
        return processor.pedalBoardState().enabled?processor.pedalCompressorReduction(detailedOwner):0.f;
    }
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
    std::unique_ptr<BA> bindPower(juce::TextButton& button,int owner,int model) {
        auto* parameter=processor.parameters().getParameter(spectralforge::pedalBypassID(owner,model));
        if(!parameter)return {};
        // ButtonAttachment sends a click notification when a parameter changes.
        // A non-notifying update keeps host recall separate from a user click.
        auto attachment=std::make_unique<BA>(*parameter,[&button](float value) {
            button.setToggleState(value>.5f,juce::dontSendNotification);
            stylePowerButton(button,true,value>.5f);
        });
        attachment->sendInitialUpdate();return attachment;
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
    void addControl(Control& c,juce::Component* container=nullptr) {
        auto& parent=container?*container:static_cast<juce::Component&>(*this);
        parent.addAndMakeVisible(c.label);parent.addAndMakeVisible(c.slider);parent.addAndMakeVisible(c.midi);
        c.label.setJustificationType(juce::Justification::centred);c.label.setFont(juce::FontOptions(11.f));
        c.label.setColour(juce::Label::textColourId,juce::Colour(0xffdce8d2));
        c.label.setColour(juce::Label::backgroundColourId,juce::Colour(0xff151c18).withAlpha(.94f));
        spectralforge::ui::configureRotaryKnob(c.slider);
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
        c.slider.setTooltip(spec.connected ? spec.label : "This hardware function is unavailable in the plugin.");
        // SliderAttachment installs its own text functions. Apply panel units
        // after attachment so host normalization cannot overwrite the display.
        const auto id=spectralforge::pedalControlID(owner,model,index);c.slider.setComponentID(id);
        c.attachment=std::make_unique<SA>(processor.parameters(),id,c.slider);
        const auto names=juce::StringArray::fromTokens(spec.choices,"|",{});
        if(juce::String(spec.choices).isNotEmpty()) {
            c.slider.textFromValueFunction=[names](double value){return names[juce::jlimit(0,names.size()-1,juce::roundToInt(value))];};
            c.slider.valueFromTextFunction=[names](const juce::String& text){const int i=names.indexOf(text,true);return i>=0?double(i):text.getDoubleValue();};
        }
        else if(model==40 && index%3==0 && index<9) {
            // Para EQ frequency knobs: logarithmic across each band, shown in Hz.
            const int band=index/3;
            c.slider.textFromValueFunction=[band](double value){const float hz=spectralforge::PedalParaEQ::frequency(band,float(value));return hz>=1000.f?juce::String(hz/1000.f,2)+" kHz":juce::String(juce::roundToInt(hz))+" Hz";};
            c.slider.valueFromTextFunction=[band](const juce::String& text){
                const auto& range=spectralforge::PedalParaEQ::bands[size_t(band)];
                const double hz=text.getDoubleValue()*(text.containsIgnoreCase("k")?1000.:1.);
                return juce::jlimit(0.,1.,std::log(juce::jlimit(double(range.low),double(range.high),hz)/range.low)/std::log(double(range.high)/range.low));
            };
        }
        else if(model==41 && index==0) {
            c.slider.textFromValueFunction=[](double value){return juce::String(value,1)+":1";};
            c.slider.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue();};
        }
        else if(model==45 && index==7) {
            c.slider.textFromValueFunction=[](double value){const float hz=spectralforge::PedalDreadfulPeril::midHz(float(value));return hz>=1000.f?juce::String(hz/1000.f,2)+" kHz":juce::String(juce::roundToInt(hz))+" Hz";};
            c.slider.valueFromTextFunction=[](const juce::String& text){const double hz=juce::jlimit(200.,2500.,text.getDoubleValue()*(text.containsIgnoreCase("k")?1000.:1.));return std::log(hz/200.)/std::log(2500./200.);};
        }
        else if(model==40 || model==41 || model==42 || (model==43 && index==1) || (model==45 && index>=2 && index<=6) || model==46 || (model==47 && index==3) || (model==48 && (index==0 || index==7))) {
            // Para EQ gains and boost, Multi Comp gain, sens and balance are in dB.
            c.slider.textFromValueFunction=[](double value){return (value>0?"+":"")+juce::String(value,1)+" dB";};
            c.slider.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue();};
        }
        else if(spec.minimum==0 && spec.maximum==1) {
            c.slider.textFromValueFunction=[](double value){return juce::String(value*10.,1);};
            c.slider.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()/10.;};
        }
        c.slider.updateText();
        c.midi.onClick=[this,id]{processor.learnMidi(id);notice.setText("Move a MIDI controller to bind this instance/control.",juce::dontSendNotification);};
    }
    void showDetails(int owner) {
        if(owner<0 || owner>=spectralforge::pedalBoardCapacity)owner=-1;
        const auto state=processor.pedalBoardState();detailedOwner=owner;detailedModel=owner<0 ? -1 : state.instances[(size_t)owner].model;
        detailBypassAttachment.reset();
        if(owner>=0) {
            detailTitle.setText(spectralforge::pedalMenuName(detailedModel),juce::dontSendNotification);
            detailReference.setText(spectralforge::pedalDescription(detailedModel),juce::dontSendNotification);
            detailReference.setTooltip(detailReference.getText());
            for(int i=0;i<spectralforge::pedalMaxControls;++i)bind(details[(size_t)i],owner,detailedModel,i,true);
            detailBypassAttachment=bindPower(detailBypass,owner,detailedModel);
        }
        if(compressorDetailsShowing()) {
            // This selector controls observation only. Viewing a bypassed pedal
            // or an inactive board must never enable or rewrite the audio path.
            if(!compressorSpectrum) {
                compressorSpectrum=std::make_unique<spectralforge::DynamicsSpectrumPanel>(processor.dynamicsAnalysis(),processor.eqCaptureBoundary());
                compressorSpectrum->setComponentID("boardCompressorSpectrum");
                compressorSpectrum->setStage(spectralforge::DynamicsStage::preCompressor);
                addChildComponent(*compressorSpectrum);
            }
            compressorSpectrum->setPreOwner(owner);
            compressorSpectrum->sourceChanged();
        }
        if(compressorSpectrum)compressorSpectrum->setVisible(compressorDetailsShowing());
        detailViewport.setViewPosition(0,0);
        refresh();resized();repaint();
    }
    void timerCallback() override {refresh(false);}
    void refresh(bool force=true) {
        if(!spectralforge::ui::visible(*this)){refreshKey.dirty=true;return;}
        if(reductionKey.update(spectralforge::ui::tenth(displayedReduction())))repaint(838,5,96,29);
        const auto state=processor.pedalBoardState();
        const bool matrix=processor.parameters().getRawParameterValue("mode")->load()==2;
        if(compressorDetailsShowing() && compressorSpectrum && compressorSourceKey.update(state.enabled?1:0))
            compressorSpectrum->sourceChanged();
        std::array<int,19> key{{matrix?1:0,detailedOwner,detailedModel}};key[18]=state.enabled?1:0;
        for(size_t i=0;i<5;++i){key[3+i*3]=state.order[i];key[4+i*3]=state.instances[i].model;key[5+i*3]=state.instances[i].bypass?1:0;}
        if(!refreshKey.update(key) && !force)return;
        syncing=true;
        if(detailedOwner>=0 && state.instances[(size_t)detailedOwner].model!=detailedModel) {syncing=false;showDetails(-1);return;}
        bool layoutChanged=false;int count=0;for(const auto& e:state.instances)if(e.model)++count;
        tap.setEnabled(matrix);tapLabel.setText(matrix ? "LOW TAP after slot" : "LOW TAP / Matrix only",juce::dontSendNotification);
        for(int position=0;position<5;++position) {
            auto& card=cards[(size_t)position];const int owner=state.order[(size_t)position],model=state.instances[(size_t)owner].model;
            if(card.owner!=owner || card.modelId!=model) {
                layoutChanged=true;
                const bool ownerChanged=card.owner!=owner;
                card.owner=owner;card.modelId=model;
                card.reference.setText(spectralforge::pedalDescription(model),juce::dontSendNotification);
                card.reference.setTooltip(card.reference.getText());
                if(ownerChanged)card.model.resetSyncExplicit(model+1);else card.model.syncSelectedId(model+1);
                card.attachment.reset();
                if(model)card.attachment=bindPower(card.bypass,owner,model);
                for(int c=0;c<6;++c)bind(card.controls[(size_t)c],owner,model,cardControl(model,c),false);
                card.model.setTooltip(model==39?"Spectral octaver: about 43-46 ms processing delay. Dense low chords can lose fundamentals.":model==38?"Monophonic octave divider. Use single notes; chord tracking is not supported.":"Choose a pedal by type. Each slot keeps its own settings.");
            }
            card.title.setText("SLOT "+juce::String(position+1)+"  /  "+(model?juce::String(spectralforge::pedalModel(model).controlCount)+" CONTROLS":"ADD A PEDAL"),juce::dontSendNotification);
            const bool visible=detailedOwner<0;
            for(juce::Component* c:std::initializer_list<juce::Component*>{&card.title,&card.model,&card.bypass,&card.left,&card.right,&card.copy,&card.remove,&card.detail})c->setVisible(visible);
            card.reference.setVisible(visible&&model!=0);
            card.model.setEnabled(true);
            card.left.setEnabled(position>0);card.right.setEnabled(position<4);card.copy.setEnabled(count<5&&model!=0);card.detail.setEnabled(model!=0);
            // Compressor pedals keep their knobs on the card; DETAIL adds the spectrum.
            card.detail.setButtonText(spectralforge::pedalModel(model).controlCount>6?"ALL CONTROLS":"DETAIL / MIDI");
            card.spectrumEdit.setVisible(false);
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
        detailViewport.setVisible(detailedOwner>=0);
        if(compressorSpectrum)compressorSpectrum->setVisible(compressorDetailsShowing());
        compressorStatus.setVisible(compressorDetailsShowing());
        if(compressorDetailsShowing())compressorStatus.setText(!state.enabled?"PRE BOARD INACTIVE / stored compressor controls":
            state.instances[(size_t)detailedOwner].bypass?"COMPRESSOR BYPASSED / measured input and output":"PRE COMPRESSOR / measured input and output",juce::dontSendNotification);
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
    // Viewing an existing owner must not activate the board or change its bank.
    void navigateOwner(int owner) {if(owner>=0 && owner<5)showDetails(owner);else showDetails(-1);}
    bool openCompressorDetails(int owner=-1) {
        const auto state=processor.pedalBoardState();
        if(owner>=0) {
            if(owner>=spectralforge::pedalBoardCapacity || !isCompressorModel(state.instances[(size_t)owner].model))return false;
            showDetails(owner);return true;
        }
        if(compressorDetailsShowing() && isCompressorModel(state.instances[(size_t)detailedOwner].model)) {
            showDetails(detailedOwner);return true;
        }
        for(const int candidate:state.order)if(juce::isPositiveAndBelow(candidate,spectralforge::pedalBoardCapacity)
            && isCompressorModel(state.instances[(size_t)candidate].model)) {showDetails(candidate);return true;}
        return false;
    }
    explicit PedalBoardPanel(ChimeraProcessor& p):processor(p) {
        setComponentID("universalPedalBoard");
        addChildComponent(compressorStatus);compressorStatus.setComponentID("boardCompressorStatus");
        compressorStatus.setFont(juce::FontOptions(10.5f));compressorStatus.setJustificationType(juce::Justification::centredRight);
        compressorStatus.setColour(juce::Label::textColourId,juce::Colour(0xffc4b28e));
        for(int position=0;position<5;++position) {
            auto& card=cards[(size_t)position];
            for(juce::Component* c:std::initializer_list<juce::Component*>{&card.title,&card.reference,&card.model,&card.bypass,&card.left,&card.right,&card.copy,&card.remove,&card.detail,&card.spectrumEdit})addAndMakeVisible(c);
            card.model.setComponentID("boardModelAt"+juce::String(position));
            card.reference.setComponentID("boardReferenceAt"+juce::String(position));
            card.title.setFont(juce::FontOptions(11.f));
            card.reference.setFont(juce::FontOptions(11.f));
            card.reference.setMinimumHorizontalScale(1.f);
            card.reference.setColour(juce::Label::textColourId,juce::Colour(0xffaeb8ab));
            card.detail.setComponentID("boardDetailAt"+juce::String(position));
            card.spectrumEdit.setComponentID("boardCompressorSpectrumAt"+juce::String(position));
            card.spectrumEdit.setTooltip("Open this compressor's live input/output spectrum and all attached controls.");
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
            card.spectrumEdit.onClick=[this,position]{openCompressorDetails(cards[(size_t)position].owner);};
            card.bypass.setClickingTogglesState(true);
            card.bypass.onClick=[this,position]{
                auto& c=cards[(size_t)position];
                if(c.attachment)c.attachment->setValueAsCompleteGesture(c.bypass.getToggleState()?1.f:0.f);
                processor.setPedalBoardEnabled(true);refresh();
            };
            for(auto& c:card.controls)addControl(c);
        }
        detailViewport.setComponentID("boardDetailViewport");detailContent.setComponentID("boardDetailControls");
        addChildComponent(detailViewport);detailViewport.setViewedComponent(&detailContent,false);
        detailViewport.setScrollBarsShown(true,false);detailViewport.setScrollBarThickness(9);
        for(auto& c:details)addControl(c,&detailContent);
        for(juce::Component* c:std::initializer_list<juce::Component*>{&close,&undo,&redo,&notice,&detailTitle,&detailReference,&detailBypass,&tapLabel,&tap})addAndMakeVisible(c);
        close.setComponentID("boardDetailClose");detailTitle.setComponentID("boardDetailTitle");detailReference.setComponentID("boardDetailReference");
        detailBypass.setComponentID("boardDetailBypass");detailBypass.setClickingTogglesState(true);
        detailBypass.onClick=[this]{
            if(detailBypassAttachment)detailBypassAttachment->setValueAsCompleteGesture(detailBypass.getToggleState()?1.f:0.f);
            processor.setPedalBoardEnabled(true);refresh();
        };
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
        g.drawText("GR "+juce::String(displayedReduction(),1)+" dB",842,7,86,25,juce::Justification::centredRight);
        const auto state=processor.pedalBoardState();int poly=0;for(const auto& instance:state.instances)if(instance.model==39)++poly;
        if(poly>0 && detailedOwner<0) {const double sampleRate=processor.getSampleRate()>0?processor.getSampleRate():48000;g.setColour(juce::Colour(0xffe6bb7b));g.drawText("PRE DELAY "+juce::String(1000.0*processor.pedalBoardLatencySamples()/sampleRate,1)+" ms / spectral octave",527,7,312,25,juce::Justification::centredLeft);}
    }
    void resized() override {
        undo.setBounds(0,5,61,25);redo.setBounds(67,5,61,25);tapLabel.setBounds(145,5,168,25);tap.setBounds(313,5,200,25);notice.setBounds(0,398,1140,17);
        close.setBounds(937,5,203,27);detailBypass.setBounds(741,5,87,27);detailTitle.setBounds(14,44,1090,24);detailReference.setBounds(14,68,1090,18);
        const bool compressor=compressorDetailsShowing();
        if(compressor)notice.setBounds(0,398,620,17);
        compressorStatus.setBounds(632,398,508,17);
        if(compressor) {
            detailTitle.setBounds(14,41,520,24);detailReference.setBounds(544,43,570,20);
            if(compressorSpectrum)compressorSpectrum->setBounds(8,72,1124,236);
            detailViewport.setBounds(8,314,1124,82);
        }else detailViewport.setBounds(0,87,1140,309);
        for(int position=0;position<5;++position) {
            auto& c=cards[(size_t)position];const int x=position*230;
            c.title.setBounds(x+9,51,202,16);c.model.setBounds(x+9,68,202,29);
            c.reference.setBounds(x+9,98,202,18);
            for(int k=0;k<6;++k) {
                const int count=spectralforge::pedalModel(juce::jmax(0,c.modelId)).controlCount;
                const bool compact=count>3;
                // Five controls keep the lower centre empty: 3 above, 2 at the
                // outer columns below. Four controls retain their 2 x 2 grid.
                const int column=count==5&&k==4?2:k%3;
                const int left=count==4?x+17+(k%2)*107:compact?x+7+column*70:x+(count==1 || k==2?71:17+k*107);
                const int top=count==4?121+(k/2)*96:compact?121+(k/3)*96:count<3?145:k==2?215:119;
                const int width=count==4?78:compact?66:78,height=compact?74:76;
                c.controls[(size_t)k].label.setBounds(left,top,width,18);c.controls[(size_t)k].slider.setBounds(left,top+18,width,height);
                c.controls[(size_t)k].slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,compact?65:78,18);
                c.controls[(size_t)k].slider.watchTextBox();
            }
            c.left.setBounds(x+9,320,31,25);c.right.setBounds(x+44,320,31,25);c.copy.setBounds(x+80,320,62,25);c.remove.setBounds(x+147,320,64,25);
            c.detail.setBounds(x+9,354,107,31);c.bypass.setBounds(x+122,354,89,31);
            c.spectrumEdit.setBounds(x+15,182,190,58);
        }
        const int count=detailedModel>=0?spectralforge::pedalModel(detailedModel).controlCount:0;
        const int columns=juce::jlimit(1,6,count),available=juce::jmax(1,detailViewport.getWidth()-11),cell=available/columns;
        detailContent.setSize(compressor?available:1140,compressor?juce::jmax(detailViewport.getHeight(),((count+columns-1)/columns)*80):309);
        for(int k=0;k<spectralforge::pedalMaxControls;++k) {
            const bool four=count==4,five=count==5;
            const int column=five&&k==4?2:k%3;
            auto& c=details[(size_t)k];
            if(compressor) {
                const int x=(k%columns)*cell,y=(k/columns)*80;
                c.label.setBounds(x+4,y,cell-8,17);
                c.slider.setBounds(x+juce::jmax(0,(cell-170)/2),y+17,105,60);
                c.slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,96,18);c.slider.watchTextBox();
                c.midi.setBounds(x+cell-63,y+34,57,24);continue;
            }
            c.slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,78,20);c.slider.watchTextBox();
            const int x=four?248+(k%2)*370:five?160+column*282:16+(k%4)*282;
            const int y=four?7+(k/2)*139:five?7+(k/3)*139:(k/4)*103;
            c.label.setBounds(x,y,190,19);c.slider.setBounds(x+34,y+20,123,79);c.midi.setBounds(x+194,y+42,61,25);
        }
    }
};
