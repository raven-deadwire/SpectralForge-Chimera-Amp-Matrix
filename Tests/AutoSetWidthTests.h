#pragma once
// Phase 14: the stereo doubler. Included by AutoSetTests.cpp after its helpers.

void widthRules() {
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{140};
        const auto rig=playedNotes(rate,widthRenderSeconds+widthWarmupSeconds,rng);
        const auto relaxed=windowsOf(pluckedEvery(300,6,rate),rate),fast=windowsOf(pluckedEvery(100,6,rate,40),rate);
        // The measures: the doubler widens, and a short spread combs more in mono than a long one.
        const auto dry=measureSpectrum(std::vector<float>(rig.begin()+std::ptrdiff_t(rate*widthWarmupSeconds),rig.end()),rate,false);
        const auto narrow=measureDoubler(rig,rate,1,dry),wide=measureDoubler(rig,rate,12,dry);
        check(wide.width>widthMinimum && wide.width>narrow.width,"The doubler does not widen with spread");
        for(const Style style:{Style::metal,Style::rock,Style::clean}) {
            const auto [low,high]=widthSpreadRange(style);
            // Off, relaxed playing: turned on, inside the style's range, the smoothest mono sum among the candidates.
            const auto on=analyseWidth(rig,rate,relaxed,false,6,false,style);
            check(on.playingDetected && !on.fast && on.onAfter && !on.onBefore && on.spreadAfter>=low-1e-3f && on.spreadAfter<=high+1e-3f,"The doubler was not turned on inside the style's range");
            check(!on.candidates.empty() && on.monoAfterDb<=widthMonoLimitDb && on.widthAfter>=widthMinimum,"The chosen spread combs in mono or barely widens");
            for(const auto& c:on.candidates)if(c.width>=widthMinimum)check(on.monoAfterDb<=c.monoRoughnessDb+1e-5f,"A smoother spread in mono was passed over");
            // Already on at the chosen spread: kept.
            const auto again=analyseWidth(rig,rate,relaxed,true,on.spreadAfter,false,style);
            check(!again.changed() && again.onAfter,"A doubler at the chosen spread was changed");
            // On outside the range (metal at 20 ms): brought into it, never switched off.
            if(style==Style::metal) {
                const auto tight=analyseWidth(rig,rate,relaxed,true,20,false,style);
                check(tight.onAfter && tight.changed() && tight.spreadAfter<=high+1e-3f,"A long spread was not shortened for metal");
            }
        }
        // Fast playing: an off doubler stays off; one that is on uses the shorter half of the range.
        const auto quick=analyseWidth(rig,rate,fast,false,6,false,Style::rock);
        check(quick.fast && !quick.changed(),"Fast playing turned the doubler on");
        const auto [low,high]=widthSpreadRange(Style::rock);
        const auto quickOn=analyseWidth(rig,rate,fast,true,14,false,Style::rock);
        check(quickOn.onAfter && quickOn.spreadAfter<=low+.5f*(high-low)+1e-3f,"Fast playing did not shorten the spread");
        // Bass, and silence.
        check(!analyseWidth(rig,rate,relaxed,false,6,true,Style::rock).changed(),"Bass turned the doubler on");
        check(!analyseWidth(std::vector<float>(rig.size()),rate,relaxed,false,6,false).playingDetected,"Silence reported as playing");
        if(rate==48000.)std::cout<<"MEASURE width rules 48 kHz: width at 1 ms "<<narrow.width<<", 12 ms "<<wide.width<<" (mono "<<narrow.monoRoughnessDb<<" / "<<wide.monoRoughnessDb
                                <<" dB); rock on > "<<analyseWidth(rig,rate,relaxed,false,6,false).spreadAfter<<" ms; fast, on at 14 ms > "<<quickOn.spreadAfter<<" ms\n";
    }
    std::cout<<"PASS auto set width rules at 44.1/48/96 kHz: widens, on inside the style range at the smoothest mono sum, kept, metal shortened, fast playing, bass, silence\n";
}
struct WidthRun {std::vector<float> before,values,undone;Controller::Status status;WidthResult analysis;};
// The input pass hears the plucks (window taps); the Tone EQ pass hears the rig (Tap::tone).
WidthRun runWidth(const Controller::Options& options,const std::vector<float>& plucks,const std::vector<float>& rig,const std::function<void(ChimeraProcessor&)>& prepare={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    WidthRun result;result.before=values(*p);
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=plucks.size();offset+=256) {
        const float* a[]{plucks.data()+offset};const float* b[]{rig.data()+offset%(rig.size()-256)};
        for(auto tap:{Tap::input,Tap::rig,Tap::compressor})p->autoSetCapture().push(tap,a,1,256);
        p->autoSetCapture().push(Tap::tone,b,1,256);
        ms+=1000.*256/48000;c.tick(ms);
    }
    check(!c.busy(),"Width run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastWidthAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerWidth() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    std::mt19937 rng{141};
    const auto rig=playedNotes(48000,8,rng);
    // Input pass: relaxed plucks; the Tone EQ pass hears the rig.
    const auto plucks=pluckedEvery(300,30);
    const Controller::Options widthOnly{1,true,Style::rock,false,false,false,1.f,Controller::lockCompressor,false,false,false,false,false,false,false,false,true,false,false,false,false,false,false,false,false,false};
    const auto full=runWidth(widthOnly,plucks,rig);
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::width);
    check(full.status.phase==P::done && step && step->changed && step->result.contains("doubler on"),"Doubler step did not run or report");
    check(a.onAfter && value(full.values,"doubleron")>.5f && near(value(full.values,"doublertime"),a.spreadAfter,.01f),"Applied doubler does not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the doubler");
    // Lock: kept, reported in order after the delay and reverb row.
    auto lockedOptions=widthOnly;lockedOptions.locks|=Controller::lockWidth;
    const auto locked=runWidth(lockedOptions,plucks,rig);
    check(stepOf(locked.status,P::width) && stepOf(locked.status,P::width)->result=="kept (locked)" && value(locked.values,"doubleron")==value(locked.before,"doubleron"),"Doubler lock did not keep it");
    // On already, at a spread past the range: strength moves part of the way.
    auto halfOptions=widthOnly;halfOptions.strength=.5f;
    const auto half=runWidth(halfOptions,plucks,rig,[](ChimeraProcessor& p){
        p.parameters().getParameter("doubleron")->setValueNotifyingHost(1);
        auto* t=p.parameters().getParameter("doublertime");t->setValueNotifyingHost(t->convertTo0to1(20));});
    check(half.analysis.changed() && near(value(half.values,"doublertime"),20+.5f*(half.analysis.spreadAfter-20),.01f) && value(half.values,"doubleron")>.5f,"Strength did not scale the spread change");
    // Bass: kept.
    auto bassOptions=widthOnly;bassOptions.instrument=2;
    const auto bass=runWidth(bassOptions,plucks,rig);
    check(stepOf(bass.status,P::width)->result.contains("bass") && value(bass.values,"doubleron")==value(bass.before,"doubleron"),"Bass turned the doubler on");
    std::cout<<"MEASURE auto set width controller: "<<step->result<<'\n';
    std::cout<<"PASS auto set width controller: doubler on from the Tone EQ pass's rig sound, lock, strength on a spread change, bass kept, undo\n";
}
