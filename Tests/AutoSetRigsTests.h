#pragma once
// Phase 19: the Dual rig levels. Included by AutoSetTests.cpp after AutoSetDualTests.h (runDual, mixOf).

float rigsFitDb(const std::vector<float>& x,double rate){return float(10*std::log10(std::max(dualFitPower(measureSpectrum(x,rate,false).bandsDb),1e-30)));}
// x scaled to ref's 80 Hz-6.3 kHz level, then by gainDb (targetNoise is peak-normalised, so its level varies).
std::vector<float> rigsAt(std::vector<float> x,const std::vector<float>& ref,double rate,float gainDb) {
    scale(x,float(std::pow(10.,(rigsFitDb(ref,rate)-rigsFitDb(x,rate)+gainDb)/20.)));return x;
}
void rigsRules() {
    const auto fitDb=rigsFitDb;
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{190};
        const auto one=targetNoise(false,rate,6,rng);
        const auto quiet=rigsAt(targetNoise(false,rate,6,rng),one,rate,-6); // rig 2 6 dB down
        const auto up=analyseRigLevels(one,quiet,rate,.5f,false);
        check(up.playingDetected && up.rig1Heard && up.rig2Heard && !up.bass,"Rig levels did not hear both rigs");
        check(up.changed() && std::abs(up.changeDb-6)<=.5f && std::fmod(std::abs(up.changeDb),rigsStepDb)==0 && !up.limited,
              "A rig 2 6 dB down was not raised by about 6 dB on the 0.5 dB grid ("+std::to_string(up.changeDb)+" dB)");
        check(near(up.differenceDb(),fitDb(one,rate)-fitDb(quiet,rate),.01f),"Rig levels are not the rigs' 80 Hz-6.3 kHz power");
        // The mix's level change is the mix itself at the new level.
        auto matched=quiet;scale(matched,float(std::pow(10.,up.changeDb/20.)));
        const size_t n=size_t(rate*rigsSeconds);
        const auto head=[n](const std::vector<float>& x){return std::vector<float>(x.begin(),x.begin()+std::ptrdiff_t(n));};
        check(near(up.levelChangeDb,fitDb(mixOf(head(one),head(matched),.5f),rate)-fitDb(mixOf(head(one),head(quiet),.5f),rate),.01f),"Rig level change does not match the mix");
        // Applied, it settles.
        check(!analyseRigLevels(one,matched,rate,.5f,false).changed(),"Matched rigs moved again");
        // Louder rig 2 comes down.
        auto loud=quiet;scale(loud,4.f); // 6 dB over rig 1
        const auto down=analyseRigLevels(one,loud,rate,.5f,false);
        check(down.changed() && std::abs(down.changeDb+6)<=.5f,"A rig 2 6 dB over was not lowered");
        // Within 1 dB: kept.
        const auto close=rigsAt(quiet,one,rate,-.8f); // 0.8 dB down
        const auto near1=analyseRigLevels(one,close,rate,.5f,false);
        check(std::abs(near1.differenceDb())<=rigsMinDifferenceDb && !near1.changed() && !near1.limited,"Rigs within 1 dB were moved");
        // The control's range, and the 12 dB limit.
        const auto roomy=analyseRigLevels(one,quiet,rate,.5f,false,{-24.f,2.f});
        check(roomy.changeDb==2.f && roomy.limited,"Rig 2 moved past its control's range");
        const auto top=analyseRigLevels(one,quiet,rate,.5f,false,{-24.f,0.f});
        check(!top.changed() && top.limited,"Rig 2 at the top of its range was raised");
        auto faint=quiet;scale(faint,.1f); // about 26 dB down
        const auto far=analyseRigLevels(one,faint,rate,.5f,false,{-48.f,48.f});
        check(far.changeDb==rigsMaxMoveDb && far.limited,"Rig 2 moved more than 12 dB");
        // A silent rig, and silence.
        const std::vector<float> silence(size_t(rate*6));
        const auto lonely=analyseRigLevels(one,silence,rate,.5f,false);
        check(lonely.playingDetected && lonely.rig1Heard && !lonely.rig2Heard && !lonely.changed(),"A silent rig 2 was raised");
        check(!analyseRigLevels(silence,silence,rate,.5f).playingDetected,"Silence reported as playing");
        if(rate==48000.)std::cout<<"MEASURE rigs rules 48 kHz: rig 1 "<<up.rig1Db<<" dB, rig 2 "<<up.rig2Db<<" dB, change "<<up.changeDb<<" dB, mix level "<<up.levelChangeDb
                                <<" dB at 50%; louder rig 2 "<<down.changeDb<<" dB; 0.8 dB apart kept ("<<near1.differenceDb()<<" dB)\n";
    }
    std::cout<<"PASS auto set rigs rules at 44.1/48/96 kHz: quiet rig 2 raised and loud rig 2 lowered on the 0.5 dB grid, fit-range power, exact mix level, settles, within 1 dB kept, control range and 12 dB limit, silent rig, silence\n";
}
void controllerRigs() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    std::mt19937 rng{191};
    const auto one=targetNoise(false,48000,8,rng);
    const auto quiet=rigsAt(targetNoise(false,48000,8,rng),one,48000,-6); // rig 2 6 dB down
    const auto setter=[](std::initializer_list<std::pair<juce::String,float>> pairs){
        std::vector<std::pair<juce::String,float>> list(pairs);
        return [list](ChimeraProcessor& p){for(const auto& [id,v]:list)p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));};};
    const auto native2=spectralforge::ampNativeOutputLevelID(2);
    const auto blendRig=setter({{"mode",1},{"dualtype",0},{"dualblend",.5f}});
    Controller::Options rigsOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,true};
    // Native heads (the default): rig 2's OUTPUT LEVEL moves; LEVEL 2 and rig 1 stay.
    const auto full=runDual(rigsOnly,one,quiet,blendRig);
    const auto& a=full.rigs;const auto* step=stepOf(full.status,P::rigs);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::dual} && step && step->changed && step->result.contains("rig 2 OUTPUT LEVEL") && !stepOf(full.status,P::dual),
          "Rig level step did not run alone in the Dual pass or report");
    check(a.changed() && std::abs(a.changeDb-6)<=.5f && near(value(full.values,native2),value(full.before,native2)+a.changeDb,.011f)
          && value(full.values,"level2")==value(full.before,"level2") && value(full.values,spectralforge::ampNativeOutputLevelID(1))==value(full.before,spectralforge::ampNativeOutputLevelID(1))
          && value(full.values,"dualblend")==value(full.before,"dualblend"),"Applied rig 2 level does not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore rig 2's level");
    // A legacy rig 2: LEVEL 2 moves.
    const auto legacy=runDual(rigsOnly,one,quiet,setter({{"mode",1},{"dualtype",0},{spectralforge::ampNativeEnabledID(2),0}}));
    check(stepOf(legacy.status,P::rigs)->result.contains("rig 2 LEVEL") && near(value(legacy.values,"level2"),legacy.rigs.changeDb,.011f)
          && value(legacy.values,native2)==value(legacy.before,native2),"A legacy rig 2 did not use LEVEL");
    auto halfOptions=rigsOnly;halfOptions.strength=.5f;
    const auto half=runDual(halfOptions,one,quiet,blendRig);
    check(near(value(half.values,native2),.5f*half.rigs.changeDb,.011f),"Strength did not scale the rig level change");
    // The lock, Crossover and Classic routing: no pass, the row says why.
    auto lockedOptions=rigsOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockRigs;
    const auto locked=runDual(lockedOptions,one,quiet,blendRig);
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::rigs) && stepOf(locked.status,P::rigs)->result=="kept (locked)"
          && value(locked.values,native2)==value(locked.before,native2),"Rig level lock did not keep the level");
    auto freeOptions=rigsOnly;freeOptions.locks=Controller::lockInput|Controller::lockGate;
    const auto cross=runDual(freeOptions,one,quiet,setter({{"mode",1},{"dualtype",1}}));
    check(cross.status.passes==std::vector<P>{P::compressor} && stepOf(cross.status,P::rigs)->result.contains("Dual crossover") && value(cross.values,native2)==value(cross.before,native2),
          "Dual crossover ran the rig level step");
    const auto classic=runDual(freeOptions,one,quiet,setter({{"mode",0}}));
    check(classic.status.passes==std::vector<P>{P::compressor} && stepOf(classic.status,P::rigs)->result.contains("Classic or Matrix routing")
          && value(classic.values,native2)==value(classic.before,native2) && value(classic.values,"level2")==value(classic.before,"level2"),"Classic routing ran the rig level step");
    // With the blend step too: the rig row comes first, and the blend hears rig 2 at its new level.
    auto bothOptions=rigsOnly;bothOptions.dual=true;
    auto dark=one;shelve(dark,3000,-10,48000);
    const auto both=runDual(bothOptions,dark,quiet,setter({{"mode",1},{"dualtype",0},{"dualblend",.2f}}));
    const auto& steps=both.status.steps;
    const auto rigRow=std::find_if(steps.begin(),steps.end(),[](const auto& s){return s.phase==P::rigs;}),blendRow=std::find_if(steps.begin(),steps.end(),[](const auto& s){return s.phase==P::dual;});
    check(rigRow!=steps.end() && blendRow!=steps.end() && rigRow<blendRow && rigRow->changed,"The rig level row does not come before the blend row");
    const float applied=value(both.values,native2)-value(both.before,native2);
    auto prescaled=quiet;const float g=juce::Decibels::decibelsToGain(applied);for(auto& v:prescaled)v*=g;
    auto blendOnly=bothOptions;blendOnly.rigs=false;
    const auto reference=runDual(blendOnly,dark,prescaled,setter({{"mode",1},{"dualtype",0},{"dualblend",.2f}}));
    check(both.analysis.playingDetected && near(both.analysis.toneErrorBeforeDb,reference.analysis.toneErrorBeforeDb,1e-3f) && near(both.analysis.proposed,reference.analysis.proposed,1e-4f)
          && near(value(both.values,"dualblend"),reference.analysis.proposed,1e-4f),"The blend step did not hear rig 2 at its matched level");
    // The routing, the Dual type or rig 2's head engine changing while the pass listens keeps everything.
    for(const auto& [id,v]:std::initializer_list<std::pair<juce::String,float>>{{"mode",2},{"dualtype",1},{spectralforge::ampNativeEnabledID(2),0}}) {
        const auto edited=runDual(rigsOnly,one,quiet,blendRig,[id,v](ChimeraProcessor& p){p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));});
        check(stepOf(edited.status,P::rigs)->result.contains("changed while listening") && !stepOf(edited.status,P::rigs)->changed
              && value(edited.values,native2)==value(edited.before,native2) && value(edited.values,"level2")==value(edited.before,"level2"),
              ("An edit of "+id+" while listening did not void the rig level step").toStdString());
    }
    // A level edited by the player while the pass listens is kept.
    const auto manual=runDual(rigsOnly,one,quiet,blendRig,setter({{native2,-3}}));
    check(near(value(manual.values,native2),-3,.011f),"Rig 2's level edited while listening was overwritten");
    // A silent rig 2 keeps the level.
    const auto silent=runDual(rigsOnly,one,std::vector<float>(one.size()),blendRig);
    check(stepOf(silent.status,P::rigs)->result.contains("rig 2 is silent") && silent.values==silent.before,"A silent rig 2 changed the level");
    std::cout<<"MEASURE auto set rigs controller: "<<step->result<<"; with the blend: "<<rigRow->result<<" | "<<blendRow->result<<'\n';
    std::cout<<"PASS auto set rigs controller: rig level step alone in the Dual pass, native OUTPUT LEVEL and legacy LEVEL, strength, lock, Crossover and Classic kept without a pass, blend hears the matched rig 2, routing, type or head edited while listening, manual edit kept, silent rig 2, undo\n";
}
