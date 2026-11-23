#pragma once
#include <iomanip>
#include <sstream>
// Phase 44: the rig and lane passes of phases 33, 37 and 40 on the real engine, through processBlock
// (docs/AUTO_SET_PHASE44.md). Earlier tests fed the controller's taps directly.
// Included by AutoSetTests.cpp after the Matrix gain tests (DI and run() are used here).

// One whole run with the played DI going through processBlock: the engine feeds the taps itself.
// levelBeforeDb, levelAfterDb (phase 46): each lane's level on the rig tap, measured on the same notes before and after the run.
struct RigPassEngineRun {Controller::Status status;std::array<float,2> gainBefore{},gainAfter{};std::array<AmpResult,2> analysis{};std::array<bool,2> analysed{};
                         std::array<float,2> levelBeforeDb{},levelAfterDb{},trimAfter{},outputBefore{},outputAfter{};std::array<int,2> channelBefore{},channelAfter{};
                         // Phase 47: the share of each lane's rig-tap energy above 3 kHz, in dB, and its PRESENCE knob, before and after the run.
                         std::array<float,2> highShareBeforeDb{},highShareAfterDb{},presenceBefore{},presenceAfter{};std::array<PowerResult,2> power{};};
// lanes: the two lanes the passes listen to (Dual rigs 0 and 1, Matrix lanes 1 and 2).
// lowNotes: pure 82 and 98 Hz notes, which do not reach a MID or HIGH band.
RigPassEngineRun runRigPassEngine(int mode,int dualType,std::array<int,2> lanes,const Controller::Options& options,std::array<float,2> gainFraction,
                                  const std::function<void(ChimeraProcessor&)>& prepare={},bool lowNotes=false) {
    using P=Controller::Phase;
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    constexpr int model=2;
    setRaw("mode",float(mode));setRaw("dualtype",float(dualType));
    std::array<juce::String,2> gainID;
    for(int index=0;index<2;++index) {
        const int lane=lanes[size_t(index)],context=spectralforge::ampNativeContext(mode,lane);
        setRaw("ampon"+juce::String(lane+1),1);setRaw(spectralforge::ampNativeEnabledID(context),1);setRaw(spectralforge::ampNativeModelID(context),float(model));
        const int channel=p->selectedAmpChannel(lane),route=p->selectedAmpNativeRoute(lane);
        gainID[size_t(index)]=spectralforge::ampNativeControlID(context,model,roleControls(model,channel,route)[ampGain],channel);
        p->parameters().getParameter(gainID[size_t(index)])->setValueNotifyingHost(gainFraction[size_t(index)]);
    }
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    RigPassEngineRun result;
    for(int index=0;index<2;++index)result.gainBefore[size_t(index)]=get(*p,gainID[size_t(index)]);
    // Played notes with eight harmonics (82 Hz to about 2.6 kHz), so every band of a crossover gets some of the playing.
    std::mt19937 rng{441};auto di=playedNotes(48000,8,rng);size_t offset=0;
    if(lowNotes) {
        // Pure tones well under the crossovers: half-second notes at 82 and 98 Hz, no harmonics, no hiss.
        for(size_t n=0;n<di.size();++n) {
            const double t=double(n%24000)/48000.,f=(n/24000)%2?98.:82.41;
            di[n]=float(.12*std::exp(-t*5)*std::min(1.,t/.003)*std::sin(2*juce::MathConstants<double>::pi*f*t));
        }
    }
    double now=juce::Time::getMillisecondCounterHiRes();
    juce::AudioBuffer<float> b(2,256);juce::MidiBuffer midi;
    const auto play=[&](int blocks,bool tick) {
        for(int k=0;k<blocks;++k) {
            if(offset+256>di.size())offset=0;
            for(int ch=0;ch<2;++ch)b.copyFrom(ch,0,di.data()+offset,256);
            offset+=256;p->processBlock(b,midi);now+=1000.*256/48000;
            if(tick && k%4==0)c.tick(now);
        }
        if(tick)c.tick(now);
    };
    play(400,false); // let the routing settle before the run starts, as a session that has been playing
    // Phase 46: a lane's level as the rig tap hears it, on two seconds of the notes from their start.
    float highShareDb=0; // set by laneLevelDb: the lane's energy above 3 kHz (two cascaded one-pole high-passes) over its whole energy
    const auto laneLevelDb=[&](int lane) {
        p->setAutoSetRigLane(lane);offset=0;play(60,false);
        p->autoSetCapture().listen({Tap::amp,Tap::cab});offset=0;play(375,false);
        std::vector<float> amp,rig;p->autoSetCapture().drainAmpSamples(amp);p->autoSetCapture().drainSamples(rig);
        p->autoSetCapture().listen(std::nullopt);p->setAutoSetRigLane(-1);
        double energy=0,high=0;
        const double k=std::exp(-2*juce::MathConstants<double>::pi*3000./48000.);double x1=0,y1=0,x2=0,y2=0;
        for(const float v:rig) {
            energy+=double(v)*v;
            y1=k*(y1+v-x1);x1=v;y2=k*(y2+y1-x2);x2=y1;high+=y2*y2;
        }
        highShareDb=float(10*std::log10((high+1e-20)/(energy+1e-20)));
        return rig.empty()?-200.f:float(10*std::log10(energy/double(rig.size())+1e-20));
    };
    const auto presenceOf=[&](int lane) {
        const int channel=p->selectedAmpChannel(lane),control=powerControl(model,channel,powerPresence);
        return control<0?-1.f:get(*p,spectralforge::ampNativeControlID(spectralforge::ampNativeContext(mode,lane),model,control,channel));
    };
    const auto read=[&](std::array<float,2>& output,std::array<int,2>& channel) {
        for(int index=0;index<2;++index) {
            output[size_t(index)]=get(*p,spectralforge::ampNativeOutputLevelID(spectralforge::ampNativeContext(mode,lanes[size_t(index)])));
            channel[size_t(index)]=p->selectedAmpChannel(lanes[size_t(index)]);
        }
    };
    for(int index=0;index<2;++index) {
        result.levelBeforeDb[size_t(index)]=laneLevelDb(lanes[size_t(index)]);result.highShareBeforeDb[size_t(index)]=highShareDb;
        result.presenceBefore[size_t(index)]=presenceOf(lanes[size_t(index)]);
    }
    read(result.outputBefore,result.channelBefore);
    offset=0;play(200,false);
    c.start();
    P last=P::idle;
    for(int step=0;c.busy() && step<240;++step) {
        const P phase=c.current().phase;
        play(47,true);
        // The analysis of a pass, read when the controller leaves it.
        if(c.current().phase!=phase || !c.busy()) {
            const int lane=Controller::dualAmpLane(phase);
            for(int index=0;index<2;++index)if(lane==lanes[size_t(index)] && phase!=last){result.analysis[size_t(index)]=c.lastAmpAnalysis();result.power[size_t(index)]=c.lastPowerAnalysis();result.analysed[size_t(index)]=true;}
            last=phase;
        }
    }
    check(!c.busy(),"The run on the real engine did not finish");
    result.status=c.current();
    for(int index=0;index<2;++index)result.gainAfter[size_t(index)]=get(*p,gainID[size_t(index)]);
    read(result.outputAfter,result.channelAfter);
    for(int index=0;index<2;++index) {
        result.trimAfter[size_t(index)]=get(*p,spectralforge::ampNativeInputTrimID(spectralforge::ampNativeContext(mode,lanes[size_t(index)])));
        result.levelAfterDb[size_t(index)]=laneLevelDb(lanes[size_t(index)]);result.highShareAfterDb[size_t(index)]=highShareDb;
        result.presenceAfter[size_t(index)]=presenceOf(lanes[size_t(index)]);
    }
    p->releaseResources();
    return result;
}
// The engine's rig tap for a muted or solo-silenced lane is silent, as phase 43 assumes.
void rigPassEngineMute() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    std::mt19937 rng{440};const auto x=targetNoise(false,48000,1,rng);
    const auto rigEnergy=[&](int mode,int lane,const char* id) {
        for(const auto* each:{"mute1","mute2","mute3","solo1","solo2","solo3"})setRaw(each,0);
        setRaw("mode",float(mode));setRaw("dualtype",0);if(id)setRaw(id,1);
        p->setAutoSetRigLane(lane);
        juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
        const auto run=[&](int blocks){for(int block=0;block<blocks;++block){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+size_t(block%20)*256,256);p->processBlock(io,midi);}};
        run(80);
        p->autoSetCapture().listen({Tap::amp,Tap::cab});run(20);
        std::vector<float> amp,rig;p->autoSetCapture().drainAmpSamples(amp);p->autoSetCapture().drainSamples(rig);
        p->autoSetCapture().listen(std::nullopt);
        double energy=0;for(const float v:rig)energy+=double(v)*v;
        check(rig.size()==size_t(20*256),"The rig tap was not filled");
        return energy;
    };
    const double dualHeard=rigEnergy(1,0,nullptr),dualMuted=rigEnergy(1,0,"mute1"),dualOtherSolo=rigEnergy(1,0,"solo2"),dualOwnSolo=rigEnergy(1,0,"solo1");
    check(dualHeard>0 && dualMuted<1e-9*dualHeard && dualOtherSolo<1e-9*dualHeard && dualOwnSolo>.5*dualHeard,
          "The engine's rig tap does not follow MUTE and SOLO for a Dual rig ("+std::to_string(dualMuted/dualHeard)+", "+std::to_string(dualOtherSolo/dualHeard)+", "+std::to_string(dualOwnSolo/dualHeard)+")");
    const double laneHeard=rigEnergy(2,2,nullptr),laneMuted=rigEnergy(2,2,"mute3"),laneLowSolo=rigEnergy(2,2,"solo1");
    check(laneHeard>0 && laneMuted<1e-9*laneHeard && laneLowSolo<1e-9*laneHeard,
          "The engine's rig tap does not follow MUTE and SOLO for a Matrix lane ("+std::to_string(laneMuted/laneHeard)+", "+std::to_string(laneLowSolo/laneHeard)+")");
    p->setAutoSetRigLane(-1);p->releaseResources();
    std::cout<<"PASS auto set rig pass engine mute: the rig tap is silent for a muted Dual rig or Matrix lane and for one silenced by another's SOLO, and heard when it is the one soloed\n";
}
void rigPassEngine() {
    using P=Controller::Phase;
    Controller::Options base{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    base.sync=base.cabIR=base.polarity=false;
    const auto pct=[](float v){return std::to_string(juce::roundToInt(v*100))+"%";};
    const auto judge=[&](const char* name,const RigPassEngineRun& r,std::array<P,2> rows) {
        check(r.status.phase==P::done && r.status.passes==std::vector<P>{rows[0],rows[1]},std::string(name)+": the passes did not run on the real engine");
        std::string measured;
        for(int index=0;index<2;++index) {
            const auto* step=stepOf(r.status,rows[size_t(index)]);const auto& a=r.analysis[size_t(index)];
            check(step!=nullptr && r.analysed[size_t(index)],std::string(name)+": a pass left no row or analysis");
            check(a.playingDetected,std::string(name)+": the playing was not heard through the engine: "+step->result.toStdString());
            // GAIN started far below the rock drive: it must rise, and the drive must come closer to the target.
            check(step->changed && r.gainAfter[size_t(index)]>r.gainBefore[size_t(index)],std::string(name)+": a low GAIN was not raised: "+step->result.toStdString());
            check(std::abs(a.compressionAfter-a.compressionTarget)<std::abs(a.compressionBefore-a.compressionTarget),std::string(name)+": the drive did not come closer to the target: "+step->result.toStdString());
            measured+=std::string(index?" | ":"")+"GAIN "+std::to_string(r.gainBefore[size_t(index)])+" > "+std::to_string(r.gainAfter[size_t(index)])+", drive "+pct(a.compressionBefore)+" > "+pct(a.compressionAfter)+" (target "+pct(a.compressionTarget)+")";
        }
        std::cout<<"MEASURE auto set rig pass engine "<<name<<": "<<measured<<'\n';
    };
    auto blend=base;blend.dualAmp=true;
    judge("Dual Blend",runRigPassEngine(1,0,{0,1},blend,{.05f,.05f}),{P::dualAmp1,P::dualAmp2});
    auto cross=base;cross.crossGain=true;
    judge("Dual Crossover",runRigPassEngine(1,1,{0,1},cross,{.05f,.05f}),{P::crossGain1,P::crossGain2});
    auto matrix=base;matrix.matrixGain=true;
    judge("Matrix",runRigPassEngine(2,0,{1,2},matrix,{.05f,.05f}),{P::matrixGain2,P::matrixGain3});
    // A muted rig on the real engine: no pass, and the other rig is set.
    const auto muted=runRigPassEngine(1,0,{0,1},blend,{.05f,.05f},[](ChimeraProcessor& p){auto* q=p.parameters().getParameter("mute1");q->setValueNotifyingHost(1.f);});
    check(muted.status.passes==std::vector<P>{P::dualAmp2} && stepOf(muted.status,P::dualAmp1) && stepOf(muted.status,P::dualAmp1)->result=="kept (this rig is muted)"
          && muted.gainAfter[0]==muted.gainBefore[0] && muted.gainAfter[1]>muted.gainBefore[1],"A muted rig on the real engine was set, or stopped the other rig");
    // Phase 45: low notes barely reach the Matrix MID and HIGH bands: the lanes are kept and the reason names the band.
    const auto empty=runRigPassEngine(2,0,{1,2},matrix,{.05f,.05f},{},true);
    const auto* mid=stepOf(empty.status,P::matrixGain2);const auto* high=stepOf(empty.status,P::matrixGain3);
    check(mid && high && !mid->changed && !high->changed && empty.gainAfter==empty.gainBefore
          && mid->result=="kept (no playing heard in the MID lane's band: play notes that reach it, or move the crossover)"
          && high->result=="kept (no playing heard in the HIGH lane's band: play notes that reach it, or move the crossover)",
          "Lanes whose band is nearly empty were set, or the reason does not name the band: "+(mid?mid->result.toStdString():std::string("no step"))+" | "+(high?high->result.toStdString():std::string("no step")));
    // The same low notes in a Dual Blend, where each rig hears the whole input: heard, and the reason is not used.
    const auto whole=runRigPassEngine(1,0,{0,1},blend,{.05f,.05f},{},true);
    check(stepOf(whole.status,P::dualAmp1) && !stepOf(whole.status,P::dualAmp1)->result.contains("no playing heard"),"Low notes were not heard by a Dual Blend rig: "+stepOf(whole.status,P::dualAmp1)->result.toStdString());
    std::cout<<"PASS auto set rig pass engine: Dual Blend, Dual Crossover and Matrix rig and lane passes through processBlock: playing heard, low GAIN raised, drive closer to the target; a muted rig kept\n";
}
// Phase 46: the input trim and channel steps of the rig and lane passes on the real engine. The trim step says
// "loudness held": here each lane's level is measured on the rig tap before and after the run.
void rigPassEngineTrimAndChannel() {
    using P=Controller::Phase;
    constexpr int model=2,rhythm=0,lead=1;
    Controller::Options base{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    base.sync=base.cabIR=base.polarity=false;
    // Both heads on the given channel; gain>=0: that channel's GAIN at that fraction of its range.
    const auto heads=[=](int mode,std::array<int,2> lanes,int channel,float gain) {
        return [=](ChimeraProcessor& p) {
            for(const int lane:lanes) {
                const int context=spectralforge::ampNativeContext(mode,lane);
                auto* q=p.parameters().getParameter(spectralforge::ampNativeChannelID(context,model));q->setValueNotifyingHost(q->convertTo0to1(float(channel)));
                if(gain>=0)p.parameters().getParameter(spectralforge::ampNativeControlID(context,model,roleControls(model,channel,0)[ampGain],channel))->setValueNotifyingHost(gain);
            }
        };
    };
    const auto db=[](float v){std::ostringstream o;o<<std::fixed<<std::setprecision(1)<<v;return o.str();};
    struct Case {const char* name;int mode,dualType;std::array<int,2> lanes;std::array<P,2> trimRows,channelRows;bool Controller::Options::*trim;bool Controller::Options::*channel;float trimGain;};
    const std::array<Case,3> cases{{
        {"Dual Blend",1,0,{0,1},{P::dualTrim1,P::dualTrim2},{P::dualChannel1,P::dualChannel2},&Controller::Options::dualTrim,&Controller::Options::dualChannel,0.f},
        {"Dual Crossover",1,1,{0,1},{P::crossTrim1,P::crossTrim2},{P::crossChannel1,P::crossChannel2},&Controller::Options::crossTrim,&Controller::Options::crossChannel,0.f},
        {"Matrix",2,0,{1,2},{P::matrixTrim2,P::matrixTrim3},{P::matrixChannel2,P::matrixChannel3},&Controller::Options::matrixTrim,&Controller::Options::matrixChannel,.2f}}};
    for(const auto& k:cases) {
        // Trim: rock, RHYTHM with GAIN at its bottom. The trim rises, OUTPUT LEVEL falls, and the lane stays about as loud.
        auto trimOptions=base;trimOptions.*(k.trim)=true;
        const auto t=runRigPassEngine(k.mode,k.dualType,k.lanes,trimOptions,{0.f,0.f},heads(k.mode,k.lanes,rhythm,k.trimGain));
        check(t.status.phase==P::done,std::string(k.name)+": the trim run on the real engine did not finish");
        std::string measured;
        for(int index=0;index<2;++index) {
            const auto* step=stepOf(t.status,k.trimRows[size_t(index)]);
            check(step && step->changed && t.trimAfter[size_t(index)]>0 && t.outputAfter[size_t(index)]<t.outputBefore[size_t(index)],
                  std::string(k.name)+": the trim was not raised with the level lowered on the real engine: "+(step?step->result.toStdString():std::string("no step")));
            const float moved=t.levelAfterDb[size_t(index)]-t.levelBeforeDb[size_t(index)];
            // Without the level held, a 12 dB trim into a clean head would raise the lane by about 10 dB.
            check(std::abs(moved)<2.f,std::string(k.name)+": the lane's level moved by "+db(moved)+" dB after the trim: "+step->result.toStdString());
            measured+=std::string(index?" | ":"")+"trim +"+db(t.trimAfter[size_t(index)])+" dB, OUTPUT LEVEL "+db(t.outputAfter[size_t(index)]-t.outputBefore[size_t(index)])+" dB, lane level "+db(t.levelBeforeDb[size_t(index)])+" > "+db(t.levelAfterDb[size_t(index)])+" dB";
        }
        std::cout<<"MEASURE auto set rig pass engine trim, "<<k.name<<": "<<measured<<'\n';
        // Channel: clean, both heads on LEAD. Each moves to RHYTHM.
        auto channelOptions=base;channelOptions.style=Style::clean;channelOptions.*(k.channel)=true;
        const auto c=runRigPassEngine(k.mode,k.dualType,k.lanes,channelOptions,{.5f,.5f},heads(k.mode,k.lanes,lead,-1.f));
        for(int index=0;index<2;++index) {
            const auto* step=stepOf(c.status,k.channelRows[size_t(index)]);
            check(step && step->changed && c.channelBefore[size_t(index)]==lead && c.channelAfter[size_t(index)]==rhythm,
                  std::string(k.name)+": the channel was not switched on the real engine: "+(step?step->result.toStdString():std::string("no step")));
        }
    }
    std::cout<<"PASS auto set rig pass engine trim and channel: Dual Blend, Dual Crossover and Matrix through processBlock: trim raised, OUTPUT LEVEL lowered, each lane's measured level within 2 dB; clean LEAD > RHYTHM\n";
}
// Phase 47: phase 34 (PRESENCE and RESONANCE of both Dual Blend rigs) on the real engine, and the two level locks
// with the trim steps. A PRESENCE move is checked against the rig's measured top end: lowered, the share of the
// rig's energy above 3 kHz must fall; raised, it must rise; kept, it must stay.
void rigPassEnginePowerAndLocks() {
    using P=Controller::Phase;
    constexpr int model=2,rhythm=0;
    Controller::Options base{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    base.sync=base.cabIR=base.polarity=false;
    const auto db=[](float v){std::ostringstream o;o<<std::fixed<<std::setprecision(2)<<v;return o.str();};
    // PRESENCE of both rigs at the given fraction of its range.
    const auto presenceAt=[=](float fraction) {
        return [=](ChimeraProcessor& p) {
            for(int rig=0;rig<2;++rig) {
                const int channel=p.selectedAmpChannel(rig);
                p.parameters().getParameter(spectralforge::ampNativeControlID(spectralforge::ampNativeContext(1,rig),model,powerControl(model,channel,powerPresence),channel))->setValueNotifyingHost(fraction);
            }
        };
    };
    auto power=base;power.dualPower=true;
    int moved=0;std::string measured;
    for(const float fraction:{1.f,0.f}) {
        const auto r=runRigPassEngine(1,0,{0,1},power,{.3f,.3f},presenceAt(fraction));
        check(r.status.phase==P::done && r.status.passes==std::vector<P>{P::dualAmp1,P::dualAmp2},"The presence passes did not run on the real engine");
        for(int rig=0;rig<2;++rig) {
            const auto* step=stepOf(r.status,rig==0?P::dualPower1:P::dualPower2);
            check(step && !step->result.contains("no playing heard"),std::string("The presence step did not hear the rig through the engine: ")+(step?step->result.toStdString():std::string("no step")));
            const float knob=r.presenceAfter[size_t(rig)]-r.presenceBefore[size_t(rig)],share=r.highShareAfterDb[size_t(rig)]-r.highShareBeforeDb[size_t(rig)];
            check(step->changed==(std::abs(knob)>1e-6f) || step->result.contains("RESONANCE"),"The presence row and the PRESENCE knob disagree: "+step->result.toStdString());
            if(knob<-1e-6f)check(share<-.05f,"PRESENCE was lowered but the rig's top end did not fall ("+db(share)+" dB): "+step->result.toStdString());
            else if(knob>1e-6f)check(share>.05f,"PRESENCE was raised but the rig's top end did not rise ("+db(share)+" dB): "+step->result.toStdString());
            else if(!step->changed)check(std::abs(share)<.3f,"Nothing was set but the rig's top end moved by "+db(share)+" dB");
            moved+=std::abs(knob)>1e-6f;
            measured+=std::string(measured.empty()?"":" | ")+"PRESENCE "+db(r.presenceBefore[size_t(rig)])+" > "+db(r.presenceAfter[size_t(rig)])+", above 3 kHz "+db(r.highShareBeforeDb[size_t(rig)])+" > "+db(r.highShareAfterDb[size_t(rig)])+" dB ["+step->result.toStdString()+"], bite excess "+db(r.power[size_t(rig)].excessBeforeDb[powerPresence])+" dB";
        }
    }
    check(moved>0,"PRESENCE at neither end of its range was moved on the real engine: "+measured);
    std::cout<<"MEASURE auto set rig pass engine presence (PRESENCE at maximum, then at minimum; rig 1, rig 2): "<<measured<<'\n';
    // The level locks on the real engine. Both heads on RHYTHM with GAIN low, as phase 46's trim runs.
    const auto heads=[=](int mode,std::array<int,2> lanes,float gain) {
        return [=](ChimeraProcessor& p) {
            for(const int lane:lanes) {
                const int context=spectralforge::ampNativeContext(mode,lane);
                auto* q=p.parameters().getParameter(spectralforge::ampNativeChannelID(context,model));q->setValueNotifyingHost(q->convertTo0to1(float(rhythm)));
                p.parameters().getParameter(spectralforge::ampNativeControlID(context,model,roleControls(model,rhythm,0)[ampGain],rhythm))->setValueNotifyingHost(gain);
            }
        };
    };
    // What a lock check saw, for its failure message: the row, the trim, OUTPUT LEVEL and the measured level before > after.
    const auto lockRow=[](const RigPassEngineRun& r,P row,int index) {
        const auto* step=stepOf(r.status,row);std::ostringstream o;o<<std::setprecision(9);
        o<<"["<<(step?step->result.toStdString():std::string("no step"))<<"], trim "<<r.trimAfter[size_t(index)]<<" dB, OUTPUT LEVEL "<<r.outputBefore[size_t(index)]<<" > "<<r.outputAfter[size_t(index)]
         <<" dB, level "<<r.levelBeforeDb[size_t(index)]<<" > "<<r.levelAfterDb[size_t(index)]<<" dB";
        return o.str();
    };
    // Dual rig levels lock: rig 2's trim and level kept, its level unmoved; rig 1 trimmed with its level held.
    auto rigs=base;rigs.dualTrim=true;rigs.locks|=Controller::lockRigs;
    const auto a=runRigPassEngine(1,0,{0,1},rigs,{0.f,0.f},heads(1,{0,1},0.f));
    check(stepOf(a.status,P::dualTrim2) && stepOf(a.status,P::dualTrim2)->result.contains("the Dual rig levels lock keeps") && a.trimAfter[1]==0.f && a.outputAfter[1]==a.outputBefore[1]
          && std::abs(a.levelAfterDb[1]-a.levelBeforeDb[1])<.3f,"The Dual rig levels lock did not keep rig 2 on the real engine: "+lockRow(a,P::dualTrim2,1)+" | rig 1: "+lockRow(a,P::dualTrim1,0));
    check(stepOf(a.status,P::dualTrim1) && stepOf(a.status,P::dualTrim1)->changed && a.trimAfter[0]>0 && std::abs(a.levelAfterDb[0]-a.levelBeforeDb[0])<2.f,"With the Dual rig levels lock rig 1 was not trimmed with its level held");
    // Matrix levels lock: both lanes kept, their levels unmoved.
    auto lanes=base;lanes.matrixTrim=true;lanes.locks|=Controller::lockMatrix;
    const auto m=runRigPassEngine(2,0,{1,2},lanes,{.2f,.2f},heads(2,{1,2},.2f));
    for(int index=0;index<2;++index) {
        const auto* step=stepOf(m.status,index==0?P::matrixTrim2:P::matrixTrim3);
        check(step && step->result.contains("the Matrix levels lock keeps") && m.trimAfter[size_t(index)]==0.f && m.outputAfter[size_t(index)]==m.outputBefore[size_t(index)]
              && std::abs(m.levelAfterDb[size_t(index)]-m.levelBeforeDb[size_t(index)])<.3f,"The Matrix levels lock did not keep a lane on the real engine: "+lockRow(m,index==0?P::matrixTrim2:P::matrixTrim3,index));
    }
    std::cout<<"PASS auto set rig pass engine presence and locks: phase 34 through processBlock with the rig's measured top end following each PRESENCE move; the Dual rig levels lock keeps rig 2 and the Matrix levels lock both lanes, levels unmoved\n";
}
