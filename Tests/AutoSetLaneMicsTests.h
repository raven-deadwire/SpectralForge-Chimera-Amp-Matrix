#pragma once
// Phase 31: the two blended mics of each Dual rig's or Matrix lane's cabinet (docs/AUTO_SET_PHASE31.md).
// Included by AutoSetTests.cpp after the mics tests (their helpers are used here).

void laneMicsRules() {
    for(const double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{310};
        const auto late=micPair(rate,6,rng,.6f,-1.f); // mic B late and opposed
        // Alignment only (a lane that plays one band): the same alignment as phase 18, the blend kept.
        const auto whole=analyseMics(late.a,late.b,rate,{0.f,0.f},false,.5f,false,Style::rock);
        const auto band=analyseMics(late.a,late.b,rate,{0.f,0.f},false,.5f,false,Style::rock,false);
        check(band.aligned() && band.delayAfterMs[0]==whole.delayAfterMs[0] && band.delayAfterMs[1]==whole.delayAfterMs[1] && band.invertAfter==whole.invertAfter
              && band.cancellationAfterDb==whole.cancellationAfterDb && band.blendAfter==.5f,"Alignment alone differs from phase 18's alignment, or moved the blend");
        // A pair that only wants a blend move is kept when the blend is not judged.
        auto dark=late.a;shelve(dark,3000,-10,rate);
        const auto judged=analyseMics(dark,late.a,rate,{0.f,0.f},false,.3f,false,Style::rock),kept=analyseMics(dark,late.a,rate,{0.f,0.f},false,.3f,false,Style::rock,false);
        check(judged.blendAfter>.3f && !kept.changed() && kept.toneErrorAfterDb==kept.toneErrorBeforeDb,"A blend that is not judged was moved");
    }
    std::cout<<"PASS auto set lane mics rules at 44.1/48/96 kHz: alignment alone equals phase 18's alignment and keeps the blend\n";
}
// The processor feeds the mic taps from the lane Auto Set chose, in a routing that has it, with both of its mics heard.
void laneMicsCapture() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    std::mt19937 rng{311};const auto x=targetNoise(false,48000,1,rng);
    for(int lane=1;lane<=3;++lane){setRaw("cab"+juce::String(lane),1);setRaw("cabBtype"+juce::String(lane),1);}
    const auto capture=[&](int mode,int micLane,std::array<float,3> blends,bool fed) {
        setRaw("mode",float(mode));for(int lane=0;lane<3;++lane)setRaw("cabblend"+juce::String(lane+1),blends[size_t(lane)]);
        p->setAutoSetMicLane(micLane);
        juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
        const auto run=[&](int blocks){for(int block=0;block<blocks;++block){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+size_t(block%20)*256,256);p->processBlock(io,midi);}};
        const auto listen20=[&] {
            p->autoSetCapture().listen({Tap::micA,Tap::micB});run(20);
            std::vector<float> first,second;p->autoSetCapture().drainSamples(first);p->autoSetCapture().drainLaneSamples(second);
            p->autoSetCapture().listen(std::nullopt);
            return std::pair{first.size(),second.size()};
        };
        // As the Classic capture test: settle, then wait (up to 5 s) for the IR worker.
        run(40);
        auto last=listen20();
        for(int round=0;round<50 && (fed?last.first!=size_t(20*256):last.first!=0);++round){juce::Thread::sleep(100);run(20);last=listen20();}
        return last;
    };
    const size_t all=20*256;
    const auto rig2=capture(1,1,{.5f,.5f,.5f},true),rig1=capture(1,0,{.5f,.5f,.5f},true),rig2OneMic=capture(1,1,{.5f,0.f,.5f},false),noThirdRig=capture(1,2,{.5f,.5f,.5f},false);
    check(rig2.first==all && rig2.second==all && rig1.first==all && rig2OneMic.first==0 && noThirdRig.first==0,
          "Dual mic taps not fed for the chosen two-mic rig only ("+std::to_string(rig2.first)+", "+std::to_string(rig1.first)+", "+std::to_string(rig2OneMic.first)+", "+std::to_string(noThirdRig.first)+")");
    const auto high=capture(2,2,{.5f,.5f,.5f},true),classicLane=capture(0,1,{.5f,.5f,.5f},false),classicAsked=capture(1,-1,{.5f,.5f,.5f},false);
    check(high.first==all && high.second==all && classicLane.first==0 && classicAsked.first==0,
          "Matrix HIGH lane mics not fed, or a lane fed in Classic, or the Classic rig fed in Dual ("+std::to_string(high.first)+", "+std::to_string(classicLane.first)+", "+std::to_string(classicAsked.first)+")");
    p->releaseResources();
    std::cout<<"PASS auto set lane mics capture: taps fed from the chosen Dual rig or Matrix lane with both mics heard; not from a lane the routing lacks, nor across Classic and lane requests\n";
}
struct LaneMicsRun {std::vector<float> before,values,undone;Controller::Status status;std::array<MicsResult,3> analysis;};
// Feeds the mic pair to the mic taps for every lane mics pass (and to the lane taps, for a Dual or Matrix pass after them).
LaneMicsRun runLaneMics(const Controller::Options& options,const MicPair& mics,const std::function<void(ChimeraProcessor&)>& prepare,const std::function<void(ChimeraProcessor&)>& during={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    prepare(*p);
    Controller c(*p);c.setOptions(options);
    LaneMicsRun result;result.before=values(*p);
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0,fed=0;c.busy() && fed<size_t(48000*40);offset=(offset+256)%(mics.a.size()-256),fed+=256) {
        const float* a[]{mics.a.data()+offset};const float* b[]{mics.b.data()+offset};
        p->autoSetCapture().push(Tap::micA,a,1,256);p->autoSetCapture().push(Tap::micB,b,1,256);
        p->autoSetCapture().push(Tap::lane1,a,1,256);p->autoSetCapture().push(Tap::lane2,b,1,256);
        for(auto tap:{Tap::input,Tap::compressor})p->autoSetCapture().push(tap,a,1,256);
        ms+=1000.*256/48000;c.tick(ms);
        if(during && fed==0)during(*p);
    }
    check(!c.busy(),"Lane mics run did not finish");
    result.values=values(*p);result.status=c.current();for(int lane=0;lane<3;++lane)result.analysis[size_t(lane)]=c.lastLaneMicsAnalysis(lane);
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
Controller::Options laneMicsOnlyOptions() {auto o=laneCutsOnlyOptions();o.laneCuts=false;o.laneMics=true;return o;}
void controllerLaneMics() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto setter=[](std::initializer_list<std::pair<juce::String,float>> pairs){
        std::vector<std::pair<juce::String,float>> list(pairs);
        return [list](ChimeraProcessor& p){for(const auto& [id,v]:list)p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));};};
    check(!Controller::Options{}.laneMics && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.laneMics,"Lane mic alignment and blend is on by default");
    std::mt19937 rng{312};
    const auto late=micPair(48000,8,rng,.6f,-1.f); // mic B late and opposed
    const auto dual=setter({{"mode",1},{"dualtype",0},{"cab1",1},{"cab2",1},{"cabblend1",.5f},{"cabBtype1",1},{"cabblend2",.5f},{"cabBtype2",1}});
    // Dual Blend: a pass per rig, before any Dual pass; both rigs are aligned.
    const auto full=runLaneMics(laneMicsOnlyOptions(),late,dual);
    const auto* step1=stepOf(full.status,P::laneMics1);const auto* step2=stepOf(full.status,P::laneMics2);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::laneMics1,P::laneMics2} && step1 && step2 && !stepOf(full.status,P::laneMics3) && step1->changed && step2->changed
          && step1->result.contains("mic A DELAY") && step1->result.contains("POLARITY inverted") && step2->result.contains("POLARITY inverted"),
          "Lane mics passes did not run per rig or report: "+(step1?step1->result.toStdString():std::string("no step")));
    for(const char* n:{"1","2"}) {
        const auto& a=full.analysis[size_t(n[0]-'1')];const juce::String s(n);
        check(a.aligned() && near(value(full.values,"cabAdelay"+s),a.delayAfterMs[0],.01f) && value(full.values,"cabBinvert"+s)>.5f && value(full.values,"cabBdelay"+s)==value(full.before,"cabBdelay"+s),"Applied alignment does not match the analysis on rig "+std::string(n));
    }
    check(value(full.values,"cabAdelay3")==value(full.before,"cabAdelay3") && value(full.values,"cabBinvert3")==value(full.before,"cabBinvert3"),"A rig the routing lacks was changed");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the lane mics");
    // Dual Blend judges the blend too; strength scales it, never the alignment.
    auto dark=late.a;shelve(dark,3000,-10,48000);
    const MicPair blendPair{dark,micsShift(late.a,0)};
    auto halfOptions=laneMicsOnlyOptions();halfOptions.strength=.5f;
    const auto half=runLaneMics(halfOptions,blendPair,setter({{"mode",1},{"dualtype",0},{"cab1",1},{"cab2",1},{"cabblend1",.3f},{"cabBtype1",1},{"cabblend2",0}}));
    check(half.status.passes==std::vector<P>{P::laneMics1} && half.analysis[0].blendAfter>.3f && near(value(half.values,"cabblend1"),.3f+.5f*(half.analysis[0].blendAfter-.3f),1e-3f)
          && stepOf(half.status,P::laneMics2)->result.contains("one mic heard (mic A)"),"Strength did not scale the blend change, or a one-mic rig ran a pass");
    check(near(value(runLaneMics(halfOptions,late,dual).values,"cabAdelay1"),full.analysis[0].delayAfterMs[0],.01f),"Strength scaled the alignment");
    // Dual Crossover: each rig plays one band, so the mics are aligned and the blend is kept, with the reason.
    const auto crossover=runLaneMics(laneMicsOnlyOptions(),blendPair,setter({{"mode",1},{"dualtype",1},{"cab1",1},{"cab2",1},{"cabblend1",.3f},{"cabBtype1",1},{"cabblend2",0}}));
    check(near(value(crossover.values,"cabblend1"),.3f,1e-4f) && stepOf(crossover.status,P::laneMics1)->result.contains("BLEND kept: this lane plays one band"),"A band-split rig's blend was judged: "+stepOf(crossover.status,P::laneMics1)->result.toStdString());
    const auto crossAligned=runLaneMics(laneMicsOnlyOptions(),late,setter({{"mode",1},{"dualtype",1},{"cab1",1},{"cab2",1},{"cabblend1",.5f},{"cabBtype1",1},{"cabblend2",0}}));
    check(stepOf(crossAligned.status,P::laneMics1)->changed && value(crossAligned.values,"cabBinvert1")>.5f && near(value(crossAligned.values,"cabblend1"),.5f,1e-4f),"A band-split rig's mics were not aligned");
    // Matrix: a pass per lane with both mics heard; the others are reported.
    const auto matrix=runLaneMics(laneMicsOnlyOptions(),late,setter({{"mode",2},{"cab1",1},{"cab2",0},{"cab3",1},{"cabblend1",1},{"cabBtype1",1},{"cabblend2",.5f},{"cabBtype2",1},{"cabblend3",.5f},{"cabBtype3",1}}));
    check(matrix.status.passes==std::vector<P>{P::laneMics3} && stepOf(matrix.status,P::laneMics1)->result.contains("one mic heard (mic B)") && stepOf(matrix.status,P::laneMics2)->result=="kept (cabinet off)"
          && stepOf(matrix.status,P::laneMics3)->changed && value(matrix.values,"cabBinvert3")>.5f && stepOf(matrix.status,P::laneMics3)->result.contains("BLEND kept"),"Matrix lanes not judged one by one");
    // Kept: the Cabinet mics lock (no pass), the Classic routing, sources edited while listening.
    auto lockedOptions=laneMicsOnlyOptions();lockedOptions.locks=Controller::lockCompressor|Controller::lockMics;
    const auto locked=runLaneMics(lockedOptions,late,dual);
    check(locked.status.passes==std::vector<P>{P::input} && stepOf(locked.status,P::laneMics1)->result=="kept (locked)" && stepOf(locked.status,P::laneMics2)->result=="kept (locked)"
          && value(locked.values,"cabAdelay1")==value(locked.before,"cabAdelay1"),"The Cabinet mics lock did not keep the lane mics");
    auto classicOptions=laneMicsOnlyOptions();classicOptions.locks=Controller::lockCompressor;
    const auto classic=runLaneMics(classicOptions,late,setter({{"mode",0},{"cabblend1",.5f},{"cabBtype1",1}}));
    check(stepOf(classic.status,P::laneMics1) && stepOf(classic.status,P::laneMics1)->result=="kept (Classic routing: the Cabinet mic alignment and blend step sets rig 1)" && !stepOf(classic.status,P::laneMics2)
          && value(classic.values,"cabAdelay1")==value(classic.before,"cabAdelay1"),"Classic routing not reported");
    const auto edited=runLaneMics(laneMicsOnlyOptions(),late,dual,setter({{"cabBtype1",2}}));
    check(stepOf(edited.status,P::laneMics1)->result=="kept (routing, cabinet or mic sources changed while listening)" && value(edited.values,"cabAdelay1")==value(edited.before,"cabAdelay1"),"Mic sources edited while listening were overwritten");
    // With a step of the Dual pass: the mic passes come first, so the rigs are judged with their mics aligned.
    auto withDual=laneMicsOnlyOptions();withDual.laneCuts=true;
    const auto ordered=runLaneMics(withDual,late,dual);
    check(ordered.status.passes==std::vector<P>{P::laneMics1,P::laneMics2,P::dual} && stepOf(ordered.status,P::laneCuts),"The lane mics passes do not come before the Dual pass");
    std::cout<<"MEASURE auto set lane mics controller: rig 1: "<<step1->result<<" | crossover: "<<stepOf(crossover.status,P::laneMics1)->result<<'\n';
    std::cout<<"PASS auto set lane mics controller: off by default; a pass per Dual rig and Matrix lane; alignment whole, blend by strength; band-split lanes keep the blend; one mic, cabinet off, lock, Classic, edited; before the Dual pass; undo\n";
}
