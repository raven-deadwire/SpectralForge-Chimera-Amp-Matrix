#pragma once
// Phase 33: the GAIN and tone of both Dual Blend rigs' heads (docs/AUTO_SET_PHASE33.md).
// Included by AutoSetTests.cpp after the amp tests (their helpers are used here).

// The rig tap hears the whole rig by default and one Dual rig alone when Auto Set asks for it.
void dualAmpCapture() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    std::mt19937 rng{330};const auto x=targetNoise(false,48000,1,rng);
    const auto capture=[&](int mode,int rigLane) {
        setRaw("mode",float(mode));setRaw("dualtype",0);p->setAutoSetRigLane(rigLane);
        juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
        const auto run=[&](int blocks){for(int block=0;block<blocks;++block){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+size_t(block%20)*256,256);p->processBlock(io,midi);}};
        run(60);
        p->autoSetCapture().listen({Tap::cab});run(20);
        std::vector<float> heard;p->autoSetCapture().drainSamples(heard);
        p->autoSetCapture().listen(std::nullopt);
        double energy=0;for(const float v:heard)energy+=double(v)*v;
        return std::pair{heard.size(),energy};
    };
    const size_t all=20*256;
    const auto mix=capture(1,-1),rig1=capture(1,0),rig2=capture(1,1),classicRig=capture(0,0),classicMix=capture(0,-1),matrixRig=capture(2,0);
    check(mix.first==all && rig1.first==all && rig2.first==all && rig1.second>0 && rig2.second>0,"The rig tap was not fed from the Dual rigs ("+std::to_string(mix.first)+", "+std::to_string(rig1.first)+", "+std::to_string(rig2.first)+")");
    check(classicMix.first==all && classicRig.first==0 && matrixRig.first==0,"A Dual rig was fed to the rig tap outside the Dual routing ("+std::to_string(classicRig.first)+", "+std::to_string(matrixRig.first)+")");
    p->releaseResources();
    std::cout<<"PASS auto set dual amp capture: the rig tap hears the whole rig by default and a Dual rig alone on request, in the Dual routing only\n";
}
struct DualAmpRun {std::vector<float> before,values,undone;Controller::Status status;std::array<juce::String,2> gainID;};
// Both Dual Blend rigs on the native head runAmp gives the Classic rig, at the given GAIN fractions.
// Each rig's pass hears the DI going into the amps and that rig's head alone (no cabinet) on the rig tap.
// heads: called once both heads are set and before they are rendered (a knob the pass should hear).
DualAmpRun runDualAmp(const Controller::Options& options,std::array<float,2> gainFraction,const std::function<void(ChimeraProcessor&)>& prepare={},const std::function<void(ChimeraProcessor&)>& during={},
                      const std::function<void(ChimeraProcessor&)>& heads={}) {
    using P=Controller::Phase;
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    constexpr int model=2;
    setRaw("mode",1);setRaw("dualtype",0);setRaw("dualblend",.5f);setRaw("ampon1",1);setRaw("ampon2",1);
    DualAmpRun result;
    std::mt19937 rng{331};const auto di=playedNotes(48000,8,rng);
    std::array<std::vector<float>,2> post;
    for(int rig=0;rig<2;++rig) {
        const int context=spectralforge::ampNativeContext(1,rig);
        setRaw(spectralforge::ampNativeEnabledID(context),1);setRaw(spectralforge::ampNativeModelID(context),float(model));
        const int channel=p->selectedAmpChannel(rig),route=p->selectedAmpNativeRoute(rig);
        const auto controls=roleControls(model,channel,route);
        result.gainID[size_t(rig)]=spectralforge::ampNativeControlID(context,model,controls[ampGain],channel);
        p->parameters().getParameter(result.gainID[size_t(rig)])->setValueNotifyingHost(gainFraction[size_t(rig)]);
    }
    if(heads)heads(*p);
    for(int rig=0;rig<2;++rig) {
        const int context=spectralforge::ampNativeContext(1,rig);
        const int channel=p->selectedAmpChannel(rig),route=p->selectedAmpNativeRoute(rig);
        const auto controls=roleControls(model,channel,route);
        std::array<float,4> now{};for(int r=0;r<4;++r)if(controls[size_t(r)]>=0)now[size_t(r)]=get(*p,spectralforge::ampNativeControlID(context,model,controls[size_t(r)],channel));
        post[size_t(rig)]=nativeAmpRenderer(p->autoSetAmpState(context),controls,di,48000)(now);
    }
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    result.before=values(*p);
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    P last=P::idle;size_t offset=0;
    for(size_t fed=0;c.busy() && fed<size_t(48000*60);fed+=256) {
        // Each pass starts the DI again, with the rig it listens to on the rig tap.
        const P now=c.current().phase;if(now!=last){last=now;offset=0;}
        if(offset+256>di.size())offset=0;
        const auto& heard=post[size_t(now==P::dualAmp2?1:0)];
        const float* in[]{di.data()+offset};const float* out[]{heard.data()+offset};
        p->autoSetCapture().push(Tap::amp,in,1,256);
        for(auto tap:{Tap::cab,Tap::input,Tap::compressor})p->autoSetCapture().push(tap,tap==Tap::cab?out:in,1,256);
        offset+=256;ms+=1000.*256/48000;c.tick(ms);
        if(during && fed==0)during(*p);
    }
    check(!c.busy(),"Dual amp run did not finish");
    result.values=values(*p);result.status=c.current();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerDualAmp() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto setter=[](std::initializer_list<std::pair<juce::String,float>> pairs){
        std::vector<std::pair<juce::String,float>> list(pairs);
        return [list](ChimeraProcessor& p){for(const auto& [id,v]:list)p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));};};
    check(!Controller::Options{}.dualAmp && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.dualAmp,"The Dual Blend rigs' amp step is on by default");
    Controller::Options dualAmpOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    dualAmpOnly.sync=dualAmpOnly.cabIR=dualAmpOnly.polarity=false;dualAmpOnly.dualAmp=true;
    // Rig 1 far too clean, rig 2 far too driven: each is moved toward the rock target on its own.
    const auto full=runDualAmp(dualAmpOnly,{.05f,.95f});
    const auto* step1=stepOf(full.status,P::dualAmp1);const auto* step2=stepOf(full.status,P::dualAmp2);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::dualAmp1,P::dualAmp2} && step1 && step2 && step1->changed && step1->result.contains("GAIN"),
          "The Dual amp passes did not run per rig or report: "+(step1?step1->result.toStdString():std::string("no step")));
    check(value(full.values,full.gainID[0])>value(full.before,full.gainID[0]),"Rig 1's low GAIN was not raised");
    check(!step2->changed || value(full.values,full.gainID[1])<value(full.before,full.gainID[1]),"Rig 2's high GAIN was raised");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the rigs' heads");
    // The Classic head's parameters are another context: untouched.
    // Strength moves each knob part of the way.
    auto halfOptions=dualAmpOnly;halfOptions.strength=.5f;
    const auto half=runDualAmp(halfOptions,{.05f,.95f});
    const float fullMove=value(full.values,full.gainID[0])-value(full.before,full.gainID[0]),halfMove=value(half.values,half.gainID[0])-value(half.before,half.gainID[0]);
    check(halfMove>0 && halfMove<fullMove,"Strength did not scale rig 1's GAIN move ("+std::to_string(halfMove)+" of "+std::to_string(fullMove)+")");
    // Kept: the Amp lock (no pass), Dual Crossover, Matrix, Classic, an amp that is off or on the classic engine, an edit while listening.
    auto lockedOptions=dualAmpOnly;lockedOptions.locks=Controller::lockCompressor|Controller::lockAmp;
    const auto locked=runDualAmp(lockedOptions,{.05f,.95f});
    check(locked.status.passes==std::vector<P>{P::input} && stepOf(locked.status,P::dualAmp1)->result=="kept (locked)" && stepOf(locked.status,P::dualAmp2)->result=="kept (locked)"
          && value(locked.values,locked.gainID[0])==value(locked.before,locked.gainID[0]),"The Amp lock did not keep the rigs' heads");
    auto openOptions=dualAmpOnly;openOptions.locks=Controller::lockCompressor;
    const auto planned=[&](const std::function<void(ChimeraProcessor&)>& prepare,const char* text,bool both) {
        const auto run=runDualAmp(openOptions,{.05f,.95f},prepare);const auto* s=stepOf(run.status,P::dualAmp1);
        check(s && !s->changed && s->result.contains(text) && (stepOf(run.status,P::dualAmp2)!=nullptr)==both && run.status.passes==std::vector<P>{P::input}
              && value(run.values,run.gainID[0])==value(run.before,run.gainID[0]),std::string("Not kept as '")+text+"': "+(s?s->result.toStdString():std::string("no step")));
    };
    planned(setter({{"dualtype",1}}),"Dual Crossover: the rigs play separate bands",true);
    planned(setter({{"mode",2}}),"Matrix routing: the lanes play separate bands",true);
    planned(setter({{"mode",0}}),"Classic routing: the Amp gain and tone step sets the head",false);
    const auto ampOff=runDualAmp(dualAmpOnly,{.05f,.95f},setter({{"ampon1",0}}));
    check(stepOf(ampOff.status,P::dualAmp1)->result=="kept (the amp is switched off)" && stepOf(ampOff.status,P::dualAmp2) && value(ampOff.values,ampOff.gainID[0])==value(ampOff.before,ampOff.gainID[0]),"A rig whose amp is off was set");
    if(!spectralforge::ampRequiresNative(2)) {
        const auto classicEngine=runDualAmp(dualAmpOnly,{.05f,.95f},setter({{spectralforge::ampNativeEnabledID(spectralforge::ampNativeContext(1,1)),0}}));
        check(stepOf(classicEngine.status,P::dualAmp2)->result.contains("classic amp engine") && stepOf(classicEngine.status,P::dualAmp1)->changed,"A rig on the classic engine was set, or stopped the other rig");
    }
    const auto edited=runDualAmp(dualAmpOnly,{.05f,.95f},{},setter({{"dualtype",1}}));
    check(stepOf(edited.status,P::dualAmp1)->result=="kept (routing, Dual type, amp, channel or input changed while listening)" && value(edited.values,edited.gainID[0])==value(edited.before,edited.gainID[0]),"The Dual type edited while listening was ignored");
    std::cout<<"MEASURE auto set dual amp controller: rig 1: "<<step1->result<<" | rig 2: "<<step2->result<<'\n';
    std::cout<<"PASS auto set dual amp controller: off by default; a pass per Dual Blend rig; low GAIN raised, high GAIN not raised; strength; Amp lock; Crossover, Matrix, Classic, amp off, classic engine, edited; undo\n";
}
