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
class EasyModeView final : public juce::Component, private juce::Timer, private juce::AsyncUpdater,
                           private juce::AudioProcessorValueTreeState::Listener {
public:
    std::function<void()> openFullEditor,toggleTuner,autoSet,autoCancel,autoUndo;
    std::function<void(int)> choosePreset,stepPreset,chooseAutoOption;

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
        autoButton.onClick=[this]{if(autoBusy){if(autoCancel)autoCancel();}else if(autoSet)autoSet();};
        autoOptions.setComponentID("easy.autoOptions");autoOptions.getProperties().set("utilityDock",true);
        autoOptions.setTooltip("Auto Set options: instrument (detected, guitar or bass), style (rock, clean or metal) and which steps run. Shared with the full editor.");
        autoOptions.onClick=[this]{showAutoOptions();};
        undo.setComponentID("easy.autoUndo");undo.getProperties().set("utilityDock",true);undo.setTooltip("Restore the settings from before the last Auto Set run.");
        undo.onClick=[this]{if(autoUndo)autoUndo();};undo.setEnabled(false);undo.setAlpha(.4f);
        autoStatus.setFont(juce::FontOptions(12.f));autoStatus.setColour(juce::Label::textColourId,muted);
        autoStatus.setJustificationType(juce::Justification::topLeft);autoStatus.setComponentID("easy.autoStatus");
        autoStatus.setText("Not sure where to start? Press AUTO SET and play.",juce::dontSendNotification);
        tuner.setComponentID("easy.tuner");tuner.getProperties().set("utilityDock",true);
        tuner.setTooltip("Tune your instrument. The output is muted while tuning if TUNER MUTE is on.");
        tuner.onClick=[this]{if(toggleTuner)toggleTuner();};
        ampName.setFont(juce::FontOptions(13.f));ampName.setColour(juce::Label::textColourId,muted);ampName.setComponentID("easy.amp");
        targetNote.setFont(juce::FontOptions(12.f,juce::Font::bold));targetNote.setColour(juce::Label::textColourId,warning);
        targetNote.setJustificationType(juce::Justification::centredRight);targetNote.setComponentID("easy.targetNote");
        for(auto* c:std::initializer_list<juce::Component*>{&full,&presets,&previous,&next,&description,&autoButton,&autoOptions,&undo,&autoStatus,&tuner,&ampName,&targetNote})addAndMakeVisible(*c);
        setAutoSetStatus({},false,false);setAutoSetOptions(0,0,true,true);

        constexpr std::array<const char*,knobCount> ids{"gain","bass","middle","treble","reverb","volume"};
        constexpr std::array<const char*,knobCount> names{"GAIN","BASS","MIDDLE","TREBLE","POST REVERB","OUTPUT"};
        constexpr std::array<const char*,knobCount> hints{"Drive and amp loudness","Low end and weight","Body and punch","Brightness and bite",
                                                          "Extra space after the amp","Final level after the limiter"};
        for(size_t i=0;i<knobs.size();++i) {
            auto& k=knobs[i];
            k.name.setText(names[i],juce::dontSendNotification);k.name.setFont(juce::FontOptions(18.f,juce::Font::bold));
            k.name.setColour(juce::Label::textColourId,ink);k.name.setJustificationType(juce::Justification::centred);
            k.hint.setText(hints[i],juce::dontSendNotification);k.hint.setFont(juce::FontOptions(12.5f));
            k.hint.setColour(juce::Label::textColourId,muted);k.hint.setJustificationType(juce::Justification::centredTop);
            k.slider.setName(names[i]);k.slider.setComponentID("easy.knob."+juce::String(ids[i]));
            spectralforge::ui::configureRotaryKnob(k.slider);k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,24);
            k.slider.getProperties().set("knobStyle",3);k.slider.setTooltip(juce::String(names[i])+": "+hints[i]);
            for(auto* c:std::initializer_list<juce::Component*>{&k.name,&k.slider,&k.hint})addAndMakeVisible(*c);
        }
        addAndMakeVisible(marker);
        knobs[4].slider.setTooltip("POST REVERB: the reverb after the amp and cabinet. 0 switches it off and keeps its mix. "
                                   "A reverb built into the amp head stays as set in FULL EDITOR.");
        knobs[5].slider.setTooltip("OUTPUT: the final output level. GAIN changes drive and amp loudness; the output limiter can hold back loud peaks.");
        // Amp knobs start unbound and disabled; the first refresh decides what they edit.
        for(int role=0;role<spectralforge::easy::roleCount;++role) {
            auto& k=knobs[(size_t)role];k.slider.setEnabled(false);
            // A host or preset can change the routing while a knob is held.
            // Rebinding then waits for the gesture to end, so the old
            // parameter always receives a matching end of its gesture.
            k.slider.onDragStart=[this,role]{knobs[(size_t)role].dragging=true;};
            k.slider.onDragEnd=[this,role]{knobs[(size_t)role].dragging=false;triggerAsyncUpdate();};
        }
        auto& state=processor.parameters();
        volumeAttachment=std::make_unique<juce::SliderParameterAttachment>(*state.getParameter("output"),knobs[5].slider);
        knobs[5].slider.setTextValueSuffix(" dB");knobs[5].slider.setNumDecimalPlacesToDisplay(1);
        bindReverb();
        for(auto* parameter:processor.getParameters())
            if(auto* withID=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);withID && routingParameter(withID->paramID)) {
                state.addParameterListener(withID->paramID,this);listened.add(withID->paramID);
            }
        refresh();startTimerHz(10);
    }
    ~EasyModeView() override {
        stopTimer();
        for(const auto& id:listened)processor.parameters().removeParameterListener(id,this);
        cancelPendingUpdate();
        // Closing the editor mid-drag must not leave a host gesture open.
        for(auto& k:knobs)if(k.dragging && k.attachment)if(auto* parameter=processor.parameters().getParameter(k.boundID))parameter->endChangeGesture();
        if(reverbGesture){reverbOn->endGesture();reverbMix->endGesture();}
    }

    void setBrand(juce::Image mark,juce::Image emblem) {wordmark=std::move(mark);brandEmblem=std::move(emblem);repaint();}
    // The editor supplies its cabinet room so players still see their rig.
    // It is view-only here: amp and cabinet windows belong to the full editor.
    // rigBounds reports each lane's rig in the room's own coordinates.
    void setRoom(std::unique_ptr<juce::Component> view,std::function<juce::Rectangle<int>(int)> bounds={}) {
        if(room)removeChildComponent(room.get());
        room=std::move(view);rigBounds=std::move(bounds);
        if(room) {
            room->setEnabled(false);room->setInterceptsMouseClicks(false,false);
            addAndMakeVisible(*room);room->setVisible(tunerPanel==nullptr);
        }
        marker.toFront(false);
        resized();
    }
    // Lane whose cabinet is under a point of this view, or -1 (e.g. over
    // the knobs). File drops use it so only a visible cabinet takes an IR.
    int laneAt(juce::Point<int> point) const {
        if(!room || !room->isVisible() || !rigBounds)return -1;
        for(int lane=0;lane<3;++lane)if(rigBounds(lane).translated(room->getX(),room->getY()).contains(point))return lane;
        return -1;
    }
    // The full editor owns preset loading and its CUSTOM detection.
    void fillPresetMenu(const std::function<void(juce::ComboBox&)>& fill) {fill(presets);}
    // origin is the factory preset an edited (CUSTOM) sound started from, or 0.
    void showPreset(int id,const juce::String& text,int origin=0) {
        const bool edited=id<=0 && origin>0 && text=="CUSTOM";
        const auto shown=edited ? juce::String::fromUTF8(spectralforge::selectablePresetName(origin-1))+"  (edited)" : text;
        if(id>0){if(presets.getSelectedId()!=id)presets.setSelectedId(id,juce::dontSendNotification);}
        else if(presets.getSelectedId()!=0 || presets.getText()!=shown){presets.setSelectedId(0,juce::dontSendNotification);presets.setText(shown,juce::dontSendNotification);}
        const auto next=id>0 ? juce::String::fromUTF8(spectralforge::selectablePresetDescription(id-1))
                       : edited ? juce::String("Changed from the preset. Use SAVE AS in the full editor to keep it as a file; pick the preset again to start over.")
                                : juce::String("Your own sound. Use SAVE AS in the full editor to keep it as a file.");
        if(description.getText()!=next)description.setText(next,juce::dontSendNotification);
    }
    void setAutoSetStatus(const juce::String& text,bool busy,bool canUndo) {
        autoBusy=busy;autoButton.setButtonText(busy?"CANCEL":"AUTO SET");autoButton.getProperties().set("utilityActive",busy);autoButton.repaint();
        autoButton.setTooltip(busy ? "Stop listening. Steps Auto Set already finished stay applied; UNDO then restores your previous settings."
                                   : "Play your instrument normally for about half a minute. Auto Set adjusts input level, noise gate, tone and dynamics. UNDO restores your previous settings.");
        autoOptions.setEnabled(!busy);
        undo.setEnabled(canUndo && !busy);undo.setAlpha(undo.isEnabled()?1.f:.4f);
        if(text.isNotEmpty())autoStatus.setText(text,juce::dontSendNotification);
    }
    // instrument 0 detect/1 guitar/2 bass; style 0 rock/1 clean/2 metal.
    void setAutoSetOptions(int instrument,int style,bool tone,bool finish) {
        autoInstrument=juce::jlimit(0,2,instrument);autoStyle=juce::jlimit(0,2,style);autoTone=tone;autoFinish=finish;
        constexpr std::array<const char*,3> instruments{"Auto","Guitar","Bass"},styles{"Rock","Clean","Metal"};
        autoOptions.setButtonText(juce::String(instruments[(size_t)autoInstrument])+" / "+styles[(size_t)autoStyle]);
    }
    juce::String autoSetOptionsText() const {return autoOptions.getButtonText();}
    bool autoSetBusy() const noexcept {return autoBusy;}
    // Rebinds immediately; the editor calls it after it loads a preset.
    void refreshNow() {cancelPendingUpdate();refresh();}
    void resized() override {
        full.setBounds(1000,18,160,34);
        previous.setBounds(40,108,40,36);presets.setBounds(88,108,560,36);next.setBounds(656,108,40,36);
        description.setBounds(40,148,656,30);
        autoButton.setBounds(730,106,118,40);autoOptions.setBounds(852,106,104,40);undo.setBounds(960,106,64,40);tuner.setBounds(1030,106,110,40);
        autoStatus.setBounds(730,150,410,30);
        if(room)room->setBounds(20,194,1140,262);
        marker.setBounds(20,194,1140,262);
        ampName.setBounds(200,474,560,20);targetNote.setBounds(760,474,390,20);
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
    // The amp lane the four amp knobs edit (first amp in Dual, MID in Matrix).
    int targetLane() const noexcept {return shownLane;}

private:
    static constexpr int knobCount=6;
    static inline const juce::Colour background{0xff0d1010},panel{0xff171b1b},line{0xff3a3a34},ink{0xffe1dbce},muted{0xffa4a79f},accent{0xffc4a678},warning{0xffe39a72};
    static void text(juce::Graphics& g,const juce::String& value,int x,int y,int w,int h,float size,juce::Colour colour,
                     juce::Justification justification=juce::Justification::centredLeft) {
        g.setColour(colour);g.setFont(juce::FontOptions(size));g.drawText(value,x,y,w,h,justification);
    }
    struct Knob {juce::Label name,hint;juce::Slider slider;std::unique_ptr<juce::SliderParameterAttachment> attachment;juce::String boundID;bool bound{},dragging{};};
    // Frames the rig the amp knobs edit when more than one amp is in the room.
    struct TargetMarker final : juce::Component {
        juce::Rectangle<int> target;
        TargetMarker(){setInterceptsMouseClicks(false,false);setComponentID("easy.targetMarker");}
        void paint(juce::Graphics& g) override {
            if(target.isEmpty())return;
            const auto area=target.toFloat().reduced(3.f);
            g.setColour(accent.withAlpha(.85f));g.drawRoundedRectangle(area,6.f,2.f);
            const juce::Rectangle<float> tag(area.getX()+8.f,area.getY()+6.f,124.f,20.f);
            g.setColour(background.withAlpha(.88f));g.fillRoundedRectangle(tag,3.f);
            g.setColour(accent);g.setFont(juce::FontOptions(11.f,juce::Font::bold));g.drawText("EASY KNOBS EDIT",tag,juce::Justification::centred);
        }
    };
    float raw(const juce::String& id) const {return processor.parameters().getRawParameterValue(id)->load();}
    // Parameters that change which head, channel, input or lane the amp
    // knobs edit, or whether that lane is heard.
    static bool routingParameter(const juce::String& id) {
        if(id=="mode" || id=="dualtype" || id=="dualblend")return true;
        for(const auto* prefix:{"ampext","ampchannel","ampon","mute","solo"})if(id.startsWith(prefix))return true;
        if(id.length()==4 && id.startsWith("amp"))return true;
        return id.startsWith("nativeAmp_c") && (id.endsWith("_enabled") || id.endsWith("_model") || id.endsWith("_channel") || id.endsWith("_route"));
    }
    void parameterChanged(const juce::String&,float) override {triggerAsyncUpdate();}
    void handleAsyncUpdate() override {refresh();}
    // Hardware-style 0-10 position, like the full editor's head knobs.
    static void showPosition(juce::Slider& slider) {
        slider.textFromValueFunction=[&slider](double v){return juce::String(slider.valueToProportionOfLength(v)*10.,1);};
        slider.valueFromTextFunction=[&slider](const juce::String& t){return slider.proportionOfLengthToValue(juce::jlimit(0.,1.,t.getDoubleValue()/10.));};
        slider.updateText();
    }
    void showAutoOptions() {
        juce::PopupMenu menu;
        menu.addSectionHeader("Instrument");
        menu.addItem(1,"Detect from playing",true,autoInstrument==0);menu.addItem(2,"Guitar",true,autoInstrument==1);menu.addItem(3,"Bass",true,autoInstrument==2);
        menu.addSectionHeader("Style");
        menu.addItem(11,"Rock",true,autoStyle==0);menu.addItem(12,"Clean",true,autoStyle==1);menu.addItem(13,"Metal",true,autoStyle==2);
        menu.addSectionHeader("Steps");
        menu.addItem(21,"Tone EQ",true,autoTone);menu.addItem(22,"De-esser and limiter",true,autoFinish);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&autoOptions),
                           [safe=juce::Component::SafePointer<EasyModeView>(this)](int id){if(safe!=nullptr && id>0 && safe->chooseAutoOption)safe->chooseAutoOption(id);});
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
    struct Target {int mode{},lane{},model{},channel{},route{},context{};bool native{};};
    Target currentTarget() const {
        Target t;t.mode=juce::jlimit(0,2,(int)raw("mode"));t.lane=spectralforge::easy::primaryLane(t.mode);
        t.model=processor.selectedAmpModel(t.lane);t.channel=processor.selectedAmpChannel(t.lane);t.route=processor.selectedAmpNativeRoute(t.lane);
        t.context=spectralforge::ampNativeContext(t.mode,t.lane);
        t.native=spectralforge::ampRequiresNative(t.model) || raw(spectralforge::ampNativeEnabledID(t.context))>.5f;
        return t;
    }
    // Parameter this role edits for the target, or empty with the reason.
    std::pair<juce::String,juce::String> resolve(const Target& t,int role) const {
        using spectralforge::easy::Role;
        const auto name=knobs[(size_t)role].name.getText();
        if(!t.native) {
            if(t.model<spectralforge::legacyAmpModelCount)return {spectralforge::easy::legacyControlID(t.lane,Role(role)),"the classic amp control"};
            return {{},name+" is unavailable while this amp's head panel is off in this project. Open FULL EDITOR to switch the head on."};
        }
        const int control=spectralforge::easy::ampControl(t.model,t.channel,t.route,Role(role));
        if(control<0)return {{},name+" is not on this amp's current channel. Open FULL EDITOR to see its own controls."};
        return {spectralforge::ampNativeControlID(t.context,t.model,control,t.channel),
                juce::String::fromUTF8(spectralforge::ampNativePanel(t.model).controls[(size_t)control].label)};
    }
    void refresh() {
        const auto t=currentTarget();shownLane=t.lane;
        for(int role=0;role<spectralforge::easy::roleCount;++role) {
            auto& k=knobs[(size_t)role];
            const auto [id,detail]=resolve(t,role);
            auto* parameter=id.isNotEmpty()?processor.parameters().getParameter(id):nullptr;
            const auto boundID=parameter?id:juce::String{};
            if(k.bound && boundID==k.boundID)continue;
            if(k.dragging)continue; // Rebind once the gesture has ended.
            k.attachment.reset();k.boundID=boundID;k.bound=true;
            if(parameter){k.attachment=std::make_unique<juce::SliderParameterAttachment>(*parameter,k.slider);showPosition(k.slider);}
            k.slider.setEnabled(parameter!=nullptr);
            k.slider.setTooltip(parameter ? k.name.getText()+": "+k.hint.getText()+". On this amp: "+detail+"."
                                          : detail.isNotEmpty() ? detail : k.name.getText()+" is not available here.");
            k.hint.setAlpha(parameter?1.f:.5f);
        }
        const auto& panel=spectralforge::ampNativePanel(t.model);
        auto amp="AMP:  "+juce::String::fromUTF8(spectralforge::ampInfo(t.model).name);
        if(panel.channels.size()>1 && t.channel>=0 && t.channel<(int)panel.channels.size())amp+="  /  "+juce::String::fromUTF8(panel.channels[(size_t)t.channel]);
        if(t.native && panel.routes.size()>1 && t.route>=0 && t.route<(int)panel.routes.size())amp+="  /  input "+juce::String::fromUTF8(panel.routes[(size_t)t.route]);
        if(t.mode>0)amp+=t.mode==1?"  /  amp 1 of 2":"  /  MID amp of 3";
        if(ampName.getText()!=amp)ampName.setText(amp,juce::dontSendNotification);
        const auto note=audibility(t);
        if(targetNote.getText()!=note){targetNote.setText(note,juce::dontSendNotification);targetNote.setTooltip(note);}
        const auto target=t.mode>0 && rigBounds && room && room->isVisible() ? rigBounds(t.lane) : juce::Rectangle<int>{};
        if(marker.target!=target){marker.target=target;marker.repaint();}
        const bool tuning=raw("tuneron")>.5f;
        tuner.setToggleState(tuning,juce::dontSendNotification);tuner.setButtonText(tuning?"TUNER ON":"TUNER");
        if(tuning!=(tunerPanel!=nullptr)) {
            if(tuning){tunerPanel=std::make_unique<PrecisionTunerPanel>(processor);addAndMakeVisible(*tunerPanel);}
            else tunerPanel.reset();
            ampName.setVisible(!tuning);targetNote.setVisible(!tuning);marker.setVisible(!tuning);if(room)room->setVisible(!tuning);
            for(auto& k:knobs)for(auto* c:std::initializer_list<juce::Component*>{&k.name,&k.slider,&k.hint})c->setVisible(!tuning);
            resized();repaint();
        }
    }
    // Why the amp knobs may not be heard, or empty when the lane is audible.
    juce::String audibility(const Target& t) const {
        const auto n=juce::String(t.lane+1);const auto who=t.mode==0?juce::String("The amp"):t.mode==1?juce::String("Amp 1"):juce::String("The MID amp");
        if(raw("mute"+n)>.5f)return who+" is muted: amp knobs are silent. Unmute it in FULL EDITOR.";
        const int lanes=t.mode==0?1:t.mode==1?2:3;bool otherSolo=false;
        for(int lane=0;lane<lanes;++lane)otherSolo=otherSolo || (lane!=t.lane && raw("solo"+juce::String(lane+1))>.5f);
        if(otherSolo && raw("solo"+n)<.5f)return "Another amp is soloed: amp knobs are silent. Clear SOLO in FULL EDITOR.";
        if(raw("ampon"+n)<.5f)return who+" is switched off: amp knobs have no effect.";
        if(t.mode==1 && raw("dualtype")<.5f && raw("dualblend")>.97f)return "Dual BLEND is all on amp 2: amp knobs are barely heard.";
        return {};
    }

    ChimeraProcessor& processor;
    juce::Image wordmark,brandEmblem;
    juce::TextButton full{"FULL EDITOR"},previous{"<"},next{">"},autoButton{"AUTO SET"},autoOptions,undo{"UNDO"},tuner{"TUNER"};
    juce::ComboBox presets;
    juce::Label description,autoStatus,ampName,targetNote;
    std::array<Knob,knobCount> knobs;
    std::unique_ptr<juce::SliderParameterAttachment> volumeAttachment;
    juce::RangedAudioParameter* reverbOnParameter{};juce::RangedAudioParameter* reverbMixParameter{};
    std::unique_ptr<juce::ParameterAttachment> reverbOn,reverbMix;
    bool reverbGesture{},updatingReverb{},autoBusy{},autoTone{true},autoFinish{true};
    int autoInstrument{},autoStyle{},shownLane{};
    juce::StringArray listened;
    std::unique_ptr<PrecisionTunerPanel> tunerPanel;
    std::unique_ptr<juce::Component> room;
    std::function<juce::Rectangle<int>(int)> rigBounds;
    TargetMarker marker;
};
