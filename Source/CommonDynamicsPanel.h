#pragma once
#include "CommonDynamics.h"
#include "DynamicsSpectrumPanel.h"
#include "GraphicalEQPanel.h"
#include "PostNativeCatalog.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <functional>

namespace spectralforge {

// Page and slot selection are editor-local. All sound attachments retain their
// parameter IDs and stay alive across navigation; no sound values are copied.
class CommonDynamicsPanel final : public juce::Component,private juce::Timer {
public:
    enum class Page {compressor,gate,limiter,dynamicEQ,multiband,deesser,preCompressor,postCompressor,lowCompressor};

    CommonDynamicsPanel(juce::AudioProcessorValueTreeState& s,CommonDynamics& d,GraphicalEQ& tone,GraphicalEQ& final,
                        DynamicsAnalysis& analysis,EQCaptureBoundary& boundary)
        :state(s),dsp(d),eq{&tone,&final},spectrum(analysis,boundary) {
        setComponentID("commonDynamics.panel");setOpaque(true);addChildComponent(spectrum);
        constexpr std::array<const char*,6> titles{"COMPRESSOR","GATE / EXPANDER","LIMITER","DYNAMIC EQ","MULTIBAND","DE-ESSER"};
        constexpr std::array<const char*,6> names{"compressor","gate","limiter","dynamicEQ","multiband","deesser"};
        for(size_t i=0;i<tabs.size();++i) {
            tabs[i].setButtonText(titles[i]);tabs[i].setComponentID("commonDynamics.page."+juce::String(names[i]));
            tabs[i].onClick=[this,i]{selectPage(Page(i));};addAndMakeVisible(tabs[i]);
        }
        toggle(compOn,"COMPRESSOR ON","cleanCompOn");toggle(gateOn,"GATE ON","gateon",true);
        addChildComponent(autoSet);addChildComponent(autoOptions);addChildComponent(autoUndo);
        autoSet.setComponentID("commonDynamics.autoSet");autoOptions.setComponentID("commonDynamics.autoOptions");autoUndo.setComponentID("commonDynamics.autoUndo");autoUndo.setEnabled(false);
        autoSet.setTooltip("Listen to about 30 seconds of playing and set input gain, the gate, the Tone EQ, the clean compressor, the multiband, the de-esser and the limiter. Include a short pause so the gate can hear the noise floor.");
        autoOptions.setTooltip("Auto Set options: instrument (detected, guitar or bass), style (rock, clean or metal) and which steps run.");
        autoUndo.setTooltip("Restore every setting Auto Set changed in its last run.");
        autoOptions.onClick=[this]{showAutoOptions();};updateAutoOptionsText();
        autoSet.onClick=[this]{if(autoBusy){if(onAutoCancel)onAutoCancel();}else if(onAutoSet)onAutoSet();};
        autoUndo.onClick=[this]{if(onAutoUndo)onAutoUndo();};
        toggle(limiterOn,"LIMITER ON","liveLimiterOn");toggle(gatePlacement,"AFTER RIG","gateAfterRig",true);
        choice(detector,"cleanCompDetector",{"Peak","RMS"});choice(gateMode,"gateMode",{"Gate","Expander"});
        constexpr std::array<const char*,8> compIDs{"cleanCompThreshold","cleanCompRatio","cleanCompAttack","cleanCompRelease","cleanCompKnee","cleanCompMakeup","cleanCompMix","cleanCompHP"};
        constexpr std::array<const char*,8> compNames{"Threshold dB","Ratio","Attack ms","Release ms","Knee dB","Makeup dB","Parallel mix","Detector HP Hz"};
        for(size_t i=0;i<comp.size();++i)control(comp[i],compIDs[i],compNames[i]);
        constexpr std::array<const char*,6> gateIDs{"gatethreshold","gateRatio","gateAttack","gatehold","gaterelease","gateRangeDb"};
        constexpr std::array<const char*,6> gateNames{"Threshold dB","Ratio","Attack ms","Hold ms","Release ms","Range dB"};
        for(size_t i=0;i<gate.size();++i)control(gate[i],gateIDs[i],gateNames[i]);
        gate[1].slider.setTooltip("Downward expansion ratio; active in Expander mode.");
        // Gate 2 and the Expander are separate stages with their own placement,
        // so pre + post gating and gate + expander can run together.
        gateUnitSelector.addItemList({"Gate 1","Gate 2","Expander"},1);gateUnitSelector.setComponentID("commonDynamics.gateUnit");
        gateUnitSelector.setSelectedId(1,juce::dontSendNotification);addChildComponent(gateUnitSelector);
        gateUnitSelector.setTooltip("Gate 1 is the main gate. Gate 2 and the Expander are extra stages that can run at the same time, each before or after the rig.");
        gateUnitSelector.onChange=[this]{selectGateUnit(gateUnitSelector.getSelectedId()-1);};
        toggle(gate2On,"GATE 2 ON","gate2On");toggle(gate2Placement,"AFTER RIG","gate2AfterRig");
        toggle(expanderOn,"EXPANDER ON","expOn");toggle(expanderPlacement,"AFTER RIG","expAfterRig");
        constexpr std::array<const char*,5> gate2IDs{"gate2Threshold","gate2Attack","gate2Hold","gate2Release","gate2RangeDb"};
        constexpr std::array<const char*,5> gate2Names{"Threshold dB","Attack ms","Hold ms","Release ms","Range dB"};
        for(size_t i=0;i<gate2.size();++i)control(gate2[i],gate2IDs[i],gate2Names[i]);
        constexpr std::array<const char*,6> expanderIDs{"expThreshold","expRatio","expAttack","expHold","expRelease","expRangeDb"};
        constexpr std::array<const char*,6> expanderNames{"Threshold dB","Ratio","Attack ms","Hold ms","Release ms","Range dB"};
        for(size_t i=0;i<expander.size();++i)control(expander[i],expanderIDs[i],expanderNames[i]);
        gate2Placement.setTooltip("Gate 2 detects on the clean DI. Off: attenuate before PRE. On: attenuate after the rig, after the main gate.");
        expanderPlacement.setTooltip("The Expander detects on the clean DI. Off: attenuate before PRE. On: attenuate after the rig.");
        expander[1].slider.setTooltip("Downward expansion ratio below the threshold. Range limits the deepest cut.");
        control(ceiling,"liveLimiterCeiling","Ceiling dBFS");control(limitRelease,"liveLimiterRelease","Release ms");
        gatePlacement.setTooltip("Keep the clean DI detector and place linked attenuation before or after the rig.");
        limiterOn.setTooltip("Stereo-linked sample-peak limiter after output trim. No lookahead; intersample peaks are not measured.");
        compOn.setTooltip("Clean stereo-linked compression after Tone EQ, before POST. Parallel mix blends aligned dry and compressed audio.");

        // The editor redirects native PRE/POST sources to their actual attached
        // detail editors via onCompressorSourceRequested.
        toggle(preOn,"LEGACY PRE ON","precompon",true);toggle(postOn,"LEGACY POST ON","buscompon",true);
        constexpr std::array<const char*,3> preIDs{"precomp","precompattack","precomplevel"};
        constexpr std::array<const char*,3> preNames{"Sustain","Attack ms","Level dB"};
        for(size_t i=0;i<pre.size();++i)control(pre[i],preIDs[i],preNames[i],true);
        constexpr std::array<const char*,5> postIDs{"busthreshold","busratio","busattack","busrelease","busmakeup"};
        constexpr std::array<const char*,5> postNames{"Threshold dB","Ratio","Attack ms","Release ms","Makeup dB"};
        for(size_t i=0;i<post.size();++i)control(post[i],postIDs[i],postNames[i],true);
        control(lowAmount,"lowcomp","LOW compression",true);control(lowBlend,"lowampmix","LOW DI / amp mix",true);

        slotSelector.addItemList({"Dynamic slot 1","Dynamic slot 2","Dynamic slot 3","Dynamic slot 4"},1);
        slotSelector.setComponentID("commonDynamics.dynamicSlot");slotSelector.setSelectedId(1,juce::dontSendNotification);addChildComponent(slotSelector);
        slotSelector.onChange=[this]{selectDynamicSlot(slotSelector.getSelectedId()-1);};
        for(int bank=0;bank<2;++bank) {
            eqBankButtons[size_t(bank)].setButtonText(bank==0?"TONE EQ":"FINAL EQ");
            eqBankButtons[size_t(bank)].setComponentID(bank==0?"commonDynamics.dynamicTone":"commonDynamics.dynamicFinal");
            eqBankButtons[size_t(bank)].onClick=[this,bank]{selectDynamicBank(bank);};addChildComponent(eqBankButtons[size_t(bank)]);
        }
        constexpr std::array<const char*,5> fields{"threshold","range","attack","release","ratio"};
        constexpr std::array<const char*,5> dynamicNames{"Threshold dB","Max cut dB","Attack ms","Release ms","Ratio"};
        for(int i=0;i<4;++i) {
            auto& slot=slots[size_t(i)];slot.target.addItemList(dynamicTargetNames(),1);slot.target.setComponentID(dynamicSlotID(i,"target"));
            slot.target.setName("Dynamic EQ "+juce::String(i+1)+" existing node");addChildComponent(slot.target);
            slot.target.setTooltip("Assign one existing Bell or shelf node. Tone and Final EQ share four dynamic slots.");
            if(auto* parameter=state.getParameter(dynamicSlotID(i,"target"))) {
                slot.targetAttachment=std::make_unique<juce::ParameterAttachment>(*parameter,[this,i](float value) {
                    slots[size_t(i)].target.setSelectedId(int(value)+1,juce::dontSendNotification);
                },nullptr);slot.targetAttachment->sendInitialUpdate();
            }
            slot.target.onChange=[this,i] {
                auto& current=slots[size_t(i)];const int target=current.target.getSelectedId()-1;
                for(int other=0;other<4;++other)if(other!=i && target!=0 && assignment(other)==target) {
                    current.target.setSelectedId(assignment(i)+1,juce::dontSendNotification);
                    notice="That node already has a dynamic slot. Its settings were kept.";refresh();return;
                }
                if(current.targetAttachment)current.targetAttachment->setValueAsCompleteGesture(float(target));
                notice.clear();if(i==selectedSlot && target>0)dynamicBank=(target-1)/12;refresh();syncPage();
            };
            for(size_t field=0;field<fields.size();++field)control(slot.controls[field],dynamicSlotID(i,fields[field]),dynamicNames[field]);
            slot.status.setFont(juce::FontOptions(12.f));slot.status.setComponentID("dynamicEQ"+juce::String(i+1)+".status");addChildComponent(slot.status);
        }
        toggle(mbOn,"MULTIBAND ON","mbOn");choice(mbDetector,"mbDetector",{"Peak","RMS"});control(mbMix,"mbMix","Parallel mix");
        mbOn.setTooltip("Turn the full multiband stage on or off with a smooth 10 ms transition. Off preserves the original samples.");
        mbMix.slider.setTooltip("Blend the unprocessed and processed band gains. At 0%, dynamics and makeup are removed; band solo remains active. With no solo selection, 0% returns the original samples.");
        for(int cross=0;cross<3;++cross) {
            control(crossovers[size_t(cross)],"mbCrossover"+juce::String(cross+1),"Crossover "+juce::String(cross+1)+" Hz");
            crossovers[size_t(cross)].slider.setTooltip("Target crossover. The boundaries stay ordered; the spectrum marker shows the applied frequency while changes settle.");
        }
        mbBandSelector.addItemList({"Band 1 / LOW","Band 2 / LOW MID","Band 3 / HIGH MID","Band 4 / HIGH"},1);
        mbBandSelector.setComponentID("commonDynamics.multibandBand");mbBandSelector.setSelectedId(1,juce::dontSendNotification);addChildComponent(mbBandSelector);
        mbBandSelector.onChange=[this]{selectedMBBand=std::clamp(mbBandSelector.getSelectedId()-1,0,3);syncPage();refresh();};
        constexpr std::array<const char*,7> mbFields{"threshold","ratio","attack","release","range","knee","makeup"};
        constexpr std::array<const char*,7> mbNames{"Threshold dB","Ratio","Attack ms","Release ms","Range dB","Knee dB","Makeup dB"};
        for(int band=0;band<4;++band) {
            auto& ui=mbBands[size_t(band)];const auto prefix="mbBand"+juce::String(band+1)+"_";
            toggle(ui.bypass,"BYPASS BAND",prefix+"bypass");toggle(ui.solo,"SOLO BAND",prefix+"solo");
            choice(ui.mode,prefix+"mode",{"Downward compression","Upward compression","Downward expansion","Upward expansion"});
            for(size_t field=0;field<mbFields.size();++field)control(ui.controls[field],prefix+mbFields[field],mbNames[field]);
            ui.bypass.setTooltip("Bypass this band's dynamics and makeup while keeping its crossover path and solo state. This preserves alignment with the other bands.");
            ui.controls[2].slider.setTooltip("Gain attack time constant in milliseconds. The selected Peak/RMS detector also affects the measured response time.");
            ui.controls[3].slider.setTooltip("Gain release time constant in milliseconds. Detector recovery also contributes, so full recovery takes longer than this value.");
        }
        toggle(deEsserOn,"DE-ESSER ON","deEsserOn");toggle(deEsserListen,"LISTEN","deEsserListen");
        constexpr std::array<const char*,7> deIDs{"deEsserFrequency","deEsserThreshold","deEsserRange","deEsserRatio","deEsserAttack","deEsserRelease","deEsserKnee"};
        constexpr std::array<const char*,7> deNames{"Split frequency Hz","Threshold dB","Max cut dB","Ratio","Attack ms","Release ms","Knee dB"};
        for(size_t i=0;i<deEsser.size();++i)control(deEsser[i],deIDs[i],deNames[i]);
        deEsserListen.setTooltip("Audition the detector/high band. This sound control is stored in presets.");
        spectrum.onMarkerDragStart=[this](int marker){endMarkerGesture();markerParameter=markerParameterFor(marker);if(markerParameter)markerParameter->beginChangeGesture();};
        spectrum.onMarkerDrag=[this](int,float frequency){if(markerParameter)markerParameter->setValueNotifyingHost(markerParameter->convertTo0to1(frequency));};
        spectrum.onMarkerDragEnd=[this](int){endMarkerGesture();};
        refresh();syncPage();startTimerHz(20);
    }
    ~CommonDynamicsPanel() override {
        stopTimer();endMarkerGesture();spectrum.onMarkerDragStart={};spectrum.onMarkerDrag={};spectrum.onMarkerDragEnd={};restoreEQEditors();
    }

    Page selectedPage() const noexcept {return page;}
    int selectedDynamicSlot() const noexcept {return selectedSlot;}
    int selectedDynamicBank() const noexcept {return dynamicBank;}
    // 0 = main gate, 1 = Gate 2, 2 = Expander.
    int selectedGateUnit() const noexcept {return gateUnit;}
    void selectGateUnit(int unit) {
        gateUnit=std::clamp(unit,0,2);gateUnitSelector.setSelectedId(gateUnit+1,juce::dontSendNotification);
        syncPage();refresh();
    }
    void setGateUnitMeterProvider(std::function<float(int)> provider){gateUnitMeterProvider=std::move(provider);}
    void invalidateAnalysisSource(){spectrum.sourceChanged();}
    void selectPage(Page next) {
        // Native PRE/POST own their actual model editor. Delegate before
        // changing the underlying page so closing a dialog returns to the
        // previously usable workspace, with no inactive legacy controls.
        if((next==Page::preCompressor || next==Page::postCompressor) && onCompressorSourceRequested
            && onCompressorSourceRequested(next))return;
        if(page!=next){endMarkerGesture();page=next;notice.clear();}
        syncPage();refresh();
        if(onPageSelected)onPageSelected(next);
    }
    void selectDynamicSlot(int slot) {
        selectedSlot=std::clamp(slot,0,3);slotSelector.setSelectedId(selectedSlot+1,juce::dontSendNotification);
        const int target=assignment(selectedSlot);if(target>0)dynamicBank=(target-1)/12;
        notice.clear();syncPage();refresh();
    }
    void selectDynamicBank(int bank) {
        dynamicBank=std::clamp(bank,0,1);const int target=assignment(selectedSlot);
        if(target>0 && (target-1)/12!=dynamicBank)
            for(int i=0;i<4;++i)if(assignment(i)>0 && (assignment(i)-1)/12==dynamicBank) {
                selectedSlot=i;slotSelector.setSelectedId(i+1,juce::dontSendNotification);break;
            }
        syncPage();refresh();
    }
    void setEQEditors(GraphicalEQPanel& tone,GraphicalEQPanel& final) {
        restoreEQEditors();eqEditors={&tone,&final};
        for(size_t bank=0;bank<eqEditors.size();++bank) {
            originalEQParents[bank]=eqEditors[bank]->getParentComponent();originalEQBounds[bank]=eqEditors[bank]->getBounds();
        }
        updateEQWorkspace();
    }
    bool isUsingEQEditor(const GraphicalEQPanel& editor) const noexcept {return editor.getParentComponent()==this;}
    void setMeterProvider(std::function<float(DynamicsStage)> provider){meterProvider=std::move(provider);}
    void setMultibandProviders(std::function<std::array<float,3>()> crossoverProvider,std::function<float(int)> reductionProvider) {
        effectiveCrossovers=std::move(crossoverProvider);mbReductionProvider=std::move(reductionProvider);
    }
    std::function<void(Page)> onPageSelected;
    std::function<bool(Page)> onCompressorSourceRequested;
    std::function<void()> onAutoSet,onAutoCancel,onAutoUndo;
    // Presentation of the Auto Set options; the editor maps them to the controller.
    struct AutoSetOptions {int instrument{0};bool tone{true};int style{0};bool finish{true};bool multiband{true};}; // instrument 0 detect/1 guitar/2 bass; style 0 rock/1 clean/2 metal
    std::function<void(const AutoSetOptions&)> onAutoOptionsChanged;
    void setAutoSetOptions(const AutoSetOptions& chosen){autoOptionsValue=chosen;updateAutoOptionsText();}
    const AutoSetOptions& autoSetOptions() const noexcept {return autoOptionsValue;}
    // Menu ids: 1-3 instrument, 11-13 style, 21 Tone EQ, 22 de-esser + limiter, 23 multiband step.
    void chooseAutoOption(int id) {
        auto& o=autoOptionsValue;
        if(id>=1 && id<=3)o.instrument=id-1;else if(id>=11 && id<=13)o.style=id-11;else if(id==21)o.tone=!o.tone;else if(id==22)o.finish=!o.finish;else if(id==23)o.multiband=!o.multiband;else return;
        updateAutoOptionsText();if(onAutoOptionsChanged)onAutoOptionsChanged(o);
    }
    // Presentation only: the controller owns the measurement and parameter writes.
    void setAutoSetStatus(const juce::String& text,bool busy,bool canUndo) {
        autoText=text;autoBusy=busy;autoSet.setButtonText(busy?"CANCEL":"AUTO SET");autoUndo.setEnabled(canUndo && !busy);autoOptions.setEnabled(!busy);
        repaint(520,38,std::max(1,getWidth()-740),46);
    }
    const juce::String& autoSetStatus() const noexcept {return autoText;}
    void visibilityChanged() override {if(!isShowing())endMarkerGesture();syncPage();}
    void parentHierarchyChanged() override {syncPage();}

    void resized() override {
        const int tabWidth=(getWidth()-24)/6;
        for(int i=0;i<6;++i)tabs[size_t(i)].setBounds(12+i*tabWidth,4,tabWidth-5,31);
        const int sideX=std::max(600,getWidth()-338),sideWidth=std::max(10,getWidth()-sideX-16),column=(sideWidth-12)/2;
        const int graphHeight=std::max(160,getHeight()-94);
        spectrum.setBounds(8,84,sideX-16,page==Page::multiband?graphHeight-57:graphHeight);
        compOn.setBounds(18,45,148,28);detector.setBounds(176,45,100,28);
        gateUnitSelector.setBounds(18,45,118,28);gateOn.setBounds(144,45,112,28);gateMode.setBounds(264,45,118,28);gatePlacement.setBounds(390,45,118,28);
        gate2On.setBounds(144,45,112,28);gate2Placement.setBounds(264,45,118,28);expanderOn.setBounds(144,45,112,28);expanderPlacement.setBounds(264,45,118,28);
        limiterOn.setBounds(18,45,160,28);preOn.setBounds(263,45,166,28);postOn.setBounds(263,45,166,28);
        const auto grid=[&](auto& controls,int y=103){for(size_t i=0;i<controls.size();++i)controls[i].bounds(sideX+int(i%2)*(column+12),y+int(i/2)*66,column);};
        grid(comp);grid(gate);grid(gate2);grid(expander);grid(pre);grid(post);grid(deEsser);
        ceiling.bounds(sideX,110,sideWidth);limitRelease.bounds(sideX,181,sideWidth);
        lowAmount.bounds(sideX,110,sideWidth);lowBlend.bounds(sideX,181,sideWidth);
        slotSelector.setBounds(18,44,176,28);eqBankButtons[0].setBounds(430,44,95,28);eqBankButtons[1].setBounds(533,44,95,28);
        const int controlsY=getHeight()-55,slotControlWidth=(getWidth()-36)/5;
        for(auto& slot:slots) {
            slot.target.setBounds(204,44,215,28);slot.status.setBounds(643,44,std::max(1,getWidth()-660),28);
            for(int field=0;field<5;++field)slot.controls[size_t(field)].bounds(18+field*slotControlWidth,controlsY,slotControlWidth-12);
        }
        for(auto& reference:eqEditors)if(auto* editor=reference.getComponent();editor && editor->getParentComponent()==this)
            editor->setBounds(0,82,getWidth(),std::max(160,getHeight()-144));
        mbOn.setBounds(18,45,153,28);mbBandSelector.setBounds(182,45,224,28);mbDetector.setBounds(417,45,100,28);
        for(auto& band:mbBands) {
            band.mode.setBounds(sideX,89,sideWidth,28);band.bypass.setBounds(sideX,124,column,25);band.solo.setBounds(sideX+column+12,124,column,25);
            for(size_t field=0;field<band.controls.size();++field)band.controls[field].bounds(sideX+int(field%2)*(column+12),158+int(field/2)*61,column);
        }
        mbMix.bounds(sideX+column+12,341,column);
        const int crossWidth=(sideX-32)/3;
        for(int cross=0;cross<3;++cross)crossovers[size_t(cross)].bounds(18+cross*crossWidth,getHeight()-56,crossWidth-12);
        deEsserOn.setBounds(18,45,150,28);deEsserListen.setBounds(178,45,104,28);
        autoSet.setBounds(sideX,getHeight()-40,100,28);autoOptions.setBounds(sideX+106,getHeight()-40,118,28);
        autoUndo.setBounds(sideX+230,getHeight()-40,std::max(1,sideWidth-230),28);repaint();
    }
    void paint(juce::Graphics& g) override {
        const juce::Colour muted(0xff9fb4ad),text(0xffd9ebe3),line(0xff273b3d);
        g.fillAll(juce::Colour(0xff121a1d));g.setColour(line);g.drawHorizontalLine(38,12.f,float(getWidth()-12));
        if(page!=Page::dynamicEQ) {
            const int sideX=std::max(600,getWidth()-338);g.drawVerticalLine(sideX-10,85.f,float(getHeight()-12));
            g.setFont(juce::FontOptions(12.f));g.setColour(muted);
            if(autoText.isNotEmpty() && autoPage())g.drawFittedText(autoText,532,41,std::max(1,getWidth()-770),38,juce::Justification::centredLeft,2);
            else g.drawText(contextText(),532,47,std::max(1,getWidth()-770),25,juce::Justification::centredLeft);
            g.setColour(text);g.setFont(juce::FontOptions(14.f,juce::Font::bold));
            const auto meter=currentReduction<0?"GAIN "+juce::String(-currentReduction,1)+" dB":"GR "+juce::String(currentReduction,1)+" dB";
            g.drawText(meter,getWidth()-222,46,204,27,juce::Justification::centredRight);
            if(page==Page::limiter)drawNote(g,sideX,269,"SAMPLE-PEAK LIMITING\nStereo linked / zero lookahead\nTrue-peak protection is not measured.");
            if(page==Page::gate)drawNote(g,sideX,319,gateUnit==0?"Clean DI detector\nRange controls maximum attenuation.":"Clean DI detector\nRuns together with Gate 1. The spectrum shows Gate 1.");
            if(page==Page::lowCompressor)drawNote(g,sideX,271,raw("mode")>1.5f?"MATRIX LOW DI\nThe compressor acts on the clean LOW branch.":"MATRIX ROUTE INACTIVE\nSelect Matrix routing to hear this compressor.");
            if(page==Page::preCompressor)drawNote(g,sideX,267,sourceIsActive()?"LEGACY PRE RACK\nNative pedalboard compressors open their own spectrum editor.":"LEGACY PRE INACTIVE\nStored controls. Select PRE again to open the current native compressor.");
            if(page==Page::postCompressor)drawNote(g,sideX,329,sourceIsActive()?"LEGACY POST RACK\nNative POST compressors use their attached controls.":"LEGACY POST INACTIVE\nStored controls. Select POST again to open the current native compressor.");
        } else if(!activeEQEditor()) {
            g.setColour(muted);g.setFont(juce::FontOptions(14.f));
            g.drawText("Select the assigned Tone / Final EQ node to edit its spectrum and dynamics together.",24,146,getWidth()-48,76,juce::Justification::centred);
        }
    }

private:
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    struct Control {
        juce::Label label;juce::Slider slider;std::unique_ptr<SA> attachment;
        void bounds(int x,int y,int width){label.setBounds(x,y,width,18);slider.setBounds(x,y+20,width,29);}
        void show(bool visible){label.setVisible(visible);slider.setVisible(visible);}
    };
    struct Slot {juce::ComboBox target;std::unique_ptr<juce::ParameterAttachment> targetAttachment;std::array<Control,5> controls;juce::Label status;};
    struct MBBand {juce::TextButton bypass,solo;juce::ComboBox mode;std::array<Control,7> controls;};
    static bool isCompressorPage(Page selected){return selected==Page::compressor || selected==Page::preCompressor || selected==Page::postCompressor || selected==Page::lowCompressor;}
    static DynamicsStage stageFor(Page selected) {
        switch(selected) {
            case Page::compressor:return DynamicsStage::compressor;case Page::gate:return DynamicsStage::gate;
            case Page::limiter:return DynamicsStage::limiter;case Page::multiband:return DynamicsStage::multiband;
            case Page::deesser:return DynamicsStage::deesser;case Page::preCompressor:return DynamicsStage::preCompressor;
            case Page::postCompressor:return DynamicsStage::postCompressor;case Page::lowCompressor:return DynamicsStage::lowCompressor;
            default:return DynamicsStage::none;
        }
    }
    float raw(const juce::String& id,float fallback=0) const {
        if(const auto* parameter=state.getRawParameterValue(id)){const float value=parameter->load();return std::isfinite(value)?value:fallback;}return fallback;
    }
    bool sourceIsActive() const {
        if(page==Page::preCompressor)return raw("boardEnabled")<.5f;
        if(page==Page::postCompressor)return raw(postNativeModeID(0))<.5f;
        if(page==Page::lowCompressor)return raw("mode")>1.5f;
        return true;
    }
    void control(Control& c,const juce::String& id,const juce::String& title,bool duplicate=false) {
        c.label.setText(title,juce::dontSendNotification);c.label.setFont(juce::FontOptions(12.f));addChildComponent(c.label);
        const bool shared=duplicate || id=="gatethreshold" || id=="gatehold" || id=="gaterelease" || id=="gateRangeDb";
        const bool rangeWithFull=id=="gateRangeDb" || id=="gate2RangeDb" || id=="expRangeDb";
        c.slider.setName(title);c.slider.setComponentID(shared?"commonDynamics."+id:id);c.slider.setSliderStyle(juce::Slider::LinearHorizontal);
        c.slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,65,27);c.slider.setWantsKeyboardFocus(true);c.slider.setNumDecimalPlacesToDisplay(2);addChildComponent(c.slider);
        if(state.getParameter(id))c.attachment=std::make_unique<SA>(state,id,c.slider);else c.slider.setEnabled(false);
        if(id=="cleanCompMix" || id=="mbMix" || id=="lowampmix" || id=="lowcomp" || id=="precomp") {
            c.slider.textFromValueFunction=[](double value){return juce::String(value*100,1)+" %";};
            c.slider.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()/100.;};
        } else if(rangeWithFull) {
            c.slider.textFromValueFunction=[](double value){return value>=95.95?juce::String("Full"):juce::String(value,1);};
            c.slider.valueFromTextFunction=[](const juce::String& text){return text.trim().equalsIgnoreCase("Full")?96.:text.getDoubleValue();};
        }
        c.slider.updateText();
    }
    void toggle(juce::TextButton& button,const juce::String& title,const juce::String& id,bool duplicate=false) {
        button.setButtonText(title);button.setClickingTogglesState(true);button.setComponentID(duplicate?"commonDynamics."+id:id);addChildComponent(button);
        if(state.getParameter(id))buttons.push_back(std::make_unique<BA>(state,id,button));else button.setEnabled(false);
    }
    void choice(juce::ComboBox& box,const juce::String& id,const juce::StringArray& options) {
        box.addItemList(options,1);box.setComponentID(id);addChildComponent(box);
        if(state.getParameter(id))choices.push_back(std::make_unique<CA>(state,id,box));else box.setEnabled(false);
    }
    int assignment(int slot) const {return std::clamp(int(raw(dynamicSlotID(slot,"target"))),0,24);}
    template<typename Controls> void showControls(Controls& controls,bool showing){for(auto& c:controls)c.show(showing);}
    void syncPage() {
        for(int i=0;i<6;++i)tabs[size_t(i)].setToggleState(i==(isCompressorPage(page)?0:int(page)),juce::dontSendNotification);
        compOn.setVisible(page==Page::compressor);detector.setVisible(page==Page::compressor);showControls(comp,page==Page::compressor);
        const bool gatePage=page==Page::gate;gateUnitSelector.setVisible(gatePage);
        gateOn.setVisible(gatePage && gateUnit==0);gateMode.setVisible(gatePage && gateUnit==0);gatePlacement.setVisible(gatePage && gateUnit==0);showControls(gate,gatePage && gateUnit==0);
        gate2On.setVisible(gatePage && gateUnit==1);gate2Placement.setVisible(gatePage && gateUnit==1);showControls(gate2,gatePage && gateUnit==1);
        expanderOn.setVisible(gatePage && gateUnit==2);expanderPlacement.setVisible(gatePage && gateUnit==2);showControls(expander,gatePage && gateUnit==2);
        limiterOn.setVisible(page==Page::limiter);ceiling.show(page==Page::limiter);limitRelease.show(page==Page::limiter);
        preOn.setVisible(page==Page::preCompressor);postOn.setVisible(page==Page::postCompressor);showControls(pre,page==Page::preCompressor);showControls(post,page==Page::postCompressor);
        lowAmount.show(page==Page::lowCompressor);lowBlend.show(page==Page::lowCompressor);
        const bool dynamic=page==Page::dynamicEQ;slotSelector.setVisible(dynamic);
        for(int bank=0;bank<2;++bank){eqBankButtons[size_t(bank)].setVisible(dynamic);eqBankButtons[size_t(bank)].setToggleState(bank==dynamicBank,juce::dontSendNotification);}
        for(int i=0;i<4;++i){auto& slot=slots[size_t(i)];const bool visible=dynamic && i==selectedSlot;slot.target.setVisible(visible);slot.status.setVisible(visible);showControls(slot.controls,visible);}
        const bool mb=page==Page::multiband;mbOn.setVisible(mb);mbDetector.setVisible(mb);mbBandSelector.setVisible(mb);mbMix.show(mb);showControls(crossovers,mb);
        for(int band=0;band<4;++band){auto& ui=mbBands[size_t(band)];const bool showing=mb && band==selectedMBBand;ui.bypass.setVisible(showing);ui.solo.setVisible(showing);ui.mode.setVisible(showing);showControls(ui.controls,showing);}
        deEsserOn.setVisible(page==Page::deesser);deEsserListen.setVisible(page==Page::deesser);
        autoSet.setVisible(autoPage());autoOptions.setVisible(autoPage());autoUndo.setVisible(autoPage());showControls(deEsser,page==Page::deesser);
        if(page==Page::preCompressor)spectrum.setPreOwner(-1);
        if(page==Page::postCompressor)spectrum.setPostNativeSource(0);
        spectrum.setVisible(!dynamic);spectrum.setStage(stageFor(page));updateEQWorkspace();resized();repaint();
    }
    GraphicalEQPanel* activeEQEditor() const {
        auto* editor=eqEditors[size_t(dynamicBank)].getComponent();return editor && editor->getParentComponent()==this?editor:nullptr;
    }
    void updateEQWorkspace() {
        if(page!=Page::dynamicEQ || !isShowing()){restoreEQEditors();return;}
        for(int bank=0;bank<2;++bank) {
            auto* editor=eqEditors[size_t(bank)].getComponent();if(!editor)continue;
            if(bank!=dynamicBank){if(editor->getParentComponent()==this)restoreEQEditor(bank);continue;}
            if(editor->getParentComponent()!=this) {
                originalEQParents[size_t(bank)]=editor->getParentComponent();originalEQBounds[size_t(bank)]=editor->getBounds();
                editor->setVisible(false);addChildComponent(*editor);editor->setCompactWorkspace(true);
            }
            editor->setBounds(0,82,getWidth(),std::max(160,getHeight()-144));
            const int target=assignment(selectedSlot);
            if(target>0 && (target-1)/12==bank && editor->selectedBand()!=(target-1)%12)editor->selectBandForEditing((target-1)%12);
            editor->setVisible(true);
        }
    }
    void restoreEQEditor(int bank) {
        auto* editor=eqEditors[size_t(bank)].getComponent();if(!editor || editor->getParentComponent()!=this)return;
        editor->setVisible(false);removeChildComponent(editor);editor->setCompactWorkspace(false);
        if(auto* parent=originalEQParents[size_t(bank)].getComponent())parent->addChildComponent(*editor);
        editor->setBounds(originalEQBounds[size_t(bank)]);
    }
    void restoreEQEditors(){for(int bank=0;bank<2;++bank)restoreEQEditor(bank);}
    void refresh() {
        const int sourceRoute=page==Page::gate?int(raw("gateAfterRig")>.5f):int(sourceIsActive());
        const int sourceKey=int(page)*2+sourceRoute;
        if(sourceKey!=observedSourceKey) {
            // Routing changes can deactivate a stored legacy/LOW editor, or
            // move the gate tap. A frozen frame belongs to its original route.
            if(observedSourceKey>=0 && observedSourceKey/2==int(page))spectrum.sourceChanged();
            observedSourceKey=sourceKey;repaint();
        }
        gate[1].slider.setEnabled(raw("gateMode")>.5f);used=0;
        for(int i=0;i<4;++i)used+=assignment(i)!=0;
        for(int i=0;i<4;++i) {
            auto& slot=slots[size_t(i)];const int target=assignment(i);bool duplicate=false;
            for(int earlier=0;earlier<i;++earlier)duplicate|=target!=0 && assignment(earlier)==target;
            juce::String status="Unassigned / choose an existing EQ node";bool enabled=false;
            if(target>0) {
                const int bank=(target-1)/12,band=(target-1)%12;const auto value=eq[size_t(bank)]->values(band);
                const bool nodeExists=eqEditors[size_t(bank)] && eqEditors[size_t(bank)]->isBandVisible(band);
                enabled=nodeExists && !duplicate && value.type<=2 && bank==dynamicBank;
                status=duplicate?"Duplicate assignment / idle":!nodeExists?"Add a Bell / shelf node in this EQ slot":value.type>2?"Bell / shelf nodes only":bank!=dynamicBank?"Select "+juce::String(bank==0?"Tone":"Final")+" EQ for this slot":!value.enabled?"Band off / enable the selected node":eq[size_t(bank)]->isBypassed()?"EQ bypassed":juce::String(eq[size_t(bank)]->dynamicReduction(band),1)+" dB GR  /  "+juce::String(used)+" of 4 slots";
            }
            slot.status.setText(i==selectedSlot && notice.isNotEmpty()?notice:status,juce::dontSendNotification);
            for(auto& c:slot.controls)c.slider.setEnabled(enabled);
            for(int targetNode=1;targetNode<=24;++targetNode) {
                bool occupied=false;for(int other=0;other<4;++other)occupied|=other!=i && assignment(other)==targetNode;
                const int bank=(targetNode-1)/12,band=(targetNode-1)%12;
                const bool usable=eqEditors[size_t(bank)] && eqEditors[size_t(bank)]->isBandVisible(band) && eq[size_t(bank)]->values(band).type<=2;
                slot.target.setItemEnabled(targetNode+1,((!occupied && usable) || targetNode==target));
            }
        }
        updateEQWorkspace();const auto stage=stageFor(page);float reduction=0;
        if(page==Page::multiband && mbReductionProvider)reduction=mbReductionProvider(selectedMBBand);
        else if(page==Page::gate && gateUnit>0)reduction=gateUnitMeterProvider?gateUnitMeterProvider(gateUnit):0.f;
        else if(meterProvider)reduction=meterProvider(stage);
        else if(page==Page::compressor)reduction=dsp.compReduction.load();
        else if(page==Page::limiter)reduction=dsp.limitReduction.load();
        reduction=sourceIsActive() && std::isfinite(reduction)?reduction:0;
        if(std::abs(reduction-currentReduction)>.049f){currentReduction=reduction;repaint(getWidth()-230,42,218,33);}
        std::array<DynamicsSpectrumPanel::FrequencyMarker,3> markers{};int count=0;
        if(page==Page::compressor){markers[0]={raw("cleanCompHP",20),"Detector HP"};count=1;spectrum.setBandHighlight(0,0);}
        else if(page==Page::deesser){const float frequency=raw("deEsserFrequency",6000);markers[0]={frequency,"Split"};count=1;spectrum.setBandHighlight(frequency,20000);}
        else if(page==Page::multiband) {
            const auto cutoffs=effectiveCrossovers?effectiveCrossovers():std::array<float,3>{raw("mbCrossover1",120),raw("mbCrossover2",700),raw("mbCrossover3",3500)};
            for(int cross=0;cross<3;++cross)markers[size_t(cross)]={cutoffs[size_t(cross)],"X"+juce::String(cross+1)};count=3;
            spectrum.setBandHighlight(selectedMBBand==0?20.f:cutoffs[size_t(selectedMBBand-1)],selectedMBBand==3?20000.f:cutoffs[size_t(selectedMBBand)]);
        } else spectrum.setBandHighlight(0,0);
        spectrum.setFrequencyMarkers(markers,count);
    }
    juce::RangedAudioParameter* markerParameterFor(int marker) {
        if(page==Page::compressor && marker==0)return state.getParameter("cleanCompHP");
        if(page==Page::deesser && marker==0)return state.getParameter("deEsserFrequency");
        if(page==Page::multiband && marker>=0 && marker<3)return state.getParameter("mbCrossover"+juce::String(marker+1));
        return nullptr;
    }
    void endMarkerGesture(){if(markerParameter){markerParameter->endChangeGesture();markerParameter=nullptr;}}
    juce::String contextText() const {
        switch(page) {
            case Page::compressor:return "After Tone EQ / before POST";
            case Page::gate:{
                const char* placement=gateUnit==1?"gate2AfterRig":gateUnit==2?"expAfterRig":"gateAfterRig";
                const juce::String unit=gateUnit==1?"Gate 2 / ":gateUnit==2?"Expander / ":"";
                return unit+(gateUnit==0?"Clean":"clean")+(raw(placement)>.5f?" DI detection / after rig":" DI detection / before rig");
            }
            case Page::limiter:return "Final output / sample peak";
            case Page::multiband:return "4 bands / stereo linked";
            case Page::deesser:return "High-band dynamics";
            case Page::preCompressor:return sourceIsActive()?"Legacy PRE signal path":"Legacy PRE source inactive";
            case Page::postCompressor:return sourceIsActive()?"Legacy POST signal path":"Legacy POST source inactive";
            case Page::lowCompressor:return sourceIsActive()?"Matrix LOW DI branch":"Matrix LOW source inactive";
            default:return {};
        }
    }
    void drawNote(juce::Graphics& g,int x,int y,const juce::String& text) const {
        g.setColour(juce::Colour(0xff9fb4ad));g.setFont(juce::FontOptions(12.f));
        g.drawFittedText(text,x,y,getWidth()-x-16,std::max(1,getHeight()-y-12),juce::Justification::topLeft,4);
    }
    void timerCallback() override {if(isShowing())refresh();}
    juce::AudioProcessorValueTreeState& state;CommonDynamics& dsp;std::array<GraphicalEQ*,2> eq;DynamicsSpectrumPanel spectrum;
    Page page{Page::compressor};std::array<juce::TextButton,6> tabs;
    juce::ComboBox detector,gateMode,slotSelector,mbDetector,mbBandSelector,gateUnitSelector;
    juce::TextButton gate2On,gate2Placement,expanderOn,expanderPlacement;
    juce::TextButton compOn,gateOn,gatePlacement,limiterOn,preOn,postOn,mbOn,deEsserOn,deEsserListen;
    juce::TextButton autoSet{"AUTO SET"},autoUndo{"UNDO AUTO"};juce::TextButton autoOptions;AutoSetOptions autoOptionsValue;juce::String autoText;bool autoBusy{};
    void updateAutoOptionsText() {
        constexpr std::array<const char*,3> instruments{"Auto","Guitar","Bass"},styles{"Rock","Clean","Metal"};
        autoOptions.setButtonText(juce::String(instruments[size_t(autoOptionsValue.instrument)])+" / "+styles[size_t(autoOptionsValue.style)]);
    }
    void showAutoOptions() {
        const auto& o=autoOptionsValue;juce::PopupMenu menu;
        menu.addSectionHeader("Instrument");
        menu.addItem(1,"Detect from playing",true,o.instrument==0);menu.addItem(2,"Guitar",true,o.instrument==1);menu.addItem(3,"Bass",true,o.instrument==2);
        menu.addSectionHeader("Style");
        menu.addItem(11,"Rock",true,o.style==0);menu.addItem(12,"Clean",true,o.style==1);menu.addItem(13,"Metal",true,o.style==2);
        menu.addSectionHeader("Steps");
        menu.addItem(21,"Tone EQ",true,o.tone);menu.addItem(23,"Multiband",true,o.multiband);menu.addItem(22,"De-esser and limiter",true,o.finish);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&autoOptions),[safe=juce::Component::SafePointer<CommonDynamicsPanel>(this)](int id){if(safe!=nullptr)safe->chooseAutoOption(id);});
    }
    bool autoPage() const noexcept {return page==Page::compressor || page==Page::gate;}
    std::array<Control,8> comp;std::array<Control,6> gate;std::array<Control,5> gate2;std::array<Control,6> expander;std::array<Control,3> pre;std::array<Control,5> post;
    Control ceiling,limitRelease,lowAmount,lowBlend,mbMix;std::array<Control,3> crossovers;std::array<Control,7> deEsser;
    std::array<Slot,4> slots;std::array<MBBand,4> mbBands;std::array<juce::TextButton,2> eqBankButtons;
    std::array<juce::Component::SafePointer<GraphicalEQPanel>,2> eqEditors;
    std::array<juce::Component::SafePointer<juce::Component>,2> originalEQParents;std::array<juce::Rectangle<int>,2> originalEQBounds;
    std::vector<std::unique_ptr<BA>> buttons;std::vector<std::unique_ptr<CA>> choices;
    std::function<float(DynamicsStage)> meterProvider;std::function<std::array<float,3>()> effectiveCrossovers;std::function<float(int)> mbReductionProvider,gateUnitMeterProvider;
    juce::RangedAudioParameter* markerParameter{};int selectedSlot{},selectedMBBand{},gateUnit{},dynamicBank{},used{},observedSourceKey{-1};float currentReduction{};juce::String notice;
};
}
