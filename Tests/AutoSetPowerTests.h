#pragma once
// Phase 12: PRESENCE and RESONANCE. Included by AutoSetTests.cpp after its helpers.

void lowShelve(std::vector<float>& x,double frequency,double gain,double rate) {
    const auto c=spectralforge::eqCoefficients(1,frequency,gain,.707,rate);double z1=0,z2=0;
    for(auto& v:x){const double y=c.b0*v+z1;z1=c.b1*v-c.a1*y+z2;z2=c.b2*v-c.a2*y;v=float(y);}
}
// Heads whose default channel shows both a PRESENCE and a RESONANCE (or DEPTH) knob.
std::vector<int> powerProbeModels() {
    std::vector<int> models;
    for(int model=0;model<spectralforge::ampModelCount;++model) {
        const auto state=spectralforge::defaultAmpNativeState(model);
        if(powerControl(model,state.channel,powerPresence)>=0 && powerControl(model,state.channel,powerResonance)>=0)models.push_back(model);
    }
    return models;
}
void powerRules() {
    const auto models=powerProbeModels();
    check(models.size()>=2,"Fewer than two heads have PRESENCE and RESONANCE on their default channel");
    int moved=0;
    for(size_t i=0;i<models.size() && i<4;++i) {
        const int model=models[i];const double rate=i==0?44100.:48000.;
        auto state=spectralforge::defaultAmpNativeState(model);
        const std::array<int,2> controls{powerControl(model,state.channel,powerPresence),powerControl(model,state.channel,powerResonance)};
        std::mt19937 rng{120+unsigned(i)};const auto di=playedNotes(rate,ampRenderSeconds,rng);
        const auto render=nativePowerRenderer(state,{},controls,di,rate);
        std::array<AmpKnob,2> knobs{};
        for(int role=0;role<2;++role){const auto& c=spectralforge::ampNativePanel(model).controls[size_t(controls[size_t(role)])];knobs[size_t(role)]={true,state.values[size_t(controls[size_t(role)])],c.minimum,c.maximum};}
        const std::array<float,2> now{knobs[0].value,knobs[1].value};
        const auto excessAt=[&](int role,float value){auto v=now;v[size_t(role)]=value;return powerExcessDb(measureSpectrum(render(v),rate,false).bandsDb,role,false,Style::rock);};
        // Each knob moves its own balance.
        for(int role=0;role<2;++role) {
            const float low=excessAt(role,knobs[size_t(role)].minimum),high=excessAt(role,knobs[size_t(role)].maximum);
            check(high>low+1.f,std::string(powerRoleNames[role])+" does not raise its balance on "+spectralforge::ampInfo(model).name);
        }
        const auto base=render(now);
        const std::vector<float> silence(base.size());
        check(!analysePower(di,silence,base,rate,knobs,render,false).playingDetected,"Silence reported as playing");
        const auto analyse=[&](const std::vector<float>& post){return analysePower(di,post,base,rate,knobs,render,false,Style::rock);};
        const auto withinLimits=[&](const PowerResult& r) {
            for(int role=0;role<2;++role) {
                const auto& k=knobs[size_t(role)];const float range=k.maximum-k.minimum;
                check(std::abs(r.proposed[size_t(role)].value-k.value)<=powerMaxMove*range+1e-4f,"A knob moved further than the limit");
                // A knob that moved brings its own balance closer.
                if(r.changed(role))check(powerCost(r.excessAfterDb[size_t(role)],role,false,Style::rock)<powerCost(r.excessBeforeDb[size_t(role)],role,false,Style::rock),
                                         std::string(powerRoleNames[role])+" moved without bringing its balance closer");
            }
            // Together the two balances never get worse.
            const auto total=[&](const std::array<float,2>& e){return powerCost(e[0],0,false,Style::rock)+powerCost(e[1],1,false,Style::rock);};
            check(total(r.excessAfterDb)<=total(r.excessBeforeDb)+1e-4f,"PRESENCE and RESONANCE together moved away from the target");
            if(r.changedKnobs()==0)check(r.excessAfterDb==r.excessBeforeDb,"Unchanged knobs report a new balance");
        };
        // Too bright: PRESENCE comes down. Too dark: it goes up.
        auto bright=base;shelve(bright,2500,9,rate);
        auto dark=base;shelve(dark,2500,-9,rate);
        const auto lessBite=analyse(bright),moreBite=analyse(dark);
        check(lessBite.playingDetected && moreBite.playingDetected,"The amp's output was not heard");
        check(lessBite.excessBeforeDb[powerPresence]>moreBite.excessBeforeDb[powerPresence]+6.f,"The bite measure does not see a +-9 dB shelf");
        check(lessBite.proposed[powerPresence].value<=knobs[0].value && moreBite.proposed[powerPresence].value>=knobs[0].value,"PRESENCE moved the wrong way");
        // Boomy: RESONANCE comes down.
        auto boomy=base;lowShelve(boomy,120,9,rate);
        const auto lessLow=analyse(boomy);
        check(lessLow.proposed[powerResonance].value<=knobs[1].value,"RESONANCE rose on a boomy sound");
        for(const auto& r:{lessBite,moreBite,lessLow})withinLimits(r);
        moved+=lessBite.changed(powerPresence)+moreBite.changed(powerPresence)+lessLow.changed(powerResonance);
        // A knob the channel does not show is left alone.
        auto partial=knobs;partial[powerResonance].present=false;
        const auto noResonance=analysePower(di,boomy,base,rate,partial,render,false,Style::rock);
        check(!noResonance.changed(powerResonance),"A missing RESONANCE knob was changed");
        std::cout<<"MEASURE power rules "<<spectralforge::ampInfo(model).name<<" "<<rate/1000<<" kHz: bright PRESENCE "<<knobs[0].value<<" > "<<lessBite.proposed[0].value
                 <<" (bite "<<lessBite.excessBeforeDb[0]<<" > "<<lessBite.excessAfterDb[0]<<" dB); dark > "<<moreBite.proposed[0].value<<" ("<<moreBite.excessBeforeDb[0]<<" > "<<moreBite.excessAfterDb[0]
                 <<" dB); boomy RESONANCE "<<knobs[1].value<<" > "<<lessLow.proposed[1].value<<" (low end "<<lessLow.excessBeforeDb[1]<<" > "<<lessLow.excessAfterDb[1]<<" dB)\n";
    }
    check(moved>=3,"The presence and resonance rules hardly ever moved a knob");
    // Driven guitar: too much low end costs double; clean and bass do not.
    check(powerCost(2,powerResonance,false,Style::rock)==4 && powerCost(2,powerResonance,false,Style::clean)==2 && powerCost(2,powerResonance,true,Style::metal)==2
          && powerCost(-2,powerResonance,false,Style::metal)==2 && powerCost(2,powerPresence,false,Style::metal)==2,"Loose low end weighting");
    std::cout<<"PASS auto set power rules: PRESENCE and RESONANCE move their balances on "<<std::min<size_t>(models.size(),4)<<" heads, silence, bright/dark/boomy, move limits, missing knob, loose low-end weighting\n";
}
void controllerPower() {
    using P=Controller::Phase;
    const auto models=powerProbeModels();
    // runAmp plays model 2 on its default channel; this test needs both knobs there.
    check(std::find(models.begin(),models.end(),2)!=models.end(),"The amp controller head has no PRESENCE and RESONANCE");
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto presenceID=[](ChimeraProcessor& p){return spectralforge::ampNativeControlID(0,2,powerControl(2,p.selectedAmpChannel(0),powerPresence),p.selectedAmpChannel(0));};
    juce::String presence;
    // The pass hears the head without a cabinet, so its top end is bright;
    // with PRESENCE at maximum, Auto Set brings it down.
    const auto bright=[&](ChimeraProcessor& p){presence=presenceID(p);p.parameters().getParameter(presence)->setValueNotifyingHost(1);};
    const Controller::Options powerOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,true};
    const auto full=runAmp(powerOnly,.3f,bright);
    const auto& r=full.power;const auto* step=stepOf(full.status,P::power);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::amp} && !stepOf(full.status,P::amp) && step && step->changed && step->result.contains("PRESENCE"),
          "Presence pass did not run alone or report");
    check(r.playingDetected && r.changed(powerPresence) && r.proposed[powerPresence].value<r.before[powerPresence].value
          && std::abs(r.excessAfterDb[powerPresence])<std::abs(r.excessBeforeDb[powerPresence]),"A bright head's PRESENCE was not lowered toward the target");
    check(near(value(full.values,presence),r.proposed[powerPresence].value,.011f),"Applied PRESENCE does not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore PRESENCE and RESONANCE");
    auto halfOptions=powerOnly;halfOptions.strength=.5f;
    const auto half=runAmp(halfOptions,.3f,bright);
    check(near(value(half.values,presence),half.power.before[powerPresence].value+.5f*(half.power.proposed[powerPresence].value-half.power.before[powerPresence].value),.011f),"Strength did not scale the PRESENCE change");
    // Its own lock keeps the knobs and skips the pass; the amp lock alone does not.
    auto lockedOptions=powerOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockPower;
    const auto locked=runAmp(lockedOptions,.3f,bright);
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::power) && stepOf(locked.status,P::power)->result=="kept (locked)"
          && value(locked.values,presence)==value(locked.before,presence),"Presence lock did not keep the knobs");
    auto both=powerOnly;both.amp=true;both.locks|=Controller::lockAmp;
    const auto ampLocked=runAmp(both,.3f,bright);
    check(ampLocked.status.passes==std::vector<P>{P::amp} && stepOf(ampLocked.status,P::amp)->result=="kept (locked)" && value(ampLocked.values,full.gainID)==value(ampLocked.before,full.gainID)
          && stepOf(ampLocked.status,P::power)->changed,"Amp lock stopped the presence step or the amp step changed GAIN");
    // Both steps: one pass, the amp row then the presence row; the second renders on the first's GAIN.
    auto together=powerOnly;together.amp=true;
    const auto run=runAmp(together,.05f,bright);
    const auto& steps=run.status.steps;
    const auto ampRow=std::find_if(steps.begin(),steps.end(),[](const auto& s){return s.phase==P::amp;}),powerRow=std::find_if(steps.begin(),steps.end(),[](const auto& s){return s.phase==P::power;});
    check(run.status.passes==std::vector<P>{P::amp} && ampRow!=steps.end() && powerRow!=steps.end() && ampRow<powerRow && ampRow->changed && run.analysis.changed(ampGain),"Amp and presence steps did not share one pass in order");
    check(run.power.playingDetected && near(value(run.values,presence),run.power.proposed[powerPresence].value,.011f),"Presence after the amp step does not match its analysis");
    // Dual routing is kept, with the reason.
    const auto dual=runAmp(powerOnly,.3f,[&](ChimeraProcessor& p){bright(p);p.parameters().getParameter("mode")->setValueNotifyingHost(p.parameters().getParameter("mode")->convertTo0to1(1));});
    check(stepOf(dual.status,P::power)->result.contains("Dual and Matrix") && dual.values==dual.before,"Dual routing presence was changed");
    std::cout<<"MEASURE auto set power controller: "<<step->result<<"; with the amp step: "<<powerRow->result<<'\n';
    std::cout<<"PASS auto set power controller: presence step alone, bright head darkened, strength, own lock, amp lock, shared pass with the amp step, Dual kept, undo\n";
}
