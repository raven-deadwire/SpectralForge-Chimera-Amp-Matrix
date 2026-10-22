#pragma once
#include "PluginProcessor.h"
#include "DynamicsSpectrumPanel.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace spectralforge {

// The Matrix LOW overview opens this focused editor instead of exposing an
// isolated compression knob. Its display tap brackets only the LOW compressor;
// the DI/AMP mix remains the released downstream routing control.
class LowCompressorPanel final : public juce::Component,private juce::Timer {
public:
    explicit LowCompressorPanel(ChimeraProcessor& owner)
        :processor(owner),spectrum(owner.dynamicsAnalysis(),owner.eqCaptureBoundary()) {
        setComponentID("matrixLowCompressor.panel");setOpaque(true);
        spectrum.setComponentID("matrixLowCompressor.spectrum");addAndMakeVisible(spectrum);
        setup(amount,amountLabel,"lowcomp","LOW COMPRESSION");setup(blend,blendLabel,"lowampmix","DI / AMP BLEND");
        amount.setTooltip("Linked RMS compression on the clean Matrix LOW DI branch. Level is set separately.");
        blend.setTooltip("0% AMP uses the clean LOW branch; 100% uses the LOW amplifier. This downstream blend follows the measured compressor.");
        refresh();startTimerHz(20);
    }
    ~LowCompressorPanel() override {stopTimer();}
    void resized() override {
        spectrum.setBounds(8,49,getWidth()-16,std::max(180,getHeight()-156));
        const int y=getHeight()-94,width=(getWidth()-54)/2;
        amountLabel.setBounds(18,y,width,19);amount.setBounds(18,y+23,width,33);
        blendLabel.setBounds(36+width,y,width,19);blend.setBounds(36+width,y+23,width,33);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff121a1d));g.setColour(juce::Colour(0xffd9ebe3));g.setFont(juce::FontOptions(18.f,juce::Font::bold));
        g.drawText("MATRIX LOW COMPRESSION",18,10,405,29,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(13.f));g.setColour(juce::Colour(0xff9fb4ad));
        g.drawText(matrix?"Clean LOW DI branch":"Matrix routing inactive",425,12,std::max(1,getWidth()-630),25,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffd3b478));g.drawText("GR "+juce::String(reduction,1)+" dB",getWidth()-198,12,180,25,juce::Justification::centredRight);
        g.setColour(juce::Colour(0xff9fb4ad));g.setFont(juce::FontOptions(11.5f));
        g.drawText("IN / OUT measures the LOW compressor. DI / AMP blend is applied after this stage.",18,getHeight()-26,getWidth()-36,19,juce::Justification::centredLeft);
    }
private:
    using Attachment=juce::AudioProcessorValueTreeState::SliderAttachment;
    void setup(juce::Slider& slider,juce::Label& label,const char* id,const char* name) {
        label.setText(name,juce::dontSendNotification);label.setFont(juce::FontOptions(12.f));label.setComponentID(juce::String(id)+".label");addAndMakeVisible(label);
        slider.setComponentID(id);slider.setName(name);slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,112,29);slider.setWantsKeyboardFocus(true);addAndMakeVisible(slider);
        attachments.push_back(std::make_unique<Attachment>(processor.parameters(),id,slider));
        const bool mix=juce::String(id)=="lowampmix";
        slider.textFromValueFunction=[mix](double value){return juce::String(value*100,1)+(mix?"% AMP":"%");};
        slider.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()/100.;};slider.updateText();
    }
    void refresh() {
        const bool active=processor.parameters().getRawParameterValue("mode")->load()>1.5f;
        if(active!=matrix || firstRefresh) {
            matrix=active;firstRefresh=false;spectrum.setStage(matrix?DynamicsStage::lowCompressor:DynamicsStage::none);repaint();
        }
        const float measured=matrix?processor.lowCompMeter():0.f,next=std::isfinite(measured)?measured:0.f;
        if(std::abs(next-reduction)>.049f){reduction=next;repaint(getWidth()-205,7,194,34);}
    }
    void timerCallback() override {if(isShowing())refresh();}
    ChimeraProcessor& processor;DynamicsSpectrumPanel spectrum;juce::Slider amount,blend;juce::Label amountLabel,blendLabel;
    std::vector<std::unique_ptr<Attachment>> attachments;bool matrix{},firstRefresh{true};float reduction{};
};
}
