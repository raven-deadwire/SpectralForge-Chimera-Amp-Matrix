#pragma once
// Phase 43: a muted rig or lane, or one not soloed while another is, is kept by the rig and lane
// passes of phases 33 to 42 (docs/AUTO_SET_PHASE43.md).
// Included by AutoSetTests.cpp after the Dual amp and Matrix gain tests (their runners are used here).

void controllerLaneMute() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto setRaw=[](const juce::String& id,float v){return [id,v](ChimeraProcessor& p){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};};
    const auto both=[](const std::function<void(ChimeraProcessor&)>& a,const std::function<void(ChimeraProcessor&)>& b){return [a,b](ChimeraProcessor& p){a(p);b(p);};};
    Controller::Options base{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    base.sync=base.cabIR=base.polarity=false;
    const auto same=[&](const std::vector<float>& a,const std::vector<float>& b,const juce::String& id){return near(value(a,id),value(b,id),1e-6f);};
    // Dual Blend: a muted rig gets no pass and the reason on its row; the other rig is still set.
    auto blend=base;blend.dualAmp=true;
    const auto muted=runDualAmp(blend,{.05f,.05f},setRaw("mute1",1));
    check(muted.status.passes==std::vector<P>{P::dualAmp2} && stepOf(muted.status,P::dualAmp1) && stepOf(muted.status,P::dualAmp1)->result=="kept (this rig is muted)"
          && same(muted.values,muted.before,muted.gainID[0]) && stepOf(muted.status,P::dualAmp2)->changed && value(muted.values,muted.gainID[1])>value(muted.before,muted.gainID[1]),
          "A muted Dual rig was listened to or set, or stopped the other rig: "+(stepOf(muted.status,P::dualAmp1)?stepOf(muted.status,P::dualAmp1)->result.toStdString():std::string("no step")));
    // A rig not soloed while the other is.
    const auto soloed=runDualAmp(blend,{.05f,.05f},setRaw("solo2",1));
    check(soloed.status.passes==std::vector<P>{P::dualAmp2} && stepOf(soloed.status,P::dualAmp1)->result=="kept (another rig is soloed)" && same(soloed.values,soloed.before,soloed.gainID[0])
          && stepOf(soloed.status,P::dualAmp2)->changed,"A rig silenced by the other rig's solo was listened to or set");
    // Both soloed: both heard.
    const auto bothSolo=runDualAmp(blend,{.05f,.05f},both(setRaw("solo1",1),setRaw("solo2",1)));
    check(bothSolo.status.passes==std::vector<P>{P::dualAmp1,P::dualAmp2} && stepOf(bothSolo.status,P::dualAmp1)->changed,"Two soloed rigs were not both set");
    // Every step of the muted rig's pass carries the reason.
    auto all=blend;all.dualChannel=all.dualTrim=all.dualPower=true;
    const auto allRows=runDualAmp(all,{.05f,.05f},setRaw("mute1",1));
    for(const P row:{P::dualChannel1,P::dualAmp1,P::dualTrim1,P::dualPower1})check(stepOf(allRows.status,row) && stepOf(allRows.status,row)->result=="kept (this rig is muted)","A muted rig's row lacks the reason");
    check(allRows.status.passes==std::vector<P>{P::dualAmp2},"A muted rig's pass ran with every rig step on");
    // Muted while its pass listens: kept in the pass.
    const auto during=runDualAmp(blend,{.05f,.05f},{},setRaw("mute1",1));
    check(during.status.passes==std::vector<P>{P::dualAmp1,P::dualAmp2} && stepOf(during.status,P::dualAmp1)->result=="kept (this rig is muted)" && same(during.values,during.before,during.gainID[0]),
          "A rig muted while listening was set");
    // Dual Crossover.
    auto cross=base;cross.crossGain=cross.crossChannel=cross.crossTrim=true;
    const auto crossMuted=runDualAmp(cross,{.05f,.05f},both(setRaw("dualtype",1),setRaw("mute2",1)));
    check(crossMuted.status.passes==std::vector<P>{P::crossGain1} && same(crossMuted.values,crossMuted.before,crossMuted.gainID[1]),"A muted Crossover rig was listened to or set");
    for(const P row:{P::crossChannel2,P::crossGain2,P::crossTrim2})check(stepOf(crossMuted.status,row) && stepOf(crossMuted.status,row)->result=="kept (this rig is muted)","A muted Crossover rig's row lacks the reason");
    // Matrix: a muted lane, and a lane silenced by another lane's solo (the LOW lane's counts).
    auto matrix=base;matrix.matrixGain=matrix.matrixChannel=matrix.matrixTrim=true;
    const auto laneMuted=runMatrixGain(matrix,{.05f,.05f},setRaw("mute3",1));
    check(laneMuted.status.passes==std::vector<P>{P::matrixGain2} && same(laneMuted.values,laneMuted.before,laneMuted.gainID[1]) && stepOf(laneMuted.status,P::matrixGain2)->changed,"A muted Matrix lane was listened to or set, or stopped the other lane");
    for(const P row:{P::matrixChannel3,P::matrixGain3,P::matrixTrim3})check(stepOf(laneMuted.status,row) && stepOf(laneMuted.status,row)->result=="kept (this lane is muted)","A muted Matrix lane's row lacks the reason");
    // With the input pass open (a run with nothing to listen to does not start), as the other steps' kept-without-a-pass tests.
    auto matrixOpen=matrix;matrixOpen.locks=Controller::lockCompressor;
    const auto lowSolo=runMatrixGain(matrixOpen,{.05f,.05f},setRaw("solo1",1));
    check(lowSolo.status.passes==std::vector<P>{P::input} && stepOf(lowSolo.status,P::matrixGain2)->result=="kept (another lane is soloed)" && stepOf(lowSolo.status,P::matrixGain3)->result=="kept (another lane is soloed)"
          && same(lowSolo.values,lowSolo.before,lowSolo.gainID[0]),[&]{std::string d="Lanes silenced by the LOW lane's solo were listened to or set:";for(auto x:lowSolo.status.passes)d+=" "+std::to_string(int(x));d+=" phase "+std::to_string(int(lowSolo.status.phase))+" steps "+std::to_string(lowSolo.status.steps.size());for(const auto& st:lowSolo.status.steps)d+=" ["+std::to_string(int(st.phase))+":"+st.result.toStdString()+"]";for(const P row:{P::matrixChannel2,P::matrixGain2,P::matrixTrim2,P::matrixGain3})d+=" | "+(stepOf(lowSolo.status,row)?stepOf(lowSolo.status,row)->result.toStdString():std::string("no step"));return d;}());
    const auto midSolo=runMatrixGain(matrix,{.05f,.05f},setRaw("solo2",1));
    check(midSolo.status.passes==std::vector<P>{P::matrixGain2} && stepOf(midSolo.status,P::matrixGain2)->changed && stepOf(midSolo.status,P::matrixGain3)->result=="kept (another lane is soloed)","A soloed Matrix lane was not the only one set");
    // The Amp lock still reads "locked", not the mute.
    auto lockedBlend=blend;lockedBlend.locks=Controller::lockCompressor|Controller::lockAmp;
    const auto lockedRun=runDualAmp(lockedBlend,{.05f,.05f},setRaw("mute1",1));
    check(stepOf(lockedRun.status,P::dualAmp1) && stepOf(lockedRun.status,P::dualAmp1)->result=="kept (locked)","A locked step of a muted rig does not read locked");
    std::cout<<"PASS auto set lane mute: a muted or solo-silenced Dual Blend rig, Dual Crossover rig and Matrix lane gets no pass and the reason on each of its rows; the others are set; muted while listening; both soloed; locked\n";
}
