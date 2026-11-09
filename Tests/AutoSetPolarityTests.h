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
        // The prediction is the flipped mix itself: the captures with rig 2 negated measure what was predicted.
        {
            const auto restored=negated(opposed,1);
            check(near(dual.cancellationAfterDb,polarityCancellationDb({&x,&restored},{.5f,.5f},rate),1e-3f),"Polarity prediction differs from the flipped mix");
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
// The verification dashboard's cases (POL-01, POL-02, POL-03): low notes
// (Drop A: A0 27.5 Hz, A1 55 Hz), lanes opposed in some bands only, a pure
// time offset, a fundamental-only opposition and an exact null.
std::vector<float> harmonicTone(double rate,double seconds,double f0,const std::vector<float>& amplitudes,double delaySeconds=0) {
    std::vector<float> y(size_t(rate*seconds));
    for(size_t i=0;i<y.size();++i) {
        const double t=double(i)/rate-delaySeconds;double v=0;
        if(t>=0)for(size_t h=0;h<amplitudes.size();++h)v+=amplitudes[h]*std::sin(2*juce::MathConstants<double>::pi*f0*double(h+1)*t+.7*double(h));
        y[i]=float(.25*v);
    }
    return y;
}
// Band power (dB) of a signal around f, by projection on a sine and cosine (independent of the analysis).
float projectionDb(const std::vector<float>& x,double rate,double f) {
    double c=0,s=0;const size_t n=std::min(x.size(),size_t(rate*polaritySeconds));
    for(size_t i=0;i<n;++i){const double a=2*juce::MathConstants<double>::pi*f*double(i)/rate;c+=x[i]*std::cos(a);s+=x[i]*std::sin(a);}
    return float(10*std::log10(std::max(1e-30,(c*c+s*s)/(double(n)*double(n)))));
}
void polarityRegressions() {
    for(double rate:{44100.,48000.,96000.})for(double f0:{27.5,55.}) {
        const std::string at=" ("+std::to_string(int(rate))+" Hz, f0 "+std::to_string(f0)+" Hz)";
        std::vector<float> ones,opposedHarmonics,cancelledHarmonics;
        for(int h=1;h<=8;++h){ones.push_back(1.f/float(h));opposedHarmonics.push_back((h==1?.8f:-.8f)/float(h));cancelledHarmonics.push_back((h==1?.8f:-1.f)/float(h));}
        const auto lane1=harmonicTone(rate,5,f0,ones);
        const auto mixOf2=[](const std::vector<float>& a,const std::vector<float>& b,float sign){std::vector<float> y(a.size());for(size_t i=0;i<y.size();++i)y[i]=.5f*a[i]+sign*.5f*b[i];return y;};
        // POL-01: fundamental in polarity, harmonics opposed (1/h, lane 2 at 0.8). A flip would cancel the fundamental: kept.
        {
            const auto lane2=harmonicTone(rate,5,f0,opposedHarmonics);
            const auto r=analysePolarity({&lane1,&lane2},{.5f,.5f},rate,true);
            check(r.playingDetected && !r.changed() && !r.delayHeld() && r.harmHeld() && r.harmedHz<=63.f && r.cancellationBeforeDb>polarityMinCancellationDb,
                  "POL-01: a flip that cancels the fundamental was made or not reported"+at+": changed "+std::to_string(r.changed())+", delay "+std::to_string(r.delayMs)+" ms (lane "+std::to_string(r.delayLane)+"), aligned "+std::to_string(r.alignedDb)+", worst "+std::to_string(r.cancellationBeforeDb)+" dB at "+std::to_string(r.worstBeforeHz)+" Hz, harm "+std::to_string(r.harmedHz)+" Hz");
            // Measured independently: the fundamental as mixed now is kept; a flip would have lost it.
            const float kept=projectionDb(mixOf2(lane1,lane2,1),rate,f0),flipped=projectionDb(mixOf2(lane1,lane2,-1),rate,f0),alone=projectionDb(lane1,rate,f0);
            check(kept>alone-1.f && flipped<alone-12.f,"POL-01: the fixture does not lose the fundamental on a flip"+at);
            // Harmonics fully opposed (the flip would gain in the harmonic bands): still kept, the fundamental band named.
            const auto deep=harmonicTone(rate,5,f0,cancelledHarmonics);
            const auto d=analysePolarity({&lane1,&deep},{.5f,.5f},rate,true);
            check(!d.changed() && d.harmHeld() && d.harmedHz<=63.f && d.harmedDb>10.f,"POL-01: a flip that wins the harmonics and loses the fundamental was made"+at);
            if(rate==48000.)std::cout<<"MEASURE polarity POL-01 f0 "<<f0<<" Hz: worst "<<r.cancellationBeforeDb<<" dB at "<<r.worstBeforeHz<<" Hz; a flip would cancel "<<r.harmedHz<<" Hz by "<<r.harmedDb
                                    <<" dB; fundamental kept "<<kept-alone<<" dB, flipped "<<flipped-alone<<" dB; harmonics fully opposed: "<<d.cancellationBeforeDb<<" dB, flip would cancel "<<d.harmedHz<<" Hz by "<<d.harmedDb<<" dB\n";
        }
        // POL-02: lane 2 is lane 1 at 0.8, 3 ms late, in polarity: a time offset, kept and reported; late and opposed too.
        {
            auto late=harmonicTone(rate,5,f0,ones,.003);for(auto& v:late)v*=.8f;
            const auto r=analysePolarity({&lane1,&late},{.5f,.5f},rate,true);
            check(!r.changed() && r.delayHeld() && r.delayLane==1 && std::abs(r.delayMs-3.f)<.1f && !r.delayOpposed && r.alignedDb<r.cancellationBeforeDb-polarityMinGainDb,
                  "POL-02: a 3 ms offset was treated as polarity"+at+" ("+std::to_string(r.delayMs)+" ms)");
            auto lateOpposed=late;for(auto& v:lateOpposed)v=-v;
            const auto o=analysePolarity({&lane1,&lateOpposed},{.5f,.5f},rate,true);
            check(!o.changed() && o.delayHeld() && std::abs(o.delayMs-3.f)<.1f && o.delayOpposed,"POL-02: a late, opposed lane was flipped or misreported"+at);
            if(rate==48000.)std::cout<<"MEASURE polarity POL-02 f0 "<<f0<<" Hz: "<<r.cancellationBeforeDb<<" dB, "<<r.delayMs<<" ms late, aligned "<<r.alignedDb<<" dB; opposed too: "<<o.delayMs<<" ms\n";
        }
        // POL-03: lane 1 = f0 + a 4 f0 marker, lane 2 = -0.8 f0 only: the fundamental cancels below 80 Hz; it flips.
        {
            const auto marked=harmonicTone(rate,5,f0,{1,0,0,.5f});
            const auto low=harmonicTone(rate,5,f0,{-.8f});
            const auto r=analysePolarity({&marked,&low},{.5f,.5f},rate,true);
            check(r.flip==std::vector<bool>{true} && r.worstBeforeHz<=63.f && r.cancellationBeforeDb>10.f && r.cancellationAfterDb<1.f,
                  "POL-03: a fundamental opposed below 80 Hz was not flipped"+at+" ("+std::to_string(r.cancellationBeforeDb)+" dB at "+std::to_string(r.worstBeforeHz)+" Hz)");
            // An exact null: the same lane, opposed, at the same weighted level. The mix is silent, and still measured.
            auto inverse=lane1;for(auto& v:inverse)v=-v;
            const auto n=analysePolarity({&lane1,&inverse},{.5f,.5f},rate,true);
            check(n.playingDetected && n.flip==std::vector<bool>{true} && n.cancellationBeforeDb>=40.f && n.cancellationAfterDb<1.f,"POL-03: an exact null was not measured or flipped"+at);
            // A weak fundamental: 20 dB under the marker still counts; 40 dB under is outside the 30 dB range (a documented limit).
            const auto weak=harmonicTone(rate,5,f0,{.1f,0,0,1}),weakOpposed=harmonicTone(rate,5,f0,{-.08f});
            check(analysePolarity({&weak,&weakOpposed},{.5f,.5f},rate,true).changed(),"POL-03: a fundamental 20 dB under the marker was not counted"+at);
            const auto faint=harmonicTone(rate,5,f0,{.01f,0,0,1}),faintOpposed=harmonicTone(rate,5,f0,{-.008f});
            check(!analysePolarity({&faint,&faintOpposed},{.5f,.5f},rate,true).changed(),"POL-03: a fundamental 40 dB under the marker counted"+at);
            if(rate==48000.)std::cout<<"MEASURE polarity POL-03 f0 "<<f0<<" Hz: "<<r.cancellationBeforeDb<<" dB at "<<r.worstBeforeHz<<" Hz > "<<r.cancellationAfterDb<<" dB; exact null "<<n.cancellationBeforeDb<<" > "<<n.cancellationAfterDb<<" dB\n";
        }
    }
    // Matrix at X1 60 Hz and X2 1.2 kHz, a low note with noise above: MID, HIGH, and both inverted, one source.
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{212};
        auto x=targetNoise(false,rate,5,rng);
        const auto tone=harmonicTone(rate,5,27.5,{1,.5f,.33f,.25f});
        for(size_t i=0;i<x.size();++i)x[i]=.5f*x[i]+tone[i];
        const auto a=linkwitzRiley(x,rate,60,false),m=linkwitzRiley(linkwitzRiley(x,rate,60,true),rate,1200,false),h=linkwitzRiley(linkwitzRiley(x,rate,60,true),rate,1200,true);
        auto mInv=m,hInv=h;for(auto& v:mInv)v=-v;for(auto& v:hInv)v=-v;
        const auto mid=analysePolarity({&a,&mInv,&h},{1,1,1},rate,true),high=analysePolarity({&a,&m,&hInv},{1,1,1},rate,true),both=analysePolarity({&a,&mInv,&hInv},{1,1,1},rate,true);
        const auto fine=analysePolarity({&a,&m,&h},{1,1,1},rate,true);
        check(mid.flip==std::vector<bool>{true,false} && high.flip==std::vector<bool>{false,true} && both.flip==std::vector<bool>{true,true} && !fine.changed()
              && !mid.delayHeld() && !high.delayHeld() && !both.delayHeld(),"POL-03: Matrix lanes at 60 Hz / 1.2 kHz were not flipped one by one or together ("+std::to_string(int(rate))+" Hz)");
        if(rate==48000.)std::cout<<"MEASURE polarity Matrix 60 Hz/1.2 kHz: MID "<<mid.cancellationBeforeDb<<" > "<<mid.cancellationAfterDb<<" dB, HIGH "<<high.cancellationBeforeDb<<" > "<<high.cancellationAfterDb
                                <<" dB, both "<<both.cancellationBeforeDb<<" > "<<both.cancellationAfterDb<<" dB\n";
    }
    std::cout<<"PASS auto set polarity regressions at 44.1/48/96 kHz, A0 and A1: band-only opposition kept (POL-01), a 3 ms offset kept and reported (POL-02), fundamental-only opposition and exact null flipped, weak fundamental range, Matrix 60 Hz/1.2 kHz (POL-03)\n";
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
    Controller::Options polarityOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,true,false,false};
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
    // A rig 2 that is late (3 ms), not opposed: kept, and the row says it is a time offset.
    auto late=std::vector<float>(144,0.f);late.insert(late.end(),x.begin(),x.end()-144);for(auto& v:late)v*=.8f;
    const auto delayed=runDual(polarityOnly,x,late,blendRig);
    check(!stepOf(delayed.status,P::polarity)->changed && stepOf(delayed.status,P::polarity)->result.contains("rig 2 is 3.00 ms late") && stepOf(delayed.status,P::polarity)->result.contains("not polarity")
          && delayed.values==delayed.before,"A late rig 2 was flipped or the row does not say so: "+stepOf(delayed.status,P::polarity)->result.toStdString());
    // Opposed in some bands only: kept, the row names the band a flip would cancel.
    {
        std::vector<float> ones,opposedHarmonics;for(int h=1;h<=8;++h){ones.push_back(1.f/float(h));opposedHarmonics.push_back((h==1?.8f:-1.f)/float(h));}
        const auto r1=harmonicTone(48000,8,55,ones),r2=harmonicTone(48000,8,55,opposedHarmonics);
        const auto phase=runDual(polarityOnly,r1,r2,blendRig);
        check(!stepOf(phase.status,P::polarity)->changed && stepOf(phase.status,P::polarity)->result.contains("a flip would make ") && phase.polarity.harmedHz<=63.f && phase.values==phase.before,
              "Rigs opposed in some bands only were flipped or misreported: "+stepOf(phase.status,P::polarity)->result.toStdString());
        std::cout<<"MEASURE auto set polarity controller holds: "<<stepOf(delayed.status,P::polarity)->result<<"; "<<stepOf(phase.status,P::polarity)->result<<'\n';
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
    std::cout<<"PASS auto set polarity controller: Dual rig 2 flipped alone in the Dual pass, strength not applied, blend hears the flip, in-phase kept, a late rig and band-only opposition kept with the reason, lock, Classic kept without a pass, edits while listening void, silent rig, Matrix MID flipped and heard by the level step, undo\n";
}
