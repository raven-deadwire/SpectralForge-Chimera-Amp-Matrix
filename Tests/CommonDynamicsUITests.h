#pragma once
#include "SignalPathUITests.h"
#include "DynamicsSpectrumPanel.h"
namespace commonDynamicsUITests {
using namespace nativeUITests;
using Page=spectralforge::CommonDynamicsPanel::Page;
using Stage=spectralforge::DynamicsStage;

inline void visibleInside(juce::Component& editor,juce::Component& root,const juce::String& id) {
    auto* component=find(root,id);require(component && component->isShowing(),"Focused dynamics control is absent");
    inside(editor,*component,"Focused dynamics control is clipped at this editor scale");
}
inline void acknowledge(ChimeraProcessor& processor) {
    juce::AudioBuffer<float> silence(2,256);silence.clear();juce::MidiBuffer midi;
    processor.processBlock(silence,midi);settle(65);prepareDynamicsCapture(processor);
}
inline void audio(ChimeraProcessor& processor,double frequency=937.5,int blocks=64) {
    // Process the production route. Opposite-polarity R must not cancel L in
    // the power spectrum. The finite burst remains below the FIFO capacity.
    juce::AudioBuffer<float> block(2,256);juce::MidiBuffer midi;
    for(int b=0;b<blocks;++b) {
        if(b%8==0)prepareDynamicsCapture(processor);
        for(int n=0;n<256;++n) {
            const float sample=float(.25*std::sin(juce::MathConstants<double>::twoPi*frequency*double(b*256+n)/48000.));
            block.setSample(0,n,sample);block.setSample(1,n,-sample);
        }
        processor.processBlock(block,midi);
        if((b+1)%8==0)settle(10);
    }
    settle(65);
    consumeSpectrumAudio(processor.dynamicsAnalysis().fifo);
    for(int bank=0;bank<2;++bank)consumeSpectrumAudio(processor.graphicalEQ(bank).analyzer);
}
inline void openWorkspace(juce::Component& editor) {
    auto* tone=find<juce::TextButton>(editor,"path.tone.eq");require(tone,"Tone EQ navigation missing");signalPathTests::click(*tone);
    auto* open=find<juce::TextButton>(editor,"commonDynamics.open.compressor");require(open && open->isShowing(),"Dynamics entry missing");signalPathTests::click(*open);
}
inline void ancestorVisibility(ChimeraProcessor& processor) {
    juce::Component host,workspace;
    auto measured=std::make_unique<spectralforge::DynamicsSpectrumPanel>(processor.dynamicsAnalysis(),processor.eqCaptureBoundary());
    host.setBounds(0,0,900,480);workspace.setBounds(host.getLocalBounds());measured->setBounds(workspace.getLocalBounds());
    host.addChildComponent(workspace);workspace.addAndMakeVisible(*measured);measured->setStage(Stage::compressor);
    host.addToDesktop(juce::ComponentPeer::windowIsTemporary);host.setVisible(true);settle(65);
    require(measured->isVisible() && !measured->isShowing() && processor.dynamicsAnalysis().uiConsumer==nullptr,"Hidden ancestor started dynamics capture");
    for(int visit=0;visit<2;++visit) {
        // Only the ancestor changes visibility: JUCE does not send a child's
        // visibilityChanged callback. No page/source setter may mask this bug.
        workspace.setVisible(true);
        waitForNativeSpectrum([&]{return processor.dynamicsAnalysis().uiConsumer==measured.get();},
                              "Showing only an ancestor did not acquire the dynamics stream");
        require(processor.dynamicsAnalysis().uiConsumer==measured.get() && measured->isWaitingForFreshCapture(),"Showing only an ancestor did not acquire the dynamics stream");
        acknowledge(processor);audio(processor);
        if(!measured->analyzerHasData() || measured->spectrumDB(false,80)<=-35.f)
            std::cerr<<"Ancestor visit="<<visit<<" data="<<measured->analyzerHasData()<<" frames="<<measured->fftFrames()
                     <<" rate="<<measured->analysisSampleRate()<<" in_db="<<measured->spectrumDB(false,80)
                     <<" drain="<<measured->isWaitingForFreshCapture()<<" enabled="<<processor.dynamicsAnalysis().fifo.enabled.load()
                     <<" pending="<<processor.dynamicsAnalysis().fifo.pendingSamples()<<'\n';
        require(measured->analyzerHasData() && measured->spectrumDB(false,80)>-35.f,"Ancestor-opened spectrum did not receive production audio");
        workspace.setVisible(false);
        waitForNativeSpectrum([&]{return processor.dynamicsAnalysis().uiConsumer==nullptr;},
                              "Hiding an ancestor did not release the dynamics stream");
        const auto frames=measured->fftFrames();audio(processor);
        require(processor.dynamicsAnalysis().uiConsumer==nullptr && !processor.dynamicsAnalysis().fifo.enabled.load() && measured->fftFrames()==frames,"Hiding an ancestor retained capture or FFT work");
    }
    measured.reset();host.removeFromDesktop();
    std::cout<<"PASS ancestor-only visibility starts, suspends and reacquires real stage audio\n";
}
inline void pageContract(ChimeraEditor& editor,spectralforge::CommonDynamicsPanel& panel,Page page) {
    auto* measured=find<spectralforge::DynamicsSpectrumPanel>(panel,"commonDynamics.spectrum");
    require(measured && measured->isShowing(),"Selected dynamics page lost its spectrum");
    inside(editor,*measured,"Dynamics spectrum is clipped");require(measured->graphBounds().getHeight()>=150,"Dynamics graph was reduced to a meter strip");
    const std::array<std::pair<Page,const char*>,6> exclusive{{{Page::compressor,"cleanCompThreshold"},{Page::gate,"commonDynamics.gatePre"},{Page::expander,"expThreshold"},
        {Page::limiter,"liveLimiterCeiling"},{Page::multiband,"mbBand1_threshold"},{Page::deesser,"deEsserFrequency"}}};
    for(const auto& [owner,id]:exclusive) {
        auto* control=find(panel,id);
        require(control && control->isShowing()==(owner==page),"Unrelated dynamics controls remain on the selected page");
    }
    switch(page) {
        case Page::compressor:
            for(const auto* id:{"cleanCompOn","cleanCompThreshold","cleanCompRatio","cleanCompAttack","cleanCompRelease","cleanCompKnee","cleanCompMakeup","cleanCompMix","cleanCompHP","cleanCompDetector"})visibleInside(editor,panel,id);break;
        case Page::gate: {
            // PRE RIG and POST RIG each show one complete gate; the released
            // settings sit on the side gateAfterRig puts them, Gate 2 on the other.
            const std::vector<const char*> mainIDs{"commonDynamics.gateon","commonDynamics.gatethreshold","gateMode","gateRatio","gateAttack","commonDynamics.gatehold","commonDynamics.gaterelease","commonDynamics.gateRangeDb"};
            const std::vector<const char*> secondIDs{"gate2On","gate2Mode","gate2Threshold","gate2Ratio","gate2Attack","gate2Hold","gate2Release","gate2RangeDb"};
            const int original=panel.selectedGateSide();int mainSides=0;
            for(int side:{0,1}) {
                panel.selectGateSide(side);require(panel.selectedGateSide()==side,"Gate side selection was not kept");
                for(const auto* id:{"commonDynamics.gatePre","commonDynamics.gatePost","commonDynamics.gateAfterRig"})visibleInside(editor,panel,id);
                const bool main=panel.mainGateShown();mainSides+=main;
                for(const auto* id:main?mainIDs:secondIDs)visibleInside(editor,panel,id);
                for(const auto* id:main?secondIDs:mainIDs){auto* control=find(panel,id);require(control && !control->isShowing(),"The other side's gate controls remain visible");}
            }
            require(mainSides==1,"PRE and POST must show different gates");
            panel.selectGateSide(original);break;
        }
        case Page::expander:
            for(const auto* id:{"expOn","expAfterRig","expThreshold","expRatio","expAttack","expHold","expRelease","expRangeDb"})visibleInside(editor,panel,id);break;
        case Page::limiter:
            for(const auto* id:{"liveLimiterOn","liveLimiterCeiling","liveLimiterRelease"})visibleInside(editor,panel,id);break;
        case Page::multiband:
            for(const auto* id:{"mbOn","mbMix","mbDetector","mbCrossover1","mbCrossover2","mbCrossover3","mbBand1_mode","mbBand1_bypass","mbBand1_solo","mbBand1_threshold","mbBand1_range","mbBand1_ratio","mbBand1_attack","mbBand1_release","mbBand1_knee","mbBand1_makeup"})visibleInside(editor,panel,id);break;
        case Page::deesser:
            for(const auto* id:{"deEsserOn","deEsserListen","deEsserFrequency","deEsserThreshold","deEsserRange","deEsserRatio","deEsserAttack","deEsserRelease","deEsserKnee"})visibleInside(editor,panel,id);break;
        default:break;
    }
}
// A recalled factory sound must toggle only its own compressor switch.
inline void recalledCompressorSwitch() {
    auto processor=std::make_unique<ChimeraProcessor>();auto& p=*processor;p.loadFactoryPreset(0);
    auto editor=std::make_unique<ChimeraEditor>(p);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);
    openWorkspace(*editor);settle(60);
    auto* panel=find<spectralforge::CommonDynamicsPanel>(*editor,"commonDynamics.panel");require(panel && panel->isShowing(),"Dynamics workspace did not open");
    panel->selectPage(Page::compressor);settle(40);
    auto* compressor=find<juce::TextButton>(*panel,"cleanCompOn");
    auto* compressorParameter=p.parameters().getParameter("cleanCompOn");
    require(compressor && compressorParameter,"Compressor switch/parameter missing");
    const auto beforeSwitch=signalPathTests::values(p);
    const bool initialOn=p.parameters().getRawParameterValue("cleanCompOn")->load()>.5f;
    require(compressor->getToggleState()==initialOn,"Compressor switch did not show the recalled preset state");
    for(int toggle=0;toggle<2;++toggle) {
        const bool expectedOn=toggle==0?!initialOn:initialOn;
        auto expected=beforeSwitch;
        expected[size_t(compressorParameter->getParameterIndex())]=expectedOn?1.f:0.f;
        signalPathTests::click(*compressor);settle(40);
        require((p.parameters().getRawParameterValue("cleanCompOn")->load()>.5f)==expectedOn
                    && compressor->getToggleState()==expectedOn && signalPathTests::values(p)==expected,
                "Compressor switch did not toggle only its recalled preset parameter");
    }
    editor.reset();settle(40);
    std::cout<<"PASS recalled factory compressor switch edits only its own parameter\n";
}
// Auto Set entry: shown only where it acts, and starting/cancelling it is
// navigation, not a sound edit.
inline void autoSetEntry(ChimeraProcessor& p,spectralforge::CommonDynamicsPanel& panel) {
    auto* start=find<juce::TextButton>(panel,"commonDynamics.autoSet");auto* undo=find<juce::TextButton>(panel,"commonDynamics.autoUndo");
    auto* tone=find<juce::TextButton>(panel,"commonDynamics.autoOptions");
    require(start && undo && tone,"Auto Set controls missing");
    for(const auto page:{Page::compressor,Page::gate,Page::limiter,Page::multiband,Page::deesser}) {
        panel.selectPage(page);settle(20);
        const bool expected=page==Page::compressor || page==Page::gate;
        require(start->isShowing()==expected && undo->isShowing()==expected && tone->isShowing()==expected,"Auto Set visibility does not match its page");
        if(expected)require(panel.getLocalBounds().contains(start->getBounds()) && panel.getLocalBounds().contains(undo->getBounds()) && panel.getLocalBounds().contains(tone->getBounds())
                            && !start->getBounds().intersects(undo->getBounds()) && !start->getBounds().intersects(tone->getBounds()) && !tone->getBounds().intersects(undo->getBounds()),
                            "Auto Set controls are clipped or overlap");
    }
    panel.selectPage(Page::compressor);settle(20);
    const auto before=signalPathTests::values(p);
    require(!undo->isEnabled(),"Undo is available before any Auto Set");
    const auto defaults=panel.autoSetOptions();
    require(defaults.instrument==0 && defaults.tone && defaults.style==0 && defaults.finish && defaults.multiband && defaults.dynamicEQ && tone->getButtonText()=="Auto / Rock","Auto Set options do not default to detect, rock, all steps");
    // The menu itself is a native popup; drive its item handler directly.
    panel.chooseAutoOption(21);panel.chooseAutoOption(22);panel.chooseAutoOption(23);panel.chooseAutoOption(25);panel.chooseAutoOption(3);panel.chooseAutoOption(13);settle(20);
    require(tone->getButtonText()=="Bass / Metal","Options button does not show the chosen instrument and style");
    signalPathTests::click(*start);settle(30);
    require(start->getButtonText()=="CANCEL" && panel.autoSetStatus().contains("Listening 1/2") && !tone->isEnabled(),("Auto Set without the optional steps did not listen in two passes: "+panel.autoSetStatus()).toRawUTF8());
    signalPathTests::click(*start);settle(30);
    panel.chooseAutoOption(21);panel.chooseAutoOption(22);panel.chooseAutoOption(23);panel.chooseAutoOption(25);panel.chooseAutoOption(1);panel.chooseAutoOption(11);settle(20);
    signalPathTests::click(*start);settle(30);
    require(start->getButtonText()=="CANCEL" && panel.autoSetStatus().contains("Listening 1/5") && tone->getButtonText()=="Auto / Rock","Auto Set did not enter listening with all five passes");
    {
        // The progress view names the step, what it listens for and the time it needs.
        auto* view=find<spectralforge::AutoSetView>(panel,"autoSet.view");
        require(view && view->isShowing() && view->report().listening && view->report().steps.size()==5 && view->report().current==0
                && view->report().listeningFor.isNotEmpty() && view->report().totalSeconds>30,"Auto Set progress view missing while listening");
        require(panel.getLocalBounds().contains(view->getBounds()),"Auto Set progress view is clipped");
    }
    signalPathTests::click(*start);settle(30);
    require(start->getButtonText()=="AUTO SET" && panel.autoSetStatus().contains("cancelled") && !undo->isEnabled() && tone->isEnabled(),"Auto Set did not cancel");
    require(!find(panel,"autoSet.view")->isShowing(),"Cancelled Auto Set left its progress view open");
    require(signalPathTests::values(p)==before,"Starting or cancelling Auto Set changed sound parameters");
    std::cout<<"PASS auto set entry: compressor/gate pages only, unclipped, options (instrument/style/steps; defaults; optional steps off = two passes), listen/cancel without parameter writes\n";
}
inline void measuredStageContracts(const juce::File& directory);
// Dynamic EQ lives on its band: DYNAMIC claims one of four shared slots, its
// controls attach to that slot, and turning it off frees the slot again.
inline void dynamicEQOnBands(ChimeraProcessor& p,juce::Component& editor,spectralforge::GraphicalEQPanel& tone,const juce::File& directory) {
    const auto target=[&](int slot){return juce::roundToInt(p.parameters().getRawParameterValue(spectralforge::dynamicSlotID(slot,"target"))->load());};
    auto* dynamic=find<juce::TextButton>(tone,spectralforge::eqID(0,-1,"dynamic"));
    auto* threshold=find<juce::Slider>(tone,spectralforge::eqID(0,-1,"dynamic_threshold"));
    require(dynamic && threshold,"Tone EQ band has no Dynamic EQ controls");
    for(const auto* field:{"dynamic","dynamic_threshold","dynamic_range","dynamic_ratio","dynamic_attack","dynamic_release"})visibleInside(editor,tone,spectralforge::eqID(0,-1,field));
    visibleInside(editor,tone,spectralforge::eqID(0,0,"frequency"));
    require(dynamic->getToggleState() && threshold->isEnabled(),"Band 1 does not show its assigned dynamic slot");
    threshold->setValue(-36.,juce::sendNotificationSync);settle(30);
    require(std::abs(p.parameters().getRawParameterValue("dynamicEQ1_threshold")->load()+36.f)<.01f,"Band dynamics controls are not attached to its slot");
    tone.selectBandForEditing(1);settle(30);
    threshold->setValue(-30.,juce::sendNotificationSync);settle(30);
    require(std::abs(p.parameters().getRawParameterValue("dynamicEQ2_threshold")->load()+30.f)<.01f && std::abs(p.parameters().getRawParameterValue("dynamicEQ1_threshold")->load()+36.f)<.01f,
            "Selecting another band did not move the controls to its own slot");
    // All four slots are in use: free one, then a band without one can claim it.
    signalPathTests::click(*dynamic);settle(30);
    require(target(1)==0 && !dynamic->getToggleState() && !threshold->isEnabled(),"DYNAMIC off did not free the band's slot");
    tone.selectBandForEditing(0);settle(30);require(dynamic->getToggleState() && target(0)==1,"Turning one band off changed another band");
    tone.selectBandForEditing(1);settle(30);signalPathTests::click(*dynamic);settle(30);
    require(target(1)==2 && dynamic->getToggleState() && threshold->isEnabled(),"DYNAMIC on did not claim a free slot");
    signalPathTests::capture(editor,directory,"tone-eq-dynamic-band");
    tone.selectBandForEditing(0);settle(30);
}
inline void run(const juce::File& directory) {
    auto processor=std::make_unique<ChimeraProcessor>();auto& p=*processor;
    for(const auto* id:{"gateon","boardEnabled","precompon","filteron","fuzzon","booston","preon","ampon1","cab1","transposeon","tuneron","cleanCompOn","liveLimiterOn","mbOn","deEsserOn"})set(p,id,0);
    set(p,"mode",0);set(p,"input",0);set(p,"output",0);
    for(int band=0;band<4;++band) {
        set(p,spectralforge::eqID(0,band,"enabled"),1);set(p,spectralforge::eqID(0,band,"type"),0);
        set(p,spectralforge::eqID(0,band,"gain"),0);set(p,spectralforge::eqID(0,band,"frequency"),float(180+band*900));
        set(p,spectralforge::dynamicSlotID(band,"target"),float(band+1));
    }
    set(p,spectralforge::eqID(1,0,"enabled"),1);set(p,spectralforge::eqID(1,0,"type"),0);
    p.prepareToPlay(48000,256);
    ancestorVisibility(p);
    auto editor=std::make_unique<ChimeraEditor>(p);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);
    signalPathTests::Writes writes;p.addListener(&writes);const auto before=signalPathTests::values(p);
    openWorkspace(*editor);
    auto* panel=find<spectralforge::CommonDynamicsPanel>(*editor,"commonDynamics.panel");require(panel && panel->isShowing(),"Dynamics workspace did not open");
    auto* measured=find<spectralforge::DynamicsSpectrumPanel>(*panel,"commonDynamics.spectrum");require(measured,"Dynamics analyzer missing");
    require(signalPathTests::values(p)==before && writes.changes==0 && writes.gestures==0,"Opening dynamics changed sound state or emitted gestures");

    // Six dynamics workspaces, in tab order. Dynamic EQ is edited on its EQ band.
    const std::array<std::pair<const char*,Page>,6> pages{{{"compressor",Page::compressor},{"gate",Page::gate},{"expander",Page::expander},
        {"limiter",Page::limiter},{"multiband",Page::multiband},{"deesser",Page::deesser}}};
    for(int scale:{75,100,125,150}) {
        editor->setSize(1180*scale/100,780*scale/100);settle(35);
        for(size_t index=0;index<pages.size();++index) {
            const auto [name,page]=pages[index];
            auto* entry=find<juce::TextButton>(*editor,"commonDynamics.open."+juce::String(name));
            require(entry && entry->isShowing() && entry->isEnabled(),"Direct dynamics entry missing");
            inside(*editor,*entry,"Direct dynamics entry is clipped");
            signalPathTests::click(*entry);
            require(panel->selectedPage()==page && entry->getToggleState(),"Direct entry selected the wrong dynamics page");
            pageContract(*editor,*panel,page);
            // The inner tabs remain usable and keep the header selection in sync.
            auto* button=find<juce::TextButton>(*panel,"commonDynamics.page."+juce::String(pages[(index+1)%pages.size()].first));
            require(button && button->isShowing(),"Dedicated dynamics tab missing");signalPathTests::click(*button);
            require(!entry->getToggleState(),"Direct entry stayed selected after switching the inner tab");
            signalPathTests::click(*entry);pageContract(*editor,*panel,page);
            acknowledge(p);audio(p);
            if(!measured->analyzerHasData())std::cerr<<"Dynamics page="<<name<<" scale="<<scale<<" showing="<<measured->isShowing()<<" owner="<<(p.dynamicsAnalysis().uiConsumer==measured)<<" selected="<<p.dynamicsAnalysis().selected.load()<<" pending_drain="<<measured->isWaitingForFreshCapture()<<" enabled="<<p.dynamicsAnalysis().fifo.enabled.load()<<" pending_samples="<<p.dynamicsAnalysis().fifo.pendingSamples()<<'\n';
            require(measured->analyzerHasData(),"Dynamics spectrum has no production audio");
            signalPathTests::capture(*editor,directory,"common-dynamics-"+juce::String(name)+"-"+juce::String(scale));
        }
        require(find(*editor,"commonDynamics.open.dynamicEQ")==nullptr && find(*panel,"commonDynamics.page.dynamicEQ")==nullptr,"Dynamic EQ must not remain a separate dynamics workspace");
    }
    require(signalPathTests::values(p)==before && writes.changes==0 && writes.gestures==0,"Dynamics page/scale navigation edited parameters or gestures");
    editor->setSize(1180,780);panel->selectPage(Page::compressor);settle(40);
    const bool wasOn=p.parameters().getRawParameterValue("cleanCompOn")->load()>.5f;
    signalPathTests::click(*find<juce::TextButton>(*panel,"cleanCompOn"));
    require((p.parameters().getRawParameterValue("cleanCompOn")->load()>.5f)!=wasOn,"Compressor switch is not attached");
    set(p,"cleanCompOn",1);set(p,"cleanCompThreshold",-32);set(p,"cleanCompRatio",4);set(p,"cleanCompAttack",1);
    set(p,"cleanCompKnee",0);set(p,"cleanCompMakeup",0);set(p,"cleanCompMix",1);set(p,"cleanCompDetector",0);
    // The panel's own timer retires the frame when the compressor route turns
    // on. A stalled macOS loop can deliver that after the first burst, so
    // measure again (bounded); the assertion itself is unchanged.
    for(int attempt=0;attempt<3;++attempt) {
        acknowledge(p);audio(p);
        if(measured->analyzerHasData() && measured->spectrumDB(false,80)>-35.f)break;
        std::cerr<<"Compressor spectrum attempt="<<attempt<<" data="<<measured->analyzerHasData()<<" in_db="<<measured->spectrumDB(false,80)
                 <<" drain="<<measured->isWaitingForFreshCapture()<<" enabled="<<p.dynamicsAnalysis().fifo.enabled.load()<<'\n';
    }
    require(measured->analyzerHasData() && measured->spectrumDB(false,80)>-35.f,"Actual anti-phase stereo input disappeared from the spectrum");
    require(measured->spectrumDB(false,80)-measured->spectrumDB(true,80)>4.f,"Compressor OUT does not show the measured attenuation");
    const auto frozenInput=measured->spectrumDB(false,80),frozenOutput=measured->spectrumDB(true,80);
    const auto freezeValues=signalPathTests::values(p);const int freezeChanges=writes.changes,freezeGestures=writes.gestures;
    measured->setSnapshotFrozen(true);const auto frozenFrames=measured->fftFrames();audio(p,3000.);
    require(measured->isSnapshotFrozen() && measured->fftFrames()==frozenFrames && measured->spectrumDB(false,80)==frozenInput && measured->spectrumDB(true,80)==frozenOutput,"Freeze did not retain one complete actual IN/OUT frame");
    require(signalPathTests::values(p)==freezeValues && writes.changes==freezeChanges && writes.gestures==freezeGestures,"Freeze changed audio parameters");

    // Reject an old callback acknowledgement and samples arriving after the
    // first UI flush. Only a later real processBlock boundary may reopen input.
    auto oldCallback=std::make_unique<spectralforge::EQCaptureBoundary::CallbackScope>(p.eqCaptureBoundary());
    panel->selectPage(Page::gate);
    require(!measured->isSnapshotFrozen() && !measured->analyzerHasData(),"Stage change relabelled a frozen compressor spectrum");
    for(int sample=0;sample<8192;++sample)p.dynamicsAnalysis().fifo.push({.5f,.5f,-.5f,-.5f});
    oldCallback.reset();settle(65);
    require(measured->isWaitingForFreshCapture() && !p.dynamicsAnalysis().fifo.enabled.load() && !measured->analyzerHasData(),"New stage accepted a late old-source callback");
    acknowledge(p);
    require(!measured->isWaitingForFreshCapture() && p.dynamicsAnalysis().fifo.pendingSamples()==0 && !measured->analyzerHasData(),"Acknowledged source transition retained old audio");
    audio(p,3000.);require(measured->analyzerHasData() && measured->spectrumDB(false,256)>-35,"Gate did not acquire its own new input");
    // A Dynamic EQ request opens the EQ holding the first dynamic band; the
    // hidden dynamics spectrum stops consuming.
    const auto gateFrames=measured->fftFrames();panel->selectPage(Page::dynamicEQ);settle(50);acknowledge(p);audio(p);
    require(measured->fftFrames()==gateFrames && !p.dynamicsAnalysis().fifo.enabled.load(),"Hidden dynamics spectrum kept consuming/performing FFTs");
    auto* toneEQ=find<spectralforge::GraphicalEQPanel>(*editor,"toneEQ_panel");
    require(toneEQ && toneEQ->isShowing() && !panel->isShowing() && toneEQ->selectedBand()==0,"Dynamic EQ request did not open Tone EQ at its dynamic band");
    dynamicEQOnBands(p,*editor,*toneEQ,directory);
    openWorkspace(*editor);

    panel->selectPage(Page::multiband);auto* band=find<juce::ComboBox>(*panel,"commonDynamics.multibandBand");require(band,"Multiband selector missing");
    const auto bandBefore=signalPathTests::values(p);
    for(int index=1;index<=4;++index) {
        band->setSelectedId(index,juce::sendNotificationSync);settle(30);
        visibleInside(*editor,*panel,"mbBand"+juce::String(index)+"_threshold");
        for(int other=1;other<=4;++other)if(other!=index)require(!find(*panel,"mbBand"+juce::String(other)+"_threshold")->isShowing(),"Multiband combines all four bands' controls");
    }
    require(signalPathTests::values(p)==bandBefore,"Multiband selection changed the sound");

    // Native PRE/POST use the real current model's controls, not the inactive
    // legacy sliders. Check the root callbacks and the single-consumer lease.
    for(int owner=1;owner<spectralforge::pedalBoardCapacity;++owner)p.setPedalModel(owner,0);
    p.setPedalModel(0,7);p.setPedalBoardEnabled(true);settle(50);
    const auto preBefore=signalPathTests::values(p);panel->selectPage(Page::preCompressor);settle(80);
    auto* boardSpectrum=find<spectralforge::DynamicsSpectrumPanel>(*editor,"boardCompressorSpectrum");
    require(boardSpectrum && boardSpectrum->isShowing() && !panel->isShowing(),"Native PRE selection did not open its compressor spectrum");
    require(p.dynamicsAnalysis().preOwner.load()==0,"Native PRE spectrum selected the wrong stable owner");
    acknowledge(p);audio(p);require(boardSpectrum->analyzerHasData() && p.dynamicsAnalysis().uiConsumer==boardSpectrum,"Native PRE has no measured owner-specific audio");
    require(signalPathTests::values(p)==preBefore,"Opening native PRE changed parameters");
    signalPathTests::capture(*editor,directory,"common-dynamics-native-pre");
    panel->selectPage(Page::compressor);openWorkspace(*editor);
    set(p,spectralforge::postNativeModeID(0),1);settle(30);const auto postBefore=signalPathTests::values(p);const auto priorPage=panel->selectedPage();
    panel->selectPage(Page::postCompressor);settle(100);
    auto* window=dialog("postNativePanel0");require(window && window->getContentComponent(),"Native POST spectrum dialog did not open");
    auto* postSpectrum=find<spectralforge::DynamicsSpectrumPanel>(*window,"postCompressorSpectrum");
    require(panel->selectedPage()==priorPage,"Native POST request replaced the underlying workspace with inactive legacy controls");
    require(postSpectrum && postSpectrum->isShowing(),"Native POST dialog lost its spectrum");acknowledge(p);audio(p);
    require(postSpectrum->analyzerHasData() && p.dynamicsAnalysis().uiConsumer==postSpectrum,"Native POST did not acquire the shared analysis stream");
    measured->setPostNativeSource(0);measured->sourceChanged();
    require(p.dynamicsAnalysis().uiConsumer==postSpectrum && p.dynamicsAnalysis().postNativeSource.load()==1 && postSpectrum->analyzerHasData(),"Suspended workspace changed the native dialog's source or analyzer lease");
    require(signalPathTests::values(p)==postBefore,"Opening native POST changed parameters");
    signalPathTests::capture(*window,directory,"common-dynamics-native-post");
    close(juce::Component::SafePointer<juce::DialogWindow>(window));acknowledge(p);audio(p);
    require(panel->selectedPage()==priorPage && p.dynamicsAnalysis().uiConsumer==measured && measured->analyzerHasData(),"Closing native dialog did not restore the previous visible page and analyzer");

    // Preset/route changes may deactivate an already-open legacy editor. Its
    // old frozen picture must disappear, and native audio must not enter it.
    p.setPedalBoardEnabled(false);panel->selectPage(Page::preCompressor);settle(60);acknowledge(p);audio(p);
    require(measured->analyzerHasData() && p.dynamicsAnalysis().preOwner.load()==-1,"Legacy PRE did not acquire its own source identity");
    measured->setSnapshotFrozen(true);p.setPedalBoardEnabled(true);
    waitForNativeSpectrum([&]{return !measured->isSnapshotFrozen() && !measured->analyzerHasData();},
                          "Legacy PRE source change did not retire its frozen frame");
    require(!measured->isSnapshotFrozen() && !measured->analyzerHasData(),"Inactive legacy PRE retained its frozen frame");
    acknowledge(p);audio(p);require(!measured->analyzerHasData(),"Legacy PRE editor accepted the active native pedalboard's audio");
    set(p,spectralforge::postNativeModeID(0),0);panel->selectPage(Page::postCompressor);settle(60);acknowledge(p);audio(p);
    require(measured->analyzerHasData() && p.dynamicsAnalysis().postNativeSource.load()==0,"Legacy POST did not acquire its own source identity");
    measured->setSnapshotFrozen(true);set(p,spectralforge::postNativeModeID(0),1);
    waitForNativeSpectrum([&]{return !measured->isSnapshotFrozen() && !measured->analyzerHasData();},
                          "Legacy POST source change did not retire its frozen frame");
    require(!measured->isSnapshotFrozen() && !measured->analyzerHasData(),"Inactive legacy POST retained its frozen frame");
    acknowledge(p);audio(p);require(!measured->analyzerHasData(),"Legacy POST editor accepted the active native POST processor's audio");

    autoSetEntry(p,*panel);
    panel->selectPage(Page::compressor);signalPathTests::click(*find<juce::TextButton>(*editor,"tunerOpen"));settle(50);
    require(!panel->isShowing(),"Dynamics remains active under the tuner workspace");
    const auto hiddenFrames=measured->fftFrames();audio(p);require(measured->fftFrames()==hiddenFrames && !p.dynamicsAnalysis().fifo.enabled.load(),"Hidden dynamics analyzer continues processing");
    signalPathTests::click(*find<juce::TextButton>(*editor,"path.tone.eq"));settle(50);
    auto* tone=find<spectralforge::GraphicalEQPanel>(*editor,"toneEQ_panel");
    require(tone && tone->isShowing() && !panel->isShowing(),"Tone EQ navigation did not leave the dynamics workspace");
    p.removeListener(&writes);editor.reset();p.releaseResources();
    measuredStageContracts(directory);
    recalledCompressorSwitch();
    std::cout<<"PASS six dedicated spectrum workspaces (gate PRE/POST, expander), 24 scaled layouts, actual stereo IN/OUT and GR, freeze/source drain, hidden FFT suspension, Dynamic EQ on Tone/Final EQ bands with four shared slots, four multiband selections, native/legacy PRE/POST source isolation and analyzer ownership\n";
}
}

#include "DynamicsStageSpectrumUITests.h"
