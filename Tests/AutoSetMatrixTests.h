#pragma once
// Phase 17: the Matrix lane levels. Included by AutoSetTests.cpp after AutoSetDualTests.h (linkwitzRiley).

// Matrix lanes from independent on-target sources, band-limited at 150 Hz and 1.2 kHz.
struct MatrixLanes {std::vector<float> low,mid,high;};
MatrixLanes matrixLanes(double rate,double seconds,std::mt19937& rng) {
    const auto a=targetNoise(false,rate,seconds,rng),b=targetNoise(false,rate,seconds,rng),c=targetNoise(false,rate,seconds,rng);
    return {linkwitzRiley(a,rate,150,false),linkwitzRiley(linkwitzRiley(b,rate,150,true),rate,1200,false),linkwitzRiley(linkwitzRiley(c,rate,150,true),rate,1200,true)};
}
std::vector<float> matrixMix(const MatrixLanes& l,float midDb,float highDb,double rate) {
    const size_t n=std::min({l.low.size(),l.mid.size(),l.high.size(),size_t(rate*matrixSeconds)});
    const float gm=float(std::pow(10.,midDb/20.)),gh=float(std::pow(10.,highDb/20.));
    std::vector<float> y(n);for(size_t i=0;i<n;++i)y[i]=l.low[i]+gm*l.mid[i]+gh*l.high[i];return y;
}
void matrixRules() {
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{170};
        const auto lanes=matrixLanes(rate,5,rng);
        const auto neutral=analyseMatrix(lanes.low,lanes.mid,lanes.high,rate,false);
        check(neutral.playingDetected && neutral.heard==std::array<bool,3>{true,true,true},"Matrix analysis did not hear the three lanes");
        // The prediction is the mix itself.
        check(near(neutral.toneErrorAfterDb,ampToneErrorDb(measureSpectrum(matrixMix(lanes,neutral.changeDb[0],neutral.changeDb[1],rate),rate,false),Style::rock),1e-4f),"Matrix prediction differs from the mix");
        for(const auto& r:{neutral})for(const float d:r.changeDb)
            check(std::abs(d)<=matrixMaxMoveDb+1e-5f && std::abs(d*2-std::round(d*2))<1e-4f,"A lane moved past the limit or off the 0.5 dB grid");
        // MID 6 dB low: it comes up against HIGH; HIGH 6 dB hot: it comes down against MID.
        // The target is a shape, so the balance between the lanes is what is checked; LOW never moves.
        auto quiet=lanes;scale(quiet.mid,.5f);
        const auto up=analyseMatrix(quiet.low,quiet.mid,quiet.high,rate,false);
        check(up.changed() && up.changeDb[0]-up.changeDb[1]>=4.f && up.toneErrorAfterDb<up.toneErrorBeforeDb-matrixMinGainDb,
              "A quiet MID lane did not come up against HIGH ("+std::to_string(up.changeDb[0])+"/"+std::to_string(up.changeDb[1])+" dB)");
        auto hot=lanes;scale(hot.high,2.f);
        const auto down=analyseMatrix(hot.low,hot.mid,hot.high,rate,false);
        check(down.changed() && down.changeDb[1]-down.changeDb[0]<=-4.f && down.toneErrorAfterDb<down.toneErrorBeforeDb-matrixMinGainDb,"A hot HIGH lane did not come down against MID ("+std::to_string(down.changeDb[0])+"/"+std::to_string(down.changeDb[1])+" dB, error "+std::to_string(down.toneErrorBeforeDb)+" > "+std::to_string(down.toneErrorAfterDb)+")");
        check(near(up.toneErrorAfterDb,ampToneErrorDb(measureSpectrum(matrixMix(quiet,up.changeDb[0],up.changeDb[1],rate),rate,false),Style::rock),1e-4f),"Matrix prediction differs from the mix after a move");
        // A control at the end of its range: that lane stays.
        const auto pinned=analyseMatrix(quiet.low,quiet.mid,quiet.high,rate,false,Style::rock,{std::pair{-6.f,0.f},std::pair{-6.f,6.f}});
        check(pinned.changeDb[0]<=0.f,"A MID level at its maximum was raised");
        // Applying the change and running again settles (no second large move).
        auto applied=quiet;scale(applied.mid,float(std::pow(10.,up.changeDb[0]/20.)));scale(applied.high,float(std::pow(10.,up.changeDb[1]/20.)));
        const auto again=analyseMatrix(applied.low,applied.mid,applied.high,rate,false);
        check(std::abs(again.changeDb[0])<=1.f && std::abs(again.changeDb[1])<=1.f,"Matrix levels did not settle after one run");
        // A silent MID lane, and silence.
        const std::vector<float> silence(lanes.mid.size());
        const auto lonely=analyseMatrix(lanes.low,silence,lanes.high,rate,false);
        check(lonely.playingDetected && !lonely.heard[1] && !lonely.changed(),"A silent MID lane was moved");
        // A silent LOW lane (the reference): MID and HIGH stay, even when they are out of balance.
        const auto noLow=analyseMatrix(silence,quiet.mid,quiet.high,rate,false);
        check(noLow.playingDetected && !noLow.heard[0] && !noLow.changed(),"Matrix levels moved without the LOW reference");
        check(!analyseMatrix(silence,silence,silence,rate).playingDetected,"Silence reported as playing");
        // One source split three ways: in polarity it adds back; MID inverted cancels at both crossovers.
        const auto x=targetNoise(false,rate,5,rng);
        const MatrixLanes split{linkwitzRiley(x,rate,150,false),linkwitzRiley(linkwitzRiley(x,rate,150,true),rate,1200,false),linkwitzRiley(linkwitzRiley(x,rate,150,true),rate,1200,true)};
        auto flipped=split;scale(flipped.mid,-1.f);
        const auto inPhase=analyseMatrix(split.low,split.mid,split.high,rate,false),cancel=analyseMatrix(flipped.low,flipped.mid,flipped.high,rate,false);
        check(cancel.cancellationDb>inPhase.cancellationDb+3.f && cancel.cancellationDb>matrixCancellationDb && neutral.cancellationDb<matrixCancellationDb && inPhase.cancellationDb<matrixCancellationDb,"An inverted MID lane was not reported as cancelling ("+std::to_string(cancel.cancellationDb)+" dB, in phase "+std::to_string(inPhase.cancellationDb)+")");
        if(rate==48000.)std::cout<<"MEASURE matrix rules 48 kHz: neutral MID/HIGH "<<neutral.changeDb[0]<<"/"<<neutral.changeDb[1]<<" dB (error "<<neutral.toneErrorBeforeDb<<" > "<<neutral.toneErrorAfterDb
                                <<"); quiet MID "<<up.changeDb[0]<<"/"<<up.changeDb[1]<<" dB (error "<<up.toneErrorBeforeDb<<" > "<<up.toneErrorAfterDb<<", "<<up.evaluations<<" mixes); hot HIGH "<<down.changeDb[0]<<"/"<<down.changeDb[1]
                                <<" dB; cancellation in phase "<<inPhase.cancellationDb<<" dB, MID inverted "<<cancel.cancellationDb<<" dB\n";
    }
    std::cout<<"PASS auto set matrix rules at 44.1/48/96 kHz: exact mix, quiet MID up, hot HIGH down, LOW anchored, 6 dB limit on a 0.5 dB grid, control range, settles, silent MID and LOW lanes, silence, crossover cancellation\n";
}
// The engine captures each Matrix lane after its level and polarity: the
// three lanes add up to the Matrix output.
void matrixLaneCapture() {
    spectralforge::Engine engine;engine.prepare({48000,256,1});engine.setLaneCapture(true);
    std::array<spectralforge::LaneState,3> lanes{};lanes[1].levelDb=-4;lanes[2].levelDb=3;lanes[2].polarity=true;
    std::mt19937 rng{171};const auto x=targetNoise(false,48000,1,rng);
    juce::AudioBuffer<float> buffer(1,256);float worst=0,energy=0;int filled=0;
    for(int block=0;block<40;++block) {
        buffer.copyFrom(0,0,x.data()+size_t(block)*256,256);
        engine.process(buffer,spectralforge::RoutingMode::matrix,150,1200,lanes);
        if(block<20)continue; // past the routing and level smoothing
        filled+=engine.laneCaptureCount()==3;
        for(int n=0;n<256;++n) {
            const float expected=engine.laneOutput(0).getSample(0,n)+engine.laneOutput(1).getSample(0,n)+engine.laneOutput(2).getSample(0,n);
            worst=std::max(worst,std::abs(buffer.getSample(0,n)-expected));energy=std::max(energy,std::abs(buffer.getSample(0,n)));
        }
    }
    check(filled==20 && energy>1e-3f && worst<1e-5f*std::max(1.f,energy),"Captured Matrix lanes do not add up to the Matrix output");
    // The processor feeds lane1-lane3 in Matrix routing for the Matrix pass, and only lane1-lane2 in Dual routing for the Dual pass.
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    const auto capture=[&](int mode,bool matrixPass) {
        setRaw("mode",float(mode));
        if(matrixPass)p->autoSetCapture().listen({Tap::lane1,Tap::lane2,Tap::lane3});else p->autoSetCapture().listen({Tap::lane1,Tap::lane2});
        juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
        for(int block=0;block<20;++block){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+size_t(block)*256,256);p->processBlock(io,midi);}
        std::vector<float> first,second,third;p->autoSetCapture().drainSamples(first);p->autoSetCapture().drainAmpSamples(second);p->autoSetCapture().drainLaneSamples(third);
        p->autoSetCapture().listen(std::nullopt);
        return std::array<size_t,3>{first.size(),second.size(),third.size()};
    };
    const auto matrix=capture(2,true),dualInMatrix=capture(2,false),matrixInDual=capture(1,true),dual=capture(1,false);
    check(matrix[0]>=8*256 && matrix[0]==matrix[1] && matrix[1]==matrix[2] && matrix[0]%256==0,"Matrix lane taps not fed together ("+std::to_string(matrix[0])+", "+std::to_string(matrix[2])+")");
    check(dualInMatrix==std::array<size_t,3>{} && matrixInDual==std::array<size_t,3>{},"Lane taps fed for the wrong routing");
    check(dual[0]>=8*256 && dual[0]==dual[1] && dual[2]==0,"Dual lane taps changed with the Matrix tap");
    p->releaseResources();
    // On the real processor, a native head's OUTPUT LEVEL scales its lane linearly
    // (applied after the head's nonlinearity), which is what the analysis assumes.
    const auto midLane=[&](float outputDb) {
        auto q=std::make_unique<ChimeraProcessor>();q->setRateAndBufferSizeDetails(48000,256);q->prepareToPlay(48000,256);
        for(const auto& [id,v]:std::initializer_list<std::pair<juce::String,float>>{{"mode",2},{spectralforge::ampNativeOutputLevelID(4),outputDb}})
            q->parameters().getParameter(id)->setValueNotifyingHost(q->parameters().getParameter(id)->convertTo0to1(v));
        juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
        for(int block=0;block<12;++block){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+size_t(block)*256,256);q->processBlock(io,midi);}
        q->autoSetCapture().listen({Tap::lane1,Tap::lane2,Tap::lane3});
        for(int block=12;block<40;++block){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+size_t(block)*256,256);q->processBlock(io,midi);}
        std::vector<float> mid;q->autoSetCapture().drainAmpSamples(mid);q->releaseResources();
        return mid;
    };
    const auto flat=midLane(0),raised=midLane(6);
    double e0=0,e1=0,worstSample=0;
    for(size_t n=0;n<std::min(flat.size(),raised.size());++n){e0+=double(flat[n])*flat[n];e1+=double(raised[n])*raised[n];worstSample=std::max(worstSample,std::abs(double(raised[n])-std::pow(10.,6./20.)*flat[n]));}
    const double gainDb=10*std::log10(std::max(e1,1e-30)/std::max(e0,1e-30));
    check(flat.size()==raised.size() && flat.size()>=20*256 && e0>0 && std::abs(gainDb-6)<.05,"A native head's OUTPUT LEVEL does not scale its Matrix lane by its dB ("+std::to_string(gainDb)+" dB)");
    std::cout<<"MEASURE matrix native OUTPUT LEVEL +6 dB: lane gain "<<gainDb<<" dB, worst sample error "<<worstSample<<'\n';
    std::cout<<"PASS auto set matrix capture: lanes after level and polarity sum to the engine output; lane1-lane3 fed in Matrix for the Matrix pass only, Dual unchanged; native OUTPUT LEVEL is linear\n";
}
struct MatrixRun {std::vector<float> before,values,undone;Controller::Status status;MatrixResult analysis;PolarityResult polarity;};
// Feeds the three lanes to the lane taps of a real processor (Matrix routing set by prepare).
MatrixRun runMatrix(const Controller::Options& options,const MatrixLanes& lanes,const std::function<void(ChimeraProcessor&)>& prepare,
                    const std::function<void(ChimeraProcessor&)>& during={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    prepare(*p);
    Controller c(*p);c.setOptions(options);
    MatrixRun result;result.before=values(*p);
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=lanes.low.size();offset+=256) {
        const float* a[]{lanes.low.data()+offset};const float* b[]{lanes.mid.data()+offset};const float* d[]{lanes.high.data()+offset};
        p->autoSetCapture().push(Tap::lane1,a,1,256);p->autoSetCapture().push(Tap::lane2,b,1,256);p->autoSetCapture().push(Tap::lane3,d,1,256);
        for(auto tap:{Tap::input,Tap::compressor})p->autoSetCapture().push(tap,a,1,256);
        ms+=1000.*256/48000;c.tick(ms);
        if(during && offset==0)during(*p);
    }
    check(!c.busy(),"Matrix run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastMatrixAnalysis();result.polarity=c.lastPolarityAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerMatrix() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    std::mt19937 rng{172};
    auto lanes=matrixLanes(48000,8,rng);scale(lanes.mid,.5f); // MID 6 dB low
    const auto setter=[](std::initializer_list<std::pair<juce::String,float>> pairs){
        std::vector<std::pair<juce::String,float>> list(pairs);
        return [list](ChimeraProcessor& p){for(const auto& [id,v]:list)p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));};};
    const auto midNative=spectralforge::ampNativeOutputLevelID(4),highNative=spectralforge::ampNativeOutputLevelID(5);
    const auto matrixMode=setter({{"mode",2}});
    const Controller::Options matrixOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,true,false,false,false,false,false,false,false};
    // Native heads (the default): the lanes' OUTPUT LEVEL moves; LEVEL and LOW stay.
    const auto full=runMatrix(matrixOnly,lanes,matrixMode);
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::matrix);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::matrix} && step && step->changed && step->result.contains("MID OUTPUT LEVEL"),"Matrix pass did not run alone or report");
    check(a.playingDetected && a.changeDb[0]>0 && near(value(full.values,midNative),a.changeDb[0],.011f) && near(value(full.values,highNative),a.changeDb[1],.011f)
          && value(full.values,"level2")==value(full.before,"level2") && value(full.values,spectralforge::ampNativeOutputLevelID(3))==value(full.before,spectralforge::ampNativeOutputLevelID(3)),
          "Applied Matrix levels do not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the Matrix levels");
    // Legacy heads: the lane LEVEL moves.
    const auto legacy=runMatrix(matrixOnly,lanes,setter({{"mode",2},{spectralforge::ampNativeEnabledID(4),0},{spectralforge::ampNativeEnabledID(5),0}}));
    check(stepOf(legacy.status,P::matrix)->result.contains("MID LEVEL") && near(value(legacy.values,"level2"),legacy.analysis.changeDb[0],.011f)
          && value(legacy.values,midNative)==value(legacy.before,midNative),"Legacy Matrix lanes did not use LEVEL");
    auto halfOptions=matrixOnly;halfOptions.strength=.5f;
    const auto half=runMatrix(halfOptions,lanes,matrixMode);
    check(near(value(half.values,midNative),.5f*half.analysis.changeDb[0],.011f),"Strength did not scale the Matrix level change");
    auto lockedOptions=matrixOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockMatrix;
    const auto locked=runMatrix(lockedOptions,lanes,matrixMode);
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::matrix) && stepOf(locked.status,P::matrix)->result=="kept (locked)"
          && value(locked.values,midNative)==value(locked.before,midNative),"Matrix lock did not keep the levels");
    // Classic routing: no pass, the row says why.
    auto classicOptions=matrixOnly;classicOptions.locks=Controller::lockInput|Controller::lockGate;
    const auto classic=runMatrix(classicOptions,lanes,setter({{"mode",0}}));
    check(classic.status.passes==std::vector<P>{P::compressor} && stepOf(classic.status,P::matrix)->result.contains("Classic or Dual routing")
          && value(classic.values,midNative)==value(classic.before,midNative) && value(classic.values,"level2")==value(classic.before,"level2"),"Classic routing ran the Matrix pass");
    // The routing or a lane's head engine changing while the pass listens keeps everything.
    for(const auto& [id,v]:std::initializer_list<std::pair<juce::String,float>>{{"mode",1},{spectralforge::ampNativeEnabledID(5),0}}) {
        const auto edited=runMatrix(matrixOnly,lanes,matrixMode,[id,v](ChimeraProcessor& p){p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));});
        check(stepOf(edited.status,P::matrix)->result.contains("changed while listening") && !stepOf(edited.status,P::matrix)->changed
              && value(edited.values,midNative)==value(edited.before,midNative) && value(edited.values,highNative)==value(edited.before,highNative),
              ("An edit of "+id+" while listening did not void the Matrix pass").toStdString());
    }
    // A MID level edited by the player while the pass listens is kept.
    const auto manual=runMatrix(matrixOnly,lanes,matrixMode,setter({{midNative,-3}}));
    check(near(value(manual.values,midNative),-3,.011f),"A MID level edited while listening was overwritten");
    // A silent HIGH lane keeps the levels.
    auto silentHigh=lanes;std::fill(silentHigh.high.begin(),silentHigh.high.end(),0.f);
    const auto silent=runMatrix(matrixOnly,silentHigh,matrixMode);
    check(stepOf(silent.status,P::matrix)->result.contains("HIGH lane is silent") && silent.values==silent.before,"A silent HIGH lane changed the levels");
    auto silentLow=lanes;std::fill(silentLow.low.begin(),silentLow.low.end(),0.f);
    const auto noLow=runMatrix(matrixOnly,silentLow,matrixMode);
    check(stepOf(noLow.status,P::matrix)->result.contains("LOW lane is silent") && noLow.values==noLow.before,"A silent LOW lane changed the levels");
    std::cout<<"MEASURE auto set matrix controller: "<<step->result<<'\n';
    std::cout<<"PASS auto set matrix controller: Matrix pass alone, native OUTPUT LEVEL and legacy LEVEL, strength, lock, Classic routing kept without a pass, routing or head edited while listening, manual edit kept, silent HIGH and LOW lanes, undo\n";
}
