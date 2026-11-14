#pragma once
// Phase 27: the Dual rig 2 and Matrix MID/HIGH cabinet cuts (docs/AUTO_SET_PHASE27.md).
// Included by AutoSetTests.cpp after the cuts, Dual and Matrix test helpers.

// The mix of weighted lanes.
std::vector<float> laneMix(const std::vector<const std::vector<float>*>& lanes,const std::vector<double>& weights) {
    std::vector<float> mix(lanes[0]->size(),0.f);
    for(size_t k=0;k<lanes.size();++k)for(size_t n=0;n<mix.size();++n)mix[n]+=float(weights[k])*(*lanes[k])[n];
    return mix;
}
// A region's fit for a re-rendered mix, against the target aligned on the mix first heard.
float laneRegionError(const std::vector<float>& heardMix,const std::vector<float>& rendered,double rate,bool low,bool bass) {
    const auto heard=measureSpectrum(heardMix,rate,bass),now=measureSpectrum(rendered,rate,bass);
    const auto target=alignedTarget(heard,Style::rock);
    return low?cutRegionErrorDb(now.bandsDb,target,heard.strongestDb,50,bass?100.f:160.f,rate):cutRegionErrorDb(now.bandsDb,target,heard.strongestDb,5000,10000,rate);
}
void laneCutsRules() {
    float worstPrediction=0;
    for(const double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{270};
        const auto a=targetNoise(false,rate,6,rng),b=targetNoise(false,rate,6,rng);
        auto fizzySource=b;shelve(fizzySource,5000,12,rate);
        auto boomySource=b;lowShelf(boomySource,100,12,rate);
        const auto rig1=withCuts(a,rate,70,9000);
        // Dual parallel at 50%: rig 2 fizzes with its HIGH cut wide open.
        const auto fizzy=withCuts(fizzySource,rate,70,14000);
        const auto fizz=analyseLaneCuts({&rig1,&fizzy},{.5,.5},{{1,{70.f,14000.f}}},rate,false);
        check(fizz.playingDetected && fizz.lanes.size()==1 && fizz.lanes[0].heard && fizz.lanes[0].highChanged() && fizz.lanes[0].highAfter<14000.f && fizz.lanes[0].highAfter>=7000.f-1e-3f
              && std::fmod(fizz.lanes[0].highAfter,100.f)==0 && fizz.lanes[0].highErrorAfterDb<fizz.lanes[0].highErrorBeforeDb-cutMinGainDb,"Rig 2's fizz did not lower its HIGH cut");
        // The prediction against rig 2 re-rendered at the new cut.
        const auto heardMix=laneMix({&rig1,&fizzy},{.5,.5});
        const auto reRendered=withCuts(fizzySource,rate,fizz.lanes[0].lowAfter,fizz.lanes[0].highAfter);
        const float actual=laneRegionError(heardMix,laneMix({&rig1,&reRendered},{.5,.5}),rate,false,false);
        worstPrediction=std::max(worstPrediction,std::abs(actual-fizz.lanes[0].highErrorAfterDb));
        // Boom: rig 2's LOW cut rises, never past the Drop A ceiling.
        const auto boomy=withCuts(boomySource,rate,30,9000);
        const auto boom=analyseLaneCuts({&rig1,&boomy},{.5,.5},{{1,{30.f,9000.f}}},rate,false);
        check(boom.lanes[0].lowChanged() && boom.lanes[0].lowAfter>30.f && boom.lanes[0].lowAfter<=cutLowLimit(false,Style::rock),"Rig 2's boom did not raise its LOW cut within the limit");
        const auto reBoom=withCuts(boomySource,rate,boom.lanes[0].lowAfter,boom.lanes[0].highAfter);
        const float actualLow=laneRegionError(laneMix({&rig1,&boomy},{.5,.5}),laneMix({&rig1,&reBoom},{.5,.5}),rate,true,false);
        worstPrediction=std::max(worstPrediction,std::abs(actualLow-boom.lanes[0].lowErrorAfterDb));
        // Re-run at its own result, rig 2's cuts settle within a few runs (each moves at most an octave).
        std::array<float,2> cuts{70.f,9000.f};int runs=0;
        for(;runs<6;++runs) {
            const auto rendered=withCuts(b,rate,cuts[0],cuts[1]);
            const auto again=analyseLaneCuts({&rig1,&rendered},{.5,.5},{{1,cuts}},rate,false);
            if(!again.changed())break;
            cuts={again.lanes[0].lowAfter,again.lanes[0].highAfter};
        }
        check(runs<6 && cuts[0]<=cutLowLimit(false,Style::rock),"Rig 2's cuts did not settle");
        // An inverted capture with a -1 weight is the same mix: the same choice.
        auto negated=fizzy;for(auto& v:negated)v=-v;
        const auto inverted=analyseLaneCuts({&rig1,&negated},{.5,-.5},{{1,{70.f,14000.f}}},rate,false);
        check(inverted.lanes[0].highAfter==fizz.lanes[0].highAfter && inverted.lanes[0].lowAfter==fizz.lanes[0].lowAfter,"A lane's polarity weight changed the choice");
        // Matrix: the HIGH lane fizzes; its HIGH cut falls, the MID lane keeps an even top.
        auto lanes=matrixLanes(rate,6,rng);
        auto fizzHigh=lanes.high;shelve(fizzHigh,5000,12,rate);
        const auto mid=withCuts(lanes.mid,rate,70,9000),high=withCuts(fizzHigh,rate,70,14000),low=withCuts(lanes.low,rate,70,9000);
        const auto matrix=analyseLaneCuts({&low,&mid,&high},{1,1,1},{{1,{70.f,9000.f}},{2,{70.f,14000.f}}},rate,false);
        check(matrix.lanes.size()==2 && matrix.lanes[1].highChanged() && matrix.lanes[1].highAfter<14000.f,"The Matrix HIGH lane's fizz did not lower its HIGH cut");
        const auto reHigh=withCuts(fizzHigh,rate,matrix.lanes[1].lowAfter,matrix.lanes[1].highAfter);
        const auto reMid=withCuts(lanes.mid,rate,matrix.lanes[0].lowAfter,matrix.lanes[0].highAfter);
        const float actualMatrix=laneRegionError(laneMix({&low,&mid,&high},{1,1,1}),laneMix({&low,&reMid,&reHigh},{1,1,1}),rate,false,false);
        worstPrediction=std::max(worstPrediction,std::abs(actualMatrix-matrix.lanes[1].highErrorAfterDb));
        // A silent lane is reported and kept; silence is not playing.
        const std::vector<float> quiet(rig1.size(),0.f);
        const auto silent=analyseLaneCuts({&rig1,&quiet},{.5,.5},{{1,{70.f,9000.f}}},rate,false);
        check(silent.playingDetected && !silent.lanes[0].heard && !silent.changed(),"A silent rig 2 was judged");
        check(!analyseLaneCuts({&quiet,&quiet},{.5,.5},{{1,{70.f,9000.f}}},rate).playingDetected,"Silence reported as playing");
        if(rate==48000.)std::cout<<"MEASURE lane cuts rules 48 kHz: rig 2 fizz HIGH 14 kHz > "<<fizz.lanes[0].highAfter<<" Hz (top "<<fizz.lanes[0].highErrorBeforeDb<<" > "<<fizz.lanes[0].highErrorAfterDb
                                <<" dB predicted, "<<actual<<" re-rendered); boom LOW 30 > "<<boom.lanes[0].lowAfter<<" Hz (low end "<<boom.lanes[0].lowErrorBeforeDb<<" > "<<boom.lanes[0].lowErrorAfterDb<<" predicted, "<<actualLow
                                <<" re-rendered); Matrix HIGH lane HIGH 14 kHz > "<<matrix.lanes[1].highAfter<<" Hz ("<<matrix.lanes[1].highErrorAfterDb<<" predicted, "<<actualMatrix<<" re-rendered)\n";
    }
    check(worstPrediction<.5f,"The lane cut prediction misses the re-rendered mix by "+std::to_string(worstPrediction)+" dB");
    std::cout<<"MEASURE lane cuts prediction vs re-rendered mix: worst "<<worstPrediction<<" dB\n";
    std::cout<<"PASS auto set lane cuts rules at 44.1/48/96 kHz: rig 2 fizz and boom, re-runs settle, polarity weight, Matrix HIGH lane, silent lane, silence, prediction vs re-render\n";
}
// The real processor's Dual rigs (the default heads; rig 2 driven hard) at rig 2's cuts.
std::pair<std::vector<float>,std::vector<float>> laneCutsProcessorRigs(const std::vector<float>& x,float low2,float high2) {
    auto q=std::make_unique<ChimeraProcessor>();q->setRateAndBufferSizeDetails(48000,256);q->prepareToPlay(48000,256);
    for(const auto& [id,v]:std::initializer_list<std::pair<juce::String,float>>{{"mode",1},{"dualtype",0},{"dualblend",.5f},{"drive2",1.f},{"cabhigh1",5000},{"cablow2",low2},{"cabhigh2",high2}})
        q->parameters().getParameter(id)->setValueNotifyingHost(q->parameters().getParameter(id)->convertTo0to1(v));
    juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
    size_t offset=0;
    for(;offset<48*256;offset+=256){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+offset,256);q->processBlock(io,midi);}
    q->autoSetCapture().listen({Tap::lane1,Tap::lane2});
    for(;offset+256<=x.size();offset+=256){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+offset,256);q->processBlock(io,midi);}
    std::pair<std::vector<float>,std::vector<float>> rigs;q->autoSetCapture().drainSamples(rigs.first);q->autoSetCapture().drainAmpSamples(rigs.second);
    q->releaseResources();
    const size_t n=std::min(rigs.first.size(),rigs.second.size());rigs.first.resize(n);rigs.second.resize(n);
    return rigs;
}
Controller::Options laneCutsOnlyOptions() {
    // The input pass is not needed: the lane cuts step listens in the Dual or Matrix pass.
    auto o=roomOnlyOptions(Style::rock);o.room=false;o.laneCuts=true;o.locks|=Controller::lockInput|Controller::lockGate;
    return o;
}
void controllerLaneCuts() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto setter=[](std::initializer_list<std::pair<juce::String,float>> pairs){
        std::vector<std::pair<juce::String,float>> list(pairs);
        return [list](ChimeraProcessor& p){for(const auto& [id,v]:list){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));}};
    };
    check(!Controller::Options{}.laneCuts && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.laneCuts,"Lane cabinet cuts are on by default");
    std::mt19937 rng{271};
    const auto a=targetNoise(false,48000,8,rng),b=targetNoise(false,48000,8,rng);
    auto fizzySource=b;shelve(fizzySource,5000,12,48000);
    const auto rig1=withCuts(a,48000,70,9000),rig2=withCuts(fizzySource,48000,70,14000);
    const auto dual=setter({{"mode",1},{"dualtype",0},{"dualblend",.5f},{"cab2",1},{"cablow2",70},{"cabhigh2",14000}});
    // Dual: the step runs alone in the Dual pass and lowers rig 2's HIGH cut.
    const auto full=runDual(laneCutsOnlyOptions(),rig1,rig2,dual);
    const auto* step=stepOf(full.status,P::laneCuts);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::dual} && step && step->changed && step->result.contains("rig 2 HIGH 14.0 kHz > "),
          "Lane cuts step did not run or report: "+(step?step->result.toStdString():std::string("no step")));
    check(near(value(full.values,"cabhigh2"),full.laneCuts.lanes[0].highAfter,1.f) && value(full.values,"cabhigh2")<14000.f && near(value(full.values,"cabhigh1"),value(full.before,"cabhigh1"),1e-3f),"Applied rig 2 cut does not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the lane cuts");
    // Strength scales the move in Hz.
    auto half=laneCutsOnlyOptions();half.strength=.5f;
    const auto halved=runDual(half,rig1,rig2,dual);
    check(near(value(halved.values,"cabhigh2"),14000.f+.5f*(halved.laneCuts.lanes[0].highAfter-14000.f),1.f),"Strength did not scale the lane cut");
    // Kept: the lock, rig 2's cabinet off, the cuts edited while listening, a silent rig 2.
    auto lockedOptions=laneCutsOnlyOptions();lockedOptions.locks=Controller::lockCompressor|Controller::lockLaneCuts;
    const auto locked=runDual(lockedOptions,rig1,rig2,dual);
    check(stepOf(locked.status,P::laneCuts) && stepOf(locked.status,P::laneCuts)->result=="kept (locked)" && locked.status.passes==std::vector<P>{P::input},"Lane cuts lock did not keep the cuts or still listened");
    const auto cabOff=runDual(laneCutsOnlyOptions(),rig1,rig2,[&](ChimeraProcessor& p){dual(p);setter({{"cab2",0}})(p);});
    check(stepOf(cabOff.status,P::laneCuts)->result=="kept (rig 2 cabinet off)" && near(value(cabOff.values,"cabhigh2"),14000.f,1.f),"A cabinet that is off had its cuts moved");
    const auto edited=runDual(laneCutsOnlyOptions(),rig1,rig2,dual,setter({{"cabhigh2",12000}}));
    check(stepOf(edited.status,P::laneCuts)->result=="kept (lane cuts edited while listening)" && near(value(edited.values,"cabhigh2"),12000.f,1.f),"A cut edited while listening was overwritten");
    const std::vector<float> quiet(rig1.size(),0.f);
    const auto silent=runDual(laneCutsOnlyOptions(),rig1,quiet,dual);
    check(stepOf(silent.status,P::laneCuts)->result.contains("rig 2 silent"),"A silent rig 2 was not reported");
    // Classic routing: no lane pass; the Cabinet cuts step covers rig 1.
    auto classicOptions=laneCutsOnlyOptions();classicOptions.locks=Controller::lockCompressor;
    const auto classic=runSpace(classicOptions);
    check(stepOf(classic.status,P::laneCuts) && stepOf(classic.status,P::laneCuts)->result=="kept (Classic routing: the Cabinet cuts step sets rig 1)","Classic routing not reported");
    // Matrix: the HIGH lane fizzes; its HIGH cut falls.
    auto lanes=matrixLanes(48000,8,rng);shelve(lanes.high,5000,12,48000);
    lanes.low=withCuts(lanes.low,48000,70,9000);lanes.mid=withCuts(lanes.mid,48000,70,9000);lanes.high=withCuts(lanes.high,48000,70,14000);
    const auto matrix=runMatrix(laneCutsOnlyOptions(),lanes,setter({{"mode",2},{"cab2",1},{"cab3",1},{"cablow2",70},{"cabhigh2",9000},{"cablow3",70},{"cabhigh3",14000}}));
    const auto* matrixStep=stepOf(matrix.status,P::laneCuts);
    check(matrix.status.passes==std::vector<P>{P::matrix} && matrixStep && matrixStep->changed && matrixStep->result.contains("HIGH lane HIGH 14.0 kHz > ") && value(matrix.values,"cabhigh3")<14000.f,
          "Matrix HIGH lane cut not lowered: "+(matrixStep?matrixStep->result.toStdString():std::string("no step")));
    // With the levels step, the lane levels row comes first and the cuts judge the levelled lanes.
    auto withLevels=laneCutsOnlyOptions();withLevels.matrix=true;
    const auto levelled=runMatrix(withLevels,lanes,setter({{"mode",2},{"cab2",1},{"cab3",1},{"cablow2",70},{"cabhigh2",9000},{"cablow3",70},{"cabhigh3",14000}}));
    int levelsAt=-1,cutsAt=-1;for(size_t i=0;i<levelled.status.steps.size();++i){if(levelled.status.steps[i].phase==P::matrix)levelsAt=int(i);if(levelled.status.steps[i].phase==P::laneCuts)cutsAt=int(i);}
    check(levelsAt>=0 && cutsAt>levelsAt,"The lane cuts row does not follow the Matrix levels row");
    // Real heads (measured): rig 2 driven hard through the processor's default Dual heads, rig 1 cut at 5 kHz, so the fizz is rig 2's; re-rendered at the proposed cut.
    const auto source=pluckedEvery(250,8,48000,6);
    std::vector<float> dry(source.size());for(size_t n=0;n<dry.size();++n)dry[n]=source[n]+.2f*a[n%a.size()];
    const auto heads=laneCutsProcessorRigs(dry,70,16000);
    // What the step chose, and, whatever it chose, a fixed move of both cuts predicted and re-rendered.
    const auto real=analyseLaneCuts({&heads.first,&heads.second},{.5,.5},{{1,{70.f,16000.f}}},48000,false);
    check(real.playingDetected && real.lanes.size()==1 && real.lanes[0].heard,"Real-head rigs not heard");
    const std::array<float,2> probe{140.f,6000.f};
    const auto predicted=analyseLaneCuts({&heads.first,&heads.second},{.5,.5},{{1,{70.f,16000.f}}},48000,false,Style::rock,probe);
    const auto again=laneCutsProcessorRigs(dry,probe[0],probe[1]);
    const auto mixBefore=laneMix({&heads.first,&heads.second},{.5,.5}),mixAfter=laneMix({&again.first,&again.second},{.5,.5});
    const float renderedLow=laneRegionError(mixBefore,mixAfter,48000,true,false),renderedTop=laneRegionError(mixBefore,mixAfter,48000,false,false);
    const float missLow=renderedLow-predicted.lanes[0].lowErrorAfterDb,missTop=renderedTop-predicted.lanes[0].highErrorAfterDb;
    check(std::abs(missLow)<.5f && std::abs(missTop)<.5f,"Real-head lane cut prediction off by "+std::to_string(missLow)+" / "+std::to_string(missTop)+" dB");
    std::cout<<"MEASURE lane cuts real heads (Dual 50%, rig 1 HIGH 5 kHz, rig 2 drive 1.0, rig 2 cuts 70 Hz / 16 kHz): step "
             <<(real.lanes[0].changed()?"LOW "+std::to_string(int(real.lanes[0].lowAfter))+" Hz, HIGH "+std::to_string(int(real.lanes[0].highAfter))+" Hz":std::string("kept"))
             <<" (low end "<<real.lanes[0].lowErrorBeforeDb<<" dB, top "<<real.lanes[0].highErrorBeforeDb<<" dB); probe 140 Hz / 6 kHz: low end "<<predicted.lanes[0].lowErrorAfterDb<<" predicted, "<<renderedLow
             <<" re-rendered; top "<<predicted.lanes[0].highErrorAfterDb<<" predicted, "<<renderedTop<<" re-rendered\n";
    std::cout<<"MEASURE auto set lane cuts controller: "<<step->result<<" | Matrix: "<<matrixStep->result<<'\n';
    std::cout<<"PASS auto set lane cuts controller: off by default; Dual rig 2 and Matrix HIGH lane cuts; strength; lock; cabinet off; edited while listening; silent rig; Classic; after the levels; undo\n";
}
