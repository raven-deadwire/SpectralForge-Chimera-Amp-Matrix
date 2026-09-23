#include "PluginEditor.h"
ChimeraAmpMatrixAudioProcessorEditor::ChimeraAmpMatrixAudioProcessorEditor(ChimeraAmpMatrixAudioProcessor&p):AudioProcessorEditor(&p){
 title.setText("SpectralForge | CHIMERA AMP MATRIX",juce::dontSendNotification);title.setJustificationType(juce::Justification::centred);title.setFont(juce::FontOptions(24.f).withStyle("Bold"));addAndMakeVisible(title);
 mode.addItemList({"Classic","Dual","Matrix"},1);addAndMakeVisible(mode);
 for(auto*s:{&x1,&x2}){s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s->setTextBoxStyle(juce::Slider::TextBoxBelow,false,80,22);s->setTextValueSuffix(" Hz");addAndMakeVisible(*s);}
 auto& st=p.parameters();modeA=std::make_unique<ComboAttachment>(st,"mode",mode);x1A=std::make_unique<SliderAttachment>(st,"x1",x1);x2A=std::make_unique<SliderAttachment>(st,"x2",x2);setSize(900,520);
}
void ChimeraAmpMatrixAudioProcessorEditor::paint(juce::Graphics&g){g.fillAll(juce::Colour(0xff111318));g.setColour(juce::Colour(0xffd4d7dc));g.setFont(14.f);g.drawText("ROUTING",50,105,140,24,juce::Justification::centredLeft);g.drawText("LOW / MID",330,105,120,24,juce::Justification::centred);g.drawText("MID / HIGH",500,105,120,24,juce::Justification::centred);}
void ChimeraAmpMatrixAudioProcessorEditor::resized(){title.setBounds(30,25,getWidth()-60,45);mode.setBounds(50,135,180,34);x1.setBounds(330,135,120,120);x2.setBounds(500,135,120,120);}
