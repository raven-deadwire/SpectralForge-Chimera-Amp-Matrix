#pragma once
#include "AutoSetStrength.h"
#include "PluginProcessor.h"
#include "AutoSet.h"
#include "AutoSetAmp.h"
#include "AutoSetCab.h"
#include "AutoSetCabIR.h"
#include "AutoSetBoost.h"
#include "AutoSetSpace.h"
#include "AutoSetPower.h"
#include "AutoSetDual.h"
#include "AutoSetMatrix.h"
#include "AutoSetCabMics.h"
#include "AutoSetPolarity.h"
#include "AutoSetWidth.h"
#include "AutoSetCabCuts.h"
#include "AutoSetBus.h"
#include "EasyMode.h"
#include "AutoSetParameterEdits.h"
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <utility>
#include <vector>

namespace spectralforge::autoset {

namespace detail {
// Resolve every range before changing the proposal. A missing parameter means
// this filter cannot be applied, so it must not influence output compensation.
template <typename Lookup>
bool legalFinalMove(FinalMove& move,Lookup&& lookup) {
    auto* gain=lookup("gain");
    auto* frequency=lookup("frequency");
    auto* q=lookup("q");
    if(!gain || !frequency || !q)return false;
    move.gainDb=gain->getNormalisableRange().snapToLegalValue(move.gainDb);
    move.frequency=frequency->getNormalisableRange().snapToLegalValue(move.frequency);
    move.q=q->getNormalisableRange().snapToLegalValue(move.q);
    return true;
}
}

// Listening passes on the message thread, in signal order. Pass 1 sets input
// gain and the gate from the signal after input gain; pass 2 (optional) sets
// the Tone EQ from the amp/cab output at the new gain; pass 3 re-measures the
// clean compressor input after both, because the amp/cab path is nonlinear and
// the Tone EQ feeds the compressor; pass 4 (optional) sets the multiband
// compressor after it; the last pass (optional) sets the de-esser and the
// output limiter from what now reaches them. Gate 2/Expander (phase 5) and
// Dynamic EQ (phase 6) are result steps of the input and Tone EQ passes. The
// amp pass (phase 8, optional) follows the input pass: the amp's GAIN and tone
// knobs shape everything after it, at the new input gain. The cabinet mic pass
// (phase 9, optional) follows the amp: the modeled mic's position and distance.
// The boost pass (phase 10, optional) comes before the amp pass: a boost pedal
// in front of the amp changes what the amp's GAIN and tone should be. The delay
// and reverb step (phase 11) uses the input pass's windows and runs before the
// last (level) pass, once the instrument is known, so that pass hears the wet sound.
class Controller final : private juce::Timer {
public:
    // gates is a result step of the input pass, not a listening pass of its own.
    // dynamicEQ is a result step of the Tone EQ pass.
    enum class Phase {idle,input,tone,compressor,multiband,finish,done,failed,gates,dynamicEQ,level,amp,cab,boost,space,power,dual,width,cuts,bus,matrix,mics,rigs,channel,polarity,sync,cabIR};
    // Settings Auto Set must keep as they are: their parameters are neither
    // written nor restored by UNDO AUTO, and a pass whose outputs are all locked
    // is skipped. The gate lock also covers Gate 2 and the Expander; the Tone EQ
    // lock also keeps the Dynamic EQ step, whose nodes are Tone EQ bands.
    enum Lock : unsigned {lockInput=1u<<0,lockGate=1u<<1,lockTone=1u<<2,lockCompressor=1u<<3,lockMultiband=1u<<4,lockDeEsser=1u<<5,lockLimiter=1u<<6,lockDynamicEQ=1u<<7,lockFinalEQ=1u<<8,lockOutput=1u<<9,lockAmp=1u<<10,lockCab=1u<<11,lockBoost=1u<<12,lockSpace=1u<<13,lockPower=1u<<14,lockDual=1u<<15,lockWidth=1u<<16,lockCuts=1u<<17,lockBus=1u<<18,lockMatrix=1u<<19,lockMics=1u<<20,lockRigs=1u<<21,lockChannel=1u<<22,lockPolarity=1u<<23,lockSync=1u<<24,lockCabIR=1u<<25};
    static constexpr std::array<Lock,26> lockOrder{lockInput,lockGate,lockTone,lockCompressor,lockMultiband,lockDeEsser,lockLimiter,lockDynamicEQ,lockFinalEQ,lockOutput,lockAmp,lockCab,lockBoost,lockSpace,lockPower,lockDual,lockWidth,lockCuts,lockBus,lockMatrix,lockMics,lockRigs,lockChannel,lockPolarity,lockSync,lockCabIR};
    // strength (minimumStrength..1) scales how far each result moves the sound:
    // input gain change and Tone EQ gains scale, the compressor and multiband
    // mix in parallel, the Dynamic EQ and de-esser ranges scale. The gates are placed for the
    // applied input gain; the limiter protects the output and stays at full.
    static constexpr float minimumStrength=.1f;
    // instrument: 0 detect, 1 guitar, 2 bass. tone/multiband/finish include the optional passes;
    // gates sets Gate 2 and the Expander from the input pass; dynamicEQ places
    // Dynamic EQ nodes on the Tone EQ from the Tone EQ pass (which then runs
    // even with the Tone EQ step off); level adds the last pass, which sets the
    // Final EQ and the output trim at the limiter input; amp adds the pass after
    // the input pass that sets the amp head's GAIN, BASS, MIDDLE and TREBLE;
    // cab adds the pass after it that places the modeled cabinet mic; boost
    // adds the pass before the amp pass that can put a boost pedal in front of it;
    // space sets the POST delay and reverb from the playing the input pass heard.
    // power sets the head's PRESENCE and RESONANCE from the amp pass's capture.
    // dual sets the Dual BLEND or CROSSOVER in a pass of its own, in Dual routing only.
    // width sets the stereo doubler from the rig sound and the playing already heard.
    // cuts sets the Classic rig's cabinet LOW and HIGH cuts from the cabinet mic pass.
    // bus sets the POST Console VCA bus compressor from the de-esser + limiter pass.
    // matrix sets the Matrix MID and HIGH lane levels in a pass of its own, in Matrix routing only.
    // mics aligns the Classic cabinet's two blended mics and sets their blend, in a pass of its own.
    // rigs matches the Dual rig 2 level to rig 1 before the Dual blend, in the Dual pass.
    // channel picks the Classic head's channel before the amp step, in the amp pass.
    // polarity flips Dual rig 2, or the Matrix MID and HIGH lanes, when they cancel, in the Dual or Matrix pass.
    // cabIR chooses the heard mic's factory IR (V30 or Jensen), in the cabinet mic pass.
    // sync turns on DELAY SYNC to the host tempo, before the delay + reverb step, when the delay is off for want of a tempo.
    struct Options {int instrument{0};bool tone{true};Style style{Style::rock};bool finish{true};bool multiband{true};bool gates{true};float strength{1.f};unsigned locks{};bool dynamicEQ{true};bool level{true};bool amp{true};bool cab{true};bool boost{true};bool space{true};bool power{true};bool dual{true};bool width{true};bool cuts{true};bool bus{true};bool matrix{true};bool mics{true};bool rigs{true};bool channel{true};bool polarity{true};bool sync{true};bool cabIR{true};};
    // One finished pass: what it changed (or why it kept the settings).
    struct Step {Phase phase{};juce::String result;bool changed{};};
    // Live view of a run, for the progress and results display.
    struct Status {
        Phase phase{Phase::idle};float progress{};juce::String text;bool canUndo{};
        std::vector<Phase> passes;int pass{-1};       // pass index into passes while listening
        double passSeconds{},passTotalSeconds{},seconds{},totalSeconds{}; // audio heard so far / needed
        float levelDb{-120.f};                         // recent peak at the measured point
        juce::String listeningFor,heard;               // what the pass measures / what it has heard so far
        std::vector<Step> steps;                       // finished passes, in order
    };
    static const char* phaseName(Phase phase) noexcept {
        switch(phase){case Phase::input:return "Input + gate";case Phase::tone:return "Tone EQ";case Phase::compressor:return "Compressor";
            case Phase::multiband:return "Multiband";case Phase::finish:return "De-esser + limiter";case Phase::gates:return "Gate 2 + Expander";case Phase::dynamicEQ:return "Dynamic EQ";case Phase::level:return "Final EQ + output";case Phase::amp:return "Amp gain + tone";case Phase::cab:return "Cabinet mic";case Phase::boost:return "Boost pedal";case Phase::space:return "Delay + reverb";case Phase::power:return "Presence + resonance";case Phase::dual:return "Dual blend";case Phase::width:return "Stereo doubler";case Phase::cuts:return "Cabinet cuts";case Phase::bus:return "Bus compressor";case Phase::matrix:return "Matrix levels";case Phase::mics:return "Cabinet mic blend";case Phase::rigs:return "Dual rig levels";case Phase::channel:return "Amp channel";case Phase::polarity:return "Lane polarity";case Phase::sync:return "Delay tempo sync";case Phase::cabIR:return "Cabinet IR";default:return "";}
    }
    static constexpr int windowsPerPass=600,settleWindows=30; // 6 s of audio, 300 ms settle
    static constexpr double toneSeconds=6,toneSettleSeconds=.3;
    static constexpr double noAudioTimeoutMs=3000;

    explicit Controller(ChimeraProcessor& p):processor(p) {
        const auto add=[&](Lock lock,std::initializer_list<const char*> ids){for(const auto* id:ids)touchedIDs.push_back({id,lock});};
        add(lockInput,{"input"});add(lockGate,{"gateon","gatethreshold","gateRangeDb"});
        add(lockCompressor,{"cleanCompOn","cleanCompThreshold","cleanCompRatio","cleanCompAttack","cleanCompRelease",
                            "cleanCompKnee","cleanCompMakeup","cleanCompMix","cleanCompDetector"});
        touchedIDs.push_back({eqID(0,-1,"bypass"),lockTone});
        for(int b=0;b<graphicalEQBands;++b)for(const auto* field:{"enabled","frequency","gain","q","type"})touchedIDs.push_back({eqID(0,b,field),lockTone});
        add(lockDeEsser,{"deEsserOn","deEsserFrequency","deEsserThreshold","deEsserRange","deEsserAttack","deEsserRelease","deEsserRatio","deEsserKnee"});
        add(lockLimiter,{"liveLimiterOn","liveLimiterCeiling","liveLimiterRelease"});
        add(lockMultiband,{"mbOn","mbMix","mbDetector","mbCrossover1","mbCrossover2","mbCrossover3","mbBandCount"});
        for(int b=0;b<multibandBandCount;++b)
            for(const auto* field:{"bypass","mode","threshold","range","ratio","attack","release","knee","makeup","solo"})touchedIDs.push_back({multibandID(b,field),lockMultiband});
        add(lockGate,{"gateAfterRig","gateMode","gate2On","gate2Mode","gate2Threshold","gate2Ratio","gate2Attack","gate2Hold","gate2Release","gate2RangeDb",
                      "expOn","expAfterRig","expThreshold","expRatio","expAttack","expHold","expRelease","expRangeDb"});
        for(int slot=0;slot<4;++slot)for(const auto* field:{"target","threshold","range","attack","release","ratio"})touchedIDs.push_back({dynamicSlotID(slot,field),lockDynamicEQ});
        touchedIDs.push_back({eqID(1,-1,"bypass"),lockFinalEQ});
        for(int b=0;b<graphicalEQBands;++b)for(const auto* field:{"enabled","frequency","gain","q","type"})touchedIDs.push_back({eqID(1,b,field),lockFinalEQ});
        add(lockOutput,{"output"});
        // Phase 8 writes the Classic routing's head knobs that Easy view calls
        // GAIN, BASS, MIDDLE and TREBLE, for every head, channel and input.
        for(int model=0;model<ampModelCount;++model) {
            const auto& panel=ampNativePanel(model);
            for(int channel=0;channel<std::max(1,int(panel.channels.size()));++channel)
                for(int route=0;route<std::max(1,int(panel.routes.size()));++route)
                    for(int role=0;role<easy::roleCount;++role)
                        if(const int control=easy::ampControl(model,channel,route,easy::Role(role));control>=0) {
                            const auto id=ampNativeControlID(ampNativeContext(0,0),model,control,channel);
                            if(std::none_of(touchedIDs.begin(),touchedIDs.end(),[&](const auto& t){return t.first==id;}))touchedIDs.push_back({id,lockAmp});
                        }
        }
        // Phase 20 writes the Classic routing's head channel, for every head.
        for(int model=0;model<ampModelCount;++model)touchedIDs.push_back({ampNativeChannelID(ampNativeContext(0,0),model),lockChannel});
        // Phase 12 writes the same heads' PRESENCE and RESONANCE (or DEPTH).
        for(int model=0;model<ampModelCount;++model) {
            const auto& panel=ampNativePanel(model);
            for(int channel=0;channel<std::max(1,int(panel.channels.size()));++channel)
                for(int role=0;role<2;++role)if(const int control=powerControl(model,channel,role);control>=0) {
                    const auto id=ampNativeControlID(ampNativeContext(0,0),model,control,channel);
                    if(std::none_of(touchedIDs.begin(),touchedIDs.end(),[&](const auto& t){return t.first==id;}))touchedIDs.push_back({id,lockPower});
                }
        }
        // Phase 9 places the Classic routing's (lane 1) modeled mic A or B and keeps its level.
        for(const auto* mic:{"A","B"}) {
            for(const auto* field:{"position","distance"})touchedIDs.push_back({originalCabID(0,(juce::String(mic)+field).toRawUTF8()),lockCab});
            touchedIDs.push_back({"cab"+juce::String(mic)+"gain1",lockCab});
        }
        add(lockDual,{"dualblend","dualcross"});
        // Phase 17 writes the Matrix MID and HIGH lanes' level: the lane LEVEL,
        // or the native head's OUTPUT LEVEL, which replaces it on a native head.
        // LEVEL 2 is also the Dual rig 2 level phase 19 writes: either lock keeps it.
        touchedIDs.push_back({"level2",Lock(lockMatrix|lockRigs)});touchedIDs.push_back({"level3",lockMatrix});
        for(const int lane:{1,2})touchedIDs.push_back({ampNativeOutputLevelID(ampNativeContext(2,lane)),lockMatrix});
        // Phase 21 writes the lane polarity of Dual rig 2 / Matrix MID (lane 2) and Matrix HIGH (lane 3).
        add(lockPolarity,{"polarity2","polarity3"});
        // Phase 22 turns on the delay's tempo sync, following the host tempo.
        add(lockSync,{"delaysync","temposync"});
        // Phase 23 chooses the Classic rig's factory IR for mic A or mic B.
        add(lockCabIR,{"cabtype1","cabBtype1"});
        // Phase 19 writes the Dual rig 2 level: LEVEL 2 (above), or the Dual B
        // native head's OUTPUT LEVEL, which replaces it on a native head.
        touchedIDs.push_back({ampNativeOutputLevelID(ampNativeContext(1,1)),lockRigs});
        // Phase 18 aligns the Classic cabinet's mic A and mic B and sets their blend.
        add(lockMics,{"cabAdelay1","cabBdelay1","cabBinvert1","cabblend1"});
        add(lockWidth,{"doubleron","doublertime"});add(lockCuts,{"cablow1","cabhigh1"});
        touchedIDs.push_back({postNativeBypassID(0,0),lockBus});
        for(const int c:{busThresholdControl,busMakeupControl,busAttackControl,busReleaseControl,busRatioControl,busInControl})touchedIDs.push_back({postNativeControlID(0,0,c),lockBus});
        add(lockSpace,{"delayon","delaytime","delayfeedback","delaymix","reverbon","reverbsize","reverbdamping","reverbmix"});
        // Phase 10 places the Green Drive on a free pedal board slot.
        for(int owner=0;owner<pedalBoardCapacity;++owner) {
            touchedIDs.push_back({pedalModelID(owner),lockBoost});touchedIDs.push_back({pedalBypassID(owner,boostPedalModel),lockBoost});
            for(int c=0;c<pedalModel(boostPedalModel).controlCount;++c)touchedIDs.push_back({pedalControlID(owner,boostPedalModel,c),lockBoost});
        }
        for(const auto& item:touchedIDs)if(auto* parameter=processor.parameters().getParameter(item.first))
            editTrackers.push_back(std::make_unique<ParameterEdits>(*parameter));
        // Read-only bus dependencies also need revisions (including A-B-A).
        // Do not add them to touchedIDs: Auto Set/UNDO must never write them.
        for(const auto& id:busReadIDs())if(auto* parameter=processor.parameters().getParameter(id))
            if(std::none_of(editTrackers.begin(),editTrackers.end(),[&](const auto& t){return &t->parameter==parameter;}))
                editTrackers.push_back(std::make_unique<ParameterEdits>(*parameter));
    }
    ~Controller() override {stopTimer();stopListening();}

    // Execution options take effect at the next start(). Newly selected locks
    // additionally protect their stages from undo of the previous run.
    void setOptions(const Options& chosen) noexcept {
        options=chosen;options.instrument=std::clamp(options.instrument,0,2);
        options.strength=std::isfinite(options.strength)?std::clamp(options.strength,minimumStrength,1.f):1.f;
        options.locks&=(1u<<lockOrder.size())-1u;
    }
    const Options& currentOptions() const noexcept {return options;}
    void start() {
        if(busy())return;
        runOptions=options;
        inputRevision=editsFor(*processor.parameters().getParameter("input")).revision();
        snapshot.clear();dynamicNodeGroups.clear();finalNodeGroups.clear();summary.clear();steps.clear();lastGates={};lastTone={};lastDynamic={};lastLevel={};lastAmp={};lastPower={};lastCab={};lastBoost={};lastSpace={};lastDual={};lastMatrix={};lastMics={};lastRigs={};lastChannel={};lastPolarity={};lastSync={};lastCabIR={};lastWidth={};lastCuts={};lastBus={};widthSamples.clear();spaceWindows.clear();lastFinish={};lastMultiband={};manualEditsKept=false;lockedPhases.clear();
        // Locked settings stay out of the snapshot, so set() never writes them.
        for(const auto& [id,lock]:touchedIDs)if(!locked(lock))if(auto* parameter=processor.parameters().getParameter(id))
            snapshot.push_back(takeSnapshot(*parameter));
        passes.clear();
        const auto plan=[&](Phase phase,bool chosen,unsigned outputs){if(!chosen)return;if((runOptions.locks&outputs)==outputs)lockedPhases.push_back(phase);else passes.push_back(phase);};
        plan(Phase::input,true,lockInput|lockGate);
        plan(Phase::boost,runOptions.boost,lockBoost);
        // The amp pass serves the amp step and the presence + resonance step,
        // which renders the same capture through the head.
        if(channelStepRuns() || ampStepRuns() || powerStepRuns())passes.push_back(Phase::amp);
        else {
            if(runOptions.channel)lockedPhases.push_back(Phase::channel);
            if(runOptions.amp)lockedPhases.push_back(Phase::amp);
            if(runOptions.power)lockedPhases.push_back(Phase::power);
        }
        // The cabinet mic pass serves the mic step, the cabinet IR step and the cabinet cuts step.
        if(cabStepRuns() || cabIRStepRuns() || cutsStepRuns())passes.push_back(Phase::cab);
        else {
            if(runOptions.cab)lockedPhases.push_back(Phase::cab);
            if(runOptions.cabIR)lockedPhases.push_back(Phase::cabIR);
            if(runOptions.cuts)lockedPhases.push_back(Phase::cuts);
        }
        // Phase 18 listens only to a Classic cabinet with both mics heard.
        if(runOptions.mics) {
            const auto t=cabTarget();
            if(locked(lockMics))lockedPhases.push_back(Phase::mics);
            else if(t.mode!=0)lockedPhases.push_back({Phase::mics,"Dual and Matrix routing: blend those mics in the full editor"});
            else if(!t.on)lockedPhases.push_back({Phase::mics,"cabinet off"});
            else if(t.mic!=-1)lockedPhases.push_back({Phase::mics,juce::String("one mic heard (mic ")+(t.mic==0?"A":"B")+")"});
            else passes.push_back(Phase::mics);
        }
        // The Dual pass serves the rig level step (phase 19) and the blend step
        // (phase 13), and listens only when the routing is Dual.
        // Phase 21 (lane polarity) runs in the Dual or the Matrix pass, whichever the routing has.
        const int routing=juce::roundToInt(raw("mode"));
        if(runOptions.polarity && !(routing==1 && (dualStepRuns() || rigsStepRuns() || polarityStepRuns()))
                               && !(routing==2 && (matrixStepRuns() || polarityStepRuns()))) {
            if(locked(lockPolarity))lockedPhases.push_back(Phase::polarity);
            else lockedPhases.push_back({Phase::polarity,"Classic routing: one rig"});
        }
        if(routing==1 && (dualStepRuns() || rigsStepRuns() || polarityStepRuns()))passes.push_back(Phase::dual);
        else {
            if(runOptions.rigs) {
                if(locked(lockRigs))lockedPhases.push_back(Phase::rigs);
                else if(juce::roundToInt(raw("mode"))!=1)lockedPhases.push_back({Phase::rigs,"Classic or Matrix routing: no Dual rigs"});
                else lockedPhases.push_back({Phase::rigs,rigsCrossoverWhy});
            }
            if(runOptions.dual) {
                if(locked(lockDual))lockedPhases.push_back(Phase::dual);
                else lockedPhases.push_back({Phase::dual,"Classic or Matrix routing: no Dual blend"});
            }
        }
        // Phase 17 listens only when the routing is Matrix: the others have no bands.
        if(routing==2 && (matrixStepRuns() || polarityStepRuns()))passes.push_back(Phase::matrix);
        else if(runOptions.matrix) {
            if(locked(lockMatrix))lockedPhases.push_back(Phase::matrix);
            else lockedPhases.push_back({Phase::matrix,"Classic or Dual routing: no Matrix lanes"});
        }
        // The Tone EQ pass serves the Tone EQ step and the Dynamic EQ step,
        // whose nodes live on Tone EQ bands (so a Tone EQ lock keeps both).
        if(runOptions.tone || runOptions.dynamicEQ) {
            if(toneStepRuns() || dynamicStepRuns())passes.push_back(Phase::tone);
            if(runOptions.tone && !toneStepRuns())lockedPhases.push_back(Phase::tone);
            if(runOptions.dynamicEQ && !dynamicStepRuns())lockedPhases.push_back(Phase::dynamicEQ);
        }
        plan(Phase::compressor,true,lockCompressor);
        plan(Phase::multiband,runOptions.multiband,lockMultiband);
        // The de-esser + limiter pass also hears the POST input for the bus compressor step.
        if(finishStepRuns() || busStepRuns())passes.push_back(Phase::finish);
        else {
            if(runOptions.finish)lockedPhases.push_back(Phase::finish);
            if(runOptions.bus)lockedPhases.push_back(Phase::bus);
        }
        spacePending=runOptions.space && !locked(lockSpace);
        widthPending=runOptions.width && !locked(lockWidth);
        // Phase 22 runs inside the delay + reverb step, which turns the delay on.
        if(runOptions.sync && !spacePending)lockedPhases.push_back(locked(lockSync)?Skipped{Phase::sync}:Skipped{Phase::sync,runOptions.space?"Delay + reverb locked":"the delay + reverb step is off"});
        if(runOptions.space && locked(lockSpace))lockedPhases.push_back(Phase::space);
        if(runOptions.width && locked(lockWidth))lockedPhases.push_back(Phase::width);
        plan(Phase::level,runOptions.level,lockFinalEQ|lockOutput);
        if(passes.empty()){publish(Phase::failed,0,"Every Auto Set step is locked or turned off. Unlock a setting in the options and try again.",0);return;}
        beginPass(0,0);
        startTimerHz(20);
    }
    void cancel() {
        if(!busy())return;
        stopTimer();stopListening();
        // Cancel only stops listening. Passes that already finished keep
        // their settings until UNDO restores the pre-run snapshot.
        publish(Phase::idle,0,changedSinceSnapshot() ? "Auto Set cancelled. Finished steps are kept; UNDO restores your previous settings."
                                                     : "Auto Set cancelled before changing anything.",0);
    }
    void undo() {
        if(busy() || snapshot.empty())return;
        // Only restore writes owned by this run. A later manual/host edit,
        // including a change to an untouched optional stage, belongs to the user.
        // A slot and its Tone EQ band form one node. Freezing just the slot
        // would leave it pointing at an undone/disabled band (or vice versa).
        const auto preserved=preservedDynamicIDs();
        for(auto& entry:snapshot)if(entry.written && owned(entry) && !keptByCurrentLock(*entry.parameter)
                                  && !preserved.contains(entry.parameter->paramID))
            setNormalised(entry,entry.before);
        snapshot.clear();
        publish(Phase::idle,0,"Undid Auto Set; newer edits were kept.",0);
    }
    bool busy() const noexcept {return busyPhase(status.phase);}
    const Status& current() const noexcept {return status;}
    std::function<void(const Status&)> onStatus;
    // Exposed so non-realtime tests can drive the controller deterministically.
    void tick(double nowMs=juce::Time::getMillisecondCounterHiRes()) {
        if(!busy())return;
        auto& capture=processor.autoSetCapture();
        const auto before=collected();
        if(status.phase==Phase::level)capture.drainOutput(outputSamples);
        else if(samplePass())capture.drainSamples(samples);
        if(status.phase==Phase::amp || status.phase==Phase::boost || status.phase==Phase::dual || status.phase==Phase::matrix || status.phase==Phase::finish)capture.drainAmpSamples(ampSamples);
        if(status.phase==Phase::matrix || status.phase==Phase::mics)capture.drainLaneSamples(laneSamples);
        if(windowPass())capture.drain(windowTap(),windows);
        if(status.phase==Phase::input)capture.drain(Tap::rig,rigWindows);
        if(lastAudioMs<0 || collected()!=before)lastAudioMs=nowMs;
        else if(nowMs-lastAudioMs>noAudioTimeoutMs){fail("No audio reached the plug-in. Start playback or play your instrument, then try again.");return;}
        const double measured=std::max(0.,double(collected())-double(samplePass()?skipSamples:skipWindows)),needed=passLength();
        if(measured<needed){publish(status.phase,progress(float(measured/needed)),passText(),measured/needed);return;}
        const std::vector<Window> measuredWindows(windows.begin()+std::min(windows.size(),size_t(skipWindows)),windows.end());
        const std::vector<float> measuredSamples(samples.begin()+std::min(samples.size(),size_t(skipSamples)),samples.end());
        if(status.phase==Phase::input)finishInput(measuredWindows);
        else if(status.phase==Phase::amp)finishAmp(measuredSamples,{ampSamples.begin()+std::min(ampSamples.size(),size_t(skipSamples)),ampSamples.end()});
        else if(status.phase==Phase::cab)finishCab(measuredSamples);
        else if(status.phase==Phase::dual)finishDual(measuredSamples,{ampSamples.begin()+std::min(ampSamples.size(),size_t(skipSamples)),ampSamples.end()});
        else if(status.phase==Phase::mics)finishMics(measuredSamples,{laneSamples.begin()+std::min(laneSamples.size(),size_t(skipSamples)),laneSamples.end()});
        else if(status.phase==Phase::matrix)finishMatrix(measuredSamples,{ampSamples.begin()+std::min(ampSamples.size(),size_t(skipSamples)),ampSamples.end()},
                                                         {laneSamples.begin()+std::min(laneSamples.size(),size_t(skipSamples)),laneSamples.end()});
        else if(status.phase==Phase::boost)finishBoost({ampSamples.begin()+std::min(ampSamples.size(),size_t(skipSamples)),ampSamples.end()});
        else if(status.phase==Phase::tone)finishTone(measuredSamples);
        else if(status.phase==Phase::compressor)finishCompressor(measuredWindows);
        else if(status.phase==Phase::multiband)finishMultiband(measuredSamples);
        else if(status.phase==Phase::finish)finishOutput(measuredSamples,measuredWindows,{ampSamples.begin()+std::min(ampSamples.size(),size_t(skipSamples)),ampSamples.end()});
        else finishLevel(std::vector<OutputFrame>(outputSamples.begin()+std::min(outputSamples.size(),size_t(skipSamples)),outputSamples.end()));
    }
private:
    void timerCallback() override {tick();}
    float raw(const juce::String& id) const {return processor.parameters().getRawParameterValue(id)->load();}
    void set(const juce::String& id,float value) {
        if(auto* parameter=processor.parameters().getParameter(id)) {
            const auto entry=std::find_if(snapshot.begin(),snapshot.end(),[&](const auto& e){return e.parameter==parameter;});
            if(entry==snapshot.end())return;
            // A value returning to its old position is still an external edit.
            if(!owned(*entry)){manualEditsKept=true;return;}
            const auto before=parameter->getValue();
            if(!setNormalised(*entry,parameter->convertTo0to1(value))){manualEditsKept=true;return;}
            if(parameter->getValue()!=before){entry->lastWritten=parameter->getValue();entry->written=true;}
        }
    }
    struct Snapshot {juce::RangedAudioParameter* parameter;float before,lastWritten;bool written;ParameterEdits* edits;uint32_t revision;};
    // The Classic routing's amp as phase 8 sees it; a change while listening voids the pass.
    struct AmpTarget {int mode{},model{},channel{},route{},context{};bool native{},on{};bool operator==(const AmpTarget&) const=default;};
    // The Classic routing's cabinet as phase 9 sees it: which mic is heard
    // (0 A, 1 B, -1 both blended) and its modeled settings (key 0: not modeled).
    // The pedal board as phase 10 sees it: the free slot the pedal would take
    // (-1: none after the last pedal), and whether a drive is already on.
    struct BoostTarget {AmpTarget amp;bool board{},driveOn{};int owner{-1};std::array<int,pedalBoardCapacity> models{},order{};bool operator==(const BoostTarget&) const=default;};
    // The Dual routing as phase 13 sees it: a change while listening voids the pass.
    // native2: whether rig 2's level is its native head's OUTPUT LEVEL.
    struct DualTarget {int mode{};bool crossover{};bool native2{};bool operator==(const DualTarget&) const=default;};
    // The Matrix routing as phase 17 sees it: the routing, and whether the MID
    // and HIGH lanes' level is a native head's OUTPUT LEVEL. A change voids the pass.
    struct MatrixTarget {int mode{};std::array<bool,2> native{};bool operator==(const MatrixTarget&) const=default;};
    struct CabTarget {int mode{};bool on{};int mic{-1};uint64_t key{};bool operator==(const CabTarget&) const=default;};
    ParameterEdits& editsFor(juce::RangedAudioParameter& parameter) const {
        const auto it=std::find_if(editTrackers.begin(),editTrackers.end(),[&](const auto& e){return &e->parameter==&parameter;});
        jassert(it!=editTrackers.end());return **it;
    }
    Snapshot takeSnapshot(juce::RangedAudioParameter& parameter) const {
        auto& edits=editsFor(parameter);const auto revision=edits.revision();const auto value=parameter.getValue();
        return {&parameter,value,value,false,&edits,revision};
    }
    static bool owned(const Snapshot& entry) {
        return entry.edits->unchanged(entry.revision) && entry.parameter->getValue()==(entry.written?entry.lastWritten:entry.before);
    }
    static bool setNormalised(Snapshot& entry,float normalised) {
        return entry.edits->write(normalised,entry.written?entry.lastWritten:entry.before,entry.revision);
    }
    bool keptByCurrentLock(const juce::RangedAudioParameter& parameter) const {
        for(const auto& [id,lock]:touchedIDs)if(id==parameter.paramID)return (options.locks&lock)!=0;
        return false;
    }
    juce::StringArray preservedDynamicIDs() const {
        juce::StringArray result;
        for(const auto& group:dynamicNodeGroups) {
            bool keep=(options.locks&(lockTone|lockDynamicEQ))!=0;
            for(const auto& entry:snapshot)if(group.contains(entry.parameter->paramID) && !owned(entry))keep=true;
            if(keep){result.addArray(group);result.addIfNotAlreadyThere(eqID(0,-1,"bypass"));}
        }
        for(const auto& group:finalNodeGroups) {
            bool keep=(options.locks&lockFinalEQ)!=0;
            for(const auto& entry:snapshot)if((group.contains(entry.parameter->paramID) || entry.parameter->paramID==eqID(1,-1,"bypass")) && !owned(entry))keep=true;
            if(keep){result.addArray(group);result.addIfNotAlreadyThere(eqID(1,-1,"bypass"));}
        }
        return result;
    }
    bool samplePass() const noexcept {return samplePass(status.phase);}
    bool windowPass() const noexcept {return status.phase==Phase::input || status.phase==Phase::compressor || status.phase==Phase::finish;}
    Tap windowTap() const noexcept {return status.phase==Phase::input?Tap::input:status.phase==Phase::finish?Tap::output:Tap::compressor;}
    size_t collected() const noexcept {return status.phase==Phase::level?outputSamples.size():samplePass()?samples.size():windows.size();}
    double rate() const noexcept {return processor.getSampleRate()>0?processor.getSampleRate():48000.;}
    double passLength() const noexcept {return samplePass()?toneSeconds*rate():double(windowsPerPass);}
    std::optional<bool> chosenBass() const noexcept {
        if(runOptions.instrument!=0)return runOptions.instrument==2;
        if(lastTone.playingDetected)return lastTone.bass;
        return std::nullopt;
    }
    void beginPass(size_t index,int settle) {
        auto& capture=processor.autoSetCapture();
        capture.listen(std::nullopt);
        std::vector<Window> discard;for(const auto t:{Tap::input,Tap::compressor,Tap::output,Tap::rig})capture.drain(t,discard);
        std::vector<float> stale;capture.drainSamples(stale);capture.drainAmpSamples(stale);capture.drainLaneSamples(stale);
        flushLocked(passes[index]);
        pass=index;windows.clear();rigWindows.clear();samples.clear();ampSamples.clear();laneSamples.clear();outputSamples.clear();levelReadSnapshot.clear();lastAudioMs=-1; // timed from the first tick of the pass
        skipWindows=settle?settleWindows:0;skipSamples=settle?int(toneSettleSeconds*rate()):0;
        status.phase=passes[pass];
        if(status.phase==Phase::tone)capture.listen(Tap::tone);
        // The amp, cab and boost passes hear the rig alone (Tap::cab), wherever
        // the signal-order editor puts the Tone EQ.
        else if(status.phase==Phase::amp){ampAtStart=ampTarget();capture.listen({Tap::amp,Tap::cab});}
        else if(status.phase==Phase::cab){cabAtStart=cabTarget();cabIRAtStart=cabSources();cutsAtStart={raw("cablow1"),raw("cabhigh1")};capture.listen(Tap::cab);}
        else if(status.phase==Phase::boost){boostAtStart=boostTarget();capture.listen({Tap::amp,Tap::cab});}
        // The Dual pass hears each rig before the blend (lane2 shares the amp tap's ring).
        else if(status.phase==Phase::dual){dualAtStart=dualTarget();polarityAtStart=lanePolarity();capture.listen({Tap::lane1,Tap::lane2});}
        // The Matrix pass hears each lane (lane3 has a ring of its own).
        // The mics pass hears mic A (sample ring) and mic B (lane3's ring).
        else if(status.phase==Phase::mics){micsAtStart=cabTarget();capture.listen({Tap::micA,Tap::micB});}
        else if(status.phase==Phase::matrix){matrixAtStart=matrixTarget();polarityAtStart=lanePolarity();capture.listen({Tap::lane1,Tap::lane2,Tap::lane3});}
        else if(status.phase==Phase::multiband)capture.listen(Tap::multiband);
        else if(status.phase==Phase::finish){
            busAtStart=busTarget();busReadSnapshot.clear();
            for(const auto& id:busReadIDs())if(auto* p=processor.parameters().getParameter(id))busReadSnapshot.push_back(takeSnapshot(*p));
            capture.listen({Tap::deesser,Tap::output,Tap::post});
        }
        else if(status.phase==Phase::level) {
            for(const auto& tracker:editTrackers)if(tracker->parameter.paramID.startsWith("finalEQ_") || tracker->parameter.paramID.startsWith("dynamicEQ") || tracker->parameter.paramID=="output")
                levelReadSnapshot.push_back(takeSnapshot(tracker->parameter));
            capture.listen(Tap::master);
        }
        else if(status.phase==Phase::input && runOptions.gates)capture.listen({Tap::input,Tap::rig});
        else capture.listen(windowTap());
        // Gate 2 / Expander need the rig's own noise: hold every gate open while the input pass listens.
        processor.setAutoSetGateBypass(status.phase==Phase::input && runOptions.gates);
        publish(passes[pass],progress(0),passText(),0);
    }
    void nextPass() {
        if(pass+1<passes.size() && passes[pass+1]==Phase::level){finishSpace();finishWidth();}
        if(pass+1<passes.size())beginPass(pass+1,1);else complete();
    }
    bool locked(unsigned lock) const noexcept {return (runOptions.locks&lock)!=0;}
    bool ampStepRuns() const noexcept {return runOptions.amp && !locked(lockAmp);}
    bool channelStepRuns() const noexcept {return runOptions.channel && !locked(lockChannel);}
    bool powerStepRuns() const noexcept {return runOptions.power && !locked(lockPower);}
    bool cabStepRuns() const noexcept {return runOptions.cab && !locked(lockCab);}
    bool cutsStepRuns() const noexcept {return runOptions.cuts && !locked(lockCuts);}
    bool cabIRStepRuns() const noexcept {return runOptions.cabIR && !locked(lockCabIR);}
    bool dualStepRuns() const noexcept {return runOptions.dual && !locked(lockDual);}
    bool matrixStepRuns() const noexcept {return runOptions.matrix && !locked(lockMatrix);}
    bool polarityStepRuns() const noexcept {return runOptions.polarity && !locked(lockPolarity);}
    // Rig levels are matched for the Dual blend only: in Crossover each rig plays its own band.
    bool rigsStepRuns() const noexcept {return runOptions.rigs && !locked(lockRigs) && raw("dualtype")<=.5f;}
    static constexpr const char* rigsCrossoverWhy="Dual crossover: each rig plays its own band";
    bool finishStepRuns() const noexcept {return runOptions.finish && (runOptions.locks&(lockDeEsser|lockLimiter))!=(lockDeEsser|lockLimiter);}
    bool busStepRuns() const noexcept {return runOptions.bus && !locked(lockBus);}
    bool toneStepRuns() const noexcept {return runOptions.tone && !locked(lockTone);}
    bool dynamicStepRuns() const noexcept {return runOptions.dynamicEQ && !locked(lockDynamicEQ) && !locked(lockTone);}
    // Signal order of passes and result steps, for reporting skipped ones.
    static int order(Phase phase) noexcept {
        switch(phase){case Phase::input:return 0;case Phase::gates:return 1;case Phase::boost:return 2;case Phase::channel:return 3;case Phase::amp:return 4;case Phase::power:return 5;case Phase::cab:return 6;case Phase::cabIR:return 7;case Phase::cuts:return 8;case Phase::mics:return 9;case Phase::polarity:return 10;case Phase::rigs:return 11;case Phase::dual:return 12;case Phase::matrix:return 13;case Phase::tone:return 14;case Phase::dynamicEQ:return 15;
            case Phase::compressor:return 16;case Phase::multiband:return 17;case Phase::finish:return 18;case Phase::bus:return 19;case Phase::sync:return 20;case Phase::space:return 21;case Phase::width:return 22;case Phase::level:return 23;default:return 24;}
    }
    float strength() const noexcept {return runOptions.strength;}
    // Report skipped (locked) passes in signal order, before the next pass that runs.
    void flushLocked(Phase upTo) {
        while(!lockedPhases.empty() && order(lockedPhases.front().phase)<order(upTo)) {
            steps.push_back({lockedPhases.front().phase,"kept ("+lockedPhases.front().why+")",false});lockedPhases.erase(lockedPhases.begin());
        }
    }
    juce::String passText() const {
        const auto step="Listening "+juce::String(int(pass)+1)+"/"+juce::String(int(passes.size()))+": ";
        if(status.phase==Phase::input)return step+"play normally, including a short pause...";
        if(status.phase==Phase::amp)return step+"keep playing as you normally would, soft and hard, for the amp's "+ampTargetText()+"...";
        if(status.phase==Phase::cab)return step+"keep playing across the neck for the "+juce::String(cabStepRuns()&&cutsStepRuns()?"cabinet mic and cuts":cutsStepRuns()?"cabinet cuts":cabStepRuns()?"cabinet mic":"cabinet IR")+"...";
        if(status.phase==Phase::boost)return step+"keep playing, including low notes and chords, for the boost pedal...";
        if(status.phase==Phase::dual)return step+"keep playing as you normally would, across the neck, for the Dual "+juce::String(raw("dualtype")>.5f?"crossover":rigsStepRuns()&&dualStepRuns()?"rig levels and blend":rigsStepRuns()?"rig levels":"blend")+"...";
        if(status.phase==Phase::mics)return step+"keep playing across the neck, low notes and chords, for the cabinet's two mics...";
        if(status.phase==Phase::matrix)return step+"keep playing as you normally would, low notes to high, for the Matrix lane levels...";
        if(status.phase==Phase::tone)return step+"keep playing across the neck for the "+toneTarget()+"...";
        if(status.phase==Phase::multiband)return step+"keep playing, low notes and hard picking, for the multiband...";
        if(status.phase==Phase::finish)return step+"keep playing, with some hard picking, for the "+juce::String(finishStepRuns()&&busStepRuns()?"de-esser, limiter and bus compressor":busStepRuns()?"bus compressor":"de-esser and limiter")+"...";
        if(status.phase==Phase::level)return step+"keep playing as you normally would, for the Final EQ and output level...";
        return step+"keep playing for the compressor...";
    }
    juce::String ampTargetText() const {
        const juce::String knobs=ampStepRuns()&&powerStepRuns()?"gain, tone, presence and resonance":powerStepRuns()?"presence and resonance":ampStepRuns()?"gain and tone":"";
        return channelStepRuns()?(knobs.isEmpty()?juce::String("channel"):"channel, "+knobs):knobs;
    }
    juce::String toneTarget() const {return toneStepRuns()&&dynamicStepRuns()?"Tone EQ and Dynamic EQ":dynamicStepRuns()?"Dynamic EQ":"Tone EQ";}
    float progress(float inPass) const {return (float(pass)+inPass)/float(passes.size());}
    void finishInput(const std::vector<Window>& measured) {
        const auto r=analyseInputGate(measured,raw("input"));
        if(!r.playingDetected){fail("No playing detected. Check the input level and try again.");return;}
        const float inputBefore=raw("input"),gateBefore=raw("gatethreshold");
        juce::String result;
        if(locked(lockInput)) {summary="Input locked";result="input kept (locked)";}
        else {
            const float proposed=scaledInputGainDb(inputBefore,r.inputGainDb,strength());
            set("input",proposed);
            const float applied=raw("input");
            summary="Input "+juce::String(applied,1)+" dB";
            result="input "+juce::String(inputBefore,1)+" > "+juce::String(applied,1)+" dB";
        }
        if(locked(lockGate)){summary+=" | Gate locked";result+="; gate kept (locked)";}
        else if(!editsFor(*processor.parameters().getParameter("input")).unchanged(inputRevision)) {
            summary+=" | Gate unchanged (input edited while listening)";
            result+="; gate kept (input edited while listening)";
        } else if(r.pausesDetected) {
            // The analysis places the gate for its proposed input gain; follow
            // the gain actually applied (locked or scaled), since the gate
            // detects after input gain.
            const float shift=raw("input")-r.inputGainDb;
            const float threshold=shift==0.f?r.gateThresholdDb:std::round(std::clamp(r.gateThresholdDb+shift,-90.f,-15.f));
            set("gatethreshold",threshold);set("gateRangeDb",r.gateRangeDb);set("gateon",1);
            const auto range=r.gateRangeDb>=96.f?juce::String("Full"):juce::String(r.gateRangeDb,0)+" dB";
            summary+=" | Gate "+juce::String(threshold,0)+" dB, range "+range;
            result+="; gate "+juce::String(gateBefore,0)+" > "+juce::String(threshold,0)+" dB, range "+range;
        } else {summary+=" | Gate unchanged (no pause heard)";result+="; gate kept (no pause heard)";}
        addStep(result,true);
        processor.setAutoSetGateBypass(false);
        if(runOptions.gates)finishGateStages(measured,r.pausesDetected);
        if(spacePending || widthPending)spaceWindows=measured;
        nextPass();
    }
    // Phase 5: Gate 2 and the Expander from the rig noise heard in the input pass.
    void finishGateStages(const std::vector<Window>& measured,bool pauseHeard) {
        const auto addGateStep=[this](const juce::String& result,bool changed){steps.push_back({Phase::gates,result,changed});};
        // This stage shares the input pass, so its rig/input comparison is
        // invalid too if the user/host changed gain, even A -> B -> A. Check
        // again here, after any callbacks from the main gate's writes.
        if(!editsFor(*processor.parameters().getParameter("input")).unchanged(inputRevision)) {
            addGateStep("kept (input edited while listening)",false);return;
        }
        if(locked(lockGate)){addGateStep("kept (locked)",false);return;}
        if(!pauseHeard){addGateStep("kept (no pause heard)",false);return;}
        const std::vector<Window> rig(rigWindows.begin()+std::min(rigWindows.size(),size_t(skipWindows)),rigWindows.end());
        const int lag=juce::roundToInt(double(std::max(0,processor.getLatencySamples()))/(rate()*.01));
        const auto r=analyseGateStages(measured,rig,lag,raw("gatethreshold"),raw("gateAfterRig")>.5f,runOptions.style);
        lastGates=r;
        const auto hiss="amp hiss "+juce::String(r.rigNoiseDb,0)+" dBFS";
        if(!r.measured){addGateStep("kept (pause too short to measure the amp's hiss)",false);return;}
        using Plan=GateStagesResult::Plan;
        if(r.plan==Plan::none) {
            set("gate2On",0);set("expOn",0);
            summary+=" | Gate 2 / Expander off ("+hiss+")";addGateStep("off ("+hiss+", the main gate is enough)",false);return;
        }
        set("gateMode",0);
        if(r.plan==Plan::hard) {
            set("gateAfterRig",1);set("expOn",0);
            set("gate2Mode",1);set("gate2Threshold",r.gate2ThresholdDb);set("gate2Ratio",2);set("gate2Attack",.5f);
            set("gate2Hold",10);set("gate2Release",80);set("gate2RangeDb",r.gate2RangeDb);set("gate2On",1);
            summary+=" | Gate after rig + PRE Gate 2 "+juce::String(r.gate2ThresholdDb,0)+" dB";
            addGateStep("main gate after the rig; PRE Gate 2 "+juce::String(r.gate2ThresholdDb,0)+" dB, up to "+juce::String(r.gate2RangeDb,0)+" dB ("+hiss+")",true);
            return;
        }
        set("gatethreshold",r.mainThresholdDb);set("gate2On",0);
        set("expAfterRig",r.expanderAfterRig?1.f:0.f);set("expThreshold",r.expanderThresholdDb);set("expRatio",r.expanderRatio);
        set("expAttack",2);set("expHold",std::max(10.f,raw("gatehold")));set("expRelease",r.expanderReleaseMs);set("expRangeDb",r.expanderRangeDb);set("expOn",1);
        summary+=" | Expander "+juce::String(r.expanderThresholdDb,0)+" dB "+juce::String(r.expanderRatio,1)+":1";
        addGateStep("Expander "+juce::String(r.expanderAfterRig?"after":"before")+" the rig at "+juce::String(r.expanderThresholdDb,0)+" dB, "
                    +juce::String(r.expanderRatio,1)+":1 up to "+juce::String(r.expanderRangeDb,0)+" dB; gate closes below "+juce::String(r.mainThresholdDb,0)+" dB ("+hiss+")",true);
    }
    // The amp the Classic routing plays through: what phase 8 can set.
    AmpTarget ampTarget() const {
        AmpTarget t;t.mode=juce::jlimit(0,2,juce::roundToInt(raw("mode")));t.context=ampNativeContext(t.mode,0);
        t.model=processor.selectedAmpModel(0);t.channel=processor.selectedAmpChannel(0);t.route=processor.selectedAmpNativeRoute(0);
        t.native=ampRequiresNative(t.model) || raw(ampNativeEnabledID(t.context))>.5f;t.on=raw("ampon1")>.5f;
        return t;
    }
    // Why Auto Set cannot set the Classic routing's head, or empty.
    juce::String ampUnavailable(const AmpTarget& t) const {
        if(!(t==ampAtStart))return "amp, channel or input changed while listening";
        if(t.mode!=0)return "Dual and Matrix routing: set those amps in the full editor";
        if(!t.on)return "the amp is switched off";
        if(!t.native)return "classic amp engine; switch the head panel on to let Auto Set set it";
        return {};
    }
    // The amp pass: phase 8 (GAIN and tone knobs), then phase 12 (PRESENCE and
    // RESONANCE) on the knobs phase 8 left, both from the same capture.
    // Phase 20 (the channel) runs first; the two knob steps then work on the
    // channel it chose, against the head as it played.
    void finishAmp(const std::vector<float>& post,const std::vector<float>& input) {
        keepRigSound(post);
        auto t=ampTarget();
        // The head as it played while the pass listened, before any step writes.
        const auto played=processor.autoSetAmpState(t.context);const auto playedKnobs=headKnobs(t);
        std::optional<std::vector<float>> playedAmp;
        if(runOptions.channel && channelStep(t,post,input)) {
            // The knob steps hear the new channel; the cabinet estimate keeps the head that played.
            const auto segment=ampSegment(input);
            playedAmp=nativeAmpRenderer(played,{-1,-1,-1,-1},segment,rate())({});
            t=ampTarget();ampAtStart=t;
        }
        const auto state=playedAmp?processor.autoSetAmpState(t.context):played;
        if(runOptions.amp)ampStep(t,state,post,input,playedAmp?&*playedAmp:nullptr);
        if(runOptions.power)powerStep(t,played,state,playedKnobs,post,input);
        nextPass();
    }
    std::vector<float> ampSegment(const std::vector<float>& input) const {return {input.begin(),input.begin()+std::ptrdiff_t(std::min(input.size(),size_t(rate()*ampRenderSeconds)))};}
    // Phase 20: the head's channel, from each channel rendered on the captured
    // input. Returns whether it switched the channel.
    bool channelStep(const AmpTarget& t,const std::vector<float>& post,const std::vector<float>& input) {
        const auto keep=[this](const juce::String& why){summary+=" | Amp channel unchanged ("+why+")";steps.push_back({Phase::channel,"kept ("+why+")",false});return false;};
        if(locked(lockChannel))return keep("locked");
        if(const auto why=ampUnavailable(t);why.isNotEmpty())return keep(why);
        const auto& panel=ampNativePanel(t.model);
        if(panel.channels.size()<2)return keep("this head has one channel");
        const auto segment=ampSegment(input);
        std::vector<ChannelCandidate> candidates(panel.channels.size());
        for(int c=0;c<int(panel.channels.size());++c) {
            const auto state=processor.autoSetAmpState(t.context,c);
            AmpKnob gain;std::array<int,4> controls{-1,-1,-1,-1};
            if(const int control=easy::ampControl(t.model,c,t.route,easy::Role(ampGain));control>=0)
                if(auto* parameter=processor.parameters().getParameter(ampNativeControlID(t.context,t.model,control,c))) {
                    const auto range=parameter->getNormalisableRange();
                    gain={true,state.values[size_t(control)],range.start,range.end};controls[ampGain]=control;
                }
            const auto fixed=nativeAmpRenderer(state,{-1,-1,-1,-1},segment,rate()),atGain=nativeAmpRenderer(state,controls,segment,rate());
            candidates[size_t(c)]={gain,[fixed,atGain](std::optional<float> g){
                if(!g)return fixed({});
                std::array<float,4> v{};v[ampGain]=*g;return atGain(v);
            }};
        }
        const auto r=analyseChannel(segment,post,rate(),t.channel,candidates,chosenInstrument(),runOptions.style);
        lastChannel=r;
        if(!r.playingDetected)return keep("no playing heard");
        const auto name=[&](int c){return juce::String::fromUTF8(panel.channels[size_t(c)]);};
        const auto pct=[](float v){return juce::String(juce::roundToInt(v*100))+"%";};
        const auto reach=[&](int c){const auto& x=r.reach[size_t(c)];return name(c)+" reaches "+pct(x.low)+"-"+pct(x.high);};
        const auto target="drive target "+pct(r.target);
        if(!r.changed())return keep(r.missBefore<=0?reach(t.channel)+", "+target:reach(t.channel)+", "+target+"; no other channel comes closer");
        set(ampNativeChannelID(t.context,t.model),float(r.proposed));
        if(processor.selectedAmpChannel(0)!=r.proposed)return keep("channel edited while applying");
        const auto move="CHANNEL "+name(r.before)+" > "+name(r.proposed);
        summary+=" | Amp "+move;
        steps.push_back({Phase::channel,move+"; "+reach(r.before)+", "+reach(r.proposed)+", "+target+" (the amp step sets GAIN on it)",true});
        return true;
    }
    // The head's GAIN, tone, PRESENCE and RESONANCE controls with their values now.
    std::vector<std::pair<int,float>> headKnobs(const AmpTarget& t) const {
        std::vector<std::pair<int,float>> knobs;
        const auto add=[&](int control) {
            if(control<0)return;
            const auto id=ampNativeControlID(t.context,t.model,control,t.channel);
            if(processor.parameters().getParameter(id))knobs.push_back({control,raw(id)});
        };
        for(int role=0;role<easy::roleCount;++role)add(easy::ampControl(t.model,t.channel,t.route,easy::Role(role)));
        for(int role=0;role<2;++role)add(powerControl(t.model,t.channel,role));
        return knobs;
    }
    // Phase 8: the head's GAIN, BASS, MIDDLE and TREBLE, from candidates
    // rendered offline through the native amp on the captured input.
    // played: the head as it played, when the channel step switched channels.
    void ampStep(const AmpTarget& t,const AmpNativeState& state,const std::vector<float>& post,const std::vector<float>& input,const std::vector<float>* played) {
        const auto keep=[this](const juce::String& why){summary+=" | Amp unchanged ("+why+")";addStep("kept ("+why+")",false);};
        if(locked(lockAmp)){keep("locked");return;}
        if(const auto why=ampUnavailable(t);why.isNotEmpty()){keep(why);return;}
        std::array<AmpKnob,4> knobs{};std::array<int,4> controls{-1,-1,-1,-1};std::array<juce::String,4> ids;
        for(int role=0;role<easy::roleCount;++role)if(const int control=easy::ampControl(t.model,t.channel,t.route,easy::Role(role));control>=0) {
            ids[size_t(role)]=ampNativeControlID(t.context,t.model,control,t.channel);
            if(auto* parameter=processor.parameters().getParameter(ids[size_t(role)])) {
                const auto range=parameter->getNormalisableRange();
                knobs[size_t(role)]={true,raw(ids[size_t(role)]),range.start,range.end};controls[size_t(role)]=control;
            }
        }
        if(std::none_of(knobs.begin(),knobs.end(),[](const auto& k){return k.present;})){keep("this head has no GAIN or tone knobs on its current channel");return;}
        std::vector<float> segment(input.begin(),input.begin()+std::min(input.size(),size_t(rate()*ampRenderSeconds)));
        const auto r=analyseAmp(segment,post,rate(),knobs,nativeAmpRenderer(state,controls,segment,rate()),chosenInstrument(),runOptions.style,played);
        lastAmp=r;
        if(!r.playingDetected){keep("no playing heard");return;}
        if(r.changedKnobs()==0){keep(juce::String(r.bass?"bass":"guitar")+" "+styleName(runOptions.style)+" drive and tone already near the target");return;}
        juce::StringArray moves;
        for(int role=0;role<easy::roleCount;++role)if(r.changed(role)) {
            const auto& before=r.before[size_t(role)];
            const float value=before.value+strength()*(r.proposed[size_t(role)].value-before.value);
            set(ids[size_t(role)],value);
            moves.add(juce::String(ampRoleNames[role])+" "+juce::String(before.value,2)+" > "+juce::String(raw(ids[size_t(role)]),2));
        }
        const auto drive="drive "+juce::String(juce::roundToInt(r.compressionBefore*100))+"% > "+juce::String(juce::roundToInt(r.compressionAfter*100))+"% (target "
                        +juce::String(juce::roundToInt(r.compressionTarget*100))+"%"+(r.gainReachable?")":", out of this knob's reach)");
        summary+=" | Amp "+moves.joinIntoString(", ");
        addStep(moves.joinIntoString(", ")+"; "+drive,true);
    }
    // Phase 12: the head's PRESENCE and RESONANCE (or DEPTH), rendered on the
    // knobs the amp step left.
    // playedKnobs: the head's knobs while the pass listened (headKnobs).
    // played: the head as it played; state: the head the knobs are set on (another channel after phase 20).
    void powerStep(const AmpTarget& t,const AmpNativeState& played,const AmpNativeState& state,const std::vector<std::pair<int,float>>& playedKnobs,const std::vector<float>& post,const std::vector<float>& input) {
        const auto keep=[this](const juce::String& why){summary+=" | Presence/resonance unchanged ("+why+")";steps.push_back({Phase::power,"kept ("+why+")",false});};
        if(locked(lockPower)){keep("locked");return;}
        if(const auto why=ampUnavailable(t);why.isNotEmpty()){keep(why);return;}
        std::array<AmpKnob,2> knobs{};std::array<int,2> controls{-1,-1};std::array<juce::String,2> ids;
        for(int role=0;role<2;++role)if(const int control=powerControl(t.model,t.channel,role);control>=0) {
            ids[size_t(role)]=ampNativeControlID(t.context,t.model,control,t.channel);
            if(auto* parameter=processor.parameters().getParameter(ids[size_t(role)])) {
                const auto range=parameter->getNormalisableRange();
                knobs[size_t(role)]={true,raw(ids[size_t(role)]),range.start,range.end};controls[size_t(role)]=control;
            }
        }
        if(!knobs[0].present && !knobs[1].present){keep("this head has no PRESENCE or RESONANCE knob on its current channel");return;}
        std::vector<float> segment(input.begin(),input.begin()+std::min(input.size(),size_t(rate()*ampRenderSeconds)));
        const auto playedAmp=nativePowerRenderer(played,playedKnobs,{-1,-1},segment,rate())({0.f,0.f});
        const auto r=analysePower(segment,post,playedAmp,rate(),knobs,nativePowerRenderer(state,headKnobs(t),controls,segment,rate()),chosenInstrument(),runOptions.style);
        lastPower=r;
        if(!r.playingDetected){keep("no playing heard");return;}
        const auto balance=[&](int role){
            const auto db=[](float v){return (v>=0?"+":"")+juce::String(v,1);};
            return juce::String(role==powerPresence?"bite ":"low end ")+db(r.excessBeforeDb[size_t(role)])+" > "+db(r.excessAfterDb[size_t(role)])+" dB";
        };
        if(r.changedKnobs()==0){keep(juce::String(r.bass?"bass":"guitar")+" "+styleName(runOptions.style)+" bite and low end already near the target");return;}
        juce::StringArray moves,balances;
        for(int role=0;role<2;++role)if(r.changed(role)) {
            const auto& before=r.before[size_t(role)];
            set(ids[size_t(role)],before.value+strength()*(r.proposed[size_t(role)].value-before.value));
            moves.add(juce::String(powerRoleNames[role])+" "+juce::String(before.value,2)+" > "+juce::String(raw(ids[size_t(role)]),2));
            balances.add(balance(role));
        }
        summary+=" | Amp "+moves.joinIntoString(", ");
        steps.push_back({Phase::power,moves.joinIntoString(", ")+"; "+balances.joinIntoString(", ")+" from the target",true});
    }
    std::optional<bool> chosenInstrument() const {return runOptions.instrument==0?std::nullopt:std::optional<bool>(runOptions.instrument==2);}
    BoostTarget boostTarget() const {
        BoostTarget t;t.amp=ampTarget();t.board=raw("boardEnabled")>.5f;
        for(int position=0;position<pedalBoardCapacity;++position)t.order[size_t(position)]=juce::roundToInt(raw(pedalOrderID(position)));
        for(int owner=0;owner<pedalBoardCapacity;++owner) {
            const int model=juce::jlimit(0,pedalModelCount-1,juce::roundToInt(raw(pedalModelID(owner))));t.models[size_t(owner)]=model;
            const juce::String category=pedalModel(model).category;
            if(model>0 && raw(pedalBypassID(owner,model))<.5f && (category=="DRIVE" || category=="FUZZ" || category=="BOOST"))t.driveOn=true;
            // A pedal with its own BOOST switched on (Power Wah, Multi Wah, Para EQ) already boosts.
            if(model>0 && raw(pedalBypassID(owner,model))<.5f) {
                const auto& controls=pedalModel(model).controls;
                for(int c=0;c<pedalModel(model).controlCount;++c)if(juce::String(controls[size_t(c)].id)=="boost.on" && raw(pedalControlID(owner,model,c))>.5f)t.driveOn=true;
            }
        }
        // The pedal goes last, right before the amp: the last position, if it is empty.
        if(const int owner=t.order[size_t(pedalBoardCapacity-1)];owner>=0 && owner<pedalBoardCapacity && t.models[size_t(owner)]==0)t.owner=owner;
        return t;
    }
    // Phase 10: a Green Drive in front of a driven amp whose low end is loose,
    // from renders of the captured input through the pedal and the native amp.
    void finishBoost(const std::vector<float>& input) {
        const auto keep=[this](const juce::String& why){summary+=" | Boost unchanged ("+why+")";addStep("kept ("+why+")",false);nextPass();};
        const auto t=boostTarget();
        if(locked(lockBoost)){keep("locked");return;}
        if(!(t==boostAtStart)){keep("pedal board or amp changed while listening");return;}
        if(t.amp.mode!=0){keep("Dual and Matrix routing: add a boost in the full editor");return;}
        if(!t.amp.on){keep("the amp is switched off");return;}
        if(!t.amp.native){keep("classic amp engine; switch the head panel on to let Auto Set judge a boost");return;}
        if(runOptions.instrument==2){keep("bass: a boost is for driven guitar amps");return;}
        if(runOptions.style==Style::clean){keep("clean style: no boost in front of a clean amp");return;}
        if(!t.board){keep("the PRE pedal board is off");return;}
        if(t.driveOn){keep("a drive, fuzz or boost is already on the board");return;}
        if(t.owner<0){keep("no free board slot after your last pedal");return;}
        const std::optional<bool> instrument=runOptions.instrument==0?std::nullopt:std::optional<bool>(false);
        std::vector<float> segment(input.begin(),input.begin()+std::min(input.size(),size_t(rate()*ampRenderSeconds)));
        const auto r=analyseBoost(segment,rate(),nativeBoostRenderer(processor.autoSetAmpState(t.amp.context),segment,rate()),instrument,runOptions.style);
        lastBoost=r;
        if(!r.playingDetected){keep("no playing heard");return;}
        if(r.bass){keep("bass: a boost is for driven guitar amps");return;}
        const auto low="low end "+juce::String(r.lowExcessBeforeDb,1)+" dB over the target";
        if(!r.engaged){keep(r.lowExcessBeforeDb<=boostLooseLowDb?"low end already tight":low+", a boost does not tighten it here");return;}
        // Strength scales the drive and the level change in dB (LEVEL 0.5 is unity).
        const float levelDb=juce::Decibels::gainToDecibels(4*r.setting.level*r.setting.level);
        const float level=std::sqrt(juce::Decibels::decibelsToGain(strength()*levelDb)/4.f),drive=strength()*r.setting.drive;
        set(pedalControlID(t.owner,boostPedalModel,0),drive);set(pedalControlID(t.owner,boostPedalModel,1),r.setting.tone);
        set(pedalControlID(t.owner,boostPedalModel,2),level);set(pedalBypassID(t.owner,boostPedalModel),0);set(pedalModelID(t.owner),float(boostPedalModel));
        const auto placed=juce::String(pedalModel(boostPedalModel).name)+" in slot "+juce::String(t.owner+1)+": OVER DRIVE "+juce::String(raw(pedalControlID(t.owner,boostPedalModel,0)),2)
                         +", TONE "+juce::String(raw(pedalControlID(t.owner,boostPedalModel,1)),2)+", LEVEL "+juce::String(raw(pedalControlID(t.owner,boostPedalModel,2)),2);
        summary+=" | Boost "+placed;
        addStep(placed+"; "+low+" > "+juce::String(r.lowExcessAfterDb,1)+" dB",true);
        nextPass();
    }
    // Phase 11: the POST delay and reverb from the playing the input pass heard.
    // An effect the player has on keeps its type and settings; only its mix moves.
    void finishSpace() {
        if(!spacePending)return;
        spacePending=false;flushLocked(Phase::sync);
        const auto addSpaceStep=[this](const juce::String& result,bool changed){steps.push_back({Phase::space,result,changed});};
        const auto bass=chosenBass();
        // Phase 22 first: it may give the delay a tempo to be timed to.
        const float hostBpm=runOptions.sync?syncStep(bass.value_or(false)):0.f;
        flushLocked(Phase::space);
        if(spaceWindows.empty()){addSpaceStep("kept (the input pass did not run, so no playing was heard)",false);return;}
        const bool synced=raw("delaysync")>.5f,host=raw("temposync")>.5f;
        // The processor reports the host tempo it follows from its next callback: use the one just read.
        const float syncBpm=synced?(host?(hostBpm>0?hostBpm:processor.currentHostTempo()):raw("tempo")):0.f;
        const auto r=analyseSpace(spaceWindows,bass.value_or(false),runOptions.style,syncBpm);
        lastSpace=r;
        if(!r.feel.measured){addSpaceStep("kept (too few notes to judge the playing)",false);return;}
        const auto feel=juce::String(r.feel.onsetsPerSecond,1)+" detected notes/s, peak "+juce::String(r.feel.peakOnsetsPerSecond,0)
                       +", "+juce::String(juce::roundToInt(r.feel.pauseFraction*100))+"% pauses";
        const auto scaled=[this](float before,float target){return std::round((before+strength()*(target-before))*100.f)/100.f;};
        juce::StringArray moves;
        if(raw("reverbon")>.5f) {
            const float before=raw("reverbmix");set("reverbmix",scaled(before,r.reverb.mix));
            if(raw("reverbmix")!=before)moves.add("reverb mix "+juce::String(before,2)+" > "+juce::String(raw("reverbmix"),2));
        } else if(r.reverb.on) {
            set("reverbsize",r.reverb.size);set("reverbdamping",r.reverb.damping);set("reverbmix",scaled(0,r.reverb.mix));set("reverbon",1);
            moves.add("reverb on: mix "+juce::String(raw("reverbmix"),2)+", size "+juce::String(raw("reverbsize"),2));
        }
        if(raw("delayon")>.5f) {
            const float before=raw("delaymix"),target=r.delay.on?r.delay.mix:std::round(before*(r.dense?.6f:1.f)*100.f)/100.f;
            set("delaymix",scaled(before,target));
            if(raw("delaymix")!=before)moves.add("delay mix "+juce::String(before,2)+" > "+juce::String(raw("delaymix"),2));
        } else if(r.delay.on) {
            set("delayfeedback",r.delay.feedback);set("delaymix",scaled(0,r.delay.mix));set("delayon",1);
            moves.add("delay on: "+juce::String(host?"host tempo ":"manual/TAP tempo ")+juce::String(syncBpm,1)+" BPM"
                      +", feedback "+juce::String(raw("delayfeedback"),2)+", mix "+juce::String(raw("delaymix"),2));
        }
        juce::String delayReason;
        if(raw("delayon")<=.5f) {
            if(bass.value_or(false) || runOptions.style==Style::metal)delayReason="; delay kept off: style";
            else if(r.feel.peakOnsetsPerSecond>spaceDelayMaxOnsets)delayReason="; delay kept off: fast phrase";
            else if(!synced)delayReason="; delay kept off: onset gaps do not establish tempo; select DELAY SYNC";
            else delayReason="; delay kept off: no usable sync tempo (40-240 BPM)";
        }
        const auto who=juce::String(bass.value_or(false)?"bass":"guitar")+(bass?"":" (assumed)")+" "+styleName(runOptions.style);
        if(moves.isEmpty()){summary+=" | Delay + reverb unchanged";addSpaceStep("kept ("+who+": "+feel+")"+delayReason,false);return;}
        summary+=" | "+moves.joinIntoString(", ");
        addSpaceStep(moves.joinIntoString("; ")+" ("+who+": "+feel+")"+delayReason,true);
    }
    // Phase 22: when the delay is off and unsynced, the playing would take a
    // delay, and the host reports a tempo, turn on DELAY SYNC following the host
    // tempo, so the delay + reverb step can time the delay to it. Returns the host
    // tempo it synced to (0: kept).
    float syncStep(bool bass) {
        const auto keep=[this](const juce::String& why){summary+=" | Delay sync unchanged ("+why+")";steps.push_back({Phase::sync,"kept ("+why+")",false});return 0.f;};
        if(locked(lockSync))return keep("locked");
        if(spaceWindows.empty())return keep("the input pass did not run, so no playing was heard");
        if(raw("delaysync")>.5f)return keep(juce::String("DELAY SYNC is already on (")+(raw("temposync")>.5f?"host":"manual/TAP")+" tempo)");
        if(raw("delayon")>.5f)return keep("the delay is on with its own time");
        const float host=processor.reportedHostTempo();
        const auto r=analyseTempoSync(spaceWindows,bass,runOptions.style,host);
        lastSync=r;
        if(!r.feel.measured)return keep("too few notes to judge the playing");
        if(!r.hostTempo)return keep("the host reports no tempo (40-240 BPM): set Chimera's TEMPO or TAP and select DELAY SYNC");
        if(!r.delayWanted)return keep(juce::String(r.reason)+"; a delay would not be added");
        set("delaysync",1);set("temposync",1);
        if(raw("delaysync")<=.5f || raw("temposync")<=.5f)return keep("delay sync edited while applying");
        const auto move="DELAY SYNC on, following the host tempo ("+juce::String(host,1)+" BPM)";
        summary+=" | "+move;
        steps.push_back({Phase::sync,move+"; the delay + reverb step times the delay to it",true});
        return host;
    }
    // The latest rig sound a sample pass heard (amp/cab output or Tone EQ input), for phase 14.
    void keepRigSound(const std::vector<float>& heard) {
        if(widthPending)widthSamples.assign(heard.begin(),heard.begin()+std::ptrdiff_t(std::min(heard.size(),size_t(rate()*(widthRenderSeconds+widthWarmupSeconds)))));
    }
    // Phase 14: the stereo doubler, a result step after the delay and reverb
    // (the doubler follows POST), just before the Final EQ and output pass.
    void finishWidth() {
        if(!widthPending)return;
        widthPending=false;flushLocked(Phase::width);
        const auto add=[this](const juce::String& result,bool changed){steps.push_back({Phase::width,result,changed});};
        const auto keep=[&](const juce::String& why){summary+=" | Doubler unchanged ("+why+")";add("kept ("+why+")",false);};
        if(processor.getMainBusNumOutputChannels()<2){keep("mono output: the doubler needs stereo");return;}
        if(widthSamples.empty()){keep("no rig sound heard: turn on the Tone EQ, amp or cabinet mic step");return;}
        const bool on=raw("doubleron")>.5f;const float spread=raw("doublertime");
        const auto r=analyseWidth(widthSamples,rate(),spaceWindows,on,spread,chosenBass(),runOptions.style);
        lastWidth=r;
        if(!r.playingDetected){keep("no playing heard");return;}
        if(r.bass){keep("bass: the low end stays centred");return;}
        if(r.fast && !on){keep("fast playing: a doubled copy would smear it");return;}
        const auto ms=[](float v){return juce::String(v,1)+" ms";};
        if(!r.changed()) {
            if(on)keep(juce::String(styleName(runOptions.style))+" spread "+ms(spread)+" already smooth in mono");
            else keep("every spread combs when summed to mono or barely widens");
            return;
        }
        juce::String move;
        if(!on) {
            set("doublertime",r.spreadAfter);set("doubleron",1);
            move="doubler on: spread "+ms(raw("doublertime"));
        } else {
            set("doublertime",spread+strength()*(r.spreadAfter-spread));
            move="spread "+ms(spread)+" > "+ms(raw("doublertime"));
        }
        const auto detail="width "+juce::String(r.widthAfter,2)+", mono roughness "+juce::String(r.monoAfterDb,1)+" dB"+(r.fast?" (fast playing: shorter spread)":"");
        summary+=" | Doubler "+move;
        add(move+"; "+detail,true);
    }
    CabTarget cabTarget() const {
        CabTarget t;t.mode=juce::jlimit(0,2,juce::roundToInt(raw("mode")));t.on=raw("cab1")>.5f;
        const bool modelA=raw(originalCabID(0,"Aon"))>.5f,modelB=raw(originalCabID(0,"Bon"))>.5f;
        const float blend=cabMicRouting(modelA,modelB,juce::roundToInt(raw("cabtype1")),juce::roundToInt(raw("cabBtype1")),raw("cabblend1")).blend;
        t.mic=blend<=0.f?0:blend>=1.f?1:-1;
        if(t.mic>=0)t.key=OriginalCabParameters::savedKey(0,t.mic,[this](const juce::String& id){return raw(id);});
        return t;
    }
    DualTarget dualTarget() const {return {juce::jlimit(0,2,juce::roundToInt(raw("mode"))),raw("dualtype")>.5f,nativeLevel(1,1)};}
    // The Dual pass: phase 19 (rig 2's level), then phase 13 (the blend or
    // crossover), which hears rig 2 at the level phase 19 applied.
    void finishDual(const std::vector<float>& rig1,const std::vector<float>& rig2In) {
        // Phase 21 first: the later steps hear rig 2 at the polarity it set
        // (the lane taps are after the lane's polarity, so a flip negates it).
        std::vector<float> flipped;
        const std::vector<float>* rig2=&rig2In;
        if(runOptions.polarity) {
            const bool crossover=raw("dualtype")>.5f;const float blend=raw("dualblend");
            const auto flips=polarityStep({&rig1,&rig2In},crossover?std::vector<float>{1.f,1.f}:std::vector<float>{1-blend,blend},{"rig 2"});
            if(!flips.empty() && flips[0]){flipped=rig2In;for(auto& v:flipped)v=-v;rig2=&flipped;}
        }
        float appliedDb=0;
        if(runOptions.rigs)appliedDb=rigsStep(rig1,*rig2);
        if(runOptions.dual) {
            if(appliedDb==0.f)dualStep(rig1,*rig2);
            else {
                // The level controls are linear, so rig 2 at its new level is the capture scaled.
                auto scaled=*rig2;const float g=juce::Decibels::decibelsToGain(appliedDb);
                for(auto& v:scaled)v*=g;
                dualStep(rig1,scaled);
            }
        }
        nextPass();
    }
    std::array<bool,2> lanePolarity() const {return {raw("polarity2")>.5f,raw("polarity3")>.5f};}
    // Phase 21: flips the polarity of the lanes after the reference (lane 1:
    // Dual rig 2 / Matrix MID, lane 2: Matrix HIGH) when that removes their
    // cancellation. Returns which lanes it flipped (empty when it kept them).
    std::vector<bool> polarityStep(const std::vector<const std::vector<float>*>& lanes,const std::vector<float>& weights,const std::vector<const char*>& names) {
        const auto keep=[this](const juce::String& why){summary+=" | Lane polarity unchanged ("+why+")";steps.push_back({Phase::polarity,"kept ("+why+")",false});return std::vector<bool>{};};
        if(locked(lockPolarity))return keep("locked");
        const int mode=juce::jlimit(0,2,juce::roundToInt(raw("mode")));
        if(mode==1 ? !(dualTarget()==dualAtStart) : !(matrixTarget()==matrixAtStart))return keep("routing, Dual type or a lane's head changed while listening");
        if(lanePolarity()!=polarityAtStart)return keep("lane polarity edited while listening");
        const auto r=analysePolarity(lanes,weights,rate(),chosenInstrument());
        lastPolarity=r;
        if(!r.playingDetected)return keep("no playing heard");
        for(size_t k=0;k<r.heard.size();++k)if(!r.heard[k])
            return keep(juce::String(k==0?(mode==1?"rig 1":"the LOW lane"):juce::String(names[k-1])+(mode==1?"":" lane"))+" is silent (muted or soloed away)");
        const auto db=[](float v){return juce::String(v,1)+" dB";};
        if(!r.changed())return keep(r.cancellationBeforeDb<=polarityMinCancellationDb
            ?"the "+juce::String(mode==1?"rigs":"lanes")+" add up (worst band "+db(r.cancellationBeforeDb)+" below their sum)"
            :"a flip does not reduce the cancellation ("+db(r.cancellationBeforeDb)+"): check delay or crossover");
        std::vector<bool> applied(r.flip.size(),false);juce::StringArray moves;
        for(size_t k=0;k<r.flip.size();++k)if(r.flip[k]) {
            const auto id="polarity"+juce::String(int(k)+2);const bool before=raw(id)>.5f;
            set(id,before?0.f:1.f);
            if((raw(id)>.5f)==before)continue; // edited by the host while applying: kept
            applied[k]=true;
            moves.add(juce::String(names[k])+" POLARITY "+(before?"inverted":"normal")+" > "+(before?"normal":"inverted"));
        }
        if(moves.isEmpty())return keep("lane polarity edited while applying");
        summary+=" | "+moves.joinIntoString(", ");
        steps.push_back({Phase::polarity,moves.joinIntoString(", ")+" ("+juce::String(mode==1?"rig 1":"LOW")+" kept as the reference); cancellation "
                         +db(r.cancellationBeforeDb)+" > "+db(r.cancellationAfterDb)+" in the worst band",true});
        return applied;
    }
    // Phase 19: rig 2's level, matched to rig 1. Returns the change applied (dB).
    float rigsStep(const std::vector<float>& rig1,const std::vector<float>& rig2) {
        const auto keep=[this](const juce::String& why){summary+=" | Dual rig levels unchanged ("+why+")";steps.push_back({Phase::rigs,"kept ("+why+")",false});return 0.f;};
        const auto t=dualTarget();
        if(locked(lockRigs))return keep("locked");
        if(!(t==dualAtStart))return keep("routing, Dual type or rig 2's head changed while listening");
        if(t.mode!=1)return keep("Classic or Matrix routing: no Dual rigs");
        if(t.crossover)return keep(rigsCrossoverWhy);
        const auto id=laneLevelID(1,1);const float current=raw(id);
        const auto range=processor.parameters().getParameter(id)->getNormalisableRange();
        const auto r=analyseRigLevels(rig1,rig2,rate(),raw("dualblend"),chosenInstrument(),{range.start-current,range.end-current});
        lastRigs=r;
        if(!r.playingDetected)return keep("no playing heard");
        if(!r.rig1Heard || !r.rig2Heard)return keep(juce::String(!r.rig1Heard?"rig 1":"rig 2")+" is silent (muted or soloed away)");
        const auto db=[](float v){return (v>0?"+":"")+juce::String(v,1)+" dB";};
        const auto heard="rig 2 "+juce::String(std::abs(r.differenceDb()),1)+" dB "+(r.differenceDb()>0?"quieter":"louder")+" than rig 1";
        if(!r.changed())return keep(juce::String(r.limited?"rig 2's level is at the end of its range; ":"rigs already within 1 dB; ")+heard);
        set(id,current+strength()*r.changeDb);
        const float applied=raw(id)-current;
        const auto move="rig 2"+juce::String(t.native2?" OUTPUT LEVEL ":" LEVEL ")+db(current)+" > "+db(raw(id));
        summary+=" | Dual "+move;
        steps.push_back({Phase::rigs,move+" (rig 1 kept as the reference; "+heard+(r.limited?", limited to "+db(r.changeDb):juce::String())+"); level "
                         +juce::String(r.levelChangeDb,1)+" dB at the current blend (the output pass restores it)",true});
        return applied;
    }
    // Phase 13: the Dual blend, or the Dual crossover, from each rig heard on its own.
    void dualStep(const std::vector<float>& rig1,const std::vector<float>& rig2) {
        const auto keep=[this](const juce::String& why){summary+=" | Dual unchanged ("+why+")";steps.push_back({Phase::dual,"kept ("+why+")",false});};
        const auto t=dualTarget();
        if(locked(lockDual)){keep("locked");return;}
        if(!(t==dualAtStart)){keep("routing or Dual type changed while listening");return;}
        if(t.mode!=1){keep("Classic or Matrix routing: no Dual blend");return;}
        const auto id=juce::String(t.crossover?"dualcross":"dualblend");
        const float current=raw(id);
        const auto r=analyseDual(rig1,rig2,rate(),t.crossover,current,chosenInstrument(),runOptions.style);
        lastDual=r;
        if(!r.playingDetected){keep("no playing heard");return;}
        if(!r.rig1Heard || !r.rig2Heard){keep(juce::String(!r.rig1Heard?"rig 1":"rig 2")+" is silent (muted or soloed away)");return;}
        const auto value=[&](float v){return t.crossover?juce::String(juce::roundToInt(v))+" Hz":juce::String(juce::roundToInt(v*100))+"% rig 2";};
        const auto fit="tone error "+juce::String(r.toneErrorBeforeDb,1)+" > "+juce::String(r.toneErrorAfterDb,1)+" dB";
        const auto cancel=!t.crossover && r.cancellationDb>dualCancellationDb?juce::String("; the rigs partly cancel (")+juce::String(r.cancellationDb,1)+" dB): check polarity or delay":juce::String();
        if(!r.changed()){keep(juce::String(r.bass?"bass":"guitar")+" "+styleName(runOptions.style)+" "+(t.crossover?"crossover":"blend")+" already near the target"+cancel);return;}
        set(id,current+strength()*(r.proposed-current));
        const auto move=juce::String(t.crossover?"CROSSOVER ":"BLEND ")+value(current)+" > "+value(raw(id));
        summary+=" | Dual "+move;
        steps.push_back({Phase::dual,move+"; "+fit+"; level "+juce::String(r.levelChangeDb,1)+" dB (the output pass restores it)"+cancel,true});
    }
    MatrixTarget matrixTarget() const {
        MatrixTarget t{juce::jlimit(0,2,juce::roundToInt(raw("mode"))),{}};
        for(const int lane:{1,2})t.native[size_t(lane-1)]=nativeLevel(lane);
        return t;
    }
    // A Matrix lane's level is its native head's OUTPUT LEVEL when the lane
    // runs a native head (the engine then ignores the lane LEVEL), else LEVEL.
    bool nativeLevel(int lane,int mode=2) const {
        const int context=ampNativeContext(mode,lane);
        // As AmpNativeParameterCache::read: an unknown model falls back to model 2.
        const int stored=juce::roundToInt(juce::jlimit(0.f,255.f,raw(ampNativeModelID(context))));
        const int model=stored<ampModelCount?stored:2;
        return ampRequiresNative(model) || raw(ampNativeEnabledID(context))>.5f;
    }
    juce::String laneLevelID(int mode,int lane) const {return nativeLevel(lane,mode)?ampNativeOutputLevelID(ampNativeContext(mode,lane)):"level"+juce::String(lane+1);}
    juce::String matrixLevelID(int lane) const {return laneLevelID(2,lane);}
    // Phase 17: the Matrix MID and HIGH lane levels, from each lane heard on its own.
    // The Matrix pass: phase 21 (lane polarity), then phase 17 (MID and HIGH
    // levels), which hears the lanes at the polarity phase 21 set.
    void finishMatrix(const std::vector<float>& low,const std::vector<float>& midIn,const std::vector<float>& highIn) {
        // Copy a lane only when phase 21 flips it.
        std::vector<float> flippedMid,flippedHigh;
        const std::vector<float>* mid=&midIn;const std::vector<float>* high=&highIn;
        if(runOptions.polarity) {
            const auto flips=polarityStep({&low,&midIn,&highIn},{1.f,1.f,1.f},{"MID","HIGH"});
            if(flips.size()==2) {
                if(flips[0]){flippedMid=midIn;for(auto& v:flippedMid)v=-v;mid=&flippedMid;}
                if(flips[1]){flippedHigh=highIn;for(auto& v:flippedHigh)v=-v;high=&flippedHigh;}
            }
        }
        if(runOptions.matrix)matrixStep(low,*mid,*high);
        nextPass();
    }
    void matrixStep(const std::vector<float>& low,const std::vector<float>& mid,const std::vector<float>& high) {
        const auto keep=[this](const juce::String& why){summary+=" | Matrix levels unchanged ("+why+")";steps.push_back({Phase::matrix,"kept ("+why+")",false});};
        const auto t=matrixTarget();
        if(locked(lockMatrix)){keep("locked");return;}
        if(!(t==matrixAtStart)){keep("routing or a lane's head changed while listening");return;}
        if(t.mode!=2){keep("Classic or Dual routing: no Matrix lanes");return;}
        std::array<juce::String,2> ids;std::array<float,2> current{};std::array<std::pair<float,float>,2> room{};
        for(size_t k=0;k<2;++k) {
            ids[k]=matrixLevelID(int(k)+1);current[k]=raw(ids[k]);
            const auto range=processor.parameters().getParameter(ids[k])->getNormalisableRange();
            room[k]={range.start-current[k],range.end-current[k]};
        }
        const auto r=analyseMatrix(low,mid,high,rate(),chosenInstrument(),runOptions.style,room);
        lastMatrix=r;
        if(!r.playingDetected){keep("no playing heard");return;}
        if(!r.heard[0] || !r.heard[1] || !r.heard[2]){keep(juce::String(!r.heard[0]?"the LOW":!r.heard[1]?"the MID":"the HIGH")+" lane is silent (muted or soloed away)");return;}
        const auto fit="tone error "+juce::String(r.toneErrorBeforeDb,1)+" > "+juce::String(r.toneErrorAfterDb,1)+" dB";
        const auto cancel=r.cancellationDb>matrixCancellationDb?juce::String("; the lanes partly cancel at the crossovers (")+juce::String(r.cancellationDb,1)+" dB): check polarity":juce::String();
        if(!r.changed()){keep(juce::String(r.bass?"bass":"guitar")+" "+styleName(runOptions.style)+" lane balance already near the target"+cancel);return;}
        juce::String move;
        constexpr const char* names[]{"MID","HIGH"};
        for(size_t k=0;k<2;++k)if(r.changeDb[k]!=0.f) {
            set(ids[k],current[k]+strength()*r.changeDb[k]);
            const auto db=[](float v){return (v>0?"+":"")+juce::String(v,1)+" dB";};
            move+=(move.isEmpty()?"":", ")+juce::String(names[k])+(t.native[k]?" OUTPUT LEVEL ":" LEVEL ")+db(current[k])+" > "+db(raw(ids[k]));
        }
        summary+=" | Matrix "+move;
        steps.push_back({Phase::matrix,move+" (LOW kept as the reference); "+fit+"; level "+juce::String(r.levelChangeDb,1)+" dB (the output pass restores it)"+cancel,true});
    }
    // Phase 18: the Classic cabinet's mic A and mic B, each heard on its own.
    void finishMics(const std::vector<float>& micA,const std::vector<float>& micB) {
        const auto keep=[this](const juce::String& why){summary+=" | Cabinet mics unchanged ("+why+")";addStep("kept ("+why+")",false);nextPass();};
        const auto t=cabTarget();
        if(locked(lockMics)){keep("locked");return;}
        if(!(t==micsAtStart)){keep("routing, cabinet or mic sources changed while listening");return;}
        if(t.mode!=0 || !t.on || t.mic!=-1){keep("both mics are no longer heard");return;}
        const std::array<float,2> delays{raw("cabAdelay1"),raw("cabBdelay1")};const bool invert=raw("cabBinvert1")>.5f;const float blend=raw("cabblend1");
        const auto r=analyseMics(micA,micB,rate(),delays,invert,blend,chosenInstrument(),runOptions.style);
        lastMics=r;
        if(!r.playingDetected){keep("no playing heard");return;}
        if(!r.aHeard || !r.bHeard){keep(juce::String(!r.aHeard?"mic A":"mic B")+" is silent");return;}
        const auto fit="tone error "+juce::String(r.toneErrorBeforeDb,1)+" > "+juce::String(r.toneErrorAfterDb,1)+" dB";
        const auto lag="mic B "+juce::String(r.lagMs,2)+" ms after mic A, correlation "+juce::String(r.correlation,2);
        if(!r.changed()){keep(juce::String(r.coherent?"mics already aligned and blended near the target; ":"mics too different to align; ")+lag);return;}
        juce::String move;
        const auto ms=[](float v){return juce::String(v,2)+" ms";};
        if(r.aligned()) {
            // Alignment is applied whole: a part-way delay would leave the mics combing.
            if(r.delayAfterMs[0]!=r.delayBeforeMs[0]){set("cabAdelay1",r.delayAfterMs[0]);move+="mic A DELAY "+ms(r.delayBeforeMs[0])+" > "+ms(raw("cabAdelay1"));}
            if(r.delayAfterMs[1]!=r.delayBeforeMs[1]){set("cabBdelay1",r.delayAfterMs[1]);move+=juce::String(move.isEmpty()?"":", ")+"mic B DELAY "+ms(r.delayBeforeMs[1])+" > "+ms(raw("cabBdelay1"));}
            if(r.invertAfter!=r.invertBefore){set("cabBinvert1",r.invertAfter?1.f:0.f);move+=juce::String(move.isEmpty()?"":", ")+"mic B POLARITY "+(r.invertAfter?"inverted":"normal");}
            move+=" (cancellation "+juce::String(r.cancellationBeforeDb,1)+" > "+juce::String(r.cancellationAfterDb,1)+" dB)";
        }
        if(r.blendAfter!=r.blendBefore) {
            set("cabblend1",blend+strength()*(r.blendAfter-blend));
            move+=juce::String(move.isEmpty()?"":"; ")+"BLEND "+juce::String(juce::roundToInt(blend*100))+"% > "+juce::String(juce::roundToInt(raw("cabblend1")*100))+"% mic B";
        }
        summary+=" | Cabinet mics "+move;
        addStep(move+"; "+fit+"; "+lag,true);
        nextPass();
    }
    // Phase 9: the heard modeled mic's position and distance, from the model's
    // own response, with the mic's level holding the loudness.
    // The cabinet mic pass: phase 9 (mic placement), then phase 15 (cuts),
    // which also counts in the mic move phase 9 applied.
    // Phase 23 (factory IR) serves a mic phase 9 cannot place; its change also counts for the cuts.
    void finishCab(const std::vector<float>& post) {
        keepRigSound(post);cabMoveDb={};
        if(runOptions.cab)cabStep(post);
        if(runOptions.cabIR)cabIRStep(post);
        if(runOptions.cuts)cutsStep(post);
        nextPass();
    }
    void cabStep(const std::vector<float>& post) {
        const auto keep=[this](const juce::String& why){summary+=" | Cab mic unchanged ("+why+")";addStep("kept ("+why+")",false);};
        const auto t=cabTarget();
        if(locked(lockCab)){keep("locked");return;}
        if(!(t==cabAtStart)){keep("cabinet or mic changed while listening");return;}
        if(t.mode!=0){keep("Dual and Matrix routing: place those mics in the full editor");return;}
        if(!t.on){keep("the cabinet is switched off");return;}
        if(t.mic<0){keep("two mics are blended; set the blend fully to one mic to let Auto Set place it");return;}
        if(t.key==0){keep(juce::String("mic ")+(t.mic?"B":"A")+" is a captured IR or filters; switch on its modeled mic to let Auto Set place it");return;}
        const juce::String mic=t.mic?"B":"A";
        const auto positionID=originalCabID(0,(mic+"position").toRawUTF8()),distanceID=originalCabID(0,(mic+"distance").toRawUTF8()),gainID="cab"+mic+"gain1";
        const juce::StringArray controls{positionID,distanceID,gainID};
        std::vector<Snapshot> previous;
        for(const auto& entry:snapshot)if(controls.contains(entry.parameter->paramID))previous.push_back(entry);
        const auto intact=[&]{
            if(previous.size()!=3)return false;
            for(const auto& entry:snapshot)if(controls.contains(entry.parameter->paramID) && !owned(entry))return false;
            return true;
        };
        // Position, distance and level are one measured placement. Even an
        // A-B-A edit must not leave just part of a safe candidate writable.
        if(!intact()){manualEditsKept=true;keep("mic controls edited while listening");return;}
        const auto current=cabLayout::settings(t.key);
        const std::optional<bool> instrument=runOptions.instrument==0?std::nullopt:std::optional<bool>(runOptions.instrument==2);
        const auto r=analyseCab(post,rate(),current,instrument,runOptions.style);
        lastCab=r;
        if(!r.playingDetected){keep("no playing heard");return;}
        if(!r.changed()){keep(juce::String(r.bass?"bass":"guitar")+" "+styleName(runOptions.style)+" balance already near the target here");return;}
        const auto snap=[this](const juce::String& id,float value){const auto* p=processor.parameters().getParameter(id);return p->convertFrom0to1(p->convertTo0to1(value));};
        const CabPlacement applied{snap(positionID,float(r.before.position+strength()*(r.proposed.position-r.before.position))),
                                   snap(distanceID,float(r.before.distanceCm+strength()*(r.proposed.distanceCm-r.before.distanceCm)))};
        // Interference is not monotonic between two safe endpoints. Check the
        // host-quantized Strength placement before writing any of its controls.
        if(!cabPlacementSafe(current,applied)){keep("the Strength placement adds a peak or notch");return;}
        const auto put=[&](const juce::String& id,float value){if(intact())set(id,value);};
        put(positionID,float(applied.position));
        put(distanceID,float(applied.distanceCm));
        // Hold the loudness for the placement actually applied.
        const float gainBefore=raw(gainID),change=cabLevelChangeDb(r.energy,current,{raw(positionID),raw(distanceID)});
        put(gainID,std::round((gainBefore-change)*10.f)/10.f);
        // A host may edit synchronously from any change-gesture callback.
        // Undo only our partial writes, retaining the host's value/history.
        if(!intact()) {
            for(const auto& saved:previous)for(auto& entry:snapshot)if(entry.parameter==saved.parameter && owned(entry)) {
                if(setNormalised(entry,saved.lastWritten))entry=saved;
            }
            manualEditsKept=true;keep("mic controls edited while applying");return;
        }
        const auto moves="mic "+mic+" position "+juce::String(r.before.position,2)+" > "+juce::String(raw(positionID),2)
                        +", distance "+juce::String(r.before.distanceCm,1)+" > "+juce::String(raw(distanceID),1)+" cm, level "
                        +juce::String(gainBefore,1)+" > "+juce::String(raw(gainID),1)+" dB";
        summary+=" | Cab "+moves;
        addStep(moves+"; balance off by "+juce::String(r.toneErrorBeforeDb,1)+" > "+juce::String(r.toneErrorAfterDb,1)+" dB",true);
        // What the applied mic move adds to every band, for the cuts step.
        const auto nowBands=cabBandsDb(current),movedBands=cabBandsDb(withPlacement(current,{raw(positionID),raw(distanceID)}));
        for(int b=0;b<toneBandCount;++b)cabMoveDb[size_t(b)]=movedBands[size_t(b)]-nowBands[size_t(b)]+(raw(gainID)-gainBefore);
    }
    std::array<int,2> cabSources() const {return {juce::roundToInt(raw("cabtype1")),juce::roundToInt(raw("cabBtype1"))};}
    // Phase 23: the heard mic's factory IR, V30 / SM57 or Jensen / SM57.
    void cabIRStep(const std::vector<float>& post) {
        const auto keep=[this](const juce::String& why){summary+=" | Cab IR unchanged ("+why+")";steps.push_back({Phase::cabIR,"kept ("+why+")",false});};
        const auto t=cabTarget();
        if(locked(lockCabIR)){keep("locked");return;}
        if(!(t==cabAtStart) || cabSources()!=cabIRAtStart){keep("cabinet, mic or IR changed while listening");return;}
        if(t.mode!=0){keep("Dual and Matrix routing: choose those rigs' IRs in the full editor");return;}
        if(!t.on){keep("the cabinet is switched off");return;}
        if(t.mic<0){keep("two mics are blended; set the blend fully to one mic to let Auto Set choose its IR");return;}
        const juce::String mic=t.mic?"B":"A";
        if(t.key!=0){keep("mic "+mic+" is a modeled mic: the cabinet mic step places it");return;}
        const juce::String id=t.mic?"cabBtype1":"cabtype1";
        const int source=cabIRAtStart[size_t(t.mic)];
        if(source==0){keep("mic "+mic+" is filters only: choose an IR in the cabinet to let Auto Set compare them");return;}
        if(source==3){keep("mic "+mic+" plays a user IR: choose IRs in the IR loader");return;}
        const auto v30=IRLibrary::factoryIR(0),jensen=IRLibrary::factoryIR(1);
        if(!v30 || !jensen){keep("the factory IRs could not be read");return;}
        const auto r=analyseCabIR(post,rate(),source,{cabIRBandsDb(v30->samples,v30->rate),cabIRBandsDb(jensen->samples,jensen->rate)},chosenInstrument(),runOptions.style);
        lastCabIR=r;
        if(!r.playingDetected){keep("no playing heard");return;}
        if(r.bass){keep("bass: both factory IRs are guitar cabinets");return;}
        if(!r.changed()){keep("guitar "+juce::String(styleName(runOptions.style))+" balance already closest with "+cabIRName(source));return;}
        set(id,float(r.proposed));
        if(juce::roundToInt(raw(id))!=r.proposed){keep("IR source edited while applying");return;}
        for(int b=0;b<toneBandCount;++b)cabMoveDb[size_t(b)]+=r.moveDb[size_t(b)];
        const auto move="mic "+mic+" IR "+cabIRName(r.before)+" > "+cabIRName(r.proposed);
        summary+=" | Cab "+move;
        steps.push_back({Phase::cabIR,move+"; balance off by "+juce::String(r.toneErrorBeforeDb,1)+" > "+juce::String(r.toneErrorAfterDb,1)+" dB; level "
            +(r.levelChangeDb>=0?"+":"")+juce::String(r.levelChangeDb,1)+" dB (the Final EQ + output step sets the level)",true});
    }
    // Phase 15: the Classic rig's cabinet LOW and HIGH cuts.
    void cutsStep(const std::vector<float>& post) {
        const auto keep=[this](const juce::String& why){summary+=" | Cab cuts unchanged ("+why+")";steps.push_back({Phase::cuts,"kept ("+why+")",false});};
        const auto t=cabTarget();
        if(locked(lockCuts)){keep("locked");return;}
        if(!(t==cabAtStart)){keep("cabinet or mic changed while listening");return;}
        if(cutsAtStart!=std::array<float,2>{raw("cablow1"),raw("cabhigh1")}){keep("cuts edited while listening");return;}
        if(t.mode!=0){keep("Dual and Matrix routing: set those rigs' cuts in the full editor");return;}
        if(!t.on){keep("the cabinet is switched off");return;}
        const auto heard=measureSpectrum(post,rate(),chosenInstrument());
        const auto r=analyseCuts(heard,rate(),cutsAtStart[0],cutsAtStart[1],cabMoveDb,runOptions.style);
        lastCuts=r;
        if(!r.playingDetected){keep("no playing heard");return;}
        if(!r.changed()){keep(juce::String(r.bass?"bass":"guitar")+" "+styleName(runOptions.style)+" low end and top already near the target");return;}
        const auto hz=[](float v){return v>=1000.f?juce::String(v/1000.f,1)+" kHz":juce::String(juce::roundToInt(v))+" Hz";};
        juce::StringArray moves,fits;
        if(r.lowChanged()) {
            set("cablow1",r.lowBefore+strength()*(r.lowAfter-r.lowBefore));
            moves.add("LOW "+hz(r.lowBefore)+" > "+hz(raw("cablow1")));
            fits.add("low end off by "+juce::String(r.lowErrorBeforeDb,1)+" > "+juce::String(r.lowErrorAfterDb,1)+" dB");
        }
        if(r.highChanged()) {
            set("cabhigh1",r.highBefore+strength()*(r.highAfter-r.highBefore));
            moves.add("HIGH "+hz(r.highBefore)+" > "+hz(raw("cabhigh1")));
            fits.add("top off by "+juce::String(r.highErrorBeforeDb,1)+" > "+juce::String(r.highErrorAfterDb,1)+" dB");
        }
        summary+=" | Cab cuts "+moves.joinIntoString(", ");
        steps.push_back({Phase::cuts,moves.joinIntoString(", ")+"; "+fits.joinIntoString(", "),true});
    }
    int slotTarget(int slot) const {return juce::roundToInt(raw(dynamicSlotID(slot,"target")));}
    // Tone EQ bands assigned to a Dynamic EQ slot keep the user's node.
    std::vector<int> freeToneBands() const {
        std::array<bool,graphicalEQBands> dynamic{};
        for(int s=0;s<4;++s){const int target=slotTarget(s);if(target>=1 && target<=graphicalEQBands)dynamic[size_t(target-1)]=true;}
        std::vector<int> bands;for(int b=0;b<graphicalEQBands;++b)if(!dynamic[size_t(b)])bands.push_back(b);
        return bands;
    }
    void finishTone(const std::vector<float>& measured) {
        keepRigSound(measured);
        const auto r=analyseTone(measured,rate(),chosenBass(),runOptions.style);
        lastTone=r;
        bool curveWritten=false;
        if(!toneStepRuns()) {}
        else if(!r.playingDetected){summary+=" | Tone EQ unchanged (no playing)";addStep("kept (no playing heard)",false);}
        else if(r.activeMoves()==0){summary+=" | Tone EQ unchanged ("+juce::String(r.bass?"bass":"guitar")+" already near target)";addStep("kept ("+juce::String(r.bass?"bass":"guitar")+" already near the target)",false);}
        else {
            // Replace the static Tone EQ curve: measured before the Tone EQ, so
            // earlier moves are not part of the reading. UNDO AUTO restores them.
            const auto bands=freeToneBands();size_t next=0;
            for(const auto& move:r.moves) {
                if(move.gainDb==0.f || next>=bands.size())continue;
                const int b=bands[next++];
                set(eqID(0,b,"type"),float(move.type));set(eqID(0,b,"frequency"),move.frequency);set(eqID(0,b,"q"),move.q);
                set(eqID(0,b,"gain"),move.gainDb*strength());set(eqID(0,b,"enabled"),1);
            }
            for(;next<bands.size();++next)set(eqID(0,bands[next],"enabled"),0);
            set(eqID(0,-1,"bypass"),0);curveWritten=true;
            summary+=" | Tone EQ "+juce::String(r.bass?"bass":"guitar")+(runOptions.instrument==0?" (estimate)":"")+": "+juce::String(r.activeMoves())+" moves";
            juce::StringArray moves;
            for(const auto& move:r.moves)if(move.gainDb!=0.f)moves.add(frequencyText(move.frequency)+" "+(move.gainDb>0?"+":"")+juce::String(move.gainDb*strength(),1));
            addStep(juce::String(r.bass?"bass":"guitar")+(runOptions.instrument==0?" (estimate)":"")+": "+moves.joinIntoString(", "),true);
        }
        if(dynamicStepRuns())finishDynamicEQ(measured,curveWritten);
        nextPass();
    }
    static juce::String frequencyText(float frequency) {return frequency>=1000.f?juce::String(frequency/1000.f,1)+" kHz":juce::String(juce::roundToInt(frequency))+" Hz";}
    // Phase 6: Dynamic EQ nodes on free Tone EQ bands, from the same capture.
    // Nodes already assigned (the user's, or an earlier run's) are kept and
    // their frequencies avoided; at most two slots are filled per run.
    void finishDynamicEQ(const std::vector<float>& measured,bool curveWritten) {
        const auto addDynamicStep=[this](const juce::String& result,bool changed){steps.push_back({Phase::dynamicEQ,result,changed});};
        // A touched-but-returned control is still a user decision. Do not
        // construct a partial node around an edited band/slot or bypass.
        for(const auto& entry:snapshot)
            if((entry.parameter->paramID.startsWith("toneEQ_") || entry.parameter->paramID.startsWith("dynamicEQ")) && !owned(entry)) {
                addDynamicStep("kept (EQ or slots edited while listening)",false);return;
            }
        // Enabling a bypassed Tone EQ would also make the user's curve audible.
        if(!curveWritten && raw(eqID(0,-1,"bypass"))>.5f)
            for(int b=0;b<graphicalEQBands;++b)if(raw(eqID(0,b,"enabled"))>.5f){addDynamicStep("kept (the Tone EQ with your curve is bypassed)",false);return;}
        std::vector<int> slots;std::vector<float> existing;
        for(int s=0;s<4;++s){const int target=slotTarget(s);if(target==0)slots.push_back(s);else if(target<=graphicalEQBands)existing.push_back(raw(eqID(0,target-1,"frequency")));}
        std::vector<int> bands;
        for(const int b:freeToneBands())if(raw(eqID(0,b,"enabled"))<.5f)bands.push_back(b);
        const int room=int(std::min({slots.size(),bands.size(),size_t(2)}));
        if(room==0){addDynamicStep(slots.empty()?"kept (all four Dynamic EQ slots are in use)":"kept (no free Tone EQ band)",false);return;}
        const auto r=analyseDynamicEQ(measured,rate(),chosenBass(),runOptions.style,existing,room);
        lastDynamic=r;
        if(!r.playingDetected){addDynamicStep("kept (no playing heard)",false);return;}
        if(r.nodes.empty()){summary+=" | Dynamic EQ unchanged (no ringing band)";addDynamicStep("kept (no band rings louder than the rest)",false);return;}
        juce::StringArray placed;
        for(size_t i=0;i<r.nodes.size();++i) {
            const auto& node=r.nodes[i];const int b=bands[i],slot=slots[i];
            const float range=node.rangeDb*strength();
            juce::StringArray group;
            for(const auto* field:{"type","frequency","q","gain","enabled"})group.add(eqID(0,b,field));
            for(const auto* field:{"threshold","range","ratio","attack","release","target"})group.add(dynamicSlotID(slot,field));
            // Reentrant host edits must not make a partially configured node
            // audible. Restore only our writes; never roll back the host edit.
            std::vector<Snapshot> previous;
            for(const auto& entry:snapshot)if(group.contains(entry.parameter->paramID))previous.push_back(entry);
            const auto intact=[&]{for(const auto& entry:snapshot)if((group.contains(entry.parameter->paramID) || entry.parameter->paramID==eqID(0,-1,"bypass")) && !owned(entry))return false;return true;};
            const auto put=[&](const juce::String& id,float value){if(intact())set(id,value);};
            // A 0 dB bell: the node only cuts while its band is above threshold.
            put(eqID(0,b,"type"),0);put(eqID(0,b,"frequency"),node.frequency);put(eqID(0,b,"q"),node.q);put(eqID(0,b,"gain"),0);
            put(dynamicSlotID(slot,"threshold"),node.thresholdDb);put(dynamicSlotID(slot,"range"),range);put(dynamicSlotID(slot,"ratio"),node.ratio);
            put(dynamicSlotID(slot,"attack"),node.attackMs);put(dynamicSlotID(slot,"release"),node.releaseMs);
            put(dynamicSlotID(slot,"target"),float(b+1));put(eqID(0,b,"enabled"),1);
            if(!intact()) {
                for(const auto& saved:previous)for(auto& entry:snapshot)if(entry.parameter==saved.parameter && owned(entry)) {
                    if(setNormalised(entry,saved.lastWritten))entry=saved;
                }
                manualEditsKept=true;break;
            }
            dynamicNodeGroups.push_back(group);
            placed.add(frequencyText(node.frequency)+" above "+juce::String(node.thresholdDb,0)+" dB, "+juce::String(node.ratio,0)+":1 up to "+juce::String(range,0)+" dB");
        }
        if(placed.isEmpty()){addDynamicStep("kept (EQ or slots edited while applying)",false);return;}
        set(eqID(0,-1,"bypass"),0);
        summary+=" | Dynamic EQ "+placed.joinIntoString(", ");
        addDynamicStep("Tone EQ "+placed.joinIntoString("; "),true);
    }
    void finishCompressor(const std::vector<Window>& measured) {
        const auto r=analyseCompressor(measured);
        if(!r.playingDetected){fail("No playing reached the compressor. "+summary+" kept.");return;}
        const float thresholdBefore=raw("cleanCompThreshold"),ratioBefore=raw("cleanCompRatio");const bool wasOn=raw("cleanCompOn")>.5f;
        set("cleanCompThreshold",r.thresholdDb);set("cleanCompRatio",r.ratio);set("cleanCompAttack",r.attackMs);
        set("cleanCompRelease",r.releaseMs);set("cleanCompKnee",r.kneeDb);set("cleanCompMakeup",r.makeupDb);
        set("cleanCompMix",strength());set("cleanCompDetector",1);set("cleanCompOn",1);
        summary+=" | Comp "+juce::String(r.thresholdDb,0)+" dB, "+juce::String(r.ratio,0)+":1, attack "+juce::String(r.attackMs,0)
                +" ms, makeup "+juce::String(r.makeupDb,1)+" dB";
        addStep((wasOn?juce::String(thresholdBefore,0)+" dB "+juce::String(ratioBefore,1)+":1":juce::String("off"))+" > "+juce::String(r.thresholdDb,0)+" dB "+juce::String(r.ratio,0)
                +":1, attack "+juce::String(r.attackMs,0)+" ms, makeup "+juce::String(r.makeupDb,1)+" dB"+mixText(),true);
        if(pass+1<passes.size())nextPass();else complete();
    }
    void finishMultiband(const std::vector<float>& measured) {
        const auto r=analyseMultiband(measured,rate(),chosenBass(),runOptions.style);
        lastMultiband=r;
        if(!r.playingDetected){summary+=" | Multiband unchanged (no playing)";addStep("kept (no playing heard)",false);}
        else if(r.activeBands()==0){summary+=" | Multiband unchanged (bands balanced)";addStep("kept (bands already even)",false);}
        else {
            constexpr std::array<const char*,4> names{"low","low-mid","high-mid","high"};
            // The analysis designs four bands (docs/SIX_BAND_1_5.md).
            set("mbBandCount",0);
            for(int c=0;c<3;++c)set("mbCrossover"+juce::String(c+1),r.crossovers[size_t(c)]);
            juce::StringArray acting;
            for(int b=0;b<multibandBandCount;++b) {
                const auto& band=r.bands[size_t(b)];
                set(multibandID(b,"solo"),0);set(multibandID(b,"mode"),0);set(multibandID(b,"bypass"),band.active?0.f:1.f);
                if(!band.active)continue;
                set(multibandID(b,"threshold"),band.thresholdDb);set(multibandID(b,"ratio"),band.ratio);set(multibandID(b,"range"),band.rangeDb);
                set(multibandID(b,"attack"),band.attackMs);set(multibandID(b,"release"),band.releaseMs);set(multibandID(b,"knee"),band.kneeDb);
                set(multibandID(b,"makeup"),0);
                acting.add(juce::String(names[size_t(b)])+" "+juce::String(band.ratio,0)+":1");
            }
            set("mbMix",strength());set("mbDetector",1);set("mbOn",1);
            summary+=" | Multiband "+acting.joinIntoString(", ");
            addStep("compressing "+acting.joinIntoString(", ")+mixText(),true);
        }
        if(pass+1<passes.size())nextPass();else complete();
    }
    // The de-esser + limiter pass: phase 3 (de-esser and limiter), then phase 16
    // (the bus compressor) from the POST input heard at the same time.
    void finishOutput(const std::vector<float>& measuredSamples,const std::vector<Window>& measuredWindows,const std::vector<float>& postInput) {
        if(runOptions.finish)finishStep(measuredSamples,measuredWindows);
        if(runOptions.bus)busStep(postInput);
        nextPass();
    }
    void finishStep(const std::vector<float>& measuredSamples,const std::vector<Window>& measuredWindows) {
        // Preserve uncertainty: a Tone EQ heuristic is not an explicit choice.
        const std::optional<bool> instrument=runOptions.instrument==0?std::nullopt:std::optional<bool>(runOptions.instrument==2);
        const auto r=analyseFinish(measuredSamples,measuredWindows,rate(),instrument,runOptions.style);
        lastFinish=r;
        juce::String result;bool changed=false;
        if(locked(lockDeEsser)){summary+=" | De-esser locked";result="de-esser kept (locked)";}
        else if(!r.playingDetected){summary+=" | De-esser unchanged (no playing)";result="de-esser kept (no playing heard)";}
        else if(!r.deEsser){summary+=" | De-esser unchanged (no reliable excess)";result="de-esser kept (no reliable excess)";}
        else {
            const float range=r.rangeDb*strength();
            set("deEsserFrequency",r.frequency);set("deEsserThreshold",r.thresholdDb);set("deEsserRange",range);set("deEsserAttack",r.attackMs);
            set("deEsserRelease",r.releaseMs);set("deEsserRatio",r.ratio);set("deEsserKnee",r.kneeDb);set("deEsserOn",1);
            summary+=" | De-esser "+juce::String(r.frequency/1000.f,1)+" kHz, "+juce::String(r.thresholdDb,0)+" dB, up to "+juce::String(range,0)+" dB";
            result="de-esser "+juce::String(r.frequency/1000.f,1)+" kHz, "+juce::String(r.thresholdDb,0)+" dB, up to "+juce::String(range,0)+" dB";changed=true;
        }
        if(locked(lockLimiter)){summary+=" | Limiter locked";result+="; limiter kept (locked)";}
        else if(r.limiterPlaying) {
            set("liveLimiterCeiling",r.ceilingDb);set("liveLimiterRelease",r.limiterReleaseMs);set("liveLimiterOn",1);
            summary+=" | Limiter "+juce::String(r.ceilingDb,0)+" dBFS (peaks "+juce::String(r.outputPeakDb,1)+" dBFS)";
            result+="; limiter "+juce::String(r.ceilingDb,0)+" dBFS (peaks "+juce::String(r.outputPeakDb,1)+")";changed=true;
        }
        addStep(result,changed);
    }
    // The POST bus section as phase 16 sees it: a change while listening voids the step.
    struct BusTarget {bool native{};int model{};bool operator==(const BusTarget&) const=default;};
    BusTarget busTarget() const {const auto s=processor.autoSetPostState().sections[0];return {s.nativeEnabled,s.selected};}
    static juce::StringArray busWriteIDs() {
        juce::StringArray ids{postNativeBypassID(0,0)};
        for(const int c:{busThresholdControl,busMakeupControl,busAttackControl,busReleaseControl,busRatioControl,busInControl})
            ids.add(postNativeControlID(0,0,c));
        return ids;
    }
    static juce::StringArray busReadIDs() {
        juce::StringArray ids{postNativeModeID(0),postNativeModelID(0),postNativeModelExtensionID(0),
            postNativeBypassID(0,0),postNativeTrimID(0,0),postNativeLevelID(0,0)};
        for(int c=0;c<postNativeModel(0,0).controlCount;++c)ids.add(postNativeControlID(0,0,c));
        for(int slot=0;slot<postModuleCount;++slot)ids.add(orderSlotID(OrderRegion::post,slot));
        return ids;
    }
    // Phase 16: the POST Console VCA as glue. The current model only hears
    // the rack input, so require dynamics first, even for bypassed predecessors.
    void busStep(const std::vector<float>& postInput) {
        const auto keep=[this](const juce::String& why){summary+=" | Bus compressor unchanged ("+why+")";steps.push_back({Phase::bus,"kept ("+why+")",false});};
        const auto t=busTarget();
        if(locked(lockBus)){keep("locked");return;}
        if(!(t==busAtStart)){keep("POST bus section changed while listening");return;}
        PostOrder order;
        for(int slot=0;slot<postModuleCount;++slot)order.slots[size_t(slot)]=juce::roundToInt(raw(orderSlotID(OrderRegion::post,slot)));
        if(!order.valid() || order.slots[0]!=postDynamicsModule){keep("POST dynamics must be first for bus Auto Set; order and controls kept");return;}
        const auto controls=busWriteIDs();
        const auto expectedReads=size_t(busReadIDs().size());
        const auto intact=[&] {
            if(busReadSnapshot.size()!=expectedReads)return false;
            for(const auto& entry:busReadSnapshot)
                if(!entry.edits->unchanged(entry.revision) || (!controls.contains(entry.parameter->paramID) && !owned(entry)))return false;
            for(const auto& entry:snapshot)if(controls.contains(entry.parameter->paramID) && !owned(entry))return false;
            return true;
        };
        if(!intact()){manualEditsKept=true;keep("POST order or bus controls edited while listening");return;}
        if(!t.native){keep("legacy POST engine: switch the bus section to native to let Auto Set set it");return;}
        if(t.model!=0){keep("the bus section plays "+juce::String(postNativeModel(0,t.model).name)+"; Auto Set sets the Console VCA");return;}
        const auto id=[](int c){return postNativeControlID(0,0,c);};
        if(raw(id(busSidechainControl))>.5f){keep("EXTERNAL SIDE-CHAIN is on: the bus compressor hears the key, not the playing");return;}
        const bool on=raw(postNativeBypassID(0,0))<.5f && raw(id(busInControl))>.5f;
        const BusSetting current{raw(id(busThresholdControl)),raw(id(busMakeupControl)),juce::roundToInt(raw(id(busAttackControl))),juce::roundToInt(raw(id(busReleaseControl))),juce::roundToInt(raw(id(busRatioControl)))};
        std::vector<float> segment(postInput.begin(),postInput.begin()+std::ptrdiff_t(std::min(postInput.size(),size_t(rate()*busRenderSeconds))));
        const auto r=analyseBus(segment,rate(),current,on,nativeBusRenderer(processor.autoSetPostState().sections[0],segment,rate()),runOptions.style,strength());
        lastBus=r;
        if(!r.playingDetected){keep("no playing heard");return;}
        const auto glue="glue "+juce::String(r.glueBeforeDb,1)+" > "+juce::String(r.glueAfterDb,1)+" dB on loud passages (target "+juce::String(r.glueTargetDb,1)+" dB"+(r.reachable?")":", out of the threshold's reach)");
        if(!r.changed()){keep(juce::String(styleName(runOptions.style))+" glue already near the target: "+glue);return;}
        static constexpr const char* ratios[]{"2:1","4:1","10:1"};static constexpr const char* attacks[]{"0.1","0.3","1","3","10","30"};static constexpr const char* releases[]{"0.1 s","0.3 s","0.6 s","1.2 s","AUTO"};
        const auto previous=snapshot;
        const auto put=[&](const juce::String& id,float value){if(intact())set(id,value);};
        juce::String move;
        if(!on) {
            put(id(busRatioControl),float(r.proposed.ratio));put(id(busAttackControl),float(r.proposed.attack));put(id(busReleaseControl),float(r.proposed.release));
            put(id(busThresholdControl),r.proposed.threshold);put(id(busMakeupControl),r.proposed.makeup);put(id(busInControl),1);put(postNativeBypassID(0,0),0);
            move="Console VCA in: threshold "+juce::String(raw(id(busThresholdControl)),1)+" dB, "+ratios[juce::jlimit(0,2,r.proposed.ratio)]+", attack "+attacks[juce::jlimit(0,5,r.proposed.attack)]
                +" ms, release "+releases[juce::jlimit(0,4,r.proposed.release)]+", make-up "+juce::String(raw(id(busMakeupControl)),1)+" dB";
        } else {
            put(id(busThresholdControl),r.proposed.threshold);put(id(busMakeupControl),r.proposed.makeup);
            move="threshold "+juce::String(current.threshold,1)+" > "+juce::String(raw(id(busThresholdControl)),1)+" dB, make-up "+juce::String(current.makeup,1)+" > "+juce::String(raw(id(busMakeupControl)),1)+" dB";
        }
        if(!intact()) {
            // Roll back only this transaction's still-owned writes. A host
            // value/gesture callback retains ownership of everything it edits.
            for(auto& entry:snapshot)if(controls.contains(entry.parameter->paramID) && owned(entry))
                for(const auto& saved:previous)if(saved.parameter==entry.parameter) {
                    if(setNormalised(entry,saved.lastWritten))entry=saved;
                    break;
                }
            manualEditsKept=true;keep("POST order or bus controls edited while applying");return;
        }
        summary+=" | Bus "+move;
        steps.push_back({Phase::bus,move+"; "+glue,true});
    }
    // Phase 7: small Final EQ corrections and the output trim, measured at the
    // limiter input after everything else is set.
    void finishLevel(const std::vector<OutputFrame>& measured) {
        // A changing EQ/trim makes the capture unsuitable for a new correction.
        if(!std::all_of(levelReadSnapshot.begin(),levelReadSnapshot.end(),owned)) {
            manualEditsKept=true;addStep("kept (Final EQ or output edited while listening; run again)",false);nextPass();return;
        }
        auto r=analyseFinalVoicing(measureOutputSpectrum(measured,rate(),chosenBass()),rate(),runOptions.style);
        std::array<FinalMove,3> appliedMoves{};
        std::array<int,3> destinations{-1,-1,-1};
        juce::StringArray parts;bool changed=false;
        const float trimBefore=raw("output");
        bool bypassedCurve=false;
        if(raw(eqID(1,-1,"bypass"))>.5f)for(int b=0;b<graphicalEQBands;++b)bypassedCurve|=raw(eqID(1,b,"enabled"))>.5f;
        std::array<bool,graphicalEQBands> dynamic{};int lastDynamicBand=-1;
        for(int slot=0;slot<4;++slot) {
            const int target=slotTarget(slot);
            if(target>graphicalEQBands && target<=2*graphicalEQBands) {
                const int b=target-graphicalEQBands-1;dynamic[size_t(b)]=true;
                if(raw(eqID(1,b,"enabled"))>.5f)lastDynamicBand=std::max(lastDynamicBand,b);
            }
        }
        std::vector<int> bands;
        // A new static filter must follow existing time-varying filters for
        // replaying it after the measured Final EQ output to remain valid.
        for(int b=lastDynamicBand+1;b<graphicalEQBands;++b)if(!dynamic[size_t(b)] && raw(eqID(1,b,"enabled"))<.5f)bands.push_back(b);
        if(locked(lockFinalEQ))parts.add("Final EQ kept (locked)");
        else if(bypassedCurve)parts.add("Final EQ kept (your curve is bypassed)");
        else if(bands.empty())parts.add("Final EQ kept (no free band)");
        else {
            size_t next=0;
            for(size_t i=0;i<r.moves.size();++i)if(r.moves[i].active && next<bands.size()) {
                destinations[i]=bands[next++];appliedMoves[i]=r.moves[i];
                if(appliedMoves[i].type!=3)appliedMoves[i].gainDb*=strength();
                if(!detail::legalFinalMove(appliedMoves[i],[&](const char* field) {
                    return processor.parameters().getParameter(eqID(1,destinations[i],field));
                })) {
                    appliedMoves[i]={};destinations[i]=-1;
                    parts.addIfNotAlreadyThere("Final EQ correction skipped (parameter unavailable)");
                }
            }
        }
        planOutputCorrection(r,measured,rate(),appliedMoves,strength(),trimBefore,locked(lockOutput));
        if(locked(lockOutput) && r.peakUnresolved && r.eqPeakDeltaDb>0) {
            // A locked trim cannot compensate a new EQ transient/boost.
            appliedMoves={};destinations={-1,-1,-1};parts.add("Final EQ kept (output lock leaves no peak headroom)");
            planOutputCorrection(r,measured,rate(),appliedMoves,strength(),trimBefore,true);
        }
        const auto previous=snapshot;
        // Read dependencies include locked controls, which are deliberately
        // absent from the write/Undo snapshot. Own notifications do not advance
        // these revisions; any host edit (including A-B-A) invalidates the plan.
        const auto intact=[&] {
            for(const auto& entry:levelReadSnapshot)
                if(!entry.edits->unchanged(entry.revision))return false;
            for(const auto& entry:snapshot)
                if((entry.parameter->paramID.startsWith("finalEQ_") || entry.parameter->paramID.startsWith("dynamicEQ")
                    || entry.parameter->paramID=="output") && !owned(entry))return false;
            return true;
        };
        const auto put=[&](const juce::String& id,float value){if(intact())set(id,value);};
        const auto rollback=[&] {
            // Restore only this level pass's owned writes. Never rewind another
            // phase or reclaim a value the host touched in an apply/rollback callback.
            for(auto& entry:snapshot)
                if((entry.parameter->paramID.startsWith("finalEQ_") || entry.parameter->paramID=="output") && owned(entry))
                    for(const auto& saved:previous)if(saved.parameter==entry.parameter) {
                        if(setNormalised(entry,saved.lastWritten))entry=saved;
                        break;
                    }
            manualEditsKept=true;
            addStep("kept (Final EQ or output edited while applying; run again)",false);
            nextPass();
        };
        juce::StringArray moves;
        std::vector<juce::StringArray> newGroups;
        for(size_t i=0;i<appliedMoves.size();++i)if(appliedMoves[i].active) {
            const int b=destinations[i];const auto& move=appliedMoves[i];
            juce::StringArray group;
            for(const char* field:{"type","frequency","q","gain","enabled"})group.add(eqID(1,b,field));
            put(eqID(1,b,"type"),float(move.type));put(eqID(1,b,"frequency"),move.frequency);put(eqID(1,b,"q"),move.q);
            put(eqID(1,b,"gain"),move.gainDb);put(eqID(1,b,"enabled"),1);
            newGroups.push_back(group);
            moves.add(move.type==3?"high pass "+frequencyText(move.frequency):frequencyText(move.frequency)+" "+juce::String(move.gainDb,1));
        }
        if(!moves.isEmpty())put(eqID(1,-1,"bypass"),0);
        if(!intact()){rollback();return;}
        if(!moves.isEmpty()){parts.add("Final EQ "+moves.joinIntoString(", "));changed=true;}
        else if(parts.isEmpty())parts.add("Final EQ kept (already near the target)");
        if(locked(lockOutput))parts.add("output kept (locked)");
        else if(r.trimChangeDb!=0) {
            put("output",trimBefore+r.trimChangeDb);
            // Output is the final write but can still re-enter the host and
            // change an EQ, a locked dependency, or output itself. In that case
            // the rendered loudness/peak prediction must not be published.
            if(!intact()){rollback();return;}
            const float actual=raw("output")-trimBefore;
            r.predictedLufs+=actual-r.trimChangeDb;r.predictedPeakDb+=actual-r.trimChangeDb;r.trimChangeDb=actual;
            r.peakUnresolved=r.predictedPeakDb>limiterCeilingDb+.001f;
            parts.add("output "+juce::String(trimBefore,1)+" > "+juce::String(raw("output"),1)+" dB"
                      +(r.loudnessMeasured?" ("+juce::String(r.loudnessLufs,1)+" LUFS heard)":" (peak protection)"));
            changed|=actual!=0;
        } else if(!r.loudnessMeasured)parts.add("output kept (too little steady playing to measure)");
        else parts.add("output kept (within adjustment limits)");
        if(!intact()){rollback();return;}
        finalNodeGroups.insert(finalNodeGroups.end(),newGroups.begin(),newGroups.end());
        if(r.peakUnresolved)parts.add("peak ceiling not reached (output lock or trim range); reduce level and run again");
        else if(r.peakLimited)parts.add("peak protection takes priority over loudness and strength");
        lastLevel=r;
        summary+=" | "+parts.joinIntoString("; ");addStep(parts.joinIntoString("; "),changed);nextPass();
    }
    void stopListening() {processor.autoSetCapture().listen(std::nullopt);processor.setAutoSetGateBypass(false);}
    void complete() {
        stopTimer();stopListening();finishSpace();finishWidth();flushLocked(Phase::done);
        publish(Phase::done,1,(manualEditsKept?juce::String("Auto Set suggestions (newer manual/host edits kept; "):juce::String("Applied ("))
                +juce::String(styleName(runOptions.style))+(strength()<1.f?", "+juce::String(juce::roundToInt(strength()*100))+"% strength":juce::String())+"): "+summary,1);
    }
    void fail(const juce::String& text) {
        stopTimer();stopListening();
        publish(Phase::failed,0,text,0);
    }
    void addStep(const juce::String& result,bool changed){steps.push_back({status.phase,result,changed});}
    juce::String mixText() const {return strength()<1.f?", mix "+juce::String(juce::roundToInt(strength()*100))+"%":juce::String();}
    // Seconds of audio each pass needs, including the short settle after a change.
    double passSeconds(size_t index) const {return (samplePass(passes[index])?toneSeconds:windowsPerPass*.01)+(index>0?toneSettleSeconds:0.);}
    static bool samplePass(Phase phase) noexcept {return phase==Phase::tone || phase==Phase::multiband || phase==Phase::finish || phase==Phase::level || phase==Phase::amp || phase==Phase::cab || phase==Phase::boost || phase==Phase::dual || phase==Phase::matrix || phase==Phase::mics;}
    void publish(Phase phase,float value,const juce::String& text,double inPass) {
        const bool listening=busyPhase(phase);
        Status next{phase,value,text,!listening && !snapshot.empty() && changedSinceSnapshot()};
        next.passes=passes;next.pass=listening?int(pass):-1;next.steps=steps;
        for(size_t i=0;i<passes.size();++i)next.totalSeconds+=passSeconds(i);
        if(listening) {
            next.passTotalSeconds=passSeconds(pass);next.passSeconds=std::clamp(inPass,0.,1.)*next.passTotalSeconds;
            for(size_t i=0;i<pass;++i)next.seconds+=passSeconds(i);
            next.seconds+=next.passSeconds;
            describeListening(next);
        } else if(phase==Phase::done)next.seconds=next.totalSeconds;
        status=std::move(next);
        if(onStatus)onStatus(status);
    }
    // What the current pass measures, and what it has heard so far.
    void describeListening(Status& s) const {
        const auto db=[](float gain){return juce::Decibels::gainToDecibels(gain,-120.f);};
        if(status.phase==Phase::input || status.phase==Phase::compressor) {
            for(size_t i=windows.size()>30?windows.size()-30:0;i<windows.size();++i)s.levelDb=std::max(s.levelDb,windows[i].peakDb);
        } else {
            // Sample passes: peak of roughly the last 100 ms.
            const size_t span=size_t(rate()*.1);float peak=0;
            for(size_t i=samples.size()>span?samples.size()-span:0;i<samples.size();++i)peak=std::max(peak,std::abs(samples[i]));
            if(status.phase==Phase::level)for(size_t i=outputSamples.size()>span?outputSamples.size()-span:0;i<outputSamples.size();++i)
                for(const auto x:outputSamples[i])peak=std::max(peak,std::abs(x));
            s.levelDb=db(peak);
            if(status.phase==Phase::finish)for(size_t i=windows.size()>30?windows.size()-30:0;i<windows.size();++i)s.levelDb=std::max(s.levelDb,windows[i].peakDb);
        }
        const bool playing=s.levelDb>minimumPlayingDb;
        switch(status.phase) {
            case Phase::input: {
                s.listeningFor=runOptions.gates?"Playing level, the noise between notes and the amp's hiss (gates held open), for input gain, the gates and the Expander"
                                            :"Playing level and the noise between notes, for input gain and the gate";
                std::vector<float> rms;for(const auto& w:windows)rms.push_back(w.rmsDb);
                const bool heardPlaying=rms.size()>=20 && percentile(rms,.9f)>minimumPlayingDb;
                const bool heardPause=heardPlaying && percentile(rms,.9f)-percentile(rms,.1f)>=12.f;
                s.heard=juce::String("Playing: ")+(heardPlaying?"heard":"not yet")+"  /  Pause: "+(heardPause?"heard":"not yet - leave a short gap");
                if(heardPause && rigWindows.size()>=20) {
                    // The quietest rig windows are the pause: roughly the hiss the amp adds.
                    std::vector<float> rig;for(const auto& w:rigWindows)rig.push_back(w.peakDb);
                    s.heard+="  /  Amp hiss: "+juce::String(percentile(rig,.1f),0)+" dBFS";
                }
                break;
            }
            case Phase::tone:s.listeningFor=toneStepRuns()?juce::String("Tone balance from lows to highs after the amp and cab, for the Tone EQ"):juce::String("Notes that ring louder than the rest after the amp and cab");
                if(toneStepRuns() && dynamicStepRuns())s.listeningFor+=", and notes that ring louder than the rest, for the Dynamic EQ";
                else if(dynamicStepRuns())s.listeningFor+=", for the Dynamic EQ";
                break;
            case Phase::compressor:s.listeningFor="How much your level jumps between soft and hard notes, for the compressor";break;
            case Phase::multiband:s.listeningFor="Which frequency bands jump around more than the rest, for the multiband";break;
            case Phase::finish:s.listeningFor=finishStepRuns()?juce::String("Harsh 4-10 kHz peaks and the loudest output peaks, for the de-esser and limiter"):juce::String("Your level going into the POST rack");
                if(busStepRuns())s.listeningFor+=finishStepRuns()?", and your level going into the POST rack, for the bus compressor":", for the bus compressor";
                break;
            case Phase::level:s.listeningFor="The overall balance and loudness at the output, for the Final EQ and output level";break;
            case Phase::amp:s.listeningFor="Your playing going into the amp and the sound after the cab, for the amp's "+ampTargetText()+" knobs";break;
            case Phase::cab:s.listeningFor="Tone balance from lows to highs after the cab, for "+juce::String(cabStepRuns()&&cutsStepRuns()?"where the cabinet mic sits and the cab's cuts":cutsStepRuns()?"the cab's LOW and HIGH cuts":cabStepRuns()?"where the cabinet mic sits":"which factory IR suits the playing");break;
            case Phase::boost:s.listeningFor="Your playing going into the amp, for whether a boost pedal tightens the amp's low end";break;
            case Phase::dual:s.listeningFor="Each Dual rig's sound on its own, for "+juce::String(rigsStepRuns()&&dualStepRuns()?"the rigs' levels and how they combine":rigsStepRuns()?"the rigs' levels":"how the two rigs combine");break;
            case Phase::mics:s.listeningFor="Each cabinet mic on its own, for their timing, polarity and blend";break;
            case Phase::matrix:s.listeningFor="Each Matrix lane's sound on its own, for how the LOW, MID and HIGH lanes combine";break;
            default:break;
        }
        if(status.phase!=Phase::input)s.heard=playing?"Playing: heard":"Playing: not yet";
        if(status.phase==Phase::input || status.phase==Phase::tone || status.phase==Phase::multiband)
            if(const auto bass=chosenBass();bass)s.heard+=juce::String("  /  ")+(*bass?"Bass":"Guitar")+(runOptions.instrument==0?" (detected)":"");
    }
    static bool busyPhase(Phase phase) noexcept {return phase==Phase::input || phase==Phase::tone || phase==Phase::compressor || phase==Phase::multiband || phase==Phase::finish || phase==Phase::level || phase==Phase::amp || phase==Phase::cab || phase==Phase::boost || phase==Phase::dual || phase==Phase::matrix || phase==Phase::mics;}
    bool changedSinceSnapshot() const {
        const auto preserved=preservedDynamicIDs();
        for(const auto& entry:snapshot)if(entry.written && owned(entry) && !keptByCurrentLock(*entry.parameter)
                                        && !preserved.contains(entry.parameter->paramID) && entry.lastWritten!=entry.before)return true;
        return false;
    }

public:
    // Last tone analysis, for tests and diagnostics.
    const ToneResult& lastToneAnalysis() const noexcept {return lastTone;}
    const FinishResult& lastFinishAnalysis() const noexcept {return lastFinish;}
    const MultibandResult& lastMultibandAnalysis() const noexcept {return lastMultiband;}
    const GateStagesResult& lastGateAnalysis() const noexcept {return lastGates;}
    const DynamicEQResult& lastDynamicAnalysis() const noexcept {return lastDynamic;}
    const OutputResult& lastLevelAnalysis() const noexcept {return lastLevel;}
    const AmpResult& lastAmpAnalysis() const noexcept {return lastAmp;}
    const PowerResult& lastPowerAnalysis() const noexcept {return lastPower;}
    const CabResult& lastCabAnalysis() const noexcept {return lastCab;}
    const BoostResult& lastBoostAnalysis() const noexcept {return lastBoost;}
    const SpaceResult& lastSpaceAnalysis() const noexcept {return lastSpace;}
    const DualResult& lastDualAnalysis() const noexcept {return lastDual;}
    const RigLevelResult& lastRigsAnalysis() const noexcept {return lastRigs;}
    const ChannelResult& lastChannelAnalysis() const noexcept {return lastChannel;}
    const PolarityResult& lastPolarityAnalysis() const noexcept {return lastPolarity;}
    const TempoSyncResult& lastSyncAnalysis() const noexcept {return lastSync;}
    const CabIRResult& lastCabIRAnalysis() const noexcept {return lastCabIR;}
    const MatrixResult& lastMatrixAnalysis() const noexcept {return lastMatrix;}
    const MicsResult& lastMicsAnalysis() const noexcept {return lastMics;}
    const WidthResult& lastWidthAnalysis() const noexcept {return lastWidth;}
    const CutResult& lastCutsAnalysis() const noexcept {return lastCuts;}
    const BusResult& lastBusAnalysis() const noexcept {return lastBus;}
private:
    ChimeraProcessor& processor;
    std::vector<std::pair<juce::String,Lock>> touchedIDs;
    // A step reported without a pass of its own, in signal order: locked, or not applicable.
    struct Skipped {Phase phase;juce::String why{"locked"};Skipped(Phase p,juce::String reason="locked"):phase(p),why(std::move(reason)){}};
    std::vector<Skipped> lockedPhases;
    Status status;
    std::vector<Phase> passes{Phase::input,Phase::compressor};size_t pass{};
    Options options,runOptions;
    std::vector<std::unique_ptr<ParameterEdits>> editTrackers;
    std::vector<Snapshot> snapshot;
    std::vector<juce::StringArray> dynamicNodeGroups,finalNodeGroups;
    std::vector<Snapshot> levelReadSnapshot,busReadSnapshot;
    std::vector<OutputFrame> outputSamples;
    bool manualEditsKept{};
    uint32_t inputRevision{};
    std::vector<Window> windows,rigWindows;std::vector<float> samples,ampSamples,laneSamples;
    AmpTarget ampAtStart;CabTarget cabAtStart;BoostTarget boostAtStart;DualTarget dualAtStart;MatrixTarget matrixAtStart;CabTarget micsAtStart;
    int skipWindows{},skipSamples{};double lastAudioMs{-1};
    juce::String summary;std::vector<Step> steps;ToneResult lastTone;GateStagesResult lastGates;DynamicEQResult lastDynamic;OutputResult lastLevel;AmpResult lastAmp;PowerResult lastPower;CabResult lastCab;BoostResult lastBoost;SpaceResult lastSpace;DualResult lastDual;RigLevelResult lastRigs;ChannelResult lastChannel;PolarityResult lastPolarity;TempoSyncResult lastSync;CabIRResult lastCabIR;std::array<int,2> cabIRAtStart{};std::array<bool,2> polarityAtStart{};MatrixResult lastMatrix;MicsResult lastMics;WidthResult lastWidth;CutResult lastCuts;BusResult lastBus;BusTarget busAtStart;std::array<float,toneBandCount> cabMoveDb{};std::array<float,2> cutsAtStart{};std::vector<float> widthSamples;std::vector<Window> spaceWindows;bool spacePending{},widthPending{};FinishResult lastFinish;MultibandResult lastMultiband;
};

}
