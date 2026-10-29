#pragma once
#include "PluginProcessor.h"
#include "NativeControlView.h"
#include "SynthLayer.h"

// 1.10 SYNTH layer (docs/SYNTH_1_10.md). SynthStrip is the compact row on the
// HEAD / CAB page (layer, on/off, ALL); SynthPanel is the full editor, with a
// live readout of the tracked pitch. Opening either changes nothing.
namespace spectralforge::synthUI {
inline juce::String noteName(double hz) {
    if(hz<=0)return "--";
    const double midi=69+12*std::log2(hz/440.0);const int note=juce::roundToInt(midi);
    static const char* names[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    const int cents=juce::roundToInt(100*(midi-note));
    return juce::String(names[(note%12+12)%12])+juce::String(note/12-1)+(cents>=0?" +":" ")+juce::String(cents)+" ct";
}

class SynthPanel final : public juce::Component,private juce::Timer {
public:
    explicit SynthPanel(ChimeraProcessor& owner):processor(owner) {
        setComponentID("synthPanel");
        for(auto* label:{&title,&character,&readout}){addAndMakeVisible(*label);label->setColour(juce::Label::textColourId,juce::Colour(0xffe1dbce));}
        title.setText("SYNTH LAYER",juce::dontSendNotification);title.setFont(juce::FontOptions(13.f,juce::Font::bold));
        character.setFont(juce::FontOptions(11.f));readout.setFont(juce::FontOptions(11.f));readout.setJustificationType(juce::Justification::centredRight);
        model.setComponentID("synthModel");model.addItemList(synthModelNames(),1);addAndMakeVisible(model);
        modelAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters(),"synthModel",model);
        enabled.setComponentID("synthOn");enabled.setClickingTogglesState(true);enabled.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffa5c1a0));addAndMakeVisible(enabled);
        enabledAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters(),"synthOn",enabled);
        struct Spec{const char* id;const char* label;int kind;};
        static constexpr std::array<Spec,10> specs{{{"synthSource","SOURCE",1},{"synthLevel","LEVEL",0},{"synthOctave","OCTAVE",0},{"synthTone","TONE",0},{"synthCharacter","CHARACTER",0},
            {"synthAttack","ATTACK",0},{"synthRelease","RELEASE",0},{"synthGlide","GLIDE",0},{"synthSensitivity","THRESHOLD",0}}};
        for(const auto& spec:specs){
            if(spec.id==nullptr)continue;
            auto control=std::make_unique<NativeControlView>();const auto& range=processor.parameters().getParameter(spec.id)->getNormalisableRange();
            if(spec.kind==1)control->bind(processor.parameters(),spec.id,spec.label,"SYNTH",1,synthSourceNames(),0,2,1,3);
            else control->bind(processor.parameters(),spec.id,spec.label,"SYNTH",0,{},range.start,range.end,range.interval,3);
            addAndMakeVisible(*control);controls.push_back(std::move(control));
        }
        refreshModel();timerCallback();startTimerHz(15);setSize(900,290);
    }
    ~SynthPanel() override {stopTimer();setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171b1b));
        g.setColour(juce::Colour(synthModels[size_t(selected())].colour).withAlpha(.55f));g.fillRoundedRectangle(8,6,(float)getWidth()-16,64,4);
        g.setColour(juce::Colour(0xff0d1010).withAlpha(.78f));g.fillRoundedRectangle(10,8,(float)getWidth()-20,60,4);
    }
    void resized() override {
        title.setBounds(18,8,260,18);model.setBounds(18,29,300,25);character.setBounds(330,29,getWidth()-640,25);readout.setBounds(getWidth()-310,29,190,25);enabled.setBounds(getWidth()-104,24,85,28);
        const int cell=(getWidth()-32)/juce::jmax(1,(int)controls.size());
        for(size_t i=0;i<controls.size();++i)controls[i]->setBounds(16+(int)i*cell,92,cell-8,170);
    }
    void refreshModel() {
        const auto& info=synthModels[size_t(selected())];
        controls[3]->label.setText(info.toneLabel,juce::dontSendNotification);controls[4]->label.setText(info.characterLabel,juce::dontSendNotification);
        character.setText(info.character,juce::dontSendNotification);character.setTooltip(info.character);shownModel=selected();repaint();
    }
    juce::String controlLabel(int index) const {return controls[size_t(index)]->label.getText();}
    juce::String readoutText() const {return readout.getText();}
private:
    ChimeraProcessor& processor;juce::Label title,character,readout;juce::ComboBox model;juce::TextButton enabled{"ON"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    std::vector<std::unique_ptr<NativeControlView>> controls;int shownModel{-1};
    int selected() const {return juce::jlimit(0,synthModelCount-1,juce::roundToInt(processor.parameters().getRawParameterValue("synthModel")->load()));}
    void timerCallback() override {
        if(selected()!=shownModel)refreshModel();
        enabled.setButtonText(enabled.getToggleState()?"ON":"OFF");
        const double hz=processor.synthTrackedFrequency();
        readout.setText(enabled.getToggleState()?"TRACKING  "+noteName(hz):"TRACKING OFF",juce::dontSendNotification);
    }
};

class SynthStrip final : public juce::Component,private juce::Timer {
public:
    explicit SynthStrip(ChimeraProcessor& owner):processor(owner) {
        setComponentID("synthStrip");
        caption.setText("SYNTH",juce::dontSendNotification);caption.setFont(juce::FontOptions(11.f,juce::Font::bold));
        caption.setColour(juce::Label::textColourId,juce::Colour(0xffe1dbce));caption.setJustificationType(juce::Justification::centredRight);addAndMakeVisible(caption);
        note.setText("GUITAR / MIDI LAYER AFTER RIG SUM",juce::dontSendNotification);note.setFont(juce::FontOptions(9.f));
        note.setColour(juce::Label::textColourId,juce::Colour(0xff9b968c));addAndMakeVisible(note);
        model.setComponentID("synthStrip.model");model.addItemList(synthModelNames(),1);model.setTooltip("SYNTH layer, played by the guitar and/or MIDI");addAndMakeVisible(model);
        modelAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters(),"synthModel",model);
        enabled.setComponentID("synthStrip.on");enabled.setClickingTogglesState(true);enabled.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffa5c1a0));
        enabled.setTooltip("Enable or bypass the SYNTH layer");addAndMakeVisible(enabled);
        enabledAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters(),"synthOn",enabled);
        all.setButtonText("ALL");all.setComponentID("synthStrip.all");all.setTooltip("Open all SYNTH controls");addAndMakeVisible(all);all.onClick=[this]{if(onOpen)onOpen();};
        timerCallback();startTimerHz(10);
    }
    ~SynthStrip() override {stopTimer();}
    std::function<void()> onOpen;
    void resized() override {
        auto area=getLocalBounds();
        all.setBounds(area.removeFromRight(43));area.removeFromRight(6);enabled.setBounds(area.removeFromRight(43));area.removeFromRight(8);
        model.setBounds(area.removeFromRight(190));area.removeFromRight(8);caption.setBounds(area.removeFromRight(60));area.removeFromRight(8);note.setBounds(area);
    }
    juce::ComboBox& modelBox() noexcept {return model;}
private:
    ChimeraProcessor& processor;juce::Label caption,note;juce::ComboBox model;juce::TextButton enabled{"OFF"},all;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    void timerCallback() override {enabled.setButtonText(enabled.getToggleState()?"ON":"OFF");}
};
} // namespace spectralforge::synthUI
