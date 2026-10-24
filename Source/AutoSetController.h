#pragma once
#include "PluginProcessor.h"
#include "AutoSet.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <utility>
#include <vector>

namespace spectralforge::autoset {

// Two listening passes on the message thread. Pass 1 sets input gain and the
// gate from the signal after input gain; pass 2 re-measures the clean
// compressor input at the new gain, because the amp/cab path is nonlinear.
class Controller final : private juce::Timer {
public:
    enum class Phase {idle,input,compressor,done,failed};
    struct Status {Phase phase{Phase::idle};float progress{};juce::String text;bool canUndo{};};
    static constexpr int windowsPerPass=600,settleWindows=30; // 6 s of audio, 300 ms settle
    static constexpr double noAudioTimeoutMs=3000;

    explicit Controller(ChimeraProcessor& p):processor(p) {}
    ~Controller() override {stopTimer();processor.autoSetCapture().listen(std::nullopt);}

    void start() {
        if(busy())return;
        snapshot.clear();
        for(const auto* id:touchedIDs)if(auto* parameter=processor.parameters().getParameter(id))snapshot.emplace_back(parameter,parameter->getValue());
        beginPass(Phase::input,Tap::input,0);
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
    bool busy() const noexcept {return status.phase==Phase::input || status.phase==Phase::compressor;}
    const Status& current() const noexcept {return status;}
    std::function<void(const Status&)> onStatus;
    // Exposed so non-realtime tests can drive the controller deterministically.
    void tick(double nowMs=juce::Time::getMillisecondCounterHiRes()) {
        if(!busy())return;
        const auto before=windows.size();
        processor.autoSetCapture().drain(tap,windows);
        if(lastAudioMs<0 || windows.size()!=before)lastAudioMs=nowMs;
        else if(nowMs-lastAudioMs>noAudioTimeoutMs){fail("No audio reached the plug-in. Start playback or play your instrument, then try again.");return;}
        const int measured=std::max(0,int(windows.size())-skip);
        if(measured<windowsPerPass){publish(status.phase,progress(measured),passText());return;}
        std::vector<Window> pass(windows.begin()+skip,windows.end());
        if(status.phase==Phase::input)finishInput(pass);else finishCompressor(pass);
    }
private:
    inline static constexpr std::array<const char*,13> touchedIDs{
        "input","gateon","gatethreshold","gateRangeDb",
        "cleanCompOn","cleanCompThreshold","cleanCompRatio","cleanCompAttack","cleanCompRelease",
        "cleanCompKnee","cleanCompMakeup","cleanCompMix","cleanCompDetector"};

    void timerCallback() override {tick();}
    float raw(const char* id) const {return processor.parameters().getRawParameterValue(id)->load();}
    void set(const char* id,float value) {
        if(auto* parameter=processor.parameters().getParameter(id))setNormalised(*parameter,parameter->convertTo0to1(value));
    }
    static void setNormalised(juce::RangedAudioParameter& parameter,float normalised) {
        if(parameter.getValue()==normalised)return;
        parameter.beginChangeGesture();parameter.setValueNotifyingHost(normalised);parameter.endChangeGesture();
    }
    void beginPass(Phase phase,Tap next,int settle) {
        auto& capture=processor.autoSetCapture();
        capture.listen(std::nullopt);
        std::vector<Window> discard;capture.drain(Tap::input,discard);capture.drain(Tap::compressor,discard);
        windows.clear();tap=next;skip=settle;lastAudioMs=-1; // timed from the first tick of the pass
        capture.listen(next);
        publish(phase,0,passText(phase));
    }
    juce::String passText(std::optional<Phase> phase={}) const {
        return (phase.value_or(status.phase)==Phase::input)
            ? "Listening 1/2: play normally, including a short pause..."
            : "Listening 2/2: keep playing for the compressor...";
    }
    float progress(int measured) const {
        const float pass=float(measured)/float(windowsPerPass);
        return status.phase==Phase::input?.5f*pass:.5f+.5f*pass;
    }
    void finishInput(const std::vector<Window>& pass) {
        const auto r=analyseInputGate(pass,raw("input"));
        if(!r.playingDetected){fail("No playing detected. Check the input level and try again.");return;}
        set("input",r.inputGainDb);
        summary="Input "+juce::String(r.inputGainDb,1)+" dB";
        if(r.pausesDetected) {
            set("gatethreshold",r.gateThresholdDb);set("gateRangeDb",r.gateRangeDb);set("gateon",1);
            summary+=" | Gate "+juce::String(r.gateThresholdDb,0)+" dB, range "+(r.gateRangeDb>=96.f?juce::String("Full"):juce::String(r.gateRangeDb,0)+" dB");
        } else summary+=" | Gate unchanged (no pause heard)";
        beginPass(Phase::compressor,Tap::compressor,settleWindows);
    }
    void finishCompressor(const std::vector<Window>& pass) {
        stopTimer();processor.autoSetCapture().listen(std::nullopt);
        const auto r=analyseCompressor(pass);
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
    static bool busyPhase(Phase phase) noexcept {return phase==Phase::input || phase==Phase::compressor;}
    bool changedSinceSnapshot() const {
        for(const auto& [parameter,value]:snapshot)if(parameter->getValue()!=value)return true;
        return false;
    }

    ChimeraProcessor& processor;
    Status status;
    std::vector<std::pair<juce::RangedAudioParameter*,float>> snapshot;
    std::vector<Window> windows;
    Tap tap{Tap::input};int skip{};double lastAudioMs{-1};
    juce::String summary;
};

}
