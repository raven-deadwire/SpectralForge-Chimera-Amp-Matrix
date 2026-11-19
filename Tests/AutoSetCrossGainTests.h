#pragma once
// Phase 37: the GAIN of both Dual Crossover rigs' heads (docs/AUTO_SET_PHASE37.md).
// Included by AutoSetTests.cpp after the Dual amp tests (runDualAmp is used here).

// In a Dual Crossover, the amp tap hears the band one rig's amp receives when Auto Set asks for
// that rig, in step with the rig tap; otherwise it hears what goes into the amps, as before.
void crossGainCapture() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    std::mt19937 rng{370};const auto x=targetNoise(false,48000,1,rng);
    struct Heard {size_t amp{},rig{};double energy{};};
    const auto capture=[&](int mode,int dualType,int rigLane) {
        setRaw("mode",float(mode));setRaw("dualtype",float(dualType));p->setAutoSetRigLane(rigLane);
        juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
        const auto run=[&](int blocks){for(int block=0;block<blocks;++block){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+size_t(block%20)*256,256);p->processBlock(io,midi);}};
        run(60);
        p->autoSetCapture().listen({Tap::amp,Tap::cab});run(20);
        std::vector<float> amp,rig;p->autoSetCapture().drainAmpSamples(amp);p->autoSetCapture().drainSamples(rig);
        p->autoSetCapture().listen(std::nullopt);
        Heard h{amp.size(),rig.size(),0};for(const float v:amp)h.energy+=double(v)*v;
        return h;
    };
    const size_t all=20*256;
    const auto whole=capture(1,1,-1),low=capture(1,1,0),high=capture(1,1,1),blend=capture(1,0,0),classic=capture(0,1,0);
    check(whole.amp==all && low.amp==all && high.amp==all && low.rig==all && high.rig==all,"The amp tap and the rig tap were not filled in step in a Dual Crossover ("
          +std::to_string(low.amp)+", "+std::to_string(low.rig)+", "+std::to_string(high.amp)+", "+std::to_string(high.rig)+")");
    check(low.energy>0 && high.energy>0 && low.energy<.95*whole.energy && high.energy<.95*whole.energy && std::abs(low.energy-high.energy)>.05*whole.energy,
          "The amp tap did not hear each Crossover rig's own band ("+std::to_string(low.energy/whole.energy)+", "+std::to_string(high.energy/whole.energy)+" of the input)");
    check(blend.amp==all && std::abs(blend.energy-whole.energy)<1e-6*whole.energy,"A Dual Blend rig's amp tap no longer hears the whole input");
    check(classic.amp==all && std::abs(classic.energy-whole.energy)<1e-6*whole.energy && classic.rig==0,"The Classic routing's amp tap changed with a rig asked for");
    p->setAutoSetRigLane(-1);p->releaseResources();
    std::cout<<"MEASURE auto set cross gain capture: rig 1's amp receives "<<low.energy/whole.energy<<" and rig 2's "<<high.energy/whole.energy<<" of the input's energy\n";
    std::cout<<"PASS auto set cross gain capture: the amp tap hears a Dual Crossover rig's band on request, in step with the rig tap; the whole input otherwise\n";
}
void controllerCrossGain() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto setRaw=[](const juce::String& id,float v){return [id,v](ChimeraProcessor& p){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};};
    const auto crossover=setRaw("dualtype",1);
    check(!Controller::Options{}.crossGain && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.crossGain,"The Dual Crossover rigs' amp gain step is on by default");
    Controller::Options gainOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    gainOnly.sync=gainOnly.cabIR=gainOnly.polarity=false;gainOnly.crossGain=true;
    // Rig 1 far too clean, rig 2 far too driven: each GAIN is moved toward the rock drive on its own band.
    const auto full=runDualAmp(gainOnly,{.05f,.95f},crossover);
    const auto* step1=stepOf(full.status,P::crossGain1);const auto* step2=stepOf(full.status,P::crossGain2);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::crossGain1,P::crossGain2} && step1 && step2 && step1->changed && step1->result.contains("GAIN") && !stepOf(full.status,P::dualAmp1),
          "The Crossover gain passes did not run per rig or report: "+(step1?step1->result.toStdString():std::string("no step")));
    check(value(full.values,full.gainID[0])>value(full.before,full.gainID[0]),"Rig 1's low GAIN was not raised");
    check(!step2->changed || value(full.values,full.gainID[1])<value(full.before,full.gainID[1]),"Rig 2's high GAIN was raised");
    // Only the two GAIN knobs move: the tone knobs are kept.
    for(const auto* step:{step1,step2})check(!step->result.contains("BASS") && !step->result.contains("MID") && !step->result.contains("TREBLE"),"A Crossover rig's tone knob was set: "+step->result.toStdString());
    size_t moved=0;for(size_t i=0;i<full.before.size();++i)moved+=near(full.values[i],full.before[i],1e-6f)?0:1;
    check(moved==size_t(1+(step2->changed?1:0)),"More than the rigs' GAIN was written ("+std::to_string(moved)+" parameters)");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the rigs' GAIN");
    // Strength moves the knob part of the way.
    auto halfOptions=gainOnly;halfOptions.strength=.5f;
    const auto half=runDualAmp(halfOptions,{.05f,.95f},crossover);
    const float fullMove=value(full.values,full.gainID[0])-value(full.before,full.gainID[0]),halfMove=value(half.values,half.gainID[0])-value(half.before,half.gainID[0]);
    check(halfMove>0 && halfMove<fullMove,"Strength did not scale rig 1's GAIN move ("+std::to_string(halfMove)+" of "+std::to_string(fullMove)+")");
    // Kept without a pass: the Amp lock, Dual Blend, Matrix, Classic.
    auto lockedOptions=gainOnly;lockedOptions.locks=Controller::lockCompressor|Controller::lockAmp;
    const auto locked=runDualAmp(lockedOptions,{.05f,.95f},crossover);
    check(locked.status.passes==std::vector<P>{P::input} && stepOf(locked.status,P::crossGain1)->result=="kept (locked)" && stepOf(locked.status,P::crossGain2)->result=="kept (locked)"
          && value(locked.values,locked.gainID[0])==value(locked.before,locked.gainID[0]),"The Amp lock did not keep the Crossover rigs' GAIN");
    auto openOptions=gainOnly;openOptions.locks=Controller::lockCompressor;
    const auto planned=[&](const std::function<void(ChimeraProcessor&)>& prepare,const char* text,bool both) {
        const auto run=runDualAmp(openOptions,{.05f,.95f},prepare);const auto* s=stepOf(run.status,P::crossGain1);
        check(s && !s->changed && s->result.contains(text) && (stepOf(run.status,P::crossGain2)!=nullptr)==both && run.status.passes==std::vector<P>{P::input}
              && value(run.values,run.gainID[0])==value(run.before,run.gainID[0]),std::string("Not kept as '")+text+"': "+(s?s->result.toStdString():std::string("no step")));
    };
    planned({},"Dual Blend: the Amp gain and tone of both Dual Blend rigs step sets the heads",true);
    planned(setRaw("mode",2),"Matrix routing: the lanes play separate bands",true);
    planned(setRaw("mode",0),"Classic routing: the Amp gain and tone step sets the head",false);
    // With the Dual Blend steps also on, a Crossover runs this step's passes only; the Blend rows say why they are kept.
    auto bothOptions=gainOnly;bothOptions.dualAmp=true;
    const auto both=runDualAmp(bothOptions,{.05f,.95f},crossover);
    check(both.status.passes==std::vector<P>{P::crossGain1,P::crossGain2} && stepOf(both.status,P::dualAmp1) && stepOf(both.status,P::dualAmp1)->result.contains("Dual Crossover")
          && stepOf(both.status,P::crossGain1)->changed,"The Blend and the Crossover step did not share a Crossover run");
    // And in a Blend, the Blend step's passes only.
    const auto blend=runDualAmp(bothOptions,{.05f,.95f});
    check(blend.status.passes==std::vector<P>{P::dualAmp1,P::dualAmp2} && stepOf(blend.status,P::crossGain1)->result.contains("Dual Blend:") && stepOf(blend.status,P::dualAmp1)->changed,
          "The Blend and the Crossover step did not share a Blend run");
    // Kept in the pass: an amp that is off, the Dual type edited while listening.
    const auto ampOff=runDualAmp(gainOnly,{.05f,.95f},[&](ChimeraProcessor& p){crossover(p);setRaw("ampon1",0)(p);});
    check(stepOf(ampOff.status,P::crossGain1)->result=="kept (the amp is switched off)" && stepOf(ampOff.status,P::crossGain2) && value(ampOff.values,ampOff.gainID[0])==value(ampOff.before,ampOff.gainID[0]),"A rig whose amp is off was set");
    const auto edited=runDualAmp(gainOnly,{.05f,.95f},crossover,setRaw("dualtype",0));
    check(stepOf(edited.status,P::crossGain1)->result=="kept (routing, Dual type, amp, channel or input changed while listening)" && value(edited.values,edited.gainID[0])==value(edited.before,edited.gainID[0]),"The Dual type edited while listening was ignored");
    std::cout<<"MEASURE auto set cross gain controller: rig 1: "<<step1->result<<" | rig 2: "<<step2->result<<'\n';
    std::cout<<"PASS auto set cross gain controller: off by default; a pass per Dual Crossover rig; GAIN only; strength; Amp lock; Blend, Matrix, Classic; with the Blend step; amp off; edited; undo\n";
}
