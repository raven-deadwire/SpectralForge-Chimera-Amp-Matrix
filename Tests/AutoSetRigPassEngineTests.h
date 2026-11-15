#pragma once
// Phase 44: the rig and lane passes of phases 33, 37 and 40 on the real engine, through processBlock
// (docs/AUTO_SET_PHASE44.md). Earlier tests fed the controller's taps directly.
// Included by AutoSetTests.cpp after the Matrix gain tests (DI and run() are used here).

// One whole run with the played DI going through processBlock: the engine feeds the taps itself.
struct RigPassEngineRun {Controller::Status status;std::array<float,2> gainBefore{},gainAfter{};std::array<AmpResult,2> analysis{};std::array<bool,2> analysed{};};
// lanes: the two lanes the passes listen to (Dual rigs 0 and 1, Matrix lanes 1 and 2).
// lowNotes: pure 82 and 98 Hz notes, which do not reach a MID or HIGH band.
RigPassEngineRun runRigPassEngine(int mode,int dualType,std::array<int,2> lanes,const Controller::Options& options,std::array<float,2> gainFraction,
                                  const std::function<void(ChimeraProcessor&)>& prepare={},bool lowNotes=false) {
    using P=Controller::Phase;
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    constexpr int model=2;
    setRaw("mode",float(mode));setRaw("dualtype",float(dualType));
    std::array<juce::String,2> gainID;
    for(int index=0;index<2;++index) {
        const int lane=lanes[size_t(index)],context=spectralforge::ampNativeContext(mode,lane);
        setRaw("ampon"+juce::String(lane+1),1);setRaw(spectralforge::ampNativeEnabledID(context),1);setRaw(spectralforge::ampNativeModelID(context),float(model));
        const int channel=p->selectedAmpChannel(lane),route=p->selectedAmpNativeRoute(lane);
        gainID[size_t(index)]=spectralforge::ampNativeControlID(context,model,roleControls(model,channel,route)[ampGain],channel);
        p->parameters().getParameter(gainID[size_t(index)])->setValueNotifyingHost(gainFraction[size_t(index)]);
    }
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    RigPassEngineRun result;
    for(int index=0;index<2;++index)result.gainBefore[size_t(index)]=get(*p,gainID[size_t(index)]);
    // Played notes with eight harmonics (82 Hz to about 2.6 kHz), so every band of a crossover gets some of the playing.
    std::mt19937 rng{441};auto di=playedNotes(48000,8,rng);size_t offset=0;
    if(lowNotes) {
        // Pure tones well under the crossovers: half-second notes at 82 and 98 Hz, no harmonics, no hiss.
        for(size_t n=0;n<di.size();++n) {
            const double t=double(n%24000)/48000.,f=(n/24000)%2?98.:82.41;
            di[n]=float(.12*std::exp(-t*5)*std::min(1.,t/.003)*std::sin(2*juce::MathConstants<double>::pi*f*t));
        }
    }
    double now=juce::Time::getMillisecondCounterHiRes();
    juce::AudioBuffer<float> b(2,256);juce::MidiBuffer midi;
    const auto play=[&](int blocks,bool tick) {
        for(int k=0;k<blocks;++k) {
            if(offset+256>di.size())offset=0;
            for(int ch=0;ch<2;++ch)b.copyFrom(ch,0,di.data()+offset,256);
            offset+=256;p->processBlock(b,midi);now+=1000.*256/48000;
            if(tick && k%4==0)c.tick(now);
        }
        if(tick)c.tick(now);
    };
    play(400,false); // let the routing settle before the run starts, as a session that has been playing
    c.start();
    P last=P::idle;
    for(int step=0;c.busy() && step<240;++step) {
        const P phase=c.current().phase;
        play(47,true);
        // The analysis of a pass, read when the controller leaves it.
        if(c.current().phase!=phase || !c.busy()) {
            const int lane=Controller::dualAmpLane(phase);
            for(int index=0;index<2;++index)if(lane==lanes[size_t(index)] && phase!=last){result.analysis[size_t(index)]=c.lastAmpAnalysis();result.analysed[size_t(index)]=true;}
            last=phase;
        }
    }
    check(!c.busy(),"The run on the real engine did not finish");
    result.status=c.current();
    for(int index=0;index<2;++index)result.gainAfter[size_t(index)]=get(*p,gainID[size_t(index)]);
    p->releaseResources();
    return result;
}
// The engine's rig tap for a muted or solo-silenced lane is silent, as phase 43 assumes.
void rigPassEngineMute() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    std::mt19937 rng{440};const auto x=targetNoise(false,48000,1,rng);
    const auto rigEnergy=[&](int mode,int lane,const char* id) {
        for(const auto* each:{"mute1","mute2","mute3","solo1","solo2","solo3"})setRaw(each,0);
        setRaw("mode",float(mode));setRaw("dualtype",0);if(id)setRaw(id,1);
        p->setAutoSetRigLane(lane);
        juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
        const auto run=[&](int blocks){for(int block=0;block<blocks;++block){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+size_t(block%20)*256,256);p->processBlock(io,midi);}};
        run(80);
        p->autoSetCapture().listen({Tap::amp,Tap::cab});run(20);
        std::vector<float> amp,rig;p->autoSetCapture().drainAmpSamples(amp);p->autoSetCapture().drainSamples(rig);
        p->autoSetCapture().listen(std::nullopt);
        double energy=0;for(const float v:rig)energy+=double(v)*v;
        check(rig.size()==size_t(20*256),"The rig tap was not filled");
        return energy;
    };
    const double dualHeard=rigEnergy(1,0,nullptr),dualMuted=rigEnergy(1,0,"mute1"),dualOtherSolo=rigEnergy(1,0,"solo2"),dualOwnSolo=rigEnergy(1,0,"solo1");
    check(dualHeard>0 && dualMuted<1e-9*dualHeard && dualOtherSolo<1e-9*dualHeard && dualOwnSolo>.5*dualHeard,
          "The engine's rig tap does not follow MUTE and SOLO for a Dual rig ("+std::to_string(dualMuted/dualHeard)+", "+std::to_string(dualOtherSolo/dualHeard)+", "+std::to_string(dualOwnSolo/dualHeard)+")");
    const double laneHeard=rigEnergy(2,2,nullptr),laneMuted=rigEnergy(2,2,"mute3"),laneLowSolo=rigEnergy(2,2,"solo1");
    check(laneHeard>0 && laneMuted<1e-9*laneHeard && laneLowSolo<1e-9*laneHeard,
          "The engine's rig tap does not follow MUTE and SOLO for a Matrix lane ("+std::to_string(laneMuted/laneHeard)+", "+std::to_string(laneLowSolo/laneHeard)+")");
    p->setAutoSetRigLane(-1);p->releaseResources();
    std::cout<<"PASS auto set rig pass engine mute: the rig tap is silent for a muted Dual rig or Matrix lane and for one silenced by another's SOLO, and heard when it is the one soloed\n";
}
void rigPassEngine() {
    using P=Controller::Phase;
    Controller::Options base{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    base.sync=base.cabIR=base.polarity=false;
    const auto pct=[](float v){return std::to_string(juce::roundToInt(v*100))+"%";};
    const auto judge=[&](const char* name,const RigPassEngineRun& r,std::array<P,2> rows) {
        check(r.status.phase==P::done && r.status.passes==std::vector<P>{rows[0],rows[1]},std::string(name)+": the passes did not run on the real engine");
        std::string measured;
        for(int index=0;index<2;++index) {
            const auto* step=stepOf(r.status,rows[size_t(index)]);const auto& a=r.analysis[size_t(index)];
            check(step!=nullptr && r.analysed[size_t(index)],std::string(name)+": a pass left no row or analysis");
            check(a.playingDetected,std::string(name)+": the playing was not heard through the engine: "+step->result.toStdString());
            // GAIN started far below the rock drive: it must rise, and the drive must come closer to the target.
            check(step->changed && r.gainAfter[size_t(index)]>r.gainBefore[size_t(index)],std::string(name)+": a low GAIN was not raised: "+step->result.toStdString());
            check(std::abs(a.compressionAfter-a.compressionTarget)<std::abs(a.compressionBefore-a.compressionTarget),std::string(name)+": the drive did not come closer to the target: "+step->result.toStdString());
            measured+=std::string(index?" | ":"")+"GAIN "+std::to_string(r.gainBefore[size_t(index)])+" > "+std::to_string(r.gainAfter[size_t(index)])+", drive "+pct(a.compressionBefore)+" > "+pct(a.compressionAfter)+" (target "+pct(a.compressionTarget)+")";
        }
        std::cout<<"MEASURE auto set rig pass engine "<<name<<": "<<measured<<'\n';
    };
    auto blend=base;blend.dualAmp=true;
    judge("Dual Blend",runRigPassEngine(1,0,{0,1},blend,{.05f,.05f}),{P::dualAmp1,P::dualAmp2});
    auto cross=base;cross.crossGain=true;
    judge("Dual Crossover",runRigPassEngine(1,1,{0,1},cross,{.05f,.05f}),{P::crossGain1,P::crossGain2});
    auto matrix=base;matrix.matrixGain=true;
    judge("Matrix",runRigPassEngine(2,0,{1,2},matrix,{.05f,.05f}),{P::matrixGain2,P::matrixGain3});
    // A muted rig on the real engine: no pass, and the other rig is set.
    const auto muted=runRigPassEngine(1,0,{0,1},blend,{.05f,.05f},[](ChimeraProcessor& p){auto* q=p.parameters().getParameter("mute1");q->setValueNotifyingHost(1.f);});
    check(muted.status.passes==std::vector<P>{P::dualAmp2} && stepOf(muted.status,P::dualAmp1) && stepOf(muted.status,P::dualAmp1)->result=="kept (this rig is muted)"
          && muted.gainAfter[0]==muted.gainBefore[0] && muted.gainAfter[1]>muted.gainBefore[1],"A muted rig on the real engine was set, or stopped the other rig");
    // Phase 45: low notes barely reach the Matrix MID and HIGH bands: the lanes are kept and the reason names the band.
    const auto empty=runRigPassEngine(2,0,{1,2},matrix,{.05f,.05f},{},true);
    const auto* mid=stepOf(empty.status,P::matrixGain2);const auto* high=stepOf(empty.status,P::matrixGain3);
    check(mid && high && !mid->changed && !high->changed && empty.gainAfter==empty.gainBefore
          && mid->result=="kept (no playing heard in the MID lane's band: play notes that reach it, or move the crossover)"
          && high->result=="kept (no playing heard in the HIGH lane's band: play notes that reach it, or move the crossover)",
          "Lanes whose band is nearly empty were set, or the reason does not name the band: "+(mid?mid->result.toStdString():std::string("no step"))+" | "+(high?high->result.toStdString():std::string("no step")));
    // The same low notes in a Dual Blend, where each rig hears the whole input: heard, and the reason is not used.
    const auto whole=runRigPassEngine(1,0,{0,1},blend,{.05f,.05f},{},true);
    check(stepOf(whole.status,P::dualAmp1) && !stepOf(whole.status,P::dualAmp1)->result.contains("no playing heard"),"Low notes were not heard by a Dual Blend rig: "+stepOf(whole.status,P::dualAmp1)->result.toStdString());
    std::cout<<"PASS auto set rig pass engine: Dual Blend, Dual Crossover and Matrix rig and lane passes through processBlock: playing heard, low GAIN raised, drive closer to the target; a muted rig kept\n";
}
