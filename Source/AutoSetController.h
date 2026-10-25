#pragma once
#include "PluginProcessor.h"
#include "AutoSet.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <utility>
#include <vector>

namespace spectralforge::autoset {

// Listening passes on the message thread, in signal order. Pass 1 sets input
// gain and the gate from the signal after input gain; pass 2 (optional) sets
// the Tone EQ from the amp/cab output at the new gain; the last pass
// re-measures the clean compressor input after both, because the amp/cab path
// is nonlinear and the Tone EQ feeds the compressor.
class Controller final : private juce::Timer {
public:
    enum class Phase {idle,input,tone,compressor,done,failed};
    enum class ToneMode {automatic,guitar,bass,off};
    struct Status {Phase phase{Phase::idle};float progress{};juce::String text;bool canUndo{};};
    static constexpr int windowsPerPass=600,settleWindows=30; // 6 s of audio, 300 ms settle
    static constexpr double toneSeconds=6,toneSettleSeconds=.3;
    static constexpr double noAudioTimeoutMs=3000;

    explicit Controller(ChimeraProcessor& p):processor(p) {
        for(const auto* id:{"input","gateon","gatethreshold","gateRangeDb",
                            "cleanCompOn","cleanCompThreshold","cleanCompRatio","cleanCompAttack","cleanCompRelease",
                            "cleanCompKnee","cleanCompMakeup","cleanCompMix","cleanCompDetector"})touchedIDs.add(id);
        touchedIDs.add(eqID(0,-1,"bypass"));
        for(int b=0;b<graphicalEQBands;++b)for(const auto* field:{"enabled","frequency","gain","q","type"})touchedIDs.add(eqID(0,b,field));
    }
    ~Controller() override {stopTimer();processor.autoSetCapture().listen(std::nullopt);}

    // Takes effect from the next start().
    void setToneMode(ToneMode mode) noexcept {toneMode=mode;}
    ToneMode currentToneMode() const noexcept {return toneMode;}
    void start() {
        if(busy())return;
        snapshot.clear();summary.clear();
        for(const auto& id:touchedIDs)if(auto* parameter=processor.parameters().getParameter(id))snapshot.emplace_back(parameter,parameter->getValue());
        passes={Phase::input};if(toneMode!=ToneMode::off)passes.push_back(Phase::tone);passes.push_back(Phase::compressor);
        beginPass(0,0);
        startTimerHz(20);
    }
    void cancel() {
        if(!busy())return;
        stopTimer();processor.autoSetCapture().listen(std::nullopt);
        publish(Phase::idle,0,"Auto Set cancelled.");
    }
    void undo() {
        if(busy() || snapshot.empty())return;
        for(auto& [parameter,value]:snapshot)setNormalised(*parameter,value);
        snapshot.clear();
        publish(Phase::idle,0,"Restored the settings from before Auto Set.");
    }
    bool busy() const noexcept {return busyPhase(status.phase);}
    const Status& current() const noexcept {return status;}
    std::function<void(const Status&)> onStatus;
    // Exposed so non-realtime tests can drive the controller deterministically.
    void tick(double nowMs=juce::Time::getMillisecondCounterHiRes()) {
        if(!busy())return;
        auto& capture=processor.autoSetCapture();
        const auto before=collected();
        if(status.phase==Phase::tone)capture.drainTone(samples);else capture.drain(tap(),windows);
        if(lastAudioMs<0 || collected()!=before)lastAudioMs=nowMs;
        else if(nowMs-lastAudioMs>noAudioTimeoutMs){fail("No audio reached the plug-in. Start playback or play your instrument, then try again.");return;}
        const double measured=std::max(0.,double(collected())-double(skip)),needed=passLength();
        if(measured<needed){publish(status.phase,progress(float(measured/needed)),passText());return;}
        if(status.phase==Phase::input)finishInput({windows.begin()+skip,windows.end()});
        else if(status.phase==Phase::tone)finishTone({samples.begin()+skip,samples.end()});
        else finishCompressor({windows.begin()+skip,windows.end()});
    }
private:
    void timerCallback() override {tick();}
    float raw(const juce::String& id) const {return processor.parameters().getRawParameterValue(id)->load();}
    void set(const juce::String& id,float value) {
        if(auto* parameter=processor.parameters().getParameter(id))setNormalised(*parameter,parameter->convertTo0to1(value));
    }
    static void setNormalised(juce::RangedAudioParameter& parameter,float normalised) {
        if(parameter.getValue()==normalised)return;
        parameter.beginChangeGesture();parameter.setValueNotifyingHost(normalised);parameter.endChangeGesture();
    }
    Tap tap() const noexcept {return status.phase==Phase::input?Tap::input:status.phase==Phase::tone?Tap::tone:Tap::compressor;}
    size_t collected() const noexcept {return status.phase==Phase::tone?samples.size():windows.size();}
    double rate() const noexcept {return processor.getSampleRate()>0?processor.getSampleRate():48000.;}
    double passLength() const noexcept {return status.phase==Phase::tone?toneSeconds*rate():double(windowsPerPass);}
    void beginPass(size_t index,int settle) {
        auto& capture=processor.autoSetCapture();
        capture.listen(std::nullopt);
        std::vector<Window> discard;capture.drain(Tap::input,discard);capture.drain(Tap::compressor,discard);
        std::vector<float> stale;capture.drainTone(stale);
        pass=index;windows.clear();samples.clear();skip=settle;lastAudioMs=-1; // timed from the first tick of the pass
        status.phase=passes[pass];
        capture.listen(tap());
        publish(passes[pass],progress(0),passText());
    }
    void nextPass() {
        if(passes[pass+1]==Phase::tone)beginPass(pass+1,int(toneSettleSeconds*rate()));
        else beginPass(pass+1,settleWindows);
    }
    juce::String passText() const {
        const auto step="Listening "+juce::String(int(pass)+1)+"/"+juce::String(int(passes.size()))+": ";
        if(status.phase==Phase::input)return step+"play normally, including a short pause...";
        if(status.phase==Phase::tone)return step+"keep playing across the neck for the Tone EQ...";
        return step+"keep playing for the compressor...";
    }
    float progress(float inPass) const {return (float(pass)+inPass)/float(passes.size());}
    void finishInput(const std::vector<Window>& measured) {
        const auto r=analyseInputGate(measured,raw("input"));
        if(!r.playingDetected){fail("No playing detected. Check the input level and try again.");return;}
        set("input",r.inputGainDb);
        summary="Input "+juce::String(r.inputGainDb,1)+" dB";
        if(r.pausesDetected) {
            set("gatethreshold",r.gateThresholdDb);set("gateRangeDb",r.gateRangeDb);set("gateon",1);
            summary+=" | Gate "+juce::String(r.gateThresholdDb,0)+" dB, range "+(r.gateRangeDb>=96.f?juce::String("Full"):juce::String(r.gateRangeDb,0)+" dB");
        } else summary+=" | Gate unchanged (no pause heard)";
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
        const auto r=analyseTone(measured,rate(),toneMode==ToneMode::automatic?std::optional<bool>{}:std::optional<bool>{toneMode==ToneMode::bass});
        lastTone=r;
        if(!r.playingDetected)summary+=" | Tone EQ unchanged (no playing)";
        else if(r.activeMoves()==0)summary+=" | Tone EQ unchanged ("+juce::String(r.bass?"bass":"guitar")+" already near target)";
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
            summary+=" | Tone EQ "+juce::String(r.bass?"bass":"guitar")+": "+juce::String(r.activeMoves())+" moves";
        }
        nextPass();
    }
    void finishCompressor(const std::vector<Window>& measured) {
        stopTimer();processor.autoSetCapture().listen(std::nullopt);
        const auto r=analyseCompressor(measured);
        if(!r.playingDetected){fail("No playing reached the compressor. "+summary+" kept.");return;}
        set("cleanCompThreshold",r.thresholdDb);set("cleanCompRatio",r.ratio);set("cleanCompAttack",r.attackMs);
        set("cleanCompRelease",r.releaseMs);set("cleanCompKnee",r.kneeDb);set("cleanCompMakeup",r.makeupDb);
        set("cleanCompMix",1);set("cleanCompDetector",1);set("cleanCompOn",1);
        summary+=" | Comp "+juce::String(r.thresholdDb,0)+" dB, "+juce::String(r.ratio,0)+":1, attack "+juce::String(r.attackMs,0)
                +" ms, makeup "+juce::String(r.makeupDb,1)+" dB";
        publish(Phase::done,1,"Applied: "+summary);
    }
    void fail(const juce::String& text) {
        stopTimer();processor.autoSetCapture().listen(std::nullopt);
        publish(Phase::failed,0,text);
    }
    void publish(Phase phase,float value,const juce::String& text) {
        status={phase,value,text,!busyPhase(phase) && !snapshot.empty() && changedSinceSnapshot()};
        if(onStatus)onStatus(status);
    }
    static bool busyPhase(Phase phase) noexcept {return phase==Phase::input || phase==Phase::tone || phase==Phase::compressor;}
    bool changedSinceSnapshot() const {
        for(const auto& [parameter,value]:snapshot)if(parameter->getValue()!=value)return true;
        return false;
    }

public:
    // Last tone analysis, for tests and diagnostics.
    const ToneResult& lastToneAnalysis() const noexcept {return lastTone;}
private:
    ChimeraProcessor& processor;
    juce::StringArray touchedIDs;
    Status status;
    std::vector<Phase> passes{Phase::input,Phase::compressor};size_t pass{};
    ToneMode toneMode{ToneMode::automatic};
    std::vector<std::pair<juce::RangedAudioParameter*,float>> snapshot;
    std::vector<Window> windows;std::vector<float> samples;
    int skip{};double lastAudioMs{-1};
    juce::String summary;ToneResult lastTone;
};

}
