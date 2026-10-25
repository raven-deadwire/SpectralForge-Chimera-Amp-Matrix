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
// the Tone EQ from the amp/cab output at the new gain; pass 3 re-measures the
// clean compressor input after both, because the amp/cab path is nonlinear and
// the Tone EQ feeds the compressor; pass 4 (optional) sets the de-esser and the
// output limiter from what now reaches them.
class Controller final : private juce::Timer {
public:
    enum class Phase {idle,input,tone,compressor,finish,done,failed};
    // instrument: 0 detect, 1 guitar, 2 bass. tone/finish include passes 2 and 4.
    struct Options {int instrument{0};bool tone{true};Style style{Style::rock};bool finish{true};};
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
        for(const auto* id:{"deEsserOn","deEsserFrequency","deEsserThreshold","deEsserRange","deEsserAttack","deEsserRelease","deEsserRatio","deEsserKnee",
                            "liveLimiterOn","liveLimiterCeiling","liveLimiterRelease"})touchedIDs.add(id);
    }
    ~Controller() override {stopTimer();processor.autoSetCapture().listen(std::nullopt);}

    // Takes effect from the next start().
    void setOptions(const Options& chosen) noexcept {options=chosen;options.instrument=std::clamp(options.instrument,0,2);}
    const Options& currentOptions() const noexcept {return options;}
    void start() {
        if(busy())return;
        snapshot.clear();summary.clear();lastTone={};lastFinish={};
        for(const auto& id:touchedIDs)if(auto* parameter=processor.parameters().getParameter(id))snapshot.emplace_back(parameter,parameter->getValue());
        passes={Phase::input};if(options.tone)passes.push_back(Phase::tone);passes.push_back(Phase::compressor);
        if(options.finish)passes.push_back(Phase::finish);
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
        if(samplePass())capture.drainSamples(samples);
        if(status.phase!=Phase::tone)capture.drain(windowTap(),windows);
        if(lastAudioMs<0 || collected()!=before)lastAudioMs=nowMs;
        else if(nowMs-lastAudioMs>noAudioTimeoutMs){fail("No audio reached the plug-in. Start playback or play your instrument, then try again.");return;}
        const double measured=std::max(0.,double(collected())-double(samplePass()?skipSamples:skipWindows)),needed=passLength();
        if(measured<needed){publish(status.phase,progress(float(measured/needed)),passText());return;}
        const std::vector<Window> measuredWindows(windows.begin()+std::min(windows.size(),size_t(skipWindows)),windows.end());
        const std::vector<float> measuredSamples(samples.begin()+std::min(samples.size(),size_t(skipSamples)),samples.end());
        if(status.phase==Phase::input)finishInput(measuredWindows);
        else if(status.phase==Phase::tone)finishTone(measuredSamples);
        else if(status.phase==Phase::compressor)finishCompressor(measuredWindows);
        else finishOutput(measuredSamples,measuredWindows);
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
    bool samplePass() const noexcept {return status.phase==Phase::tone || status.phase==Phase::finish;}
    Tap windowTap() const noexcept {return status.phase==Phase::input?Tap::input:status.phase==Phase::finish?Tap::output:Tap::compressor;}
    size_t collected() const noexcept {return samplePass()?samples.size():windows.size();}
    double rate() const noexcept {return processor.getSampleRate()>0?processor.getSampleRate():48000.;}
    double passLength() const noexcept {return samplePass()?toneSeconds*rate():double(windowsPerPass);}
    std::optional<bool> chosenBass() const noexcept {
        if(options.instrument!=0)return options.instrument==2;
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
        else if(status.phase==Phase::finish)capture.listen({Tap::deesser,Tap::output});
        else capture.listen(windowTap());
        publish(passes[pass],progress(0),passText());
    }
    void nextPass() {beginPass(pass+1,1);}
    juce::String passText() const {
        const auto step="Listening "+juce::String(int(pass)+1)+"/"+juce::String(int(passes.size()))+": ";
        if(status.phase==Phase::input)return step+"play normally, including a short pause...";
        if(status.phase==Phase::tone)return step+"keep playing across the neck for the Tone EQ...";
        if(status.phase==Phase::finish)return step+"keep playing, with some hard picking, for the de-esser and limiter...";
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
        const auto r=analyseTone(measured,rate(),chosenBass(),options.style);
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
        const auto r=analyseCompressor(measured);
        if(!r.playingDetected){fail("No playing reached the compressor. "+summary+" kept.");return;}
        set("cleanCompThreshold",r.thresholdDb);set("cleanCompRatio",r.ratio);set("cleanCompAttack",r.attackMs);
        set("cleanCompRelease",r.releaseMs);set("cleanCompKnee",r.kneeDb);set("cleanCompMakeup",r.makeupDb);
        set("cleanCompMix",1);set("cleanCompDetector",1);set("cleanCompOn",1);
        summary+=" | Comp "+juce::String(r.thresholdDb,0)+" dB, "+juce::String(r.ratio,0)+":1, attack "+juce::String(r.attackMs,0)
                +" ms, makeup "+juce::String(r.makeupDb,1)+" dB";
        if(pass+1<passes.size())nextPass();else complete();
    }
    void finishOutput(const std::vector<float>& measuredSamples,const std::vector<Window>& measuredWindows) {
        const auto r=analyseFinish(measuredSamples,measuredWindows,rate(),chosenBass(),options.style);
        lastFinish=r;
        if(!r.playingDetected)summary+=" | De-esser unchanged (no playing)";
        else if(!r.deEsser)summary+=" | De-esser off (not harsh)";
        else {
            set("deEsserFrequency",r.frequency);set("deEsserThreshold",r.thresholdDb);set("deEsserRange",r.rangeDb);set("deEsserAttack",r.attackMs);
            set("deEsserRelease",r.releaseMs);set("deEsserRatio",r.ratio);set("deEsserKnee",r.kneeDb);set("deEsserOn",1);
            summary+=" | De-esser "+juce::String(r.frequency/1000.f,1)+" kHz, "+juce::String(r.thresholdDb,0)+" dB, up to "+juce::String(r.rangeDb,0)+" dB";
        }
        if(r.limiterPlaying) {
            set("liveLimiterCeiling",r.ceilingDb);set("liveLimiterRelease",r.limiterReleaseMs);set("liveLimiterOn",1);
            summary+=" | Limiter "+juce::String(r.ceilingDb,0)+" dBFS (peaks "+juce::String(r.outputPeakDb,1)+" dBFS)";
        }
        complete();
    }
    void complete() {
        stopTimer();processor.autoSetCapture().listen(std::nullopt);
        publish(Phase::done,1,"Applied ("+juce::String(styleName(options.style))+"): "+summary);
    }
    void fail(const juce::String& text) {
        stopTimer();processor.autoSetCapture().listen(std::nullopt);
        publish(Phase::failed,0,text);
    }
    void publish(Phase phase,float value,const juce::String& text) {
        status={phase,value,text,!busyPhase(phase) && !snapshot.empty() && changedSinceSnapshot()};
        if(onStatus)onStatus(status);
    }
    static bool busyPhase(Phase phase) noexcept {return phase==Phase::input || phase==Phase::tone || phase==Phase::compressor || phase==Phase::finish;}
    bool changedSinceSnapshot() const {
        for(const auto& [parameter,value]:snapshot)if(parameter->getValue()!=value)return true;
        return false;
    }

public:
    // Last tone analysis, for tests and diagnostics.
    const ToneResult& lastToneAnalysis() const noexcept {return lastTone;}
    const FinishResult& lastFinishAnalysis() const noexcept {return lastFinish;}
private:
    ChimeraProcessor& processor;
    juce::StringArray touchedIDs;
    Status status;
    std::vector<Phase> passes{Phase::input,Phase::compressor};size_t pass{};
    Options options;
    std::vector<std::pair<juce::RangedAudioParameter*,float>> snapshot;
    std::vector<Window> windows;std::vector<float> samples;
    int skipWindows{},skipSamples{};double lastAudioMs{-1};
    juce::String summary;ToneResult lastTone;FinishResult lastFinish;
};

}
