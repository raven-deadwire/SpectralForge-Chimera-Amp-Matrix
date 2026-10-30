#pragma once
#include "AutoSetStrength.h"
#include "PluginProcessor.h"
#include "AutoSet.h"
#include "AutoSetAmp.h"
#include "AutoSetCab.h"
#include "AutoSetBoost.h"
#include "EasyMode.h"
#include "AutoSetParameterEdits.h"
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <utility>
#include <vector>

namespace spectralforge::autoset {

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
// in front of the amp changes what the amp's GAIN and tone should be.
class Controller final : private juce::Timer {
public:
    // gates is a result step of the input pass, not a listening pass of its own.
    // dynamicEQ is a result step of the Tone EQ pass.
    enum class Phase {idle,input,tone,compressor,multiband,finish,done,failed,gates,dynamicEQ,level,amp,cab,boost};
    // Settings Auto Set must keep as they are: their parameters are neither
    // written nor restored by UNDO AUTO, and a pass whose outputs are all locked
    // is skipped. The gate lock also covers Gate 2 and the Expander; the Tone EQ
    // lock also keeps the Dynamic EQ step, whose nodes are Tone EQ bands.
    enum Lock : unsigned {lockInput=1u<<0,lockGate=1u<<1,lockTone=1u<<2,lockCompressor=1u<<3,lockMultiband=1u<<4,lockDeEsser=1u<<5,lockLimiter=1u<<6,lockDynamicEQ=1u<<7,lockFinalEQ=1u<<8,lockOutput=1u<<9,lockAmp=1u<<10,lockCab=1u<<11,lockBoost=1u<<12};
    static constexpr std::array<Lock,13> lockOrder{lockInput,lockGate,lockTone,lockCompressor,lockMultiband,lockDeEsser,lockLimiter,lockDynamicEQ,lockFinalEQ,lockOutput,lockAmp,lockCab,lockBoost};
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
    // adds the pass before the amp pass that can put a boost pedal in front of it.
    struct Options {int instrument{0};bool tone{true};Style style{Style::rock};bool finish{true};bool multiband{true};bool gates{true};float strength{1.f};unsigned locks{};bool dynamicEQ{true};bool level{true};bool amp{true};bool cab{true};bool boost{true};};
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
            case Phase::multiband:return "Multiband";case Phase::finish:return "De-esser + limiter";case Phase::gates:return "Gate 2 + Expander";case Phase::dynamicEQ:return "Dynamic EQ";case Phase::level:return "Final EQ + output";case Phase::amp:return "Amp gain + tone";case Phase::cab:return "Cabinet mic";case Phase::boost:return "Boost pedal";default:return "";}
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
        add(lockMultiband,{"mbOn","mbMix","mbDetector","mbCrossover1","mbCrossover2","mbCrossover3"});
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
        // Phase 9 places the Classic routing's (lane 1) modeled mic A or B and keeps its level.
        for(const auto* mic:{"A","B"}) {
            for(const auto* field:{"position","distance"})touchedIDs.push_back({originalCabID(0,(juce::String(mic)+field).toRawUTF8()),lockCab});
            touchedIDs.push_back({"cab"+juce::String(mic)+"gain1",lockCab});
        }
        // Phase 10 places the Green Drive on a free pedal board slot.
        for(int owner=0;owner<pedalBoardCapacity;++owner) {
            touchedIDs.push_back({pedalModelID(owner),lockBoost});touchedIDs.push_back({pedalBypassID(owner,boostPedalModel),lockBoost});
            for(int c=0;c<pedalModel(boostPedalModel).controlCount;++c)touchedIDs.push_back({pedalControlID(owner,boostPedalModel,c),lockBoost});
        }
        for(const auto& item:touchedIDs)if(auto* parameter=processor.parameters().getParameter(item.first))
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
        snapshot.clear();dynamicNodeGroups.clear();summary.clear();steps.clear();lastGates={};lastTone={};lastDynamic={};lastLevel={};lastAmp={};lastCab={};lastBoost={};lastFinish={};lastMultiband={};manualEditsKept=false;lockedPhases.clear();
        // Locked settings stay out of the snapshot, so set() never writes them.
        for(const auto& [id,lock]:touchedIDs)if(!locked(lock))if(auto* parameter=processor.parameters().getParameter(id))
            snapshot.push_back(takeSnapshot(*parameter));
        passes.clear();
        const auto plan=[&](Phase phase,bool chosen,unsigned outputs){if(!chosen)return;if((runOptions.locks&outputs)==outputs)lockedPhases.push_back(phase);else passes.push_back(phase);};
        plan(Phase::input,true,lockInput|lockGate);
        plan(Phase::boost,runOptions.boost,lockBoost);
        plan(Phase::amp,runOptions.amp,lockAmp);
        plan(Phase::cab,runOptions.cab,lockCab);
        // The Tone EQ pass serves the Tone EQ step and the Dynamic EQ step,
        // whose nodes live on Tone EQ bands (so a Tone EQ lock keeps both).
        if(runOptions.tone || runOptions.dynamicEQ) {
            if(toneStepRuns() || dynamicStepRuns())passes.push_back(Phase::tone);
            if(runOptions.tone && !toneStepRuns())lockedPhases.push_back(Phase::tone);
            if(runOptions.dynamicEQ && !dynamicStepRuns())lockedPhases.push_back(Phase::dynamicEQ);
        }
        plan(Phase::compressor,true,lockCompressor);
        plan(Phase::multiband,runOptions.multiband,lockMultiband);plan(Phase::finish,runOptions.finish,lockDeEsser|lockLimiter);
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
        if(samplePass())capture.drainSamples(samples);
        if(status.phase==Phase::amp || status.phase==Phase::boost)capture.drainAmpSamples(ampSamples);
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
        else if(status.phase==Phase::boost)finishBoost({ampSamples.begin()+std::min(ampSamples.size(),size_t(skipSamples)),ampSamples.end()});
        else if(status.phase==Phase::tone)finishTone(measuredSamples);
        else if(status.phase==Phase::compressor)finishCompressor(measuredWindows);
        else if(status.phase==Phase::multiband)finishMultiband(measuredSamples);
        else if(status.phase==Phase::finish)finishOutput(measuredSamples,measuredWindows);
        else finishLevel(measuredSamples);
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
        return result;
    }
    bool samplePass() const noexcept {return samplePass(status.phase);}
    bool windowPass() const noexcept {return status.phase==Phase::input || status.phase==Phase::compressor || status.phase==Phase::finish;}
    Tap windowTap() const noexcept {return status.phase==Phase::input?Tap::input:status.phase==Phase::finish?Tap::output:Tap::compressor;}
    size_t collected() const noexcept {return samplePass()?samples.size():windows.size();}
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
        std::vector<float> stale;capture.drainSamples(stale);capture.drainAmpSamples(stale);
        flushLocked(passes[index]);
        pass=index;windows.clear();rigWindows.clear();samples.clear();ampSamples.clear();lastAudioMs=-1; // timed from the first tick of the pass
        skipWindows=settle?settleWindows:0;skipSamples=settle?int(toneSettleSeconds*rate()):0;
        status.phase=passes[pass];
        if(status.phase==Phase::tone)capture.listen(Tap::tone);
        else if(status.phase==Phase::amp){ampAtStart=ampTarget();capture.listen({Tap::amp,Tap::tone});}
        else if(status.phase==Phase::cab){cabAtStart=cabTarget();capture.listen(Tap::tone);}
        else if(status.phase==Phase::boost){boostAtStart=boostTarget();capture.listen({Tap::amp,Tap::tone});}
        else if(status.phase==Phase::multiband)capture.listen(Tap::multiband);
        else if(status.phase==Phase::finish)capture.listen({Tap::deesser,Tap::output});
        else if(status.phase==Phase::level)capture.listen(Tap::master);
        else if(status.phase==Phase::input && runOptions.gates)capture.listen({Tap::input,Tap::rig});
        else capture.listen(windowTap());
        // Gate 2 / Expander need the rig's own noise: hold every gate open while the input pass listens.
        processor.setAutoSetGateBypass(status.phase==Phase::input && runOptions.gates);
        publish(passes[pass],progress(0),passText(),0);
    }
    void nextPass() {if(pass+1<passes.size())beginPass(pass+1,1);else complete();}
    bool locked(unsigned lock) const noexcept {return (runOptions.locks&lock)!=0;}
    bool toneStepRuns() const noexcept {return runOptions.tone && !locked(lockTone);}
    bool dynamicStepRuns() const noexcept {return runOptions.dynamicEQ && !locked(lockDynamicEQ) && !locked(lockTone);}
    // Signal order of passes and result steps, for reporting skipped ones.
    static int order(Phase phase) noexcept {
        switch(phase){case Phase::input:return 0;case Phase::gates:return 1;case Phase::boost:return 2;case Phase::amp:return 3;case Phase::cab:return 4;case Phase::tone:return 5;case Phase::dynamicEQ:return 6;
            case Phase::compressor:return 7;case Phase::multiband:return 8;case Phase::finish:return 9;case Phase::level:return 10;default:return 11;}
    }
    float strength() const noexcept {return runOptions.strength;}
    // Report skipped (locked) passes in signal order, before the next pass that runs.
    void flushLocked(Phase upTo) {
        while(!lockedPhases.empty() && order(lockedPhases.front())<order(upTo)) {
            steps.push_back({lockedPhases.front(),"kept (locked)",false});lockedPhases.erase(lockedPhases.begin());
        }
    }
    juce::String passText() const {
        const auto step="Listening "+juce::String(int(pass)+1)+"/"+juce::String(int(passes.size()))+": ";
        if(status.phase==Phase::input)return step+"play normally, including a short pause...";
        if(status.phase==Phase::amp)return step+"keep playing as you normally would, soft and hard, for the amp's gain and tone...";
        if(status.phase==Phase::cab)return step+"keep playing across the neck for the cabinet mic...";
        if(status.phase==Phase::boost)return step+"keep playing, including low notes and chords, for the boost pedal...";
        if(status.phase==Phase::tone)return step+"keep playing across the neck for the "+toneTarget()+"...";
        if(status.phase==Phase::multiband)return step+"keep playing, low notes and hard picking, for the multiband...";
        if(status.phase==Phase::finish)return step+"keep playing, with some hard picking, for the de-esser and limiter...";
        if(status.phase==Phase::level)return step+"keep playing as you normally would, for the Final EQ and output level...";
        return step+"keep playing for the compressor...";
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
    // Phase 8: the head's GAIN, BASS, MIDDLE and TREBLE, from candidates
    // rendered offline through the native amp on the captured input.
    void finishAmp(const std::vector<float>& post,const std::vector<float>& input) {
        const auto keep=[this](const juce::String& why){summary+=" | Amp unchanged ("+why+")";addStep("kept ("+why+")",false);nextPass();};
        const auto t=ampTarget();
        if(locked(lockAmp)){keep("locked");return;}
        if(!(t==ampAtStart)){keep("amp, channel or input changed while listening");return;}
        if(t.mode!=0){keep("Dual and Matrix routing: set those amps in the full editor");return;}
        if(!t.on){keep("the amp is switched off");return;}
        if(!t.native){keep("classic amp engine; switch the head panel on to let Auto Set set it");return;}
        std::array<AmpKnob,4> knobs{};std::array<int,4> controls{-1,-1,-1,-1};std::array<juce::String,4> ids;
        for(int role=0;role<easy::roleCount;++role)if(const int control=easy::ampControl(t.model,t.channel,t.route,easy::Role(role));control>=0) {
            ids[size_t(role)]=ampNativeControlID(t.context,t.model,control,t.channel);
            if(auto* parameter=processor.parameters().getParameter(ids[size_t(role)])) {
                const auto range=parameter->getNormalisableRange();
                knobs[size_t(role)]={true,raw(ids[size_t(role)]),range.start,range.end};controls[size_t(role)]=control;
            }
        }
        if(std::none_of(knobs.begin(),knobs.end(),[](const auto& k){return k.present;})){keep("this head has no GAIN or tone knobs on its current channel");return;}
        const std::optional<bool> instrument=runOptions.instrument==0?std::nullopt:std::optional<bool>(runOptions.instrument==2);
        std::vector<float> segment(input.begin(),input.begin()+std::min(input.size(),size_t(rate()*ampRenderSeconds)));
        const auto r=analyseAmp(segment,post,rate(),knobs,nativeAmpRenderer(processor.autoSetAmpState(t.context),controls,segment,rate()),instrument,runOptions.style);
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
        nextPass();
    }
    BoostTarget boostTarget() const {
        BoostTarget t;t.amp=ampTarget();t.board=raw("boardEnabled")>.5f;
        for(int position=0;position<pedalBoardCapacity;++position)t.order[size_t(position)]=juce::roundToInt(raw(pedalOrderID(position)));
        for(int owner=0;owner<pedalBoardCapacity;++owner) {
            const int model=juce::jlimit(0,pedalModelCount-1,juce::roundToInt(raw(pedalModelID(owner))));t.models[size_t(owner)]=model;
            const juce::String category=pedalModel(model).category;
            if(model>0 && raw(pedalBypassID(owner,model))<.5f && (category=="DRIVE" || category=="FUZZ" || category=="BOOST"))t.driveOn=true;
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
    CabTarget cabTarget() const {
        CabTarget t;t.mode=juce::jlimit(0,2,juce::roundToInt(raw("mode")));t.on=raw("cab1")>.5f;
        const bool modelA=raw(originalCabID(0,"Aon"))>.5f,modelB=raw(originalCabID(0,"Bon"))>.5f;
        const float blend=cabMicRouting(modelA,modelB,juce::roundToInt(raw("cabtype1")),juce::roundToInt(raw("cabBtype1")),raw("cabblend1")).blend;
        t.mic=blend<=0.f?0:blend>=1.f?1:-1;
        if(t.mic>=0)t.key=OriginalCabParameters::savedKey(0,t.mic,[this](const juce::String& id){return raw(id);});
        return t;
    }
    // Phase 9: the heard modeled mic's position and distance, from the model's
    // own response, with the mic's level holding the loudness.
    void finishCab(const std::vector<float>& post) {
        const auto keep=[this](const juce::String& why){summary+=" | Cab mic unchanged ("+why+")";addStep("kept ("+why+")",false);nextPass();};
        const auto t=cabTarget();
        if(locked(lockCab)){keep("locked");return;}
        if(!(t==cabAtStart)){keep("cabinet or mic changed while listening");return;}
        if(t.mode!=0){keep("Dual and Matrix routing: place those mics in the full editor");return;}
        if(!t.on){keep("the cabinet is switched off");return;}
        if(t.mic<0){keep("two mics are blended; set the blend fully to one mic to let Auto Set place it");return;}
        if(t.key==0){keep(juce::String("mic ")+(t.mic?"B":"A")+" is a captured IR or filters; switch on its modeled mic to let Auto Set place it");return;}
        const auto current=cabLayout::settings(t.key);
        const std::optional<bool> instrument=runOptions.instrument==0?std::nullopt:std::optional<bool>(runOptions.instrument==2);
        const auto r=analyseCab(post,rate(),current,instrument,runOptions.style);
        lastCab=r;
        if(!r.playingDetected){keep("no playing heard");return;}
        if(!r.changed()){keep(juce::String(r.bass?"bass":"guitar")+" "+styleName(runOptions.style)+" balance already near the target here");return;}
        const juce::String mic=t.mic?"B":"A";
        const auto positionID=originalCabID(0,(mic+"position").toRawUTF8()),distanceID=originalCabID(0,(mic+"distance").toRawUTF8()),gainID="cab"+mic+"gain1";
        set(positionID,float(r.before.position+strength()*(r.proposed.position-r.before.position)));
        set(distanceID,float(r.before.distanceCm+strength()*(r.proposed.distanceCm-r.before.distanceCm)));
        // Hold the loudness for the placement actually applied.
        const float gainBefore=raw(gainID),change=cabLevelChangeDb(r.heard,current,{raw(positionID),raw(distanceID)});
        set(gainID,std::round((gainBefore-change)*10.f)/10.f);
        const auto moves="mic "+mic+" position "+juce::String(r.before.position,2)+" > "+juce::String(raw(positionID),2)
                        +", distance "+juce::String(r.before.distanceCm,1)+" > "+juce::String(raw(distanceID),1)+" cm, level "
                        +juce::String(gainBefore,1)+" > "+juce::String(raw(gainID),1)+" dB";
        summary+=" | Cab "+moves;
        addStep(moves+"; balance off by "+juce::String(r.toneErrorBeforeDb,1)+" > "+juce::String(r.toneErrorAfterDb,1)+" dB",true);
        nextPass();
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
    void finishOutput(const std::vector<float>& measuredSamples,const std::vector<Window>& measuredWindows) {
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
        nextPass();
    }
    // Phase 7: small Final EQ corrections and the output trim, measured at the
    // limiter input after everything else is set.
    void finishLevel(const std::vector<float>& measured) {
        const auto r=analyseOutput(measured,rate(),chosenBass(),runOptions.style);
        lastLevel=r;
        if(!r.playingDetected){summary+=" | Final EQ / output unchanged (no playing)";addStep("kept (no playing heard)",false);nextPass();return;}
        juce::StringArray parts;bool changed=false;
        if(locked(lockFinalEQ))parts.add("Final EQ kept (locked)");
        else if(r.activeMoves()==0)parts.add("Final EQ kept (already near the target)");
        else {
            // The user's curve behind a bypassed Final EQ must stay inaudible.
            bool bypassedCurve=false;
            if(raw(eqID(1,-1,"bypass"))>.5f)for(int b=0;b<graphicalEQBands;++b)bypassedCurve|=raw(eqID(1,b,"enabled"))>.5f;
            std::array<bool,graphicalEQBands> dynamic{};
            for(int s=0;s<4;++s){const int target=slotTarget(s);if(target>graphicalEQBands && target<=2*graphicalEQBands)dynamic[size_t(target-graphicalEQBands-1)]=true;}
            std::vector<int> bands;for(int b=0;b<graphicalEQBands;++b)if(!dynamic[size_t(b)] && raw(eqID(1,b,"enabled"))<.5f)bands.push_back(b);
            if(bypassedCurve)parts.add("Final EQ kept (your curve is bypassed)");
            else if(bands.empty())parts.add("Final EQ kept (no free band)");
            else {
                juce::StringArray moves;size_t next=0;
                for(const auto& move:r.moves) {
                    if(!move.active || next>=bands.size())continue;
                    const int b=bands[next++];const float gain=move.type==3?0.f:move.gainDb*strength();
                    set(eqID(1,b,"type"),float(move.type));set(eqID(1,b,"frequency"),move.frequency);set(eqID(1,b,"q"),move.q);
                    set(eqID(1,b,"gain"),gain);set(eqID(1,b,"enabled"),1);
                    moves.add(move.type==3?"high pass "+frequencyText(move.frequency):frequencyText(move.frequency)+" "+(gain>0?"+":"")+juce::String(gain,1));
                }
                set(eqID(1,-1,"bypass"),0);
                parts.add("Final EQ "+moves.joinIntoString(", "));changed=true;
            }
        }
        const float trimBefore=raw("output");
        if(locked(lockOutput))parts.add("output kept (locked)");
        else if(!r.loudnessMeasured)parts.add("output kept (too little steady playing to measure)");
        else if(r.trimChangeDb==0.f)parts.add("output kept ("+juce::String(r.loudnessLufs,1)+" LUFS, near "+juce::String(outputTargetLufs,0)+")");
        else {
            set("output",scaledInputGainDb(trimBefore,trimBefore+r.trimChangeDb,strength()));
            const float applied=raw("output");
            parts.add("output "+juce::String(trimBefore,1)+" > "+juce::String(applied,1)+" dB ("+juce::String(r.loudnessLufs,1)+" LUFS"
                      +(r.peakLimited?", held below the limiter ceiling":", toward "+juce::String(outputTargetLufs,0))+")");
            changed|=applied!=trimBefore;
        }
        summary+=" | "+parts.joinIntoString("; ");
        addStep(parts.joinIntoString("; "),changed);
        nextPass();
    }
    void stopListening() {processor.autoSetCapture().listen(std::nullopt);processor.setAutoSetGateBypass(false);}
    void complete() {
        stopTimer();stopListening();flushLocked(Phase::done);
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
    static bool samplePass(Phase phase) noexcept {return phase==Phase::tone || phase==Phase::multiband || phase==Phase::finish || phase==Phase::level || phase==Phase::amp || phase==Phase::cab || phase==Phase::boost;}
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
            case Phase::finish:s.listeningFor="Harsh 4-10 kHz peaks and the loudest output peaks, for the de-esser and limiter";break;
            case Phase::level:s.listeningFor="The overall balance and loudness at the output, for the Final EQ and output level";break;
            case Phase::amp:s.listeningFor="Your playing going into the amp and the sound after the cab, for the amp's gain and tone knobs";break;
            case Phase::cab:s.listeningFor="Tone balance from lows to highs after the cab, for where the cabinet mic sits";break;
            case Phase::boost:s.listeningFor="Your playing going into the amp, for whether a boost pedal tightens the amp's low end";break;
            default:break;
        }
        if(status.phase!=Phase::input)s.heard=playing?"Playing: heard":"Playing: not yet";
        if(status.phase==Phase::input || status.phase==Phase::tone || status.phase==Phase::multiband)
            if(const auto bass=chosenBass();bass)s.heard+=juce::String("  /  ")+(*bass?"Bass":"Guitar")+(runOptions.instrument==0?" (detected)":"");
    }
    static bool busyPhase(Phase phase) noexcept {return phase==Phase::input || phase==Phase::tone || phase==Phase::compressor || phase==Phase::multiband || phase==Phase::finish || phase==Phase::level || phase==Phase::amp || phase==Phase::cab || phase==Phase::boost;}
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
    const CabResult& lastCabAnalysis() const noexcept {return lastCab;}
    const BoostResult& lastBoostAnalysis() const noexcept {return lastBoost;}
private:
    ChimeraProcessor& processor;
    std::vector<std::pair<juce::String,Lock>> touchedIDs;std::vector<Phase> lockedPhases;
    Status status;
    std::vector<Phase> passes{Phase::input,Phase::compressor};size_t pass{};
    Options options,runOptions;
    std::vector<std::unique_ptr<ParameterEdits>> editTrackers;
    std::vector<Snapshot> snapshot;
    std::vector<juce::StringArray> dynamicNodeGroups;
    bool manualEditsKept{};
    uint32_t inputRevision{};
    std::vector<Window> windows,rigWindows;std::vector<float> samples,ampSamples;
    AmpTarget ampAtStart;CabTarget cabAtStart;BoostTarget boostAtStart;
    int skipWindows{},skipSamples{};double lastAudioMs{-1};
    juce::String summary;std::vector<Step> steps;ToneResult lastTone;GateStagesResult lastGates;DynamicEQResult lastDynamic;OutputResult lastLevel;AmpResult lastAmp;CabResult lastCab;BoostResult lastBoost;FinishResult lastFinish;MultibandResult lastMultiband;
};

}
