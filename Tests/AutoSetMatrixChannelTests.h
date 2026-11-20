#pragma once
// Phase 41: the amp channel of the Matrix MID and HIGH lanes' heads (docs/AUTO_SET_PHASE41.md).
// Included by AutoSetTests.cpp after the Matrix gain tests (runMatrixGain is used here).

void controllerMatrixChannel() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    check(!Controller::Options{}.matrixChannel && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.matrixChannel,"The Matrix lanes' amp channel step is on by default");
    constexpr int model=2,rhythm=0,lead=1; // as the Classic channel test: RHYTHM reaches about 5-75% drive, LEAD about 42-90%
    const std::array<juce::String,2> channelID{spectralforge::ampNativeChannelID(spectralforge::ampNativeContext(2,1),model),spectralforge::ampNativeChannelID(spectralforge::ampNativeContext(2,2),model)};
    Controller::Options channelOnly{1,false,Style::clean,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    channelOnly.sync=channelOnly.cabIR=channelOnly.polarity=false;channelOnly.matrixChannel=true;
    const auto setChannels=[&](int mid,int high) {
        return [=,&channelID](ChimeraProcessor& p){
            const std::array<int,2> channels{mid,high};
            for(int index=0;index<2;++index){auto* q=p.parameters().getParameter(channelID[size_t(index)]);q->setValueNotifyingHost(q->convertTo0to1(float(channels[size_t(index)])));}
        };
    };
    const auto channelOf=[&](const std::vector<float>& v,int index){return juce::roundToInt(value(v,channelID[size_t(index)]));};
    const auto setRaw=[](const juce::String& id,float v){return [id,v](ChimeraProcessor& p){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};};
    // Clean, both lanes on LEAD: each lane moves to RHYTHM in its own pass, alone.
    const auto full=runMatrixGain(channelOnly,{.5f,.5f},{},{},setChannels(lead,lead));
    const auto* step2=stepOf(full.status,P::matrixChannel2);const auto* step3=stepOf(full.status,P::matrixChannel3);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::matrixGain2,P::matrixGain3} && !stepOf(full.status,P::matrixGain2) && step2 && step3 && step2->changed && step3->changed
          && step2->result.contains("CHANNEL LEAD > RHYTHM") && step3->result.contains("CHANNEL LEAD > RHYTHM"),"The channel step did not run alone in each Matrix lane's pass or report: "+(step2?step2->result.toStdString():std::string("no step")));
    check(channelOf(full.before,0)==lead && channelOf(full.values,0)==rhythm && channelOf(full.values,1)==rhythm,"The lanes' channels were not switched");
    check(step2->result.contains("at its knobs"),"Without the lanes' gain step a channel was not judged at its knobs");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the lanes' channels");
    // One lane already on RHYTHM stays; the other is still switched.
    const auto mixed=runMatrixGain(channelOnly,{.5f,.5f},{},{},setChannels(rhythm,lead));
    check(!stepOf(mixed.status,P::matrixChannel2)->changed && channelOf(mixed.values,0)==rhythm && stepOf(mixed.status,P::matrixChannel3)->changed && channelOf(mixed.values,1)==rhythm,"Lanes on different channels were not judged one by one");
    // With the lanes' gain step: per lane, the channel row comes first, and the channel is judged by what GAIN can reach.
    auto withGain=channelOnly;withGain.matrixGain=true;
    const auto ordered=runMatrixGain(withGain,{.5f,.5f},{},{},setChannels(lead,lead));
    const auto at=[&](P phase){for(size_t i=0;i<ordered.status.steps.size();++i)if(ordered.status.steps[i].phase==phase)return int(i);return -1;};
    check(at(P::matrixChannel2)>=0 && at(P::matrixGain2)>at(P::matrixChannel2) && at(P::matrixChannel3)>at(P::matrixGain2) && at(P::matrixGain3)>at(P::matrixChannel3),"The lanes' channel and gain rows are out of order");
    check(!stepOf(ordered.status,P::matrixChannel2)->result.contains("at its knobs"),"With the lanes' gain step a channel was judged at its knobs");
    // The Amp lock keeps GAIN; the channel is then judged at its knobs and still set.
    auto gainLocked=withGain;gainLocked.locks|=Controller::lockAmp;
    const auto knobsKept=runMatrixGain(gainLocked,{.5f,.5f},{},{},setChannels(lead,lead));
    check(knobsKept.status.passes==std::vector<P>{P::matrixGain2,P::matrixGain3} && stepOf(knobsKept.status,P::matrixGain2)->result=="kept (locked)" && stepOf(knobsKept.status,P::matrixChannel2)->changed,
          "With the Amp lock the channel step did not run alone");
    // Kept: the Amp channel lock (no pass), Classic, Dual Blend, Dual Crossover, an amp that is off.
    auto lockedOptions=channelOnly;lockedOptions.locks=Controller::lockCompressor|Controller::lockChannel;
    const auto locked=runMatrixGain(lockedOptions,{.5f,.5f},{},{},setChannels(lead,lead));
    check(locked.status.passes==std::vector<P>{P::input} && stepOf(locked.status,P::matrixChannel2)->result=="kept (locked)" && stepOf(locked.status,P::matrixChannel3)->result=="kept (locked)" && channelOf(locked.values,0)==lead,"The Amp channel lock did not keep the lanes' channels");
    auto openOptions=channelOnly;openOptions.locks=Controller::lockCompressor;
    const auto planned=[&](const std::function<void(ChimeraProcessor&)>& prepare,const char* text,bool bothRows) {
        const auto run=runMatrixGain(openOptions,{.5f,.5f},prepare,{},setChannels(lead,lead));const auto* s=stepOf(run.status,P::matrixChannel2);
        check(s && !s->changed && s->result.contains(text) && (stepOf(run.status,P::matrixChannel3)!=nullptr)==bothRows && run.status.passes==std::vector<P>{P::input} && channelOf(run.values,0)==lead,
              std::string("Not kept as '")+text+"': "+(s?s->result.toStdString():std::string("no step")));
    };
    planned(setRaw("mode",0),"Classic routing: the Amp channel selection step sets the head",false);
    planned([&](ChimeraProcessor& p){setRaw("mode",1)(p);setRaw("dualtype",0)(p);},"Dual Blend: the Amp channel selection of both Dual Blend rigs step sets the heads",true);
    planned([&](ChimeraProcessor& p){setRaw("mode",1)(p);setRaw("dualtype",1)(p);},"Dual Crossover: the Amp channel selection of both Dual Crossover rigs step sets the heads",true);
    const auto ampOff=runMatrixGain(channelOnly,{.5f,.5f},setRaw("ampon2",0),{},setChannels(lead,lead));
    check(stepOf(ampOff.status,P::matrixChannel2)->result=="kept (the amp is switched off)" && channelOf(ampOff.values,0)==lead && stepOf(ampOff.status,P::matrixChannel3)->changed,"A lane whose amp is off was switched, or stopped the other lane");
    std::cout<<"MEASURE auto set matrix channel controller: MID: "<<step2->result<<'\n';
    std::cout<<"PASS auto set matrix channel controller: off by default; in each Matrix lane's pass, alone or before the lane's gain step; clean LEAD > RHYTHM per lane; Amp lock; channel lock; Classic, Blend, Crossover, amp off; undo\n";
}
