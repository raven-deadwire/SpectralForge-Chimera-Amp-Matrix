#pragma once
// Phase 29: the Dual rigs' and Matrix lanes' factory cabinet IRs (docs/AUTO_SET_PHASE29.md).
// Included by AutoSetTests.cpp after the cabinet IR and lane cuts tests (their helpers are used here).

std::array<std::vector<float>,2> laneIRFactory(double rate) {
    std::array<std::vector<float>,2> loaded;
    for(int i=0;i<2;++i){const auto ir=spectralforge::IRLibrary::factoryIR(i);check(ir!=nullptr,"Factory IR missing");loaded[size_t(i)]=laneIRLoaded(ir->samples,ir->rate,rate);}
    return loaded;
}
float laneMixError(const std::vector<const std::vector<float>*>& lanes,const std::vector<double>& weights,double rate,Style style=Style::rock) {
    return ampToneErrorDb(measureSpectrum(laneMix(lanes,weights),rate,false),style);
}
void laneIRRules() {
    float worstPrediction=0,worstLevel=0;
    for(const double rate:{44100.,48000.,96000.}) {
        const auto factory=laneIRFactory(rate);
        std::mt19937 rngA{290},rngB{291};
        // Two rigs with amp sounds that suit the Jensen; rig 1 plays the V30, rig 2 the Jensen.
        const auto amp1=irSuitedNoise(rate,6,rngA,1,Style::rock),amp2=irSuitedNoise(rate,6,rngB,1,Style::rock);
        const auto v1=renderFactoryIR(amp1,rate,0),j1=renderFactoryIR(amp1,rate,1),j2=renderFactoryIR(amp2,rate,1),v2=renderFactoryIR(amp2,rate,0);
        const auto r=analyseLaneIR({&v1,&j2},{.5,.5},{{0,1},{1,2}},rate,factory,false);
        check(r.playingDetected && !r.bass && r.lanes.size()==2 && r.lanes[0].heard && r.lanes[0].changed() && r.lanes[0].proposed==2 && !r.lanes[1].changed()
              && r.lanes[0].toneErrorAfterDb<r.lanes[0].toneErrorBeforeDb-cabIRMinGainDb && r.lanes[1].toneErrorBeforeDb==r.lanes[0].toneErrorAfterDb,
              "Rig 1 was not switched to the suited IR, or rig 2 was switched away ("+std::to_string(r.lanes[0].toneErrorBeforeDb)+" > "+std::to_string(r.lanes[0].toneErrorAfterDb)+" dB)");
        // The prediction against the engine's own convolution of the other IR (juce::dsp::Convolution, resampled).
        const float rendered=laneMixError({&j1,&j2},{.5,.5},rate);
        worstPrediction=std::max(worstPrediction,std::abs(rendered-r.lanes[0].toneErrorAfterDb));
        const auto power=[&](const std::vector<float>& x){const auto s=measureSpectrum(x,rate,false);double p=0;for(int b=0;b<toneBandCount;++b)p+=std::pow(10.,s.bandsDb[size_t(b)]/10.);return 10*std::log10(p);};
        worstLevel=std::max(worstLevel,float(std::abs(power(laneMix({&j1,&j2},{.5,.5}))-power(laneMix({&v1,&j2},{.5,.5}))-r.lanes[0].levelChangeDb)));
        // Both rigs on the suited IR: nothing moves.
        check(!analyseLaneIR({&j1,&j2},{.5,.5},{{0,2},{1,2}},rate,factory,false).changed(),"A suited IR was switched away");
        // Both rigs on the V30: rig 1 is switched first, rig 2 is then judged from that mix.
        const auto both=analyseLaneIR({&v1,&v2},{.5,.5},{{0,1},{1,1}},rate,factory,false);
        check(both.lanes.size()==2 && both.lanes[0].changed() && both.lanes[1].toneErrorBeforeDb==both.lanes[0].toneErrorAfterDb,"Rig 2 was not judged from rig 1's chosen IR");
        if(both.lanes[1].changed())worstPrediction=std::max(worstPrediction,std::abs(laneMixError({&j1,&j2},{.5,.5},rate)-both.lanes[1].toneErrorAfterDb));
        // The same amp sound in both rigs (correlated lanes): the IRs' delays and phases decide the sum.
        // Rig 1's switch is predicted whatever the choice, also with rig 2 inverted.
        for(const double w2:{.5,-.5}) {
            const auto same=analyseLaneIR({&v1,&j1},{.5,w2},{{0,1}},rate,factory,false,Style::rock,true);
            const float actual=laneMixError({&j1,&j1},{.5,w2},rate);
            // Both rigs then play the same signal: inverted, they cancel to silence, which has no balance.
            if(w2>0)worstPrediction=std::max(worstPrediction,std::abs(actual-same.lanes[0].toneErrorAfterDb));
            const auto back=analyseLaneIR({&j1,&v1},{.5,w2},{{0,2}},rate,factory,false,Style::rock,true);
            worstPrediction=std::max(worstPrediction,std::abs(laneMixError({&v1,&v1},{.5,w2},rate)-back.lanes[0].toneErrorAfterDb)*(w2>0?1.f:0.f));
            const auto other=analyseLaneIR({&v1,&j2},{.5,w2},{{0,1}},rate,factory,false,Style::rock,true);
            worstPrediction=std::max(worstPrediction,std::abs(laneMixError({&j1,&j2},{.5,w2},rate)-other.lanes[0].toneErrorAfterDb));
        }
        // Matrix: three lanes, the HIGH lane on the V30.
        std::mt19937 rngC{292};
        const auto amp3=irSuitedNoise(rate,6,rngC,1,Style::rock);
        const auto v3=renderFactoryIR(amp3,rate,0),j3=renderFactoryIR(amp3,rate,1);
        const auto matrix=analyseLaneIR({&j1,&j2,&v3},{1,1,1},{{0,2},{1,2},{2,1}},rate,factory,false);
        check(matrix.lanes.size()==3 && !matrix.lanes[0].changed() && !matrix.lanes[1].changed() && matrix.lanes[2].changed() && matrix.lanes[2].proposed==2,"The Matrix HIGH lane was not switched alone");
        worstPrediction=std::max(worstPrediction,std::abs(laneMixError({&j1,&j2,&j3},{1,1,1},rate)-matrix.lanes[2].toneErrorAfterDb));
        // Kept: a silent lane, bass, silence, a lane that is no factory source.
        const std::vector<float> quiet(v1.size(),0.f);
        const auto silent=analyseLaneIR({&quiet,&j2},{.5,.5},{{0,1}},rate,factory,false);
        check(silent.playingDetected && silent.lanes.size()==1 && !silent.lanes[0].heard && !silent.changed(),"A silent rig was judged");
        check(!analyseLaneIR({&v1,&j2},{.5,.5},{{0,1},{1,2}},rate,factory,true).changed(),"Bass switched a guitar IR");
        check(!analyseLaneIR({&quiet,&quiet},{.5,.5},{{0,1}},rate,factory).playingDetected,"Silence reported as playing");
        check(analyseLaneIR({&v1,&j2},{.5,.5},{{0,3},{1,0}},rate,factory,false).lanes.empty(),"A non-factory source was judged");
        // The low end: the suited IR with its low end cut away (a 110 Hz low cut) still balances closer
        // from 80 Hz, but would take the heard 44-71 Hz down and leave it under the target: kept.
        auto cutFactory=factory;cutFactory[1]=withCuts(factory[1],rate,110,20000);
        // Rig 2 is low in the mix here, so the mix's low end is rig 1's.
        const auto held=analyseLaneIR({&v1,&j2},{.5,.05},{{0,1}},rate,cutFactory,false);
        check(held.lanes.size()==1 && !held.lanes[0].changed() && held.lanes[0].lowEndHeld && held.lanes[0].lowEndChangeDb<-cabIRMaxLowLossDb && held.lanes[0].lowEndAfterDb<-cabIRMaxLowLossDb,
              "A switch that cuts the low strings' fundamentals was made ("+std::to_string(held.lanes[0].lowEndChangeDb)+" dB, "+std::to_string(held.lanes[0].lowEndAfterDb)+" dB under, held "+std::to_string(held.lanes[0].lowEndHeld)+")");
        if(rate==48000.)std::cout<<"MEASURE lane IR rules 48 kHz: rig 1 V30 > Jensen, balance "<<r.lanes[0].toneErrorBeforeDb<<" > "<<r.lanes[0].toneErrorAfterDb<<" dB predicted, "<<rendered<<" rendered; level "<<r.lanes[0].levelChangeDb
                                <<" dB; low end held at "<<held.lanes[0].lowEndChangeDb<<" dB, "<<held.lanes[0].lowEndAfterDb<<" dB under the target\n";
    }
    check(worstPrediction<.5f,"The lane IR prediction misses the rendered mix by "+std::to_string(worstPrediction)+" dB");
    check(worstLevel<.3f,"The lane IR level change is off by "+std::to_string(worstLevel)+" dB");
    std::cout<<"MEASURE lane IR prediction vs rendered mix: worst "<<worstPrediction<<" dB, worst level "<<worstLevel<<" dB\n";
    std::cout<<"PASS auto set lane IR rules at 44.1/48/96 kHz: suited IR chosen and kept, lanes in order, correlated and inverted lanes, Matrix HIGH lane, silent, bass, silence, other sources, low end held, prediction vs render\n";
}
// The real processor's Dual rigs (the default heads) with the given cabinet sources for mic A of rig 1 and rig 2.
std::pair<std::vector<float>,std::vector<float>> laneIRProcessorRigs(const std::vector<float>& x,int source1,int source2) {
    auto q=std::make_unique<ChimeraProcessor>();q->setRateAndBufferSizeDetails(48000,256);
    const auto setAll=[&]{
        for(const auto& [id,v]:std::initializer_list<std::pair<juce::String,float>>{{"mode",1},{"dualtype",0},{"dualblend",.5f},{"cab1",1},{"cab2",1},{"cabblend1",0},{"cabblend2",0},
                {spectralforge::originalCabID(0,"Aon"),0},{spectralforge::originalCabID(1,"Aon"),0},{"cabtype1",float(source1)},{"cabtype2",float(source2)}})
            q->parameters().getParameter(id)->setValueNotifyingHost(q->parameters().getParameter(id)->convertTo0to1(v));
    };
    setAll();q->prepareToPlay(48000,256);setAll();
    juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
    size_t offset=0;
    // Two seconds first: the cabinets load their IRs off the audio thread.
    for(;offset<375*256;offset+=256){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+offset,256);q->processBlock(io,midi);if(offset%(64*256)==0)juce::Thread::sleep(20);}
    q->autoSetCapture().listen({Tap::lane1,Tap::lane2});
    for(;offset+256<=x.size();offset+=256){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+offset,256);q->processBlock(io,midi);}
    std::pair<std::vector<float>,std::vector<float>> rigs;q->autoSetCapture().drainSamples(rigs.first);q->autoSetCapture().drainAmpSamples(rigs.second);
    q->releaseResources();
    const size_t n=std::min(rigs.first.size(),rigs.second.size());rigs.first.resize(n);rigs.second.resize(n);
    return rigs;
}
Controller::Options laneIROnlyOptions() {auto o=laneCutsOnlyOptions();o.laneCuts=false;o.laneIR=true;return o;}
void controllerLaneIR() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto source=[&](const std::vector<float>& v,const juce::String& id){return juce::roundToInt(value(v,id));};
    const auto setter=[](std::initializer_list<std::pair<juce::String,float>> pairs){
        std::vector<std::pair<juce::String,float>> list(pairs);
        return [list](ChimeraProcessor& p){for(const auto& [id,v]:list){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));}};
    };
    check(!Controller::Options{}.laneIR && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.laneIR,"Lane cabinet IRs are on by default");
    std::mt19937 rngA{293},rngB{294},rngC{295};
    const auto amp1=irSuitedNoise(48000,8,rngA,1,Style::rock),amp2=irSuitedNoise(48000,8,rngB,1,Style::rock),amp3=irSuitedNoise(48000,8,rngC,1,Style::rock);
    const auto v1=renderFactoryIR(amp1,48000,0),j1=renderFactoryIR(amp1,48000,1),j2=renderFactoryIR(amp2,48000,1),j3=renderFactoryIR(amp3,48000,1);
    const auto aOn0=spectralforge::originalCabID(0,"Aon"),aOn1=spectralforge::originalCabID(1,"Aon"),aOn2=spectralforge::originalCabID(2,"Aon"),bOn0=spectralforge::originalCabID(0,"Bon");
    // Dual, mic A of both rigs a factory IR: rig 1 the V30 (its playing suits the Jensen), rig 2 the Jensen.
    const auto dual=setter({{"mode",1},{"dualtype",0},{"dualblend",.5f},{"cab1",1},{"cab2",1},{"cabblend1",0},{"cabblend2",0},{aOn0,0},{aOn1,0},{"cabtype1",1},{"cabtype2",2}});
    const auto with=[&](std::initializer_list<std::pair<juce::String,float>> extra){const auto more=setter(extra);return [dual,more](ChimeraProcessor& p){dual(p);more(p);};};
    const auto full=runDual(laneIROnlyOptions(),v1,j2,dual);
    const auto* step=stepOf(full.status,P::laneIR);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::dual} && step && step->changed && step->result.contains("rig 1 mic A IR V30 / SM57 > Jensen / SM57")
          && step->result.contains("rig 2 already closest with Jensen / SM57") && step->result.contains("level"),"Lane IR step did not run alone in the Dual pass or report: "+(step?step->result.toStdString():std::string("no step")));
    check(source(full.before,"cabtype1")==1 && source(full.values,"cabtype1")==2 && source(full.values,"cabtype2")==2 && full.laneIR.lanes.size()==2 && full.laneIR.lanes[0].proposed==2,"cabtype1 is not the Jensen, or rig 2 moved");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the lane IR");
    // Strength is not applied: an IR is switched or kept.
    auto quarter=laneIROnlyOptions();quarter.strength=.25f;
    check(source(runDual(quarter,v1,j2,dual).values,"cabtype1")==2,"Strength changed the IR choice");
    // Both rigs suited: kept.
    const auto suited=runDual(laneIROnlyOptions(),j1,j2,with({{"cabtype1",2}}));
    check(stepOf(suited.status,P::laneIR) && !stepOf(suited.status,P::laneIR)->changed && stepOf(suited.status,P::laneIR)->result.contains("rig 1 already closest with Jensen / SM57") && source(suited.values,"cabtype1")==2,"Suited IRs were not kept");
    // Mic B heard (the blend fully on B): its source is switched.
    const auto micB=runDual(laneIROnlyOptions(),v1,j2,with({{"cabblend1",1},{bOn0,0},{"cabBtype1",1},{"cabtype1",2}}));
    check(stepOf(micB.status,P::laneIR)->result.contains("rig 1 mic B IR V30 / SM57 > Jensen / SM57") && source(micB.values,"cabBtype1")==2 && source(micB.values,"cabtype1")==2,"Mic B's source was not switched: "+stepOf(micB.status,P::laneIR)->result.toStdString());
    // Kept: the Cabinet IR lock, and each rig that cannot be judged, with its reason.
    auto lockedOptions=laneIROnlyOptions();lockedOptions.locks=Controller::lockCompressor|Controller::lockCabIR;
    const auto locked=runDual(lockedOptions,v1,j2,dual);
    check(stepOf(locked.status,P::laneIR) && stepOf(locked.status,P::laneIR)->result=="kept (locked)" && locked.status.passes==std::vector<P>{P::input} && source(locked.values,"cabtype1")==1,"The Cabinet IR lock did not keep the lane IRs or still listened");
    const auto reason=[&](std::initializer_list<std::pair<juce::String,float>> extra,const char* text) {
        const auto run=runDual(laneIROnlyOptions(),v1,j2,with(extra));
        const auto* s=stepOf(run.status,P::laneIR);
        check(s && !s->changed && s->result.contains(text) && source(run.values,"cabtype1")==source(run.before,"cabtype1"),std::string("Not kept as '")+text+"': "+(s?s->result.toStdString():std::string("no step")));
    };
    reason({{"cab1",0}},"rig 1 cabinet off");
    reason({{"cabblend1",.5f},{bOn0,0},{"cabBtype1",1}},"rig 1 blends two mics");
    reason({{aOn0,1}},"rig 1 plays a modeled mic");
    reason({{"cabtype1",0}},"rig 1 is filters only");
    reason({{"cabtype1",3}},"rig 1 plays a user IR");
    const std::vector<float> quiet(v1.size(),0.f);
    check(stepOf(runDual(laneIROnlyOptions(),quiet,j2,dual).status,P::laneIR)->result.contains("rig 1 silent"),"A silent rig 1 was not reported");
    const auto edited=runDual(laneIROnlyOptions(),v1,j2,dual,setter({{"cabtype2",1}}));
    check(stepOf(edited.status,P::laneIR)->result=="kept (a cabinet, mic or IR changed while listening)" && source(edited.values,"cabtype1")==1 && source(edited.values,"cabtype2")==1,"A source edited while listening was overwritten");
    auto bassOptions=laneIROnlyOptions();bassOptions.instrument=2;
    check(stepOf(runDual(bassOptions,v1,j2,dual).status,P::laneIR)->result=="kept (bass: both factory IRs are guitar cabinets)","Bass switched a guitar IR");
    // Classic routing: no lane pass; the Cabinet factory IR choice step covers rig 1.
    auto classicOptions=laneIROnlyOptions();classicOptions.locks=Controller::lockCompressor;
    const auto classic=runSpace(classicOptions);
    check(stepOf(classic.status,P::laneIR) && stepOf(classic.status,P::laneIR)->result=="kept (Classic routing: the Cabinet factory IR choice step sets rig 1)","Classic routing not reported");
    // With the cut steps: a switched IR makes the captures stale, so the cuts wait for the next run; without a switch they are judged.
    auto withCutsOptions=laneIROnlyOptions();withCutsOptions.laneCuts=withCutsOptions.rig1Cuts=true;
    const auto stale=runDual(withCutsOptions,v1,j2,dual);
    const juce::String wait="kept (a cabinet IR was switched in this run: run Auto Set again to judge the cuts)";
    int irAt=-1,rig1At=-1,lanesAt=-1;
    for(size_t i=0;i<stale.status.steps.size();++i){const auto ph=stale.status.steps[i].phase;if(ph==P::laneIR)irAt=int(i);if(ph==P::rig1Cuts)rig1At=int(i);if(ph==P::laneCuts)lanesAt=int(i);}
    check(irAt>=0 && rig1At>irAt && lanesAt>rig1At && stale.status.steps[size_t(irAt)].changed && stale.status.steps[size_t(rig1At)].result==wait && stale.status.steps[size_t(lanesAt)].result==wait
          && near(value(stale.values,"cabhigh1"),value(stale.before,"cabhigh1"),1e-3f),"The cuts did not wait after an IR switch, or the rows are out of order");
    const auto fresh=runDual(withCutsOptions,j1,j2,with({{"cabtype1",2}}));
    check(stepOf(fresh.status,P::rig1Cuts) && stepOf(fresh.status,P::rig1Cuts)->result!=wait && stepOf(fresh.status,P::laneCuts)->result!=wait,"The cuts waited without an IR switch");
    // Matrix: the LOW lane plays the V30.
    MatrixLanes lanes;lanes.low=v1;lanes.mid=j2;lanes.high=j3;
    const auto matrix=runMatrix(laneIROnlyOptions(),lanes,setter({{"mode",2},{"cab1",1},{"cab2",1},{"cab3",1},{"cabblend1",0},{"cabblend2",0},{"cabblend3",0},{aOn0,0},{aOn1,0},{aOn2,0},{"cabtype1",1},{"cabtype2",2},{"cabtype3",2}}));
    const auto* matrixStep=stepOf(matrix.status,P::laneIR);
    check(matrix.status.passes==std::vector<P>{P::matrix} && matrixStep && matrixStep->changed && matrixStep->result.contains("LOW lane mic A IR V30 / SM57 > Jensen / SM57") && source(matrix.values,"cabtype1")==2
          && source(matrix.values,"cabtype2")==2 && source(matrix.values,"cabtype3")==2,"Matrix LOW lane IR not switched: "+(matrixStep?matrixStep->result.toStdString():std::string("no step")));
    // Real heads (measured): notes through the processor's default Dual heads, both rigs on the V30; rig 1's
    // switch to the Jensen is predicted from the captures and re-rendered by the engine.
    const auto notes=pluckedEvery(250,8,48000,6);
    std::vector<float> dry(notes.size());for(size_t n=0;n<dry.size();++n)dry[n]=notes[n]+.2f*amp1[n%amp1.size()];
    const auto heads=laneIRProcessorRigs(dry,1,1),again=laneIRProcessorRigs(dry,2,1);
    double moved=0,size=0;for(size_t n=0;n<std::min(heads.first.size(),again.first.size());++n){moved+=std::pow(double(heads.first[n]-again.first[n]),2);size+=std::pow(double(heads.first[n]),2);}
    check(size>0 && moved>1e-3*size,"The processor did not play the other IR on rig 1");
    const auto factory=laneIRFactory(48000);
    const auto real=analyseLaneIR({&heads.first,&heads.second},{.5,.5},{{0,1},{1,1}},48000,factory,false);
    const auto predicted=analyseLaneIR({&heads.first,&heads.second},{.5,.5},{{0,1}},48000,factory,false,Style::rock,true);
    check(real.playingDetected && predicted.lanes.size()==1 && predicted.lanes[0].heard,"Real-head rigs not heard");
    const float renderedError=laneMixError({&again.first,&again.second},{.5,.5},48000);
    const float miss=renderedError-predicted.lanes[0].toneErrorAfterDb;
    check(std::abs(miss)<.5f,"Real-head lane IR prediction off by "+std::to_string(miss)+" dB");
    std::cout<<"MEASURE lane IR real heads (Dual 50%, both rigs V30): step "<<(real.changed()?"switches":"keeps")<<" (balance "<<real.lanes[0].toneErrorBeforeDb<<" dB); rig 1 > Jensen: balance "
             <<predicted.lanes[0].toneErrorAfterDb<<" dB predicted, "<<renderedError<<" dB re-rendered\n";
    std::cout<<"MEASURE auto set lane IR controller: "<<step->result<<" | Matrix: "<<matrixStep->result<<'\n';
    std::cout<<"PASS auto set lane IR controller: off by default; Dual rig 1 and Matrix LOW lane IRs; mic B; strength not applied; Cabinet IR lock; cabinet off, blended, modeled, filters only, user IR, silent, edited, bass, Classic; cuts wait after a switch; undo; real heads\n";
}
