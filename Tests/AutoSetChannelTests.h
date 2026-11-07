#pragma once
// Phase 20: the amp channel. Included by AutoSetTests.cpp after the amp tests (syntheticAmp, playedNotes, roleControls).

// A synthetic channel: syntheticAmp with GAIN limited to [low, high], now at `now`.
ChannelCandidate syntheticChannel(const AmpRenderer& amp,float now,float low,float high) {
    return {{true,now,low,high},[amp,now](std::optional<float> g){return amp({g?*g:now,5,5,5});}};
}
void channelRules() {
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{200};const auto di=playedNotes(rate,ampRenderSeconds,rng);
        const auto amp=syntheticAmp(di,rate);
        const auto clean=syntheticChannel(amp,1.5f,0,3),lead=syntheticChannel(amp,8.5f,7,10);
        const std::vector<ChannelCandidate> both{clean,lead};
        const auto at=[&](int current,Style style){return analyseChannel(di,both[size_t(current)].render(std::nullopt),rate,current,both,false,style);};
        // A clean channel cannot reach a metal drive: the lead channel can.
        const auto toLead=at(0,Style::metal);
        check(toLead.playingDetected && !toLead.bass && near(toLead.target,.65f,1e-6f) && toLead.reach.size()==2 && toLead.reach[0].present && toLead.reach[1].present,"Channel analysis did not hear both channels");
        check(toLead.reach[0].high<.65f-ampCompressionTolerance && toLead.missBefore>0 && toLead.changed() && toLead.proposed==1 && channelDistance(toLead.reach[1],toLead.target)<channelDistance(toLead.reach[0],toLead.target)-ampCompressionTolerance,
              "A clean channel short of a metal drive did not move to the lead channel");
        // A lead channel cannot stay clean: the clean channel can.
        const auto toClean=at(1,Style::clean);
        check(toClean.reach[1].low>.15f+ampCompressionTolerance && toClean.changed() && toClean.proposed==0 && channelDistance(toClean.reach[0],toClean.target)<channelDistance(toClean.reach[1],toClean.target)-ampCompressionTolerance,"A lead channel too driven for clean did not move to the clean channel (lead "+std::to_string(toClean.reach[1].low)+"-"+std::to_string(toClean.reach[1].high)+", clean "+std::to_string(toClean.reach[0].low)+"-"+std::to_string(toClean.reach[0].high)+")");
        // A channel that reaches the target stays.
        const std::vector<ChannelCandidate> wide{syntheticChannel(amp,5,0,10),lead};
        const auto stay=analyseChannel(di,wide[0].render(std::nullopt),rate,0,wide,false,Style::clean);
        check(!stay.changed() && stay.missBefore==0,"A channel that reaches the target was changed");
        const auto reach=[&](const ChannelReach& c,float t){return t>=c.low-ampCompressionTolerance && t<=c.high+ampCompressionTolerance;};
        const auto rock=at(1,Style::rock);
        check(rock.changed()==(!reach(rock.reach[1],.45f) && channelDistance(rock.reach[0],.45f)<channelDistance(rock.reach[1],.45f)-ampCompressionTolerance),"Rock kept or moved the lead channel against the reach");
        // The reach brackets the drive at the channel's own knobs, and follows GAIN.
        for(const auto& c:toLead.reach)check(c.low<=c.now+1e-6f && c.now<=c.high+1e-6f,"Channel reach does not contain its own drive");
        check(near(toLead.reach[0].now,ampCompression(di,amp({1.5f,5,5,5}),rate),1e-6f) && near(toLead.reach[0].high,ampCompression(di,amp({3,5,5,5}),rate),1e-6f),"Channel reach is not the rendered drive");
        // No closer channel: kept. A silent channel is not a candidate.
        const std::vector<ChannelCandidate> mute{clean,{{true,8.5f,7,10},[&](std::optional<float>){return std::vector<float>(di.size());}}};
        const auto alone=analyseChannel(di,clean.render(std::nullopt),rate,0,mute,false,Style::metal);
        check(alone.missBefore>0 && !alone.changed() && !alone.reach[1].present,"A silent channel was chosen");
        const std::vector<ChannelCandidate> one{clean};
        check(!analyseChannel(di,clean.render(std::nullopt),rate,0,one,false,Style::metal).changed(),"One channel moved");
        // A third channel closer still wins; ties keep the lower number.
        const std::vector<ChannelCandidate> three{clean,syntheticChannel(amp,5,4,6),lead};
        const auto best=analyseChannel(di,clean.render(std::nullopt),rate,0,three,false,Style::metal);
        check(best.changed() && channelMiss(best.reach[size_t(best.proposed)],best.target)==best.missAfter && best.missAfter<=channelMiss(best.reach[1],best.target)
              && best.missAfter<=channelMiss(best.reach[2],best.target),"The closest channel was not chosen");
        // The tolerance counts once: a channel 9% short of the target moves to one
        // that reaches it (9% closer, more than the 8% tolerance).
        if(rate==48000.) {
            const auto reachOf=[](float low,float high){return ChannelReach{true,low,low,high};};
            const float t=ampCompressionTarget(false,Style::metal);
            check(channelChoice({reachOf(0,t-.09f),reachOf(.3f,.9f)},0,t)==1,"A channel 9% short did not move to one that reaches the target");
            check(channelChoice({reachOf(0,t-.07f),reachOf(.3f,.9f)},0,t)==0,"A channel within the tolerance was moved");
            check(channelChoice({reachOf(0,t-.12f),reachOf(0,t-.05f)},0,t)==0,"A channel only 7% closer was chosen");
            check(channelChoice({reachOf(0,t-.20f),reachOf(0,t-.11f)},0,t)==1,"A channel 9% closer, still short, was not chosen");
            check(channelChoice({reachOf(0,t-.20f),reachOf(.3f,.9f),reachOf(.3f,.9f)},0,t)==1,"A tie did not keep the lower channel");
        }
        // Silence.
        const std::vector<float> silence(di.size());
        check(!analyseChannel(silence,silence,rate,0,both,false).playingDetected,"Silence reported as playing");
        if(rate==48000.)std::cout<<"MEASURE channel rules 48 kHz: clean reaches "<<toLead.reach[0].low<<"-"<<toLead.reach[0].high<<", lead "<<toLead.reach[1].low<<"-"<<toLead.reach[1].high
                                <<"; metal clean > "<<toLead.proposed<<" (miss "<<toLead.missBefore<<" > "<<toLead.missAfter<<"); clean lead > "<<toClean.proposed<<"; rock on lead "<<(rock.changed()?"moved":"kept")<<'\n';
    }
    std::cout<<"PASS auto set channel rules at 44.1/48/96 kHz: clean to lead for metal, lead to clean for clean, reaching channel kept, reach is the rendered drive, silent channel and single channel kept, closest of three, tolerance counted once, silence\n";
}
struct ChannelRun {std::vector<float> before,values,undone;Controller::Status status;ChannelResult analysis;AmpResult amp;};
// A real processor with native head `model` on `channel` in Classic routing; its
// rendered output is fed as the amp/cab tap.
ChannelRun runChannel(const Controller::Options& options,int model,int channel,const std::function<void(ChimeraProcessor&)>& prepare={},
                      const std::function<void(ChimeraProcessor&)>& during={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    setRaw("mode",0);setRaw("ampon1",1);setRaw(spectralforge::ampNativeEnabledID(0),1);setRaw(spectralforge::ampNativeModelID(0),float(model));
    setRaw(spectralforge::ampNativeChannelID(0,model),float(channel));
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    ChannelRun result;result.before=values(*p);
    std::mt19937 rng{63};const auto di=playedNotes(48000,8,rng);
    const auto post=nativeAmpRenderer(p->autoSetAmpState(0),{-1,-1,-1,-1},di,48000)({});
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=di.size();offset+=256) {
        const float* in[]{di.data()+offset};const float* out[]{post.data()+offset};
        p->autoSetCapture().push(Tap::amp,in,1,256);
        for(auto tap:{Tap::cab,Tap::input,Tap::compressor})p->autoSetCapture().push(tap,tap==Tap::cab?out:in,1,256);
        ms+=1000.*256/48000;c.tick(ms);
        if(during && offset==0)during(*p);
    }
    check(!c.busy(),"Channel run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastChannelAnalysis();result.amp=c.lastAmpAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerChannel() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    constexpr int model=2,rhythm=0,lead=1; // RHYTHM reaches about 5-75% drive, LEAD about 42-90%
    const auto channelID=spectralforge::ampNativeChannelID(0,model);
    Controller::Options channelOnly{1,false,Style::clean,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,true,false};
    // Clean on LEAD: the channel moves to RHYTHM, alone in the amp pass.
    const auto full=runChannel(channelOnly,model,lead);
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::channel);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::amp} && step && step->changed && step->result.contains("CHANNEL LEAD > RHYTHM") && !stepOf(full.status,P::amp),
          "Channel step did not run alone in the amp pass or report");
    check(a.playingDetected && a.before==lead && a.proposed==rhythm && near(a.target,.15f,1e-6f) && juce::roundToInt(value(full.values,channelID))==rhythm,"Applied channel does not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the channel");
    // Rock on LEAD: LEAD reaches the drive, so it stays.
    auto rockOptions=channelOnly;rockOptions.style=Style::rock;
    const auto rock=runChannel(rockOptions,model,lead);
    check(!stepOf(rock.status,P::channel)->changed && stepOf(rock.status,P::channel)->result.contains("LEAD reaches") && rock.values==rock.before,"A channel that reaches the drive was changed");
    // Strength does not scale a channel switch.
    auto halfOptions=channelOnly;halfOptions.strength=.5f;
    const auto half=runChannel(halfOptions,model,lead);
    check(juce::roundToInt(value(half.values,channelID))==rhythm,"A channel switch was scaled by strength");
    // With the amp step: the channel row comes first and the amp step sets GAIN on the new channel.
    auto withAmp=channelOnly;withAmp.amp=true;
    const auto both=runChannel(withAmp,model,lead);
    const auto& steps=both.status.steps;
    const auto channelRow=std::find_if(steps.begin(),steps.end(),[](const auto& s){return s.phase==P::channel;}),ampRow=std::find_if(steps.begin(),steps.end(),[](const auto& s){return s.phase==P::amp;});
    check(channelRow!=steps.end() && ampRow!=steps.end() && channelRow<ampRow && channelRow->changed && !ampRow->result.contains("changed while listening"),"The channel row does not come before the amp row");
    const auto controls=roleControls(model,rhythm,0);
    const auto rhythmGain=spectralforge::ampNativeControlID(0,model,controls[ampGain],rhythm);
    check(both.amp.playingDetected && near(both.amp.compressionBefore,both.analysis.reach[size_t(rhythm)].now,.02f)
          && (!both.amp.changed(ampGain) || near(value(both.values,rhythmGain),both.amp.proposed[ampGain].value,.011f*(both.amp.before[ampGain].maximum-both.amp.before[ampGain].minimum))),
          "The amp step did not work on the new channel");
    // The lock keeps the channel and, alone, skips the pass.
    auto lockedOptions=channelOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockChannel;
    const auto locked=runChannel(lockedOptions,model,lead);
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::channel)->result=="kept (locked)" && juce::roundToInt(value(locked.values,channelID))==lead,"Channel lock did not keep the channel");
    // A one-channel head, Dual routing and the classic engine: kept, with the reason.
    auto freeOptions=channelOnly;freeOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockCompressor;
    int single=-1;for(int m=0;m<spectralforge::ampModelCount && single<0;++m)if(spectralforge::ampNativePanel(m).channels.size()==1)single=m;
    if(single>=0) {
        const auto one=runChannel(freeOptions,single,0);
        check(stepOf(one.status,P::channel)->result.contains("one channel") && one.values==one.before,"A one-channel head was changed");
    }
    const auto dual=runChannel(freeOptions,model,lead,[](ChimeraProcessor& p){p.parameters().getParameter("mode")->setValueNotifyingHost(p.parameters().getParameter("mode")->convertTo0to1(1));});
    check(stepOf(dual.status,P::channel)->result.contains("Dual and Matrix") && dual.values==dual.before,"Dual routing channel was changed");
    // The channel changing while listening voids the step.
    const auto edited=runChannel(freeOptions,model,lead,{},[&](ChimeraProcessor& p){p.parameters().getParameter(channelID)->setValueNotifyingHost(p.parameters().getParameter(channelID)->convertTo0to1(float(rhythm)));});
    check(stepOf(edited.status,P::channel)->result.contains("changed while listening") && !stepOf(edited.status,P::channel)->changed,"A channel edited while listening did not void the step");
    // Metal on the clean channel of a head whose CLEAN channel stops short of it.
    const int heavy=22;
    auto metalOptions=freeOptions;metalOptions.style=Style::metal;
    const auto metal=runChannel(metalOptions,heavy,0);
    check(stepOf(metal.status,P::channel)->changed && juce::roundToInt(value(metal.values,spectralforge::ampNativeChannelID(0,heavy)))==1,"A clean channel short of a metal drive did not move");
    std::cout<<"MEASURE auto set channel controller: "<<step->result<<"; with the amp step: "<<ampRow->result<<"; metal: "<<stepOf(metal.status,P::channel)->result<<'\n';
    std::cout<<"PASS auto set channel controller: channel step alone in the amp pass, clean moves LEAD to RHYTHM, rock keeps LEAD, strength not applied, amp step sets GAIN on the new channel, lock, one-channel head, Dual, edited while listening, metal moves CLEAN to LEAD, undo\n";
}
