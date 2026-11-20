#pragma once
// Phase 40: the GAIN of the Matrix MID and HIGH lanes' heads (docs/AUTO_SET_PHASE40.md).
// Included by AutoSetTests.cpp after the Dual amp tests (their helpers are used here).

// In the Matrix routing, the rig tap hears one lane alone and the amp tap the band that lane's amp
// receives when Auto Set asks for that lane, in step; otherwise both hear what they did before.
void matrixGainCapture() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    std::mt19937 rng{400};const auto x=targetNoise(false,48000,1,rng);
    struct Heard {size_t amp{},rig{};double energy{},rigEnergy{};};
    const auto capture=[&](int mode,int lane) {
        setRaw("mode",float(mode));setRaw("dualtype",0);p->setAutoSetRigLane(lane);
        juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
        const auto run=[&](int blocks){for(int block=0;block<blocks;++block){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+size_t(block%20)*256,256);p->processBlock(io,midi);}};
        run(60);
        p->autoSetCapture().listen({Tap::amp,Tap::cab});run(20);
        std::vector<float> amp,rig;p->autoSetCapture().drainAmpSamples(amp);p->autoSetCapture().drainSamples(rig);
        p->autoSetCapture().listen(std::nullopt);
        Heard h{amp.size(),rig.size(),0,0};for(const float v:amp)h.energy+=double(v)*v;for(const float v:rig)h.rigEnergy+=double(v)*v;
        return h;
    };
    const size_t all=20*256;
    const auto whole=capture(2,-1),mid=capture(2,1),high=capture(2,2),dualLane3=capture(1,2);
    check(whole.amp==all && whole.rig==all && mid.amp==all && mid.rig==all && high.amp==all && high.rig==all,"The amp tap and the rig tap were not filled in step in the Matrix ("
          +std::to_string(mid.amp)+", "+std::to_string(mid.rig)+", "+std::to_string(high.amp)+", "+std::to_string(high.rig)+")");
    check(mid.energy>0 && high.energy>0 && mid.energy<.95*whole.energy && high.energy<.95*whole.energy && std::abs(mid.energy-high.energy)>.02*whole.energy,
          "The amp tap did not hear each Matrix lane's own band ("+std::to_string(mid.energy/whole.energy)+", "+std::to_string(high.energy/whole.energy)+" of the input)");
    check(mid.rigEnergy>0 && high.rigEnergy>0 && std::abs(mid.rigEnergy-high.rigEnergy)>1e-3*(mid.rigEnergy+high.rigEnergy),"The rig tap did not hear each Matrix lane alone");
    check(dualLane3.rig==0,"A third lane was fed to the rig tap in the Dual routing");
    p->setAutoSetRigLane(-1);p->releaseResources();
    std::cout<<"MEASURE auto set matrix gain capture: the MID lane's amp receives "<<mid.energy/whole.energy<<" and the HIGH lane's "<<high.energy/whole.energy<<" of the input's energy\n";
    std::cout<<"PASS auto set matrix gain capture: the amp tap hears a Matrix lane's band and the rig tap that lane alone on request, in step; a lane the Dual routing lacks feeds nothing\n";
}
struct MatrixGainRun {std::vector<float> before,values,undone;Controller::Status status;std::array<juce::String,2> gainID;};
// The Matrix MID and HIGH lanes on the native head runAmp gives the Classic rig, at the given GAIN fractions.
// Each lane's pass hears the DI as that lane's band and that lane's head alone (no cabinet) on the rig tap.
// heads: called once both heads are set and before they are rendered (a channel the pass should hear).
MatrixGainRun runMatrixGain(const Controller::Options& options,std::array<float,2> gainFraction,const std::function<void(ChimeraProcessor&)>& prepare={},const std::function<void(ChimeraProcessor&)>& during={},
                            const std::function<void(ChimeraProcessor&)>& heads={}) {
    using P=Controller::Phase;
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    constexpr int model=2;
    setRaw("mode",2);setRaw("ampon2",1);setRaw("ampon3",1);
    MatrixGainRun result;
    std::mt19937 rng{401};const auto di=playedNotes(48000,8,rng);
    std::array<std::vector<float>,2> post;
    for(int index=0;index<2;++index) {
        const int lane=index+1,context=spectralforge::ampNativeContext(2,lane);
        setRaw(spectralforge::ampNativeEnabledID(context),1);setRaw(spectralforge::ampNativeModelID(context),float(model));
        const int channel=p->selectedAmpChannel(lane),route=p->selectedAmpNativeRoute(lane);
        const auto controls=roleControls(model,channel,route);
        result.gainID[size_t(index)]=spectralforge::ampNativeControlID(context,model,controls[ampGain],channel);
        p->parameters().getParameter(result.gainID[size_t(index)])->setValueNotifyingHost(gainFraction[size_t(index)]);
    }
    if(heads)heads(*p);
    for(int index=0;index<2;++index) {
        const int lane=index+1,context=spectralforge::ampNativeContext(2,lane);
        const int channel=p->selectedAmpChannel(lane),route=p->selectedAmpNativeRoute(lane);
        const auto controls=roleControls(model,channel,route);
        std::array<float,4> now{};for(int r=0;r<4;++r)if(controls[size_t(r)]>=0)now[size_t(r)]=get(*p,spectralforge::ampNativeControlID(context,model,controls[size_t(r)],channel));
        post[size_t(index)]=nativeAmpRenderer(p->autoSetAmpState(context),controls,di,48000)(now);
    }
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    result.before=values(*p);
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    P last=P::idle;size_t offset=0;
    for(size_t fed=0;c.busy() && fed<size_t(48000*60);fed+=256) {
        // Each pass starts the DI again, with the lane it listens to on the rig tap.
        const P now=c.current().phase;if(now!=last){last=now;offset=0;}
        if(offset+256>di.size())offset=0;
        const auto& heard=post[size_t(now==P::matrixGain3?1:0)];
        const float* in[]{di.data()+offset};const float* out[]{heard.data()+offset};
        p->autoSetCapture().push(Tap::amp,in,1,256);
        for(auto tap:{Tap::cab,Tap::input,Tap::compressor})p->autoSetCapture().push(tap,tap==Tap::cab?out:in,1,256);
        offset+=256;ms+=1000.*256/48000;c.tick(ms);
        if(during && fed==0)during(*p);
    }
    check(!c.busy(),"Matrix gain run did not finish");
    result.values=values(*p);result.status=c.current();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerMatrixGain() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto setRaw=[](const juce::String& id,float v){return [id,v](ChimeraProcessor& p){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};};
    check(!Controller::Options{}.matrixGain && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.matrixGain,"The Matrix lanes' amp gain step is on by default");
    Controller::Options gainOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    gainOnly.sync=gainOnly.cabIR=gainOnly.polarity=false;gainOnly.matrixGain=true;
    // MID far too clean, HIGH far too driven: each GAIN is moved toward the rock drive on its own band.
    const auto full=runMatrixGain(gainOnly,{.05f,.95f});
    const auto* step2=stepOf(full.status,P::matrixGain2);const auto* step3=stepOf(full.status,P::matrixGain3);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::matrixGain2,P::matrixGain3} && step2 && step3 && step2->changed && step2->result.contains("GAIN"),
          "The Matrix gain passes did not run per lane or report: "+(step2?step2->result.toStdString():std::string("no step")));
    check(value(full.values,full.gainID[0])>value(full.before,full.gainID[0]),"The MID lane's low GAIN was not raised");
    check(!step3->changed || value(full.values,full.gainID[1])<value(full.before,full.gainID[1]),"The HIGH lane's high GAIN was raised");
    for(const auto* step:{step2,step3})check(!step->result.contains("BASS") && !step->result.contains("MID") && !step->result.contains("TREBLE"),"A Matrix lane's tone knob was set: "+step->result.toStdString());
    size_t moved=0;for(size_t i=0;i<full.before.size();++i)moved+=near(full.values[i],full.before[i],1e-6f)?0:1;
    check(moved==size_t(1+(step3->changed?1:0)),"More than the lanes' GAIN was written ("+std::to_string(moved)+" parameters)");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the lanes' GAIN");
    // Strength moves the knob part of the way.
    auto halfOptions=gainOnly;halfOptions.strength=.5f;
    const auto half=runMatrixGain(halfOptions,{.05f,.95f});
    const float fullMove=value(full.values,full.gainID[0])-value(full.before,full.gainID[0]),halfMove=value(half.values,half.gainID[0])-value(half.before,half.gainID[0]);
    check(halfMove>0 && halfMove<fullMove,"Strength did not scale the MID lane's GAIN move ("+std::to_string(halfMove)+" of "+std::to_string(fullMove)+")");
    // Kept without a pass: the Amp lock, Classic, Dual Blend, Dual Crossover.
    auto lockedOptions=gainOnly;lockedOptions.locks=Controller::lockCompressor|Controller::lockAmp;
    const auto locked=runMatrixGain(lockedOptions,{.05f,.95f});
    check(locked.status.passes==std::vector<P>{P::input} && stepOf(locked.status,P::matrixGain2)->result=="kept (locked)" && stepOf(locked.status,P::matrixGain3)->result=="kept (locked)"
          && value(locked.values,locked.gainID[0])==value(locked.before,locked.gainID[0]),"The Amp lock did not keep the Matrix lanes' GAIN");
    auto openOptions=gainOnly;openOptions.locks=Controller::lockCompressor;
    const auto planned=[&](const std::function<void(ChimeraProcessor&)>& prepare,const char* text,bool both) {
        const auto run=runMatrixGain(openOptions,{.05f,.95f},prepare);const auto* s=stepOf(run.status,P::matrixGain2);
        check(s && !s->changed && s->result.contains(text) && (stepOf(run.status,P::matrixGain3)!=nullptr)==both && run.status.passes==std::vector<P>{P::input}
              && value(run.values,run.gainID[0])==value(run.before,run.gainID[0]),std::string("Not kept as '")+text+"': "+(s?s->result.toStdString():std::string("no step")));
    };
    planned(setRaw("mode",0),"Classic routing: the Amp gain and tone step sets the head",false);
    planned([&](ChimeraProcessor& p){setRaw("mode",1)(p);setRaw("dualtype",0)(p);},"Dual Blend: the Amp gain and tone of both Dual Blend rigs step sets the heads",true);
    planned([&](ChimeraProcessor& p){setRaw("mode",1)(p);setRaw("dualtype",1)(p);},"Dual Crossover: the Amp gain of both Dual Crossover rigs step sets the heads",true);
    // Kept in the pass: an amp that is off (the other lane is still set), the routing edited while listening.
    const auto ampOff=runMatrixGain(gainOnly,{.05f,.95f},setRaw("ampon2",0));
    check(stepOf(ampOff.status,P::matrixGain2)->result=="kept (the amp is switched off)" && stepOf(ampOff.status,P::matrixGain3) && value(ampOff.values,ampOff.gainID[0])==value(ampOff.before,ampOff.gainID[0]),"A lane whose amp is off was set");
    const auto edited=runMatrixGain(gainOnly,{.05f,.95f},{},setRaw("mode",1));
    check(stepOf(edited.status,P::matrixGain2)->result=="kept (routing, amp, channel or input changed while listening)" && value(edited.values,edited.gainID[0])==value(edited.before,edited.gainID[0]),"The routing edited while listening was ignored");
    std::cout<<"MEASURE auto set matrix gain controller: MID: "<<step2->result<<" | HIGH: "<<step3->result<<'\n';
    std::cout<<"PASS auto set matrix gain controller: off by default; a pass per Matrix MID and HIGH lane; GAIN only; strength; Amp lock; Classic, Blend, Crossover; amp off; edited; undo\n";
}
