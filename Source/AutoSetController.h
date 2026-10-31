#pragma once
#include "AutoSetStrength.h"
#include "PluginProcessor.h"
#include "AutoSet.h"
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
// Dynamic EQ (phase 6) are result steps of the input and Tone EQ passes.
class Controller final : private juce::Timer {
public:
    // gates is a result step of the input pass, not a listening pass of its own.
    // dynamicEQ is a result step of the Tone EQ pass.
    enum class Phase {idle,input,tone,compressor,multiband,finish,done,failed,gates,dynamicEQ,level};
    // Settings Auto Set must keep as they are: their parameters are neither
    // written nor restored by UNDO AUTO, and a pass whose outputs are all locked
    // is skipped. The gate lock also covers Gate 2 and the Expander; the Tone EQ
    // lock also keeps the Dynamic EQ step, whose nodes are Tone EQ bands.
    enum Lock : unsigned {lockInput=1u<<0,lockGate=1u<<1,lockTone=1u<<2,lockCompressor=1u<<3,lockMultiband=1u<<4,lockDeEsser=1u<<5,lockLimiter=1u<<6,lockDynamicEQ=1u<<7,lockFinalEQ=1u<<8,lockOutput=1u<<9};
    static constexpr std::array<Lock,10> lockOrder{lockInput,lockGate,lockTone,lockCompressor,lockMultiband,lockDeEsser,lockLimiter,lockDynamicEQ,lockFinalEQ,lockOutput};
    // strength (minimumStrength..1) scales how far each result moves the sound:
    // input gain change and Tone EQ gains scale, the compressor and multiband
    // mix in parallel, the Dynamic EQ and de-esser ranges scale. The gates are placed for the
    // applied input gain; the limiter protects the output and stays at full.
    static constexpr float minimumStrength=.1f;
    // instrument: 0 detect, 1 guitar, 2 bass. tone/multiband/finish include the optional passes;
    // gates sets Gate 2 and the Expander from the input pass; dynamicEQ places
    // Dynamic EQ nodes on the Tone EQ from the Tone EQ pass (which then runs
    // even with the Tone EQ step off); level adds the last pass, which sets the
    // Final EQ and the output trim at the limiter input.
    struct Options {int instrument{0};bool tone{true};Style style{Style::rock};bool finish{true};bool multiband{true};bool gates{true};float strength{1.f};unsigned locks{};bool dynamicEQ{true};bool level{true};};
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
            case Phase::multiband:return "Multiband";case Phase::finish:return "De-esser + limiter";case Phase::gates:return "Gate 2 + Expander";case Phase::dynamicEQ:return "Dynamic EQ";case Phase::level:return "Final EQ + output";default:return "";}
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
        snapshot.clear();dynamicNodeGroups.clear();finalNodeGroups.clear();summary.clear();steps.clear();lastGates={};lastTone={};lastDynamic={};lastLevel={};lastFinish={};lastMultiband={};manualEditsKept=false;lockedPhases.clear();
        // Locked settings stay out of the snapshot, so set() never writes them.
        for(const auto& [id,lock]:touchedIDs)if(!locked(lock))if(auto* parameter=processor.parameters().getParameter(id))
            snapshot.push_back(takeSnapshot(*parameter));
        passes.clear();
        const auto plan=[&](Phase phase,bool chosen,unsigned outputs){if(!chosen)return;if((runOptions.locks&outputs)==outputs)lockedPhases.push_back(phase);else passes.push_back(phase);};
        plan(Phase::input,true,lockInput|lockGate);
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
        if(status.phase==Phase::level)capture.drainOutput(outputSamples);
        else if(samplePass())capture.drainSamples(samples);
        if(windowPass())capture.drain(windowTap(),windows);
        if(status.phase==Phase::input)capture.drain(Tap::rig,rigWindows);
        if(lastAudioMs<0 || collected()!=before)lastAudioMs=nowMs;
        else if(nowMs-lastAudioMs>noAudioTimeoutMs){fail("No audio reached the plug-in. Start playback or play your instrument, then try again.");return;}
        const double measured=std::max(0.,double(collected())-double(samplePass()?skipSamples:skipWindows)),needed=passLength();
        if(measured<needed){publish(status.phase,progress(float(measured/needed)),passText(),measured/needed);return;}
        const std::vector<Window> measuredWindows(windows.begin()+std::min(windows.size(),size_t(skipWindows)),windows.end());
        const std::vector<float> measuredSamples(samples.begin()+std::min(samples.size(),size_t(skipSamples)),samples.end());
        if(status.phase==Phase::input)finishInput(measuredWindows);
        else if(status.phase==Phase::tone)finishTone(measuredSamples);
        else if(status.phase==Phase::compressor)finishCompressor(measuredWindows);
        else if(status.phase==Phase::multiband)finishMultiband(measuredSamples);
        else if(status.phase==Phase::finish)finishOutput(measuredSamples,measuredWindows);
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
        std::vector<float> stale;capture.drainSamples(stale);
        flushLocked(passes[index]);
        pass=index;windows.clear();rigWindows.clear();samples.clear();outputSamples.clear();levelReadSnapshot.clear();lastAudioMs=-1; // timed from the first tick of the pass
        skipWindows=settle?settleWindows:0;skipSamples=settle?int(toneSettleSeconds*rate()):0;
        status.phase=passes[pass];
        if(status.phase==Phase::tone)capture.listen(Tap::tone);
        else if(status.phase==Phase::multiband)capture.listen(Tap::multiband);
        else if(status.phase==Phase::finish)capture.listen({Tap::deesser,Tap::output});
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
    void nextPass() {if(pass+1<passes.size())beginPass(pass+1,1);else complete();}
    bool locked(unsigned lock) const noexcept {return (runOptions.locks&lock)!=0;}
    bool toneStepRuns() const noexcept {return runOptions.tone && !locked(lockTone);}
    bool dynamicStepRuns() const noexcept {return runOptions.dynamicEQ && !locked(lockDynamicEQ) && !locked(lockTone);}
    // Signal order of passes and result steps, for reporting skipped ones.
    static int order(Phase phase) noexcept {
        switch(phase){case Phase::input:return 0;case Phase::gates:return 1;case Phase::tone:return 2;case Phase::dynamicEQ:return 3;case Phase::compressor:return 4;
            case Phase::multiband:return 5;case Phase::finish:return 6;case Phase::level:return 7;default:return 8;}
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
        juce::StringArray moves;
        std::vector<juce::StringArray> newGroups;
        for(size_t i=0;i<appliedMoves.size();++i)if(appliedMoves[i].active) {
            const int b=destinations[i];const auto& move=appliedMoves[i];
            juce::StringArray group;
            for(const char* field:{"type","frequency","q","gain","enabled"})group.add(eqID(1,b,field));
            set(eqID(1,b,"type"),float(move.type));set(eqID(1,b,"frequency"),move.frequency);set(eqID(1,b,"q"),move.q);
            set(eqID(1,b,"gain"),move.gainDb);set(eqID(1,b,"enabled"),1);
            newGroups.push_back(group);
            moves.add(move.type==3?"high pass "+frequencyText(move.frequency):frequencyText(move.frequency)+" "+juce::String(move.gainDb,1));
        }
        if(!moves.isEmpty())set(eqID(1,-1,"bypass"),0);
        // Re-entrant host edits must not leave a partially configured new band.
        bool intact=true;
        for(const auto& entry:snapshot)if(entry.parameter->paramID.startsWith("finalEQ_") || entry.parameter->paramID.startsWith("dynamicEQ") || entry.parameter->paramID=="output")intact&=owned(entry);
        if(!intact) {
            for(auto& entry:snapshot)if(entry.parameter->paramID.startsWith("finalEQ_") && owned(entry))
                for(const auto& saved:previous)if(saved.parameter==entry.parameter && setNormalised(entry,saved.lastWritten)){entry=saved;break;}
            manualEditsKept=true;addStep("kept (Final EQ or output edited while applying; run again)",false);nextPass();return;
        }
        finalNodeGroups.insert(finalNodeGroups.end(),newGroups.begin(),newGroups.end());
        if(!moves.isEmpty()){parts.add("Final EQ "+moves.joinIntoString(", "));changed=true;}
        else if(parts.isEmpty())parts.add("Final EQ kept (already near the target)");
        if(locked(lockOutput))parts.add("output kept (locked)");
        else if(r.trimChangeDb!=0) {
            set("output",trimBefore+r.trimChangeDb);
            const float actual=raw("output")-trimBefore;
            r.predictedLufs+=actual-r.trimChangeDb;r.predictedPeakDb+=actual-r.trimChangeDb;r.trimChangeDb=actual;
            r.peakUnresolved=r.predictedPeakDb>limiterCeilingDb+.001f;
            parts.add("output "+juce::String(trimBefore,1)+" > "+juce::String(raw("output"),1)+" dB"
                      +(r.loudnessMeasured?" ("+juce::String(r.loudnessLufs,1)+" LUFS heard)":" (peak protection)"));
            changed|=actual!=0;
        } else if(!r.loudnessMeasured)parts.add("output kept (too little steady playing to measure)");
        else parts.add("output kept (within adjustment limits)");
        if(r.peakUnresolved)parts.add("peak ceiling not reached (output lock or trim range); reduce level and run again");
        else if(r.peakLimited)parts.add("peak protection takes priority over loudness and strength");
        lastLevel=r;
        summary+=" | "+parts.joinIntoString("; ");addStep(parts.joinIntoString("; "),changed);nextPass();
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
    static bool samplePass(Phase phase) noexcept {return phase==Phase::tone || phase==Phase::multiband || phase==Phase::finish || phase==Phase::level;}
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
            case Phase::finish:s.listeningFor="Harsh 4-10 kHz peaks and the loudest output peaks, for the de-esser and limiter";break;
            case Phase::level:s.listeningFor="The overall balance and loudness at the output, for the Final EQ and output level";break;
            default:break;
        }
        if(status.phase!=Phase::input)s.heard=playing?"Playing: heard":"Playing: not yet";
        if(status.phase==Phase::input || status.phase==Phase::tone || status.phase==Phase::multiband)
            if(const auto bass=chosenBass();bass)s.heard+=juce::String("  /  ")+(*bass?"Bass":"Guitar")+(runOptions.instrument==0?" (detected)":"");
    }
    static bool busyPhase(Phase phase) noexcept {return phase==Phase::input || phase==Phase::tone || phase==Phase::compressor || phase==Phase::multiband || phase==Phase::finish || phase==Phase::level;}
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
private:
    ChimeraProcessor& processor;
    std::vector<std::pair<juce::String,Lock>> touchedIDs;std::vector<Phase> lockedPhases;
    Status status;
    std::vector<Phase> passes{Phase::input,Phase::compressor};size_t pass{};
    Options options,runOptions;
    std::vector<std::unique_ptr<ParameterEdits>> editTrackers;
    std::vector<Snapshot> snapshot;
    std::vector<juce::StringArray> dynamicNodeGroups,finalNodeGroups;
    std::vector<Snapshot> levelReadSnapshot;
    std::vector<OutputFrame> outputSamples;
    bool manualEditsKept{};
    uint32_t inputRevision{};
    std::vector<Window> windows,rigWindows;std::vector<float> samples;
    int skipWindows{},skipSamples{};double lastAudioMs{-1};
    juce::String summary;std::vector<Step> steps;ToneResult lastTone;GateStagesResult lastGates;DynamicEQResult lastDynamic;OutputResult lastLevel;FinishResult lastFinish;MultibandResult lastMultiband;
};

}
