#pragma once
// Phase 34: PRESENCE and RESONANCE of both Dual Blend rigs' heads (docs/AUTO_SET_PHASE34.md).
// Included by AutoSetTests.cpp after the Dual amp tests (runDualAmp is used here).

// MSVC only: compiled without optimisation, as the other tests added since phase 29 (see AutoSetLaneIRTests.h).
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("",off)
#endif
void controllerDualPower() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto setter=[](std::initializer_list<std::pair<juce::String,float>> pairs){
        std::vector<std::pair<juce::String,float>> list(pairs);
        return [list](ChimeraProcessor& p){for(const auto& [id,v]:list)p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));};};
    check(!Controller::Options{}.dualPower && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.dualPower,"The Dual Blend rigs' presence + resonance step is on by default");
    Controller::Options powerOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    powerOnly.sync=powerOnly.cabIR=powerOnly.polarity=false;powerOnly.dualPower=true;
    // Each rig's PRESENCE on the head runDualAmp plays (model 2, its default channel).
    const auto presenceID=[](ChimeraProcessor& p,int rig){const int channel=p.selectedAmpChannel(rig);return spectralforge::ampNativeControlID(spectralforge::ampNativeContext(1,rig),2,powerControl(2,channel,powerPresence),channel);};
    std::array<juce::String,2> presence;
    // The passes hear the heads without a cabinet, so their top end is bright; with PRESENCE at maximum it comes down.
    const auto bright=[&](ChimeraProcessor& p){for(int rig=0;rig<2;++rig){presence[size_t(rig)]=presenceID(p,rig);p.parameters().getParameter(presence[size_t(rig)])->setValueNotifyingHost(1);}};
    const auto full=runDualAmp(powerOnly,{.3f,.3f},{},{},bright);
    const auto* step1=stepOf(full.status,P::dualPower1);const auto* step2=stepOf(full.status,P::dualPower2);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::dualAmp1,P::dualAmp2} && !stepOf(full.status,P::dualAmp1) && !stepOf(full.status,P::dualAmp2)
          && step1 && step2 && step1->changed && step2->changed && step1->result.contains("PRESENCE") && step2->result.contains("PRESENCE"),
          "The presence step did not run alone in each rig's pass or report: "+(step1?step1->result.toStdString():std::string("no step")));
    for(int rig=0;rig<2;++rig)check(value(full.values,presence[size_t(rig)])<value(full.before,presence[size_t(rig)]),"Rig "+std::to_string(rig+1)+"'s bright PRESENCE was not lowered");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the rigs' PRESENCE and RESONANCE");
    // Strength moves the knob part of the way.
    auto halfOptions=powerOnly;halfOptions.strength=.5f;
    const auto half=runDualAmp(halfOptions,{.3f,.3f},{},{},bright);
    const float fullMove=value(full.before,presence[0])-value(full.values,presence[0]),halfMove=value(half.before,presence[0])-value(half.values,presence[0]);
    check(halfMove>0 && halfMove<fullMove,"Strength did not scale rig 1's PRESENCE move ("+std::to_string(halfMove)+" of "+std::to_string(fullMove)+")");
    // With the rigs' amp step: per rig, the amp row comes before the presence row; each lock keeps its own step.
    auto bothOptions=powerOnly;bothOptions.dualAmp=true;
    const auto both=runDualAmp(bothOptions,{.05f,.05f},{},{},bright);
    const auto at=[&](P phase){for(size_t i=0;i<both.status.steps.size();++i)if(both.status.steps[i].phase==phase)return int(i);return -1;};
    check(at(P::dualAmp1)>=0 && at(P::dualPower1)>at(P::dualAmp1) && at(P::dualAmp2)>at(P::dualPower1) && at(P::dualPower2)>at(P::dualAmp2) && both.status.passes==std::vector<P>{P::dualAmp1,P::dualAmp2},
          "The rigs' amp and presence rows are out of order");
    auto powerLocked=bothOptions;powerLocked.locks|=Controller::lockPower;
    const auto ampOnly=runDualAmp(powerLocked,{.05f,.05f},{},{},bright);
    check(stepOf(ampOnly.status,P::dualPower1)->result=="kept (locked)" && stepOf(ampOnly.status,P::dualAmp1)->changed && value(ampOnly.values,presence[0])==value(ampOnly.before,presence[0])
          && ampOnly.status.passes==std::vector<P>{P::dualAmp1,P::dualAmp2},"The Presence + resonance lock did not keep the rigs' knobs alone");
    auto ampLocked=bothOptions;ampLocked.locks|=Controller::lockAmp;
    const auto presenceOnly=runDualAmp(ampLocked,{.05f,.05f},{},{},bright);
    check(stepOf(presenceOnly.status,P::dualAmp1)->result=="kept (locked)" && stepOf(presenceOnly.status,P::dualPower1)->changed && value(presenceOnly.values,presenceOnly.gainID[0])==value(presenceOnly.before,presenceOnly.gainID[0]),
          "The Amp lock did not leave the rigs' presence step running");
    auto allLocked=powerOnly;allLocked.locks=Controller::lockCompressor|Controller::lockPower;
    const auto locked=runDualAmp(allLocked,{.3f,.3f},{},{},bright);
    check(locked.status.passes==std::vector<P>{P::input} && stepOf(locked.status,P::dualPower1)->result=="kept (locked)" && stepOf(locked.status,P::dualPower2)->result=="kept (locked)","The Presence + resonance lock still ran a pass");
    // Kept without a pass: Dual Crossover, Matrix, Classic.
    auto openOptions=powerOnly;openOptions.locks=Controller::lockCompressor;
    const auto planned=[&](const std::function<void(ChimeraProcessor&)>& prepare,const char* text,bool bothRows) {
        const auto run=runDualAmp(openOptions,{.3f,.3f},prepare,{},bright);const auto* s=stepOf(run.status,P::dualPower1);
        check(s && !s->changed && s->result.contains(text) && (stepOf(run.status,P::dualPower2)!=nullptr)==bothRows && run.status.passes==std::vector<P>{P::input}
              && value(run.values,presence[0])==value(run.before,presence[0]),std::string("Not kept as '")+text+"': "+(s?s->result.toStdString():std::string("no step")));
    };
    planned(setter({{"dualtype",1}}),"Dual Crossover: the rigs play separate bands",true);
    planned(setter({{"mode",2}}),"Matrix routing: the lanes play separate bands",true);
    planned(setter({{"mode",0}}),"Classic routing: the Presence and resonance step sets the head",false);
    // An amp that is off keeps that rig; the other is still set.
    const auto ampOff=runDualAmp(powerOnly,{.3f,.3f},setter({{"ampon1",0}}),{},bright);
    check(stepOf(ampOff.status,P::dualPower1)->result=="kept (the amp is switched off)" && stepOf(ampOff.status,P::dualPower2)->changed && value(ampOff.values,presence[0])==value(ampOff.before,presence[0]),"A rig whose amp is off was set, or stopped the other rig");
    std::cout<<"MEASURE auto set dual power controller: rig 1: "<<step1->result<<" | rig 2: "<<step2->result<<'\n';
    std::cout<<"PASS auto set dual power controller: off by default; in each rig's pass, alone or after the rig's amp step; bright PRESENCE lowered; strength; each lock alone; Crossover, Matrix, Classic, amp off; undo\n";
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("",on)
#endif
