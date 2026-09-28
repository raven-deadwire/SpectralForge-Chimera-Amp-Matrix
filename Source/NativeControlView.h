#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// Host updates only refresh the widgets. A user gesture is the only event that
// activates a native bank, so simply opening an older project is non-mutating.
class NativeControlView final : public juce::Component {
public:
    juce::Label label,group;
    juce::Slider slider;
    juce::ComboBox choice;
    juce::TextButton toggle;
    std::function<void()> activate;
    NativeControlView() {
        for(auto* l:{&label,&group}) {addAndMakeVisible(*l);l->setJustificationType(juce::Justification::centred);l->setColour(juce::Label::textColourId,juce::Colour(0xffe1dbce));}
        label.setFont(juce::FontOptions(10.5f));group.setFont(juce::FontOptions(8.5f));group.setColour(juce::Label::textColourId,juce::Colour(0xffa4a79f));
        addChildComponent(slider);addChildComponent(choice);addChildComponent(toggle);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,18);
        slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff111615));
        toggle.setClickingTogglesState(true);
    }
    void bind(juce::AudioProcessorValueTreeState& state,const juce::String& id,const juce::String& name,const juce::String& section,
              int kind,const juce::StringArray& options,double minimum,double maximum,double interval,int knobStyle) {
        attachment.reset();type=kind;labels=options;
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
        slider.onDragStart=[this]{if(activate)activate();dragging=true;if(attachment)attachment->beginGesture();};
        slider.onValueChange=[this]{if(!attachment)return;if(activate)activate();if(dragging)attachment->setValueAsPartOfGesture((float)slider.getValue());else attachment->setValueAsCompleteGesture((float)slider.getValue());};
        slider.onDragEnd=[this]{if(attachment)attachment->endGesture();dragging=false;};
        choice.onChange=[this]{if(activate)activate();if(attachment)attachment->setValueAsCompleteGesture((float)(choice.getSelectedId()-1));};
        toggle.onClick=[this]{if(activate)activate();if(attachment)attachment->setValueAsCompleteGesture(toggle.getToggleState()?1.f:0.f);updateToggle();};
    }
    void resized() override {
        group.setBounds(2,0,getWidth()-4,13);label.setBounds(2,13,getWidth()-4,27);
        slider.setBounds(2,39,getWidth()-4,getHeight()-41);
        choice.setBounds(5,47,getWidth()-10,27);toggle.setBounds(9,47,getWidth()-18,27);
    }
private:
    int type{};bool dragging{};juce::StringArray labels;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    void updateToggle(){toggle.setButtonText(labels.size()>1?labels[toggle.getToggleState()?1:0]:toggle.getToggleState()?"ON":"OFF");}
};
