#pragma once
#include "CommonDynamics.h"
#include "ThreeBandSaturation.h"
#include "FilterModulation.h"
#include "DynamicsSpectrumPanel.h"
#include "GraphicalEQPanel.h"
#include "PostNativeCatalog.h"
#include "AutoSetView.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <functional>

namespace spectralforge {

// Page and slot selection are editor-local. All sound attachments retain their
// parameter IDs and stay alive across navigation; no sound values are copied.
class CommonDynamicsPanel final : public juce::Component,private juce::Timer {
public:
    enum class Page {compressor,gate,limiter,dynamicEQ,multiband,deesser,preCompressor,postCompressor,lowCompressor,expander,saturation,filter};

    // Dynamic EQ is edited on its Tone / Final EQ band; Page::dynamicEQ is kept
    // only so older navigation can be redirected there.
    CommonDynamicsPanel(juce::AudioProcessorValueTreeState& s,CommonDynamics& d,DynamicsAnalysis& analysis,EQCaptureBoundary& boundary)
        :state(s),dsp(d),spectrum(analysis,boundary) {
        setComponentID("commonDynamics.panel");setOpaque(true);addChildComponent(spectrum);
        constexpr std::array<const char*,8> titles{"COMPRESSOR","GATE","EXPANDER","LIMITER","MULTIBAND","SATURATION","FILTER","DE-ESSER"};
        constexpr std::array<const char*,8> names{"compressor","gate","expander","limiter","multiband","saturation","filter","deesser"};
        for(size_t i=0;i<tabs.size();++i) {
            tabs[i].setButtonText(titles[i]);tabs[i].setComponentID("commonDynamics.page."+juce::String(names[i]));
            tabs[i].onClick=[this,i]{selectPage(tabPages[i]);};addAndMakeVisible(tabs[i]);
        }
        toggle(compOn,"COMPRESSOR ON","cleanCompOn");toggle(gateOn,"GATE ON","gateon",true);
        addChildComponent(autoSet);addChildComponent(autoOptions);addChildComponent(autoUndo);
        autoSet.setComponentID("commonDynamics.autoSet");autoOptions.setComponentID("commonDynamics.autoOptions");autoUndo.setComponentID("commonDynamics.autoUndo");autoUndo.setEnabled(false);
        autoSet.setTooltip("Listen to about 52 seconds of playing and set input gain, the gates and the Expander, the amp's gain and tone knobs, where the cabinet mic sits, the Tone EQ and its Dynamic EQ, the clean compressor, the multiband, the de-esser and the limiter, then the Final EQ and the output level. Include a short pause so the gate can hear the noise floor.");
        autoOptions.setTooltip("Auto Set options: instrument (detected, guitar or bass), style (rock, clean or metal), which steps run, strength and locked settings.");
        autoUndo.setTooltip("Restore every setting Auto Set changed in its last run.");
        autoOptions.onClick=[this]{showAutoOptions();};updateAutoOptionsText();
        autoSet.onClick=[this]{if(autoBusy){if(onAutoCancel)onAutoCancel();}else if(onAutoSet)onAutoSet();};
        autoUndo.onClick=[this]{if(onAutoUndo)onAutoUndo();};
        toggle(limiterOn,"LIMITER ON","liveLimiterOn");toggle(gatePlacement,"SWAP PRE / POST","gateAfterRig",true);
        choice(detector,"cleanCompDetector",{"Peak","RMS"});choice(gateMode,"gateMode",{"Gate","Expander"});
        constexpr std::array<const char*,8> compIDs{"cleanCompThreshold","cleanCompRatio","cleanCompAttack","cleanCompRelease","cleanCompKnee","cleanCompMakeup","cleanCompMix","cleanCompHP"};
        constexpr std::array<const char*,8> compNames{"Threshold dB","Ratio","Attack ms","Release ms","Knee dB","Makeup dB","Parallel mix","Detector HP Hz"};
        for(size_t i=0;i<comp.size();++i)control(comp[i],compIDs[i],compNames[i]);
        constexpr std::array<const char*,6> gateIDs{"gatethreshold","gateRatio","gateAttack","gatehold","gaterelease","gateRangeDb"};
        constexpr std::array<const char*,6> gateNames{"Threshold dB","Ratio","Attack ms","Hold ms","Release ms","Range dB"};
        for(size_t i=0;i<gate.size();++i)control(gate[i],gateIDs[i],gateNames[i]);
        gate[1].slider.setTooltip("Downward expansion ratio; active in Expander mode.");
        // The Gate page edits a PRE and a POST gate. The released gate settings
        // sit where gateAfterRig puts them; Gate 2 takes the other side.
        for(int side=0;side<2;++side) {
            auto& button=gateSides[size_t(side)];button.setButtonText(side==0?"PRE RIG":"POST RIG");button.setClickingTogglesState(false);
            button.setComponentID(side==0?"commonDynamics.gatePre":"commonDynamics.gatePost");addChildComponent(button);
            button.setTooltip(side==0?"Gate before PRE pedals and the amp.":"Gate after AMP/CAB, before Tone EQ. It still listens to the clean input.");
            button.onClick=[this,side]{selectGateSide(side);};
        }
        toggle(gate2On,"GATE ON","gate2On");choice(gate2Mode,"gate2Mode",{"Gate","Expander"});
        toggle(expanderOn,"EXPANDER ON","expOn");toggle(expanderPlacement,"AFTER RIG","expAfterRig");
        constexpr std::array<const char*,6> gate2IDs{"gate2Threshold","gate2Ratio","gate2Attack","gate2Hold","gate2Release","gate2RangeDb"};
        constexpr std::array<const char*,6> gateNames2{"Threshold dB","Ratio","Attack ms","Hold ms","Release ms","Range dB"};
        for(size_t i=0;i<gate2.size();++i)control(gate2[i],gate2IDs[i],gateNames2[i]);
        gate2[1].slider.setTooltip("Downward expansion ratio; active in Expander mode.");
        constexpr std::array<const char*,6> expanderIDs{"expThreshold","expRatio","expAttack","expHold","expRelease","expRangeDb"};
        for(size_t i=0;i<expander.size();++i)control(expander[i],expanderIDs[i],gateNames2[i]);
        expanderPlacement.setTooltip("The Expander listens to the clean input. Off: reduce noise before PRE. On: reduce it after the rig.");
        expander[1].slider.setTooltip("Downward expansion ratio below the threshold. Range limits the deepest cut.");
        control(ceiling,"liveLimiterCeiling","Ceiling dBFS");control(limitRelease,"liveLimiterRelease","Release ms");
        gatePlacement.setTooltip("Swap the PRE and POST gate settings between the two sides of the rig.");
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

        // External sidechain keys (docs/SIDECHAIN_SYNTH_BUS.md).
        toggle(compKey,"EXT KEY","scKeyComp");toggle(gateKey,"EXT KEY","scKeyGates");toggle(mbKey,"EXT KEY","scKeyMultiband");
        compKey.setTooltip("Detect from the plugin's Sidechain input instead of the signal, when the host connects one.");
        gateKey.setTooltip("The Gate, Gate 2 and the Expander detect from the plugin's Sidechain input instead of the clean input, when the host connects one.");
        mbKey.setTooltip("Every band detects from the same band of the plugin's Sidechain input, when the host connects one.");
        toggle(mbOn,"MULTIBAND ON","mbOn");choice(mbDetector,"mbDetector",{"Peak","RMS"});control(mbMix,"mbMix","Parallel mix");
        mbOn.setTooltip("Turn the full multiband stage on or off with a smooth 10 ms transition. Off preserves the original samples.");
        mbMix.slider.setTooltip("Blend the unprocessed and processed band gains. At 0%, dynamics and makeup are removed; band solo remains active. With no solo selection, 0% returns the original samples.");
        for(int cross=0;cross<multibandMaxCrossovers;++cross) {
            control(crossovers[size_t(cross)],"mbCrossover"+juce::String(cross+1),"Crossover "+juce::String(cross+1)+" Hz");
            crossovers[size_t(cross)].slider.setTooltip("Target crossover. The boundaries stay ordered; the spectrum marker shows the applied frequency while changes settle.");
        }
        // Stereo domain and detector link (docs/MULTIBAND_STEREO_1_5.md).
        choice(mbStereo,"mbStereoMode",{"L/R","M/S"});
        mbStereo.setTooltip("Process left/right, or mid/side (M = (L + R) / 2, S = (L - R) / 2). Changing it fades the stage out and back in (about 50 ms).");
        mbLink.setSliderStyle(juce::Slider::LinearBar);mbLink.setComponentID("mbLink");mbLink.setTextValueSuffix(" % link");addChildComponent(mbLink);
        if(state.getParameter("mbLink"))mbLinkAttachment=std::make_unique<SA>(state,"mbLink",mbLink);else mbLink.setEnabled(false);
        mbLink.setNumDecimalPlacesToDisplay(0);
        mbLink.setTooltip("Stereo link: at 100% both channels follow the louder detector (the original linked behaviour); at 0% each channel (or M and S) follows its own.");
        mbBandSelector.addItemList({"Band 1 / LOW","Band 2 / LOW MID","Band 3 / HIGH MID","Band 4 / HIGH","Band 5","Band 6"},1);
        mbBandSelector.setComponentID("commonDynamics.multibandBand");mbBandSelector.setSelectedId(1,juce::dontSendNotification);addChildComponent(mbBandSelector);
        mbBandSelector.onChange=[this]{selectedMBBand=std::clamp(mbBandSelector.getSelectedId()-1,0,shownMBBands-1);syncPage();refresh();};
        // Six-band expansion (docs/SIX_BAND_1_5.md).
        choice(mbBandCount,"mbBandCount",{"4 BANDS","6 BANDS"});
        mbBandCount.setTooltip("Four or six bands. Six adds crossovers 4 and 5 above crossover 3, and bands 5 and 6. Changing it fades the stage out and back in (about 50 ms).");
        applyMBBandCount(4);
        constexpr std::array<const char*,7> mbFields{"threshold","ratio","attack","release","range","knee","makeup"};
        constexpr std::array<const char*,7> mbNames{"Threshold dB","Ratio","Attack ms","Release ms","Range dB","Knee dB","Makeup dB"};
        for(int band=0;band<multibandMaxBandCount;++band) {
            auto& ui=mbBands[size_t(band)];const auto prefix="mbBand"+juce::String(band+1)+"_";
            toggle(ui.bypass,"BYPASS BAND",prefix+"bypass");toggle(ui.solo,"SOLO BAND",prefix+"solo");
            choice(ui.mode,prefix+"mode",{"Downward compression","Upward compression","Downward expansion","Upward expansion"});
            choice(ui.channels,prefix+"channels",{"Both channels","Mid only","Side only"});
            ui.channels.setTooltip("Process this band on both channels, or only on mid/left or side/right. The other channel passes unchanged.");
            for(size_t field=0;field<mbFields.size();++field)control(ui.controls[field],prefix+mbFields[field],mbNames[field]);
            ui.bypass.setTooltip("Bypass this band's dynamics and makeup while keeping its crossover path and solo state. This preserves alignment with the other bands.");
            ui.controls[2].slider.setTooltip("Gain attack time constant in milliseconds. The selected Peak/RMS detector also affects the measured response time.");
            ui.controls[3].slider.setTooltip("Gain release time constant in milliseconds. Detector recovery also contributes, so full recovery takes longer than this value.");
        }
        // Three-band saturation (docs/SATURATION_1_6.md).
        toggle(satOn,"SATURATION ON","satOn");choice(satQuality,"satQuality",{"Live","Studio 4x"});
        satOn.setTooltip("Switching moves through a phase-matched all-pass (about 35 ms), so no frequency cancels. Off leaves the signal untouched.");
        satQuality.setTooltip("Live: no added latency. Studio 4x: oversampled (less aliasing), adds its latency while selected. Changing it fades the stage out and in.");
        satBandSelector.addItemList({"Band 1 / LOW","Band 2 / MID","Band 3 / HIGH"},1);
        satBandSelector.setComponentID("commonDynamics.saturationBand");satBandSelector.setSelectedId(1,juce::dontSendNotification);addChildComponent(satBandSelector);
        satBandSelector.onChange=[this]{selectedSatBand=std::clamp(satBandSelector.getSelectedId()-1,0,2);syncPage();refresh();};
        control(satMix,"satMix","Mix %");control(satOutput,"satOutput","Output dB");
        satMix.slider.setTooltip("Amount of every band's change. 0% keeps all levels (the crossovers' phase rotation stays while the stage is on).");
        for(int cross=0;cross<2;++cross)control(satCrossovers[size_t(cross)],"satCrossover"+juce::String(cross+1),"Crossover "+juce::String(cross+1)+" Hz");
        for(int band=0;band<3;++band) {
            auto& ui=satBands[size_t(band)];
            choice(ui.type,saturationID(band,"type"),saturationTypeNames());
            ui.type.setTooltip("Clean passes the band unchanged. Tape: soft symmetric. Tube: asymmetric (even harmonics). Clip: firm cubic clip.");
            control(ui.drive,saturationID(band,"drive"),"Drive dB");
        }
        // Two-filter modulation (docs/FILTER_MODULATION_1_7.md).
        toggle(fltOn,"FILTER ON","fltOn");
        fltOn.setTooltip("Turn both filters on or off with a 10 ms crossfade. Off leaves the signal untouched.");
        fltSelector.addItemList({"Filter A","Filter B","Modulation"},1);   // Modulation: the 1.9 LFO 2 / lock / retrigger / envelope source
        fltSelector.setComponentID("commonDynamics.filterUnit");fltSelector.setSelectedId(1,juce::dontSendNotification);addChildComponent(fltSelector);
        fltSelector.onChange=[this]{selectedFilter=std::clamp(fltSelector.getSelectedId()-1,0,filterCount);syncPage();refresh();};
        choice(fltRouting,"fltRouting",filterRoutingNames());
        fltRouting.setTooltip("Serial: A feeds B. Parallel: A and B both hear the input and are averaged.");
        choice(fltLfoShape,"fltLfoShape",filterLfoShapeNames());choice(fltLfoSync,"fltLfoSync",filterLfoSyncNames());
        fltLfoShape.setTooltip("LFO waveform shared by both filters.");
        fltLfoSync.setTooltip("Free uses LFO Rate. A note value follows the host or Chimera tempo; LOCK TO BAR aligns it to the host position.");
        // 1.9 expanded modulation (docs/MODULATION_1_9.md).
        choice(fltLfo2Shape,"fltLfo2Shape",filterLfoShapeNames());choice(fltLfo2Sync,"fltLfo2Sync",filterLfoSyncNames());
        fltLfo2Shape.setTooltip("LFO 2 waveform, shared by both filters (each has its own LFO 2 amount).");
        fltLfo2Sync.setTooltip("Free uses LFO 2 rate; a note value follows the tempo.");
        toggle(fltLock,"LOCK TO BAR","fltLfoLock");toggle(fltRetrigger,"RETRIGGER","fltLfoRetrigger");
        fltLock.setTooltip("Synced LFOs follow the host's bar position while the transport plays, so they land on the same beat every pass.");
        fltRetrigger.setTooltip("Restart both LFOs on each attack (when not locked to the bar).");
        choice(fltEnvSource,"fltEnvSource",filterEnvelopeSourceNames());
        fltEnvSource.setTooltip("The envelope follower listens to this processor's own signal, or to the plugin's Sidechain input when the host connects one.");
        control(fltMix,"fltMix","Mix %");control(fltOutput,"fltOutput","Output dB");
        constexpr std::array<const char*,6> modIDs{"fltLfoRate","fltLfoStereo","fltEnvAttack","fltEnvRelease","fltEnvSensitivity","fltLfo2Rate"};
        constexpr std::array<const char*,6> modNames{"LFO rate Hz","LFO stereo deg","Env attack ms","Env release ms","Env sensitivity dB","LFO 2 rate Hz"};
        for(size_t i=0;i<fltMod.size();++i)control(fltMod[i],modIDs[i],modNames[i]);
        fltMod[1].slider.setTooltip("Phase offset of the right channel's LFO; 180 sweeps the two channels in opposite directions.");
        fltMod[4].slider.setTooltip("Gain into the envelope follower. The follower reaches full scale at 0 dBFS with 0 dB sensitivity.");
        for(int f=0;f<filterCount;++f) {
            auto& ui=fltUnits[size_t(f)];
            toggle(ui.on,f==0?"FILTER A ON":"FILTER B ON",filterID(f,"on"));
            choice(ui.type,filterID(f,"type"),filterTypeNames());choice(ui.slope,filterID(f,"slope"),filterSlopeNames());
            constexpr std::array<const char*,6> fields{"cutoff","resonance","lfo","env","lfo2","drive"};
            constexpr std::array<const char*,6> labels{"Cutoff Hz","Resonance %","LFO amount oct","Envelope amount oct","LFO 2 amount oct","Drive %"};
            for(size_t i=0;i<ui.controls.size();++i)control(ui.controls[i],filterID(f,fields[i]),labels[i]);
            ui.controls[2].slider.setTooltip("How far the LFO moves this cutoff, in octaves each way.");
            ui.controls[3].slider.setTooltip("How far the envelope moves this cutoff at full level, in octaves. Negative values close it.");
            ui.controls[4].slider.setTooltip("How far LFO 2 moves this cutoff, in octaves each way.");
            ui.controls[5].slider.setTooltip("Soft saturation ahead of this filter, unity at full scale. 0 % is exact bypass.");
        }
        toggle(deEsserOn,"DE-ESSER ON","deEsserOn");toggle(deEsserListen,"LISTEN","deEsserListen");
        constexpr std::array<const char*,7> deIDs{"deEsserFrequency","deEsserThreshold","deEsserRange","deEsserRatio","deEsserAttack","deEsserRelease","deEsserKnee"};
        constexpr std::array<const char*,7> deNames{"Split frequency Hz","Threshold dB","Max cut dB","Ratio","Attack ms","Release ms","Knee dB"};
        for(size_t i=0;i<deEsser.size();++i)control(deEsser[i],deIDs[i],deNames[i]);
        deEsserListen.setTooltip("Audition the detector/high band. This sound control is stored in presets.");
        spectrum.onMarkerDragStart=[this](int marker){endMarkerGesture();markerParameter=markerParameterFor(marker);if(markerParameter)markerParameter->beginChangeGesture();};
        spectrum.onMarkerDrag=[this](int,float frequency){if(markerParameter)markerParameter->setValueNotifyingHost(markerParameter->convertTo0to1(frequency));};
        spectrum.onMarkerDragEnd=[this](int){endMarkerGesture();};
        addChildComponent(autoView);
        autoView.onClose=[this]{autoDismissed=true;updateAutoView();};
        autoView.onShow=[this](int target){if(onAutoSetShow)onAutoSetShow(target);};
        refresh();syncPage();startTimerHz(20);
    }
    ~CommonDynamicsPanel() override {
        stopTimer();endMarkerGesture();spectrum.onMarkerDragStart={};spectrum.onMarkerDrag={};spectrum.onMarkerDragEnd={};
    }

    Page selectedPage() const noexcept {return page;}
    // Gate page side: 0 = PRE RIG, 1 = POST RIG.
    int selectedGateSide() const noexcept {return gateSide;}
    void selectGateSide(int side){
        gateSide=std::clamp(side,0,1);syncPage();refresh();
        if(page==Page::gate && onPageSelected)onPageSelected(page); // the path highlight follows the side
    }
    // The released gate settings are shown on the side gateAfterRig puts them.
    bool mainGateShown() const {return (gateSide==1)==(raw("gateAfterRig")>.5f);}
    void setGateUnitMeterProvider(std::function<float(int)> provider){gateUnitMeterProvider=std::move(provider);}
    void invalidateAnalysisSource(){spectrum.sourceChanged();}
    void selectPage(Page next) {
        // Native PRE/POST own their actual model editor. Delegate before
        // changing the underlying page so closing a dialog returns to the
        // previously usable workspace, with no inactive legacy controls.
        if(next==Page::dynamicEQ){if(onDynamicEQRequested)onDynamicEQRequested();return;}
        if((next==Page::preCompressor || next==Page::postCompressor) && onCompressorSourceRequested
            && onCompressorSourceRequested(next))return;
        if(page!=next){endMarkerGesture();page=next;}
        syncPage();refresh();
        if(onPageSelected)onPageSelected(next);
    }
    void setMeterProvider(std::function<float(DynamicsStage)> provider){meterProvider=std::move(provider);}
    void setMultibandProviders(std::function<std::array<float,multibandMaxCrossovers>()> crossoverProvider,std::function<float(int)> reductionProvider) {
        effectiveCrossovers=std::move(crossoverProvider);mbReductionProvider=std::move(reductionProvider);
    }
    std::function<void(Page)> onPageSelected;
    std::function<bool(Page)> onCompressorSourceRequested;
    std::function<void()> onDynamicEQRequested;
    std::function<void()> onAutoSet,onAutoCancel,onAutoUndo;
    // Presentation of the Auto Set options; the editor maps them to the controller.
    // instrument 0 detect/1 guitar/2 bass; style 0 rock/1 clean/2 metal; strength in percent;
    // locks: bit i keeps autoLockNames[i] as it is (the controller's Lock order).
    struct AutoSetOptions {int instrument{0};bool tone{true};int style{0};bool finish{true};bool multiband{true};bool gates{true};int strength{100};unsigned locks{};bool dynamicEQ{true};bool level{true};bool amp{true};bool cab{true};};
    static constexpr std::array<const char*,12> autoLockNames{"Input gain","Gate","Tone EQ","Compressor","Multiband","De-esser","Limiter","Dynamic EQ","Final EQ","Output level","Amp","Cabinet mic"};
    static constexpr std::array<int,4> autoStrengths{100,75,50,25};
    std::function<void(const AutoSetOptions&)> onAutoOptionsChanged;
    void setAutoSetOptions(const AutoSetOptions& chosen){autoOptionsValue=chosen;updateAutoOptionsText();}
    const AutoSetOptions& autoSetOptions() const noexcept {return autoOptionsValue;}
    // Menu ids: 1-3 instrument, 11-13 style, 21 Tone EQ, 22 de-esser + limiter, 23 multiband step,
    // 24 Gate 2 + Expander step, 25 Dynamic EQ step, 26 Final EQ + output step, 27 amp step, 28 cabinet mic step, 31-34 strength, 41-52 locks.
    void chooseAutoOption(int id) {
        auto& o=autoOptionsValue;
        if(id>=1 && id<=3)o.instrument=id-1;else if(id>=11 && id<=13)o.style=id-11;else if(id==21)o.tone=!o.tone;else if(id==22)o.finish=!o.finish;else if(id==23)o.multiband=!o.multiband;else if(id==24)o.gates=!o.gates;else if(id==25)o.dynamicEQ=!o.dynamicEQ;else if(id==26)o.level=!o.level;else if(id==27)o.amp=!o.amp;else if(id==28)o.cab=!o.cab;
        else if(id>=31 && id<31+int(autoStrengths.size()))o.strength=autoStrengths[size_t(id-31)];
        else if(id>=41 && id<41+int(autoLockNames.size()))o.locks^=1u<<(id-41);else return;
        updateAutoOptionsText();if(onAutoOptionsChanged)onAutoOptionsChanged(o);
    }
    // Presentation only: the controller owns the measurement and parameter writes.
    void setAutoSetStatus(const juce::String& text,bool busy,bool canUndo) {
        autoText=text;autoBusy=busy;autoSet.setButtonText(busy?"CANCEL":"AUTO SET");autoUndo.setEnabled(canUndo && !busy);autoOptions.setEnabled(!busy);
        repaint(520,38,std::max(1,getWidth()-740),46);
    }
    const juce::String& autoSetStatus() const noexcept {return autoText;}
    // Progress while Auto Set listens, then its results; shown over the spectrum.
    void setAutoSetReport(const AutoSetReport& report) {
        if(report.listening)autoDismissed=false;
        autoView.setReport(report);updateAutoView();
    }
    const AutoSetView& autoSetView() const noexcept {return autoView;}
    std::function<void(int)> onAutoSetShow;
    void visibilityChanged() override {if(!isShowing())endMarkerGesture();syncPage();}
    void parentHierarchyChanged() override {syncPage();}

    void resized() override {
        const int tabWidth=(getWidth()-24)/int(tabs.size());
        for(size_t i=0;i<tabs.size();++i)tabs[i].setBounds(12+int(i)*tabWidth,4,tabWidth-5,31);
        const int sideX=std::max(600,getWidth()-338),sideWidth=std::max(10,getWidth()-sideX-16),column=(sideWidth-12)/2;
        const int graphHeight=std::max(160,getHeight()-94);
        spectrum.setBounds(8,84,sideX-16,page==Page::multiband || page==Page::filter?graphHeight-57:graphHeight);
        compOn.setBounds(18,45,148,28);detector.setBounds(176,45,100,28);compKey.setBounds(284,45,90,28);
        gateKey.setBounds(page==Page::expander?294:532,45,90,28);mbKey.setBounds(634,45,90,28);
        gateSides[0].setBounds(18,45,74,28);gateSides[1].setBounds(96,45,80,28);
        gateOn.setBounds(184,45,96,28);gate2On.setBounds(184,45,96,28);gateMode.setBounds(288,45,104,28);gate2Mode.setBounds(288,45,104,28);
        gatePlacement.setBounds(400,45,124,28);expanderOn.setBounds(18,45,140,28);expanderPlacement.setBounds(168,45,118,28);
        limiterOn.setBounds(18,45,160,28);preOn.setBounds(263,45,166,28);postOn.setBounds(263,45,166,28);
        const auto grid=[&](auto& controls,int y=103){for(size_t i=0;i<controls.size();++i)controls[i].bounds(sideX+int(i%2)*(column+12),y+int(i/2)*66,column);};
        grid(comp);grid(gate);grid(gate2);grid(expander);grid(pre);grid(post);grid(deEsser);
        ceiling.bounds(sideX,110,sideWidth);limitRelease.bounds(sideX,181,sideWidth);
        lowAmount.bounds(sideX,110,sideWidth);lowBlend.bounds(sideX,181,sideWidth);
        mbOn.setBounds(18,45,153,28);mbBandSelector.setBounds(180,45,170,28);mbDetector.setBounds(358,45,80,28);
        mbStereo.setBounds(446,45,72,28);mbLink.setBounds(526,45,100,28);
        for(auto& band:mbBands) {
            band.mode.setBounds(sideX,89,column,28);band.channels.setBounds(sideX+column+12,89,column,28);band.bypass.setBounds(sideX,124,column,25);band.solo.setBounds(sideX+column+12,124,column,25);
            for(size_t field=0;field<band.controls.size();++field)band.controls[field].bounds(sideX+int(field%2)*(column+12),158+int(field/2)*61,column);
        }
        mbMix.bounds(sideX+column+12,341,column);
        // Bottom row: the band count, then the active crossovers.
        mbBandCount.setBounds(18,getHeight()-36,96,28);
        const int crossCount=shownMBBands-1,crossWidth=(sideX-32-108)/crossCount;
        for(int cross=0;cross<crossCount;++cross)crossovers[size_t(cross)].bounds(126+cross*crossWidth,getHeight()-56,crossWidth-12);
        satOn.setBounds(18,45,153,28);satBandSelector.setBounds(180,45,170,28);satQuality.setBounds(358,45,110,28);
        for(auto& band:satBands){band.type.setBounds(sideX,89,sideWidth,28);band.drive.bounds(sideX,130,sideWidth);}
        satMix.bounds(sideX,195,column);satOutput.bounds(sideX+column+12,195,column);
        for(int cross=0;cross<2;++cross)satCrossovers[size_t(cross)].bounds(18+cross*((sideX-32)/3),getHeight()-56,(sideX-32)/3-12);
        fltOn.setBounds(18,45,153,28);fltSelector.setBounds(180,45,110,28);fltRouting.setBounds(298,45,150,28);
        fltLfoShape.setBounds(456,45,96,28);fltLfoSync.setBounds(560,45,84,28);
        for(auto& unit:fltUnits) {
            unit.on.setBounds(sideX,89,column,28);unit.type.setBounds(sideX+column+12,89,column,28);unit.slope.setBounds(sideX,124,column,25);
            for(size_t i=0;i<unit.controls.size();++i)unit.controls[i].bounds(sideX+int(i%2)*(column+12),158+int(i/2)*61,column);
        }
        fltMix.bounds(sideX,341,column);fltOutput.bounds(sideX+column+12,341,column);
        // The Modulation view uses the side column the filter units use.
        fltLfo2Shape.setBounds(sideX,89,column,28);fltLfo2Sync.setBounds(sideX+column+12,89,column,28);
        fltLock.setBounds(sideX,124,column,28);fltRetrigger.setBounds(sideX+column+12,124,column,28);fltEnvSource.setBounds(sideX,160,sideWidth,28);
        {const int width=(sideX-32)/int(fltMod.size());for(size_t i=0;i<fltMod.size();++i)fltMod[i].bounds(18+int(i)*width,getHeight()-56,width-12);}
        deEsserOn.setBounds(18,45,150,28);deEsserListen.setBounds(178,45,104,28);
        autoSet.setBounds(sideX,getHeight()-40,100,28);autoOptions.setBounds(sideX+106,getHeight()-40,118,28);
        autoUndo.setBounds(sideX+230,getHeight()-40,std::max(1,sideWidth-230),28);updateAutoView();repaint();
    }
    void paint(juce::Graphics& g) override {
        const juce::Colour muted(0xff9fb4ad),text(0xffd9ebe3),line(0xff273b3d);
        g.fillAll(juce::Colour(0xff121a1d));g.setColour(line);g.drawHorizontalLine(38,12.f,float(getWidth()-12));
        {
            const int sideX=std::max(600,getWidth()-338);g.drawVerticalLine(sideX-10,85.f,float(getHeight()-12));
            g.setFont(juce::FontOptions(12.f));g.setColour(muted);
            if(autoText.isNotEmpty() && autoPage())g.drawFittedText(autoText,532,41,std::max(1,getWidth()-770),38,juce::Justification::centredLeft,2);
            else {const int x=page==Page::multiband?732:page==Page::filter?652:532;g.drawText(contextText(),x,47,std::max(1,getWidth()-770-(x-532)),25,juce::Justification::centredLeft);}
            g.setColour(text);g.setFont(juce::FontOptions(14.f,juce::Font::bold));
            const auto meter=currentReduction<0?"GAIN "+juce::String(-currentReduction,1)+" dB":"GR "+juce::String(currentReduction,1)+" dB";
            if(page!=Page::saturation && page!=Page::filter)g.drawText(meter,getWidth()-222,46,204,27,juce::Justification::centredRight);   // saturation has no gain reduction
            if(page==Page::limiter)drawNote(g,sideX,269,"SAMPLE-PEAK LIMITING\nStereo linked / zero lookahead\nTrue-peak protection is not measured.");
            if(page==Page::gate)drawNote(g,sideX,319,"Clean DI detector\nPRE and POST gates run together.\nRange controls maximum attenuation.");
            if(page==Page::expander)drawNote(g,sideX,319,"Clean DI detector\nRuns together with the gates.\nThe spectrum shows the main gate input.");
            if(page==Page::lowCompressor)drawNote(g,sideX,271,raw("mode")>1.5f?"MATRIX LOW DI\nThe compressor acts on the clean LOW branch.":"MATRIX ROUTE INACTIVE\nSelect Matrix routing to hear this compressor.");
            if(page==Page::preCompressor)drawNote(g,sideX,267,sourceIsActive()?"LEGACY PRE RACK\nNative pedalboard compressors open their own spectrum editor.":"LEGACY PRE INACTIVE\nStored controls. Select PRE again to open the current native compressor.");
            if(page==Page::postCompressor)drawNote(g,sideX,329,sourceIsActive()?"LEGACY POST RACK\nNative POST compressors use their attached controls.":"LEGACY POST INACTIVE\nStored controls. Select POST again to open the current native compressor.");
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
    struct MBBand {juce::TextButton bypass,solo;juce::ComboBox mode,channels;std::array<Control,7> controls;};
    static bool isCompressorPage(Page selected){return selected==Page::compressor || selected==Page::preCompressor || selected==Page::postCompressor || selected==Page::lowCompressor;}
    static DynamicsStage stageFor(Page selected) {
        switch(selected) {
            case Page::compressor:return DynamicsStage::compressor;case Page::gate:case Page::expander:return DynamicsStage::gate;
            case Page::limiter:return DynamicsStage::limiter;case Page::multiband:return DynamicsStage::multiband;
            case Page::deesser:return DynamicsStage::deesser;case Page::preCompressor:return DynamicsStage::preCompressor;
            case Page::postCompressor:return DynamicsStage::postCompressor;case Page::lowCompressor:return DynamicsStage::lowCompressor;
            case Page::saturation:return DynamicsStage::saturation;
            case Page::filter:return DynamicsStage::filter;
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
    template<typename Controls> void showControls(Controls& controls,bool showing){for(auto& c:controls)c.show(showing);}
    void syncPage() {
        for(size_t i=0;i<tabs.size();++i)tabs[i].setToggleState(tabPages[i]==(isCompressorPage(page)?Page::compressor:page),juce::dontSendNotification);
        compOn.setVisible(page==Page::compressor);detector.setVisible(page==Page::compressor);compKey.setVisible(page==Page::compressor);
        gateKey.setVisible(page==Page::gate || page==Page::expander);mbKey.setVisible(page==Page::multiband);showControls(comp,page==Page::compressor);
        const bool gatePage=page==Page::gate,mainGate=mainGateShown();shownMainGate=mainGate;
        for(int side=0;side<2;++side){gateSides[size_t(side)].setVisible(gatePage);gateSides[size_t(side)].setToggleState(side==gateSide,juce::dontSendNotification);}
        gateOn.setVisible(gatePage && mainGate);gateMode.setVisible(gatePage && mainGate);showControls(gate,gatePage && mainGate);
        gate2On.setVisible(gatePage && !mainGate);gate2Mode.setVisible(gatePage && !mainGate);showControls(gate2,gatePage && !mainGate);
        gatePlacement.setVisible(gatePage);
        const bool expanderPage=page==Page::expander;
        expanderOn.setVisible(expanderPage);expanderPlacement.setVisible(expanderPage);showControls(expander,expanderPage);
        limiterOn.setVisible(page==Page::limiter);ceiling.show(page==Page::limiter);limitRelease.show(page==Page::limiter);
        preOn.setVisible(page==Page::preCompressor);postOn.setVisible(page==Page::postCompressor);showControls(pre,page==Page::preCompressor);showControls(post,page==Page::postCompressor);
        lowAmount.show(page==Page::lowCompressor);lowBlend.show(page==Page::lowCompressor);
        const bool mb=page==Page::multiband;mbOn.setVisible(mb);mbDetector.setVisible(mb);mbStereo.setVisible(mb);mbLink.setVisible(mb);mbBandSelector.setVisible(mb);mbBandCount.setVisible(mb);mbMix.show(mb);
        for(int cross=0;cross<multibandMaxCrossovers;++cross)crossovers[size_t(cross)].show(mb && cross<shownMBBands-1);
        for(int band=0;band<multibandMaxBandCount;++band){auto& ui=mbBands[size_t(band)];const bool showing=mb && band==selectedMBBand;ui.bypass.setVisible(showing);ui.solo.setVisible(showing);ui.mode.setVisible(showing);ui.channels.setVisible(showing);showControls(ui.controls,showing);}
        const bool sat=page==Page::saturation;satOn.setVisible(sat);satQuality.setVisible(sat);satBandSelector.setVisible(sat);satMix.show(sat);satOutput.show(sat);showControls(satCrossovers,sat);
        for(int band=0;band<3;++band){auto& ui=satBands[size_t(band)];const bool showing=sat && band==selectedSatBand;ui.type.setVisible(showing);ui.drive.show(showing);}
        const bool flt=page==Page::filter;fltOn.setVisible(flt);fltSelector.setVisible(flt);fltRouting.setVisible(flt);fltLfoShape.setVisible(flt);fltLfoSync.setVisible(flt);
        fltMix.show(flt);fltOutput.show(flt);showControls(fltMod,flt);
        for(juce::Component* c:{static_cast<juce::Component*>(&fltLfo2Shape),static_cast<juce::Component*>(&fltLfo2Sync),static_cast<juce::Component*>(&fltLock),static_cast<juce::Component*>(&fltRetrigger),static_cast<juce::Component*>(&fltEnvSource)})c->setVisible(flt && selectedFilter==filterCount);
        fltMix.show(flt && selectedFilter<filterCount);fltOutput.show(flt && selectedFilter<filterCount);
        for(int f=0;f<filterCount;++f){auto& ui=fltUnits[size_t(f)];const bool showing=flt && f==selectedFilter;ui.on.setVisible(showing);ui.type.setVisible(showing);ui.slope.setVisible(showing);showControls(ui.controls,showing);}
        deEsserOn.setVisible(page==Page::deesser);deEsserListen.setVisible(page==Page::deesser);
        autoSet.setVisible(autoPage());autoOptions.setVisible(autoPage());autoUndo.setVisible(autoPage());showControls(deEsser,page==Page::deesser);
        if(page==Page::preCompressor)spectrum.setPreOwner(-1);
        if(page==Page::postCompressor)spectrum.setPostNativeSource(0);
        spectrum.setVisible(true);spectrum.setStage(stageFor(page));resized();repaint();
    }
    void refresh() {
        if(const int count=raw("mbBandCount")>.5f?6:4;count!=shownMBBands){applyMBBandCount(count);syncPage();}
        {   // Channel names follow the stereo domain.
            const bool midSide=raw("mbStereoMode")>.5f;
            if(midSide!=channelNamesMidSide) {
                channelNamesMidSide=midSide;
                for(auto& band:mbBands){band.channels.changeItemText(2,midSide?"Mid only":"Left only");band.channels.changeItemText(3,midSide?"Side only":"Right only");}
            }
        }
        const int sourceRoute=page==Page::gate?int(raw("gateAfterRig")>.5f):int(sourceIsActive());
        const int sourceKey=int(page)*2+sourceRoute;
        if(sourceKey!=observedSourceKey) {
            // Routing changes can deactivate a stored legacy/LOW editor, or
            // move the gate tap. A frozen frame belongs to its original route.
            if(observedSourceKey>=0 && observedSourceKey/2==int(page))spectrum.sourceChanged();
            observedSourceKey=sourceKey;repaint();
        }
        gate[1].slider.setEnabled(raw("gateMode")>.5f);gate2[1].slider.setEnabled(raw("gate2Mode")>.5f);
        if(page==Page::gate && mainGateShown()!=shownMainGate)syncPage(); // SWAP or automation moved the settings
        const auto stage=stageFor(page);float reduction=0;
        if(page==Page::multiband && mbReductionProvider)reduction=mbReductionProvider(selectedMBBand);
        else if((page==Page::gate && !mainGateShown()) || page==Page::expander)reduction=gateUnitMeterProvider?gateUnitMeterProvider(page==Page::expander?2:1):0.f;
        else if(meterProvider)reduction=meterProvider(stage);
        else if(page==Page::compressor)reduction=dsp.compReduction.load();
        else if(page==Page::limiter)reduction=dsp.limitReduction.load();
        reduction=sourceIsActive() && std::isfinite(reduction)?reduction:0;
        if(std::abs(reduction-currentReduction)>.049f){currentReduction=reduction;repaint(getWidth()-230,42,218,33);}
        std::array<DynamicsSpectrumPanel::FrequencyMarker,DynamicsSpectrumPanel::maxFrequencyMarkers> markers{};int count=0;
        if(page==Page::compressor){markers[0]={raw("cleanCompHP",20),"Detector HP"};count=1;spectrum.setBandHighlight(0,0);}
        else if(page==Page::deesser){const float frequency=raw("deEsserFrequency",6000);markers[0]={frequency,"Split"};count=1;spectrum.setBandHighlight(frequency,20000);}
        else if(page==Page::multiband) {
            const auto cutoffs=effectiveCrossovers?effectiveCrossovers():std::array<float,multibandMaxCrossovers>{raw("mbCrossover1",120),raw("mbCrossover2",700),raw("mbCrossover3",3500),raw("mbCrossover4",6000),raw("mbCrossover5",11000)};
            count=shownMBBands-1;
            for(int cross=0;cross<count;++cross)markers[size_t(cross)]={cutoffs[size_t(cross)],"X"+juce::String(cross+1)};
            spectrum.setBandHighlight(selectedMBBand==0?20.f:cutoffs[size_t(selectedMBBand-1)],selectedMBBand==count?20000.f:cutoffs[size_t(selectedMBBand)]);
        } else if(page==Page::saturation) {
            const std::array<float,2> cutoffs{raw("satCrossover1",200),raw("satCrossover2",2500)};
            for(int cross=0;cross<2;++cross)markers[size_t(cross)]={cutoffs[size_t(cross)],"X"+juce::String(cross+1)};count=2;
            spectrum.setBandHighlight(selectedSatBand==0?20.f:cutoffs[size_t(selectedSatBand-1)],selectedSatBand==2?20000.f:cutoffs[size_t(selectedSatBand)]);
        } else if(page==Page::filter) {
            for(int f=0;f<filterCount;++f)markers[size_t(f)]={raw(filterID(f,"cutoff"),f==0?1000.f:80.f),f==0?"A":"B"};count=filterCount;
            spectrum.setBandHighlight(0,0);
        } else spectrum.setBandHighlight(0,0);
        fltMod[0].slider.setEnabled(raw("fltLfoSync")<.5f);   // a synced LFO follows the tempo
        fltMod[5].slider.setEnabled(raw("fltLfo2Sync")<.5f);
        spectrum.setFrequencyMarkers(markers,count);
    }
    juce::RangedAudioParameter* markerParameterFor(int marker) {
        if(page==Page::compressor && marker==0)return state.getParameter("cleanCompHP");
        if(page==Page::deesser && marker==0)return state.getParameter("deEsserFrequency");
        if(page==Page::multiband && marker>=0 && marker<shownMBBands-1)return state.getParameter("mbCrossover"+juce::String(marker+1));
        if(page==Page::saturation && marker>=0 && marker<2)return state.getParameter("satCrossover"+juce::String(marker+1));
        if(page==Page::filter && marker>=0 && marker<filterCount)return state.getParameter(filterID(marker,"cutoff"));
        return nullptr;
    }
    void endMarkerGesture(){if(markerParameter){markerParameter->endChangeGesture();markerParameter=nullptr;}}
    juce::String contextText() const {
        switch(page) {
            case Page::compressor:return "After Tone EQ / before POST";
            case Page::gate:return gateSide==0?"PRE RIG / clean DI detection / before PRE":"POST RIG / clean DI detection / after rig";
            case Page::expander:return raw("expAfterRig")>.5f?"Expander / clean DI detection / after rig":"Expander / clean DI detection / before rig";
            case Page::limiter:return "Final output / sample peak";
            case Page::multiband:return juce::String(shownMBBands)+" bands / stereo linked";
            case Page::deesser:return "High-band dynamics";
            case Page::saturation:return raw("satQuality")>.5f?"3 bands / Studio 4x / after multiband":"3 bands / Live, no latency / after multiband";
            case Page::filter:return "2 filters / after saturation";
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
    juce::AudioProcessorValueTreeState& state;CommonDynamics& dsp;DynamicsSpectrumPanel spectrum;
    Page page{Page::compressor};std::array<juce::TextButton,8> tabs;
    static constexpr std::array<Page,8> tabPages{Page::compressor,Page::gate,Page::expander,Page::limiter,Page::multiband,Page::saturation,Page::filter,Page::deesser};
    struct FltUnit {juce::TextButton on;juce::ComboBox type,slope;std::array<Control,6> controls;};
    std::array<FltUnit,filterCount> fltUnits;Control fltMix,fltOutput;std::array<Control,6> fltMod;
    juce::TextButton fltOn,fltLock,fltRetrigger;juce::ComboBox fltSelector,fltRouting,fltLfoShape,fltLfoSync,fltLfo2Shape,fltLfo2Sync,fltEnvSource;int selectedFilter{};
    struct SatBand {juce::ComboBox type;Control drive;};
    std::array<SatBand,3> satBands;Control satMix,satOutput;std::array<Control,2> satCrossovers;
    juce::TextButton satOn;juce::ComboBox satQuality,satBandSelector;int selectedSatBand{};
    juce::ComboBox detector,gateMode,gate2Mode,mbDetector,mbBandSelector,mbStereo,mbBandCount;int shownMBBands{4};
    // Band names and the selectable bands follow the band count.
    void applyMBBandCount(int count) {
        shownMBBands=count==6?6:4;
        constexpr std::array<const char*,4> four{"Band 1 / LOW","Band 2 / LOW MID","Band 3 / HIGH MID","Band 4 / HIGH"};
        constexpr std::array<const char*,6> six{"Band 1 / LOW","Band 2 / LOW MID","Band 3 / MID","Band 4 / HIGH MID","Band 5 / PRESENCE","Band 6 / AIR"};
        for(int band=0;band<multibandMaxBandCount;++band) {
            mbBandSelector.changeItemText(band+1,shownMBBands==6?six[size_t(band)]:band<4?four[size_t(band)]:"");
            mbBandSelector.setItemEnabled(band+1,band<shownMBBands);
        }
        if(selectedMBBand>=shownMBBands){selectedMBBand=shownMBBands-1;mbBandSelector.setSelectedId(shownMBBands,juce::dontSendNotification);}
        else mbBandSelector.setSelectedId(selectedMBBand+1,juce::dontSendNotification);   // refreshes the shown name
    }
    juce::Slider mbLink;std::unique_ptr<SA> mbLinkAttachment;bool channelNamesMidSide{true};
    juce::TextButton gate2On,expanderOn,expanderPlacement;std::array<juce::TextButton,2> gateSides;
    juce::TextButton compOn,gateOn,gatePlacement,limiterOn,preOn,postOn,mbOn,deEsserOn,deEsserListen,compKey,gateKey,mbKey;
    juce::TextButton autoSet{"AUTO SET"},autoUndo{"UNDO AUTO"};juce::TextButton autoOptions;AutoSetOptions autoOptionsValue;juce::String autoText;bool autoBusy{};
    void updateAutoOptionsText() {
        constexpr std::array<const char*,3> instruments{"Auto","Guitar","Bass"},styles{"Rock","Clean","Metal"};
        const auto& o=autoOptionsValue;
        autoOptions.setButtonText(juce::String(instruments[size_t(o.instrument)])+" / "+styles[size_t(o.style)]
            +(o.strength<100?" "+juce::String(o.strength)+"%":juce::String())+(o.locks!=0?juce::String(" *"):juce::String()));
    }
    void showAutoOptions() {
        const auto& o=autoOptionsValue;juce::PopupMenu menu;
        menu.addSectionHeader("Instrument");
        menu.addItem(1,"Detect from playing",true,o.instrument==0);menu.addItem(2,"Guitar",true,o.instrument==1);menu.addItem(3,"Bass",true,o.instrument==2);
        menu.addSectionHeader("Style");
        menu.addItem(11,"Rock",true,o.style==0);menu.addItem(12,"Clean",true,o.style==1);menu.addItem(13,"Metal",true,o.style==2);
        menu.addSectionHeader("Steps");
        menu.addItem(24,"Gate 2 and Expander",true,o.gates);menu.addItem(27,"Amp gain and tone",true,o.amp);menu.addItem(28,"Cabinet mic placement",true,o.cab);menu.addItem(21,"Tone EQ",true,o.tone);menu.addItem(25,"Dynamic EQ",true,o.dynamicEQ);menu.addItem(23,"Multiband",true,o.multiband);menu.addItem(22,"De-esser and limiter",true,o.finish);menu.addItem(26,"Final EQ and output level",true,o.level);
        menu.addSectionHeader("Strength");
        for(size_t i=0;i<autoStrengths.size();++i)menu.addItem(31+int(i),juce::String(autoStrengths[i])+"%",true,o.strength==autoStrengths[i]);
        menu.addSectionHeader("Lock (Auto Set keeps these)");
        for(size_t i=0;i<autoLockNames.size();++i)menu.addItem(41+int(i),autoLockNames[i],true,(o.locks>>i&1u)!=0);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&autoOptions),[safe=juce::Component::SafePointer<CommonDynamicsPanel>(this)](int id){if(safe!=nullptr)safe->chooseAutoOption(id);});
    }
    bool autoPage() const noexcept {return page==Page::compressor || page==Page::gate;}
    void updateAutoView() {
        const auto& r=autoView.report();
        const bool showing=autoPage() && (r.listening || ((r.finished || r.message.isNotEmpty()) && !autoDismissed));
        const int sideX=std::max(600,getWidth()-338);
        autoView.setBounds(16,92,std::max(1,sideX-32),autoView.preferredHeight());autoView.setVisible(showing);
        if(showing)autoView.toFront(false);
    }
    AutoSetView autoView;bool autoDismissed{};
    std::array<Control,8> comp;std::array<Control,6> gate;std::array<Control,6> gate2;std::array<Control,6> expander;std::array<Control,3> pre;std::array<Control,5> post;
    Control ceiling,limitRelease,lowAmount,lowBlend,mbMix;std::array<Control,multibandMaxCrossovers> crossovers;std::array<Control,7> deEsser;
    std::array<MBBand,multibandMaxBandCount> mbBands;
    std::vector<std::unique_ptr<BA>> buttons;std::vector<std::unique_ptr<CA>> choices;
    std::function<float(DynamicsStage)> meterProvider;std::function<std::array<float,multibandMaxCrossovers>()> effectiveCrossovers;std::function<float(int)> mbReductionProvider,gateUnitMeterProvider;
    juce::RangedAudioParameter* markerParameter{};int selectedMBBand{},gateSide{},observedSourceKey{-1};bool shownMainGate{true};float currentReduction{};
};
}
