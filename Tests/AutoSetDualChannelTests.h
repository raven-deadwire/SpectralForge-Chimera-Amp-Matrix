#pragma once
// Phase 35: the amp channel of both Dual Blend rigs' heads (docs/AUTO_SET_PHASE35.md).
// Included by AutoSetTests.cpp after the Dual amp tests (runDualAmp is used here).

// MSVC only: compiled without optimisation, as the other tests added since phase 29 (see AutoSetLaneIRTests.h).
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("",off)
#endif
void controllerDualChannel() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    check(!Controller::Options{}.dualChannel && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.dualChannel,"The Dual Blend rigs' amp channel step is on by default");
    constexpr int model=2,rhythm=0,lead=1; // as the Classic channel test: RHYTHM reaches about 5-75% drive, LEAD about 42-90%
    const std::array<juce::String,2> channelID{spectralforge::ampNativeChannelID(spectralforge::ampNativeContext(1,0),model),spectralforge::ampNativeChannelID(spectralforge::ampNativeContext(1,1),model)};
    Controller::Options channelOnly{1,false,Style::clean,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    channelOnly.sync=channelOnly.cabIR=channelOnly.polarity=false;channelOnly.dualChannel=true;
    const auto setChannels=[&](int rig1,int rig2) {
        return [=,&channelID](ChimeraProcessor& p){
            const std::array<int,2> channels{rig1,rig2};
            for(int rig=0;rig<2;++rig){auto* q=p.parameters().getParameter(channelID[size_t(rig)]);q->setValueNotifyingHost(q->convertTo0to1(float(channels[size_t(rig)])));}
        };
    };
    const auto channelOf=[&](const std::vector<float>& v,int rig){return juce::roundToInt(value(v,channelID[size_t(rig)]));};
    const auto setRaw=[](const juce::String& id,float v){return [id,v](ChimeraProcessor& p){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};};
    // Clean, both rigs on LEAD: each rig moves to RHYTHM in its own pass, alone.
    const auto full=runDualAmp(channelOnly,{.5f,.5f},{},{},setChannels(lead,lead));
    const auto* step1=stepOf(full.status,P::dualChannel1);const auto* step2=stepOf(full.status,P::dualChannel2);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::dualAmp1,P::dualAmp2} && !stepOf(full.status,P::dualAmp1) && step1 && step2 && step1->changed && step2->changed
          && step1->result.contains("CHANNEL LEAD > RHYTHM") && step2->result.contains("CHANNEL LEAD > RHYTHM"),"The channel step did not run alone in each rig's pass or report: "+(step1?step1->result.toStdString():std::string("no step")));
    check(channelOf(full.before,0)==lead && channelOf(full.values,0)==rhythm && channelOf(full.values,1)==rhythm,"The rigs' channels were not switched");
    check(step1->result.contains("at its knobs"),"Without the rigs' amp step a channel was not judged at its knobs");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the rigs' channels");
    // One rig already on RHYTHM stays; the other is still switched.
    const auto mixed=runDualAmp(channelOnly,{.5f,.5f},{},{},setChannels(rhythm,lead));
    check(!stepOf(mixed.status,P::dualChannel1)->changed && channelOf(mixed.values,0)==rhythm && stepOf(mixed.status,P::dualChannel2)->changed && channelOf(mixed.values,1)==rhythm,"Rigs on different channels were not judged one by one");
    // With the rigs' amp step: per rig, the channel row comes first.
    auto withAmp=channelOnly;withAmp.dualAmp=true;
    const auto ordered=runDualAmp(withAmp,{.5f,.5f},{},{},setChannels(lead,lead));
    const auto at=[&](P phase){for(size_t i=0;i<ordered.status.steps.size();++i)if(ordered.status.steps[i].phase==phase)return int(i);return -1;};
    check(at(P::dualChannel1)>=0 && at(P::dualAmp1)>at(P::dualChannel1) && at(P::dualChannel2)>at(P::dualAmp1) && at(P::dualAmp2)>at(P::dualChannel2),"The rigs' channel and amp rows are out of order");
    // Kept: the Amp channel lock (no pass), Dual Crossover, Matrix, Classic, an amp that is off.
    auto lockedOptions=channelOnly;lockedOptions.locks=Controller::lockCompressor|Controller::lockChannel;
    const auto locked=runDualAmp(lockedOptions,{.5f,.5f},{},{},setChannels(lead,lead));
    check(locked.status.passes==std::vector<P>{P::input} && stepOf(locked.status,P::dualChannel1)->result=="kept (locked)" && stepOf(locked.status,P::dualChannel2)->result=="kept (locked)" && channelOf(locked.values,0)==lead,"The Amp channel lock did not keep the rigs' channels");
    auto openOptions=channelOnly;openOptions.locks=Controller::lockCompressor;
    const auto planned=[&](const std::function<void(ChimeraProcessor&)>& prepare,const char* text,bool bothRows) {
        const auto run=runDualAmp(openOptions,{.5f,.5f},prepare,{},setChannels(lead,lead));const auto* s=stepOf(run.status,P::dualChannel1);
        check(s && !s->changed && s->result.contains(text) && (stepOf(run.status,P::dualChannel2)!=nullptr)==bothRows && run.status.passes==std::vector<P>{P::input} && channelOf(run.values,0)==lead,
              std::string("Not kept as '")+text+"': "+(s?s->result.toStdString():std::string("no step")));
    };
    planned(setRaw("dualtype",1),"Dual Crossover: the rigs play separate bands",true);
    planned(setRaw("mode",2),"Matrix routing: the lanes play separate bands",true);
    planned(setRaw("mode",0),"Classic routing: the Amp channel selection step sets the head",false);
    const auto ampOff=runDualAmp(channelOnly,{.5f,.5f},setRaw("ampon1",0),{},setChannels(lead,lead));
    check(stepOf(ampOff.status,P::dualChannel1)->result=="kept (the amp is switched off)" && channelOf(ampOff.values,0)==lead && stepOf(ampOff.status,P::dualChannel2)->changed,"A rig whose amp is off was switched, or stopped the other rig");
    std::cout<<"MEASURE auto set dual channel controller: rig 1: "<<step1->result<<'\n';
    std::cout<<"PASS auto set dual channel controller: off by default; in each rig's pass, alone or before the rig's amp step; clean LEAD > RHYTHM per rig; lock; Crossover, Matrix, Classic, amp off; undo\n";
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("",on)
#endif
