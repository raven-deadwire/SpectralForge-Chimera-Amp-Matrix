#pragma once
// Phase 30: the Dual rigs' and Matrix lanes' modeled cabinet mics (docs/AUTO_SET_PHASE30.md).
// Included by AutoSetTests.cpp after the cabinet and lane IR tests (their helpers are used here).

// MSVC only: these functions are compiled without optimisation. With them optimised, the Windows
// compiler died while generating code for AutoSetTests.cpp (cl D8040, no diagnostic) from phase 29 on.
// The analysis they call lives in Source/ and stays optimised.
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("",off)
#endif
// A source through the modeled cabinet at a placement, as the engine plays it (the generated IR).
std::vector<float> throughModeledMic(const std::vector<float>& x,double rate,const spectralforge::cabLayout::Settings& s,CabPlacement p,float gainDb=0) {
    auto y=convolve(x,spectralforge::cabLayout::generate(spectralforge::cabLayout::key(withPlacement(s,p)),rate));
    const float g=juce::Decibels::decibelsToGain(gainDb);for(auto& v:y)v*=g;
    return y;
}
double lanePowerDb(const std::vector<float>& x){double p=0;for(const float v:x)p+=double(v)*v;return 10*std::log10(std::max(p,1e-30));}
void laneMicRules() {
    float worstPrediction=0,worstHold=0;
    for(const double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{300};
        const auto s=modeledMic(.5,10);const CabPlacement here{.5,10};
        const auto even=targetNoise(false,rate,6,rng),other=targetNoise(false,rate,6,rng);
        auto dark=targetNoise(false,rate,6,rng);shelve(dark,3000,-8,rate);
        auto bright=targetNoise(false,rate,6,rng);shelve(bright,3000,8,rate);
        const auto rig2=throughModeledMic(other,rate,s,here);
        // Dual parallel at 50%: rig 1 is dark through its mic; its mic moves toward the cap, and the mix comes closer.
        const auto darkRig=throughModeledMic(dark,rate,s,here);
        const auto r=analyseLaneMics({&darkRig,&rig2},{.5,.5},{{0,s}},rate,false);
        check(r.playingDetected && r.lanes.size()==1 && r.lanes[0].heard && r.lanes[0].changed() && r.lanes[0].proposed.position<.5
              && r.lanes[0].toneErrorAfterDb<r.lanes[0].toneErrorBeforeDb-cabToneMinGainDb,"A dark rig 1 did not move its mic toward the cap");
        check(std::abs(r.lanes[0].proposed.position-.5)<=cabPositionMaxMove+1e-6 && r.lanes[0].proposed.distanceCm>=5-1e-6 && r.lanes[0].proposed.distanceCm<=20+1e-6
              && std::abs(r.lanes[0].proposed.distanceCm*2-std::round(r.lanes[0].proposed.distanceCm*2))<1e-6 && cabPlacementSafe(s,r.lanes[0].proposed),"The mic left phase 9's candidates or limits");
        // The prediction against rig 1 rendered at the new placement, at the level-holding gain.
        const auto moved=throughModeledMic(dark,rate,s,r.lanes[0].proposed,r.lanes[0].holdGainDb);
        const float rendered=laneMixError({&moved,&rig2},{.5,.5},rate);
        worstPrediction=std::max(worstPrediction,std::abs(rendered-r.lanes[0].toneErrorAfterDb));
        worstHold=std::max(worstHold,float(std::abs(lanePowerDb(moved)-lanePowerDb(darkRig))));
        // A bright rig 1: the mix comes closer too, with the single rig's choice when rig 2 is silent in the mix.
        const auto brightRig=throughModeledMic(bright,rate,s,here);
        const auto darker=analyseLaneMics({&brightRig,&rig2},{.5,.5},{{0,s}},rate,false);
        check(darker.lanes[0].changed() && darker.lanes[0].toneErrorAfterDb<darker.lanes[0].toneErrorBeforeDb-cabToneMinGainDb && cabPlacementSafe(s,darker.lanes[0].proposed),"A bright rig 1 did not move its mic");
        // Alone in the mix (rig 2 at zero weight), the choice is phase 9's for that rig.
        const auto alone=analyseLaneMics({&brightRig,&rig2},{1.,0.},{{0,s}},rate,false);const auto classic=analyseCab(brightRig,rate,s,false);
        check(alone.lanes[0].proposed==classic.proposed,"A rig alone in the mix was not placed as phase 9 places it");
        const auto movedBright=throughModeledMic(bright,rate,s,darker.lanes[0].proposed,darker.lanes[0].holdGainDb);
        worstPrediction=std::max(worstPrediction,std::abs(laneMixError({&movedBright,&rig2},{.5,.5},rate)-darker.lanes[0].toneErrorAfterDb));
        const auto evenRig=throughModeledMic(even,rate,s,here);
        // The same source in both rigs (correlated lanes): a mic moved away arrives later, so the sum combs.
        // Fixed moves are predicted whatever the choice, also with rig 2 inverted.
        for(const CabPlacement probe:{CabPlacement{.3,20},CabPlacement{.8,5}})for(const double w2:{.5,-.35}) {
            const auto same2=throughModeledMic(dark,rate,s,here);
            const auto fixed=analyseLaneMics({&darkRig,&same2},{.5,w2},{{0,s}},rate,false,Style::rock,probe);
            const auto probed=throughModeledMic(dark,rate,s,probe,fixed.lanes[0].holdGainDb);
            worstPrediction=std::max(worstPrediction,std::abs(laneMixError({&probed,&same2},{.5,w2},rate)-fixed.lanes[0].toneErrorAfterDb));
        }
        // Both rigs dark and both mics free: rig 1 first, rig 2 judged from that mix.
        auto dark2=targetNoise(false,rate,6,rng);shelve(dark2,3000,-8,rate);
        const auto darkRig2=throughModeledMic(dark2,rate,s,here);
        const auto both=analyseLaneMics({&darkRig,&darkRig2},{.5,.5},{{0,s},{1,s}},rate,false);
        check(both.lanes.size()==2 && both.lanes[0].changed() && both.lanes[1].toneErrorBeforeDb==both.lanes[0].toneErrorAfterDb,"Rig 2 was not judged from rig 1's chosen placement");
        const auto both1=throughModeledMic(dark,rate,s,both.lanes[0].proposed,both.lanes[0].holdGainDb),both2=throughModeledMic(dark2,rate,s,both.lanes[1].proposed,both.lanes[1].holdGainDb);
        worstPrediction=std::max(worstPrediction,std::abs(laneMixError({&both1,&both2},{.5,.5},rate)-both.lanes[1].toneErrorAfterDb));
        // Matrix: three lanes, the HIGH lane's mic alone may move.
        const auto lane3=throughModeledMic(dark2,rate,s,here);
        const auto matrix=analyseLaneMics({&evenRig,&rig2,&lane3},{1,1,1},{{2,s}},rate,false,Style::rock,CabPlacement{.2,6});
        const auto moved3=throughModeledMic(dark2,rate,s,{.2,6},matrix.lanes[0].holdGainDb);
        worstPrediction=std::max(worstPrediction,std::abs(laneMixError({&evenRig,&rig2,&moved3},{1,1,1},rate)-matrix.lanes[0].toneErrorAfterDb));
        // Kept: a silent lane; silence is not playing.
        const std::vector<float> quiet(darkRig.size(),0.f);
        const auto silent=analyseLaneMics({&quiet,&rig2},{.5,.5},{{0,s}},rate,false);
        check(silent.playingDetected && silent.lanes.size()==1 && !silent.lanes[0].heard && !silent.changed(),"A silent rig was judged");
        check(!analyseLaneMics({&quiet,&quiet},{.5,.5},{{0,s}},rate).playingDetected,"Silence reported as playing");
        if(rate==48000.)std::cout<<"MEASURE lane mic rules 48 kHz: dark rig 1 > position "<<r.lanes[0].proposed.position<<", "<<r.lanes[0].proposed.distanceCm<<" cm, hold "<<r.lanes[0].holdGainDb<<" dB; balance "
                                <<r.lanes[0].toneErrorBeforeDb<<" > "<<r.lanes[0].toneErrorAfterDb<<" dB predicted, "<<rendered<<" rendered; bright rig 1 > position "<<darker.lanes[0].proposed.position<<", "
                                <<darker.lanes[0].proposed.distanceCm<<" cm ("<<darker.lanes[0].toneErrorBeforeDb<<" > "<<darker.lanes[0].toneErrorAfterDb<<" dB)\n";
    }
    std::cout<<"MEASURE lane mic prediction vs rendered mix: worst "<<worstPrediction<<" dB; level hold worst "<<worstHold<<" dB\n";
    check(worstPrediction<.5f,"The lane mic prediction misses the rendered mix by "+std::to_string(worstPrediction)+" dB");
    check(worstHold<.5f,"The level-holding gain misses the rendered lane by "+std::to_string(worstHold)+" dB");
    std::cout<<"PASS auto set lane mic rules at 44.1/48/96 kHz: dark and bright rig, a rig alone as phase 9, limits, correlated and inverted lanes, lanes in order, Matrix lane, silent, silence, prediction vs render, level hold\n";
}
// The model a lane's mic A plays in a processor prepared this way.
spectralforge::cabLayout::Settings laneMicSettings(const std::function<void(ChimeraProcessor&)>& prepare,int lane) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    prepare(*p);
    const auto key=spectralforge::OriginalCabParameters::savedKey(lane,0,[&](const juce::String& id){return p->parameters().getRawParameterValue(id)->load();});
    p->releaseResources();
    check(key!=0,"The lane's mic A is not a modeled mic");
    return spectralforge::cabLayout::settings(key);
}
// The real processor's Dual rigs (the default heads), both through modeled mic A, rig 1's at a placement and level.
std::pair<std::vector<float>,std::vector<float>> laneMicProcessorRigs(const std::vector<float>& x,CabPlacement mic1,float gain1) {
    auto q=std::make_unique<ChimeraProcessor>();q->setRateAndBufferSizeDetails(48000,256);
    const auto setAll=[&]{
        for(const auto& [id,v]:std::initializer_list<std::pair<juce::String,float>>{{"mode",1},{"dualtype",0},{"dualblend",.5f},{"cab1",1},{"cab2",1},{"cabblend1",0},{"cabblend2",0},
                {"ocab1_Aon",1},{"ocab2_Aon",1},{"ocab1_Aposition",float(mic1.position)},{"ocab1_Adistance",float(mic1.distanceCm)},{"cabAgain1",gain1},{"ocab2_Aposition",.5f},{"ocab2_Adistance",10}})
            q->parameters().getParameter(id)->setValueNotifyingHost(q->parameters().getParameter(id)->convertTo0to1(v));
    };
    setAll();q->prepareToPlay(48000,256);setAll();
    juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
    size_t offset=0;
    // Two seconds first: the cabinets generate their models off the audio thread.
    for(;offset<375*256;offset+=256){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+offset,256);q->processBlock(io,midi);if(offset%(64*256)==0)juce::Thread::sleep(20);}
    q->autoSetCapture().listen({Tap::lane1,Tap::lane2});
    for(;offset+256<=x.size();offset+=256){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+offset,256);q->processBlock(io,midi);}
    std::pair<std::vector<float>,std::vector<float>> rigs;q->autoSetCapture().drainSamples(rigs.first);q->autoSetCapture().drainAmpSamples(rigs.second);
    q->releaseResources();
    const size_t n=std::min(rigs.first.size(),rigs.second.size());rigs.first.resize(n);rigs.second.resize(n);
    return rigs;
}
Controller::Options laneMicOnlyOptions() {auto o=laneCutsOnlyOptions();o.laneCuts=false;o.laneMic=true;return o;}
void controllerLaneMic() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto setter=[](std::initializer_list<std::pair<juce::String,float>> pairs){
        std::vector<std::pair<juce::String,float>> list(pairs);
        return [list](ChimeraProcessor& p){for(const auto& [id,v]:list){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));}};
    };
    check(!Controller::Options{}.laneMic && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.laneMic,"Lane cabinet mics are on by default");
    // Dual, mic A of both rigs modeled at position 0.5, 10 cm; rig 1's level at -2 dB.
    const auto dual=setter({{"mode",1},{"dualtype",0},{"dualblend",.5f},{"cab1",1},{"cab2",1},{"cabblend1",0},{"cabblend2",0},{"ocab1_Aon",1},{"ocab2_Aon",1},
                            {"ocab1_Aposition",.5f},{"ocab1_Adistance",10},{"ocab2_Aposition",.5f},{"ocab2_Adistance",10},{"cabAgain1",-2},{"cabAgain2",0}});
    const auto with=[&](std::initializer_list<std::pair<juce::String,float>> extra){const auto more=setter(extra);return [dual,more](ChimeraProcessor& p){dual(p);more(p);};};
    const auto model1=laneMicSettings(dual,0),model2=laneMicSettings(dual,1);
    std::mt19937 rng{301};
    auto dark=targetNoise(false,48000,8,rng);shelve(dark,3000,-8,48000);
    const auto even=targetNoise(false,48000,8,rng);
    const CabPlacement here{.5,10};
    const auto rig1=throughModeledMic(dark,48000,model1,here),rig2=throughModeledMic(even,48000,model2,here);
    // The step runs alone in the Dual pass, moves rig 1's mic and holds its level.
    const auto full=runDual(laneMicOnlyOptions(),rig1,rig2,dual);
    const auto* step=stepOf(full.status,P::laneMic);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::dual} && step && step->changed && step->result.contains("rig 1 mic A position 0.50 > ") && step->result.contains("level -2.0 > "),
          "Lane mic step did not run alone in the Dual pass or report: "+(step?step->result.toStdString():std::string("no step")));
    const auto& a=full.laneMic;
    check(a.lanes.size()==2 && a.lanes[0].changed() && near(value(full.values,"ocab1_Aposition"),float(a.lanes[0].proposed.position),.0011f) && near(value(full.values,"ocab1_Adistance"),float(a.lanes[0].proposed.distanceCm),.051f)
          && near(value(full.values,"ocab2_Aposition"),float(a.lanes[1].proposed.position),.0011f) && near(value(full.values,"ocab2_Adistance"),float(a.lanes[1].proposed.distanceCm),.051f),
          "Applied placements do not match the analysis: "+step->result.toStdString());
    const float level=cabLevelChangeDb(measureCabEnergy(rig1,48000),model1,{value(full.values,"ocab1_Aposition"),value(full.values,"ocab1_Adistance")});
    check(near(value(full.values,"cabAgain1"),std::round((-2.f-level)*10.f)/10.f,.011f) && std::abs(-level-a.lanes[0].holdGainDb)<.5f,"The mic's level does not hold the lane's loudness ("+std::to_string(-level)+" dB applied, "+std::to_string(a.lanes[0].holdGainDb)+" dB modelled)");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the lane mic placement and level");
    // Strength moves the mic part of the way (when that placement is safe).
    auto halfOptions=laneMicOnlyOptions();halfOptions.strength=.5f;
    const auto half=runDual(halfOptions,rig1,rig2,dual);
    const auto* halfStep=stepOf(half.status,P::laneMic);
    check(halfStep && (halfStep->changed ? near(value(half.values,"ocab1_Aposition"),float(.5+.5*(a.lanes[0].proposed.position-.5)),.0011f) : halfStep->result.contains("the Strength placement adds a peak or notch")),
          "Strength did not scale the mic move: "+(halfStep?halfStep->result.toStdString():std::string("no step")));
    // Kept: the Cabinet mic lock, and each rig that cannot be judged, with its reason.
    auto lockedOptions=laneMicOnlyOptions();lockedOptions.locks=Controller::lockCompressor|Controller::lockCab;
    const auto locked=runDual(lockedOptions,rig1,rig2,dual);
    check(stepOf(locked.status,P::laneMic) && stepOf(locked.status,P::laneMic)->result=="kept (locked)" && locked.status.passes==std::vector<P>{P::input} && near(value(locked.values,"ocab1_Aposition"),.5f,1e-4f),"The Cabinet mic lock did not keep the lane mics or still listened");
    const auto reason=[&](std::initializer_list<std::pair<juce::String,float>> extra,const char* text) {
        const auto run=runDual(laneMicOnlyOptions(),rig1,rig2,with(extra));
        const auto* s=stepOf(run.status,P::laneMic);
        check(s && s->result.contains(text) && near(value(run.values,"ocab1_Aposition"),.5f,1e-4f) && near(value(run.values,"cabAgain1"),-2.f,1e-4f),std::string("Not kept as '")+text+"': "+(s?s->result.toStdString():std::string("no step")));
    };
    reason({{"cab1",0}},"rig 1 cabinet off");
    reason({{"cabblend1",.5f},{"ocab1_Bon",1}},"rig 1 blends two mics");
    reason({{"ocab1_Aon",0},{"cabtype1",1}},"rig 1 plays a captured IR or filters");
    const std::vector<float> quiet(rig1.size(),0.f);
    check(stepOf(runDual(laneMicOnlyOptions(),quiet,rig2,dual).status,P::laneMic)->result.contains("rig 1 silent"),"A silent rig 1 was not reported");
    const auto edited=runDual(laneMicOnlyOptions(),rig1,rig2,dual,setter({{"ocab1_Adistance",12}}));
    check(stepOf(edited.status,P::laneMic)->result=="kept (a cabinet or mic changed while listening)" && near(value(edited.values,"ocab1_Adistance"),12.f,.051f) && near(value(edited.values,"ocab1_Aposition"),.5f,1e-4f),"A mic edited while listening was overwritten");
    const auto levelEdited=runDual(laneMicOnlyOptions(),rig1,rig2,dual,setter({{"cabAgain1",-5}}));
    check(stepOf(levelEdited.status,P::laneMic)->result=="kept (a cabinet or mic changed while listening)" && near(value(levelEdited.values,"cabAgain1"),-5.f,1e-3f),"A mic level edited while listening was overwritten");
    // Classic routing: no lane pass; the Cabinet mic placement step covers rig 1.
    auto classicOptions=laneMicOnlyOptions();classicOptions.locks=Controller::lockCompressor;
    const auto classic=runSpace(classicOptions);
    check(stepOf(classic.status,P::laneMic) && stepOf(classic.status,P::laneMic)->result=="kept (Classic routing: the Cabinet mic placement step places rig 1's mic)","Classic routing not reported");
    // With the IR and cut steps: a moved mic makes the captures stale, so those wait for the next run.
    auto allOptions=laneMicOnlyOptions();allOptions.laneIR=allOptions.laneCuts=allOptions.rig1Cuts=true;
    const auto stale=runDual(allOptions,rig1,rig2,dual);
    int micAt=-1,irAt=-1,rig1At=-1,lanesAt=-1;
    for(size_t i=0;i<stale.status.steps.size();++i){const auto ph=stale.status.steps[i].phase;if(ph==P::laneMic)micAt=int(i);if(ph==P::laneIR)irAt=int(i);if(ph==P::rig1Cuts)rig1At=int(i);if(ph==P::laneCuts)lanesAt=int(i);}
    check(micAt>=0 && irAt>micAt && rig1At>irAt && lanesAt>rig1At && stale.status.steps[size_t(micAt)].changed
          && stale.status.steps[size_t(irAt)].result=="kept (a cabinet mic was moved in this run: run Auto Set again to judge the IRs)"
          && stale.status.steps[size_t(rig1At)].result=="kept (a cabinet mic was moved in this run: run Auto Set again to judge the cuts)"
          && stale.status.steps[size_t(lanesAt)].result==stale.status.steps[size_t(rig1At)].result,"The IR and cut steps did not wait after a mic move, or the rows are out of order");
    // Matrix: the HIGH lane is dark through its modeled mic; the LOW and MID lanes play factory IRs.
    const auto matrixSetup=setter({{"mode",2},{"cab1",1},{"cab2",1},{"cab3",1},{"cabblend1",0},{"cabblend2",0},{"cabblend3",0},{"ocab1_Aon",0},{"ocab2_Aon",0},{"ocab3_Aon",1},
                                   {"cabtype1",1},{"cabtype2",1},{"ocab3_Aposition",.5f},{"ocab3_Adistance",10}});
    const auto model3=laneMicSettings(matrixSetup,2);
    MatrixLanes lanes;lanes.low=rig2;lanes.mid=throughModeledMic(targetNoise(false,48000,8,rng),48000,model2,here);lanes.high=throughModeledMic(dark,48000,model3,here);
    // The LOW and MID lanes sit 14 dB down, so the mix's balance is the HIGH lane's.
    for(auto* lane:{&lanes.low,&lanes.mid})for(auto& v:*lane)v*=.2f;
    const auto matrix=runMatrix(laneMicOnlyOptions(),lanes,matrixSetup);
    const auto* matrixStep=stepOf(matrix.status,P::laneMic);
    check(matrix.status.passes==std::vector<P>{P::matrix} && matrixStep && matrixStep->changed && matrixStep->result.contains("HIGH lane mic A position 0.50 > ") && matrixStep->result.contains("LOW lane plays a captured IR or filters") && !near(value(matrix.values,"ocab3_Aposition"),.5f,1e-3f),
          "Matrix HIGH lane mic not moved: "+(matrixStep?matrixStep->result.toStdString():std::string("no step")));
    // Real heads (measured): notes through the processor's default Dual heads, both rigs through modeled mics;
    // a fixed move of rig 1's mic is predicted from the captures and re-rendered by the engine at the level-holding gain.
    const auto notes=pluckedEvery(250,8,48000,6);
    std::vector<float> dry(notes.size());for(size_t n=0;n<dry.size();++n)dry[n]=notes[n]+.2f*even[n%even.size()];
    const auto heads=laneMicProcessorRigs(dry,here,0);
    const CabPlacement probe{.2,16};
    const auto real=analyseLaneMics({&heads.first,&heads.second},{.5,.5},{{0,model1},{1,model2}},48000,false);
    const auto predicted=analyseLaneMics({&heads.first,&heads.second},{.5,.5},{{0,model1}},48000,false,Style::rock,probe);
    check(real.playingDetected && predicted.lanes.size()==1 && predicted.lanes[0].heard,"Real-head rigs not heard");
    const float hold=cabLevelChangeDb(measureCabEnergy(heads.first,48000),model1,probe);
    const auto again=laneMicProcessorRigs(dry,probe,-hold);
    const float renderedError=laneMixError({&again.first,&again.second},{.5,.5},48000),miss=renderedError-predicted.lanes[0].toneErrorAfterDb;
    check(std::abs(miss)<.5f,"Real-head lane mic prediction off by "+std::to_string(miss)+" dB");
    check(std::abs(lanePowerDb(again.first)-lanePowerDb(heads.first))<.5,"The engine's rig 1 level was not held: "+std::to_string(lanePowerDb(again.first)-lanePowerDb(heads.first))+" dB");
    std::cout<<"MEASURE lane mic real heads (Dual 50%, modeled mics at 0.5 / 10 cm): step "<<(real.changed()?"moves a mic":"keeps")<<" (balance "<<real.lanes[0].toneErrorBeforeDb<<" dB); rig 1 mic to 0.2 / 16 cm: balance "
             <<predicted.lanes[0].toneErrorAfterDb<<" dB predicted, "<<renderedError<<" dB re-rendered; hold "<<predicted.lanes[0].holdGainDb<<" dB modelled, "<<-hold<<" dB applied, rig 1 level change "
             <<lanePowerDb(again.first)-lanePowerDb(heads.first)<<" dB\n";
    std::cout<<"MEASURE auto set lane mic controller: "<<step->result<<" | 50%: "<<halfStep->result<<" | Matrix: "<<matrixStep->result<<'\n';
    std::cout<<"PASS auto set lane mic controller: off by default; Dual rig 1 and Matrix HIGH lane mics; level held; strength; Cabinet mic lock; cabinet off, blended, not modeled, silent, edited, Classic; IR and cut steps wait after a move; undo; real heads\n";
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("",on)
#endif
