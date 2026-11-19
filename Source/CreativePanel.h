#pragma once
#include "PluginProcessor.h"
#include "NativeControlView.h"
#include "CreativeFX.h"
#include "RasterArtwork.h"

// 1.9 CREATIVE slot (docs/CREATIVE_1_9.md). CreativeStrip is the compact row
// on the POST page (model, on/off, ALL); CreativePanel is the full editor it
// opens. Every control is attached to its host parameter; opening either
// changes nothing.
namespace spectralforge::creativeUI {
inline void styleButton(juce::TextButton& button) {
    button.setClickingTogglesState(true);
    button.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffa5c1a0));
}

class CreativePanel final : public juce::Component,private juce::Timer {
public:
    // `slotIndex` 1: the second CREATIVE slot (docs/POST_SECOND_SLOTS.md).
    explicit CreativePanel(ChimeraProcessor& owner,int slotIndex=0):processor(owner),slot(slotIndex) {
        setComponentID(slot?"creativePanel_2":"creativePanel");
        for(auto* label:{&title,&character}){addAndMakeVisible(*label);label->setColour(juce::Label::textColourId,juce::Colour(0xffe1dbce));}
        title.setText(slot?"CREATIVE 2":"CREATIVE",juce::dontSendNotification);title.setFont(juce::FontOptions(13.f,juce::Font::bold));character.setFont(juce::FontOptions(11.f));
        model.setComponentID(id("creativeModel"));model.addItemList(creativeModelNames(),1);addAndMakeVisible(model);
        modelAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters(),id("creativeModel"),model);
        enabled.setComponentID(id("creativeOn"));styleButton(enabled);addAndMakeVisible(enabled);
        enabledAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters(),id("creativeOn"),enabled);
        const std::array<const char*,6> ids{"creativeA","creativeB","creativeC","creativeMix","creativeSync","creativeOutput"};
        for(size_t i=0;i<ids.size();++i){
            auto control=std::make_unique<NativeControlView>();
            const auto& range=processor.parameters().getParameter(id(ids[i]))->getNormalisableRange();
            if(i==4)control->bind(processor.parameters(),id(ids[i]),"SYNC","CREATIVE",1,creativeSyncNames(),0,creativeSyncNames().size()-1,1,3);
            else control->bind(processor.parameters(),id(ids[i]),i==3?"MIX":i==5?"OUTPUT":"A","CREATIVE",0,{},range.start,range.end,range.interval,3);
            addAndMakeVisible(*control);controls.push_back(std::move(control));
        }
        refreshModel();timerCallback();startTimerHz(10);setSize(760,270);
    }
    ~CreativePanel() override {stopTimer();setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171b1b));
        // The model's procedural face behind the header, dimmed so the controls stay legible.
        art::rackSurface(g,{0,0,(float)getWidth(),78},art::creativeSurface(selected()));
        g.setColour(juce::Colour(0xff0d1010).withAlpha(.82f));g.fillRoundedRectangle(8,6,(float)getWidth()-16,64,4);
    }
    void resized() override {
        title.setBounds(18,8,260,18);model.setBounds(18,29,330,25);character.setBounds(360,29,getWidth()-480,25);enabled.setBounds(getWidth()-104,24,85,28);
        const int cell=(getWidth()-32)/juce::jmax(1,(int)controls.size());
        for(size_t i=0;i<controls.size();++i)controls[i]->setBounds(16+(int)i*cell,92,cell-8,154);
    }
    // The A / B / C labels name what each control does in the selected model.
    void refreshModel() {
        const auto& info=creativeModels[size_t(selected())];
        for(size_t i=0;i<3;++i){controls[i]->label.setText(info.controls[i],juce::dontSendNotification);controls[i]->label.setTooltip(juce::String("CREATIVE / ")+info.name+" / "+info.controls[i]);}
        character.setText(info.character,juce::dontSendNotification);character.setTooltip(info.character);
        shownModel=selected();repaint();
    }
    juce::String controlLabel(int index) const {return controls[size_t(index)]->label.getText();}
private:
    ChimeraProcessor& processor;int slot{};juce::Label title,character;juce::ComboBox model;juce::TextButton enabled{"ON"};
    juce::String id(const char* x) const {return postSlotID(slot,x);}
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    std::vector<std::unique_ptr<NativeControlView>> controls;int shownModel{-1};
    int selected() const {return juce::jlimit(0,creativeModelCount-1,juce::roundToInt(processor.parameters().getRawParameterValue(id("creativeModel"))->load()));}
    void timerCallback() override {
        if(selected()!=shownModel)refreshModel();
        enabled.setButtonText(enabled.getToggleState()?"ON":"OFF");
    }
};

// Compact POST-page row: CREATIVE caption, model, on/off and ALL.
class CreativeStrip final : public juce::Component,private juce::Timer {
public:
    // `slotIndex` 1: the second CREATIVE slot's strip (docs/POST_SECOND_SLOTS.md).
    explicit CreativeStrip(ChimeraProcessor& owner,int slotIndex=0):processor(owner),slot(slotIndex) {
        const juce::String suffix=slot?"_2":"";
        setComponentID("creativeStrip"+suffix);
        caption.setText(slot?"CREATIVE 2":"CREATIVE",juce::dontSendNotification);caption.setFont(juce::FontOptions(11.f,juce::Font::bold));
        caption.setColour(juce::Label::textColourId,juce::Colour(0xffe1dbce));caption.setJustificationType(juce::Justification::centredRight);addAndMakeVisible(caption);
        note.setText("AFTER DELAY / BEFORE REVERB",juce::dontSendNotification);note.setFont(juce::FontOptions(9.f));
        note.setColour(juce::Label::textColourId,juce::Colour(0xff9b968c));addAndMakeVisible(note);
        model.setComponentID("creativeStrip"+suffix+".model");model.addItemList(creativeModelNames(),1);model.setTooltip(slot?"Second CREATIVE model":"CREATIVE model (after DELAY, before REVERB)");addAndMakeVisible(model);
        modelAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters(),postSlotID(slot,"creativeModel"),model);
        enabled.setComponentID("creativeStrip"+suffix+".on");styleButton(enabled);enabled.setTooltip(slot?"Enable or bypass the second CREATIVE slot":"Enable or bypass the CREATIVE slot");addAndMakeVisible(enabled);
        enabledAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters(),postSlotID(slot,"creativeOn"),enabled);
        all.setButtonText("ALL");all.setComponentID("creativeStrip"+suffix+".all");all.setTooltip("Open all CREATIVE controls");addAndMakeVisible(all);
        all.onClick=[this]{if(onOpen)onOpen();};
        timerCallback();startTimerHz(10);
    }
    ~CreativeStrip() override {stopTimer();}
    std::function<void()> onOpen;
    void resized() override {
        auto area=getLocalBounds();
        all.setBounds(area.removeFromRight(43));area.removeFromRight(6);
        enabled.setBounds(area.removeFromRight(43));area.removeFromRight(8);
        model.setBounds(area.removeFromRight(190));area.removeFromRight(8);
        caption.setBounds(area.removeFromRight(70));area.removeFromRight(8);
        // The left edge is the POST page's drag grip; the right end leaves room
        // for the editor's slot 1 / 2 selector.
        note.setBounds(area.withTrimmedLeft(14).withTrimmedRight(52));
    }
    // Names the neighbours in the current POST order.
    void setPlacement(const juce::String& text) {if(note.getText()!=text)note.setText(text,juce::dontSendNotification);}
    juce::ComboBox& modelBox() noexcept {return model;}
private:
    ChimeraProcessor& processor;int slot{};juce::Label caption,note;juce::ComboBox model;juce::TextButton enabled{"OFF"},all;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    void timerCallback() override {enabled.setButtonText(enabled.getToggleState()?"ON":"OFF");}
};
} // namespace spectralforge::creativeUI
