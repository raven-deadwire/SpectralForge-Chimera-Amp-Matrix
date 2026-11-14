#pragma once
// Phase 28: the Dual rig 1 and Matrix LOW lane cabinet cuts (docs/AUTO_SET_PHASE28.md).
// Included by AutoSetTests.cpp after the lane cuts tests (their helpers are used here).

void rig1CutsRules() {
    float worstPrediction=0;
    for(const double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{280};
        const auto a=targetNoise(false,rate,6,rng),b=targetNoise(false,rate,6,rng);
        auto fizzyA=a;shelve(fizzyA,5000,12,rate);
        auto fizzyB=b;shelve(fizzyB,5000,12,rate);
        const auto even2=withCuts(b,rate,70,9000);
        // Dual parallel at 50%: rig 1 fizzes with its HIGH cut wide open; only rig 1 may move.
        const auto fizzy1=withCuts(fizzyA,rate,70,14000);
        const auto one=analyseLaneCuts({&fizzy1,&even2},{.5,.5},{{0,{70.f,14000.f}}},rate,false);
        check(one.playingDetected && one.lanes.size()==1 && one.lanes[0].lane==0 && one.lanes[0].heard && one.lanes[0].highChanged() && one.lanes[0].highAfter<14000.f
              && one.lanes[0].highAfter>=7000.f-1e-3f && one.lanes[0].highErrorAfterDb<one.lanes[0].highErrorBeforeDb-cutMinGainDb,"Rig 1's fizz did not lower its HIGH cut");
        const auto heardOne=laneMix({&fizzy1,&even2},{.5,.5});
        const auto re1=withCuts(fizzyA,rate,one.lanes[0].lowAfter,one.lanes[0].highAfter);
        const float actualOne=laneRegionError(heardOne,laneMix({&re1,&even2},{.5,.5}),rate,false,false);
        worstPrediction=std::max(worstPrediction,std::abs(actualOne-one.lanes[0].highErrorAfterDb));
        // Both rigs fizz and both may move: rig 1 is judged first, rig 2 with rig 1's choice.
        const auto fizzy2=withCuts(fizzyB,rate,70,14000);
        const auto both=analyseLaneCuts({&fizzy1,&fizzy2},{.5,.5},{{0,{70.f,14000.f}},{1,{70.f,14000.f}}},rate,false);
        check(both.lanes.size()==2 && both.lanes[0].lane==0 && both.lanes[1].lane==1 && both.lanes[0].highChanged() && both.lanes[0].highAfter<14000.f
              && both.lanes[1].highErrorBeforeDb==both.lanes[0].highErrorAfterDb,"Rig 2 was not judged from rig 1's chosen cut");
        const auto heardBoth=laneMix({&fizzy1,&fizzy2},{.5,.5});
        const auto reBoth1=withCuts(fizzyA,rate,both.lanes[0].lowAfter,both.lanes[0].highAfter),reBoth2=withCuts(fizzyB,rate,both.lanes[1].lowAfter,both.lanes[1].highAfter);
        const float actualBoth=laneRegionError(heardBoth,laneMix({&reBoth1,&reBoth2},{.5,.5}),rate,false,false);
        worstPrediction=std::max(worstPrediction,std::abs(actualBoth-both.lanes[1].highErrorAfterDb));
        // Matrix: the LOW lane booms with its LOW cut at 30 Hz; the cut rises within the Drop A ceiling.
        auto lanes=matrixLanes(rate,6,rng);
        auto boomLow=lanes.low;lowShelf(boomLow,100,12,rate);
        const auto low=withCuts(boomLow,rate,30,9000),mid=withCuts(lanes.mid,rate,70,9000),high=withCuts(lanes.high,rate,70,9000);
        const auto matrix=analyseLaneCuts({&low,&mid,&high},{1,1,1},{{0,{30.f,9000.f}}},rate,false);
        check(matrix.lanes.size()==1 && matrix.lanes[0].lowChanged() && matrix.lanes[0].lowAfter>30.f && matrix.lanes[0].lowAfter<=cutLowLimit(false,Style::rock),
              "The Matrix LOW lane's boom did not raise its LOW cut within the limit");
        const auto reLow=withCuts(boomLow,rate,matrix.lanes[0].lowAfter,matrix.lanes[0].highAfter);
        const float actualLow=laneRegionError(laneMix({&low,&mid,&high},{1,1,1}),laneMix({&reLow,&mid,&high},{1,1,1}),rate,true,false);
        worstPrediction=std::max(worstPrediction,std::abs(actualLow-matrix.lanes[0].lowErrorAfterDb));
        // A silent rig 1 is reported and kept.
        const std::vector<float> quiet(even2.size(),0.f);
        const auto silent=analyseLaneCuts({&quiet,&even2},{.5,.5},{{0,{70.f,9000.f}}},rate,false);
        check(silent.playingDetected && !silent.lanes[0].heard && !silent.changed(),"A silent rig 1 was judged");
        if(rate==48000.)std::cout<<"MEASURE rig 1 cuts rules 48 kHz: rig 1 fizz HIGH 14 kHz > "<<one.lanes[0].highAfter<<" Hz (top "<<one.lanes[0].highErrorBeforeDb<<" > "<<one.lanes[0].highErrorAfterDb
                                <<" dB predicted, "<<actualOne<<" re-rendered); both rigs: rig 1 > "<<both.lanes[0].highAfter<<" Hz, rig 2 > "<<both.lanes[1].highAfter<<" Hz ("<<both.lanes[1].highErrorAfterDb
                                <<" predicted, "<<actualBoth<<" re-rendered); Matrix LOW lane LOW 30 > "<<matrix.lanes[0].lowAfter<<" Hz ("<<matrix.lanes[0].lowErrorAfterDb<<" predicted, "<<actualLow<<" re-rendered)\n";
    }
    check(worstPrediction<.5f,"The rig 1 cut prediction misses the re-rendered mix by "+std::to_string(worstPrediction)+" dB");
    std::cout<<"MEASURE rig 1 cuts prediction vs re-rendered mix: worst "<<worstPrediction<<" dB\n";
    std::cout<<"PASS auto set rig 1 cuts rules at 44.1/48/96 kHz: rig 1 fizz, both rigs in order, Matrix LOW lane boom, silent rig 1, prediction vs re-render\n";
}
Controller::Options rig1CutsOnlyOptions() {auto o=laneCutsOnlyOptions();o.laneCuts=false;o.rig1Cuts=true;return o;}
void controllerRig1Cuts() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto setter=[](std::initializer_list<std::pair<juce::String,float>> pairs){
        std::vector<std::pair<juce::String,float>> list(pairs);
        return [list](ChimeraProcessor& p){for(const auto& [id,v]:list){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));}};
    };
    check(!Controller::Options{}.rig1Cuts && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.rig1Cuts,"Rig 1 / LOW lane cabinet cuts are on by default");
    std::mt19937 rng{281};
    const auto a=targetNoise(false,48000,8,rng),b=targetNoise(false,48000,8,rng);
    auto fizzyA=a;shelve(fizzyA,5000,12,48000);
    auto fizzyB=b;shelve(fizzyB,5000,12,48000);
    const auto rig1=withCuts(fizzyA,48000,70,14000),rig2=withCuts(b,48000,70,9000);
    const auto dual=setter({{"mode",1},{"dualtype",0},{"dualblend",.5f},{"cab1",1},{"cab2",1},{"cablow1",70},{"cabhigh1",14000},{"cablow2",70},{"cabhigh2",9000}});
    // Dual: the step runs alone in the Dual pass and lowers rig 1's HIGH cut; rig 2's cuts stay.
    const auto full=runDual(rig1CutsOnlyOptions(),rig1,rig2,dual);
    const auto* step=stepOf(full.status,P::rig1Cuts);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::dual} && step && step->changed && step->result.contains("rig 1 HIGH 14.0 kHz > ") && !stepOf(full.status,P::laneCuts),
          "Rig 1 cuts step did not run or report: "+(step?step->result.toStdString():std::string("no step")));
    check(full.laneCuts.lanes.size()==1 && near(value(full.values,"cabhigh1"),full.laneCuts.lanes[0].highAfter,1.f) && value(full.values,"cabhigh1")<14000.f
          && near(value(full.values,"cabhigh2"),9000.f,1e-3f) && near(value(full.values,"cablow2"),70.f,1e-3f),"Applied rig 1 cut does not match the analysis, or rig 2 moved");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore rig 1's cuts");
    // Strength scales the move in Hz.
    auto half=rig1CutsOnlyOptions();half.strength=.5f;
    const auto halved=runDual(half,rig1,rig2,dual);
    check(near(value(halved.values,"cabhigh1"),14000.f+.5f*(halved.laneCuts.lanes[0].highAfter-14000.f),1.f),"Strength did not scale rig 1's cut");
    // Kept: the Cabinet cuts lock, rig 1's cabinet off, the cuts edited while listening, a silent rig 1.
    auto lockedOptions=rig1CutsOnlyOptions();lockedOptions.locks=Controller::lockCompressor|Controller::lockCuts;
    const auto locked=runDual(lockedOptions,rig1,rig2,dual);
    check(stepOf(locked.status,P::rig1Cuts) && stepOf(locked.status,P::rig1Cuts)->result=="kept (locked)" && locked.status.passes==std::vector<P>{P::input},"The Cabinet cuts lock did not keep rig 1's cuts or still listened");
    const auto cabOff=runDual(rig1CutsOnlyOptions(),rig1,rig2,[&](ChimeraProcessor& p){dual(p);setter({{"cab1",0}})(p);});
    check(stepOf(cabOff.status,P::rig1Cuts)->result=="kept (rig 1 cabinet off)" && near(value(cabOff.values,"cabhigh1"),14000.f,1.f),"A cabinet that is off had its cuts moved");
    const auto edited=runDual(rig1CutsOnlyOptions(),rig1,rig2,dual,setter({{"cabhigh1",12000}}));
    check(stepOf(edited.status,P::rig1Cuts)->result=="kept (cuts edited while listening)" && near(value(edited.values,"cabhigh1"),12000.f,1.f),"A cut edited while listening was overwritten");
    const std::vector<float> quiet(rig1.size(),0.f);
    const auto silent=runDual(rig1CutsOnlyOptions(),quiet,rig2,dual);
    check(stepOf(silent.status,P::rig1Cuts)->result.contains("rig 1 silent"),"A silent rig 1 was not reported");
    // Classic routing: no lane pass; the Cabinet cuts step covers rig 1.
    auto classicOptions=rig1CutsOnlyOptions();classicOptions.locks=Controller::lockCompressor;
    const auto classic=runSpace(classicOptions);
    check(stepOf(classic.status,P::rig1Cuts) && stepOf(classic.status,P::rig1Cuts)->result=="kept (Classic routing: the Cabinet cuts step sets rig 1)","Classic routing not reported");
    // With the lane cuts step: both rigs fizz; rig 1's row comes first and both cuts fall.
    const auto fizzy2=withCuts(fizzyB,48000,70,14000);
    const auto dualBoth=setter({{"mode",1},{"dualtype",0},{"dualblend",.5f},{"cab1",1},{"cab2",1},{"cablow1",70},{"cabhigh1",14000},{"cablow2",70},{"cabhigh2",14000}});
    auto bothOptions=rig1CutsOnlyOptions();bothOptions.laneCuts=true;
    const auto both=runDual(bothOptions,rig1,fizzy2,dualBoth);
    int rig1At=-1,lanesAt=-1;for(size_t i=0;i<both.status.steps.size();++i){if(both.status.steps[i].phase==P::rig1Cuts)rig1At=int(i);if(both.status.steps[i].phase==P::laneCuts)lanesAt=int(i);}
    check(rig1At>=0 && lanesAt>rig1At && both.laneCuts.lanes.size()==2 && value(both.values,"cabhigh1")<14000.f && near(value(both.values,"cabhigh1"),both.laneCuts.lanes[0].highAfter,1.f)
          && near(value(both.values,"cabhigh2"),both.laneCuts.lanes[1].highAfter,1.f),"Both steps together did not report rig 1 first or apply both analyses");
    // The lane cuts lock keeps rig 2 only; the Cabinet cuts lock keeps rig 1 only.
    auto laneLocked=bothOptions;laneLocked.locks|=Controller::lockLaneCuts;
    const auto onlyRig1=runDual(laneLocked,rig1,fizzy2,dualBoth);
    check(stepOf(onlyRig1.status,P::laneCuts) && stepOf(onlyRig1.status,P::laneCuts)->result=="kept (locked)" && stepOf(onlyRig1.status,P::rig1Cuts)->changed
          && near(value(onlyRig1.values,"cabhigh2"),14000.f,1e-3f) && value(onlyRig1.values,"cabhigh1")<14000.f,"The lane cuts lock did not keep rig 2 alone");
    auto rig1Locked=bothOptions;rig1Locked.locks|=Controller::lockCuts;
    const auto onlyRig2=runDual(rig1Locked,rig1,fizzy2,dualBoth);
    check(stepOf(onlyRig2.status,P::rig1Cuts) && stepOf(onlyRig2.status,P::rig1Cuts)->result=="kept (locked)" && near(value(onlyRig2.values,"cabhigh1"),14000.f,1e-3f)
          && near(value(onlyRig2.values,"cablow1"),70.f,1e-3f),"The Cabinet cuts lock did not keep rig 1 alone");
    // Matrix: the LOW lane booms with its LOW cut at 30 Hz; the cut rises.
    auto lanes=matrixLanes(48000,8,rng);lowShelf(lanes.low,100,12,48000);
    lanes.low=withCuts(lanes.low,48000,30,9000);lanes.mid=withCuts(lanes.mid,48000,70,9000);lanes.high=withCuts(lanes.high,48000,70,9000);
    const auto matrix=runMatrix(rig1CutsOnlyOptions(),lanes,setter({{"mode",2},{"cab1",1},{"cab2",1},{"cab3",1},{"cablow1",30},{"cabhigh1",9000},{"cablow2",70},{"cabhigh2",9000},{"cablow3",70},{"cabhigh3",9000}}));
    const auto* matrixStep=stepOf(matrix.status,P::rig1Cuts);
    check(matrix.status.passes==std::vector<P>{P::matrix} && matrixStep && matrixStep->changed && matrixStep->result.contains("LOW lane LOW 30 Hz > ") && value(matrix.values,"cablow1")>30.f
          && value(matrix.values,"cablow1")<=cutLowLimit(false,Style::rock)+1e-3f,"Matrix LOW lane cut not raised: "+(matrixStep?matrixStep->result.toStdString():std::string("no step")));
    std::cout<<"MEASURE auto set rig 1 cuts controller: "<<step->result<<" | with lane cuts: "<<both.status.steps[size_t(rig1At)].result<<" + "<<both.status.steps[size_t(lanesAt)].result<<" | Matrix: "<<matrixStep->result<<'\n';
    std::cout<<"PASS auto set rig 1 cuts controller: off by default; Dual rig 1 and Matrix LOW lane cuts; strength; Cabinet cuts lock; cabinet off; edited while listening; silent rig; Classic; with the lane cuts step, each lock alone; undo\n";
}
