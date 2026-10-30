#include "GuitarSignaturePresets.h"
#include "FactoryNativeVoicing.h"
#include "PluginProcessor.h"
#include "CabIRFile.h"
#include "IRStateAssets.h"
#include "SynthSampleIO.h"
#include <algorithm>
#include "PluginEditor.h"
#include "FactoryPresets.h"
#include "ReleaseInfo.h"
#include "BetaLifecycleDisplay.h"
#include "StandaloneDiagnostics.h"
#include "../Shared/ChimeraCapturePolicy.h"

namespace {
std::pair<int,int> nativeSelectionFromLegacy(int model,int channel) noexcept {
    const auto& panel=spectralforge::ampNativePanel(model);
    // The first appended release represented Model T input routing as channels,
    // and offered only the SLO overdrive circuit. Keep those meanings on recall.
    if(model==19)return {0,juce::jlimit(0,(int)panel.routes.size()-1,channel)};
    if(model==21)return {1,0};
    return {model>=spectralforge::legacyAmpModelCount?juce::jlimit(0,(int)panel.channels.size()-1,channel):panel.defaultChannel,0};
}
}

void ChimeraProcessor::setIHostApplication(Steinberg::FUnknown* host)
{
    // Queried during VST3 controller connection, before the OBS bridge can
    // prepare/process. Never infer capture state from a host name or playhead.
    void* policy=nullptr;
    if(host && host->queryInterface(spectralforge::chimeraCapturePolicyId,&policy)==Steinberg::kResultOk && policy) {
        auto* capture=static_cast<spectralforge::IChimeraCapturePolicy*>(policy);
        levelMatch.lockCaptureSafe();
        capture->acknowledgeCapturePolicy();
        capture->release();
    }
}

ChimeraProcessor::ChimeraProcessor()
    : ChimeraProcessor(spectralforge::beta::compiledMetadata(), spectralforge::beta::utcNow,
                       spectralforge::beta::tester::Store::load()) {}

ChimeraProcessor::ChimeraProcessor(const spectralforge::beta::Metadata& metadata, spectralforge::beta::Clock clock,
                                   const spectralforge::beta::tester::Grant& tester)
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                      .withOutput("Output",juce::AudioChannelSet::stereo(),true)
                                      // Optional, off until the host enables them (docs/SIDECHAIN_SYNTH_BUS.md).
                                      .withInput("Sidechain",juce::AudioChannelSet::stereo(),false)
                                      .withOutput("Synth",juce::AudioChannelSet::stereo(),false)),
      betaClock(clock), betaLifecycle(metadata,clock(),tester.valid), testerHolder(tester.valid ? tester.name : juce::String{})
{
    originalCabParameters.bind(state);
    toneEQ.bind(state,0);finalEQ.bind(state,1);
    dynamics.bind(state);multiband.bind(state);saturation.bind(state);filterModulation.bind(state);creative.bind(state);synth.bind(state);postFX.setCreative(&creative);deesser.bind(state);
    preFX.setAnalysis(&dynamicsAnalysisState);postFX.setAnalysis(&dynamicsAnalysisState);
    pedalBoard.setAnalysis(&dynamicsAnalysisState);engine.setDynamicsAnalysis(&dynamicsAnalysisState);
    const std::array<const char*,12> cabIds{"cabblend","cabAgain","cabAdelay","cabAinvert","cabBgain","cabBdelay","cabBinvert","cabBlow","cabBhigh","cabBtype","cabAcutsOff","cabBcutsOff"};
    for(int i=0;i<3;++i) for(size_t j=0;j<cabIds.size();++j) cabPanelParameters[i][j]=state.getRawParameterValue(juce::String(cabIds[j])+juce::String(i+1));
    clearMidi();
    boardParameters.bind(state);
    ampSelection.bind(state);
    nativeAmps.bind(state);nativePost.bind(state);
    gateAfterRig=state.getRawParameterValue("gateAfterRig");
    gateRangeDb=state.getRawParameterValue("gateRangeDb");
    gate2.bind(state);expander.bind(state);room.bind(state);signalOrder.bind(state);activeOrder=signalOrder.read({});
    synthOutput=state.getRawParameterValue("synthOutput");keyComp=state.getRawParameterValue("scKeyComp");
    keyGates=state.getRawParameterValue("scKeyGates");keyMultiband=state.getRawParameterValue("scKeyMultiband");
    syncTransposeQuality();
    const std::array<const char*,extraCount> extraIds{"dualtype","dualblend","dualcross","inputmode","doubleron","doublertime","tempo","temposync","metronome"};
    for(size_t i=0;i<extraIds.size();++i)extras[i]=state.getRawParameterValue(extraIds[i]);
    lowCompParameter=state.getRawParameterValue("lowcomp");lowAmpMixParameter=state.getRawParameterValue("lowampmix");
    preOrderParameter=state.getRawParameterValue("preorder");
    gainOrderParameter=state.getRawParameterValue("gainorder");
    const std::array<const char*,globalCount> ids{"mode","x1","x2","input","output","gateon","gatethreshold","gaterelease","gatehold","transposeon","transpose","oversampling","tuneron","tunermute"};
    for(size_t i=0;i<ids.size();++i) globals[i]=state.getRawParameterValue(ids[i]);
    for(size_t i=0;i<spectralforge::fxSpecs.size();++i) fxParameters[i]=state.getRawParameterValue(spectralforge::fxSpecs[i].id);
    for(size_t i=0;i<modelParameters.size();++i)modelParameters[i]=state.getRawParameterValue(spectralforge::modelFamilies[i].parameter);
    for(size_t i=0;i<modelParameters.size();++i)modelExtensionParameters[i]=spectralforge::effectFamilyExtended(int(i))?state.getRawParameterValue(spectralforge::effectExtensionID(int(i))):nullptr;
    const std::array<const char*,18> laneIds{"amp","drive","level","bass","lowmid","highmid","treble","presence","resonance","bandtone","mute","solo","polarity","cab","cablow","cabhigh","cabtype","ampon"};
    for(int i=0;i<3;++i) for(size_t k=0;k<laneIds.size();++k)
        laneParameters[i][k]=state.getRawParameterValue(juce::String(laneIds[k])+juce::String(i+1));
}
void ChimeraProcessor::recheckBetaClock()
{
    // Also picks up a tester code entered in another Chimera instance.
    if(const auto stored=spectralforge::beta::tester::Store::load(); stored.valid && !betaLifecycle.testerExtension()) {
        testerHolder=stored.name;
        betaLifecycle.setTesterExtension(true);
    }
    (void)betaLifecycle.recoverFromClockCorrection(betaClock());
}
spectralforge::beta::tester::Grant ChimeraProcessor::applyTesterCode(const juce::String& code)
{
    const auto grant=spectralforge::beta::tester::Store::save(code);
    if(grant.valid) {
        testerHolder=grant.name;
        betaLifecycle.setTesterExtension(true);
    }
    return grant;
}
void ChimeraProcessor::removeTesterCode()
{
    spectralforge::beta::tester::Store::clear();
    testerHolder.clear();
    betaLifecycle.setTesterExtension(false);
}
juce::String ChimeraProcessor::betaNotice() const
{
    using Status=spectralforge::beta::Status;
    juce::String text=juce::String("BETA / ")+spectralforge::release::version+" / Build "+CHIMERA_BUILD_REVISION;
    text+="\nSource: "+juce::String(CHIMERA_SOURCE_SHA);
    text+="\nExpiry (UTC): "+juce::String(CHIMERA_BETA_EXPIRY_UTC[0] ? CHIMERA_BETA_EXPIRY_UTC : "Not set - development only");
    text+="\nExpiry (KST / UTC+09:00): "+juce::String(CHIMERA_BETA_EXPIRY_KST[0] ? CHIMERA_BETA_EXPIRY_KST : "Not set - development only");
    if(juce::String(CHIMERA_BETA_MODE)=="qualification-only")text+="\nTEST ONLY - synthetic CI expiry; not approved for release.";
    const auto current=betaStatus();
    text+="\n"+juce::String(spectralforge::beta::remainingText(current));
    switch(current.status) {
        case Status::notBeta:
            text+=juce::String(CHIMERA_BETA_MODE)=="pending-approval"
                ? "\nExpiry approval pending. This development build cannot be published."
                : "\nBeta expiry is not enabled for this build.";break;
        case Status::valid:text+="\nOffline beta is valid.";break;
        case Status::warning:text+="\nExpires within 14 days. Save your project and plan an update.";break;
        case Status::expired:
            text+=betaProcessingPermitted()
                ? "\nExpired: this active instance keeps its sound until removed. Newly opened instances will be muted."
                : "\nExpired: this new instance has muted output. Your project and sounds remain editable and saveable.";break;
        case Status::clockUntrusted:
            text+=betaProcessingPermitted()
                ? "\nSystem clock is invalid. Check the clock; this instance keeps processing."
                : "\nSystem clock is invalid. This instance remains muted. Correct the clock and choose RECHECK CLOCK.";break;
        case Status::configurationError:text+="\nInvalid beta build metadata: output is muted. Install a qualified successor.";break;
        case Status::testerExtended:text+="\nBeta tester code: "+testerHolder+". This build and later betas do not expire on this computer.";break;
    }
    if(!betaProcessingPermitted() && (current.status==Status::valid || current.status==Status::warning))
        text+="\nThis instance is still muted. After correcting the system clock, choose RECHECK CLOCK.";
    text+="\nProjects, presets, automation, A/B and IR data are preserved. Updates are optional network actions.";
    return text;
}

ChimeraProcessor::~ChimeraProcessor()
{
    const spectralforge::lifecycle::Scope trace("processor.destroy", this);
    releaseResources();
}
void ChimeraProcessor::releaseResources()
{
    const spectralforge::lifecycle::Scope trace("processor.release", this);
    // The host has stopped processing. Signal both workers before joining
    // either, then reclaim ALL IR kernels while the plugin is still loaded.
    // Each juce::dsp::Convolution owns a further background loader: merely
    // stopping IRLibrary left those threads alive until member destruction.
    lastProcessTicks.store(0); // stopped: a recall applies at once
    library.requestStop(); tuner.requestStop();
    library.stop(); tuner.stop();
    for (int lane=0; lane<3; ++lane) engine.cabinet(lane).clear();
    jassert(backgroundResourcesReleased());
}
void ChimeraProcessor::prepareToPlay(double sr,int block)
{
    const spectralforge::lifecycle::Scope trace("processor.prepare", this);
    releaseResources(); cpuAverage.store(0);cpuPeak.store(0);rate=sr; maximumBlock=juce::jmax(1,block);
    switchFadeLength=juce::jmax(1,int(sr*.005));switchFadePosition=switchFadeLength;switchFade=SwitchFade::idle;processedSincePrepare=holdForSwitch=false;
    lastProcessTicks.store(0);fadedSilent.store(true);
    autoSetCaptureState.prepare(sr);
    const juce::dsp::ProcessSpec spec{sr,(juce::uint32)maximumBlock,(juce::uint32)juce::jmax(1,getMainBusNumOutputChannels())};
    keyBuffer.setSize(2,maximumBlock);keyBuffer.clear();
    toneKeyDelay.prepare(4096,maximumBlock);postKeyDelay.prepare(4096,maximumBlock);
    // Install the restored panel state before Cab::prepare initializes its
    // smoothers. This also clears a previous A/B blend/bypass after reprepare;
    // the normal process() path still smooths live automation as before.
    for(int i=0;i<3;++i) {
        const auto get=[&](int id){return cabPanelParameters[i][id]->load();};
        auto& cab=engine.cabinet(i);auto* b=cab.secondMic();
        cab.blend=spectralforge::cabMicRouting(originalCabParameters.values[i][3]->load()>.5f,originalCabParameters.values[i][8]->load()>.5f,int(laneParameters[i][16]->load()),int(get(9)),get(0)).blend;cab.gainDb=get(1);cab.delayMs=get(2);cab.invert=get(3)>.5f;
        b->gainDb=get(4);b->delayMs=get(5);b->invert=get(6)>.5f;
        cab.enable(laneParameters[i][13]->load()>.5f);
    }
    std::array<spectralforge::AmpNativeState,3> initialNativeStates{};
    for(int i=0;i<3;++i)initialNativeStates[(size_t)i]=nativeAmps.read(ampContext(i));
    engine.prepare(spec,initialNativeStates); preFX.prepare(spec); pedalBoard.prepare(spec); postFX.prepare(spec);creative.prepare(spec);synth.prepare(sr,int(spec.maximumBlockSize));utilities.prepare(spec);levelMatch.prepare(sr,int(spec.numChannels)); tuner.prepare(sr);toneEQ.prepare(sr);finalEQ.prepare(sr);dynamics.prepare(sr);multiband.prepare(sr);saturation.prepare(sr,maximumBlock);filterModulation.prepare(sr);deesser.prepare(sr);
    dynamicsAnalysisState.prepare(sr,int(spec.maximumBlockSize));
    audioBoard=boardParameters.read();
    const int maximumGateDelay=juce::jmax(preFX.latency(true),pedalBoard.maximumLatency()+preFX.transpose.latency())+engine.latency();
    postRigGate.prepare(spec,maximumGateDelay);gate2.prepare(spec,maximumGateDelay);expander.prepare(spec,maximumGateDelay);
    room.prepare(spec);
    activeOrder=signalOrder.read(activeOrder);toneSwitch.prepare(sr);postSwitch.prepare(sr);
    gate2Gain.store(1);expanderGain.store(1);
    hostBypass.prepare(postFX.latency()+engine.latency()
        +juce::jmax(preFX.latency(true),pedalBoard.maximumLatency()+preFX.transpose.latency()));
    preReduction.store(0);postReduction.store(0);for(auto& meter:postPeaks)meter.store(0);
    for(auto& meter:nativePostMeters)meter.store(0);
    std::array<int,3> sources{};
    for(int i=0;i<3;++i) sources[i]=(int)laneParameters[i][16]->load();
    for(int i=0;i<3;++i) engine.cabinet(i).secondMic()->requestedSource.store(int(cabPanelParameters[i][9]->load()));
    syncOriginalCabRequests();
    library.prepare(spec,sources);
    inputGain.reset(sr,.020); outputGain.reset(sr,.020); tuningMute.reset(sr,.005);
    inputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(globals[input]->load()));
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(globals[output]->load()));
    tuningMute.setCurrentAndTargetValue(1);
    betaOutputGain.reset(sr,.050);
    betaOutputGain.setCurrentAndTargetValue(betaLifecycle.processingPermitted() ? 1.f : 0.f);
    engine.setOversampling((int)globals[os]->load());
    // Gate lookahead above is sized while the transposer is on its Classic
    // (largest) window; select the stored quality mode now, before latency is
    // reported, so the host gets the active mode's delay from the first block.
    preFX.transpose.setMode(transposeQualityMode.load());
    const bool pitchActive=globals[pitchOn]->load()>.5f && int(globals[semitones]->load())!=0;
    setLatencySamples(saturation.latency()+postFX.latency()+engine.latency()+(audioBoard.enabled ? pedalBoard.latency(audioBoard)+(pitchActive ? preFX.transpose.latency() : 0) : preFX.latency(pitchActive)));
}
bool ChimeraProcessor::isBusesLayoutSupported(const BusesLayout& buses) const
{
    const auto mono=juce::AudioChannelSet::mono(),stereo=juce::AudioChannelSet::stereo(),off=juce::AudioChannelSet::disabled();
    // Main: mono or stereo, input equal to output. Sidechain: off, mono or
    // stereo. Synth bus: off or stereo.
    const auto sidechain=buses.inputBuses.size()>1?buses.getChannelSet(true,1):off;
    const auto synthBus=buses.outputBuses.size()>1?buses.getChannelSet(false,1):off;
    return (buses.getMainOutputChannelSet()==mono || buses.getMainOutputChannelSet()==stereo) &&
            buses.getMainInputChannelSet()==buses.getMainOutputChannelSet() &&
            (sidechain==off || sidechain==mono || sidechain==stereo) && (synthBus==off || synthBus==stereo) &&
            buses.inputBuses.size()<=2 && buses.outputBuses.size()<=2;
}
juce::AudioBuffer<float> ChimeraProcessor::busBuffer(juce::AudioBuffer<float>& hostBuffer,bool input) const
{
    if(getBusCount(input)<2 || !getBus(input,1)->isEnabled() || getChannelCountOfBus(input,1)==0)return {};
    const int first=getChannelIndexInProcessBlockBuffer(input,1,0),count=getChannelCountOfBus(input,1);
    if(first+count>hostBuffer.getNumChannels())return {};
    return {hostBuffer.getArrayOfWritePointers()+first,count,hostBuffer.getNumSamples()};
}
void ChimeraProcessor::processBlock(juce::AudioBuffer<float>& hostBuffer,juce::MidiBuffer& midi)
{
    spectralforge::EQCaptureBoundary::CallbackScope eqCaptureScope(eqCaptureBoundaryState);
    const auto started=juce::Time::getHighResolutionTicks();
    juce::ScopedNoDenormals noDenormals;
    updateComparisonProtection();
    audioThread.store(std::hash<std::thread::id>{}(std::this_thread::get_id()),std::memory_order_relaxed);
    resetProcessingIfPending();
    if(fadeRequested.exchange(false)) {
        // beginSwitch() waits for silence before it changes the state.
        if(!processedSincePrepare || switchFade==SwitchFade::hold || switchFade==SwitchFade::wait)fadedSilent.store(true);
        else {switchFade=SwitchFade::out;holdForSwitch=true;}
    }
    if(switchPending.exchange(false)) {
        holdForSwitch=false;
        // Nothing has played since prepareToPlay: there is no sound to fade.
        if(!processedSincePrepare){resetProcessing(true);switchFade=SwitchFade::idle;switchFadePosition=switchFadeLength;}
        else if(switchFade==SwitchFade::hold || switchFade==SwitchFade::wait) { // already silent
            resetProcessing(true);switchWait=getLatencySamples()+int(rate*switchSettleSeconds);switchCabinetBudget=int(rate*switchCabinetWaitSeconds);switchCabinetLate=false;
            if(switchWait>0)switchFade=SwitchFade::wait;else{switchFade=SwitchFade::in;switchFadePosition=0;}
        }
        else {hostBypass.reset();switchFade=SwitchFade::out;}
    }
    // The main bus, and the optional sidechain and SYNTH bus. Input and output
    // buses share buffer channels: the sidechain's channels are the SYNTH
    // bus's, so each chunk's key is copied out before its SYNTH bus is written.
    auto buffer=getBusBuffer(hostBuffer,false,0);
    const auto keyBus=busBuffer(hostBuffer,true);auto synthBus=busBuffer(hostBuffer,false);
    // Capture before input gain/routing/effects, inside the callback CPU meter.
    hostBypass.process(buffer.getArrayOfWritePointers(),buffer.getNumChannels(),buffer.getNumSamples(),0,false);
    for(const auto metadata:midi) {
        const auto message=metadata.getMessage();if(!message.isController())continue;
        const int cc=message.getControllerNumber(),learn=midiLearn.exchange(-1);
        if(learn>=0) midiMap[(size_t)cc].store(learn);
        const int target=midiMap[(size_t)cc].load();
        if(target>=0 && target<getParameters().size()) getParameters()[target]->setValueNotifyingHost(message.getControllerValue()/127.f);
    }
    synth.handleMidi(midi);   // 1.10 SYNTH notes, applied at the block start
    float bpm=extras[tempo]->load();
    if(extras[hostTempo]->load()>.5f) if(auto* playhead=getPlayHead()) if(const auto position=playhead->getPosition()) if(const auto hostBpm=position->getBpm()) bpm=juce::jlimit(40.f,240.f,float(*hostBpm));
    tempoMeter.store(bpm);
    // Host position for bar-locked modulation (docs/MODULATION_1_9.md): known
    // only while the transport plays.
    hostPpq=std::numeric_limits<double>::quiet_NaN();
    hostPpqBpm=std::numeric_limits<double>::quiet_NaN();
    // The position advances at the host's own tempo, whatever Chimera's tempo setting is.
    hostBarStart=std::numeric_limits<double>::quiet_NaN();hostBarBeats=0.;
    if(auto* playhead=getPlayHead())if(const auto position=playhead->getPosition())if(position->getIsPlaying())if(const auto ppq=position->getPpqPosition())if(const auto hostBpm=position->getBpm())if(*hostBpm>0.){
        hostPpq=*ppq;hostPpqBpm=*hostBpm;
        // The bar the LFO lock counts from: the host's last bar line and bar length.
        if(const auto bar=position->getPpqPositionOfLastBarStart())if(const auto signature=position->getTimeSignature())
            if(signature->numerator>0 && signature->denominator>0){hostBarStart=*bar;hostBarBeats=4.0*signature->numerator/signature->denominator;}
    }
    // Hosts may deliver blocks larger than prepareToPlay's hint. All DSP and
    // convolution buffers remain bounded to the prepared capacity.
    for(int offset=0;offset<buffer.getNumSamples();offset+=maximumBlock)
    {
        const int count=juce::jmin(maximumBlock,buffer.getNumSamples()-offset);
        std::array<float*,2> channels{};
        for(int c=0;c<buffer.getNumChannels();++c) channels[(size_t)c]=buffer.getWritePointer(c,offset);
        for(int done=0;done<count;) {
            // Fade-out ends inside this chunk: process up to the reset point,
            // reset, then process the rest with the recalled sound.
            const int n=switchFade==SwitchFade::out?juce::jmin(count-done,juce::jmax(1,switchFadePosition)):count-done;
            std::array<float*,2> sub{};
            for(int c=0;c<buffer.getNumChannels();++c) sub[(size_t)c]=channels[(size_t)c]+done;
            juce::AudioBuffer<float> part(sub.data(),buffer.getNumChannels(),n);
            // This chunk's key, copied before the SYNTH bus overwrites it.
            juce::AudioBuffer<float> key(keyBuffer.getArrayOfWritePointers(),juce::jmin(2,keyBus.getNumChannels()),n);
            for(int c=0;c<key.getNumChannels();++c)key.copyFrom(c,0,keyBus,c,offset+done,n);
            externalKey=key.getNumChannels()>0?&key:nullptr;
            toneKeyDelayed=toneKeyDelay.process(externalKey,saturation.latency());postKeyDelayed=postKeyDelay.process(externalKey,postFX.latency());
            chunkPpq=std::isfinite(hostPpq)?hostPpq+double(offset+done)*hostPpqBpm/60.0/rate:hostPpq;
            busLeft=synthBus.getNumChannels()>0?synthBus.getWritePointer(0,offset+done):nullptr;
            busRight=synthBus.getNumChannels()>1?synthBus.getWritePointer(1,offset+done):busLeft;
            // While a recall keeps the output muted, recalled cabinets install
            // at once instead of crossfading 50 ms from the previous ones.
            engine.setInstantCabinetSwap(switchFade==SwitchFade::wait);
            process(part);
            externalKey=nullptr;
            // The switch fade applies to the main and SYNTH bus together.
            std::array<float*,4> faded{sub[0],sub[1]};int fadedChannels=buffer.getNumChannels();
            if(busLeft!=nullptr){faded[(size_t)fadedChannels++]=busLeft;if(busRight!=busLeft)faded[(size_t)fadedChannels++]=busRight;}
            juce::AudioBuffer<float> everything(faded.data(),fadedChannels,n);
            applySwitchFade(everything);
            done+=n;
        }
    }
    processedSincePrepare=true;lastProcessTicks.store(juce::Time::getHighResolutionTicks());
    // The policy is constructed once, off the audio thread. Expiry cannot stop
    // an admitted instance, including recording or non-realtime rendering.
    // A newly expired instance keeps the complete graph/latency and emits zero;
    // a user-requested clock correction fades the processed signal in (50 ms).
    betaOutputGain.setTargetValue(betaLifecycle.processingPermitted() ? 1.f : 0.f);
    if(betaOutputGain.isSmoothing()) {
        for(int n=0;n<buffer.getNumSamples();++n) {
            const float gain=betaOutputGain.getNextValue();
            for(int c=0;c<buffer.getNumChannels();++c)buffer.setSample(c,n,buffer.getSample(c,n)*gain);
            for(int c=0;c<synthBus.getNumChannels();++c)synthBus.setSample(c,n,synthBus.getSample(c,n)*gain);
        }
    } else if(betaOutputGain.getCurrentValue()==0.f) {buffer.clear();synthBus.clear();}
    if(!betaLifecycle.processingPermitted())outputPeak.store(0.f);
    if(buffer.getNumSamples()>0 && rate>0) {
        const double budget=buffer.getNumSamples()/rate;
        const float load=float(100.0*juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks()-started)/budget);
        const float decay=float(std::exp(-budget));
        cpuAverage.store(decay*cpuAverage.load()+(1-decay)*load);
        cpuPeak.store(juce::jmax(load,cpuPeak.load()*decay));
    }
}
void ChimeraProcessor::processBlockBypassed(juce::AudioBuffer<float>& hostBuffer,juce::MidiBuffer&)
{
    auto buffer=getBusBuffer(hostBuffer,false,0);
    {auto synthBus=busBuffer(hostBuffer,false);synthBus.clear();}   // bypassed: no synth on its bus
    updateComparisonProtection();
    resetProcessingIfPending();
    // Bypassed, the wet sound is not heard: a recall needs no fade.
    if(fadeRequested.exchange(false))fadedSilent.store(true);
    if(switchPending.exchange(false)){resetProcessing(true);switchFade=SwitchFade::idle;switchFadePosition=switchFadeLength;holdForSwitch=false;}
    // JUCE AAX's MasterBypassID invokes this callback. Its default implementation
    // passes dry audio immediately even though PDC still uses our nonzero delay.
    // Use the currently reported delay; parameter changes take effect when the
    // wet graph next processes (or prepareToPlay is called), as before.
    hostBypass.process(buffer.getArrayOfWritePointers(),buffer.getNumChannels(),buffer.getNumSamples(),getLatencySamples(),true);
}
void ChimeraProcessor::updateComparisonProtection()
{
    bool printing=isNonRealtime();
    if(auto* playhead=getPlayHead())if(const auto position=playhead->getPosition())
        printing=printing || position->getIsRecording();
    levelMatch.protectOutput(printing);
}
void ChimeraProcessor::resetProcessingIfPending()
{
    if(!resetPending.exchange(false)) return;
    resetProcessing(true);
    // A host reset ends any switch fade: the reset sound plays at once.
    if(switchFade!=SwitchFade::idle){switchFade=SwitchFade::idle;switchFadePosition=switchFadeLength;}
}
void ChimeraProcessor::resetProcessing(bool hostBypassToo)
{
    engine.reset();preFX.reset();pedalBoard.reset();postRigGate.reset();gate2.reset();expander.reset();postFX.reset();creative.reset();synth.reset();utilities.reset();
    toneEQ.reset();finalEQ.reset();dynamics.reset();multiband.reset();saturation.reset();filterModulation.reset();deesser.reset();room.reset();if(hostBypassToo)hostBypass.reset();
}
void ChimeraProcessor::applySwitchFade(juce::AudioBuffer<float>& part)
{
    if(switchFade==SwitchFade::idle)return;
    const auto gain=[this]{return float(.5-.5*std::cos(juce::MathConstants<double>::pi*switchFadePosition/switchFadeLength));};
    for(int n=0;n<part.getNumSamples();++n) {
        float g=1;
        if(switchFade==SwitchFade::out){if(switchFadePosition>0)--switchFadePosition;g=gain();}
        else if(switchFade==SwitchFade::hold){g=0;if(--switchHold<=0){switchFade=SwitchFade::in;switchFadePosition=0;holdForSwitch=false;}} // abandoned: no state change came
        else if(switchFade==SwitchFade::wait){
            g=0;
            if(--switchWait<=0) {
                // Re-check the cabinets every 64 samples while the budget lasts.
                // A cabinet installed late (at once, no crossfade) gets the
                // settle time again so the dynamics have heard it first.
                if(switchCabinetBudget>0 && !engine.audibleCabinetsSettled()){switchWait=juce::jmin(64,switchCabinetBudget);switchCabinetBudget-=switchWait;switchCabinetLate=true;}
                else if(switchCabinetLate){switchCabinetLate=false;switchWait=juce::jmax(1,int(rate*switchSettleSeconds));}
                else {switchFade=SwitchFade::in;switchFadePosition=0;}
            }
        }
        else if(switchFade==SwitchFade::in){if(++switchFadePosition>=switchFadeLength){switchFadePosition=switchFadeLength;switchFade=SwitchFade::idle;}g=gain();}
        for(int c=0;c<part.getNumChannels();++c)part.getWritePointer(c)[n]*=g;
    }
    // The fade-out reached silence at the end of this part: reset now, before
    // the next sample, and stay muted while the cleared delay lines refill.
    if(switchFade==SwitchFade::out && switchFadePosition==0 && holdForSwitch) {
        switchFade=SwitchFade::hold;switchHold=int(rate*.5);fadedSilent.store(true);
    } else if(switchFade==SwitchFade::out && switchFadePosition==0) {
        resetProcessing(false);
        switchWait=getLatencySamples()+int(rate*switchSettleSeconds);switchCabinetBudget=int(rate*switchCabinetWaitSeconds);switchCabinetLate=false;
        if(switchWait>0)switchFade=SwitchFade::wait;else{switchFade=SwitchFade::in;switchFadePosition=0;}
    }
}
void ChimeraProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if(extras[inputMode]->load()>.5f && buffer.getNumChannels()==2) buffer.copyFrom(1,0,buffer,0,0,buffer.getNumSamples());
    const auto value=[this](Global id){return globals[id]->load();};
    inputGain.setTargetValue(juce::Decibels::decibelsToGain(value(input)));
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(value(output)));
    float peak=0;
    for(int n=0;n<buffer.getNumSamples();++n)
    {
        const float gain=inputGain.getNextValue();
        for(int c=0;c<buffer.getNumChannels();++c)
        {
            const float x=buffer.getSample(c,n)*gain;
            buffer.setSample(c,n,std::isfinite(x) ? x : 0.f);
            peak=juce::jmax(peak,std::abs(buffer.getSample(c,n)));
        }
    }
    const float meterDecay=float(std::exp(-buffer.getNumSamples()/(rate*.4)));
    inputPeak.store(juce::jmax(peak,inputPeak.load()*meterDecay));
    autoSetCaptureState.push(spectralforge::autoset::Tap::input,buffer.getArrayOfReadPointers(),buffer.getNumChannels(),buffer.getNumSamples());
    measureStage(0,buffer);
    tuner.push(buffer,value(tunerOn)>.5f);
    synth.analyse(buffer);   // 1.10 SYNTH tracks the clean input
    synth.render(buffer.getNumSamples(),tempoMeter.load());   // rendered once, added at its insert point below
    {   // SYNTH output: the guitar mix, its own bus, or both. Without a SYNTH bus it stays in the mix.
        const int output=juce::jlimit(0,2,juce::roundToInt(synthOutput->load()));const bool bus=busLeft!=nullptr;
        synth.setOutputs(output!=1 || !bus,bus && output>=1);
        if(bus)synth.writeBus(busLeft,busRight,buffer.getNumSamples());
    }
    synthTracked.store(synth.read().on?synth.pitchTracker().hz():0.0,std::memory_order_relaxed);
    auto fx=spectralforge::readFX(fxParameters);fx.envelopeFirst=preOrderParameter->load()>.5f;fx.boostAfterDrive=gainOrderParameter->load()>.5f;for(size_t i=0;i<modelParameters.size();++i)fx.models[i]=modelExtensionParameters[i]?spectralforge::effectSelectedModel(int(i),modelParameters[i]->load(),modelExtensionParameters[i]->load()):(int)modelParameters[i]->load();fx.tempo=tempoMeter.load();if(fx.delaySync)fx.delayMs=60000.f/tempoMeter.load();
    fx.postNative=nativePost.read();
    const bool pitching=value(pitchOn)>.5f && int(value(semitones))!=0;
    const auto before=boardEditSequence.load(std::memory_order_acquire);
    if((before&1u)==0) {
        const auto candidate=boardParameters.read();
        if(before==boardEditSequence.load(std::memory_order_acquire)) audioBoard=candidate;
    }
    const bool gateAtOutput=gateAfterRig->load()>.5f;
    preFX.gate.setAnalysis(gateAtOutput?nullptr:&dynamicsAnalysisState);
#if CHIMERA_ROUTING_DIAGNOSTICS
    routingGateObserved.store(int(gateAtOutput)); routingBlocks.fetch_add(1);
#endif
    const float gateRange=gateRangeDb->load();
    preFX.gate.setMode(dynamics.expander(),dynamics.gateRatio(),dynamics.gateAttack());
    postRigGate.setMode(dynamics.expander(),dynamics.gateRatio(),dynamics.gateAttack());
    // Apply the transpose quality/latency mode before reading transpose.latency()
    // below; switching is allocation-free and re-reports latency via line ~231.
    preFX.transpose.setMode(transposeQualityMode.load());
    const int preLatency=audioBoard.enabled ? pedalBoard.latency(audioBoard)+(pitching ? preFX.transpose.latency() : 0) : preFX.latency(pitching);
    // Auto Set holds every gate open while it measures the noise the rig adds.
    const bool gatesAllowed=!autoSetGateBypass.load(std::memory_order_relaxed);
    const bool gateEnabled=gatesAllowed && value(gateOn)>.5f;
    // External key (docs/SIDECHAIN_SYNTH_BUS.md): each switched-on detector
    // listens to the sidechain instead of the clean input.
    const auto* gateKey=externalKey!=nullptr && keyGates->load()>.5f ? externalKey : nullptr;
    const auto& gateDetector=gateKey!=nullptr ? *gateKey : static_cast<const juce::AudioBuffer<float>&>(buffer);
    preFX.gate.setKey(gateKey);
    postRigGate.detect(gateDetector,gateEnabled,value(threshold),value(release),value(hold),preLatency+engine.latency(),gateRange);
    // Gate 2 and the Expander detect on the same clean input. Pre-rig units
    // apply here, ahead of the main gate: Expander > Gate 2 > Gate.
    expander.detect(gateDetector,preLatency+engine.latency(),gatesAllowed);gate2.detect(gateDetector,preLatency+engine.latency(),gatesAllowed);
    if(!expander.afterRig())expander.apply(buffer);
    if(!gate2.afterRig())gate2.apply(buffer);
    if(audioBoard.enabled) {
        preFX.gate.process(buffer,!gateAtOutput && gateEnabled,value(threshold),value(release),value(hold),gateRange);
        preFX.transpose.process(buffer,pitching,(int)value(semitones));
        pedalBoard.process(buffer,audioBoard);
    } else preFX.process(buffer,!gateAtOutput && gateEnabled,value(threshold),value(release),value(hold),pitching,(int)value(semitones),fx,gateRange);
    boardReduction.store(audioBoard.enabled ? pedalBoard.compressorReduction() : 0.f);
    preReduction.store(audioBoard.enabled ? pedalBoard.compressorReduction() : preFX.compressor.reduction());
    measureStage(1,buffer);
    const int latency=saturation.latency()+postFX.latency()+engine.latency()+(audioBoard.enabled ? pedalBoard.latency(audioBoard)+(pitching ? preFX.transpose.latency() : 0) : preFX.latency(pitching));
    if(getLatencySamples()!=latency) setLatencySamples(latency);
    engine.setOversampling((int)value(os));
    syncOriginalCabRequests();
    std::array<spectralforge::LaneState,3> lanes{};
    for(int i=0;i<3;++i)
    {
        auto f=[this,i](int k){return laneParameters[i][(size_t)k]->load();};
        auto& lane=lanes[i];
        lane.amp=ampSelection.model(i); lane.ampChannel=ampSelection.channel(i,lane.amp);
        lane.drive=f(1); lane.levelDb=f(2); lane.bass=f(3); lane.lowMid=f(4);
        lane.native=nativeAmps.read(spectralforge::ampNativeContext((int)value(mode),i));
        if(lane.native.enabled) {lane.amp=lane.native.model;lane.ampChannel=lane.native.channel;lane.levelDb=0.f;}
        lane.highMid=f(5); lane.treble=f(6); lane.presence=f(7); lane.resonance=f(8); lane.bandTone=f(9);
        lane.mute=f(10)>.5f; lane.solo=f(11)>.5f; lane.polarity=f(12)>.5f; lane.cab=f(13)>.5f;
        lane.cabLow=f(14); lane.cabHigh=f(15); lane.ampEnabled=f(17)>.5f;
        engine.cabinet(i).requestedSource.store((int)f(16));
        applyCabinetParameters(i,engine.cabinet(i));
    }
    lanes[0].lowComp=lowCompParameter->load();lanes[0].lowAmpMix=lowAmpMixParameter->load();
    const bool dualCross=value(mode)==1 && extras[dualType]->load()>.5f;
    // Room Core: the engine fills the send bus only while the room is on (or
    // its send is fading out after Room off).
    const auto roomSends=room.sends();
    juce::AudioBuffer<float>* roomBus=room.wantsInput() ? &room.bus(buffer.getNumChannels(),buffer.getNumSamples()) : nullptr;
    synth.addAt(spectralforge::SynthInsert::ampInput,buffer);   // SYNTH insert: into the amps, like a pedal
    engine.process(buffer,(spectralforge::RoutingMode)(int)value(mode),dualCross ? extras[dualFrequency]->load() : value(x1),value(x2),lanes,audioBoard.enabled ? &pedalBoard.cleanOutput() : &preFX.cleanOutput(),dualCross,extras[dualBlend]->load(),
                   roomBus,roomBus!=nullptr ? &roomSends : nullptr);
    lowCompGain.store(engine.lowReduction());
    autoSetCaptureState.push(spectralforge::autoset::Tap::rig,buffer.getArrayOfReadPointers(),buffer.getNumChannels(),buffer.getNumSamples());
    // Post-rig gates attenuate the send bus with the direct sum's own gain.
    if(gateAtOutput) {spectralforge::DynamicsAnalysis::Scope capture(&dynamicsAnalysisState,spectralforge::DynamicsStage::gate,buffer);postRigGate.apply(buffer,roomBus);}
    gateGain.store(gateAtOutput ? postRigGate.reduction() : preFX.gate.reduction());
    // Post-rig order mirrors the pre-rig one: Gate > Gate 2 > Expander.
    if(gate2.afterRig())gate2.apply(buffer,roomBus);
    if(expander.afterRig())expander.apply(buffer,roomBus);
    // The wet return joins after the gates and before Tone EQ, so a tail
    // already in the room is not cut by a closing gate.
    if(room.running())room.process(roomBus,buffer);
    gate2Gain.store(gate2.reduction());expanderGain.store(expander.reduction());
    // SYNTH insert (docs/SYNTH_PAGE.md): after RIG SUM and the post-rig gates, before Tone EQ.
    // 1.4 signal order (docs/SIGNAL_ORDER_1_4.md): the tone / dynamics block and
    // the POST rack run in their saved order. A change fades the region's
    // output out and back in around the switch; with nothing audible yet it
    // applies at once.
    {const auto requested=signalOrder.read(activeOrder);
     const bool immediate=!processedSincePrepare || switchFade==SwitchFade::hold || switchFade==SwitchFade::wait;
     if(toneSwitch.begin(requested.tone!=activeOrder.tone,immediate))activeOrder.tone=requested.tone;
     if(postSwitch.begin(requested.post!=activeOrder.post,immediate))activeOrder.post=requested.post;}
    synth.addAt(spectralforge::SynthInsert::beforeToneEQ,buffer);   // at the start of the tone / dynamics block
    measureStage(2,buffer);
    bool latentBefore=false;   // HQ saturation ran ahead: keyed detectors read the key delayed to match
    for(const int module:activeOrder.tone.slots) {
        using spectralforge::autoset::Tap;
        const auto* toneKey=latentBefore?toneKeyDelayed:externalKey;
        const auto tap=[&](Tap t){autoSetCaptureState.push(t,buffer.getArrayOfReadPointers(),buffer.getNumChannels(),buffer.getNumSamples());};
        switch(module) {
            case spectralforge::toneEQModule:tap(Tap::tone);toneEQ.process(buffer);break;
            case spectralforge::compressorModule:tap(Tap::compressor);
                {spectralforge::DynamicsAnalysis::Scope capture(&dynamicsAnalysisState,spectralforge::DynamicsStage::compressor,buffer);dynamics.compress(buffer,toneKey!=nullptr && keyComp->load()>.5f ? toneKey : nullptr);}break;
            case spectralforge::multibandModule:tap(Tap::multiband);
                {spectralforge::DynamicsAnalysis::Scope capture(&dynamicsAnalysisState,spectralforge::DynamicsStage::multiband,buffer);multiband.process(buffer,toneKey!=nullptr && keyMultiband->load()>.5f ? toneKey : nullptr);}break;
            // Three-band saturation (docs/SATURATION_1_6.md): adds only what each band's
            // curve changes; Off / Mix 0 / Clean bands leave the signal untouched.
            case spectralforge::saturationModule:
                {spectralforge::DynamicsAnalysis::Scope capture(&dynamicsAnalysisState,spectralforge::DynamicsStage::saturation,buffer);saturation.process(buffer);latentBefore=saturation.latency()>0;}break;
            // Two-filter modulation (docs/FILTER_MODULATION_1_7.md): off leaves the signal untouched.
            case spectralforge::filterModule:
                {spectralforge::DynamicsAnalysis::Scope capture(&dynamicsAnalysisState,spectralforge::DynamicsStage::filter,buffer);filterModulation.process(buffer,tempoMeter.load(),chunkPpq,toneKey,hostPpqBpm,hostBarStart,hostBarBeats);}break;
            case spectralforge::deesserModule:tap(Tap::deesser);
                {spectralforge::DynamicsAnalysis::Scope capture(&dynamicsAnalysisState,spectralforge::DynamicsStage::deesser,buffer);deesser.process(buffer);}break;
            default:break;
        }
    }
    toneSwitch.apply(buffer);
    synth.addAt(spectralforge::SynthInsert::beforePost,buffer);
    postFX.setKey(externalKey,postKeyDelayed);   // Console VCA's EXTERNAL SIDE-CHAIN switch decides
    postFX.process(buffer,fx,activeOrder.post);
    postFX.setKey(nullptr);preFX.gate.setKey(nullptr);
    postSwitch.apply(buffer);
    synth.addAt(spectralforge::SynthInsert::afterPost,buffer);
    measureStage(3,buffer);
    postReduction.store(postFX.compressorReduction());
    for(int section=0;section<3;++section)nativePostMeters[(size_t)section].store(postFX.nativeMeter(section));
    for(size_t i=0;i<postPeaks.size();++i)
        postPeaks[i].store(juce::jmax(postFX.stagePeaks[i],postPeaks[i].load()*meterDecay));
    utilities.process(buffer,tempoMeter.load(),extras[doublerOn]->load()>.5f,extras[doublerTime]->load(),extras[metronome]->load()>.5f,restartClick.exchange(false));
    finalEQ.process(buffer);
    tuningMute.setTargetValue(value(tunerOn)>.5f && value(tunerMute)>.5f ? 0.f : 1.f);
    peak=0;
    for(int n=0;n<buffer.getNumSamples();++n)
    {
        const float gain=outputGain.getNextValue()*tuningMute.getNextValue();
        for(int c=0;c<buffer.getNumChannels();++c)
        {
            const float x=buffer.getSample(c,n)*gain;
            buffer.setSample(c,n,std::isfinite(x) ? x : 0.f);
        }
    }
    autoSetCaptureState.push(spectralforge::autoset::Tap::output,buffer.getArrayOfReadPointers(),buffer.getNumChannels(),buffer.getNumSamples());
    {spectralforge::DynamicsAnalysis::Scope capture(&dynamicsAnalysisState,spectralforge::DynamicsStage::limiter,buffer);dynamics.limit(buffer);} // Last gain stage after output trim.
    levelMatch.process(buffer,selectedComparison.load()); // Comparison output cut; meters show what is heard.
    for(int c=0;c<buffer.getNumChannels();++c)peak=juce::jmax(peak,buffer.getMagnitude(c,0,buffer.getNumSamples()));
    outputPeak.store(juce::jmax(peak,outputPeak.load()*meterDecay));
    measureStage(4,buffer);
}
juce::Result ChimeraProcessor::loadMicIR(int lane,int slot,const juce::File& file)
{
    if(slot==0) return loadIR(lane,file);
    if(lane<0 || lane>2 || slot!=1) return juce::Result::fail("Invalid mic slot");
    auto result=library.importFile(lane+3,file);
    if(result.wasOk()) {
        const bool baked=library.userIsCabinetExport(lane+3);const auto n=juce::String(lane+1);
        setRawParameter("cabBcutsOff"+n,baked?1.f:0.f);
        if(baked)for(const auto* field:{"gain","delay","invert"})setRawParameter("cabB"+juce::String(field)+n,0);
    }
    if(result.wasOk()) { setRawParameter(spectralforge::originalCabID(lane,"Bon"),0); setRawParameter("cabBtype"+juce::String(lane+1),3.f); engine.cabinet(lane).secondMic()->requestedSource.store(3); }
    return result;
}
// 1.4 signal order (docs/SIGNAL_ORDER_1_4.md): edits on the message thread.
// Slots are written one at a time, and the audio thread keeps its previous
// order while a region is not a permutation. The writes are ordered so that
// every intermediate state is invalid: the first changed slot first takes
// the second's current value (a duplicate), the others follow, and the first
// changed slot is written last. So no third, valid order can be heard.
void ChimeraProcessor::writeOrder(const spectralforge::SignalOrder& order)
{
    using spectralforge::OrderRegion;
    const auto write=[&](OrderRegion region,const int* target,int count){
        std::array<int,8> changed{};int k=0;
        const auto current=[&](int slot){return juce::roundToInt(state.getRawParameterValue(spectralforge::orderSlotID(region,slot))->load());};
        const auto set=[&](int slot,int value){setRawParameter(spectralforge::orderSlotID(region,slot),float(value));};
        for(int i=0;i<count;++i)if(current(i)!=target[i])changed[(size_t)k++]=i;
        if(k==0)return;
        if(k==1){set(changed[0],target[changed[0]]);return;}   // only from an invalid state
        set(changed[0],current(changed[1]));
        for(int j=2;j<k;++j)set(changed[(size_t)j],target[changed[(size_t)j]]);
        set(changed[1],target[changed[1]]);
        set(changed[0],target[changed[0]]);
    };
    write(OrderRegion::tone,order.tone.slots.data(),spectralforge::toneModuleCount);
    write(OrderRegion::post,order.post.slots.data(),spectralforge::postModuleCount);
}
bool ChimeraProcessor::commitOrder(const spectralforge::SignalOrder& next)
{
    const auto current=requestedOrder();
    if(next==current || !next.tone.valid() || !next.post.valid())return false;
    orderUndo.push_back(current);if(orderUndo.size()>64)orderUndo.erase(orderUndo.begin());
    orderRedo.clear();writeOrder(next);return true;
}
bool ChimeraProcessor::placeModule(spectralforge::OrderRegion region,int module,int position)
{
    if(module<0 || module>=spectralforge::moduleCount(region))return false;
    auto next=requestedOrder();
    if(region==spectralforge::OrderRegion::tone)next.tone=next.tone.placed(module,position);else next.post=next.post.placed(module,position);
    return commitOrder(next);
}
bool ChimeraProcessor::moveModule(spectralforge::OrderRegion region,int module,int delta)
{
    const auto current=requestedOrder();
    const int from=region==spectralforge::OrderRegion::tone?current.tone.position(module):current.post.position(module);
    const int to=from+delta;
    if(from<0 || to<0 || to>=spectralforge::moduleCount(region))return false;
    return placeModule(region,module,to);
}
bool ChimeraProcessor::resetOrder(spectralforge::OrderRegion region)
{
    auto next=requestedOrder();
    if(region==spectralforge::OrderRegion::tone)next.tone=spectralforge::ToneOrder::initial();else next.post=spectralforge::PostOrder::initial();
    return commitOrder(next);
}
bool ChimeraProcessor::applyOrderPreset(spectralforge::OrderRegion region,int preset)
{
    const auto& presets=spectralforge::orderPresets(region);
    if(preset<0 || preset>=int(presets.size()))return false;
    auto next=requestedOrder();
    if(region==spectralforge::OrderRegion::tone)for(int i=0;i<spectralforge::toneModuleCount;++i)next.tone.slots[(size_t)i]=presets[(size_t)preset].slots[(size_t)i];
    else for(int i=0;i<spectralforge::postModuleCount;++i)next.post.slots[(size_t)i]=presets[(size_t)preset].slots[(size_t)i];
    return commitOrder(next);
}
bool ChimeraProcessor::undoOrder()
{
    if(orderUndo.empty())return false;
    const auto previous=orderUndo.back();orderUndo.pop_back();
    orderRedo.push_back(requestedOrder());writeOrder(previous);return true;
}
bool ChimeraProcessor::redoOrder()
{
    if(orderRedo.empty())return false;
    const auto next=orderRedo.back();orderRedo.pop_back();
    orderUndo.push_back(requestedOrder());writeOrder(next);return true;
}
juce::Result ChimeraProcessor::loadSynthSample(int osc,const juce::File& file)
{
    if(osc<0 || osc>=spectralforge::synthOscCount)return juce::Result::fail("Invalid oscillator");
    std::unique_ptr<spectralforge::SynthSample> sample;
    const auto result=spectralforge::synthSample::load(file,sample);
    if(result.failed())return result;
    const int root=spectralforge::synthSample::rootNote(*sample);
    synth.swapSample(osc,std::move(sample));   // the replaced sample is destroyed here, on this thread
    setRawParameter(spectralforge::synthOscID(osc,"On"),1);setRawParameter(spectralforge::synthOscID(osc,"Mode"),1);
    setRawParameter(spectralforge::synthOscID(osc,"Root"),float(root));
    return result;
}
void ChimeraProcessor::clearSynthSample(int osc)
{
    if(osc<0 || osc>=spectralforge::synthOscCount)return;
    synth.swapSample(osc,nullptr);
    setRawParameter(spectralforge::synthOscID(osc,"Mode"),0);
}
juce::Result ChimeraProcessor::loadIR(int lane,const juce::File& file)
{
    if(lane<0 || lane>2)return juce::Result::fail("Invalid lane");
    const auto result=library.importFile(lane,file);
    if(result.wasOk())
    {
        setRawParameter(spectralforge::originalCabID(lane,"Aon"),0);
        const auto n=juce::String(lane+1);const bool baked=library.userIsCabinetExport(lane);
        setRawParameter("cabAcutsOff"+n,baked?1.f:0.f);
        if(baked) {
            // This file already contains both mics, cuts, polarity and delays.
            // Load it as one complete cabinet, with no second application.
            setRawParameter("cab"+n,1);setRawParameter("cabblend"+n,0);
            for(const auto* field:{"gain","delay","invert"})setRawParameter("cabA"+juce::String(field)+n,0);
        }
        auto* parameter=state.getParameter("cabtype"+juce::String(lane+1));
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(3)); parameter->endChangeGesture();
        engine.cabinet(lane).requestedSource.store(3);
    }
    return result;
}
juce::AudioProcessorValueTreeState::ParameterLayout ChimeraProcessor::layout(){juce::AudioProcessorValueTreeState::ParameterLayout p;p.add(std::make_unique<juce::AudioParameterChoice>("mode","Routing",juce::StringArray{"Classic","Dual","Matrix"},0));p.add(std::make_unique<juce::AudioParameterFloat>("x1","X1",60.f,350.f,150.f));p.add(std::make_unique<juce::AudioParameterFloat>("x2","X2",500.f,4000.f,1200.f));for(int i=1;i<=3;++i){auto n=juce::String(i);p.add(std::make_unique<juce::AudioParameterChoice>("amp"+n,"Amp "+n,spectralforge::legacyAmpNames(),2));p.add(std::make_unique<juce::AudioParameterFloat>("drive"+n,"Drive "+n,0.f,1.f,.35f));p.add(std::make_unique<juce::AudioParameterFloat>("level"+n,"Level "+n,-24.f,12.f,0.f));for(auto id:{"bass","lowmid","highmid","treble"})p.add(std::make_unique<juce::AudioParameterFloat>(juce::String(id)+n,juce::String(id)+" "+n,-12.f,12.f,0.f));p.add(std::make_unique<juce::AudioParameterFloat>("presence"+n,"Presence "+n,0.f,10.f,0.f));p.add(std::make_unique<juce::AudioParameterFloat>("resonance"+n,"Resonance "+n,0.f,10.f,0.f));p.add(std::make_unique<juce::AudioParameterBool>("mute"+n,"Mute "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("solo"+n,"Solo "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("polarity"+n,"Polarity "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("cab"+n,"Cab "+n,true));p.add(std::make_unique<juce::AudioParameterFloat>("cablow"+n,"Cab Low "+n,20.f,500.f,70.f));p.add(std::make_unique<juce::AudioParameterFloat>("cabhigh"+n,"Cab High "+n,1500.f,20000.f,9000.f));}for(int i=1;i<=3;++i){auto n=juce::String(i);p.add(std::make_unique<juce::AudioParameterFloat>("bandtone"+n,"Matrix Band Tone "+n,-12.f,12.f,0.f));}
    for(int i=1;i<=3;++i) p.add(std::make_unique<juce::AudioParameterChoice>("cabtype"+juce::String(i),"Cabinet source "+juce::String(i),juce::StringArray{"Filters only","V30 / SM57","Jensen / SM57","User IR"},1));
    auto number=[&](const char* id,const char* name,float low,float high,float value,float interval=0.01f) { p.add(std::make_unique<juce::AudioParameterFloat>(id,name,juce::NormalisableRange<float>{low,high,interval},value)); };
    auto toggle=[&](const char* id,const char* name,bool value) { p.add(std::make_unique<juce::AudioParameterBool>(id,name,value)); };
    number("input","Input gain",-24,24,0); number("output","Output gain",-36,12,-6);
    toggle("gateon","Noise gate",true); number("gatethreshold","Gate threshold",-90,-15,-65);
    number("gaterelease","Gate release",5,500,80); number("gatehold","Gate hold",0,150,20);
    toggle("transposeon","Transpose",false);
    p.add(std::make_unique<juce::AudioParameterInt>("transpose","Transpose semitones",-12,12,0));
    p.add(std::make_unique<juce::AudioParameterChoice>("oversampling","Oversampling",juce::StringArray{"1x","2x","4x","8x"},2));
    toggle("tuneron","Tuner",false); toggle("tunermute","Mute while tuning",true);
    number("tunerref","Tuner A4 reference",430,450,440);
    toggle("preon","Pre drive",false);number("predrive","Pre drive amount",0,1,.3f);number("pretone","Pre drive tone",1000,10000,4000);number("prelevel","Pre drive level",-18,12,0);
    toggle("delayon","Post delay",false);number("delaytime","Delay time",20,1000,250);number("delayfeedback","Delay feedback",0,.85f,.25f);number("delaymix","Delay mix",0,.6f,.2f);
    toggle("reverbon","Post reverb",false);number("reverbsize","Reverb room size",0,1,.35f);number("reverbdamping","Reverb damping",0,1,.55f);number("reverbmix","Reverb mix",0,.6f,.15f);
    for(int i=1;i<=3;++i) p.add(std::make_unique<juce::AudioParameterBool>("ampon"+juce::String(i),"Amplifier enabled "+juce::String(i),true));
    number("lowcomp","Matrix LOW compression",0,1,.35f);
    number("lowampmix","Matrix LOW DI to amp blend",0,1,0);
    for(size_t i=12;i<spectralforge::fxSpecs.size();++i) {const auto& spec=spectralforge::fxSpecs[i];if(spec.toggle)toggle(spec.id,spec.id,spec.initial>.5f);else number(spec.id,spec.id,spec.minimum,spec.maximum,spec.initial,spec.interval);}
    p.add(std::make_unique<juce::AudioParameterChoice>("dualtype","Dual routing",juce::StringArray{"Blend","Crossover"},0));
    number("dualblend","Dual blend",0,1,.5f);number("dualcross","Dual crossover",60,4000,350);
    p.add(std::make_unique<juce::AudioParameterChoice>("inputmode","Input mode",juce::StringArray{"Stereo","Mono L"},0));
    toggle("doubleron","Doubler",false);number("doublertime","Doubler spread",1,20,6);
    number("tempo","Tempo",40,240,120);toggle("temposync","Follow host tempo",false);toggle("metronome","Metronome",false);
    for(size_t i=0;i<spectralforge::modelFamilies.size();++i) {const auto& family=spectralforge::modelFamilies[i];p.add(std::make_unique<juce::AudioParameterChoice>(family.parameter,juce::String(family.category)+" model",spectralforge::modelNames((int)i),0));}
    p.add(std::make_unique<juce::AudioParameterChoice>("preorder","Pedal detector order",juce::StringArray{"Compressor first","Envelope first"},1));
    // Append new parameters so existing host parameter indices remain stable.
    p.add(std::make_unique<juce::AudioParameterChoice>("gainorder","Pedal gain order",juce::StringArray{"Fuzz > Boost > Drive","Fuzz > Drive > Boost"},0));
    spectralforge::addPedalBoardParameters(p);
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"gateAfterRig",1},"Gate after rig (input detector)",false));
    spectralforge::addAmpSelectionParameters(p);
    spectralforge::addAmpNativeParameters(p);
    spectralforge::addPostNativeParameters(p);
    spectralforge::appendNewAmpNativeParameters(p);
    // Append after ALL released parameters, including native model banks.
    // Version hint 2 also keeps this after the released AU hint-1 parameters.
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gateRangeDb",2},"Gate range",
        juce::NormalisableRange<float>{0.f,spectralforge::NoiseGate::fullRangeDb,.1f},spectralforge::NoiseGate::fullRangeDb,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v,int) {return v>=spectralforge::NoiseGate::fullRangeDb ? juce::String("Full") : juce::String(v,1)+" dB";})
            .withValueFromStringFunction([](const juce::String& text) {return text.trim().equalsIgnoreCase("Full") ? spectralforge::NoiseGate::fullRangeDb : text.getFloatValue();})));
    spectralforge::appendNewAmpNativeParameters(p,spectralforge::firstOriginalAmpModel,spectralforge::niflheimrAmpModel);
    spectralforge::appendOriginalChannelParameters(p);
    spectralforge::appendNiflheimrParameters(p);
for(int i=1;i<=3;++i) {
    const auto n=juce::String(i);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"cabBtype"+n,6},"Mic B source "+n,juce::StringArray{"Filters only","V30 / SM57","Jensen / SM57","User IR"},0));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"cabblend"+n,6},"Mic A/B blend "+n,juce::NormalisableRange<float>{0.f,1.f},0.f));
    for(const auto* slot:{"A","B"}) {
        const auto id=juce::String("cab")+slot;
        // Preserve the existing linear host mapping, but make its unity point
        // exact. Fused multiply-add can otherwise turn the default 2/3 into
        // a small nonzero dB value on arm64 during project migration.
        juce::NormalisableRange<float> gainRange{-24.f,12.f,
            [](float start,float end,float normalised) {
                return normalised == (0.f-start)/(end-start) ? 0.f : start+(end-start)*normalised;
            },
            [](float start,float end,float value) {return (value-start)/(end-start);},
            [](float start,float end,float value) {
                // Keep the original 0.01 dB grid and clamping. Its multiply-add
                // can also leave a residue at unity; canonicalize only that bin.
                const juce::NormalisableRange<float> legacy{start,end,.01f};
                const float step=std::floor((value-start)/.01f+.5f);
                return step==std::floor((0.f-start)/.01f+.5f) ? 0.f : legacy.snapToLegalValue(value);
            }};
        gainRange.interval=.01f;
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id+"gain"+n,6},id+" level "+n,gainRange,0.f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id+"delay"+n,6},id+" delay ms "+n,juce::NormalisableRange<float>{0.f,20.f},0.f));
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id+"invert"+n,6},id+" polarity "+n,false));
    }
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"cabBlow"+n,6},"Mic B low cut "+n,juce::NormalisableRange<float>{20.f,500.f},70.f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"cabBhigh"+n,6},"Mic B high cut "+n,juce::NormalisableRange<float>{1500.f,20000.f},9000.f));
}
spectralforge::appendOriginalCabParameters(p);
spectralforge::appendCabExpansionParameters(p);
spectralforge::appendCabLayoutParameters(p);
spectralforge::appendGraphicalEQParameters(p);
// Append only: preserve every released host index and AU version ordering.
juce::StringArray tunerProfiles;for(const auto* name:spectralforge::tuning::profileNames)tunerProfiles.add(name);
p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"tunerProfile",11},"Tuner temperament",tunerProfiles,0));
p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"tunerDisplay",11},"Tuner display",juce::StringArray{"Strobe","Needle"},0));
p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"tunerString",11},"Tuner string (0 = auto)",0,8,0));
for(int i=0;i<8;++i){
    const auto suffix=juce::String(i+1);
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"tunerNote"+suffix,11},"Custom tuner string "+suffix+" MIDI note",12,96,spectralforge::tuning::openMidi[size_t(i)]));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"tunerOffset"+suffix,11},"Custom tuner string "+suffix+" cents",juce::NormalisableRange<float>{-50,50,.1f},0));
}
spectralforge::appendCommonDynamicsParameters(p);
// New independent dynamics append after the complete released parameter prefix.
spectralforge::appendMultibandParameters(p);
spectralforge::appendDeEsserParameters(p);
spectralforge::appendExtraGateParameters(p);
// Append-only; existing projects keep their cuts and normalization policy.
for(int lane=1;lane<=3;++lane)for(const auto* mic:{"A","B"})
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"cab"+juce::String(mic)+"cutsOff"+juce::String(lane),15},"Mic "+juce::String(mic)+" cuts bypass "+juce::String(lane),false));
// Room Core 1.3.3 (version hint 15, as the cut bypass) follows.
spectralforge::appendRoomParameters(p);
// 1.4 PRE Expansion A pedal banks (version 16).
spectralforge::appendPedalExpansionParameters(p);
spectralforge::appendMultibandStereoParameters(p);
spectralforge::appendSaturationParameters(p);
// 1.6 reference heads (models 26-30), version hint 20.
spectralforge::appendNewAmpNativeParameters(p,spectralforge::firstReferenceAmpModel,spectralforge::ampModelCount,spectralforge::referenceAmpParameterVersion);
// 1.7 two-filter modulation, version hint 21.
spectralforge::appendFilterModulationParameters(p);
// 1.7 POST expansion, version hint 22.
spectralforge::appendPostExpansionParameters(p);
// 1.8 POST motion/space: appended model extensions for DELAY / REVERB / MODULATION, version hint 23.
for(int family:{1,2,10})
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{spectralforge::effectExtensionID(family),spectralforge::effectExtensionParameterVersion},
        juce::String(spectralforge::modelFamilies[(size_t)family].category)+" model extension",0,15,0,juce::AudioParameterIntAttributes().withAutomatable(false)));
// Room Advanced 1.8 (version hint 24) and its spatial stage (25).
spectralforge::appendRoomAdvancedParameters(p);
spectralforge::appendRoomSpaceParameters(p);
// 1.9 CREATIVE slot, version hint 26 (24 and 25 belong to Room Advanced).
spectralforge::appendCreativeParameters(p);
// 1.10 SYNTH layer, version hint 27.
spectralforge::appendSynthParameters(p);
// External sidechain keys and the SYNTH output choice, version hint 28.
spectralforge::appendSidechainParameters(p);
// 1.4 signal order: tone / dynamics block and POST rack positions, version hint 29.
spectralforge::appendSignalOrderParameters(p);
// 1.9 expanded filter modulation, version hint 30.
spectralforge::appendFilterModulationExtensionParameters(p);
return p;
}

// One mapping from parameters to a cabinet, for the engine every block and
// for the offline copy that renders/exports a lane's cabinet.
void ChimeraProcessor::applyCabinetParameters(int lane,spectralforge::Cab& cab) const
{
    const auto get=[&](int id){return cabPanelParameters[lane][id]->load();};
    auto* b=cab.secondMic();
    cab.blend=spectralforge::cabMicRouting(originalCabParameters.values[lane][3]->load()>.5f,originalCabParameters.values[lane][8]->load()>.5f,int(laneParameters[lane][16]->load()),int(get(9)),get(0)).blend;
    cab.gainDb=get(1); cab.delayMs=get(2); cab.invert=get(3)>.5f;
    b->gainDb=get(4); b->delayMs=get(5); b->invert=get(6)>.5f;
    b->setCuts(get(7),get(8)); b->requestedSource.store(int(get(9)));
    cab.bypassCuts=get(10)>.5f;b->bypassCuts=get(11)>.5f;
}
juce::AudioBuffer<float> ChimeraProcessor::renderCabinet(int lane,const juce::AudioBuffer<float>& input,double sampleRate) const
{
    lane=juce::jlimit(0,2,lane);
    constexpr int block=512;
    const juce::dsp::ProcessSpec spec{sampleRate,(juce::uint32)block,2};
    std::array<spectralforge::Cab,3> cabs;
    spectralforge::IRLibrary copy{{&cabs[0],&cabs[1],&cabs[2]}};
    copy.restore(library.save()); // the project's own IRs, not the user folder
    std::array<int,3> sources{};
    for(int i=0;i<3;++i) {
        auto& cab=cabs[(size_t)i];
        if(i!=lane) {cab.requestedSource.store(0);cab.prepare(spec);continue;}
        cab.requestedSource.store((int)laneParameters[i][16]->load());
        cab.requestedModel.store(originalCabParameters.read(i,0));
        cab.secondMic()->requestedModel.store(originalCabParameters.read(i,1));
        applyCabinetParameters(i,cab);
        cab.enable(laneParameters[i][13]->load()>.5f);
        sources[(size_t)i]=cab.requestedSource.load();
        cab.prepare(spec); // snaps gain, blend and delay smoothers to the values above
        applyCabinetParameters(i,cab); // prepare resets filters; restore mic B cuts afterwards
    }
    copy.prepare(spec,sources); // builds every requested kernel before returning
    copy.requestStop();copy.stop();
    auto& cab=cabs[(size_t)lane];
    cab.setCuts(laneParameters[lane][14]->load(),laneParameters[lane][15]->load());
    juce::AudioBuffer<float> output(2,input.getNumSamples());output.clear();
    for(int c=0;c<2;++c)output.copyFrom(c,0,input,juce::jmin(c,input.getNumChannels()-1),0,input.getNumSamples());
    for(int offset=0;offset<output.getNumSamples();offset+=block) {
        const int count=juce::jmin(block,output.getNumSamples()-offset);
        std::array<float*,2> channels{output.getWritePointer(0,offset),output.getWritePointer(1,offset)};
        juce::AudioBuffer<float> part(channels.data(),2,count);
        cab.process(part);
    }
    for(auto& c:cabs)c.clear();
    return output;
}
juce::AudioBuffer<float> ChimeraProcessor::cabinetImpulseResponse(int lane,double sampleRate,double maximumSeconds) const
{
    juce::AudioBuffer<float> impulse(1,juce::jmax(1,int(sampleRate*maximumSeconds)));impulse.clear();impulse.setSample(0,0,1.f);
    auto ir=renderCabinet(lane,impulse,sampleRate);
    // Bound the SUM of discarded samples, not each individual sample: a
    // long quiet IIR tail can add coherently and move phase near a notch.
    // This also bounds the discarded DFT magnitude at every frequency.
    const float peak=juce::jmax(ir.getMagnitude(0,0,ir.getNumSamples()),ir.getMagnitude(1,0,ir.getNumSamples()));
    double remaining=double(peak)*1.0e-7;int length=ir.getNumSamples();
    while(length>int(sampleRate*.01)) {
        const double tail=juce::jmax(std::abs(ir.getSample(0,length-1)),std::abs(ir.getSample(1,length-1)));
        if(tail>remaining)break;
        remaining-=tail;--length;
    }
    ir.setSize(2,length,true);
    return ir;
}
juce::Result ChimeraProcessor::exportCabinetIR(int lane,const juce::File& file) const
{
    const double sampleRate=getSampleRate()>0 ? getSampleRate() : 48000.;
    const auto ir=cabinetImpulseResponse(lane,sampleRate);
    bool stereo=false;
    for(int n=0;n<ir.getNumSamples() && !stereo;++n)stereo=ir.getSample(0,n)!=ir.getSample(1,n);
    juce::AudioBuffer<float> out(stereo?2:1,ir.getNumSamples());
    for(int c=0;c<out.getNumChannels();++c)out.copyFrom(c,0,ir,c,0,ir.getNumSamples());
    if(file.isDirectory())return juce::Result::fail("The destination is a directory");
    juce::MemoryBlock encoded;
    {
        auto stream=std::make_unique<juce::MemoryOutputStream>(encoded,false);
        juce::StringPairArray metadata;
        metadata.set(juce::WavAudioFormat::bwavDescription,spectralforge::IRLibrary::cabinetExportTag);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),sampleRate,(unsigned)out.getNumChannels(),32,metadata,0));
        if(!writer)return juce::Result::fail("Cannot create a WAV writer");
        stream.release();
        if(!writer->writeFromAudioSampleBuffer(out,0,out.getNumSamples()) || !writer->flush())
            return juce::Result::fail("Cannot encode the IR samples");
    }
    return spectralforge::cabIRFile::writeAtomically(file,encoded);
}
void ChimeraProcessor::syncOriginalCabRequests()
{
    for(int i=0;i<3;++i) {
        engine.cabinet(i).requestedModel.store(originalCabParameters.read(i,0));
        engine.cabinet(i).secondMic()->requestedModel.store(originalCabParameters.read(i,1));
    }
}

juce::ValueTree ChimeraProcessor::captureCore()
{
    auto saved=state.copyState();
    saved.removeChild(saved.getChildWithName("USER_IRS"),nullptr);
    saved.removeChild(saved.getChildWithName("COMPARISONS"),nullptr);
    eqEditState.saveMetadata(saved);
    saved.appendChild(library.save(),nullptr); saved.setProperty("schemaVersion",10,nullptr);
    return saved;
}
void ChimeraProcessor::setTransposeQuality(int mode)
{
    // Message-thread setter: mirror into the audio-thread atomic and stamp the
    // persisted ValueTree property (captureCore/copyState serialises it).
    mode=juce::jlimit(0,2,mode);
    transposeQualityMode.store(mode);
    state.state.setProperty("transposeQuality",mode,nullptr);
}
void ChimeraProcessor::syncTransposeQuality()
{
    // After a state replace, keep the current mode unless the restored tree
    // carries one (an old project or a factory snapshot has none), then re-stamp
    // so the property stays present on the live tree for the next save.
    const int mode=juce::jlimit(0,2,(int)state.state.getProperty("transposeQuality",transposeQualityMode.load()));
    transposeQualityMode.store(mode);
    state.state.setProperty("transposeQuality",mode,nullptr);
}
void ChimeraProcessor::getStateInformation(juce::MemoryBlock& data)
{
    (void)tryGetStateInformation(data);
}
bool ChimeraProcessor::tryGetStateInformation(juce::MemoryBlock& data)
{
    auto saved=captureCore();const auto core=saved.createCopy();
    juce::ValueTree slots("COMPARISONS");
    std::lock_guard<std::mutex> lock(comparisonMutex);
    const int active=selectedComparison.load();
    slots.setProperty("active",active,nullptr);
      for(int i=0;i<2;++i) if(i==active || comparisons[(size_t)i].isValid()) {auto slot=(i==active ? saved : comparisons[(size_t)i]).createCopy();slot.setProperty("slot",i,nullptr);slots.appendChild(slot,nullptr);}
    saved.appendChild(slots,nullptr);
    juce::ValueTree midi("MIDI_MAP");for(int cc=0;cc<128;++cc) {const int index=midiMap[(size_t)cc].load();if(index<0 || index>=getParameters().size())continue;if(auto* parameter=dynamic_cast<juce::AudioProcessorParameterWithID*>(getParameters()[index])) {juce::ValueTree item("CC");item.setProperty("cc",cc,nullptr);item.setProperty("id",parameter->paramID,nullptr);midi.appendChild(item,nullptr);}}
    saved.appendChild(midi,nullptr);
    // SYNTH samples belong to the project, not to an A/B slot.
    saved.appendChild(spectralforge::synthSample::save(synth),nullptr);
    if(!spectralforge::irState::serialize(saved,data))return false;
    comparisons[(size_t)active]=core;
    return true;
}
void ChimeraProcessor::beginSwitch()
{
    // Only while audio is running on another thread: offline loads, the audio
    // thread itself and a stopped host change the state at once.
    const auto last=lastProcessTicks.load();
    if(last==0 || juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks()-last)>.2)return;
    if(audioThread.load(std::memory_order_relaxed)==std::hash<std::thread::id>{}(std::this_thread::get_id()))return;
    fadedSilent.store(false);fadeRequested.store(true);
    // Bounded: the old sound needs 5 ms of audio; a stalled host costs at most 30 ms.
    const auto deadline=juce::Time::getHighResolutionTicks()+juce::Time::secondsToHighResolutionTicks(.03);
    while(!fadedSilent.load() && juce::Time::getHighResolutionTicks()<deadline)juce::Thread::sleep(1);
}
void ChimeraProcessor::restoreCore(juce::ValueTree restored)
{
    ++headRecallRevision;
    if(!restored.isValid() || !restored.hasType("PARAMS")) return;
    beginSwitch(); // the old sound fades out (and stays muted) before any of the new state applies
    spectralforge::EQEditState::RestoreScope eqRestore(eqEditState,restored);
    boardUndo.clear();boardRedo.clear();orderUndo.clear();orderRedo.clear();midiLearn.store(-1);
    const spectralforge::ParamIndex restoredById(restored);
    const bool missingNative=!restoredById[spectralforge::ampNativeModelID(0)].isValid();
    const bool missingPost=!restoredById[spectralforge::postNativeModelID(0)].isValid();
    const bool missingBoard=!restoredById[spectralforge::pedalModelID(0)].isValid();
    const bool hasOriginalV1=restoredById[spectralforge::ampNativeControlID(0,spectralforge::firstOriginalAmpModel,0)].isValid();
    // Enumerate registered parameters via the current tree, but obtain every
    // missing value from the parameter definition, never from the session.
    const auto defaults=state.copyState();
    for(auto child:defaults)
    {
        const auto id=child.getProperty("id").toString();
        if(id.isEmpty() || restoredById[id].isValid()) continue;
        auto* parameter=state.getParameter(id);if(!parameter) continue;
        float value=parameter->convertFrom0to1(parameter->getDefaultValue());
        // Old single-channel snapshots retain their exact transfer and saved
        // knob values. New inactive channels start with the new response.
        if(hasOriginalV1 && id.startsWith("originalAmp_") && id.endsWith("_ch0_response"))value=0;
        if(hasOriginalV1 && id.startsWith("originalAmp_") && !id.contains("_ch"))for(size_t c=0;c<spectralforge::original::controlCount;++c)
            for(int context=0;context<spectralforge::ampNativeContextCount;++context)
                if(id==spectralforge::ampNativeControlID(context,spectralforge::firstOriginalAmpModel,int(c)))value=spectralforge::original::controls[c].initial;
        // Missing gateRangeDb uses its Full default, including old A/B slots.
        // Never inherit the current session's finite floor during migration.
        if(id.startsWith("cabtype") || id=="gateon" || id=="output" || id=="lowcomp" || id=="preorder" || id=="gainorder" || id=="boardEnabled") value=0;
        if((id.startsWith("nativeAmp_")&&id.endsWith("_enabled")) || (id.startsWith("pn_")&&id.endsWith("_native")))value=0;
        juce::ValueTree item("PARAM");item.setProperty("id",id,nullptr);item.setProperty("value",value,nullptr);restored.appendChild(item,nullptr);
    }
    library.restore(restored.getChildWithName("USER_IRS"));restored.removeChild(restored.getChildWithName("USER_IRS"),nullptr);
    restored.removeChild(restored.getChildWithName("COMPARISONS"),nullptr);state.replaceState(restored);syncOriginalCabRequests();syncTransposeQuality();switchPending.store(true);
    if(missingNative || missingPost || missingBoard)seedNativeSelections(missingBoard,missingNative,missingPost);
    // A stored order that is not a permutation (partial or edited state) is
    // replaced by the default, so the audio, the routing view and a fresh
    // instance all agree.
    writeOrder(signalOrder.read({}));
    for(int i=0;i<3;++i) { engine.cabinet(i).requestedSource.store((int)laneParameters[i][16]->load()); engine.cabinet(i).secondMic()->requestedSource.store(int(cabPanelParameters[i][9]->load())); }
    library.wake();
}
void ChimeraProcessor::setStateInformation(const void* data,int size)
{
    // Preserve the reader budget for both legacy and pooled projects.
    if(size<=0 || size>64*1024*1024) return;
    auto xml=getXmlFromBinary(data,size);if(!xml || !xml->hasTagName("PARAMS")) return;
    auto restored=juce::ValueTree::fromXml(*xml);
    if(!spectralforge::irState::unpack(restored))return;
    const auto saved=restored.getChildWithName("COMPARISONS");
    { std::lock_guard<std::mutex> lock(comparisonMutex);
      comparisons={};selectedComparison.store(juce::jlimit(0,1,(int)saved.getProperty("active",0)));levelMatch.forget();
      for(auto child:saved) {const int slot=(int)child.getProperty("slot",-1);if(slot>=0 && slot<2) comparisons[(size_t)slot]=child.createCopy();}
    }
    clearMidi();for(auto item:restored.getChildWithName("MIDI_MAP")) {const int cc=(int)item.getProperty("cc",-1);auto* parameter=state.getParameter(item.getProperty("id").toString());if(cc>=0 && cc<128 && parameter)midiMap[(size_t)cc].store(parameter->getParameterIndex());}
    restored.removeChild(restored.getChildWithName("MIDI_MAP"),nullptr);
    {auto loaded=spectralforge::synthSample::restore(restored.getChildWithName("SYNTH_SAMPLES"));
     for(int o=0;o<spectralforge::synthOscCount;++o)synth.swapSample(o,std::move(loaded[(size_t)o]));}
    restored.removeChild(restored.getChildWithName("SYNTH_SAMPLES"),nullptr);restoreCore(restored);
    warmInactiveComparison();
}
void ChimeraProcessor::selectComparison(int slot)
{
    slot=juce::jlimit(0,1,slot);if(slot==selectedComparison.load()) return;
    auto current=captureCore();juce::ValueTree next;
    { std::lock_guard<std::mutex> lock(comparisonMutex);
      comparisons[(size_t)selectedComparison.load()]=current.createCopy();
      if(!comparisons[(size_t)slot].isValid()) comparisons[(size_t)slot]=current.createCopy();
      next=comparisons[(size_t)slot].createCopy();selectedComparison.store(slot);
    }
    restoreCore(next);levelMatch.switched();
    warmInactiveComparison();
}
void ChimeraProcessor::copyComparison()
{
    eqEditState.barrier();
    auto current=captureCore();
    { std::lock_guard<std::mutex> lock(comparisonMutex);
      comparisons[(size_t)(1-selectedComparison.load())]=current.createCopy();levelMatch.copied(selectedComparison.load()); }
    warmInactiveComparison();
}
void ChimeraProcessor::warmInactiveComparison()
{
    // The other A/B slot is the likeliest next recall: keep its modeled
    // cabinets generated so switching to it installs them at once.
    juce::ValueTree other;
    { std::lock_guard<std::mutex> lock(comparisonMutex); other=comparisons[(size_t)(1-selectedComparison.load())]; }
    std::array<uint64_t,6> models{};
    if(other.isValid()) {
        // Missing values take the parameter default, as restoreCore does.
        const spectralforge::ParamIndex saved(other);
        const auto get=[&](const juce::String& id) {
            if(const auto node=saved[id];node.isValid()) return float(node.getProperty("value"));
            auto* parameter=state.getParameter(id);
            return parameter ? parameter->convertFrom0to1(parameter->getDefaultValue()) : 0.f;
        };
        for(int lane=0;lane<3;++lane)for(int mic=0;mic<2;++mic)
            models[size_t(lane+3*mic)]=spectralforge::OriginalCabParameters::savedKey(lane,mic,get);
    }
    library.keepWarm(models);
}
juce::AudioProcessorEditor* ChimeraProcessor::createEditor() {
    auto* editor=new ChimeraEditor(*this);
    editor->setEasyMode(spectralforge::easy::Preference::startsEasy());
    return editor;
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChimeraProcessor(); }

void ChimeraProcessor::clearMidi() {for(auto& target:midiMap)target.store(-1);midiLearn.store(-1);}
void ChimeraProcessor::learnMidi(const juce::String& id) {if(auto* parameter=state.getParameter(id))midiLearn.store(parameter->getParameterIndex());}
void ChimeraProcessor::tapTempo() {
    const double now=juce::Time::getMillisecondCounterHiRes(),interval=now-lastTap;lastTap=now;
    if(interval<250 || interval>1500) {tapCount=0;restartClick.store(true);return;}
    tapIntervals[(size_t)(tapCount++%4)]=interval;double sum=0;const int count=juce::jmin(4,tapCount);for(int i=0;i<count;++i)sum+=tapIntervals[(size_t)i];
    auto* parameter=state.getParameter("tempo");parameter->setValueNotifyingHost(parameter->convertTo0to1(float(60000/(sum/count))));state.getParameter("temposync")->setValueNotifyingHost(0);restartClick.store(true);
}
void ChimeraProcessor::loadFactoryPreset(int index) {
    if(index<0 || index>=spectralforge::selectablePresetCount)return;
    levelMatch.invalidate(selectedComparison.load());
    ++headRecallRevision;
    beginSwitch(); // the old sound fades out (and stays muted) while the snapshot is built
    {
        const auto snapshot=spectralforge::isNiflheimrPreset(index)
            ? spectralforge::niflheimrPresetSnapshot(state,index-spectralforge::niflheimrPresetStart)
            : spectralforge::isOriginalPreset(index)
            ? spectralforge::originalPresetSnapshot(state,index-spectralforge::originalPresetStart)
            : spectralforge::isGuitarSignature(index)
            ? spectralforge::guitarSignatureSnapshot(state,index-spectralforge::factoryPresetCount)
            : spectralforge::factoryNativeSnapshot(state,index);
        spectralforge::EQEditState::RestoreScope eqRestore(eqEditState,snapshot,true);
        state.replaceState(snapshot);
        syncTransposeQuality();
        // Canonicalize host values as well as APVTS values. Bool/choice adapters
        // may already cache the snapped value of a noncanonical host write and
        // suppress an otherwise identical replaceState update.
        const spectralforge::ParamIndex snapshotById(snapshot);
        for(auto* raw:getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(raw)) {
            const auto node=snapshotById[parameter->paramID];
            parameter->setValueNotifyingHost(parameter->convertTo0to1(float(node.getProperty("value"))));
        }
        boardUndo.clear();boardRedo.clear();orderUndo.clear();orderRedo.clear();
        // Factory recipes own their modeled CAB. Resolving a user's IR folder
        // here would bypass that cabinet and make recall machine-dependent.
        switchPending.store(true);
    }
}

int ChimeraProcessor::ampContext(int lane) const noexcept {
    return spectralforge::ampNativeContext((int)globals[mode]->load(),lane);
}
int ChimeraProcessor::selectedAmpModel(int lane) const noexcept {
    const auto native=nativeAmps.read(ampContext(lane));
    return native.enabled?native.model:ampSelection.model(lane);
}
int ChimeraProcessor::selectedAmpChannel(int lane) const noexcept {
    const auto native=nativeAmps.read(ampContext(lane));
    if(native.enabled)return native.channel;
    const int model=selectedAmpModel(lane);
    return nativeSelectionFromLegacy(model,ampSelection.channel(lane,model)).first;
}
int ChimeraProcessor::selectedAmpNativeRoute(int lane) const noexcept {
    const int context=ampContext(lane);const auto native=nativeAmps.read(context);
    if(native.enabled)return native.inputRoute;
    const int model=ampSelection.model(lane);
    if(model==19)return nativeSelectionFromLegacy(model,ampSelection.channel(lane,model)).second;
    return juce::jlimit(0,(int)spectralforge::ampNativePanel(model).routes.size()-1,
        juce::roundToInt(spectralforge::AmpNativeParameterCache::value(nativeAmps.routes[(size_t)context][(size_t)model],0.f)));
}
void ChimeraProcessor::activateNativeAmp(int lane) {
    if(lane<0 || lane>=3)return;
    const int context=ampContext(lane);const auto native=nativeAmps.read(context);
    if(native.enabled) {
        if(spectralforge::ampRequiresNative(native.model))setRawParameter(spectralforge::ampNativeEnabledID(context),1.f);
        return;
    }
    const int model=ampSelection.model(lane);const auto selection=nativeSelectionFromLegacy(model,ampSelection.channel(lane,model));
    setRawParameter(spectralforge::ampNativeModelID(context),float(model));
    setRawParameter(spectralforge::ampNativeChannelID(context,model),float(selection.first));
    if(model>=spectralforge::legacyAmpModelCount)setRawParameter(spectralforge::ampNativeRouteID(context,model),float(selection.second));
    setRawParameter(spectralforge::ampNativeEnabledID(context),1.f);
}
void ChimeraProcessor::activateNativePost(int section) {
    if(section>=0 && section<3)setRawParameter(spectralforge::postNativeModeID(section),1.f);
}
void ChimeraProcessor::seedNativeSelections(bool seedBoard,bool seedAmps,bool seedPost) {
    // Populate the new presentation from released selections. Old projects keep
    // their audio until an explicit edit; native banks remain independently saved.
    for(int context=0;seedAmps && context<spectralforge::ampNativeContextCount;++context) {
        const int lane=context==0?0:context<3?context-1:context-3;
        const int model=ampSelection.model(lane);
        setRawParameter(spectralforge::ampNativeEnabledID(context),0.f);
        setRawParameter(spectralforge::ampNativeModelID(context),float(model));
        const auto selection=nativeSelectionFromLegacy(model,ampSelection.channel(lane,model));
        setRawParameter(spectralforge::ampNativeChannelID(context,model),float(selection.first));
        setRawParameter(spectralforge::ampNativeRouteID(context,model),float(selection.second));
    }
    constexpr const char* postOn[]{"buscompon","preampon","eqon"};
    for(int s=0;seedPost && s<3;++s) {
        const int model=juce::jlimit(0,2,(int)modelParameters[(size_t)(7+s)]->load());
        setRawParameter(spectralforge::postNativeModeID(s),0.f);
        setRawParameter(spectralforge::postNativeModelID(s),float(model));
        setRawParameter(spectralforge::postNativeModelExtensionID(s),0.f);
        setRawParameter(spectralforge::postNativeBypassID(s,model),state.getRawParameterValue(postOn[s])->load()>.5f?0.f:1.f);
    }
    if(!seedBoard)return;
    constexpr int families[]{3,4,5,6,0},first[]{6,11,16,21,1};
    constexpr const char* on[]{"precompon","filteron","fuzzon","booston","preon"};
    for(int owner=0;owner<5;++owner) {
        const int model=first[owner]+juce::jlimit(0,4,(int)modelParameters[(size_t)families[owner]]->load());
        setRawParameter(spectralforge::pedalModelID(owner),float(model));
        setRawParameter(spectralforge::pedalBypassID(owner,model),state.getRawParameterValue(on[owner])->load()>.5f?0.f:1.f);
        setRawParameter(spectralforge::pedalOrderID(owner),float(owner));
    }
    if(preOrderParameter->load()>.5f) {setRawParameter(spectralforge::pedalOrderID(0),1);setRawParameter(spectralforge::pedalOrderID(1),0);}
    if(gainOrderParameter->load()>.5f) {setRawParameter(spectralforge::pedalOrderID(3),4);setRawParameter(spectralforge::pedalOrderID(4),3);}
}
void ChimeraProcessor::setAmpModel(int lane,int model) {
    if(lane<0 || lane>=3 || model<0 || model>=spectralforge::ampModelCount)return;
    setRawParameter(spectralforge::ampNativeModelID(ampContext(lane)),float(model));
    setRawParameter(spectralforge::ampNativeEnabledID(ampContext(lane)),1.f);
    if(model<spectralforge::legacyAmpModelCount) {
        // Set the legacy value before deactivating the extension bank. An old
        // host automation write alone never deactivates an explicit new model.
        setRawParameter("amp"+juce::String(lane+1),float(model));
        setRawParameter(spectralforge::ampExtensionID(lane),0.f);
    } else if(model<spectralforge::releasedNativeAmpModelCount) {
        setRawParameter(spectralforge::ampExtensionID(lane),float(model-spectralforge::legacyAmpModelCount+1));
    }
}
void ChimeraProcessor::setAmpChannel(int lane,int channel) {
    if(lane<0 || lane>=3)return;
    const int model=selectedAmpModel(lane);
    if(channel<0 || channel>=(int)spectralforge::ampNativePanel(model).channels.size())return;
    activateNativeAmp(lane);
    setRawParameter(spectralforge::ampNativeChannelID(ampContext(lane),model),float(channel));
    if(model==19)setRawParameter(spectralforge::ampChannelID(lane,model),float(selectedAmpNativeRoute(lane)));
    else if(model==21) {if(channel==1)setRawParameter(spectralforge::ampChannelID(lane,model),0.f);}
    else if(spectralforge::isNewAmpModel(model) && channel<spectralforge::newAmpChannelCount(model))setRawParameter(spectralforge::ampChannelID(lane,model),float(channel));
}
void ChimeraProcessor::setAmpNativeRoute(int lane,int route) {
    if(lane<0 || lane>=3)return;
    const int model=selectedAmpModel(lane);
    if(route<0 || route>=(int)spectralforge::ampNativePanel(model).routes.size())return;
    activateNativeAmp(lane);setRawParameter(spectralforge::ampNativeRouteID(ampContext(lane),model),float(route));
    if(model==19)setRawParameter(spectralforge::ampChannelID(lane,model),float(route));
}
void ChimeraProcessor::resetAmpSelection() {
    for(int lane=0;lane<3;++lane) {
        setRawParameter(spectralforge::ampExtensionID(lane),0.f);
        for(int bank=0;bank<spectralforge::extendedAmpBankCount;++bank) {
            const int model=spectralforge::legacyAmpModelCount+bank;
            setRawParameter(spectralforge::ampChannelID(lane,model),float(spectralforge::newAmpDefaultChannel(model)));
        }
    }
}
void ChimeraProcessor::setRawParameter(const juce::String& id,float value) {
    if(auto* parameter=state.getParameter(id)) {
        parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(value));parameter->endChangeGesture();
    }
}
void ChimeraProcessor::rememberPedalEdit() {
    juce::ValueTree snapshot("BOARD_EDIT");
    for(auto child:state.copyState()) if(child.getProperty("id").toString().startsWith("board"))snapshot.appendChild(child.createCopy(),nullptr);
    boardUndo.push_back(snapshot);if(boardUndo.size()>32)boardUndo.erase(boardUndo.begin());boardRedo.clear();
}
void ChimeraProcessor::setPedalBoardEnabled(bool enabled) {
    if(boardParameters.read().enabled==enabled)return;
    rememberPedalEdit();beginSwitch();boardEditSequence.fetch_add(1,std::memory_order_acq_rel);
    setRawParameter("boardEnabled",enabled?1.f:0.f);
    boardEditSequence.fetch_add(1,std::memory_order_release);switchPending.store(true);
}
void ChimeraProcessor::setPedalModel(int owner,int model) {
    if(owner<0 || owner>=5 || model<0 || model>=spectralforge::pedalModelCount || !spectralforge::pedalModel(model).implemented)return;
    rememberPedalEdit();boardEditSequence.fetch_add(1,std::memory_order_acq_rel);
    midiLearn.store(-1);
    // Retire controller bindings for this owner when an explicit replacement is made.
    const auto prefix="board"+juce::String(owner)+"_";
    for(auto& binding:midiMap) {
        const auto index=binding.load();
        if(index>=0 && index<getParameters().size()) if(auto* p=dynamic_cast<juce::AudioProcessorParameterWithID*>(getParameters()[index]))
            if(p->paramID.startsWith(prefix))binding.store(-1);
    }
    setRawParameter(spectralforge::pedalModelID(owner),float(model));
    const auto usedKey="boardBankUsed_"+juce::String(owner)+"_"+juce::String(model);
    const bool used=(bool)state.state.getProperty(usedKey,false);
    if(model)state.state.setProperty(usedKey,true,nullptr);
    if(model)setRawParameter(spectralforge::pedalBypassID(owner,model),0);
    for(int c=0;!used && c<spectralforge::pedalModel(model).controlCount;++c)
        setRawParameter(spectralforge::pedalControlID(owner,model,c),spectralforge::pedalModel(model).controls[(size_t)c].initial);
    setRawParameter("boardEnabled",1);
    boardEditSequence.fetch_add(1,std::memory_order_release);
}
void ChimeraProcessor::movePedal(int owner,int direction) {
    const auto board=boardParameters.read();int position=-1;
    for(int i=0;i<5;++i)if(board.order[(size_t)i]==owner)position=i;
    const int destination=position+direction;
    if(position<0 || destination<0 || destination>=5)return;
    rememberPedalEdit();boardEditSequence.fetch_add(1,std::memory_order_acq_rel);
    setRawParameter(spectralforge::pedalOrderID(position),float(board.order[(size_t)destination]));
    setRawParameter(spectralforge::pedalOrderID(destination),float(owner));
    setRawParameter("boardEnabled",1);
    boardEditSequence.fetch_add(1,std::memory_order_release);
}
bool ChimeraProcessor::duplicatePedal(int owner) {
    if(owner<0 || owner>=5)return false;
    const auto board=boardParameters.read();const auto source=board.instances[(size_t)owner];if(!source.model)return false;
    int target=-1;for(int i:board.order)if(!board.instances[(size_t)i].model) {target=i;break;}
    if(target<0)return false;
    rememberPedalEdit();boardEditSequence.fetch_add(1,std::memory_order_acq_rel);
    midiLearn.store(-1);
    const auto prefix="board"+juce::String(target)+"_";
    for(auto& binding:midiMap) {const int index=binding.load();if(index>=0&&index<getParameters().size())
        if(auto* p=dynamic_cast<juce::AudioProcessorParameterWithID*>(getParameters()[index]))if(p->paramID.startsWith(prefix))binding.store(-1);}
    setRawParameter(spectralforge::pedalModelID(target),float(source.model));
    state.state.setProperty("boardBankUsed_"+juce::String(target)+"_"+juce::String(source.model),true,nullptr);
    setRawParameter(spectralforge::pedalBypassID(target,source.model),source.bypass?1.f:0.f);
    for(int c=0;c<spectralforge::pedalModel(source.model).controlCount;++c)
        setRawParameter(spectralforge::pedalControlID(target,source.model,c),source.controls[(size_t)c]);
    setRawParameter("boardEnabled",1);
    boardEditSequence.fetch_add(1,std::memory_order_release);return true;
}
void ChimeraProcessor::undoPedalEdit(bool redo) {
    auto& from=redo ? boardRedo : boardUndo;auto& to=redo ? boardUndo : boardRedo;if(from.empty())return;
    juce::ValueTree current("BOARD_EDIT");
    for(auto child:state.copyState())if(child.getProperty("id").toString().startsWith("board"))current.appendChild(child.createCopy(),nullptr);
    to.push_back(current);const auto next=from.back();from.pop_back();boardEditSequence.fetch_add(1,std::memory_order_acq_rel);
    for(auto child:next)setRawParameter(child.getProperty("id").toString(),float(child.getProperty("value")));
    boardEditSequence.fetch_add(1,std::memory_order_release);
}

void ChimeraProcessor::measureStage(int index,const juce::AudioBuffer<float>& buffer) {
    double energy=0;float peak=0;const int count=buffer.getNumSamples()*buffer.getNumChannels();
    for(int c=0;c<buffer.getNumChannels();++c)for(int n=0;n<buffer.getNumSamples();++n) {
        const float v=buffer.getSample(c,n);energy+=double(v)*v;peak=juce::jmax(peak,std::abs(v));
    }
    stageRms[(size_t)index].store(count>0 ? float(std::sqrt(energy/count)) : 0.f);stagePeaks[(size_t)index].store(peak);
}
juce::String ChimeraProcessor::diagnosticReport() const {
    auto object=std::make_unique<juce::DynamicObject>();
    object->setProperty("schema",1);object->setProperty("version",spectralforge::release::version);
    object->setProperty("build_revision",spectralforge::release::revision);
    object->setProperty("sample_rate",getSampleRate());object->setProperty("host_block_hint",getBlockSize());
    object->setProperty("channels",getMainBusNumOutputChannels());object->setProperty("reported_latency_samples",getLatencySamples());
    object->setProperty("mode",int(globals[mode]->load()));object->setProperty("universal_board",boardParameters.enabled->load()>.5f);
    object->setProperty("transpose_enabled",globals[pitchOn]->load()>.5f);object->setProperty("semitones",int(globals[semitones]->load()));
    object->setProperty("oversampling_index",int(globals[os]->load()));object->setProperty("output_trim_db",globals[output]->load());
    object->setProperty("post_rig_gate",gateAfterRig->load()>.5f);object->setProperty("callback_average_percent",cpuAverage.load());
    object->setProperty("callback_peak_percent",cpuPeak.load());
    object->setProperty("tone_eq_average_percent",toneEQ.average.load());object->setProperty("tone_eq_peak_percent",toneEQ.peak.load());
    object->setProperty("final_eq_average_percent",finalEQ.average.load());object->setProperty("final_eq_peak_percent",finalEQ.peak.load());object->setProperty("measurements","Last processed block, unweighted RMS/peak; not LUFS or whole-PC CPU. No audio is included.");
    juce::Array<juce::var> stages;const char* names[]{"INPUT_TRIM","PRE","RIG_GATE","POST","OUTPUT"};
    for(int i=0;i<5;++i) {auto stage=std::make_unique<juce::DynamicObject>();stage->setProperty("stage",names[i]);
        stage->setProperty("rms_dbfs",juce::Decibels::gainToDecibels(stageRms[(size_t)i].load(),-120.f));
        stage->setProperty("peak_dbfs",juce::Decibels::gainToDecibels(stagePeaks[(size_t)i].load(),-120.f));stages.add(juce::var(stage.release()));}
    object->setProperty("stages",stages);
   #if defined(JUCE_ASIO) && JUCE_ASIO
    object->setProperty("asio_built",true);
   #else
    object->setProperty("asio_built",false);
   #endif
    if(spectralforge::standalone::audioDeviceDiagnostics) {
        auto device=spectralforge::standalone::audioDeviceDiagnostics();
        // Driver claim + Chimera's reported delay; not a measured loopback round trip.
        spectralforge::standalone::addReportedLatency(device,getLatencySamples());
        object->setProperty("audio_device",device);
    }
    return juce::JSON::toString(juce::var(object.release()),false);
}
