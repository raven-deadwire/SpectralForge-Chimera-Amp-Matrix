#pragma once
// Phase 32: the boost pedal in a Dual Blend, judged on the two rigs' mix (docs/AUTO_SET_PHASE32.md).
// Included by AutoSetTests.cpp after the boost tests (their helpers are used here).

// MSVC only: compiled without optimisation, as the other tests added since phase 29 (see AutoSetLaneIRTests.h).
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("",off)
#endif
void dualBoostRules() {
    std::mt19937 rng{320};const auto di=playedNotes(48000,6,rng);
    const auto head=drivenHead();
    const auto single=nativeBoostRenderer(head,di,48000);
    // A mix of one rig alone, or of two equal rigs, is that rig.
    const BoostSetting setting{.2f,.5f,.8f};
    for(const auto& s:{std::optional<BoostSetting>{},std::optional<BoostSetting>{setting}}) {
        const auto one=single(s),alone=blendedBoostRenderer(single,single,1.f,0.f)(s),equal=blendedBoostRenderer(single,single,.5f,.5f)(s),opposed=blendedBoostRenderer(single,single,.5f,-.5f)(s);
        float worstAlone=0,worstEqual=0,worstOpposed=0;
        for(size_t n=0;n<one.size();++n){worstAlone=std::max(worstAlone,std::abs(alone[n]-one[n]));worstEqual=std::max(worstEqual,std::abs(equal[n]-one[n]));worstOpposed=std::max(worstOpposed,std::abs(opposed[n]));}
        check(worstAlone<1e-6f && worstEqual<1e-5f && worstOpposed<1e-6f,"The blended render is not the weighted sum of the rigs");
    }
    // Two driven rigs: the mix is loose and a boost tightens it, as for one rig.
    const auto one=analyseBoost(di,48000,single,false,Style::metal),mix=analyseBoost(di,48000,blendedBoostRenderer(single,single,.5f,.5f),false,Style::metal);
    check(one.engaged && mix.engaged && near(mix.lowExcessBeforeDb,one.lowExcessBeforeDb,.05f) && mix.setting.drive==one.setting.drive && mix.setting.level==one.setting.level,"Two equal rigs were not judged as one");
    std::cout<<"PASS auto set dual boost rules: the blended render is the weighted sum of the rigs; two equal driven rigs are judged as one\n";
}
void controllerDualBoost() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    check(!Controller::Options{}.dualBoost && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.dualBoost,"The Dual Blend boost is on by default");
    Controller::Options boostOnly{1,false,Style::metal,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,true,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    boostOnly.sync=boostOnly.cabIR=boostOnly.polarity=false;
    auto dualOptions=boostOnly;dualOptions.dualBoost=true;
    constexpr int model=2;
    // A Dual routing with both rigs on the driven native head runBoost gives the Classic rig.
    const auto dualRig=[&](int routing,bool crossover,std::array<bool,2> native) {
        return [=](ChimeraProcessor& p) {
            const auto setRaw=[&](const juce::String& id,float v){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
            setRaw("mode",float(routing));setRaw("dualtype",crossover?1.f:0.f);setRaw("dualblend",.5f);setRaw("ampon1",1);setRaw("ampon2",1);
            for(int lane=0;lane<2;++lane) {
                const int context=spectralforge::ampNativeContext(routing,lane);
                setRaw(spectralforge::ampNativeModelID(context),float(model));setRaw(spectralforge::ampNativeEnabledID(context),native[size_t(lane)]?1.f:0.f);
                const int channel=p.selectedAmpChannel(lane),route=p.selectedAmpNativeRoute(lane);
                const auto controls=roleControls(model,channel,route);
                p.parameters().getParameter(spectralforge::ampNativeControlID(context,model,controls[ampGain],channel))->setValueNotifyingHost(.8f);
            }
        };
    };
    // Off by default: a Dual routing keeps the boost and names the option.
    const auto off=runBoost(boostOnly,dualRig(1,false,{true,true}));
    check(stepOf(off.status,P::boost) && stepOf(off.status,P::boost)->result=="kept (Dual and Matrix routing: add a boost in the full editor, or switch on the boost's Dual Blend option)","A Dual routing without the option was not kept with the reason");
    // Dual Blend with the option: the mix of the two driven rigs gets the pedal.
    const auto full=runBoost(dualOptions,dualRig(1,false,{true,true}));
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::boost);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::boost} && step && step->changed && step->result.contains("Green Drive in slot 5") && step->result.contains("the rigs' mix: low end"),
          "The Dual Blend boost did not run or report: "+(step?step->result.toStdString():std::string("no step")));
    const auto id=[](int c){return spectralforge::pedalControlID(4,boostPedalModel,c);};
    check(a.engaged && near(value(full.values,spectralforge::pedalModelID(4)),float(boostPedalModel),.01f) && value(full.values,spectralforge::pedalBypassID(4,boostPedalModel))<.5f
          && near(value(full.values,id(0)),a.setting.drive,.011f) && near(value(full.values,id(2)),a.setting.level,.011f),"Applied boost does not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not remove the boost");
    // Kept, with the reason: Dual Crossover, a rig on the classic engine, the Matrix routing.
    const auto reason=[&](const std::function<void(ChimeraProcessor&)>& prepare,const char* text) {
        const auto run=runBoost(dualOptions,prepare);const auto* s=stepOf(run.status,P::boost);
        check(s && !s->changed && s->result.contains(text) && value(run.values,spectralforge::pedalModelID(4))==value(run.before,spectralforge::pedalModelID(4)),std::string("Not kept as '")+text+"': "+(s?s->result.toStdString():std::string("no step")));
    };
    reason(dualRig(1,true,{true,true}),"Dual Crossover: the rigs play separate bands");
    if(!spectralforge::ampRequiresNative(model))reason(dualRig(1,false,{true,false}),"rig 2 uses the classic amp engine");
    reason(dualRig(2,false,{true,true}),"Matrix routing: add a boost in the full editor");
    // The Classic routing is unchanged by the option.
    const auto classic=runBoost(dualOptions);
    check(stepOf(classic.status,P::boost)->changed && !stepOf(classic.status,P::boost)->result.contains("the rigs' mix"),"The option changed the Classic boost");
    std::cout<<"MEASURE auto set dual boost controller: "<<step->result<<'\n';
    std::cout<<"PASS auto set dual boost controller: off by default (kept with the option named); Dual Blend judged on the mix; Crossover, classic engine and Matrix kept; Classic unchanged; undo\n";
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("",on)
#endif
