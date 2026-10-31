#include "CabMicRouting.h"
#pragma once
#include "OriginalCabParameters.h"
#include "KeyDelay.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <thread>
#include "DynamicsAnalysis.h"
#include "AutoSet.h"
#include "MultibandDynamics.h"
#include "ThreeBandSaturation.h"
#include "FilterModulation.h"
#include "CreativeFX.h"
#include "SynthLayer.h"
#include "Sidechain.h"
#include "SignalOrderParameters.h"
#include "DeEsser.h"
#include "ChimeraDSP.h"
#include "FXParameters.h"
#include "IRLibrary.h"
#include "PerformanceUtilities.h"
#include "ABLevelMatch.h"
#include "ParamIndex.h"
#include "PedalBoardParameters.h"
#include "PedalBoardDSP.h"
#include "PostRigGate.h"
#include "ExtraGates.h"
#include "RoomCore.h"
#include "AmpSelectionParameters.h"
#include "AmpNativeParameters.h"
#include "PostNativeParameters.h"
#include "GraphicalEQ.h"
#include "BetaLifecycleRuntime.h"
#include "TesterCode.h"
#include "EQEditState.h"
#include "EQCaptureBoundary.h"

#include "AAXIdentity.h"
#include "HostBypass.h"

class ChimeraProcessor : public juce::AudioProcessor, private juce::VST3ClientExtensions {
public:
    ChimeraProcessor();
    ~ChimeraProcessor() override;
    juce::VST3ClientExtensions* getVST3ClientExtensions() override { return this; }
    void setIHostApplication(Steinberg::FUnknown*) override;
#if CHIMERA_ROUTING_DIAGNOSTICS
    std::atomic<int> routingGateObserved{-1};
    std::atomic<uint64_t> routingBlocks{0};
#endif
    spectralforge::beta::Evaluation betaStatus() const noexcept { return betaLifecycle.observe(betaClock()); }
    bool betaProcessingPermitted() const noexcept { return betaLifecycle.processingPermitted(); }
    juce::String betaNotice() const;
    void recheckBetaClock();
    // Message thread. A verified beta tester code lifts the expiry for this
    // user on this computer (all later betas signed with the same key).
    spectralforge::beta::tester::Grant applyTesterCode(const juce::String& code);
    void removeTesterCode();
    const juce::String& testerCodeHolder() const noexcept { return testerHolder; }
    void prepareToPlay(double,int) override;
    void releaseResources() override;
    // Diagnostic/test query. Host audio processing must already be stopped.
    bool backgroundResourcesReleased() const noexcept { return tuner.stopped() && library.resourcesReleased(); }
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AAXClientExtensions& getAAXClientExtensions() override {
        static spectralforge::AAXIdentity identity;
        return identity;
    }
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    // Hosts call this to drop tails without re-preparing (VST3 setProcessing(false),
    // AU Reset, a re-enabled OBS filter). It may arrive on the audio thread, so it
    // only flags the existing reset that the next processed block performs.
    void reset() override { resetPending.store(true); }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SpectralForge Chimera"; }
    bool acceptsMidi() const override { return true; }
    // 1.10 SYNTH: the tracked guitar pitch (Hz, 0 when none), for the panel readout.
    double synthTrackedFrequency() const noexcept { return synthTracked.load(std::memory_order_relaxed); }
    // 1.4 signal order (docs/SIGNAL_ORDER_1_4.md). Message thread. Each edit
    // writes the slot parameters (one host gesture each) and can be undone.
    spectralforge::SignalOrder requestedOrder() const noexcept { return signalOrder.read({}); }
    bool placeModule(spectralforge::OrderRegion region,int module,int position);
    bool moveModule(spectralforge::OrderRegion region,int module,int delta);
    bool resetOrder(spectralforge::OrderRegion region);
    bool applyOrderPreset(spectralforge::OrderRegion region,int preset);
    bool undoOrder();bool redoOrder();
    bool canUndoOrder() const noexcept {return !orderUndo.empty();}
    bool canRedoOrder() const noexcept {return !orderRedo.empty();}
    // Whether the host has enabled the optional buses.
    bool sidechainEnabled() const { return getBusCount(true)>1 && getBus(true,1)->isEnabled(); }
    bool synthBusEnabled() const { return getBusCount(false)>1 && getBus(false,1)->isEnabled(); }
    // The synth engine, read by the SYNTH page (scope, LFO and tracking readouts).
    const spectralforge::SynthLayer& synthEngine() const noexcept { return synth; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override {
        double tail=1.1;
        if(fxParameters[4] && fxParameters[4]->load()>.5f) tail+=(fxParameters[48]->load()>.5f ? 1.5 : fxParameters[5]->load()*.001)*std::log(.001)/std::log(juce::jlimit(.0001f,.85f,fxParameters[6]->load()));
        if(fxParameters[8] && fxParameters[8]->load()>.5f) tail+=12.0;
        return tail+toneEQ.tailSeconds()+finalEQ.tailSeconds()+room.tailSeconds();
    }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    bool tryGetStateInformation(juce::MemoryBlock&);
    void setStateInformation(const void*,int) override;
    juce::AudioProcessorValueTreeState& parameters() { return state; }
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    juce::Result loadIR(int lane,const juce::File& file);
    juce::Result loadMicIR(int lane,int slot,const juce::File& file);
    // SYNTH sample oscillators (docs/SYNTH_V2.md). Message thread. Loading
    // switches the oscillator on in Sample mode and sets its root to the
    // detected pitch (one host gesture each); the sample is saved in the state.
    juce::Result loadSynthSample(int osc,const juce::File& file);
    void clearSynthSample(int osc);
    // Message thread. Renders `input` through a copy of a lane's cabinet as it
    // is now set up (modeled or captured IRs, mic A/B blend, gain, delay and
    // polarity, low/high cuts, cabinet on/off) without touching the running
    // engine. With an impulse as input, the result is the cabinet's IR.
    juce::AudioBuffer<float> renderCabinet(int lane,const juce::AudioBuffer<float>& input,double sampleRate) const;
    juce::AudioBuffer<float> cabinetImpulseResponse(int lane,double sampleRate,double maximumSeconds=2.0) const;
    // Writes the lane's cabinet IR as a 32-bit float WAV at the session rate
    // (48 kHz when none), mono when both channels match.
    juce::Result exportCabinetIR(int lane,const juce::File& file) const;
    juce::String micName(int lane,int slot) const { return library.userName(lane+3*slot); }
    spectralforge::CabMicRouting micRouting(int lane) const {
        const auto n=juce::String(lane+1);
        return spectralforge::cabMicRouting(
            state.getRawParameterValue(spectralforge::originalCabID(lane,"Aon"))->load()>.5f,
            state.getRawParameterValue(spectralforge::originalCabID(lane,"Bon"))->load()>.5f,
            int(state.getRawParameterValue("cabtype"+n)->load()),int(state.getRawParameterValue("cabBtype"+n)->load()),
            state.getRawParameterValue("cabblend"+n)->load());
    }
    juce::String micStatus(int lane,int slot) const {
        const auto route=micRouting(lane);if(slot?route.offB:route.offA)return "MIC OFF | No fallback filters";
        const auto model=originalCabParameters.read(lane,slot);
        const int source=(int)state.getRawParameterValue((slot ? "cabBtype" : "cabtype")+juce::String(lane+1))->load();
        const auto selected=uint64_t(source+1) | (model<<4);
        if(library.displayRevision(lane+3*slot)[1]!=selected)
            return juce::String(model ? "Original model selected" : source ? "Captured IR selected" : "Filters only selected")
                +" | Awaiting audio processing";
        return library.status(lane+3*slot);
    }
    spectralforge::IRMetadata micMetadata(int lane,int slot) const {
        const int source=(int)state.getRawParameterValue((slot ? "cabBtype" : "cabtype")+juce::String(lane+1))->load();
        return library.metadata(lane+3*slot,source);
    }
    // The capture chooser describes the retained file even while its slot uses
    // an original response. Never infer a capture identity from modeled metadata.
    spectralforge::IRMetadata micCaptureMetadata(int lane,int slot) const {
        const int source=(int)state.getRawParameterValue((slot ? "cabBtype" : "cabtype")+juce::String(lane+1))->load();
        return library.metadata(lane+3*slot,source,false);
    }
    std::array<uint64_t,4> micDisplayRevision(int lane,int slot) const noexcept {return library.displayRevision(lane+3*slot);}
    juce::String userIRName(int lane) const {return library.userName(lane);}
    float preCompressorReduction() const {return preReduction.load();}
    float postCompressorReduction() const {return postReduction.load();}
    float postModuleLevel(int position) const {return postPeaks[(size_t)juce::jlimit(0,5,position)].load();}
    float postNativeMeter(int section) const {return nativePostMeters[(size_t)juce::jlimit(0,2,section)].load();}
    juce::String cabStatus(int lane) const { return library.status(lane); }
    spectralforge::IRMetadata cabMetadata(int lane) const { return library.metadata(lane,(int)state.getRawParameterValue("cabtype"+juce::String(lane+1))->load()); }
    std::array<uint64_t,4> cabDisplayRevision(int lane) const noexcept {return library.displayRevision(lane);}
    void setCabMetadata(int lane,const spectralforge::IRMetadata& metadata) { library.setMetadata(lane,metadata); }
    float inputMeter() const { return inputPeak.load(); }
    float outputMeter() const { return outputPeak.load(); }
    spectralforge::GraphicalEQ& graphicalEQ(int instance) noexcept {return instance==0 ? toneEQ : finalEQ;}
    spectralforge::CommonDynamics& commonDynamics() noexcept {return dynamics;}
    spectralforge::DynamicsAnalysis& dynamicsAnalysis() noexcept {return dynamicsAnalysisState;}
    spectralforge::autoset::Capture& autoSetCapture() noexcept {return autoSetCaptureState;}
    // Message thread: Auto Set holds all gates open while it listens to the rig's noise.
    void setAutoSetGateBypass(bool bypass) noexcept {autoSetGateBypass.store(bypass,std::memory_order_relaxed);}
    // The native amp settings a routing context plays with (Auto Set renders candidates offline).
    spectralforge::AmpNativeState autoSetAmpState(int context) const noexcept {return nativeAmps.read(context);}
    spectralforge::MultibandDynamics& multibandDynamics() noexcept {return multiband;}
    spectralforge::DeEsser& deEsser() noexcept {return deesser;}
    float pedalCompressorReduction(int owner) const noexcept {return pedalBoard.compressorReduction(owner);}
    spectralforge::EQEditState& eqEditing() noexcept {return eqEditState;}
    spectralforge::EQCaptureBoundary& eqCaptureBoundary() noexcept {return eqCaptureBoundaryState;}
    float cpuLoad() const { return cpuAverage.load(); }
    float cpuPeakLoad() const { return cpuPeak.load(); }
    float lowCompMeter() const {return lowCompGain.load();}
    float gateMeter() const { return gateGain.load(); }
    // Linear gain of Gate 2 (unit 1) or the Expander (unit 2); unit 0 is the main gate.
    float gateUnitMeter(int unit) const { return unit==1 ? gate2Gain.load() : unit==2 ? expanderGain.load() : gateGain.load(); }
    float tuningFrequency() const { return tuner.frequency(); }
    float tuningConfidence() const { return tuner.confidence(); }
    void selectComparison(int slot);
    void copyComparison();
    int comparisonSlot() const {return selectedComparison.load();}
    // A/B comparison gain on the output, never saved (see ABLevelMatch.h).
    spectralforge::ABLevelMatch& abLevelMatch() noexcept {return levelMatch;}
    const spectralforge::ABLevelMatch& abLevelMatch() const noexcept {return levelMatch;}
    void loadFactoryPreset(int index);
    // Presentation invalidation only; never serialized or read by the DSP.
    uint64_t ampHeadRecallRevision() const noexcept {return headRecallRevision.load();}
    void learnMidi(const juce::String& parameterId);
    void clearMidi();
    bool learningMidi() const {return midiLearn.load()>=0;}
    float currentTempo() const {return tempoMeter.load();}
    void tapTempo();
    int pitchLatency() const { return preFX.transpose.latency(); }
    // Transpose quality/latency mode (0 Classic, 1 Low-Latency, 2 Tight). Stored
    // as a ValueTree property, not a host parameter, so it is not automatable and
    // does not enter the frozen parameter inventory; the audio thread reads the
    // atomic mirror.
    void setTransposeQuality(int mode);
    // Room Core diagnostics (tests): samples run through the room and the
    // last block's send-bus peak.
    uint64_t roomProcessedSamples() const noexcept { return room.processedSamples(); }
    float roomInputPeak() const noexcept { return room.lastInputPeak(); }
    int transposeQuality() const { return transposeQualityMode.load(); }
    spectralforge::PedalBoardState pedalBoardState() const { return boardParameters.read(); }
    void setPedalBoardEnabled(bool enabled);
    int pedalBoardLatencySamples()const{return pedalBoard.latency(boardParameters.read());}
    void setPedalModel(int owner,int model);
    void movePedal(int owner,int direction);
    bool duplicatePedal(int owner);
    void undoPedalEdit(bool redo=false);
    float pedalReduction() const { return boardReduction.load(); }
    juce::String diagnosticReport() const;
    int selectedAmpModel(int lane) const noexcept;
    int selectedAmpChannel(int lane) const noexcept;
    int selectedAmpNativeRoute(int lane) const noexcept;
    void setAmpModel(int lane,int model);
    void setAmpChannel(int lane,int channel);
    void setAmpNativeRoute(int lane,int route);
    void activateNativeAmp(int lane);
    void activateNativePost(int section);
private:
    friend struct ChimeraBetaTestAccess;
    friend struct ChimeraCabExportTestAccess;
    ChimeraProcessor(const spectralforge::beta::Metadata&, spectralforge::beta::Clock,
                     const spectralforge::beta::tester::Grant& tester={});
    const spectralforge::beta::Clock betaClock;
    spectralforge::beta::InstanceLatch betaLifecycle;
    juce::String testerHolder; // message thread only
    juce::SmoothedValue<float> betaOutputGain;
    spectralforge::AmpSelectionParameterCache ampSelection;
    spectralforge::AmpNativeParameterCache nativeAmps;
    spectralforge::PostNativeParameterCache nativePost;
    int ampContext(int lane) const noexcept;
    void seedNativeSelections(bool seedBoard,bool seedAmps=true,bool seedPost=true);
    void resetAmpSelection();
    void rememberPedalEdit();
    void setRawParameter(const juce::String&,float);
    std::vector<juce::ValueTree> boardUndo,boardRedo;
    std::atomic<unsigned> boardEditSequence{0};
    spectralforge::PedalBoardParameterCache boardParameters;
    spectralforge::PedalBoardDSP pedalBoard;
    spectralforge::PostRigGate postRigGate;
    spectralforge::ExtraGate gate2{spectralforge::ExtraGateUnit::gate2},expander{spectralforge::ExtraGateUnit::expander};
    std::atomic<float> gate2Gain{1},expanderGain{1};
    std::atomic<float>* gateAfterRig{};
    std::atomic<float>* gateRangeDb{};
    std::atomic<int> transposeQualityMode{0};
    spectralforge::PedalBoardState audioBoard;
    std::atomic<float> boardReduction{0};
    std::array<std::atomic<float>,5> stageRms{},stagePeaks{};
    void measureStage(int,const juce::AudioBuffer<float>&);
    void resetProcessingIfPending();
    void updateComparisonProtection();
    void resetProcessing(bool hostBypassToo);
    void applySwitchFade(juce::AudioBuffer<float>&);
    void beginSwitch();
    void process(juce::AudioBuffer<float>&);
    juce::ValueTree captureCore();
    void syncTransposeQuality();
    void restoreCore(juce::ValueTree);
    void warmInactiveComparison();
    std::array<juce::ValueTree,2> comparisons;
    std::mutex comparisonMutex;
    std::atomic<int> selectedComparison{0};
    std::atomic<uint64_t> headRecallRevision{0};
    std::atomic<bool> resetPending{false};
    // A recalled sound (preset, A/B, project, board on/off) resets the DSP
    // behind a short fade instead of cutting. While audio runs, beginSwitch()
    // lets the old sound fade out (5 ms) before the state changes, since new
    // parameters on the old DSP would already jump. Then: reset, mute for the
    // reported latency (cleared delay lines), fade in (5 ms). Host reset()
    // stays immediate, and so does the first block after prepareToPlay.
    std::atomic<bool> switchPending{false},fadeRequested{false},fadedSilent{false};
    std::atomic<juce::int64> lastProcessTicks{0};std::atomic<size_t> audioThread{0};
    // After the reset, stay muted for the latency plus 20 ms while the new
    // sound already runs: parameter smoothers ramp from the old sound's values
    // and dynamics detectors start without gain reduction, so the first note
    // would otherwise come out louder than the recalled sound.
    static constexpr double switchSettleSeconds=.02;
    // Then, if a cabinet that can be heard is still building the recalled IR
    // or model on the worker (or crossfading to it), stay muted until it is
    // ready, at most this long: otherwise the old cabinet plays first. While
    // muted, a recalled cabinet installs without the 50 ms crossfade; one that
    // arrives late gets the settle time again before the fade-in.
    static constexpr double switchCabinetWaitSeconds=.5;
    enum class SwitchFade {idle,out,hold,wait,in};
    SwitchFade switchFade{SwitchFade::idle};int switchFadeLength{240},switchFadePosition{240},switchWait{},switchHold{},switchCabinetBudget{};
    bool processedSincePrepare{},holdForSwitch{},switchCabinetLate{};
    juce::AudioProcessorValueTreeState state{*this,nullptr,"PARAMS",layout()};
    spectralforge::EQEditState eqEditState{state};
    spectralforge::EQCaptureBoundary eqCaptureBoundaryState;
    std::array<std::array<std::atomic<float>*,12>,3> cabPanelParameters{};
    spectralforge::OriginalCabParameters originalCabParameters;
    void syncOriginalCabRequests();
    void applyCabinetParameters(int lane,spectralforge::Cab& cab) const;
    spectralforge::Engine engine;
    spectralforge::IRLibrary library{{&engine.cabinet(0),&engine.cabinet(1),&engine.cabinet(2)}};
    spectralforge::PreFXChain preFX;
    spectralforge::PostFXChain postFX;
    spectralforge::CreativeFX creative;
    spectralforge::SynthLayer synth;
    std::atomic<double> synthTracked{0.0};
    // Sidechain and SYNTH bus (docs/SIDECHAIN_SYNTH_BUS.md). The key and bus
    // pointers are valid only while process() runs on one chunk.
    juce::AudioBuffer<float> busBuffer(juce::AudioBuffer<float>& hostBuffer,bool input) const;
    std::atomic<float>* synthOutput{};std::atomic<float>* keyComp{};std::atomic<float>* keyGates{};std::atomic<float>* keyMultiband{};
    juce::AudioBuffer<float> keyBuffer;const juce::AudioBuffer<float>* externalKey{};
    // The key delayed by HQ saturation (tone region) and the POST preamp, for keyed modules placed after them.
    spectralforge::KeyDelay toneKeyDelay,postKeyDelay;const juce::AudioBuffer<float>* toneKeyDelayed{};const juce::AudioBuffer<float>* postKeyDelayed{};float* busLeft{};float* busRight{};
    spectralforge::SignalOrderParameters signalOrder;spectralforge::SignalOrder activeOrder;   // activeOrder: audio thread
    spectralforge::OrderSwitch toneSwitch,postSwitch;
    double hostPpq{std::numeric_limits<double>::quiet_NaN()},hostPpqBpm{std::numeric_limits<double>::quiet_NaN()},hostBarStart{std::numeric_limits<double>::quiet_NaN()},hostBarBeats{},chunkPpq{std::numeric_limits<double>::quiet_NaN()};
    std::vector<spectralforge::SignalOrder> orderUndo,orderRedo;
    void writeOrder(const spectralforge::SignalOrder& order);
    bool commitOrder(const spectralforge::SignalOrder& next);
    // The SYNTH bus is delayed by the reported latency, like the main output.
    juce::AudioBuffer<float> busDelay;int busDelayWrite{};
    void delaySynthBus(float* left,float* right,int samples) noexcept;
    spectralforge::HostBypass hostBypass;
    spectralforge::GraphicalEQ toneEQ,finalEQ;
    spectralforge::CommonDynamics dynamics;
    spectralforge::MultibandDynamics multiband;
    spectralforge::ThreeBandSaturation saturation;
    spectralforge::FilterModulation filterModulation;
    spectralforge::DeEsser deesser;
    spectralforge::RoomCore room;
    spectralforge::DynamicsAnalysis dynamicsAnalysisState;
    spectralforge::autoset::Capture autoSetCaptureState;
    std::atomic<bool> autoSetGateBypass{false};
    spectralforge::Tuner tuner;
    spectralforge::PerformanceUtilities utilities;
    spectralforge::ABLevelMatch levelMatch;
    std::array<std::atomic<int>,128> midiMap;
    std::atomic<int> midiLearn{-1};
    std::atomic<bool> restartClick{false};
    std::atomic<float> tempoMeter{120};
    double lastTap{};std::array<double,4> tapIntervals{};int tapCount{};
    enum Extra {dualType,dualBlend,dualFrequency,inputMode,doublerOn,doublerTime,tempo,hostTempo,metronome,extraCount};
    std::array<std::atomic<float>*,extraCount> extras{};
    std::array<std::atomic<float>*,spectralforge::fxSpecs.size()> fxParameters{};
    std::array<std::atomic<float>*,11> modelParameters{},modelExtensionParameters{};
    std::atomic<float>* lowCompParameter{}, * lowAmpMixParameter{}, * preOrderParameter{}, * gainOrderParameter{};
    enum Global { mode,x1,x2,input,output,gateOn,threshold,release,hold,pitchOn,semitones,os,tunerOn,tunerMute,globalCount };
    std::array<std::atomic<float>*,globalCount> globals{};
    std::array<std::array<std::atomic<float>*,18>,3> laneParameters{};
    juce::SmoothedValue<float> inputGain, outputGain, tuningMute;
    std::atomic<float> inputPeak{0},outputPeak{0},gateGain{1},lowCompGain{0};
    std::atomic<float> preReduction{0},postReduction{0};
    std::array<std::atomic<float>,6> postPeaks{};
    std::array<std::atomic<float>,3> nativePostMeters{};
    std::atomic<float> cpuAverage{0},cpuPeak{0};
    int maximumBlock{512};
    double rate{48000};
};
