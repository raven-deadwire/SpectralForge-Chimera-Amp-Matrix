#pragma once
// Phase 21: Dual and Matrix lane polarity. Included by AutoSetTests.cpp after the Dual and Matrix tests (runDual, runMatrix, linkwitzRiley).

void polarityRules() {
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{210};
        const auto x=targetNoise(false,rate,5,rng),other=targetNoise(false,rate,5,rng);
        const auto negated=[](std::vector<float> v,float g){for(auto& s:v)s*=-g;return v;};
        const auto copy=[](std::vector<float> v,float g){for(auto& s:v)s*=g;return v;};
        // Dual blend: rig 2 the same source, opposed: it flips, and the mix adds back.
        const auto opposed=negated(x,.8f);
        const auto dual=analysePolarity({&x,&opposed},{.5f,.5f},rate,false);
        check(dual.playingDetected && dual.heard==std::vector<bool>{true,true} && dual.flip==std::vector<bool>{true}
              && dual.cancellationBeforeDb>polarityMinCancellationDb && dual.cancellationAfterDb<1.f,"Opposed Dual rigs were not flipped ("+std::to_string(dual.cancellationBeforeDb)+" > "+std::to_string(dual.cancellationAfterDb)+" dB)");
        // The prediction is the flipped mix itself.
        {
            const size_t n=size_t(rate*polaritySeconds);
            std::vector<float> mix(n);for(size_t i=0;i<n;++i)mix[i]=.5f*x[i]+.5f*(-opposed[i]);
            const std::vector<float> a(x.begin(),x.begin()+std::ptrdiff_t(n)),b(opposed.begin(),opposed.begin()+std::ptrdiff_t(n));
            const float measured=polarityCancellationDb(measureSpectrum(mix,rate,false),{measureSpectrum(a,rate,false),measureSpectrum(b,rate,false)},{.5f,.5f});
            check(near(dual.cancellationAfterDb,measured,1e-3f),"Polarity prediction differs from the flipped mix");
        }
        // In phase, and independent rigs: kept.
        const auto same=copy(x,.8f);
        const auto inPhase=analysePolarity({&x,&same},{.5f,.5f},rate,false),independent=analysePolarity({&x,&other},{.5f,.5f},rate,false);
        check(!inPhase.changed() && inPhase.cancellationBeforeDb<1.f && !independent.changed() && independent.cancellationBeforeDb<polarityMinCancellationDb,"In-phase or independent rigs were flipped");
        // Crossover weights (1, 1): a high rig opposed to the low one cancels at the split; it flips.
        const auto low=linkwitzRiley(x,rate,400,false),high=negated(linkwitzRiley(x,rate,400,true),1);
        const auto cross=analysePolarity({&low,&high},{1,1},rate,false);
        check(cross.flip==std::vector<bool>{true} && cross.cancellationAfterDb<cross.cancellationBeforeDb-polarityMinGainDb,"An opposed crossover rig was not flipped");
        // Matrix: one source split three ways; MID inverted flips MID only, HIGH inverted flips HIGH only.
        const auto a=linkwitzRiley(x,rate,150,false),m=linkwitzRiley(linkwitzRiley(x,rate,150,true),rate,1200,false),h=linkwitzRiley(linkwitzRiley(x,rate,150,true),rate,1200,true);
        const auto mInv=negated(m,1),hInv=negated(h,1);
        const auto midFlip=analysePolarity({&a,&mInv,&h},{1,1,1},rate,false),highFlip=analysePolarity({&a,&m,&hInv},{1,1,1},rate,false),fine=analysePolarity({&a,&m,&h},{1,1,1},rate,false);
        check(midFlip.flip==std::vector<bool>{true,false} && highFlip.flip==std::vector<bool>{false,true} && !fine.changed(),"Matrix lanes were not flipped one by one");
        check(midFlip.cancellationBeforeDb>polarityMinCancellationDb && midFlip.cancellationAfterDb<midFlip.cancellationBeforeDb-polarityMinGainDb,"The MID flip did not remove the cancellation");
        // A silent lane, and silence: kept.
        const std::vector<float> silence(x.size());
        const auto lonely=analysePolarity({&x,&silence},{.5f,.5f},rate,false);
        check(lonely.playingDetected && !lonely.heard[1] && !lonely.changed(),"A silent rig was flipped");
        check(!analysePolarity({&silence,&silence},{.5f,.5f},rate).playingDetected,"Silence reported as playing");
        if(rate==48000.)std::cout<<"MEASURE polarity rules 48 kHz: opposed Dual rigs "<<dual.cancellationBeforeDb<<" > "<<dual.cancellationAfterDb<<" dB; crossover "<<cross.cancellationBeforeDb<<" > "<<cross.cancellationAfterDb
                                <<" dB; Matrix MID inverted "<<midFlip.cancellationBeforeDb<<" > "<<midFlip.cancellationAfterDb<<" dB; independent rigs "<<independent.cancellationBeforeDb<<" dB\n";
    }
    std::cout<<"PASS auto set polarity rules at 44.1/48/96 kHz: opposed Dual rigs flipped, exact prediction, in-phase and independent kept, crossover, Matrix MID and HIGH one by one, silent lane, silence\n";
}
void controllerPolarity() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    std::mt19937 rng{211};
    const auto x=targetNoise(false,48000,8,rng);
    auto opposed=x;for(auto& v:opposed)v*=-.8f;
    const auto setter=[](std::initializer_list<std::pair<juce::String,float>> pairs){
        std::vector<std::pair<juce::String,float>> list(pairs);
        return [list](ChimeraProcessor& p){for(const auto& [id,v]:list)p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));};};
    const auto blendRig=setter({{"mode",1},{"dualtype",0},{"dualblend",.5f}});
    Controller::Options polarityOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,true,false,false,false};
    // Dual: rig 2 opposed to rig 1 flips, alone in the Dual pass.
    const auto full=runDual(polarityOnly,x,opposed,blendRig);
    const auto* step=stepOf(full.status,P::polarity);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::dual} && step && step->changed && step->result.contains("rig 2 POLARITY normal > inverted")
          && !stepOf(full.status,P::dual) && !stepOf(full.status,P::rigs),"Polarity step did not run alone in the Dual pass or report");
    check(full.polarity.flip==std::vector<bool>{true} && value(full.values,"polarity2")>.5f && value(full.values,"polarity3")==value(full.before,"polarity3")
          && value(full.values,"polarity1")==value(full.before,"polarity1"),"Applied polarity does not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the polarity");
    // Strength does not scale a flip.
    auto halfOptions=polarityOnly;halfOptions.strength=.5f;
    check(value(runDual(halfOptions,x,opposed,blendRig).values,"polarity2")>.5f,"A polarity flip was scaled by strength");
    // With the blend step: the blend hears rig 2 flipped (no cancellation left), and the polarity row comes first.
    auto withBlend=polarityOnly;withBlend.dual=true;
    const auto both=runDual(withBlend,x,opposed,blendRig);
    const auto& steps=both.status.steps;
    const auto polarityRow=std::find_if(steps.begin(),steps.end(),[](const auto& s){return s.phase==P::polarity;}),blendRow=std::find_if(steps.begin(),steps.end(),[](const auto& s){return s.phase==P::dual;});
    check(polarityRow!=steps.end() && blendRow!=steps.end() && polarityRow<blendRow && both.analysis.playingDetected && both.analysis.cancellationDb<dualCancellationDb,
          "The blend step did not hear rig 2 at its new polarity ("+std::to_string(both.analysis.cancellationDb)+" dB)");
    // In phase: kept.
    auto inPhase=x;for(auto& v:inPhase)v*=.8f;
    const auto kept=runDual(polarityOnly,x,inPhase,blendRig);
    check(!stepOf(kept.status,P::polarity)->changed && stepOf(kept.status,P::polarity)->result.contains("add up") && kept.values==kept.before,"In-phase rigs were flipped");
    // The lock, and Classic routing: no pass, the row says why.
    auto lockedOptions=polarityOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockPolarity;
    const auto locked=runDual(lockedOptions,x,opposed,blendRig);
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::polarity)->result=="kept (locked)" && value(locked.values,"polarity2")==value(locked.before,"polarity2"),"Polarity lock did not keep it");
    auto freeOptions=polarityOnly;freeOptions.locks=Controller::lockInput|Controller::lockGate;
    const auto classic=runDual(freeOptions,x,opposed,setter({{"mode",0}}));
    check(classic.status.passes==std::vector<P>{P::compressor} && stepOf(classic.status,P::polarity)->result.contains("Classic routing") && value(classic.values,"polarity2")==value(classic.before,"polarity2"),"Classic routing ran the polarity step");
    // Polarity or routing edited while listening voids it; a silent rig 2 keeps it.
    for(const auto& [id,v]:std::initializer_list<std::pair<juce::String,float>>{{"polarity2",1},{"dualtype",1}}) {
        const auto edited=runDual(polarityOnly,x,opposed,blendRig,[id,v](ChimeraProcessor& p){p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));});
        check(!stepOf(edited.status,P::polarity)->changed && stepOf(edited.status,P::polarity)->result.contains("while listening"),("An edit of "+id+" while listening did not void the polarity step").toStdString());
    }
    const auto silent=runDual(polarityOnly,x,std::vector<float>(x.size()),blendRig);
    check(stepOf(silent.status,P::polarity)->result.contains("rig 2 is silent") && silent.values==silent.before,"A silent rig 2 was flipped");
    // Matrix: one source split three ways with MID inverted: MID flips, in the Matrix pass, before the levels.
    const MatrixLanes split{linkwitzRiley(x,48000,150,false),linkwitzRiley(linkwitzRiley(x,48000,150,true),48000,1200,false),linkwitzRiley(linkwitzRiley(x,48000,150,true),48000,1200,true)};
    auto midInverted=split;scale(midInverted.mid,-1.f);
    const auto matrix=runMatrix(polarityOnly,midInverted,setter({{"mode",2}}));
    const auto* matrixStep=stepOf(matrix.status,P::polarity);
    check(matrix.status.passes==std::vector<P>{P::matrix} && matrixStep && matrixStep->changed && matrixStep->result.contains("MID POLARITY normal > inverted")
          && value(matrix.values,"polarity2")>.5f && value(matrix.values,"polarity3")==value(matrix.before,"polarity3") && !stepOf(matrix.status,P::matrix),"The Matrix MID lane was not flipped alone");
    auto matrixToo=polarityOnly;matrixToo.matrix=true;
    const auto withLevels=runMatrix(matrixToo,midInverted,setter({{"mode",2}}));
    check(withLevels.analysis.playingDetected && withLevels.analysis.cancellationDb<matrixCancellationDb,"The Matrix level step did not hear the MID lane flipped ("+std::to_string(withLevels.analysis.cancellationDb)+" dB)");
    std::cout<<"MEASURE auto set polarity controller: "<<step->result<<"; Matrix: "<<matrixStep->result<<'\n';
    std::cout<<"PASS auto set polarity controller: Dual rig 2 flipped alone in the Dual pass, strength not applied, blend hears the flip, in-phase kept, lock, Classic kept without a pass, edits while listening void, silent rig, Matrix MID flipped and heard by the level step, undo\n";
}
