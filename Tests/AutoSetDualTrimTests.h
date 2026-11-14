#pragma once
// Phase 36: the amp input trim of both Dual Blend rigs' heads (docs/AUTO_SET_PHASE36.md).
// Included by AutoSetTests.cpp after the Dual amp tests (runDualAmp is used here).

void controllerDualTrim() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    check(!Controller::Options{}.dualTrim && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.dualTrim,"The Dual Blend rigs' amp input trim step is on by default");
    constexpr int model=2,rhythm=0;
    const std::array<int,2> context{spectralforge::ampNativeContext(1,0),spectralforge::ampNativeContext(1,1)};
    const std::array<juce::String,2> trimID{spectralforge::ampNativeInputTrimID(context[0]),spectralforge::ampNativeInputTrimID(context[1])};
    const std::array<juce::String,2> outputID{spectralforge::ampNativeOutputLevelID(context[0]),spectralforge::ampNativeOutputLevelID(context[1])};
    Controller::Options trimOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    trimOnly.sync=trimOnly.cabIR=trimOnly.polarity=false;trimOnly.dualTrim=true;
    // Both rigs on RHYTHM with that channel's GAIN at its bottom, as the Classic trim test.
    const auto rhythmHeads=[&](ChimeraProcessor& p){
        for(int rig=0;rig<2;++rig) {
            auto* channel=p.parameters().getParameter(spectralforge::ampNativeChannelID(context[size_t(rig)],model));channel->setValueNotifyingHost(channel->convertTo0to1(float(rhythm)));
            p.parameters().getParameter(spectralforge::ampNativeControlID(context[size_t(rig)],model,roleControls(model,rhythm,0)[ampGain],rhythm))->setValueNotifyingHost(0.f);
        }
    };
    const auto setRaw=[](const juce::String& id,float v){return [id,v](ChimeraProcessor& p){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};};
    // Rock with GAIN at its bottom: each rig's trim rises and its OUTPUT LEVEL holds the loudness.
    const auto full=runDualAmp(trimOnly,{0.f,0.f},{},{},rhythmHeads);
    const auto* step1=stepOf(full.status,P::dualTrim1);const auto* step2=stepOf(full.status,P::dualTrim2);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::dualAmp1,P::dualAmp2} && !stepOf(full.status,P::dualAmp1) && step1 && step2 && step1->changed && step2->changed
          && step1->result.contains("INPUT TRIM +0.0 dB > +") && step1->result.contains("OUTPUT LEVEL") && step2->result.contains("INPUT TRIM +0.0 dB > +"),
          "The trim step did not run alone in each rig's pass or report: "+(step1?step1->result.toStdString():std::string("no step")));
    for(int rig=0;rig<2;++rig)check(value(full.values,trimID[size_t(rig)])>0 && value(full.values,outputID[size_t(rig)])<value(full.before,outputID[size_t(rig)]),"Rig "+std::to_string(rig+1)+"'s trim did not rise with its level lowered");
    check(near(value(full.values,spectralforge::ampNativeInputTrimID(0)),value(full.before,spectralforge::ampNativeInputTrimID(0)),1e-6f),"The Classic head's trim was touched");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the rigs' trim and level");
    // Strength scales the trim.
    auto halfOptions=trimOnly;halfOptions.strength=.5f;
    const auto half=runDualAmp(halfOptions,{0.f,0.f},{},{},rhythmHeads);
    check(value(half.values,trimID[0])>0 && value(half.values,trimID[0])<value(full.values,trimID[0]),"Strength did not scale rig 1's trim");
    // The Dual rig levels lock keeps rig 2's OUTPUT LEVEL, so rig 2's trim is kept with the reason; rig 1 is still set.
    auto rigsLocked=trimOnly;rigsLocked.locks|=Controller::lockRigs;
    const auto levelKept=runDualAmp(rigsLocked,{0.f,0.f},{},{},rhythmHeads);
    check(stepOf(levelKept.status,P::dualTrim2)->result.contains("the Dual rig levels lock keeps") && near(value(levelKept.values,trimID[1]),value(levelKept.before,trimID[1]),1e-6f)
          && near(value(levelKept.values,outputID[1]),value(levelKept.before,outputID[1]),1e-6f) && stepOf(levelKept.status,P::dualTrim1)->changed,"The Dual rig levels lock did not keep rig 2's trim and level");
    // With the rigs' amp step: per rig, the amp row comes before the trim row.
    auto withAmp=trimOnly;withAmp.dualAmp=true;
    const auto ordered=runDualAmp(withAmp,{0.f,0.f},{},{},rhythmHeads);
    const auto at=[&](P phase){for(size_t i=0;i<ordered.status.steps.size();++i)if(ordered.status.steps[i].phase==phase)return int(i);return -1;};
    check(at(P::dualAmp1)>=0 && at(P::dualTrim1)>at(P::dualAmp1) && at(P::dualAmp2)>at(P::dualTrim1) && at(P::dualTrim2)>at(P::dualAmp2),"The rigs' amp and trim rows are out of order");
    // Kept: the Amp input trim lock (no pass), Dual Crossover, Matrix, Classic, an amp that is off, a trim edited while listening.
    auto lockedOptions=trimOnly;lockedOptions.locks=Controller::lockCompressor|Controller::lockTrim;
    const auto locked=runDualAmp(lockedOptions,{0.f,0.f},{},{},rhythmHeads);
    check(locked.status.passes==std::vector<P>{P::input} && stepOf(locked.status,P::dualTrim1)->result=="kept (locked)" && stepOf(locked.status,P::dualTrim2)->result=="kept (locked)"
          && near(value(locked.values,trimID[0]),value(locked.before,trimID[0]),1e-6f),"The Amp input trim lock did not keep the rigs' trim");
    auto openOptions=trimOnly;openOptions.locks=Controller::lockCompressor;
    const auto planned=[&](const std::function<void(ChimeraProcessor&)>& prepare,const char* text,bool bothRows) {
        const auto run=runDualAmp(openOptions,{0.f,0.f},prepare,{},rhythmHeads);const auto* s=stepOf(run.status,P::dualTrim1);
        check(s && !s->changed && s->result.contains(text) && (stepOf(run.status,P::dualTrim2)!=nullptr)==bothRows && run.status.passes==std::vector<P>{P::input}
              && near(value(run.values,trimID[0]),value(run.before,trimID[0]),1e-6f),std::string("Not kept as '")+text+"': "+(s?s->result.toStdString():std::string("no step")));
    };
    planned(setRaw("dualtype",1),"Dual Crossover: the rigs play separate bands",true);
    planned(setRaw("mode",2),"Matrix routing: the lanes play separate bands",true);
    planned(setRaw("mode",0),"Classic routing: the Amp input trim step sets the head",false);
    const auto ampOff=runDualAmp(trimOnly,{0.f,0.f},setRaw("ampon1",0),{},rhythmHeads);
    check(stepOf(ampOff.status,P::dualTrim1)->result=="kept (the amp is switched off)" && stepOf(ampOff.status,P::dualTrim2)->changed && near(value(ampOff.values,trimID[0]),value(ampOff.before,trimID[0]),1e-6f),"A rig whose amp is off was trimmed, or stopped the other rig");
    const auto edited=runDualAmp(trimOnly,{0.f,0.f},{},setRaw(trimID[0],3.f),rhythmHeads);
    check(stepOf(edited.status,P::dualTrim1)->result=="kept (INPUT TRIM or OUTPUT LEVEL edited while listening)" && near(value(edited.values,trimID[0]),3.f,.011f),"A trim edited while listening was overwritten");
    std::cout<<"MEASURE auto set dual trim controller: rig 1: "<<step1->result<<" | rig 2: "<<step2->result<<'\n';
    std::cout<<"PASS auto set dual trim controller: off by default; in each rig's pass, alone or after the rig's amp step; trim raised with the level held; strength; rig levels lock keeps rig 2; trim lock; Crossover, Matrix, Classic, amp off, edited; undo\n";
}
