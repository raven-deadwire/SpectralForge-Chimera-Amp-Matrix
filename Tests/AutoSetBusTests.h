#pragma once
// Phase 16: the POST bus compressor (Console VCA). Included by AutoSetTests.cpp after its helpers.

// Plucked notes with soft and hard picking (8 dB apart), the input a bus compressor glues.
std::vector<float> dynamicPlaying(double rate,double seconds,std::mt19937& rng) {
    auto x=playedNotes(rate,seconds,rng);
    const size_t note=size_t(rate*.25);
    for(size_t n=0;n<x.size();++n)x[n]*=((n/note)%3==0)?1.f:.4f;
    scale(x,2.f);
    return x;
}
void busRules() {
    const auto section=spectralforge::defaultPostNativeState(true).sections[0];
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{160};
        const auto x=dynamicPlaying(rate,busRenderSeconds,rng);
        const auto renderFor=nativeBusRenderer(section,x,rate);
        // The glue measure rises as the threshold falls.
        const BusSetting rock{-18,0,5,4,0};
        const auto render=renderFor(rock);
        const float high=busGlue(x,render(-6),rate).loudDb,low=busGlue(x,render(-30),rate).loudDb;
        check(low>high+1.f,"Gain reduction does not rise as the threshold falls ("+std::to_string(high)+" / "+std::to_string(low)+" dB)");
        for(const Style style:{Style::clean,Style::rock,Style::metal}) {
            // Off: switched in with glue settings, the threshold reaching the target, make-up holding the loudness.
            const auto r=analyseBus(x,rate,{-18,0,4,1,1},false,renderFor,style);
            check(r.playingDetected && r.changed() && !r.wasOn,"An off bus compressor was not switched in");
            check(r.reachable && std::abs(r.glueAfterDb-busGlueTarget(style))<=busToleranceDb,"Glue missed its target ("+std::to_string(r.glueAfterDb)+" dB)");
            check(r.proposed.release==4 && r.proposed.ratio==(style==Style::metal?1:0) && r.proposed.attack==(style==Style::metal?4:5),"Glue settings by style");
            check(r.proposed.makeup>0 && r.proposed.makeup<=r.glueAfterDb+.5f && std::fmod(r.proposed.makeup*2,1.f)==0 && std::fmod(r.proposed.threshold*2,1.f)==0,"Make-up or threshold off the 0.5 dB grid or beyond the glue");
            // Already in at that threshold: kept.
            const auto kept=analyseBus(x,rate,r.proposed,true,renderFor,style);
            check(!kept.changed(),"A bus compressor at the target was changed");
            // Already in, squashing hard: the threshold rises; ratio, attack and release stay.
            const BusSetting squash{-40,0,0,0,2};
            const auto eased=analyseBus(x,rate,squash,true,renderFor,style);
            check(eased.changed() && eased.proposed.threshold>squash.threshold && eased.proposed.ratio==2 && eased.proposed.attack==0 && eased.proposed.release==0
                  && eased.glueAfterDb<eased.glueBeforeDb,"A squashing compressor was not eased (or its ratio, attack or release changed)");
        }
        // Strength scales the glue target.
        const auto half=analyseBus(x,rate,rock,false,renderFor,Style::rock,.5f);
        check(near(half.glueTargetDb,busGlueTarget(Style::rock)*.5f,1e-5f) && std::abs(half.glueAfterDb-half.glueTargetDb)<=busToleranceDb,"Strength did not scale the glue");
        check(!analyseBus(std::vector<float>(x.size()),rate,rock,false,renderFor,Style::rock).playingDetected,"Silence reported as playing");
        if(rate==48000.){const auto r=analyseBus(x,rate,rock,false,renderFor,Style::rock);
            std::cout<<"MEASURE bus rules 48 kHz: GR at -6 dB "<<high<<", -30 dB "<<low<<" dB; rock threshold "<<r.proposed.threshold<<" dB, glue "<<r.glueAfterDb<<" dB, make-up "<<r.proposed.makeup<<" dB\n";}
    }
    std::cout<<"PASS auto set bus rules at 44.1/48/96 kHz: glue rises as the threshold falls, switched in at the style target, kept at target, squash eased with the player's ratio/attack/release, strength, silence\n";
}
struct BusRun {std::vector<float> before,values,undone;Controller::Status status;BusResult analysis;};
// The de-esser + limiter pass hears the playing at the de-esser and output taps and the POST input.
BusRun runBus(const Controller::Options& options,const std::vector<float>& x,const std::function<void(ChimeraProcessor&)>& prepare={},const std::function<void(ChimeraProcessor&)>& during={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    BusRun result;result.before=values(*p);
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=x.size();offset+=256) {
        const float* a[]{x.data()+offset};
        for(auto tap:{Tap::deesser,Tap::output,Tap::post,Tap::input,Tap::compressor})p->autoSetCapture().push(tap,a,1,256);
        ms+=1000.*256/48000;c.tick(ms);
        if(during && offset==0)during(*p);
    }
    check(!c.busy(),"Bus run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastBusAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerBus() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto id=[](int c){return spectralforge::postNativeControlID(0,0,c);};
    std::mt19937 rng{161};const auto x=dynamicPlaying(48000,10,rng);
    const Controller::Options busOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,true,false,false,false,false,false,false,false};
    const auto full=runBus(busOnly,x);
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::bus);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::finish} && !stepOf(full.status,P::finish) && step && step->changed && step->result.contains("Console VCA in"),
          "Bus step did not run alone in the de-esser + limiter pass or report");
    check(value(full.values,spectralforge::postNativeBypassID(0,0))<.5f && value(full.values,id(busInControl))>.5f && near(value(full.values,id(busThresholdControl)),a.proposed.threshold,.01f)
          && near(value(full.values,id(busMakeupControl)),a.proposed.makeup,.01f) && value(full.values,id(busRatioControl))==float(a.proposed.ratio),"Applied bus compressor does not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the bus compressor");
    // Strength halves the glue target.
    auto halfOptions=busOnly;halfOptions.strength=.5f;
    const auto half=runBus(halfOptions,x);
    check(near(half.analysis.glueTargetDb,busGlueTarget(Style::rock)*.5f,1e-5f) && half.analysis.proposed.threshold>a.proposed.threshold,"Strength did not lighten the glue");
    // Its own lock keeps it and skips the pass; the de-esser and limiter locks alone keep the pass for it.
    auto lockedOptions=busOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockBus;
    const auto locked=runBus(lockedOptions,x);
    const auto busKept=[&](const BusRun& run){
        for(const auto& name:{spectralforge::postNativeBypassID(0,0),id(busThresholdControl),id(busMakeupControl),id(busRatioControl),id(busAttackControl),id(busReleaseControl),id(busInControl)})
            if(value(run.values,name)!=value(run.before,name))return false;
        return true;};
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::bus)->result=="kept (locked)" && busKept(locked),"Bus lock did not keep it");
    auto both=busOnly;both.finish=true;both.locks|=Controller::lockDeEsser|Controller::lockLimiter;
    const auto finishLocked=runBus(both,x);
    check(finishLocked.status.passes==std::vector<P>{P::finish} && stepOf(finishLocked.status,P::finish)->result.contains("kept (locked)") && stepOf(finishLocked.status,P::bus)->changed,
          "De-esser and limiter locks stopped the bus step");
    // Legacy POST engine, another model, the side-chain and a model change while listening keep it.
    const auto set=[](ChimeraProcessor& p,const juce::String& name,float v){auto* q=p.parameters().getParameter(name);q->setValueNotifyingHost(q->convertTo0to1(v));};
    const auto legacy=runBus(busOnly,x,[&](ChimeraProcessor& p){set(p,spectralforge::postNativeModeID(0),0);});
    check(stepOf(legacy.status,P::bus)->result.contains("legacy POST engine") && busKept(legacy),"Legacy POST engine bus was changed");
    const auto fet=runBus(busOnly,x,[&](ChimeraProcessor& p){set(p,spectralforge::postNativeModelID(0),1);});
    check(stepOf(fet.status,P::bus)->result.contains("Auto Set sets the Console VCA") && busKept(fet),"Another bus model was changed");
    const auto keyed=runBus(busOnly,x,[&](ChimeraProcessor& p){set(p,id(busSidechainControl),1);});
    check(stepOf(keyed.status,P::bus)->result.contains("SIDE-CHAIN") && busKept(keyed),"A keyed bus compressor was changed");
    const auto edited=runBus(busOnly,x,{},[&](ChimeraProcessor& p){set(p,spectralforge::postNativeModelID(0),2);});
    check(stepOf(edited.status,P::bus)->result.contains("changed while listening"),"A bus model change while listening was not caught");
    std::cout<<"MEASURE auto set bus controller: "<<step->result<<'\n';
    std::cout<<"PASS auto set bus controller: bus step alone in the de-esser + limiter pass, strength, own lock, de-esser/limiter locks, legacy engine, other model, side-chain and edits while listening kept, undo\n";
}
