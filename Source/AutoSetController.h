#pragma once
#include "PluginProcessor.h"
#include "AutoSet.h"
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
// output limiter from what now reaches them.
class Controller final : private juce::Timer {
public:
    enum class Phase {idle,input,tone,compressor,multiband,finish,done,failed};
    // instrument: 0 detect, 1 guitar, 2 bass. tone/multiband/finish include the optional passes.
    struct Options {int instrument{0};bool tone{true};Style style{Style::rock};bool finish{true};bool multiband{true};};
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
            case Phase::multiband:return "Multiband";case Phase::finish:return "De-esser + limiter";default:return "";}
    }
    static constexpr int windowsPerPass=600,settleWindows=30; // 6 s of audio, 300 ms settle
    static constexpr double toneSeconds=6,toneSettleSeconds=.3;
    static constexpr double noAudioTimeoutMs=3000;

    explicit Controller(ChimeraProcessor& p):processor(p) {
        for(const auto* id:{"input","gateon","gatethreshold","gateRangeDb",
                            "cleanCompOn","cleanCompThreshold","cleanCompRatio","cleanCompAttack","cleanCompRelease",
                            "cleanCompKnee","cleanCompMakeup","cleanCompMix","cleanCompDetector"})touchedIDs.add(id);
        touchedIDs.add(eqID(0,-1,"bypass"));
        for(int b=0;b<graphicalEQBands;++b)for(const auto* field:{"enabled","frequency","gain","q","type"})touchedIDs.add(eqID(0,b,field));
        for(const auto* id:{"deEsserOn","deEsserFrequency","deEsserThreshold","deEsserRange","deEsserAttack","deEsserRelease","deEsserRatio","deEsserKnee",
                            "liveLimiterOn","liveLimiterCeiling","liveLimiterRelease",
                            "mbOn","mbMix","mbDetector","mbCrossover1","mbCrossover2","mbCrossover3"})touchedIDs.add(id);
        for(int b=0;b<multibandBandCount;++b)
            for(const auto* field:{"bypass","mode","threshold","range","ratio","attack","release","knee","makeup","solo"})touchedIDs.add(multibandID(b,field));
        for(const auto& id:touchedIDs)if(auto* parameter=processor.parameters().getParameter(id))
            editTrackers.push_back(std::make_unique<ParameterEdits>(*parameter));
    }
    ~Controller() override {stopTimer();processor.autoSetCapture().listen(std::nullopt);}

    // Takes effect from the next start().
    void setOptions(const Options& chosen) noexcept {options=chosen;options.instrument=std::clamp(options.instrument,0,2);}
    const Options& currentOptions() const noexcept {return options;}
    void start() {
        if(busy())return;
        runOptions=options;
        inputRevision=editsFor(*processor.parameters().getParameter("input")).revision();
        snapshot.clear();summary.clear();steps.clear();lastTone={};lastFinish={};lastMultiband={};manualEditsKept=false;
        for(const auto& id:touchedIDs)if(auto* parameter=processor.parameters().getParameter(id))snapshot.push_back(takeSnapshot(*parameter));
        passes={Phase::input};if(runOptions.tone)passes.push_back(Phase::tone);passes.push_back(Phase::compressor);
        if(runOptions.multiband)passes.push_back(Phase::multiband);
        if(runOptions.finish)passes.push_back(Phase::finish);
        beginPass(0,0);
        startTimerHz(20);
    }
    void cancel() {
        if(!busy())return;
        stopTimer();processor.autoSetCapture().listen(std::nullopt);
        // Cancel only stops listening. Passes that already finished keep
        // their settings until UNDO restores the pre-run snapshot.
        publish(Phase::idle,0,changedSinceSnapshot() ? "Auto Set cancelled. Finished steps are kept; UNDO restores your previous settings."
                                                     : "Auto Set cancelled before changing anything.",0);
    }
    void undo() {
        if(busy() || snapshot.empty())return;
        // Only restore writes owned by this run. A later manual/host edit,
        // including a change to an untouched optional stage, belongs to the user.
        for(auto& entry:snapshot)if(entry.written && owned(entry))
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
        if(windowPass())capture.drain(windowTap(),windows);
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
        else finishOutput(measuredSamples,measuredWindows);
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
    bool samplePass() const noexcept {return status.phase==Phase::tone || status.phase==Phase::multiband || status.phase==Phase::finish;}
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
        std::vector<Window> discard;for(const auto t:{Tap::input,Tap::compressor,Tap::output})capture.drain(t,discard);
        std::vector<float> stale;capture.drainSamples(stale);
        pass=index;windows.clear();samples.clear();lastAudioMs=-1; // timed from the first tick of the pass
        skipWindows=settle?settleWindows:0;skipSamples=settle?int(toneSettleSeconds*rate()):0;
        status.phase=passes[pass];
        if(status.phase==Phase::tone)capture.listen(Tap::tone);
        else if(status.phase==Phase::multiband)capture.listen(Tap::multiband);
        else if(status.phase==Phase::finish)capture.listen({Tap::deesser,Tap::output});
        else capture.listen(windowTap());
        publish(passes[pass],progress(0),passText(),0);
    }
    void nextPass() {beginPass(pass+1,1);}
    juce::String passText() const {
        const auto step="Listening "+juce::String(int(pass)+1)+"/"+juce::String(int(passes.size()))+": ";
        if(status.phase==Phase::input)return step+"play normally, including a short pause...";
        if(status.phase==Phase::tone)return step+"keep playing across the neck for the Tone EQ...";
        if(status.phase==Phase::multiband)return step+"keep playing, low notes and hard picking, for the multiband...";
        if(status.phase==Phase::finish)return step+"keep playing, with some hard picking, for the de-esser and limiter...";
        return step+"keep playing for the compressor...";
    }
    float progress(float inPass) const {return (float(pass)+inPass)/float(passes.size());}
    void finishInput(const std::vector<Window>& measured) {
        const auto r=analyseInputGate(measured,raw("input"));
        if(!r.playingDetected){fail("No playing detected. Check the input level and try again.");return;}
        const float inputBefore=raw("input"),gateBefore=raw("gatethreshold");
        set("input",r.inputGainDb);
        const float applied=raw("input");
        summary="Input "+juce::String(applied,1)+" dB";
        juce::String result="input "+juce::String(inputBefore,1)+" > "+juce::String(applied,1)+" dB";
        if(!editsFor(*processor.parameters().getParameter("input")).unchanged(inputRevision)) {
            // The capture can contain multiple gain settings. A single shift
            // cannot reconstruct its noise/playing percentiles safely.
            summary+=" | Gate unchanged (input edited while listening)";
            result+="; gate kept (input edited while listening)";
        } else if(r.pausesDetected) {
            set("gatethreshold",r.gateThresholdDb);set("gateRangeDb",r.gateRangeDb);set("gateon",1);
            const auto range=r.gateRangeDb>=96.f?juce::String("Full"):juce::String(r.gateRangeDb,0)+" dB";
            summary+=" | Gate "+juce::String(r.gateThresholdDb,0)+" dB, range "+range;
            result+="; gate "+juce::String(gateBefore,0)+" > "+juce::String(r.gateThresholdDb,0)+" dB, range "+range;
        } else {summary+=" | Gate unchanged (no pause heard)";result+="; gate kept (no pause heard)";}
        addStep(result,true);
        nextPass();
    }
    // Tone EQ bands assigned to a Dynamic EQ slot keep the user's node.
    std::vector<int> freeToneBands() const {
        std::array<bool,graphicalEQBands> dynamic{};
        for(int s=0;s<4;++s){const int target=juce::roundToInt(raw(dynamicSlotID(s,"target")));if(target>=1 && target<=graphicalEQBands)dynamic[size_t(target-1)]=true;}
        std::vector<int> bands;for(int b=0;b<graphicalEQBands;++b)if(!dynamic[size_t(b)])bands.push_back(b);
        return bands;
    }
    void finishTone(const std::vector<float>& measured) {
        const auto r=analyseTone(measured,rate(),chosenBass(),runOptions.style);
        lastTone=r;
        if(!r.playingDetected){summary+=" | Tone EQ unchanged (no playing)";addStep("kept (no playing heard)",false);}
        else if(r.activeMoves()==0){summary+=" | Tone EQ unchanged ("+juce::String(r.bass?"bass":"guitar")+" already near target)";addStep("kept ("+juce::String(r.bass?"bass":"guitar")+" already near the target)",false);}
        else {
            // Replace the static Tone EQ curve: measured before the Tone EQ, so
            // earlier moves are not part of the reading. UNDO AUTO restores them.
            const auto bands=freeToneBands();size_t next=0;
            for(const auto& move:r.moves) {
                if(move.gainDb==0.f || next>=bands.size())continue;
                const int b=bands[next++];
                set(eqID(0,b,"type"),float(move.type));set(eqID(0,b,"frequency"),move.frequency);set(eqID(0,b,"q"),move.q);
                set(eqID(0,b,"gain"),move.gainDb);set(eqID(0,b,"enabled"),1);
            }
            for(;next<bands.size();++next)set(eqID(0,bands[next],"enabled"),0);
            set(eqID(0,-1,"bypass"),0);
            summary+=" | Tone EQ "+juce::String(r.bass?"bass":"guitar")+(runOptions.instrument==0?" (estimate)":"")+": "+juce::String(r.activeMoves())+" moves";
            juce::StringArray moves;
            for(const auto& move:r.moves)if(move.gainDb!=0.f)moves.add((move.frequency>=1000.f?juce::String(move.frequency/1000.f,1)+" kHz":juce::String(juce::roundToInt(move.frequency))+" Hz")+" "+(move.gainDb>0?"+":"")+juce::String(move.gainDb,1));
            addStep(juce::String(r.bass?"bass":"guitar")+(runOptions.instrument==0?" (estimate)":"")+": "+moves.joinIntoString(", "),true);
        }
        nextPass();
    }
    void finishCompressor(const std::vector<Window>& measured) {
        const auto r=analyseCompressor(measured);
        if(!r.playingDetected){fail("No playing reached the compressor. "+summary+" kept.");return;}
        const float thresholdBefore=raw("cleanCompThreshold"),ratioBefore=raw("cleanCompRatio");const bool wasOn=raw("cleanCompOn")>.5f;
        set("cleanCompThreshold",r.thresholdDb);set("cleanCompRatio",r.ratio);set("cleanCompAttack",r.attackMs);
        set("cleanCompRelease",r.releaseMs);set("cleanCompKnee",r.kneeDb);set("cleanCompMakeup",r.makeupDb);
        set("cleanCompMix",1);set("cleanCompDetector",1);set("cleanCompOn",1);
        summary+=" | Comp "+juce::String(r.thresholdDb,0)+" dB, "+juce::String(r.ratio,0)+":1, attack "+juce::String(r.attackMs,0)
                +" ms, makeup "+juce::String(r.makeupDb,1)+" dB";
        addStep((wasOn?juce::String(thresholdBefore,0)+" dB "+juce::String(ratioBefore,1)+":1":juce::String("off"))+" > "+juce::String(r.thresholdDb,0)+" dB "+juce::String(r.ratio,0)
                +":1, attack "+juce::String(r.attackMs,0)+" ms, makeup "+juce::String(r.makeupDb,1)+" dB",true);
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
            set("mbMix",1);set("mbDetector",1);set("mbOn",1);
            summary+=" | Multiband "+acting.joinIntoString(", ");
            addStep("compressing "+acting.joinIntoString(", "),true);
        }
        if(pass+1<passes.size())nextPass();else complete();
    }
    void finishOutput(const std::vector<float>& measuredSamples,const std::vector<Window>& measuredWindows) {
        // Preserve uncertainty: a Tone EQ heuristic is not an explicit choice.
        const std::optional<bool> instrument=runOptions.instrument==0?std::nullopt:std::optional<bool>(runOptions.instrument==2);
        const auto r=analyseFinish(measuredSamples,measuredWindows,rate(),instrument,runOptions.style);
        lastFinish=r;
        juce::String result;bool changed=false;
        if(!r.playingDetected){summary+=" | De-esser unchanged (no playing)";result="de-esser kept (no playing heard)";}
        else if(!r.deEsser){summary+=" | De-esser unchanged (no reliable excess)";result="de-esser kept (no reliable excess)";}
        else {
            set("deEsserFrequency",r.frequency);set("deEsserThreshold",r.thresholdDb);set("deEsserRange",r.rangeDb);set("deEsserAttack",r.attackMs);
            set("deEsserRelease",r.releaseMs);set("deEsserRatio",r.ratio);set("deEsserKnee",r.kneeDb);set("deEsserOn",1);
            summary+=" | De-esser "+juce::String(r.frequency/1000.f,1)+" kHz, "+juce::String(r.thresholdDb,0)+" dB, up to "+juce::String(r.rangeDb,0)+" dB";
            result="de-esser "+juce::String(r.frequency/1000.f,1)+" kHz, "+juce::String(r.thresholdDb,0)+" dB, up to "+juce::String(r.rangeDb,0)+" dB";changed=true;
        }
        if(r.limiterPlaying) {
            set("liveLimiterCeiling",r.ceilingDb);set("liveLimiterRelease",r.limiterReleaseMs);set("liveLimiterOn",1);
            summary+=" | Limiter "+juce::String(r.ceilingDb,0)+" dBFS (peaks "+juce::String(r.outputPeakDb,1)+" dBFS)";
            result+="; limiter "+juce::String(r.ceilingDb,0)+" dBFS (peaks "+juce::String(r.outputPeakDb,1)+")";changed=true;
        }
        addStep(result,changed);
        complete();
    }
    void complete() {
        stopTimer();processor.autoSetCapture().listen(std::nullopt);
        publish(Phase::done,1,(manualEditsKept?juce::String("Auto Set suggestions (newer manual/host edits kept; "):juce::String("Applied ("))
                +juce::String(styleName(runOptions.style))+"): "+summary,1);
    }
    void fail(const juce::String& text) {
        stopTimer();processor.autoSetCapture().listen(std::nullopt);
        publish(Phase::failed,0,text,0);
    }
    void addStep(const juce::String& result,bool changed){steps.push_back({status.phase,result,changed});}
    // Seconds of audio each pass needs, including the short settle after a change.
    double passSeconds(size_t index) const {return (samplePass(passes[index])?toneSeconds:windowsPerPass*.01)+(index>0?toneSettleSeconds:0.);}
    static bool samplePass(Phase phase) noexcept {return phase==Phase::tone || phase==Phase::multiband || phase==Phase::finish;}
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
                s.listeningFor="Playing level and the noise between notes, for input gain and the gate";
                std::vector<float> rms;for(const auto& w:windows)rms.push_back(w.rmsDb);
                const bool heardPlaying=rms.size()>=20 && percentile(rms,.9f)>minimumPlayingDb;
                const bool heardPause=heardPlaying && percentile(rms,.9f)-percentile(rms,.1f)>=12.f;
                s.heard=juce::String("Playing: ")+(heardPlaying?"heard":"not yet")+"  /  Pause: "+(heardPause?"heard":"not yet - leave a short gap");
                break;
            }
            case Phase::tone:s.listeningFor="Tone balance from lows to highs after the amp and cab, for the Tone EQ";break;
            case Phase::compressor:s.listeningFor="How much your level jumps between soft and hard notes, for the compressor";break;
            case Phase::multiband:s.listeningFor="Which frequency bands jump around more than the rest, for the multiband";break;
            case Phase::finish:s.listeningFor="Harsh 4-10 kHz peaks and the loudest output peaks, for the de-esser and limiter";break;
            default:break;
        }
        if(status.phase!=Phase::input)s.heard=playing?"Playing: heard":"Playing: not yet";
        if(status.phase==Phase::input || status.phase==Phase::tone || status.phase==Phase::multiband)
            if(const auto bass=chosenBass();bass)s.heard+=juce::String("  /  ")+(*bass?"Bass":"Guitar")+(runOptions.instrument==0?" (detected)":"");
    }
    static bool busyPhase(Phase phase) noexcept {return phase==Phase::input || phase==Phase::tone || phase==Phase::compressor || phase==Phase::multiband || phase==Phase::finish;}
    bool changedSinceSnapshot() const {
        for(const auto& entry:snapshot)if(entry.written && owned(entry) && entry.lastWritten!=entry.before)return true;
        return false;
    }

public:
    // Last tone analysis, for tests and diagnostics.
    const ToneResult& lastToneAnalysis() const noexcept {return lastTone;}
    const FinishResult& lastFinishAnalysis() const noexcept {return lastFinish;}
    const MultibandResult& lastMultibandAnalysis() const noexcept {return lastMultiband;}
private:
    ChimeraProcessor& processor;
    juce::StringArray touchedIDs;
    Status status;
    std::vector<Phase> passes{Phase::input,Phase::compressor};size_t pass{};
    Options options,runOptions;
    std::vector<std::unique_ptr<ParameterEdits>> editTrackers;
    std::vector<Snapshot> snapshot;
    bool manualEditsKept{};
    uint32_t inputRevision{};
    std::vector<Window> windows;std::vector<float> samples;
    int skipWindows{},skipSamples{};double lastAudioMs{-1};
    juce::String summary;std::vector<Step> steps;ToneResult lastTone;FinishResult lastFinish;MultibandResult lastMultiband;
};

}
