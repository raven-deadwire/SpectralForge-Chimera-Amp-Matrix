#pragma once
#include "EasyMode.h"
#include "AmpCatalog.h"
#include "GuitarSignaturePresets.h"
#include "KnobInteraction.h"
#include "PluginProcessor.h"
#include "PrecisionTunerPanel.h"
#include "RasterArtwork.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

// A beginner surface over the same sound state as the full editor: choose a
// preset, shape it with six knobs, tune, or let Auto Set level the input.
class EasyModeView final : public juce::Component, private juce::Timer {
public:
    std::function<void()> openFullEditor,toggleTuner,autoSet,autoCancel,autoUndo;
    std::function<void(int)> choosePreset,stepPreset;

    explicit EasyModeView(ChimeraProcessor& p):processor(p) {
        setComponentID("easyView");setName("Easy view");setOpaque(true);
        full.setComponentID("easy.fullEditor");full.getProperties().set("utilityDock",true);
        full.setTooltip("Open every control: routing modes, pedalboard, cabinets, EQ and racks. Your sound stays as it is.");
        full.onClick=[this]{if(openFullEditor)openFullEditor();};
        presets.setName("Preset");presets.setComponentID("easy.preset");
        presets.setTooltip("Start from a finished sound, then adjust it with the knobs below.");
        presets.onChange=[this]{if(presets.getSelectedId()>0 && choosePreset)choosePreset(presets.getSelectedId());};
        previous.setName("Previous preset");next.setName("Next preset");
        previous.onClick=[this]{if(stepPreset)stepPreset(-1);};next.onClick=[this]{if(stepPreset)stepPreset(1);};
        description.setFont(juce::FontOptions(13.f));description.setColour(juce::Label::textColourId,muted);
        description.setJustificationType(juce::Justification::topLeft);description.setComponentID("easy.presetDescription");
        autoButton.setComponentID("easy.autoSet");autoButton.getProperties().set("utilityDock",true);
        autoButton.setTooltip("Play your instrument normally for about half a minute. Auto Set adjusts input level, noise gate, tone and dynamics. UNDO restores your previous settings.");
        autoButton.onClick=[this]{if(autoBusy){if(autoCancel)autoCancel();}else if(autoSet)autoSet();};
        undo.setComponentID("easy.autoUndo");undo.getProperties().set("utilityDock",true);undo.setTooltip("Restore the settings from before Auto Set.");
        undo.onClick=[this]{if(autoUndo)autoUndo();};undo.setEnabled(false);undo.setAlpha(.4f);
        autoStatus.setFont(juce::FontOptions(12.f));autoStatus.setColour(juce::Label::textColourId,muted);
        autoStatus.setJustificationType(juce::Justification::topLeft);autoStatus.setComponentID("easy.autoStatus");
        autoStatus.setText("Not sure where to start? Press AUTO SET and play.",juce::dontSendNotification);
        tuner.setComponentID("easy.tuner");tuner.getProperties().set("utilityDock",true);
        tuner.setTooltip("Tune your instrument. The output is muted while tuning if TUNER MUTE is on.");
        tuner.onClick=[this]{if(toggleTuner)toggleTuner();};
        ampName.setFont(juce::FontOptions(13.f));ampName.setColour(juce::Label::textColourId,muted);ampName.setComponentID("easy.amp");
        for(auto* c:std::initializer_list<juce::Component*>{&full,&presets,&previous,&next,&description,&autoButton,&undo,&autoStatus,&tuner,&ampName})addAndMakeVisible(*c);

        constexpr std::array<const char*,knobCount> names{"GAIN","BASS","MIDDLE","TREBLE","REVERB","VOLUME"};
        constexpr std::array<const char*,knobCount> hints{"Clean  >  distorted","Low end and weight","Body and punch","Brightness and bite","Room and space","Overall loudness"};
        for(size_t i=0;i<knobs.size();++i) {
            auto& k=knobs[i];
            k.name.setText(names[i],juce::dontSendNotification);k.name.setFont(juce::FontOptions(18.f,juce::Font::bold));
            k.name.setColour(juce::Label::textColourId,ink);k.name.setJustificationType(juce::Justification::centred);
            k.hint.setText(hints[i],juce::dontSendNotification);k.hint.setFont(juce::FontOptions(12.5f));
            k.hint.setColour(juce::Label::textColourId,muted);k.hint.setJustificationType(juce::Justification::centredTop);
            k.slider.setName(names[i]);k.slider.setComponentID("easy.knob."+juce::String(names[i]).toLowerCase());
            spectralforge::ui::configureRotaryKnob(k.slider);k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,24);
            k.slider.getProperties().set("knobStyle",3);k.slider.setTooltip(juce::String(names[i])+": "+hints[i]);
            for(auto* c:std::initializer_list<juce::Component*>{&k.name,&k.slider,&k.hint})addAndMakeVisible(*c);
        }
        auto& state=processor.parameters();
        volumeAttachment=std::make_unique<juce::SliderParameterAttachment>(*state.getParameter("output"),knobs[5].slider);
        knobs[5].slider.setTextValueSuffix(" dB");knobs[5].slider.setNumDecimalPlacesToDisplay(1);
        bindReverb();
        refresh();startTimerHz(10);
    }
    ~EasyModeView() override {stopTimer();}

    void setBrand(juce::Image mark,juce::Image emblem) {wordmark=std::move(mark);brandEmblem=std::move(emblem);repaint();}
    // The editor supplies its cabinet room so players still see their rig.
    void setRoom(std::unique_ptr<juce::Component> view) {
        if(room)removeChildComponent(room.get());
        room=std::move(view);
        if(room){addAndMakeVisible(*room);room->setVisible(tunerPanel==nullptr);}
        resized();
    }
    // The full editor owns preset loading and its CUSTOM detection.
    void fillPresetMenu(const std::function<void(juce::ComboBox&)>& fill) {fill(presets);}
    void showPreset(int id,const juce::String& text) {
        if(id>0){if(presets.getSelectedId()!=id)presets.setSelectedId(id,juce::dontSendNotification);}
        else if(presets.getSelectedId()!=0 || presets.getText()!=text){presets.setSelectedId(0,juce::dontSendNotification);presets.setText(text,juce::dontSendNotification);}
        const auto next=id>0 ? juce::String::fromUTF8(spectralforge::selectablePresetDescription(id-1))
                             : juce::String("Your own sound. Use SAVE AS in the full editor to keep it as a file.");
        if(description.getText()!=next)description.setText(next,juce::dontSendNotification);
    }
    void setAutoSetStatus(const juce::String& text,bool busy,bool canUndo) {
        autoBusy=busy;autoButton.setButtonText(busy?"CANCEL":"AUTO SET");autoButton.getProperties().set("utilityActive",busy);autoButton.repaint();
        undo.setEnabled(canUndo && !busy);undo.setAlpha(undo.isEnabled()?1.f:.4f);
        if(text.isNotEmpty())autoStatus.setText(text,juce::dontSendNotification);
    }
    void resized() override {
        full.setBounds(1000,18,160,34);
        previous.setBounds(40,108,40,36);presets.setBounds(88,108,560,36);next.setBounds(656,108,40,36);
        description.setBounds(40,148,656,30);
        autoButton.setBounds(730,106,190,40);undo.setBounds(930,106,90,40);tuner.setBounds(1030,106,110,40);
        autoStatus.setBounds(730,150,410,30);
        if(room)room->setBounds(20,194,1140,262);
        ampName.setBounds(200,474,940,20);
        const int cell=1140/knobCount;
        for(int i=0;i<knobCount;++i) {
            auto& k=knobs[(size_t)i];const int x=20+i*cell;
            k.name.setBounds(x,498,cell,24);k.slider.setBounds(x+30,522,cell-60,150);k.hint.setBounds(x+8,676,cell-16,22);
        }
        if(tunerPanel)tunerPanel->setBounds(20,194,1140,412);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(background);
        spectralforge::art::raster(g,spectralforge::art::Surface::workbench,{0,0,1180,780});
        g.setColour(background.withAlpha(.55f));g.fillRect(0,0,1180,780);
        g.setColour(accent.withAlpha(.7f));g.fillRect(20,20,2,36);
        if(brandEmblem.isValid())g.drawImageWithin(brandEmblem,25,5,62,60,juce::RectanglePlacement::centred);
        if(wordmark.isValid())g.drawImageWithin(wordmark,96,4,365,59,juce::RectanglePlacement::centred);
        text(g,"EASY VIEW",480,24,200,22,14.f,accent);
        g.setColour(line);g.drawHorizontalLine(67,20,1160);
        const auto box=[&](juce::Rectangle<float> area){g.setColour(panel.withAlpha(.9f));g.fillRoundedRectangle(area,6);g.setColour(line);g.drawRoundedRectangle(area,6,1);};
        box({20,76,1140,108});
        text(g,"1   PICK A SOUND",40,80,400,24,15.f,ink);
        text(g,"3   GET READY",730,80,400,24,15.f,ink);
        if(!tunerPanel) {
            box({20,466,1140,276});
            text(g,"2   SHAPE IT",40,470,160,24,15.f,ink);
            const int cell=1140/knobCount;
            g.setColour(line);g.drawVerticalLine(20+4*cell,500,716);
            text(g,"AMP",40,702,4*cell-40,18,11.f,muted,juce::Justification::centred);
            text(g,"EFFECT AND OUTPUT",20+4*cell,702,2*cell,18,11.f,muted,juce::Justification::centred);
        }
        text(g,"Every knob is the same setting as in the full editor; switch views at any time without losing your sound. Pedals, two-amp rigs and EQ are in FULL EDITOR.",
             28,748,1124,24,12.f,muted);
    }
    // Checked bindings let tests and the editor confirm what each knob edits.
    juce::String boundParameter(int knob) const {return knob>=0 && knob<spectralforge::easy::roleCount ? knobs[(size_t)knob].boundID : juce::String{};}

private:
    static constexpr int knobCount=6;
    static inline const juce::Colour background{0xff0d1010},panel{0xff171b1b},line{0xff3a3a34},ink{0xffe1dbce},muted{0xffa4a79f},accent{0xffc4a678};
    static void text(juce::Graphics& g,const juce::String& value,int x,int y,int w,int h,float size,juce::Colour colour,
                     juce::Justification justification=juce::Justification::centredLeft) {
        g.setColour(colour);g.setFont(juce::FontOptions(size));g.drawText(value,x,y,w,h,justification);
    }
    struct Knob {juce::Label name,hint;juce::Slider slider;std::unique_ptr<juce::SliderParameterAttachment> attachment;juce::String boundID;};
    float raw(const char* id) const {return processor.parameters().getRawParameterValue(id)->load();}
    // Hardware-style 0-10 position, like the full editor's head knobs.
    static void showPosition(juce::Slider& slider) {
        slider.textFromValueFunction=[&slider](double v){return juce::String(slider.valueToProportionOfLength(v)*10.,1);};
        slider.valueFromTextFunction=[&slider](const juce::String& t){return slider.proportionOfLengthToValue(juce::jlimit(0.,1.,t.getDoubleValue()/10.));};
        slider.updateText();
    }
    void bindReverb() {
        // REVERB shows 0 while the reverb is off; turning it up switches the
        // reverb on, and back to 0 switches it off. The mix setting is kept.
        auto& state=processor.parameters();auto& slider=knobs[4].slider;
        auto* mix=state.getParameter("reverbmix");
        const auto range=mix->getNormalisableRange();
        slider.setNormalisableRange({(double)range.start,(double)range.end,.001});
        reverbOnParameter=state.getParameter("reverbon");reverbMixParameter=mix;
        reverbOn=std::make_unique<juce::ParameterAttachment>(*reverbOnParameter,[this](float){syncReverb();});
        reverbMix=std::make_unique<juce::ParameterAttachment>(*mix,[this](float){syncReverb();});
        slider.onDragStart=[this]{reverbGesture=true;reverbOn->beginGesture();reverbMix->beginGesture();};
        slider.onDragEnd=[this]{reverbOn->endGesture();reverbMix->endGesture();reverbGesture=false;};
        slider.onValueChange=[this]{
            if(updatingReverb)return;
            const float value=(float)knobs[4].slider.getValue();const bool on=value>.001f;
            // The knob already shows the new value; echoing the two writes
            // back mid-change would leave the slider on a stale value.
            const juce::ScopedValueSetter<bool> writing(updatingReverb,true);
            if(reverbGesture){if(on)reverbMix->setValueAsPartOfGesture(value);reverbOn->setValueAsPartOfGesture(on?1.f:0.f);}
            else {if(on)reverbMix->setValueAsCompleteGesture(value);reverbOn->setValueAsCompleteGesture(on?1.f:0.f);}
        };
        showPosition(slider);syncReverb();
    }
    void syncReverb() {
        if(updatingReverb)return;
        const juce::ScopedValueSetter<bool> guard(updatingReverb,true);
        // Read the parameters themselves: inside a change callback the raw
        // value cache may not have been updated yet.
        const auto value=[](const juce::RangedAudioParameter& p){return p.convertFrom0to1(p.getValue());};
        knobs[4].slider.setValue(value(*reverbOnParameter)>.5f?value(*reverbMixParameter):0.f,juce::dontSendNotification);
    }
    void timerCallback() override {refresh();}
    void refresh() {
        const int mode=juce::jlimit(0,2,(int)raw("mode"));const int lane=spectralforge::easy::primaryLane(mode);
        const int model=processor.selectedAmpModel(lane),channel=processor.selectedAmpChannel(lane);
        const int context=spectralforge::ampNativeContext(mode,lane);
        const bool native=spectralforge::ampRequiresNative(model)
            || processor.parameters().getRawParameterValue(spectralforge::ampNativeEnabledID(context))->load()>.5f;
        for(int role=0;role<spectralforge::easy::roleCount;++role) {
            auto& k=knobs[(size_t)role];
            const int control=native?spectralforge::easy::ampControl(model,channel,spectralforge::easy::Role(role)):-1;
            const auto id=control>=0?spectralforge::ampNativeControlID(context,model,control,channel):juce::String{};
            if(id==k.boundID)continue;
            k.attachment.reset();k.boundID=id;
            auto* parameter=id.isNotEmpty()?processor.parameters().getParameter(id):nullptr;
            if(parameter){k.attachment=std::make_unique<juce::SliderParameterAttachment>(*parameter,k.slider);showPosition(k.slider);}
            k.slider.setEnabled(parameter!=nullptr);
            k.slider.setTooltip(parameter?k.name.getText()+": "+k.hint.getText():k.name.getText()+" is not on this amp. Open FULL EDITOR to see its own controls.");
            k.hint.setAlpha(parameter?1.f:.5f);
        }
        const auto& panel=spectralforge::ampNativePanel(model);
        auto amp="AMP:  "+juce::String::fromUTF8(spectralforge::ampInfo(model).name);
        if(panel.channels.size()>1 && channel>=0 && channel<(int)panel.channels.size())amp+="  /  "+juce::String::fromUTF8(panel.channels[(size_t)channel]);
        if(mode>0)amp+=mode==1?"  /  first of two amps":"  /  middle band of three";
        if(ampName.getText()!=amp)ampName.setText(amp,juce::dontSendNotification);
        const bool tuning=raw("tuneron")>.5f;
        tuner.setToggleState(tuning,juce::dontSendNotification);tuner.setButtonText(tuning?"TUNER ON":"TUNER");
        if(tuning!=(tunerPanel!=nullptr)) {
            if(tuning){tunerPanel=std::make_unique<PrecisionTunerPanel>(processor);addAndMakeVisible(*tunerPanel);}
            else tunerPanel.reset();
            ampName.setVisible(!tuning);if(room)room->setVisible(!tuning);
            for(auto& k:knobs)for(auto* c:std::initializer_list<juce::Component*>{&k.name,&k.slider,&k.hint})c->setVisible(!tuning);
            resized();repaint();
        }
    }

    ChimeraProcessor& processor;
    juce::Image wordmark,brandEmblem;
    juce::TextButton full{"FULL EDITOR"},previous{"<"},next{">"},autoButton{"AUTO SET"},undo{"UNDO"},tuner{"TUNER"};
    juce::ComboBox presets;
    juce::Label description,autoStatus,ampName;
    std::array<Knob,knobCount> knobs;
    std::unique_ptr<juce::SliderParameterAttachment> volumeAttachment;
    juce::RangedAudioParameter* reverbOnParameter{};juce::RangedAudioParameter* reverbMixParameter{};
    std::unique_ptr<juce::ParameterAttachment> reverbOn,reverbMix;
    bool reverbGesture{},updatingReverb{},autoBusy{};
    std::unique_ptr<PrecisionTunerPanel> tunerPanel;
    std::unique_ptr<juce::Component> room;
};
