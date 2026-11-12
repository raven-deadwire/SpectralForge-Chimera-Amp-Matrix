#pragma once
// Phase 25: the Matrix crossovers X1 and X2. Included by AutoSetTests.cpp after AutoSetMatrixTests.h.

// One source split the way the engine splits it: LOW = LP(X1) AP(X2), MID = HP(X1) LP(X2), HIGH = HP(X1) HP(X2).
MatrixLanes crossoverSplit(const std::vector<float>& x,double rate,float x1,float x2) {
    const auto lp=linkwitzRiley(x,rate,x1,false),hp=linkwitzRiley(x,rate,x1,true);
    auto low=linkwitzRiley(lp,rate,x2,false);const auto lowHigh=linkwitzRiley(lp,rate,x2,true);
    for(size_t n=0;n<low.size();++n)low[n]+=lowHigh[n];
    return {std::move(low),linkwitzRiley(hp,rate,x2,false),linkwitzRiley(hp,rate,x2,true)};
}
// Lanes as an amp would play them: a gain, optionally driven (a soft clip).
MatrixLanes crossoverLanes(const std::vector<float>& x,double rate,float x1,float x2,std::array<float,3> gainDb,float drive=0) {
    auto l=crossoverSplit(x,rate,x1,x2);
    std::array<std::vector<float>*,3> lanes{&l.low,&l.mid,&l.high};
    for(int k=0;k<3;++k) {
        const float g=float(std::pow(10.,gainDb[size_t(k)]/20.));
        for(auto& v:*lanes[size_t(k)])v=drive>0?g*std::tanh(drive*v)/drive:g*v;
    }
    return l;
}
float crossoverHeardError(const MatrixLanes& l,double rate,Style style=Style::rock) {
    return ampToneErrorDb(measureSpectrum(matrixMix(l,0,0,rate),rate,false),style);
}
// The prediction for a split, against the lanes rendered at that split.
float crossoverPredictionError(const std::vector<float>& x,double rate,float x1,float x2,float a,float b,std::array<float,3> gainDb,float drive=0) {
    const auto now=crossoverLanes(x,rate,x1,x2,gainDb,drive);
    const size_t n=std::min({now.low.size(),size_t(rate*matrixSeconds)});
    const auto head=[n](const std::vector<float>& v){return std::vector<float>(v.begin(),v.begin()+std::ptrdiff_t(n));};
    const auto heard=measureSpectrum(matrixMix(now,0,0,rate),rate,false);
    const std::array<Spectrum,3> lanes{measureSpectrum(head(now.low),rate,false),measureSpectrum(head(now.mid),rate,false),measureSpectrum(head(now.high),rate,false)};
    const float predicted=ampToneErrorDb(crossoverPredicted(heard,lanes,{},{},x1,x2,a,b),Style::rock);
    return predicted-crossoverHeardError(crossoverLanes(x,rate,a,b,gainDb,drive),rate);
}
void crossoverRules() {
    // The modelled responses sum flat (as the engine's split does) and LP/HP cross at -6 dB.
    for(double f:{40.,150.,400.,1200.,5000.}) {
        std::complex<double> sum;for(int k=0;k<3;++k)sum+=crossoverLaneResponse(k,f,150,1200);
        check(std::abs(std::abs(sum)-1)<1e-9,"The modelled lanes do not sum flat");
    }
    check(std::abs(std::abs(lr4(150,150,false))-.5)<1e-9 && std::abs(std::abs(lr4(150,150,true))-.5)<1e-9,"The model's Linkwitz-Riley does not cross at -6 dB");
    float worstLinear=0,worstDriven=0;
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{250};
        const auto x=targetNoise(false,rate,5,rng);
        // Lanes as split: the mix is flat whatever the crossovers, so they stay.
        const auto flat=crossoverLanes(x,rate,150,1200,{0,0,0});
        const auto neutral=analyseCrossover(flat.low,flat.mid,flat.high,rate,150,1200,{},{},false);
        check(neutral.playingDetected && neutral.heard==std::array<bool,3>{true,true,true} && !neutral.changed()
              && std::abs(neutral.toneErrorBeforeDb-crossoverHeardError(flat,rate))<1e-4f,"Flat lanes moved the crossovers or the error is not the heard one");
        // Lanes out of balance: the split moves where the prediction fits better, and the lanes
        // re-rendered at the new split fit as predicted (linear lanes: the prediction is exact).
        std::vector<CrossoverResult> moved;
        for(const auto& gains:{std::array<float,3>{0,0,9},std::array<float,3>{0,0,-9},std::array<float,3>{9,0,0},std::array<float,3>{0,-9,0}}) {
            const auto l=crossoverLanes(x,rate,150,1200,gains);
            const auto r=analyseCrossover(l.low,l.mid,l.high,rate,150,1200,{},{},false);
            const float rendered=crossoverHeardError(crossoverLanes(x,rate,r.x1After,r.x2After,gains),rate);
            const auto name=std::to_string(gains[0])+"/"+std::to_string(gains[1])+"/"+std::to_string(gains[2])+" dB lanes";
            check(r.changed() && r.toneErrorAfterDb<r.toneErrorBeforeDb-crossoverMinGainDb && std::abs(rendered-r.toneErrorAfterDb)<.1f,
                  "Crossovers for "+name+": "+std::to_string(r.x1After)+"/"+std::to_string(r.x2After)+" Hz, error "+std::to_string(r.toneErrorBeforeDb)+" > "+std::to_string(r.toneErrorAfterDb)+" predicted, "+std::to_string(rendered)+" rendered");
            moved.push_back(r);
            if(rate==48000.)std::cout<<"MEASURE crossover rules 48 kHz, "<<name<<": X1/X2 "<<r.x1After<<"/"<<r.x2After<<" Hz, error "<<r.toneErrorBeforeDb<<" > "<<r.toneErrorAfterDb<<" predicted, "<<rendered<<" rendered ("<<r.evaluations<<" evaluations)\n";
        }
        // Candidates: within a third of an octave, on 1 Hz, in range, X2 at least an octave above X1.
        for(const auto& r:moved)
            check(std::abs(std::log2(r.x1After/r.x1Before))<=1/3.f+1e-3f && std::abs(std::log2(r.x2After/r.x2Before))<=1/3.f+1e-3f
                  && r.x1After==std::round(r.x1After) && r.x2After==std::round(r.x2After) && r.x2After>=crossoverMinRatio*r.x1After,"A crossover moved off the candidate grid");
        const std::array<float,3> hotHigh{0,0,9};
        const auto hot=crossoverLanes(x,rate,150,1200,hotHigh);
        const auto edge=crossoverLanes(x,rate,340,700,hotHigh);
        const auto edged=analyseCrossover(edge.low,edge.mid,edge.high,rate,340,700,{},{},false);
        check(edged.x1After>=crossoverX1Range[0] && edged.x1After<=crossoverX1Range[1] && edged.x2After>=crossoverX2Range[0] && edged.x2After<=crossoverX2Range[1]
              && edged.x2After>=crossoverMinRatio*edged.x1After,"A crossover left its range or the octave spacing");
        // Phase 17's level changes count: HIGH brought down 9 dB first is the flat split, which stays.
        const auto levelled=analyseCrossover(hot.low,hot.mid,hot.high,rate,150,1200,{0,0,-9},{},false);
        check(!levelled.changed() && std::abs(levelled.toneErrorBeforeDb-neutral.toneErrorBeforeDb)<.05f,"A lane level change was not applied before the crossovers");
        // An inverted MID lane: the model knows the lanes cancel at the crossovers.
        auto inverted=flat;scale(inverted.mid,-1.f);
        const auto cancel=analyseCrossover(inverted.low,inverted.mid,inverted.high,rate,150,1200,{},{false,true,false},false);
        check(cancel.playingDetected && std::abs(cancel.toneErrorBeforeDb-crossoverHeardError(inverted,rate))<1e-4f,"The heard error ignores the inverted lane");
        // Silent lanes and silence.
        const std::vector<float> silence(flat.mid.size());
        const auto lonely=analyseCrossover(hot.low,silence,hot.high,rate,150,1200,{},{},false);
        check(lonely.playingDetected && !lonely.heard[1] && !lonely.changed(),"A silent MID lane moved the crossovers");
        check(!analyseCrossover(silence,silence,silence,rate,150,1200).playingDetected,"Silence reported as playing");
        // Prediction against the re-rendered lanes over the candidate grid: exact for linear lanes, approximate when driven.
        for(int i=-crossoverSteps;i<=crossoverSteps;++i)for(int j=-crossoverSteps;j<=crossoverSteps;++j) {
            const float a=std::round(150*std::pow(2.f,i/6.f)),b=std::round(1200*std::pow(2.f,j/6.f));
            worstLinear=std::max(worstLinear,std::abs(crossoverPredictionError(x,rate,150,1200,a,b,hotHigh)));
            if(rate==48000.)worstDriven=std::max(worstDriven,std::abs(crossoverPredictionError(x,rate,150,1200,a,b,hotHigh,4)));
        }
    }
    check(worstLinear<.05f,"The linear prediction misses the re-rendered lanes by "+std::to_string(worstLinear)+" dB");
    std::cout<<"MEASURE crossover prediction vs re-rendered lanes over the grid: linear worst "<<worstLinear<<" dB; driven (tanh x4) worst "<<worstDriven<<" dB\n";
    std::cout<<"PASS auto set crossover rules at 44.1/48/96 kHz: flat split kept, four out-of-balance lane sets moved to a split that re-renders as predicted, grid/range/octave spacing, lane levels first, polarity, silent lane, silence, prediction vs re-render\n";
}
// The real processor's lanes for a source at X1/X2 (Matrix routing, the default heads).
MatrixLanes crossoverProcessorLanes(const std::vector<float>& x,float x1,float x2) {
    auto q=std::make_unique<ChimeraProcessor>();q->setRateAndBufferSizeDetails(48000,256);q->prepareToPlay(48000,256);
    for(const auto& [id,v]:std::initializer_list<std::pair<juce::String,float>>{{"mode",2},{"x1",x1},{"x2",x2}})
        q->parameters().getParameter(id)->setValueNotifyingHost(q->parameters().getParameter(id)->convertTo0to1(v));
    juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
    size_t offset=0;
    for(;offset<48*256;offset+=256){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+offset,256);q->processBlock(io,midi);}
    q->autoSetCapture().listen({Tap::lane1,Tap::lane2,Tap::lane3});
    for(;offset+256<=x.size();offset+=256){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+offset,256);q->processBlock(io,midi);}
    MatrixLanes l;q->autoSetCapture().drainSamples(l.low);q->autoSetCapture().drainAmpSamples(l.mid);q->autoSetCapture().drainLaneSamples(l.high);
    q->releaseResources();
    return l;
}
void controllerCrossover() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    check(!Controller::Options{}.crossover && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.crossover,"Matrix crossovers are on by default");
    const auto setter=[](std::initializer_list<std::pair<juce::String,float>> pairs){
        std::vector<std::pair<juce::String,float>> list(pairs);
        return [list](ChimeraProcessor& p){for(const auto& [id,v]:list)p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));};};
    Controller::Options crossoverOnly;
    crossoverOnly.instrument=1;crossoverOnly.style=Style::rock;crossoverOnly.strength=1.f;crossoverOnly.locks=Controller::lockInput|Controller::lockGate|Controller::lockCompressor;
    for(bool* b:{&crossoverOnly.tone,&crossoverOnly.finish,&crossoverOnly.multiband,&crossoverOnly.gates,&crossoverOnly.dynamicEQ,&crossoverOnly.level,&crossoverOnly.amp,&crossoverOnly.cab,&crossoverOnly.boost,
                 &crossoverOnly.space,&crossoverOnly.power,&crossoverOnly.dual,&crossoverOnly.width,&crossoverOnly.cuts,&crossoverOnly.bus,&crossoverOnly.matrix,&crossoverOnly.mics,&crossoverOnly.rigs,
                 &crossoverOnly.channel,&crossoverOnly.polarity,&crossoverOnly.sync,&crossoverOnly.cabIR,&crossoverOnly.trim})*b=false;
    crossoverOnly.crossover=true;
    std::mt19937 rng{251};
    const auto x=targetNoise(false,48000,8,rng);
    const auto lanes=crossoverLanes(x,48000,150,1200,{0,0,9});
    const auto matrixMode=setter({{"mode",2}});
    // HIGH 9 dB hot: the Matrix pass runs alone for the crossovers and moves them.
    const auto full=runMatrix(crossoverOnly,lanes,matrixMode);
    const auto* step=stepOf(full.status,P::crossover);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::matrix} && step && step->changed && step->result.contains("X1 150 Hz > ") && step->result.contains(", X2 1.20 kHz > ") && step->result.contains("predicted")
          && !stepOf(full.status,P::matrix),"Crossover step did not run alone in the Matrix pass or report"+(step?(": "+step->result).toStdString():std::string()));
    const auto& a=full.crossover;
    check(near(value(full.values,"x1"),a.x1After,.51f) && near(value(full.values,"x2"),a.x2After,.51f) && a.changed(),"Applied crossovers do not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore X1 and X2");
    // 50% strength: half the move in octaves.
    auto halfOptions=crossoverOnly;halfOptions.strength=.5f;
    const auto half=runMatrix(halfOptions,lanes,matrixMode);
    check(near(value(half.values,"x1"),std::round(150*std::sqrt(half.crossover.x1After/150)),.51f) && near(value(half.values,"x2"),std::round(1200*std::sqrt(half.crossover.x2After/1200)),.51f),"Strength did not scale the crossover move");
    // With the levels step: levels first (HIGH comes down), then the crossovers judge the levelled lanes; rows in order.
    auto withLevels=crossoverOnly;withLevels.matrix=true;
    const auto both=runMatrix(withLevels,lanes,matrixMode);
    const auto& steps=both.status.steps;
    const auto row=[&](P phase){return std::find_if(steps.begin(),steps.end(),[phase](const auto& s){return s.phase==phase;});};
    check(row(P::matrix)!=steps.end() && row(P::crossover)!=steps.end() && row(P::matrix)<row(P::crossover),"Matrix level and crossover rows are out of order");
    check(row(P::matrix)->changed && (!row(P::crossover)->changed || both.crossover.toneErrorBeforeDb<full.crossover.toneErrorBeforeDb),"The crossover step did not see the level change");
    // Lock: no pass, the row says so.
    auto lockedOptions=crossoverOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockCrossover;
    const auto locked=runMatrix(lockedOptions,lanes,matrixMode);
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::crossover)->result=="kept (locked)" && value(locked.values,"x2")==value(locked.before,"x2"),"Crossover lock did not keep X2");
    // Classic routing: no pass.
    auto classicOptions=crossoverOnly;classicOptions.locks=Controller::lockInput|Controller::lockGate;
    const auto classic=runMatrix(classicOptions,lanes,setter({{"mode",0}}));
    check(classic.status.passes==std::vector<P>{P::compressor} && stepOf(classic.status,P::crossover)->result.contains("Classic or Dual routing") && value(classic.values,"x1")==value(classic.before,"x1") && value(classic.values,"x2")==value(classic.before,"x2"),"Classic routing ran the crossover step");
    // X2 edited while listening, and the routing changed while listening: kept.
    const auto edited=runMatrix(crossoverOnly,lanes,matrixMode,setter({{"x2",900}}));
    check(stepOf(edited.status,P::crossover)->result.contains("edited while listening") && near(value(edited.values,"x2"),900,.51f),"An X2 edited while listening was overwritten");
    const auto rerouted=runMatrix(crossoverOnly,lanes,matrixMode,setter({{"mode",1}}));
    check(stepOf(rerouted.status,P::crossover)->result.contains("changed while listening") && value(rerouted.values,"x2")==value(rerouted.before,"x2"),"A routing change while listening did not void the crossovers");
    // A silent MID lane keeps them.
    auto silentMid=lanes;std::fill(silentMid.mid.begin(),silentMid.mid.end(),0.f);
    const auto silent=runMatrix(crossoverOnly,silentMid,matrixMode);
    check(stepOf(silent.status,P::crossover)->result.contains("MID lane is silent") && value(silent.values,"x1")==value(silent.before,"x1") && value(silent.values,"x2")==value(silent.before,"x2"),"A silent MID lane changed the crossovers");
    // The real processor, default heads (driven): the prediction against the lanes re-rendered at the proposed split.
    {
        std::mt19937 notes{252};const auto di=playedNotes(48000,6,notes);
        const auto now=crossoverProcessorLanes(di,150,1200);
        auto hotNow=now;scale(hotNow.high,float(std::pow(10.,9/20.)));
        const auto r=analyseCrossover(hotNow.low,hotNow.mid,hotNow.high,48000,150,1200,{},{},false);
        auto next=crossoverProcessorLanes(di,r.x1After,r.x2After);scale(next.high,float(std::pow(10.,9/20.)));
        const float actual=crossoverHeardError(next,48000);
        check(r.playingDetected && near(r.toneErrorBeforeDb,crossoverHeardError(hotNow,48000),1e-3f),"The processor lanes' heard error is not the analysed one");
        std::cout<<"MEASURE auto set crossover on the processor (default heads, HIGH +9 dB): X1/X2 150/1200 > "<<r.x1After<<"/"<<r.x2After<<" Hz, error "<<r.toneErrorBeforeDb
                 <<" > "<<r.toneErrorAfterDb<<" dB predicted, "<<actual<<" dB re-rendered (prediction miss "<<r.toneErrorAfterDb-actual<<" dB)\n";
    }
    std::cout<<"MEASURE auto set crossover controller: "<<step->result<<'\n';
    std::cout<<"PASS auto set crossover controller: off by default, alone in the Matrix pass, moved for a hot HIGH lane, strength in octaves, after the lane levels, lock, Classic routing, X2 edited or routing changed while listening, silent MID lane, undo, processor prediction measured\n";
}
