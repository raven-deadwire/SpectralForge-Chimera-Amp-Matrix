#pragma once
// Phase 22: the delay's tempo sync to the host. Included by AutoSetTests.cpp after the space tests (runSpace, windowsOf, pluckedEvery).

void syncRules() {
    const auto sparse=windowsOf(pluckedEvery(1000,6));
    // A host tempo, sparse clean guitar: phase 11 would add a delay, so sync is wanted.
    const auto clean=analyseTempoSync(sparse,false,Style::clean,90);
    check(clean.feel.measured && clean.hostTempo && clean.delayWanted && near(clean.bpm,90.f,1e-6f) && near(clean.delay.timeMs,60000.f/90.f,.5f),"A host tempo with sparse clean playing did not want a synced delay");
    // No host tempo, or one outside 40-240 BPM: nothing to sync to.
    for(const float bpm:{0.f,30.f,300.f,std::numeric_limits<float>::quiet_NaN()})
        check(!analyseTempoSync(sparse,false,Style::clean,bpm).hostTempo,"A missing or out-of-range host tempo was used");
    // Phase 11 would add no delay: bass, metal, a fast phrase.
    check(std::string(analyseTempoSync(sparse,true,Style::rock,120).reason)=="bass" && std::string(analyseTempoSync(sparse,false,Style::metal,120).reason)=="metal style"
          && std::string(analyseTempoSync(windowsOf(pluckedEvery(200,6,48000,20)),false,Style::rock,120).reason)=="fast phrase","Sync was wanted where phase 11 adds no delay");
    check(!analyseTempoSync(windowsOf(std::vector<float>(48000*6)),false,Style::rock,120).feel.measured,"Silence reported as playing");
    std::cout<<"MEASURE sync rules: sparse clean at 90 BPM: delay "<<clean.delay.timeMs<<" ms, mix "<<clean.delay.mix<<'\n';
    std::cout<<"PASS auto set sync rules: host tempo with a delay wanted, missing/out-of-range tempo, bass, metal and fast phrase kept, silence\n";
}
// A host that reports a tempo (and nothing else).
struct TempoPlayHead final : juce::AudioPlayHead {
    double bpm{};
    juce::Optional<PositionInfo> getPosition() const override {PositionInfo p;if(bpm>0)p.setBpm(bpm);return p;}
};
void controllerSync() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    static TempoPlayHead host;
    // runSpace selects DELAY SYNC at 60 BPM; these runs start with it off and a host tempo.
    const auto withHost=[](double bpm,std::initializer_list<std::pair<const char*,float>> extra={}){
        std::vector<std::pair<const char*,float>> list(extra);
        return [bpm,list](ChimeraProcessor& p){
            for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{{"delaysync",0},{"temposync",0}}){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));}
            for(const auto& [id,v]:list){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));}
            host.bpm=bpm;p.setPlayHead(&host);
            juce::AudioBuffer<float> io(2,256);io.clear();juce::MidiBuffer midi;p.processBlock(io,midi);
        };
    };
    Controller::Options syncOnly{1,false,Style::clean,false,false,false,1.f,Controller::lockCompressor,false,false,false,false,false,true,false,false,false,false,false,false,false,false,false,false,true};
    // Host at 90 BPM: sync goes on, following the host, and the delay + reverb step puts the delay on at that tempo.
    const auto full=runSpace(syncOnly,withHost(90));
    const auto* step=stepOf(full.status,P::sync);const auto* space=stepOf(full.status,P::space);
    check(full.status.phase==P::done && step && step->changed && step->result.contains("DELAY SYNC on, following the host tempo (90.0 BPM)") && space && space->result.contains("delay on: host tempo 90.0 BPM")
          && value(full.values,"delaysync")>.5f && value(full.values,"temposync")>.5f && value(full.values,"delayon")>.5f,"Delay sync was not turned on to the host tempo");
    const auto& rows=full.status.steps;
    check(std::find_if(rows.begin(),rows.end(),[](const auto& s){return s.phase==P::sync;})<std::find_if(rows.begin(),rows.end(),[](const auto& s){return s.phase==P::space;}),"The sync row does not come before the delay + reverb row");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the delay sync");
    // No host tempo: kept, and the delay stays off as before.
    const auto none=runSpace(syncOnly,withHost(0));
    check(stepOf(none.status,P::sync)->result.contains("host reports no tempo") && value(none.values,"delaysync")<.5f && value(none.values,"delayon")<.5f
          && stepOf(none.status,P::space)->result.contains("select DELAY SYNC"),"No host tempo changed the delay");
    // Sync already on, a delay already on, metal: kept.
    const auto already=runSpace(syncOnly,withHost(90,{{"delaysync",1}}));
    check(stepOf(already.status,P::sync)->result.contains("already on") && value(already.values,"temposync")<.5f,"Sync already on was changed");
    const auto ownDelay=runSpace(syncOnly,withHost(90,{{"delayon",1}}));
    check(stepOf(ownDelay.status,P::sync)->result.contains("delay is on with its own time") && value(ownDelay.values,"delaysync")<.5f,"A free-time delay was synced");
    auto metal=syncOnly;metal.style=Style::metal;
    check(stepOf(runSpace(metal,withHost(90)).status,P::sync)->result.contains("metal style"),"Metal turned on a delay sync");
    // The lock, and Delay + reverb locked: kept.
    auto lockedOptions=syncOnly;lockedOptions.locks=Controller::lockCompressor|Controller::lockSync;
    const auto locked=runSpace(lockedOptions,withHost(90));
    check(stepOf(locked.status,P::sync)->result=="kept (locked)" && value(locked.values,"delaysync")<.5f,"Sync lock did not keep it");
    auto spaceLocked=syncOnly;spaceLocked.locks=Controller::lockCompressor|Controller::lockSpace;
    check(stepOf(runSpace(spaceLocked,withHost(90)).status,P::sync)->result.contains("Delay + reverb locked"),"Sync ran with the delay + reverb step locked");
    std::cout<<"MEASURE auto set sync controller: "<<step->result<<" | "<<space->result<<'\n';
    std::cout<<"PASS auto set sync controller: host tempo turns sync on before the delay + reverb step, which adds the delay at it; no host tempo, sync on, own delay, metal kept; lock; delay + reverb locked; undo\n";
}
