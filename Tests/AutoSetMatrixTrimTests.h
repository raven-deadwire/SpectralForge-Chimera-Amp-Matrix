#pragma once
// Phase 42: the amp input trim of the Matrix MID and HIGH lanes' heads (docs/AUTO_SET_PHASE42.md).
// Included by AutoSetTests.cpp after the Matrix gain tests (runMatrixGain is used here).

void controllerMatrixTrim() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    check(!Controller::Options{}.matrixTrim && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.matrixTrim,"The Matrix lanes' amp input trim step is on by default");
    constexpr int model=2,rhythm=0;
    const std::array<int,2> context{spectralforge::ampNativeContext(2,1),spectralforge::ampNativeContext(2,2)};
    const std::array<juce::String,2> trimID{spectralforge::ampNativeInputTrimID(context[0]),spectralforge::ampNativeInputTrimID(context[1])};
    const std::array<juce::String,2> outputID{spectralforge::ampNativeOutputLevelID(context[0]),spectralforge::ampNativeOutputLevelID(context[1])};
    Controller::Options trimOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    trimOnly.sync=trimOnly.cabIR=trimOnly.polarity=false;trimOnly.matrixTrim=true;
    // Both lanes on RHYTHM with that channel's GAIN at its bottom, as the Classic trim test.
    const auto rhythmHeads=[&](ChimeraProcessor& p){
        for(int index=0;index<2;++index) {
            auto* channel=p.parameters().getParameter(spectralforge::ampNativeChannelID(context[size_t(index)],model));channel->setValueNotifyingHost(channel->convertTo0to1(float(rhythm)));
            p.parameters().getParameter(spectralforge::ampNativeControlID(context[size_t(index)],model,roleControls(model,rhythm,0)[ampGain],rhythm))->setValueNotifyingHost(0.f);
        }
    };
    const auto setRaw=[](const juce::String& id,float v){return [id,v](ChimeraProcessor& p){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};};
    // Rock with GAIN at its bottom: each lane's trim rises and its OUTPUT LEVEL holds the loudness.
    const auto full=runMatrixGain(trimOnly,{0.f,0.f},{},{},rhythmHeads);
    const auto* step2=stepOf(full.status,P::matrixTrim2);const auto* step3=stepOf(full.status,P::matrixTrim3);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::matrixGain2,P::matrixGain3} && !stepOf(full.status,P::matrixGain2) && step2 && step3 && step2->changed && step3->changed
          && step2->result.contains("INPUT TRIM +0.0 dB > +") && step2->result.contains("OUTPUT LEVEL") && step3->result.contains("INPUT TRIM +0.0 dB > +"),
          "The trim step did not run alone in each Matrix lane's pass or report: "+(step2?step2->result.toStdString():std::string("no step")));
    for(int index=0;index<2;++index)check(value(full.values,trimID[size_t(index)])>0 && value(full.values,outputID[size_t(index)])<value(full.before,outputID[size_t(index)]),"Lane "+std::to_string(index+2)+"'s trim did not rise with its level lowered");
    check(near(value(full.values,spectralforge::ampNativeInputTrimID(0)),value(full.before,spectralforge::ampNativeInputTrimID(0)),1e-6f)
          && near(value(full.values,spectralforge::ampNativeInputTrimID(spectralforge::ampNativeContext(2,0))),value(full.before,spectralforge::ampNativeInputTrimID(spectralforge::ampNativeContext(2,0))),1e-6f),"The Classic or the LOW lane's trim was touched");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the lanes' trim and level");
    // Strength scales the trim.
    auto halfOptions=trimOnly;halfOptions.strength=.5f;
    const auto half=runMatrixGain(halfOptions,{0.f,0.f},{},{},rhythmHeads);
    check(value(half.values,trimID[0])>0 && value(half.values,trimID[0])<value(full.values,trimID[0]),"Strength did not scale the MID lane's trim");
    // The Matrix levels lock keeps both lanes' OUTPUT LEVEL, so both trims are kept with the reason, in the passes.
    auto levelsLocked=trimOnly;levelsLocked.locks|=Controller::lockMatrix;
    const auto levelKept=runMatrixGain(levelsLocked,{0.f,0.f},{},{},rhythmHeads);
    for(int index=0;index<2;++index)check(stepOf(levelKept.status,index==0?P::matrixTrim2:P::matrixTrim3)->result.contains("the Matrix levels lock keeps")
          && near(value(levelKept.values,trimID[size_t(index)]),value(levelKept.before,trimID[size_t(index)]),1e-6f) && near(value(levelKept.values,outputID[size_t(index)]),value(levelKept.before,outputID[size_t(index)]),1e-6f),
          "The Matrix levels lock did not keep lane "+std::to_string(index+2)+"'s trim and level");
    // With the lanes' gain step: per lane, the gain row comes before the trim row, and the trim works past the GAIN the step proposed.
    auto withGain=trimOnly;withGain.matrixGain=true;
    const auto ordered=runMatrixGain(withGain,{0.f,0.f},{},{},rhythmHeads);
    const auto at=[&](P phase){for(size_t i=0;i<ordered.status.steps.size();++i)if(ordered.status.steps[i].phase==phase)return int(i);return -1;};
    check(at(P::matrixGain2)>=0 && at(P::matrixTrim2)>at(P::matrixGain2) && at(P::matrixGain3)>at(P::matrixTrim2) && at(P::matrixTrim3)>at(P::matrixGain3),"The lanes' gain and trim rows are out of order");
    check(value(ordered.values,trimID[0])<value(full.values,trimID[0]),"The trim did not count the GAIN the lane's gain step proposed");
    // Kept: the Amp input trim lock (no pass), Classic, Dual Blend, Dual Crossover, an amp that is off, a trim edited while listening.
    auto lockedOptions=trimOnly;lockedOptions.locks=Controller::lockCompressor|Controller::lockTrim;
    const auto locked=runMatrixGain(lockedOptions,{0.f,0.f},{},{},rhythmHeads);
    check(locked.status.passes==std::vector<P>{P::input} && stepOf(locked.status,P::matrixTrim2)->result=="kept (locked)" && stepOf(locked.status,P::matrixTrim3)->result=="kept (locked)"
          && near(value(locked.values,trimID[0]),value(locked.before,trimID[0]),1e-6f),"The Amp input trim lock did not keep the lanes' trim");
    auto openOptions=trimOnly;openOptions.locks=Controller::lockCompressor;
    const auto planned=[&](const std::function<void(ChimeraProcessor&)>& prepare,const char* text,bool bothRows) {
        const auto run=runMatrixGain(openOptions,{0.f,0.f},prepare,{},rhythmHeads);const auto* s=stepOf(run.status,P::matrixTrim2);
        check(s && !s->changed && s->result.contains(text) && (stepOf(run.status,P::matrixTrim3)!=nullptr)==bothRows && run.status.passes==std::vector<P>{P::input}
              && near(value(run.values,trimID[0]),value(run.before,trimID[0]),1e-6f),std::string("Not kept as '")+text+"': "+(s?s->result.toStdString():std::string("no step")));
    };
    planned(setRaw("mode",0),"Classic routing: the Amp input trim step sets the head",false);
    planned([&](ChimeraProcessor& p){setRaw("mode",1)(p);setRaw("dualtype",0)(p);},"Dual Blend: the Amp input trim of both Dual Blend rigs step sets the heads",true);
    planned([&](ChimeraProcessor& p){setRaw("mode",1)(p);setRaw("dualtype",1)(p);},"Dual Crossover: the Amp input trim of both Dual Crossover rigs step sets the heads",true);
    const auto ampOff=runMatrixGain(trimOnly,{0.f,0.f},setRaw("ampon2",0),{},rhythmHeads);
    check(stepOf(ampOff.status,P::matrixTrim2)->result=="kept (the amp is switched off)" && stepOf(ampOff.status,P::matrixTrim3)->changed && near(value(ampOff.values,trimID[0]),value(ampOff.before,trimID[0]),1e-6f),"A lane whose amp is off was trimmed, or stopped the other lane");
    const auto edited=runMatrixGain(trimOnly,{0.f,0.f},{},setRaw(trimID[0],3.f),rhythmHeads);
    check(stepOf(edited.status,P::matrixTrim2)->result=="kept (INPUT TRIM or OUTPUT LEVEL edited while listening)" && near(value(edited.values,trimID[0]),3.f,.011f),"A trim edited while listening was overwritten");
    std::cout<<"MEASURE auto set matrix trim controller: MID: "<<step2->result<<" | HIGH: "<<step3->result<<" | with the gain step, MID trim "<<value(ordered.values,trimID[0])<<" dB\n";
    std::cout<<"PASS auto set matrix trim controller: off by default; in each Matrix lane's pass, alone or after the lane's gain step; trim raised with the level held; strength; Matrix levels lock keeps both; trim lock; Classic, Blend, Crossover, amp off, edited; undo\n";
}
