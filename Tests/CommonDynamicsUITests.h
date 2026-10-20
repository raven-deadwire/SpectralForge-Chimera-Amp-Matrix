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
    processor.processBlock(silence,midi);settle(65);
}
inline void audio(ChimeraProcessor& processor,double frequency=937.5,int blocks=64) {
    // Process the production route. Opposite-polarity R must not cancel L in
    // the power spectrum. The finite burst remains below the FIFO capacity.
    juce::AudioBuffer<float> block(2,256);juce::MidiBuffer midi;
    for(int b=0;b<blocks;++b) {
        for(int n=0;n<256;++n) {
            const float sample=float(.25*std::sin(juce::MathConstants<double>::twoPi*frequency*double(b*256+n)/48000.));
            block.setSample(0,n,sample);block.setSample(1,n,-sample);
        }
        processor.processBlock(block,midi);
    }
    settle(65);
}
inline void openWorkspace(juce::Component& editor) {
    auto* tone=find<juce::TextButton>(editor,"path.tone.eq");require(tone,"Tone EQ navigation missing");signalPathTests::click(*tone);
    auto* open=find<juce::TextButton>(editor,"commonDynamics.open");require(open && open->isShowing(),"Dynamics entry missing");signalPathTests::click(*open);
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
        workspace.setVisible(true);settle(65);
        require(processor.dynamicsAnalysis().uiConsumer==measured.get() && measured->isWaitingForFreshCapture(),"Showing only an ancestor did not acquire the dynamics stream");
        acknowledge(processor);audio(processor);
        require(measured->analyzerHasData() && measured->spectrumDB(false,80)>-35.f,"Ancestor-opened spectrum did not receive production audio");
        workspace.setVisible(false);settle(65);const auto frames=measured->fftFrames();audio(processor);
        require(processor.dynamicsAnalysis().uiConsumer==nullptr && !processor.dynamicsAnalysis().fifo.enabled.load() && measured->fftFrames()==frames,"Hiding an ancestor retained capture or FFT work");
    }
    measured.reset();host.removeFromDesktop();
    std::cout<<"PASS ancestor-only visibility starts, suspends and reacquires real stage audio\n";
}
inline void pageContract(ChimeraEditor& editor,spectralforge::CommonDynamicsPanel& panel,Page page) {
    const bool dynamic=page==Page::dynamicEQ;
    auto* measured=find<spectralforge::DynamicsSpectrumPanel>(panel,"commonDynamics.spectrum");
    require(measured && measured->isShowing()!=dynamic,"Selected dynamics page lost its spectrum or left a second consumer visible");
    if(!dynamic){inside(editor,*measured,"Dynamics spectrum is clipped");require(measured->graphBounds().getHeight()>=150,"Dynamics graph was reduced to a meter strip");}
    constexpr std::array<const char*,6> exclusive{"cleanCompThreshold","commonDynamics.gatethreshold","liveLimiterCeiling","dynamicEQ1_threshold","mbBand1_threshold","deEsserFrequency"};
    for(int index=0;index<6;++index) {
        auto* control=find(panel,exclusive[size_t(index)]);
        require(control && control->isShowing()==(index==int(page)),"Unrelated dynamics controls remain on the selected page");
    }
    switch(page) {
        case Page::compressor:
            for(const auto* id:{"cleanCompOn","cleanCompThreshold","cleanCompRatio","cleanCompAttack","cleanCompRelease","cleanCompKnee","cleanCompMakeup","cleanCompMix","cleanCompHP","cleanCompDetector"})visibleInside(editor,panel,id);break;
        case Page::gate:
            for(const auto* id:{"commonDynamics.gateon","commonDynamics.gatethreshold","gateMode","gateRatio","gateAttack","commonDynamics.gatehold","commonDynamics.gaterelease","commonDynamics.gateRangeDb","commonDynamics.gateAfterRig"})visibleInside(editor,panel,id);break;
        case Page::limiter:
            for(const auto* id:{"liveLimiterOn","liveLimiterCeiling","liveLimiterRelease"})visibleInside(editor,panel,id);break;
        case Page::dynamicEQ: {
            auto* eq=find<spectralforge::GraphicalEQPanel>(panel,"toneEQ_panel");
            require(eq && eq->isShowing() && panel.isUsingEQEditor(*eq),"Dynamic EQ did not borrow the actual Tone EQ editor");
            require(eq->selectedBand()==0,"Dynamic slot does not select its assigned EQ node");
            for(const auto* field:{"frequency","gain","q"})visibleInside(editor,panel,spectralforge::eqID(0,0,field));
            for(const auto* field:{"target","threshold","range","attack","release","ratio"})visibleInside(editor,panel,spectralforge::dynamicSlotID(0,field));
            require(!find(editor,"finalEQ_panel")->isShowing(),"Two EQ banks consume/display the dynamic workspace simultaneously");break;
        }
        case Page::multiband:
            for(const auto* id:{"mbOn","mbMix","mbDetector","mbCrossover1","mbCrossover2","mbCrossover3","mbBand1_mode","mbBand1_bypass","mbBand1_solo","mbBand1_threshold","mbBand1_range","mbBand1_ratio","mbBand1_attack","mbBand1_release","mbBand1_knee","mbBand1_makeup"})visibleInside(editor,panel,id);break;
        case Page::deesser:
            for(const auto* id:{"deEsserOn","deEsserListen","deEsserFrequency","deEsserThreshold","deEsserRange","deEsserRatio","deEsserAttack","deEsserRelease","deEsserKnee"})visibleInside(editor,panel,id);break;
        default:break;
    }
}
inline void measuredStageContracts(const juce::File& directory);
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

    constexpr std::array<const char*,6> pages{"compressor","gate","limiter","dynamicEQ","multiband","deesser"};
    for(int scale:{75,100,125,150}) {
        editor->setSize(1180*scale/100,780*scale/100);settle(35);
        for(int page=0;page<6;++page) {
            auto* button=find<juce::TextButton>(*panel,"commonDynamics.page."+juce::String(pages[size_t(page)]));require(button && button->isShowing(),"Dedicated dynamics tab missing");
            signalPathTests::click(*button);pageContract(*editor,*panel,Page(page));
            acknowledge(p);audio(p);
            if(page==int(Page::dynamicEQ))require(find<spectralforge::GraphicalEQPanel>(*panel,"toneEQ_panel")->analyzerHasData(),"Dynamic EQ spectrum has no production audio");
            else {
                if(!measured->analyzerHasData())std::cerr<<"Dynamics page="<<pages[size_t(page)]<<" scale="<<scale<<" showing="<<measured->isShowing()<<" owner="<<(p.dynamicsAnalysis().uiConsumer==measured)<<" selected="<<p.dynamicsAnalysis().selected.load()<<" pending_drain="<<measured->isWaitingForFreshCapture()<<" enabled="<<p.dynamicsAnalysis().fifo.enabled.load()<<" pending_samples="<<p.dynamicsAnalysis().fifo.pendingSamples()<<'\n';
                require(measured->analyzerHasData(),"Dynamics spectrum has no production audio");
            }
            signalPathTests::capture(*editor,directory,"common-dynamics-"+juce::String(pages[size_t(page)])+"-"+juce::String(scale));
        }
    }
    require(signalPathTests::values(p)==before && writes.changes==0 && writes.gestures==0,"Dynamics page/scale navigation edited parameters or gestures");
    editor->setSize(1180,780);panel->selectPage(Page::compressor);settle(40);
    const bool wasOn=p.parameters().getRawParameterValue("cleanCompOn")->load()>.5f;
    signalPathTests::click(*find<juce::TextButton>(*panel,"cleanCompOn"));
    require((p.parameters().getRawParameterValue("cleanCompOn")->load()>.5f)!=wasOn,"Compressor switch is not attached");
    set(p,"cleanCompOn",1);set(p,"cleanCompThreshold",-32);set(p,"cleanCompRatio",4);set(p,"cleanCompAttack",1);
    set(p,"cleanCompKnee",0);set(p,"cleanCompMakeup",0);set(p,"cleanCompMix",1);set(p,"cleanCompDetector",0);
    acknowledge(p);audio(p);
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
    const auto gateFrames=measured->fftFrames();panel->selectPage(Page::dynamicEQ);acknowledge(p);audio(p);
    require(measured->fftFrames()==gateFrames && !p.dynamicsAnalysis().fifo.enabled.load(),"Hidden dynamics spectrum kept consuming/performing FFTs");

    // The selected slot exposes the target's frequency/gain/Q alongside all
    // five dynamic controls. Changing the editor-local slot never writes sound.
    const auto slotsBefore=signalPathTests::values(p);const int slotChanges=writes.changes,slotGestures=writes.gestures;
    for(int slot=0;slot<4;++slot) {
        panel->selectDynamicSlot(slot);settle(35);
        for(const auto* field:{"frequency","gain","q"})visibleInside(*editor,*panel,spectralforge::eqID(0,slot,field));
        for(const auto* field:{"target","threshold","range","attack","release","ratio"})visibleInside(*editor,*panel,spectralforge::dynamicSlotID(slot,field));
        for(int other=0;other<4;++other)if(other!=slot)require(!find(*panel,spectralforge::dynamicSlotID(other,"threshold"))->isShowing(),"Four dynamic control rows remain combined");
    }
    require(signalPathTests::values(p)==slotsBefore && writes.changes==slotChanges && writes.gestures==slotGestures,"Dynamic slot navigation edited sound");
    panel->selectDynamicSlot(0);
    auto* frequency=find<juce::Slider>(*panel,spectralforge::eqID(0,0,"frequency"));require(frequency,"Assigned node frequency is not editable");
    frequency->setValue(1420.,juce::sendNotificationSync);settle(35);
    require(std::abs(p.parameters().getRawParameterValue(spectralforge::eqID(0,0,"frequency"))->load()-1420.f)<.1f,"Dynamic workspace frequency control edits the wrong EQ node");
    auto* threshold=find<juce::Slider>(*panel,"dynamicEQ1_threshold");threshold->setValue(-36.,juce::sendNotificationSync);settle(30);
    require(std::abs(p.parameters().getRawParameterValue("dynamicEQ1_threshold")->load()+36.f)<.01f,"Visible slot dynamics controls are not attached");
    panel->selectDynamicSlot(1);auto* second=find<juce::ComboBox>(*panel,"dynamicEQ2_target");second->setSelectedId(2,juce::sendNotificationSync);settle(35);
    require(p.parameters().getRawParameterValue("dynamicEQ1_target")->load()==1 && p.parameters().getRawParameterValue("dynamicEQ2_target")->load()==2,"Duplicate target displaced an existing slot");
    panel->selectDynamicSlot(3);auto* fourth=find<juce::ComboBox>(*panel,"dynamicEQ4_target");fourth->setSelectedId(14,juce::sendNotificationSync);settle(50);
    require(panel->selectedDynamicBank()==1 && find<spectralforge::GraphicalEQPanel>(*panel,"finalEQ_panel")->isShowing(),"Final target did not move the same dynamic editor to Final EQ");
    visibleInside(*editor,*panel,spectralforge::eqID(1,0,"frequency"));visibleInside(*editor,*panel,"dynamicEQ4_threshold");
    signalPathTests::capture(*editor,directory,"common-dynamics-final-slot");

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
    measured->setSnapshotFrozen(true);p.setPedalBoardEnabled(true);settle(70);
    require(!measured->isSnapshotFrozen() && !measured->analyzerHasData(),"Inactive legacy PRE retained its frozen frame");
    acknowledge(p);audio(p);require(!measured->analyzerHasData(),"Legacy PRE editor accepted the active native pedalboard's audio");
    set(p,spectralforge::postNativeModeID(0),0);panel->selectPage(Page::postCompressor);settle(60);acknowledge(p);audio(p);
    require(measured->analyzerHasData() && p.dynamicsAnalysis().postNativeSource.load()==0,"Legacy POST did not acquire its own source identity");
    measured->setSnapshotFrozen(true);set(p,spectralforge::postNativeModeID(0),1);settle(70);
    require(!measured->isSnapshotFrozen() && !measured->analyzerHasData(),"Inactive legacy POST retained its frozen frame");
    acknowledge(p);audio(p);require(!measured->analyzerHasData(),"Legacy POST editor accepted the active native POST processor's audio");

    panel->selectPage(Page::compressor);signalPathTests::click(*find<juce::TextButton>(*editor,"tunerOpen"));settle(50);
    require(!panel->isShowing(),"Dynamics remains active under the tuner workspace");
    const auto hiddenFrames=measured->fftFrames();audio(p);require(measured->fftFrames()==hiddenFrames && !p.dynamicsAnalysis().fifo.enabled.load(),"Hidden dynamics analyzer continues processing");
    signalPathTests::click(*find<juce::TextButton>(*editor,"path.tone.eq"));settle(50);
    auto* tone=find<spectralforge::GraphicalEQPanel>(*editor,"toneEQ_panel");
    require(tone && tone->isShowing() && !panel->isShowing() && !panel->isUsingEQEditor(*tone),"Borrowed Tone EQ editor was not restored to normal navigation");
    p.removeListener(&writes);editor.reset();p.releaseResources();
    measuredStageContracts(directory);
    std::cout<<"PASS six dedicated spectrum workspaces, 24 scaled layouts, actual stereo IN/OUT and GR, freeze/source drain, hidden FFT suspension, shared EQ target editing, four dynamic slots, four multiband selections, native/legacy PRE/POST source isolation and analyzer ownership\n";
}
}

#include "DynamicsStageSpectrumUITests.h"
