#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// Opt-in head readout. Keep the editor and rotary geometry stable while hiding
// idle numbers; ordinary detail-panel sliders retain their existing behaviour.
class NativeControlSlider final : public juce::Slider, private juce::Timer {
public:
    std::function<void(bool)> onReadoutVisibility;
    std::function<bool()> canInteract;
    void setInteractionReadout(bool enabled) {transient=enabled;setReadoutVisible(false);}
    bool isReadoutVisible() const {return transient && readout;}
    void beginValueAdjustment() {if(transient){stopTimer();pointerActive=true;setReadoutVisible(true);}}
    void endValueAdjustment() {if(transient){pointerActive=false;if(!editing){stopTimer();setReadoutVisible(false);}}}
    void cancelValueInteraction() {
        if(!transient)return;
        stopTimer();pointerActive=false;hideTextBox(true);editing=false;setReadoutVisible(false);
    }
    void readoutEditorShown() {if(transient){stopTimer();editing=true;setReadoutVisible(true);}}
    void readoutEditorHidden() {if(transient){editing=false;if(!pointerActive)setReadoutVisible(false);}}
    void showTextBox() {
        // Label moves keyboard focus to its editor before onEditorShow fires.
        // Mark editing first so the slider's focusLost cannot hide that editor.
        if(transient){if(canInteract && !canInteract())return;readoutEditorShown();}
        juce::Slider::showTextBox();
    }
    void mouseDoubleClick(const juce::MouseEvent& event) override {
        if(transient)showTextBox();else juce::Slider::mouseDoubleClick(event);
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if(transient && canInteract && !canInteract())return false;
        if(transient && (key==juce::KeyPress::returnKey || key==juce::KeyPress::F2Key)){showTextBox();return true;}
        const bool handled=juce::Slider::keyPressed(key);
        if(transient && handled){setReadoutVisible(true);startTimer(800);}
        return handled;
    }
    void focusLost(FocusChangeType cause) override {juce::Slider::focusLost(cause);if(transient && !editing && !pointerActive){stopTimer();setReadoutVisible(false);}}
    void lookAndFeelChanged() override {
        // JUCE replaces the text label on geometry/theme changes without an
        // editor-hidden callback. Cancel the unfinished entry before replacing it.
        if(transient && editing){hideTextBox(true);editing=false;setReadoutVisible(pointerActive);}
        juce::Slider::lookAndFeelChanged();if(transient)syncReadout();
    }
private:
    bool transient{},readout{},pointerActive{},editing{};
    void timerCallback() override {stopTimer();if(!pointerActive && !editing)setReadoutVisible(false);}
    void syncReadout() {for(auto* child:getChildren())if(auto* value=dynamic_cast<juce::Label*>(child))value->setVisible(!transient || readout);}
    void setReadoutVisible(bool visible) {readout=visible;syncReadout();if(onReadoutVisibility)onReadoutVisibility(visible);}
};

// Host updates only refresh the widgets. A user gesture is the only event that
// activates a native bank, so simply opening an older project is non-mutating.
class NativeControlView final : public juce::Component {
public:
    juce::Label label,group;
    NativeControlSlider slider;
    juce::ComboBox choice;
    juce::TextButton toggle;
    std::function<void()> activate;
    std::function<bool()> canEdit;
    ~NativeControlView() override {
        finishInteraction();
        // Slider can end its own drag while its members are being destroyed.
        // The attachment and our flags have already gone away by that point.
        slider.onDragStart=nullptr;slider.onDragEnd=nullptr;slider.onValueChange=nullptr;
        slider.onReadoutVisibility=nullptr;slider.canInteract=nullptr;
    }
    void finishInteraction(bool dismissEditor=true) {if(dragging && attachment)attachment->endGesture();dragging=false;if(dismissEditor)slider.cancelValueInteraction();else slider.endValueAdjustment();}
    void visibilityChanged() override {if(!isShowing())finishInteraction();}

    NativeControlView() {
        slider.canInteract=[this]{return !canEdit || canEdit();};
        for(auto* l:{&label,&group}) {addAndMakeVisible(*l);l->setJustificationType(juce::Justification::centred);l->setColour(juce::Label::textColourId,juce::Colour(0xffe1dbce));}
        label.setFont(juce::FontOptions(10.5f));group.setFont(juce::FontOptions(8.5f));group.setColour(juce::Label::textColourId,juce::Colour(0xffa4a79f));
        addChildComponent(slider);addChildComponent(choice);addChildComponent(toggle);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,18);
        slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff111615));
        toggle.setClickingTogglesState(true);
    }
    void bind(juce::AudioProcessorValueTreeState& state,const juce::String& id,const juce::String& name,const juce::String& section,
              int kind,const juce::StringArray& options,double minimum,double maximum,double interval,int knobStyle) {
        finishInteraction();attachment.reset();type=kind;labels=options;
        label.setText(name,juce::dontSendNotification);label.setTooltip(section+" / "+name);group.setText(section,juce::dontSendNotification);
        setComponentID(id+"_control");slider.setComponentID(id);choice.setComponentID(id);toggle.setComponentID(id);
        slider.setName(name);choice.setName(name);toggle.setName(name);
        slider.setTooltip(section+" / "+name);choice.setTooltip(section+" / "+name);toggle.setTooltip(section+" / "+name);
        slider.setVisible(type==0);choice.setVisible(type==1);toggle.setVisible(type==2);
        slider.setRange(minimum,maximum,interval);slider.setNumDecimalPlacesToDisplay(maximum<=1.0?2:1);slider.getProperties().set("knobStyle",knobStyle);
        choice.clear(juce::dontSendNotification);choice.addItemList(options,1);
        if(auto* parameter=state.getParameter(id)) {
            attachment=std::make_unique<juce::ParameterAttachment>(*parameter,[this](float value){
                slider.setValue(value,juce::dontSendNotification);choice.setSelectedId(juce::roundToInt(value)+1,juce::dontSendNotification);
                toggle.setToggleState(value>.5f,juce::dontSendNotification);updateToggle();
            });
            attachment->sendInitialUpdate();
        }
        slider.onDragStart=[this]{finishInteraction();if(canEdit && !canEdit())return;if(activate)activate();dragging=true;if(attachment)attachment->beginGesture();slider.beginValueAdjustment();};
        slider.onValueChange=[this]{if(!attachment)return;if(canEdit && !canEdit()){finishInteraction();attachment->sendInitialUpdate();return;}if(activate)activate();if(dragging)attachment->setValueAsPartOfGesture((float)slider.getValue());else attachment->setValueAsCompleteGesture((float)slider.getValue());};
        slider.onDragEnd=[this]{finishInteraction(false);};
        choice.onChange=[this]{if(activate)activate();if(attachment)attachment->setValueAsCompleteGesture((float)(choice.getSelectedId()-1));};
        toggle.onClick=[this]{if(activate)activate();if(attachment)attachment->setValueAsCompleteGesture(toggle.getToggleState()?1.f:0.f);updateToggle();};
    }
    void resized() override {
        if(compact) {
            group.setVisible(false);label.setBounds(2,0,getWidth()-4,17);
            slider.setBounds(2,17,getWidth()-4,getHeight()-18);
            choice.setBounds(4,25,getWidth()-8,26);toggle.setBounds(6,25,getWidth()-12,26);return;
        }
        group.setVisible(true);
        group.setBounds(2,0,getWidth()-4,13);label.setBounds(2,13,getWidth()-4,27);
        slider.setBounds(2,39,getWidth()-4,getHeight()-41);
        choice.setBounds(5,47,getWidth()-10,27);toggle.setBounds(9,47,getWidth()-18,27);
    }
    void setCompact(bool value) {
        compact=value;slider.setTextBoxStyle(compact?juce::Slider::TextBoxRight:juce::Slider::TextBoxBelow,false,compact?62:82,18);resized();
    }
    void showHardwarePosition(double displayMin=0,double displayMax=10) {
        const double lo=slider.getMinimum(),hi=slider.getMaximum();
        slider.textFromValueFunction=[=](double value){return juce::String(displayMin+(value-lo)/(hi-lo)*(displayMax-displayMin),1);};
        slider.valueFromTextFunction=[=](const juce::String& text){return lo+(text.getDoubleValue()-displayMin)/(displayMax-displayMin)*(hi-lo);};
        slider.updateText();
    }
private:
    int type{};bool dragging{},compact{};juce::StringArray labels;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    void updateToggle(){toggle.setButtonText(labels.size()>1?labels[toggle.getToggleState()?1:0]:toggle.getToggleState()?"ON":"OFF");}
};
