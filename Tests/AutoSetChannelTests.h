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
            // The amp step off or locked: a channel is judged at its own knobs, so
            // one that would reach the target only by its GAIN is not chosen.
            const auto at=[](float now,float low,float high){return ChannelReach{true,now,low,high,7};};
            check(channelChoice({at(.2f,.1f,.3f),at(.2f,.1f,.9f)},0,t,true)==1,"A channel reaching the target by GAIN was not chosen");
            check(channelChoice({at(.2f,.1f,.3f),at(.2f,.1f,.9f)},0,t,false)==0,"With the amp step off, a channel was chosen for a GAIN it will not get");
            check(channelChoice({at(.2f,.1f,.3f),at(.6f,.1f,.9f)},0,t,false)==1,"With the amp step off, a channel already near the target was not chosen");
            // Strength: the amp step moves only part of the way, so a candidate must be closer at that strength.
            check(channelChoice({at(.2f,.1f,.3f),at(.2f,.1f,.6f)},0,t,true,1.f)==1,"Full strength did not move to the reaching channel");
            check(channelChoice({at(.2f,.1f,.3f),at(.2f,.1f,.6f)},0,t,true,.25f)==0,"A channel switch ignored a 25% strength");
            check(near(channelAchieved(at(.2f,.1f,.6f),t,true,.25f),.3f,1e-6f) && near(channelAchieved(at(.2f,.1f,.6f),t,false,1.f),.2f,1e-6f),"Achieved drive wrong");
        }
        if(rate==48000.) {
            // Low GAIN is probed (0 and 5% of the range), so a channel that only
            // needs a low GAIN is seen reaching low drives.
            const std::vector<ChannelCandidate> mid{syntheticChannel(amp,5,0,10),lead};
            const auto low=analyseChannel(di,mid[0].render(std::nullopt),rate,0,mid,false,Style::clean);
            check(low.reach[0].present && low.reach[0].heard>=6 && low.reach[0].low<=ampCompression(di,amp({.5f,5,5,5}),rate)+1e-6f,"GAIN below 10% was not probed");
            // A channel whose own GAIN is too quiet still counts by its audible probes.
            const ChannelCandidate quietOwn{{true,0,0,10},[amp,&di](std::optional<float> g){return g?amp({*g,5,5,5}):std::vector<float>(di.size());}};
            const std::vector<ChannelCandidate> withQuiet{clean,quietOwn};
            const auto quiet=analyseChannel(di,clean.render(std::nullopt),rate,0,withQuiet,false,Style::metal);
            check(quiet.reach[1].present && quiet.reach[1].heard>=4 && quiet.changed() && quiet.proposed==1,"A channel with a quiet stored GAIN was left out");
            // Only its own knobs audible: too little to compare, kept with the reason.
            const ChannelCandidate onlyOwn{{true,1.5f,0,3},[amp,&di](std::optional<float> g){return g?std::vector<float>(di.size()):amp({1.5f,5,5,5});}};
            const std::vector<ChannelCandidate> fewer{onlyOwn,lead};
            const auto few=analyseChannel(di,onlyOwn.render(std::nullopt),rate,0,fewer,false,Style::metal);
            check(few.hold==ChannelHold::quietSettings && !few.changed(),"One audible GAIN setting was judged");
            // Playing without soft and hard notes cannot be compared: kept with the reason.
            std::vector<float> steady(di.size());for(size_t n=0;n<steady.size();++n)steady[n]=float(.2*std::sin(2*juce::MathConstants<double>::pi*110*double(n)/rate));
            const std::vector<ChannelCandidate> steadyBoth{syntheticChannel(syntheticAmp(steady,rate),1.5f,0,3),syntheticChannel(syntheticAmp(steady,rate),8.5f,7,10)};
            const auto flat=analyseChannel(steady,steadyBoth[0].render(std::nullopt),rate,0,steadyBoth,false,Style::metal);
            check(flat.playingDetected && flat.hold==ChannelHold::steadyPlaying && !flat.changed(),"Steady playing was judged");
            // Strength and the amp step reach the analysis.
            const auto partial=analyseChannel(di,clean.render(std::nullopt),rate,0,both,false,Style::metal,true,.25f);
            check(near(partial.strength,.25f,1e-6f) && partial.proposed==channelChoice(partial.reach,0,partial.target,true,.25f),"Strength did not reach the channel choice");
            const auto fixedKnobs=analyseChannel(di,clean.render(std::nullopt),rate,0,both,false,Style::metal,false);
            check(!fixedKnobs.gainAdjustable && fixedKnobs.proposed==channelChoice(fixedKnobs.reach,0,fixedKnobs.target,false),"The amp step's absence did not reach the channel choice");
        }
        // Silence.
        const std::vector<float> silence(di.size());
        check(!analyseChannel(silence,silence,rate,0,both,false).playingDetected,"Silence reported as playing");
        if(rate==48000.)std::cout<<"MEASURE channel rules 48 kHz: clean reaches "<<toLead.reach[0].low<<"-"<<toLead.reach[0].high<<", lead "<<toLead.reach[1].low<<"-"<<toLead.reach[1].high
                                <<"; metal clean > "<<toLead.proposed<<" (miss "<<toLead.missBefore<<" > "<<toLead.missAfter<<"); clean lead > "<<toClean.proposed<<"; rock on lead "<<(rock.changed()?"moved":"kept")<<'\n';
    }
    // The step is off unless the user turns it on (it changes the head's voicing).
    check(!Controller::Options{}.channel && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.channel,"Amp channel selection is on by default");
    std::cout<<"PASS auto set channel rules at 44.1/48/96 kHz: clean to lead for metal, lead to clean for clean, reaching channel kept, reach is the rendered drive, silent channel and single channel kept, closest of three, tolerance counted once, silence; off by default, judged at the knobs without the amp step and at Strength, low GAIN probed, quiet stored GAIN counted, holds for steady playing and one audible setting\n";
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
    Controller::Options channelOnly{1,false,Style::clean,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,true,false,false,false,false};
    // Clean on LEAD: the channel moves to RHYTHM, alone in the amp pass.
    const auto full=runChannel(channelOnly,model,lead);
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::channel);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::amp} && step && step->changed && step->result.contains("CHANNEL LEAD > RHYTHM") && !stepOf(full.status,P::amp),
          "Channel step did not run alone in the amp pass or report");
    check(a.playingDetected && a.before==lead && a.proposed==rhythm && near(a.target,.15f,1e-6f) && juce::roundToInt(value(full.values,channelID))==rhythm,"Applied channel does not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the channel");
    // Without the amp step, each channel is judged at its own knobs.
    check(step->result.contains("at its knobs") && step->result.contains("amp step is off or locked") && !a.gainAdjustable,"Without the amp step the channel was not judged at its knobs");
    // Rock on LEAD with the amp step: LEAD's GAIN reaches the drive, so it stays.
    auto rockOptions=channelOnly;rockOptions.style=Style::rock;rockOptions.amp=true;
    const auto rock=runChannel(rockOptions,model,lead);
    check(!stepOf(rock.status,P::channel)->changed && stepOf(rock.status,P::channel)->result.contains("LEAD reaches") && juce::roundToInt(value(rock.values,channelID))==lead,"A channel that reaches the drive was changed");
    // A switch is not scaled by Strength (the amp step being off, its GAIN move does not count either).
    auto halfOptions=channelOnly;halfOptions.strength=.5f;
    const auto half=runChannel(halfOptions,model,lead);
    check(juce::roundToInt(value(half.values,channelID))==rhythm,"A channel switch was scaled by strength");
    // With the amp step: the channel row comes first and the amp step sets GAIN on the new channel.
    // With the amp step, LEAD can be turned down (GAIN 0-5% is probed): Clean keeps LEAD.
    auto withAmp=channelOnly;withAmp.amp=true;
    const auto turnedDown=runChannel(withAmp,model,lead);
    check(!stepOf(turnedDown.status,P::channel)->changed && juce::roundToInt(value(turnedDown.values,channelID))==lead && turnedDown.analysis.reach[size_t(lead)].low<=.15f+ampCompressionTolerance,
          "Clean moved LEAD although its GAIN can come down to the target");
    // Metal on a CLEAN channel that cannot reach it, with the amp step: the
    // channel row comes first and the amp step sets GAIN on the new channel.
    constexpr int heavyModel=22,heavyClean=0,heavyLead=1;
    auto metalAmp=withAmp;metalAmp.style=Style::metal;
    const auto both=runChannel(metalAmp,heavyModel,heavyClean);
    const auto& steps=both.status.steps;
    const auto channelRow=std::find_if(steps.begin(),steps.end(),[](const auto& s){return s.phase==P::channel;}),ampRow=std::find_if(steps.begin(),steps.end(),[](const auto& s){return s.phase==P::amp;});
    check(channelRow!=steps.end() && ampRow!=steps.end() && channelRow<ampRow && channelRow->changed && !ampRow->result.contains("changed while listening")
          && juce::roundToInt(value(both.values,spectralforge::ampNativeChannelID(0,heavyModel)))==heavyLead,"The channel row does not come before the amp row");
    const auto controls=roleControls(heavyModel,heavyLead,0);
    const auto leadGain=spectralforge::ampNativeControlID(0,heavyModel,controls[ampGain],heavyLead);
    check(both.amp.playingDetected && near(both.amp.compressionBefore,both.analysis.reach[size_t(heavyLead)].now,.02f)
          && (!both.amp.changed(ampGain) || near(value(both.values,leadGain),both.amp.proposed[ampGain].value,.011f*(both.amp.before[ampGain].maximum-both.amp.before[ampGain].minimum))),
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
    std::cout<<"MEASURE auto set channel controller: "<<step->result<<"; with the amp step: "<<channelRow->result<<" / "<<ampRow->result<<"; metal: "<<stepOf(metal.status,P::channel)->result<<'\n';
    std::cout<<"PASS auto set channel controller: channel step alone in the amp pass, clean moves LEAD to RHYTHM, rock keeps LEAD, strength not applied, amp step sets GAIN on the new channel, lock, one-channel head, Dual, edited while listening, metal moves CLEAN to LEAD, undo\n";
}
