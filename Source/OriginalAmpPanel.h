#pragma once
#include "OriginalAmpState.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace spectralforge::original {
// Self-contained development panel. Production amp selection/undo/full-rig
// preset recall is a separate integration step, not implied by this component.
class OriginalAmpPanel : public juce::Component {
    juce::AudioProcessorValueTreeState& parameters;
    int context;
    juce::Label title;
    juce::ToggleButton enabled{"ON"};
    juce::ComboBox preset;
    std::array<juce::Slider,controlCount> sliders;
    std::array<juce::Label,controlCount> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,controlCount> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment;
public:
    OriginalAmpPanel(juce::AudioProcessorValueTreeState& state,int bank):parameters(state),context(juce::jlimit(0,contextCount-1,bank)) {
        title.setText(juce::String::fromUTF8("Náströnd"),juce::dontSendNotification);addAndMakeVisible(title);
        addAndMakeVisible(enabled);enableAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(parameters,parameterPrefix(context)+"enabled",enabled);
        preset.setTextWhenNothingSelected("Original presets");
        for(std::size_t i=0;i<presets.size();++i)preset.addItem(juce::String::fromUTF8(presets[i].name),int(i)+1);
        preset.onChange=[this]{selectPreset(preset.getSelectedId()-1);};addAndMakeVisible(preset);
        for(std::size_t i=0;i<controlCount;++i) {
            auto& slider=sliders[i];slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,76,22);
            slider.setComponentID(parameterID(context,i));addAndMakeVisible(slider);
            labels[i].setText(controls[i].label,juce::dontSendNotification);labels[i].setJustificationType(juce::Justification::centred);addAndMakeVisible(labels[i]);
            attachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters,parameterID(context,i),slider);
            // Attachments install their own text conversion; override afterwards.
            if(i!=std::size_t(Control::midFrequency)) {
                slider.textFromValueFunction=[](double v){return juce::String(v*10,1);};
                slider.valueFromTextFunction=[](const juce::String& s){return s.getDoubleValue()/10;};
            } else {
                slider.textFromValueFunction=[](double v){return juce::String(juce::roundToInt(v))+" Hz";};
                slider.valueFromTextFunction=[](const juce::String& s){return s.getDoubleValue();};
            }
            slider.updateText();
        }
        setSize(720,390);
    }
    void selectPreset(int index) {
        if(index<0||index>=int(presets.size()))return;
        preset.setSelectedId(index+1,juce::dontSendNotification);
        for(std::size_t c=0;c<controlCount;++c)if(auto* p=parameters.getParameter(parameterID(context,c))) {
            p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(presets[std::size_t(index)].state.values[c]));p->endChangeGesture();
        }
    }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff15191d)); }
    void resized() override {
        title.setBounds(10,6,180,30);enabled.setBounds(190,6,60,30);preset.setBounds(getWidth()-210,6,200,30);
        const int cell=getWidth()/5,height=(getHeight()-46)/3;
        for(std::size_t i=0;i<controlCount;++i) {
            // Conventional controls in two rows, all five macros in the last.
            const int row=i<8?int(i)/4:2,col=i<8?int(i)%4:int(i)-8;
            const int width=i<8?getWidth()/4:cell;
            labels[i].setBounds(col*width,46+row*height,width,20);
            sliders[i].setBounds(col*width,66+row*height,width,height-20);
        }
    }
};
}
