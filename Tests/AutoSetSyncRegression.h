#pragma once
// Additional Phase 22 coverage; --sync-boundaries runs only these regressions.
// Synthetic playhead coverage is not a real DAW or listening qualification.
namespace syncRegression {
constexpr double rate=48000.;
constexpr int block=256;
enum class Supply { bpm,missingBpm,missingPosition,noPlayhead };
struct Case {const char* name;Supply supply;double bpm;float effective,reported;};

void set(ChimeraProcessor& p,const char* id,float value) {
    auto* q=p.parameters().getParameter(id);check(q!=nullptr,std::string("Missing sync parameter: ")+id);
    q->setValueNotifyingHost(q->convertTo0to1(value));
}
float get(ChimeraProcessor& p,const char* id) {return p.parameters().getRawParameterValue(id)->load();}

struct Probe {
    // Host outlives the processor, including when a check throws.
    TempoPlayHead host;
    std::unique_ptr<ChimeraProcessor> processor=std::make_unique<ChimeraProcessor>();
    juce::MidiBuffer midi;
    int channels{};
    float previous{},peak{},maxStep{};
    explicit Probe(const juce::MemoryBlock* saved=nullptr,bool restoreBeforePrepare=true) {
        auto& p=*processor;
        p.setRateAndBufferSizeDetails(rate,block);
        for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{
            {"delayon",1},{"delaysync",1},{"temposync",1},{"delaymodel",0},{"delaymodelExt",0},
            {"delayfeedback",0},{"delaymix",.5f},{"reverbon",0},{"tempo",90},{"delaytime",310}})set(p,id,v);
        if(saved && restoreBeforePrepare)p.setStateInformation(saved->getData(),int(saved->getSize()));
        p.prepareToPlay(rate,block);
        if(saved && !restoreBeforePrepare)p.setStateInformation(saved->getData(),int(saved->getSize()));
        channels=std::max(p.getTotalNumInputChannels(),p.getTotalNumOutputChannels());
        host.bpm=100;p.setPlayHead(&host);
    }
    ~Probe(){processor->setPlayHead(nullptr);processor->releaseResources();}
    void supply(const Case& c) {
        host.bpm=c.bpm;host.provideBpm=c.supply!=Supply::missingBpm;
        host.providePosition=c.supply!=Supply::missingPosition;
        processor->setPlayHead(c.supply==Supply::noPlayhead?nullptr:&host);
        if(c.supply==Supply::bpm) {
            const auto position=host.getPosition();
            check(position && position->getBpm(),std::string(c.name)+": BPM was not supplied");
            const double sent=*position->getBpm();
            check(std::isnan(c.bpm)?std::isnan(sent):sent==c.bpm,std::string(c.name)+": fixture changed BPM");
        }
    }
    void process(juce::AudioBuffer<float>& io) {
        midi.clear();processor->processBlock(io,midi);
        for(int c=0;c<io.getNumChannels();++c)for(int n=0;n<io.getNumSamples();++n) {
            const float v=io.getSample(c,n);
            check(std::isfinite(v),"Non-finite sync transition output");
            peak=std::max(peak,std::abs(v));
            if(c==0){maxStep=std::max(maxStep,std::abs(v-previous));previous=v;}
        }
    }
    void meters(const Case& c) {
        auto& p=*processor;
        check(near(p.currentTempo(),c.effective,.001f),std::string(c.name)+": wrong effective tempo");
        check(near(p.reportedHostTempo(),c.reported,.001f),std::string(c.name)+": wrong reported host tempo");
        const float followed=get(p,"temposync")>.5f?c.reported:0;
        check(near(p.currentHostTempo(),followed,.001f),std::string(c.name)+": wrong followed host meter");
        check(near(get(p,"tempo"),90,1e-6f),std::string(c.name)+": host overwrote internal TEMPO");
    }
    // No reset/reprepare/warm-up render between calls. Retain and inspect every
    // sample, including the first block after a host change. A 300 ms lead-in
    // separates settled echo timing from the transition's diagnostic metrics.
    std::vector<float> burst(const Case& c) {
        supply(c);
        constexpr int count=96000,start=14400,length=480; // 2 s; 40 BPM echo at 1.8 s
        std::vector<float> out;out.reserve(count);
        juce::AudioBuffer<float> io(channels,block);
        for(int offset=0;offset<count;offset+=block) {
            const int frames=std::min(block,count-offset);io.setSize(channels,frames,false,false,true);io.clear();
            for(int n=0;n<frames;++n)if(offset+n>=start && offset+n<start+length) {
                const float x=.3f*float(std::sin(2*juce::MathConstants<double>::pi*1000*double(offset+n-start)/rate));
                for(int channel=0;channel<std::min(2,channels);++channel)io.setSample(channel,n,x);
            }
            process(io);meters(c);
            out.insert(out.end(),io.getReadPointer(0),io.getReadPointer(0)+frames);
        }
        return out;
    }
};

double echoMs(const std::vector<float>& audio) {
    std::vector<float> env(audio.size()/48);
    for(size_t m=0;m<env.size();++m){double e=0;for(size_t n=m*48;n<m*48+48;++n)e+=double(audio[n])*audio[n];env[m]=float(std::sqrt(e/48));}
    const auto onset=[&](size_t first,size_t last){
        last=std::min(last,env.size());float peak=0;
        for(size_t m=first;m<last;++m)peak=std::max(peak,env[m]);
        for(size_t m=first;m<last;++m)if(peak>1e-4f && env[m]>=peak*.5f)return double(m);
        return -1.;
    };
    const double direct=onset(250,450);if(direct<0)return -1.;
    const double echo=onset(size_t(direct)+150,size_t(direct)+1700);
    return echo<0?-1.:echo-direct;
}
void expectEcho(const std::vector<float>& audio,double expected,const char* name) {
    const double measured=echoMs(audio);
    check(std::abs(measured-expected)<=2,std::string(name)+": echo "+std::to_string(measured)+" ms; expected "+std::to_string(expected));
    std::cout<<"MEASURE sync boundary "<<name<<": echo_ms="<<measured<<" expected_ms="<<expected<<'\n';
}

void rules() {
    const auto sparse=windowsOf(pluckedEvery(1000,6));
    for(const float bpm:{39.9f,40.f,100.f,120.f,240.f,240.1f}) {
        const bool valid=bpm>=40 && bpm<=240;
        const auto r=analyseTempoSync(sparse,false,Style::clean,bpm);
        check(r.hostTempo==valid && r.delayWanted==valid,"Wrong inclusive Auto Set tempo boundary");
        if(valid)check(near(r.delay.timeMs,60000.f/bpm,.01f),"Wrong boundary rule delay time");
    }
    for(const float bpm:{0.f,-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        check(!analyseTempoSync(sparse,false,Style::clean,bpm).hostTempo,"Auto Set accepted an invalid BPM");
    std::cout<<"PASS sync inclusive boundary rules (automatic selection only)\n";
}

void stream() {
    Probe p;
    // The out-of-range values intentionally lock in current DSP clamp behavior,
    // separately from Auto Set's refusal to select a new delay at those values.
    const Case cases[]{
        {"start100",Supply::bpm,100,100,100},{"to120",Supply::bpm,120,120,120},
        {"return100",Supply::bpm,100,100,100},{"at40",Supply::bpm,40,40,40},
        {"below40",Supply::bpm,39.9,40,0},{"return40",Supply::bpm,40,40,40},
        {"at240",Supply::bpm,240,240,240},{"above240",Supply::bpm,240.1,240,0},
        {"return240",Supply::bpm,240,240,240},{"beforeLoss100",Supply::bpm,100,100,100},
        {"missingBpm",Supply::missingBpm,100,90,0},{"recover120",Supply::bpm,120,120,120},
        {"missingPosition",Supply::missingPosition,120,90,0},{"recover100",Supply::bpm,100,100,100},
        {"noPlayhead",Supply::noPlayhead,100,90,0},{"recoverAgain120",Supply::bpm,120,120,120},
        {"actualNaN",Supply::bpm,std::numeric_limits<double>::quiet_NaN(),90,0},
        {"actualInfinity",Supply::bpm,std::numeric_limits<double>::infinity(),90,0},
        {"actualZero",Supply::bpm,0,90,0},{"actualNegative",Supply::bpm,-1,90,0},
        {"finalRecovery100",Supply::bpm,100,100,100}};
    for(const auto& c:cases)expectEcho(p.burst(c),60000./c.effective,c.name);
    std::cout<<"PASS sync continuous processor stream: boundary echoes, invalid/missing host, recovery; all output finite\n";
}

void transientDiagnostics() {
    for(const float feedback:{0.f,.3f}) {
        Probe p;set(*p.processor,"delayfeedback",feedback);
        juce::AudioBuffer<float> io(p.channels,block);
        // Keep a sustained signal and the live delay buffer across both tempo
        // directions, one-block loss, repeated loss, a ramp and stop/resume.
        for(int b=0;b<600;++b) {
            Case c{"live",Supply::bpm,100,100,100};
            if(b>=150 && b<250)c={"step120",Supply::bpm,120,120,120};
            if(b==300 || (b>=320 && b<336 && b%2==0))c={"shortLoss",Supply::missingBpm,100,90,0};
            if(b>=360 && b<460){const float bpm=100.f+20.f*float(b-360)/99.f;c={"ramp",Supply::bpm,bpm,bpm,bpm};}
            if(b>=460)c={"afterRamp",Supply::bpm,120,120,120};
            p.host.playing=!(b>=500 && b<530);p.supply(c);io.clear();
            for(int n=0;n<block;++n){const float x=.1f*float(std::sin(2*juce::MathConstants<double>::pi*1000*double(b*block+n)/rate));for(int channel=0;channel<std::min(2,p.channels);++channel)io.setSample(channel,n,x);}
            p.process(io);p.meters(c);
        }
        // These are diagnostics, not a click/pitch/no-missing-echo quality gate.
        // The current 35 ms moving read head can change pitch on tempo changes.
        std::cout<<"MEASURE sync transition diagnostics: feedback="<<feedback<<" peak="<<p.peak<<" max_sample_step="<<p.maxStep<<"; listening/artifact quality NOT VALIDATED\n";
    }
    std::cout<<"PASS sync live callback tempo/missing-data/ramp/stop-resume contract; output finite (not artifact-free certification)\n";
}

void restoredParameters(Probe& p,bool follow,bool sync) {
    for(const auto& [id,value]:std::initializer_list<std::pair<const char*,float>>{
        {"tempo",90},{"temposync",follow?1.f:0.f},{"delaysync",sync?1.f:0.f},
        {"delayon",1},{"delaytime",310},{"delayfeedback",0},{"delaymix",.5f}})
        check(near(get(*p.processor,id),value,1e-5f),std::string("Recall lost ")+id);
}
void recall() {
    Probe source;
    expectEcho(source.burst({"saveAt100",Supply::bpm,100,100,100}),600,"saveAt100");
    juce::MemoryBlock saved;source.processor->getStateInformation(saved);check(saved.getSize()>0,"Empty sync state");
    for(const bool beforePrepare:{true,false}) {
        Probe fresh(&saved,beforePrepare);restoredParameters(fresh,true,true);
        expectEcho(fresh.burst({"freshHost120",Supply::bpm,120,120,120}),500,beforePrepare?"restoreBeforePrepare":"restoreAfterPrepare");
    }
    // Restore over an already-running, differently configured processor.
    set(*source.processor,"tempo",45);set(*source.processor,"delaytime",700);
    set(*source.processor,"delaysync",0);set(*source.processor,"temposync",0);
    juce::AudioBuffer<float> io(source.channels,block);io.clear();source.process(io);
    source.processor->setStateInformation(saved.getData(),int(saved.getSize()));restoredParameters(source,true,true);
    expectEcho(source.burst({"usedHost120",Supply::bpm,120,120,120}),500,"restoreUsedInstance");
    source.processor->setStateInformation(saved.getData(),int(saved.getSize()));
    expectEcho(source.burst({"restoredMissingHost",Supply::missingBpm,120,90,0}),60000./90,"restoreWithoutHost");
    for(const bool synced:{true,false}) {
        set(*source.processor,"temposync",0);set(*source.processor,"delaysync",synced?1.f:0.f);
        juce::MemoryBlock manual;source.processor->getStateInformation(manual);
        Probe restored(&manual);restoredParameters(restored,false,synced);
        expectEcho(restored.burst({"manualAtHost120",Supply::bpm,120,90,120}),synced?60000./90:310,synced?"restoreManualSync":"restoreFreeTime");
    }
    std::cout<<"PASS sync state round-trip: prepare order, reused instance, current/missing host, manual and free-time controls; delay tails are not serialized\n";
}
void run() {rules();stream();transientDiagnostics();recall();}
} // namespace syncRegression
