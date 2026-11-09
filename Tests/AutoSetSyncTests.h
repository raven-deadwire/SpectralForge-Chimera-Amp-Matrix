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
    bool provideBpm{true},providePosition{true},playing{true};
    juce::Optional<PositionInfo> getPosition() const override {
        if(!providePosition)return {};
        PositionInfo p;p.setIsPlaying(playing);
        // Availability and numeric validity are independent. In particular,
        // NaN must reach the processor rather than turn into a missing BPM.
        if(provideBpm)p.setBpm(bpm);
        return p;
    }
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
            host.bpm=bpm;host.provideBpm=bpm!=0;p.setPlayHead(&host);
            juce::AudioBuffer<float> io(2,256);io.clear();juce::MidiBuffer midi;p.processBlock(io,midi);
        };
    };
    Controller::Options syncOnly{1,false,Style::clean,false,false,false,1.f,Controller::lockCompressor,false,false,false,false,false,true,false,false,false,false,false,false,false,false,false,false,true,false};
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

// DELAY-01 (verification dashboard): at 100 BPM the synced delay is a quarter
// note, 600 ms. The controller rows say 100 BPM, and the processor's echo,
// rendered, arrives 600 ms after the playing; it follows a host tempo change
// and falls back to Chimera's TEMPO when the host stops reporting one.
void syncTiming() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    static TempoPlayHead host;
    const auto set=[](ChimeraProcessor& p,const char* id,float v){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    // The controller at 100 BPM.
    Controller::Options syncOnly{1,false,Style::clean,false,false,false,1.f,Controller::lockCompressor,false,false,false,false,false,true,false,false,false,false,false,false,false,false,false,false,true,false};
    const auto run=runSpace(syncOnly,[&](ChimeraProcessor& p){
        set(p,"delaysync",0);set(p,"temposync",0);host.bpm=100;p.setPlayHead(&host);
        juce::AudioBuffer<float> io(2,256);io.clear();juce::MidiBuffer midi;p.processBlock(io,midi);});
    const auto* step=stepOf(run.status,P::sync);const auto* space=stepOf(run.status,P::space);
    check(step && step->changed && step->result.contains("(100.0 BPM)") && space && space->result.contains("delay on: host tempo 100.0 BPM")
          && value(run.values,"delaysync")>.5f && value(run.values,"temposync")>.5f && value(run.values,"delayon")>.5f,"DELAY-01: the 100 BPM rows or parameters are wrong");
    // The processor: the delay on, synced, following the host; no feedback, no reverb, an even mix.
    auto storage=std::make_unique<ChimeraProcessor>();auto& p=*storage;
    p.setRateAndBufferSizeDetails(48000,256);p.prepareToPlay(48000,256);
    for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{{"delayon",1},{"delaysync",1},{"temposync",1},{"delaymodel",0},{"delayfeedback",0},{"delaymix",.5f},{"reverbon",0},{"tempo",90}})set(p,id,v);
    host.bpm=100;p.setPlayHead(&host);
    const int channels=std::max(p.getTotalNumInputChannels(),p.getTotalNumOutputChannels());
    // A 10 ms 1 kHz burst after 300 ms of silence; the time from its onset to the echo's.
    const auto echoMs=[&]() {
        const double rate=48000;const size_t total=size_t(rate*1.6),start=size_t(rate*.3),burst=size_t(rate*.01);
        std::vector<float> out(total);juce::AudioBuffer<float> io(channels,256);juce::MidiBuffer midi;
        for(size_t offset=0;offset<total;offset+=256) {
            io.clear();
            for(int n=0;n<256;++n){const size_t i=offset+size_t(n);if(i>=start && i<start+burst){const float x=.3f*float(std::sin(2*juce::MathConstants<double>::pi*1000*double(i-start)/rate));for(int c=0;c<std::min(2,channels);++c)io.setSample(c,n,x);}}
            p.processBlock(io,midi);
            for(int n=0;n<256 && offset+size_t(n)<total;++n)out[offset+size_t(n)]=io.getSample(0,n);
        }
        // 1 ms RMS envelope; an onset is the first millisecond above half its segment's peak.
        std::vector<float> env(total/48);for(size_t m=0;m<env.size();++m){double e=0;for(size_t i=m*48;i<m*48+48;++i)e+=double(out[i])*out[i];env[m]=float(std::sqrt(e/48));}
        const auto onset=[&](size_t from,size_t to){float top=0;for(size_t m=from;m<to;++m)top=std::max(top,env[m]);for(size_t m=from;m<to;++m)if(env[m]>=.5f*top && top>1e-4f)return double(m);return -1.;};
        const double direct=onset(250,550);
        if(direct<0)return -1.;
        const double echo=onset(size_t(direct)+150,std::min(env.size(),size_t(direct)+1250));
        return echo<0?-1.:echo-direct;
    };
    echoMs(); // settle the delay time
    const double at100=echoMs();
    host.bpm=120;echoMs();const double at120=echoMs();
    host.provideBpm=false;echoMs();const double noHost=echoMs();
    host.provideBpm=true;host.bpm=std::numeric_limits<double>::quiet_NaN();
    const auto invalidPosition=host.getPosition();
    check(invalidPosition && invalidPosition->getBpm() && std::isnan(*invalidPosition->getBpm()),"DELAY-01: the test host did not deliver NaN");
    echoMs();const double nanHost=echoMs();
    p.setPlayHead(nullptr);p.releaseResources();
    check(std::abs(at100-600)<=2,"DELAY-01: the echo at 100 BPM came after "+std::to_string(at100)+" ms, not 600");
    check(std::abs(at120-500)<=2,"DELAY-01: the echo did not follow the host to 120 BPM ("+std::to_string(at120)+" ms)");
    check(std::abs(noHost-60000./90)<=2 && std::abs(nanHost-60000./90)<=2,"DELAY-01: without a host tempo the echo did not use Chimera's TEMPO, 90 BPM ("+std::to_string(noHost)+", "+std::to_string(nanHost)+" ms)");
    std::cout<<"MEASURE auto set sync timing: "<<step->result<<" | echo "<<at100<<" ms at 100 BPM, "<<at120<<" ms at 120 BPM, "<<noHost<<" ms with no host tempo and "<<nanHost<<" ms with NaN (TEMPO 90)\n";
    std::cout<<"PASS auto set sync timing (DELAY-01): 100 BPM rows and parameters, rendered echo at 600 ms, follows the host to 120 BPM, falls back to TEMPO without a host tempo\n";
}

