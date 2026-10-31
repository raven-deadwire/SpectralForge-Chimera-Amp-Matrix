#pragma once
#include "AutoSetSpaceFixtures.h"

// Included by AutoSetTests.cpp: shares check(), runSpace() and DI WAV output.
void spaceSignalRegression(const juce::File& report={}) {
    juce::String csv="rate,block,fixture,reference_bpm,expected_onsets,detected_onsets,detected_nps,peak_nps,candidate_bpm,candidate_error_percent,unsynced_delay,synced_delay,synced_ms,pause_fraction,sustain_ms\n";
    int cases=0;
    for(double rate:{44100.,48000.,96000.})for(int block:{64,256,1024})for(const auto& f:spaceFixtures::scores()) {
        const auto w=spaceFixtures::capture(spaceFixtures::render(f,rate),rate,block);
        const auto free=analyseSpace(w,false,Style::clean);
        const auto sync=analyseSpace(w,false,Style::clean,float(f.bpm));
        check(free.feel.onsets==int(f.notes.size()),"Space onset count: "+f.name);
        check(near(free.feel.onsetsPerSecond,float(f.notes.size()/6.),.001f),"Space density: "+f.name);
        check(!free.delay.on && free.delay.timeMs==0,"Untrusted pulse enabled free-time delay: "+f.name);
        if(sync.delay.on)check(near(sync.delay.timeMs,float(60000./f.bpm),.001f),"Synced delay missed reference BPM");
        if(f.name=="long-rests" || f.name=="sixteenths" || f.name=="triplets")
            check(!sync.delay.on && sync.feel.peakOnsetsPerSecond>4,"Busy phrase hidden by rests/average");
        if(f.name=="sustain" || f.name=="syncopation" || (f.bpm<=120 && (f.name=="eighths" || f.name=="shuffle" || f.name=="uneven-picking")))
            check(sync.delay.on,"Usable sync and sparse phrase were rejected: "+f.name);
        check(!analyseSpace(w,true,Style::clean,float(f.bpm)).delay.on && !analyseSpace(w,false,Style::metal,float(f.bpm)).delay.on,"Bass/metal got new delay");
        const double candidate=free.feel.beatMs>0?60000./free.feel.beatMs:0;
        csv+=juce::String(rate,0)+","+juce::String(block)+","+f.name+","+juce::String(f.bpm,0)+","+juce::String(int(f.notes.size()))+","+juce::String(free.feel.onsets)
            +","+juce::String(free.feel.onsetsPerSecond,6)+","+juce::String(free.feel.peakOnsetsPerSecond,0)+","+juce::String(candidate,6)+","+juce::String(100.*(candidate/f.bpm-1),6)
            +",0,"+juce::String(int(sync.delay.on))+","+juce::String(sync.delay.timeMs,6)+","+juce::String(free.feel.pauseFraction,6)+","+juce::String(free.feel.sustainMs,3)+"\n";
        ++cases;
    }
    const auto w=spaceFixtures::capture(spaceFixtures::render(spaceFixtures::scores().front(),48000),48000);
    for(float bpm:{0.f,-1.f,39.f,241.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        check(!analyseSpace(w,false,Style::clean,bpm).delay.on,"Invalid tempo enabled delay");
    for(double ms:{0.,-1.,std::numeric_limits<double>::quiet_NaN()})check(!measurePlayingFeel(w,ms).measured,"Invalid window duration accepted");
    auto bad=w;bad[20].rmsDb=std::numeric_limits<float>::quiet_NaN();check(!measurePlayingFeel(bad).measured,"Nonfinite input accepted");
    // Count the initial attack too, with no invented pre-roll requirement.
    spaceFixtures::Fixture boundary{"first-window",120,{{0,.25,.2},{.5,.25,.2},{1,.25,.2},{1.5,.25,.2}}};
    check(measurePlayingFeel(spaceFixtures::capture(spaceFixtures::render(boundary,48000),48000)).onsets==4,"First attack was dropped");
    if(report!=juce::File())check(report.replaceWithText(csv),"Cannot write space report");
    std::cout<<"PASS space rhythm matrix: "<<cases<<" synthetic cases, exact onsets, untrusted pulse blocked, busy bursts blocked, trusted sync exact\n";
}

void spaceWriteFixtures(const juce::File& dir) {
    check(dir.createDirectory().wasOk(),"Cannot create space fixture directory");
    juce::String events="fixture,bpm,onset_seconds,amplitude,duration_seconds\n";
    for(const auto& f:spaceFixtures::scores()) {
        for(const auto& n:f.notes)events+=juce::String(f.name)+","+juce::String(f.bpm,0)+","+juce::String(n.time,9)+","+juce::String(n.amplitude,6)+","+juce::String(n.duration,6)+"\n";
        for(double rate:{44100.,48000.,96000.}) {
            const auto x=spaceFixtures::render(f,rate);juce::AudioBuffer<float> b(1,int(x.size()));b.copyFrom(0,0,x.data(),int(x.size()));
            autoSetDI::write(dir.getChildFile(juce::String(f.name)+"-"+juce::String(f.bpm,0)+"bpm-"+juce::String(rate,0)+".wav"),b,rate);
        }
    }
    check(dir.getChildFile("events.csv").replaceWithText(events),"Cannot write fixture score");
}

struct SpacePlayHead final:juce::AudioPlayHead {
    double bpm{120};bool position{true},hasBpm{true};
    juce::Optional<PositionInfo> getPosition() const override {
        if(!position)return {};
        PositionInfo p;if(hasBpm)p.setBpm(bpm);return p;
    }
};
void spaceSet(ChimeraProcessor& p,const char* id,float value) {
    auto* q=p.parameters().getParameter(id);check(q!=nullptr,std::string("Missing parameter ")+id);q->setValueNotifyingHost(q->convertTo0to1(value));
}
void spaceAudioTick(ChimeraProcessor& p) {
    juce::AudioBuffer<float> b(2,256);b.clear();juce::MidiBuffer midi;p.processBlock(b,midi);
}
// Render the real echo module with the tempo supplied by real processBlock.
// This tests sample timing, not acoustic/DAW approval or listener preference.
void spaceEchoTiming(double rate,float bpm) {
    spectralforge::EchoModule echo;echo.prepare({rate,256,2});
    const float ms=60000.f/bpm;juce::AudioBuffer<float> b(2,256);
    for(int k=0;k<40;++k){b.clear();echo.process(b,true,0,ms,0,1);}
    int peak=-1;float maximum=0;
    for(int n=0;n<int(rate*1.6);n+=256) {
        b.clear();if(n==0)b.setSample(0,0,1);
        echo.process(b,true,0,ms,0,1);
        for(int i=0;i<256;++i)if(std::abs(b.getSample(0,i))>maximum){maximum=std::abs(b.getSample(0,i));peak=n+i;}
    }
    check(maximum>.45f && std::abs(double(peak)-rate*60./bpm)<=1.1,"Synced echo sample timing");
}
void spaceControllerRegression() {
    const Controller::Options only{1,false,Style::clean,false,false,false,1.f,Controller::lockCompressor,false,false,false,false,false,true};
    const auto value=[](const std::vector<float>& v,const char* id){return valueOf(LockRun{v,{},{}},id);};
    const auto free=runSpace(only,[](ChimeraProcessor& p){spaceSet(p,"delaysync",0);spaceSet(p,"delaytime",417);});
    check(value(free.values,"delayon")==0 && value(free.values,"delaytime")==value(free.before,"delaytime"),"Untrusted beat changed free delay");
    check(stepOf(free.status,Controller::Phase::space)->result.contains("onset gaps do not establish tempo"),"Missing no-tempo explanation");
    // Existing active effects retain all non-mix state, with sync on and off.
    for(float sync:{0.f,1.f}) {
        const auto own=runSpace(only,[sync](ChimeraProcessor& p){
            for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{{"delayon",1},{"delaytime",417},{"delayfeedback",.67f},{"delaymix",.42f},{"delaysync",sync},{"delaymodel",2},{"reverbon",1},{"reverbsize",.83f},{"reverbdamping",.17f},{"reverbmodel",2},{"tempo",137}})spaceSet(p,id,v);
        });
        for(const auto* id:{"delayon","delaytime","delayfeedback","delaysync","delaymodel","reverbon","reverbsize","reverbdamping","reverbmodel","tempo","temposync"})
            check(value(own.values,id)==value(own.before,id),std::string("Active effect setting changed: ")+id);
        for(size_t i=0;i<own.before.size();++i)check(near(own.undone[i],own.before[i],1e-6f),"Space undo changed original state");
    }
    const auto manual=runSpace(only,[](ChimeraProcessor& p){spaceSet(p,"tempo",137);});
    check(manual.analysis.delay.on && near(manual.analysis.delay.timeMs,60000.f/137,.001f)
          && stepOf(manual.status,Controller::Phase::space)->result.contains("manual/TAP tempo 137"),"Manual sync mislabeled as host");
    const auto noHost=runSpace(only,[](ChimeraProcessor& p){spaceSet(p,"temposync",1);spaceAudioTick(p);});
    check(value(noHost.values,"delayon")==0,"No playhead enabled delay");
    SpacePlayHead host;
    for(double bpm:{40.,90.,120.,150.,240.}) {
        host.bpm=bpm;
        const auto synced=runSpace(only,[&](ChimeraProcessor& p){spaceSet(p,"temposync",1);p.setPlayHead(&host);spaceAudioTick(p);});
        check(value(synced.values,"delayon")==1 && near(synced.analysis.delay.timeMs,float(60000./bpm),.001f),"Real host BPM not used by Auto Set");
        check(value(synced.values,"delaytime")==value(synced.before,"delaytime"),"Host sync overwrote free-time setting");
    }
    for(int mode=0;mode<7;++mode) {
        host.position=mode!=0;host.hasBpm=mode!=1;host.bpm=mode==2?0:mode==3?-20:mode==4?std::numeric_limits<double>::quiet_NaN():mode==5?300:std::numeric_limits<double>::infinity();
        const auto missing=runSpace(only,[&](ChimeraProcessor& p){spaceSet(p,"temposync",1);p.setPlayHead(&host);spaceAudioTick(p);});
        check(value(missing.values,"delayon")==0 && stepOf(missing.status,Controller::Phase::space)->result.contains("no usable sync tempo"),"Missing/invalid host enabled delay via fallback");
    }
    // Real processor tempo changes, loss of a formerly valid host and reprepare.
    host.position=host.hasBpm=true;
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);spaceSet(*p,"temposync",1);p->setPlayHead(&host);
    for(float bpm:{120.f,90.f,150.f}) {
        host.bpm=bpm;spaceAudioTick(*p);check(near(p->currentTempo(),bpm,.001f) && near(p->currentHostTempo(),bpm,.001f),"Host tempo change not followed");
        for(double rate:{44100.,48000.,96000.})spaceEchoTiming(rate,p->currentTempo());
    }
    host.hasBpm=false;spaceAudioTick(*p);check(p->currentHostTempo()==0,"Lost host tempo remained trusted");
    host.hasBpm=true;spaceAudioTick(*p);p->prepareToPlay(48000,256);check(p->currentHostTempo()==0,"Reprepare retained stale host confidence");
    p->setPlayHead(nullptr);p->releaseResources();
    std::cout<<"PASS space controller: free-time guard, active effect state + undo, 5 host BPMs, 7 missing/invalid host cases, tempo changes and 9 echo impulse timings\n";
}
