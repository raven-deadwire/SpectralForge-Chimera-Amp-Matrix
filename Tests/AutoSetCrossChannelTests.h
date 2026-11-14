#pragma once
// Phase 38: the amp channel of both Dual Crossover rigs' heads (docs/AUTO_SET_PHASE38.md).
// Included by AutoSetTests.cpp after the Dual amp tests (runDualAmp is used here).

void controllerCrossChannel() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    check(!Controller::Options{}.crossChannel && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.crossChannel,"The Dual Crossover rigs' amp channel step is on by default");
    constexpr int model=2,rhythm=0,lead=1; // as the Classic channel test: RHYTHM reaches about 5-75% drive, LEAD about 42-90%
    const std::array<juce::String,2> channelID{spectralforge::ampNativeChannelID(spectralforge::ampNativeContext(1,0),model),spectralforge::ampNativeChannelID(spectralforge::ampNativeContext(1,1),model)};
    Controller::Options channelOnly{1,false,Style::clean,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    channelOnly.sync=channelOnly.cabIR=channelOnly.polarity=false;channelOnly.crossChannel=true;
    const auto setChannels=[&](int rig1,int rig2) {
        return [=,&channelID](ChimeraProcessor& p){
            const std::array<int,2> channels{rig1,rig2};
            for(int rig=0;rig<2;++rig){auto* q=p.parameters().getParameter(channelID[size_t(rig)]);q->setValueNotifyingHost(q->convertTo0to1(float(channels[size_t(rig)])));}
        };
    };
    const auto channelOf=[&](const std::vector<float>& v,int rig){return juce::roundToInt(value(v,channelID[size_t(rig)]));};
    const auto setRaw=[](const juce::String& id,float v){return [id,v](ChimeraProcessor& p){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};};
    const auto crossover=setRaw("dualtype",1);
    // Clean, both rigs on LEAD: each rig moves to RHYTHM in its own pass, alone.
    const auto full=runDualAmp(channelOnly,{.5f,.5f},crossover,{},setChannels(lead,lead));
    const auto* step1=stepOf(full.status,P::crossChannel1);const auto* step2=stepOf(full.status,P::crossChannel2);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::crossGain1,P::crossGain2} && !stepOf(full.status,P::crossGain1) && step1 && step2 && step1->changed && step2->changed
          && step1->result.contains("CHANNEL LEAD > RHYTHM") && step2->result.contains("CHANNEL LEAD > RHYTHM"),"The channel step did not run alone in each Crossover rig's pass or report: "+(step1?step1->result.toStdString():std::string("no step")));
    check(channelOf(full.before,0)==lead && channelOf(full.values,0)==rhythm && channelOf(full.values,1)==rhythm,"The rigs' channels were not switched");
    check(step1->result.contains("at its knobs"),"Without the rigs' gain step a channel was not judged at its knobs");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the rigs' channels");
    // One rig already on RHYTHM stays; the other is still switched.
    const auto mixed=runDualAmp(channelOnly,{.5f,.5f},crossover,{},setChannels(rhythm,lead));
    check(!stepOf(mixed.status,P::crossChannel1)->changed && channelOf(mixed.values,0)==rhythm && stepOf(mixed.status,P::crossChannel2)->changed && channelOf(mixed.values,1)==rhythm,"Rigs on different channels were not judged one by one");
    // With the rigs' gain step: per rig, the channel row comes first, and the channel is judged by what GAIN can reach.
    auto withGain=channelOnly;withGain.crossGain=true;
    const auto ordered=runDualAmp(withGain,{.5f,.5f},crossover,{},setChannels(lead,lead));
    const auto at=[&](P phase){for(size_t i=0;i<ordered.status.steps.size();++i)if(ordered.status.steps[i].phase==phase)return int(i);return -1;};
    check(at(P::crossChannel1)>=0 && at(P::crossGain1)>at(P::crossChannel1) && at(P::crossChannel2)>at(P::crossGain1) && at(P::crossGain2)>at(P::crossChannel2),"The rigs' channel and gain rows are out of order");
    check(!stepOf(ordered.status,P::crossChannel1)->result.contains("at its knobs"),"With the rigs' gain step a channel was judged at its knobs");
    // The Amp lock keeps GAIN; the channel is then judged at its knobs and still set.
    auto gainLocked=withGain;gainLocked.locks|=Controller::lockAmp;
    const auto knobsKept=runDualAmp(gainLocked,{.5f,.5f},crossover,{},setChannels(lead,lead));
    check(knobsKept.status.passes==std::vector<P>{P::crossGain1,P::crossGain2} && stepOf(knobsKept.status,P::crossGain1)->result=="kept (locked)" && stepOf(knobsKept.status,P::crossChannel1)->changed
          && value(knobsKept.values,knobsKept.gainID[0])==value(knobsKept.before,knobsKept.gainID[0]),"With the Amp lock the channel step did not run alone");
    // Kept: the Amp channel lock (no pass), Dual Blend, Matrix, Classic, an amp that is off.
    auto lockedOptions=channelOnly;lockedOptions.locks=Controller::lockCompressor|Controller::lockChannel;
    const auto locked=runDualAmp(lockedOptions,{.5f,.5f},crossover,{},setChannels(lead,lead));
    check(locked.status.passes==std::vector<P>{P::input} && stepOf(locked.status,P::crossChannel1)->result=="kept (locked)" && stepOf(locked.status,P::crossChannel2)->result=="kept (locked)" && channelOf(locked.values,0)==lead,"The Amp channel lock did not keep the rigs' channels");
    auto openOptions=channelOnly;openOptions.locks=Controller::lockCompressor;
    const auto planned=[&](const std::function<void(ChimeraProcessor&)>& prepare,const char* text,bool bothRows) {
        const auto run=runDualAmp(openOptions,{.5f,.5f},prepare,{},setChannels(lead,lead));const auto* s=stepOf(run.status,P::crossChannel1);
        check(s && !s->changed && s->result.contains(text) && (stepOf(run.status,P::crossChannel2)!=nullptr)==bothRows && run.status.passes==std::vector<P>{P::input} && channelOf(run.values,0)==lead,
              std::string("Not kept as '")+text+"': "+(s?s->result.toStdString():std::string("no step")));
    };
    planned({},"Dual Blend: the Amp channel selection of both Dual Blend rigs step sets the heads",true);
    planned(setRaw("mode",2),"Matrix routing: the lanes play separate bands",true);
    planned(setRaw("mode",0),"Classic routing: the Amp channel selection step sets the head",false);
    const auto ampOff=runDualAmp(channelOnly,{.5f,.5f},[&](ChimeraProcessor& p){crossover(p);setRaw("ampon1",0)(p);},{},setChannels(lead,lead));
    check(stepOf(ampOff.status,P::crossChannel1)->result=="kept (the amp is switched off)" && channelOf(ampOff.values,0)==lead && stepOf(ampOff.status,P::crossChannel2)->changed,"A rig whose amp is off was switched, or stopped the other rig");
    std::cout<<"MEASURE auto set cross channel controller: rig 1: "<<step1->result<<'\n';
    std::cout<<"PASS auto set cross channel controller: off by default; in each Crossover rig's pass, alone or before the rig's gain step; clean LEAD > RHYTHM per rig; Amp lock; channel lock; Blend, Matrix, Classic, amp off; undo\n";
}
